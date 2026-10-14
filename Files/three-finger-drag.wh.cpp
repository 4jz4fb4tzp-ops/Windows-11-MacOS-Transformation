// ==WindhawkMod==
// @id              three-finger-drag
// @name            Three Finger Drag (macOS-Style)
// @description     Mit 3 Fingern auf dem Precision Touchpad ziehen = linke Maustaste gedrückt halten
// @version         1.0
// @author          Okan
// @include         explorer.exe
// @compilerOptions -lhid
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Three Finger Drag

Wie unter macOS: Drei Finger aufs Touchpad legen und bewegen = linke Maustaste
gedrückt halten und ziehen (Fenster verschieben, Text markieren, Dateien ziehen).

**Wichtig:** In Windows unter *Einstellungen → Bluetooth & Geräte → Touchpad →
Gesten mit drei Fingern* die **Wischbewegungen auf „Nichts"** stellen, sonst
kollidiert die Windows-Geste (App-Wechsel) mit dem Ziehen.

Funktioniert nur mit Precision Touchpads.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- releaseDelay: 300
  $name: Loslass-Verzögerung (ms)
  $description: So lange bleibt die Taste nach dem Abheben gedrückt, um nachzugreifen (0 = sofort loslassen)
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <hidusage.h>
#include <hidpi.h>
#include <algorithm>
#include <map>
#include <vector>

// HID-Konstanten
constexpr USAGE kPageGeneric   = 0x01;
constexpr USAGE kUsageX        = 0x30;
constexpr USAGE kUsageY        = 0x31;
constexpr USAGE kPageDigitizer = 0x0D;
constexpr USAGE kUsageTouchpad = 0x05;
constexpr USAGE kUsageTip      = 0x42;
constexpr USAGE kUsageContactId    = 0x51;
constexpr USAGE kUsageContactCount = 0x54;

constexpr UINT_PTR kTimerRelease  = 1;
constexpr UINT_PTR kTimerWatchdog = 2;
constexpr DWORD kStaleMs = 150;

// Basis-Auflösung: Touchpad-Bewegung wird wie eine Maus mit dieser DPI behandelt.
// Windows wendet darauf die eingestellte Zeigergeschwindigkeit und
// "Zeigerbeschleunigung verbessern" an, also exakt wie bei der Maus.
constexpr double kCountsPerInch = 600.0;

struct Slot {
    USHORT link;
};

struct DeviceInfo {
    std::vector<BYTE> ppd;
    std::vector<Slot> slots;
    LONG xRange = 1, yRange = 1;
    double countsPerUnitX = 1.0, countsPerUnitY = 1.0;
    ULONG remaining = 0;   // Hybrid-Modus: noch ausstehende Kontakte im Frame
    bool hasCount = false;
};

struct Contact {
    LONG x, y;
    DWORD tick;
};

static HANDLE g_mutex;
static HANDLE g_thread;
static HANDLE g_ready;
static HWND g_hwnd;

static std::map<HANDLE, DeviceInfo> g_devices;
static std::map<ULONG, Contact> g_touch;

static bool g_dragging = false;
static bool g_anchorValid = false;
static ULONG g_anchorId = 0;
static LONG g_lastX = 0, g_lastY = 0;
static double g_accX = 0, g_accY = 0;  // Nachkommareste der Bewegung
static double g_countsPerUnitX = 1, g_countsPerUnitY = 1;

static int g_releaseDelay = 300;

static void LoadSettings() {
    g_releaseDelay = Wh_GetIntSetting(L"releaseDelay");
    if (g_releaseDelay < 0) g_releaseDelay = 0;
}

static void SendButton(DWORD flag) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flag;
    SendInput(1, &in, sizeof(in));
}

// Relative Bewegung: Windows wendet Mausgeschwindigkeit + Beschleunigung an
static void MoveRelative(double dx, double dy) {
    g_accX += dx;
    g_accY += dy;
    LONG mx = (LONG)g_accX, my = (LONG)g_accY;
    if (!mx && !my) return;
    g_accX -= mx;
    g_accY -= my;

    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = mx;
    in.mi.dy = my;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(in));
}

// Physische Größe einer Achse in Zoll aus den HID-Caps (0 = unbekannt)
static double AxisInches(const HIDP_VALUE_CAPS& c) {
    double phys = (double)(c.PhysicalMax - c.PhysicalMin);
    if (phys <= 0) return 0;
    int e = c.UnitsExp & 0xF;
    if (e >= 8) e -= 16;
    double size = phys;
    for (; e > 0; e--) size *= 10;
    for (; e < 0; e++) size /= 10;
    switch (c.Units) {
        case 0x11: return size / 2.54;  // Zentimeter
        case 0x13: return size;         // Zoll
    }
    return 0;
}

