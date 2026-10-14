// ==WindhawkMod==
// @id              turbo-boost-tray-toggle
// @name            Turbo Boost Tray Toggle
// @description     Tray-Icon zum Ein-/Ausschalten des CPU-Turbo-Boosts, optional automatisch je nach Netz/Akku
// @version         1.1
// @author          Okan
// @include         explorer.exe
// @compilerOptions -lpowrprof -lshell32 -lgdi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Turbo Boost Tray Toggle
- **Linksklick** aufs Tray-Icon: Turbo Boost an/aus
- **Rechtsklick**: Menü
  - *Turbo Boost* – an/aus
  - *Automatisch: Netz an / Akku aus* – Boost wird beim Anstecken des
    Ladegeräts aktiviert und im Akkubetrieb deaktiviert
- Orange = Boost an, Grau = Boost aus

Im Automatik-Modus übersteuert ein Klick nur den aktuellen Zustand.
Beim nächsten Wechsel Netz/Akku greift wieder die Automatik.

Ändert den "Prozessorleistungs-Boostmodus" im aktiven Energieplan.
Funktioniert mit Intel und AMD.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- onMode: 2
  $name: Boost-Modus für "An"
  $description: 1 = Aktiviert, 2 = Aggressiv (Windows-Standard), 3 = Effizient aktiviert, 4 = Effizient aggressiv
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <powrprof.h>
#include <shellapi.h>
#include <vector>

// GUID_PROCESSOR_SETTINGS_SUBGROUP / GUID_PROCESSOR_PERF_BOOST_MODE
static const GUID kSubProcessor = {0x54533251, 0x82be, 0x4824, {0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00}};
static const GUID kBoostMode    = {0xbe337238, 0x0d82, 0x4146, {0xa9, 0x60, 0x4f, 0x37, 0x49, 0xd4, 0x70, 0xc7}};

#define WM_TRAY    (WM_APP + 1)
#define IDM_TOGGLE 1001
#define IDM_AUTO   1002
#define TIMER_ID   1
static const wchar_t kClass[] = L"WhTurboBoostToggleWnd";

static HANDLE g_mutex, g_thread, g_ready;
static HWND   g_hwnd;
static HICON  g_iconOn, g_iconOff;
static UINT   g_msgTaskbarCreated;
static int    g_onMode = 2;
static bool   g_auto = false;
static int    g_lastAc = -1;   // letzter bekannter Netzstatus (1 = Netz, 0 = Akku)

// ---------- Energieplan ----------

static bool OnBattery() {
    SYSTEM_POWER_STATUS ps{};
    return GetSystemPowerStatus(&ps) && ps.ACLineStatus == 0;
}

static bool ReadBoost(DWORD* ac, DWORD* dc) {
    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) != ERROR_SUCCESS) return false;
    DWORD r1 = PowerReadACValueIndex(nullptr, scheme, &kSubProcessor, &kBoostMode, ac);
    DWORD r2 = PowerReadDCValueIndex(nullptr, scheme, &kSubProcessor, &kBoostMode, dc);
    LocalFree(scheme);
    return r1 == ERROR_SUCCESS && r2 == ERROR_SUCCESS;
}

static DWORD WriteBoost(DWORD ac, DWORD dc) {
    GUID* scheme = nullptr;
    DWORD r = PowerGetActiveScheme(nullptr, &scheme);
    if (r != ERROR_SUCCESS) return r;
    r = PowerWriteACValueIndex(nullptr, scheme, &kSubProcessor, &kBoostMode, ac);
    if (r == ERROR_SUCCESS)
        r = PowerWriteDCValueIndex(nullptr, scheme, &kSubProcessor, &kBoostMode, dc);
    if (r == ERROR_SUCCESS)
        r = PowerSetActiveScheme(nullptr, scheme);  // Änderung sofort anwenden
    LocalFree(scheme);
    return r;
}

// Boost-Zustand für die aktuelle Stromquelle
static bool IsBoostOn() {
    DWORD ac = 1, dc = 1;
    ReadBoost(&ac, &dc);
    return (OnBattery() ? dc : ac) != 0;
}

// Automatik: Netz = an, Akku = aus (Windows schaltet dann selbst je nach Quelle)
static DWORD ApplyAuto() {
    return WriteBoost((DWORD)g_onMode, 0);
}

// ---------- Icon ----------

static HICON MakeIcon(DWORD argb) {
    int s = GetSystemMetrics(SM_CXSMICON);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = s;
    bi.bmiHeader.biHeight = -s;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;

    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!color) return nullptr;

    DWORD* px = (DWORD*)bits;
    float c = (s - 1) / 2.0f, r = s / 2.0f - 0.5f, inner = r * 0.35f;
    for (int y = 0; y < s; y++) {
        for (int x = 0; x < s; x++) {
            float d2 = (x - c) * (x - c) + (y - c) * (y - c);
            px[y * s + x] = d2 <= inner * inner ? 0xFFFFFFFF
                          : d2 <= r * r         ? argb
                                                : 0x00000000;
        }
    }

    std::vector<BYTE> maskBits(((s + 15) / 16) * 2 * s, 0);
    HBITMAP mask = CreateBitmap(s, s, 1, 1, maskBits.data());
    ICONINFO ii{TRUE, 0, 0, mask, color};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    return icon;
}

// ---------- Tray ----------