static void StartDrag() {
    g_accX = g_accY = 0;
    g_anchorValid = false;
    g_dragging = true;
    SendButton(MOUSEEVENTF_LEFTDOWN);
}

static void EndDrag() {
    if (!g_dragging) return;
    g_dragging = false;
    g_anchorValid = false;
    KillTimer(g_hwnd, kTimerRelease);
    SendButton(MOUSEEVENTF_LEFTUP);
}

static DeviceInfo* GetDevice(HANDLE h) {
    auto it = g_devices.find(h);
    if (it != g_devices.end()) return &it->second;

    DeviceInfo dev;
    UINT size = 0;
    GetRawInputDeviceInfoW(h, RIDI_PREPARSEDDATA, nullptr, &size);
    if (!size) return nullptr;
    dev.ppd.resize(size);
    if (GetRawInputDeviceInfoW(h, RIDI_PREPARSEDDATA, dev.ppd.data(), &size) == (UINT)-1)
        return nullptr;
    auto pp = (PHIDP_PREPARSED_DATA)dev.ppd.data();

    HIDP_CAPS caps;
    if (HidP_GetCaps(pp, &caps) != HIDP_STATUS_SUCCESS) return nullptr;
    USHORT n = caps.NumberInputValueCaps;
    std::vector<HIDP_VALUE_CAPS> vc(n);
    HidP_GetValueCaps(HidP_Input, vc.data(), &n, pp);

    std::map<USHORT, int> found;  // link -> Bitmaske (1 = X, 2 = Y)
    double inchX = 0, inchY = 0;
    for (auto& c : vc) {
        USAGE u = c.IsRange ? c.Range.UsageMin : c.NotRange.Usage;
        if (c.UsagePage == kPageDigitizer && u == kUsageContactCount) dev.hasCount = true;
        if (c.UsagePage != kPageGeneric) continue;
        if (u == kUsageX) {
            found[c.LinkCollection] |= 1;
            dev.xRange = std::max<LONG>(1, c.LogicalMax - c.LogicalMin);
            inchX = AxisInches(c);
        } else if (u == kUsageY) {
            found[c.LinkCollection] |= 2;
            dev.yRange = std::max<LONG>(1, c.LogicalMax - c.LogicalMin);
            inchY = AxisInches(c);
        }
    }
    for (auto& [link, mask] : found)
        if (mask == 3) dev.slots.push_back({link});
    std::sort(dev.slots.begin(), dev.slots.end(),
              [](const Slot& a, const Slot& b) { return a.link < b.link; });

    // Fallback, falls das Touchpad keine physische Größe meldet: ~10 x 6,5 cm
    if (inchX <= 0) inchX = 4.0;
    if (inchY <= 0) inchY = 2.6;
    dev.countsPerUnitX = kCountsPerInch * inchX / dev.xRange;
    dev.countsPerUnitY = kCountsPerInch * inchY / dev.yRange;

    Wh_Log(L"Touchpad: %zu Slots, %.2f x %.2f Zoll",
           dev.slots.size(), inchX, inchY);
    return &(g_devices[h] = std::move(dev));
}

static void HandleContact(ULONG id, bool tip, LONG x, LONG y) {
    if (!tip) {
        g_touch.erase(id);
        if (g_anchorValid && id == g_anchorId) g_anchorValid = false;
        return;
    }
    g_touch[id] = {x, y, GetTickCount()};

    if (!g_dragging || g_touch.size() < 3) return;

    if (!g_anchorValid) {
        g_anchorValid = true;
        g_anchorId = id;
        g_lastX = x;
        g_lastY = y;
        return;
    }
    if (id != g_anchorId) return;

    LONG dx = x - g_lastX, dy = y - g_lastY;
    g_lastX = x;
    g_lastY = y;
    if (dx || dy)
        MoveRelative(dx * g_countsPerUnitX, dy * g_countsPerUnitY);
}

static void Evaluate() {
    DWORD now = GetTickCount();
    for (auto it = g_touch.begin(); it != g_touch.end();) {
        if (now - it->second.tick > kStaleMs) {
            if (g_anchorValid && it->first == g_anchorId) g_anchorValid = false;
            it = g_touch.erase(it);
        } else {
            ++it;
        }
    }

    size_t n = g_touch.size();
    if (n < 3) g_anchorValid = false;

    if (n >= 3 && !g_dragging) StartDrag();

    if (g_dragging) {
        if (n > 0) {
            KillTimer(g_hwnd, kTimerRelease);
        } else if (g_releaseDelay == 0) {
            EndDrag();
        } else {
            SetTimer(g_hwnd, kTimerRelease, g_releaseDelay, nullptr);
        }
    }
}

static void ProcessReport(DeviceInfo& dev, BYTE* report, ULONG len) {
    auto pp = (PHIDP_PREPARSED_DATA)dev.ppd.data();
    auto r = (PCHAR)report;

    size_t valid = dev.slots.size();
    if (dev.hasCount) {
        ULONG cc = 0;
        if (HidP_GetUsageValue(HidP_Input, kPageDigitizer, 0, kUsageContactCount,
                               &cc, pp, r, len) == HIDP_STATUS_SUCCESS && cc > 0)
            dev.remaining = cc;
        valid = std::min<size_t>(dev.remaining, dev.slots.size());
        dev.remaining -= (ULONG)valid;
    }

    for (size_t i = 0; i < valid; i++) {
        USHORT link = dev.slots[i].link;
        ULONG x = 0, y = 0, id = 0;
        if (HidP_GetUsageValue(HidP_Input, kPageGeneric, link, kUsageX, &x, pp, r, len) != HIDP_STATUS_SUCCESS ||
            HidP_GetUsageValue(HidP_Input, kPageGeneric, link, kUsageY, &y, pp, r, len) != HIDP_STATUS_SUCCESS)
            continue;
        HidP_GetUsageValue(HidP_Input, kPageDigitizer, link, kUsageContactId, &id, pp, r, len);

        bool tip = false;
        USAGE usages[32];
        ULONG cnt = ARRAYSIZE(usages);
        if (HidP_GetUsages(HidP_Input, kPageDigitizer, link, usages, &cnt, pp, r, len) == HIDP_STATUS_SUCCESS)
            for (ULONG k = 0; k < cnt; k++)
                if (usages[k] == kUsageTip) tip = true;

        HandleContact(id, tip, (LONG)x, (LONG)y);
    }
}

static void HandleRawInput(HRAWINPUT h) {
    static std::vector<BYTE> buf;
    UINT size = 0;
    GetRawInputData(h, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
    if (!size) return;
    buf.resize(size);
    if (GetRawInputData(h, RID_INPUT, buf.data(), &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
        return;

    auto ri = (RAWINPUT*)buf.data();
    if (ri->header.dwType != RIM_TYPEHID) return;
    DeviceInfo* dev = GetDevice(ri->header.hDevice);
    if (!dev || dev->slots.empty()) return;

    g_countsPerUnitX = dev->countsPerUnitX;
    g_countsPerUnitY = dev->countsPerUnitY;

    for (DWORD i = 0; i < ri->data.hid.dwCount; i++)
        ProcessReport(*dev, ri->data.hid.bRawData + i * ri->data.hid.dwSizeHid,
                      ri->data.hid.dwSizeHid);

    Evaluate();
    SetTimer(g_hwnd, kTimerWatchdog, 250, nullptr);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_INPUT:
            HandleRawInput((HRAWINPUT)lParam);
            break;  // DefWindowProc muss für WM_INPUT aufgerufen werden
        case WM_TIMER:
            if (wParam == kTimerRelease) {
                EndDrag();
            } else if (wParam == kTimerWatchdog) {
                // Keine Reports mehr -> alle Finger gelten als abgehoben
                KillTimer(hwnd, kTimerWatchdog);
                g_touch.clear();
                Evaluate();
            }
            return 0;
        case WM_CLOSE:
            EndDrag();
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static DWORD WINAPI ThreadProc(LPVOID) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"Windhawk_ThreeFingerDrag";
    RegisterClassW(&wc);

    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, hInst, nullptr);

    RAWINPUTDEVICE rid{kPageDigitizer, kUsageTouchpad, RIDEV_INPUTSINK, g_hwnd};
    if (!g_hwnd || !RegisterRawInputDevices(&rid, 1, sizeof(rid)))
        Wh_Log(L"Registrierung fehlgeschlagen: %lu", GetLastError());

    SetEvent(g_ready);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    UnregisterClassW(wc.lpszClassName, hInst);
    return 0;
}

BOOL Wh_ModInit() {
    LoadSettings();

    // Nur eine Instanz (falls mehrere explorer.exe-Prozesse laufen)
    g_mutex = CreateMutexW(nullptr, FALSE, L"Windhawk_ThreeFingerDrag_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(g_mutex);
        g_mutex = nullptr;
        return TRUE;
    }

    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    if (g_thread) WaitForSingleObject(g_ready, 5000);
    return TRUE;
}

void Wh_ModUninit() {
    if (g_thread) {
        if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_ready) CloseHandle(g_ready);
    if (g_mutex) CloseHandle(g_mutex);
}

void Wh_ModSettingsChanged() {
    LoadSettings();
}