static void UpdateTray(DWORD action, DWORD err = 0) {
    bool on = IsBoostOn();
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = on ? g_iconOn : g_iconOff;
    if (err)
        wsprintfW(nid.szTip, L"Turbo Boost: Fehler %lu", err);
    else
        wsprintfW(nid.szTip, L"Turbo Boost: %s%s",
                  on ? L"AN" : L"AUS",
                  g_auto ? L" [Auto]" : L"");
    Shell_NotifyIconW(action, &nid);
}

static void Toggle() {
    DWORD nv = IsBoostOn() ? 0 : (DWORD)g_onMode;
    DWORD err;
    if (g_auto) {
        // Nur aktuelle Stromquelle übersteuern; beim nächsten Wechsel greift die Automatik
        DWORD ac = (DWORD)g_onMode, dc = 0;
        ReadBoost(&ac, &dc);
        if (OnBattery()) dc = nv; else ac = nv;
        err = WriteBoost(ac, dc);
    } else {
        err = WriteBoost(nv, nv);
    }
    UpdateTray(NIM_MODIFY, err);
}

static void SetAuto(bool enable) {
    g_auto = enable;
    Wh_SetIntValue(L"auto", enable ? 1 : 0);
    DWORD err;
    if (enable) {
        err = ApplyAuto();
    } else {
        // Aktuellen Zustand für beide Quellen übernehmen
        DWORD v = IsBoostOn() ? (DWORD)g_onMode : 0;
        err = WriteBoost(v, v);
    }
    UpdateTray(NIM_MODIFY, err);
}

static void OnPowerSourceMaybeChanged() {
    int ac = OnBattery() ? 0 : 1;
    if (ac == g_lastAc) return;
    g_lastAc = ac;
    DWORD err = g_auto ? ApplyAuto() : 0;
    UpdateTray(NIM_MODIFY, err);
}

static void ShowMenu(HWND h) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (IsBoostOn() ? MF_CHECKED : 0), IDM_TOGGLE, L"Turbo Boost");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (g_auto ? MF_CHECKED : 0), IDM_AUTO,
                L"Automatisch: Netz an / Akku aus");
    SetForegroundWindow(h);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, h, nullptr);
    PostMessageW(h, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == g_msgTaskbarCreated && m != 0) {  // Explorer/Taskleiste neu gestartet
        UpdateTray(NIM_ADD);
        return 0;
    }
    switch (m) {
        case WM_TRAY:
            if (LOWORD(l) == WM_LBUTTONUP) Toggle();
            else if (LOWORD(l) == WM_RBUTTONUP) ShowMenu(h);
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == IDM_TOGGLE) Toggle();
            else if (LOWORD(w) == IDM_AUTO) SetAuto(!g_auto);
            return 0;
        case WM_POWERBROADCAST:
            if (w == PBT_APMPOWERSTATUSCHANGE || w == PBT_APMRESUMEAUTOMATIC)
                OnPowerSourceMaybeChanged();
            return TRUE;
        case WM_TIMER:  // Fallback + externe Änderungen übernehmen
            OnPowerSourceMaybeChanged();
            UpdateTray(NIM_MODIFY);
            return 0;
        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY: {
            KillTimer(h, TIMER_ID);
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(nid);
            nid.hWnd = h;
            nid.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &nid);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

static DWORD WINAPI TrayThread(LPVOID) {
    HINSTANCE inst = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = kClass;
    RegisterClassW(&wc);

    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_iconOn  = MakeIcon(0xFFFF8C00);  // orange
    g_iconOff = MakeIcon(0xFF8A8A8A);  // grau

    g_hwnd = CreateWindowExW(0, kClass, L"", WS_POPUP, 0, 0, 0, 0,
                             nullptr, nullptr, inst, nullptr);
    SetEvent(g_ready);
    if (!g_hwnd) {
        UnregisterClassW(kClass, inst);
        return 1;
    }

    g_lastAc = OnBattery() ? 0 : 1;
    if (g_auto) ApplyAuto();
    UpdateTray(NIM_ADD);
    SetTimer(g_hwnd, TIMER_ID, 5000, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_hwnd = nullptr;
    UnregisterClassW(kClass, inst);
    if (g_iconOn)  DestroyIcon(g_iconOn);
    if (g_iconOff) DestroyIcon(g_iconOff);
    return 0;
}

// ---------- Windhawk ----------

static void LoadSettings() {
    int v = Wh_GetIntSetting(L"onMode");
    g_onMode = (v >= 1 && v <= 4) ? v : 2;
}

BOOL Wh_ModInit() {
    // Nur eine Instanz, falls mehrere explorer.exe-Prozesse laufen
    g_mutex = CreateMutexW(nullptr, FALSE, L"Local\\WhTurboBoostTrayToggle");
    if (!g_mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (g_mutex) CloseHandle(g_mutex);
        g_mutex = nullptr;
        return TRUE;
    }

    LoadSettings();
    g_auto = Wh_GetIntValue(L"auto", 0) != 0;  // gespeicherte Auswahl
    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, TrayThread, nullptr, 0, nullptr);
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
    if (g_ready) { CloseHandle(g_ready); g_ready = nullptr; }
    if (g_mutex) { CloseHandle(g_mutex); g_mutex = nullptr; }
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    if (g_auto) ApplyAuto();  // geänderten Boost-Modus sofort übernehmen
}