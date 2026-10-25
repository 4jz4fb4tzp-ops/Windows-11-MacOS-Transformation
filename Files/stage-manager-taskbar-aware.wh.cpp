// ==WindhawkMod==
// @id              stage-manager-taskbar-aware
// @name            Stage Manager (macOS-Stil)
// @description     Aktives Fenster in der Mitte, zuletzt benutzte Fenster als schwebende Karten gegenüber der Taskleiste
// @version         2.3.3
// @author          Okan
// @include         explorer.exe
// @compilerOptions -ldwmapi -lshell32 -lgdi32 -luser32 -ladvapi32 -lole32 -luuid -lwindowscodecs
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Stage Manager für Windows

- Nur ein Fenster ist "auf der Bühne". Die zuletzt benutzten Fenster schweben
  als leicht geneigte Karten mit Schatten und App-Symbol am Bildschirmrand.
- Klick auf eine Karte holt das Fenster auf die Bühne.
- Ziehst/vergrößerst du das aktive Fenster über die Karten (oder maximierst es),
  gleitet der Stage Manager aus dem Bild. Maus an den Bildschirmrand → er kommt
  wieder, Maus weg → er verschwindet wieder.
- Ein-/Ausschalten: Kachel im Kontrollzentrum der Menüleiste, Symbol im
  Infobereich der Taskleiste (Linksklick) oder Win+Alt+S.
- Die App-Symbole kommen aus dem Mod "macOS Dock": dieselben PNGs, die du dort
  einer App zugewiesen hast. Apps ohne eigenes Symbol behalten das
  Windows-Symbol.

## Position (Modus "Automatisch")
| Taskleiste | Stage Manager |
|------------|---------------|
| rechts     | links         |
| links      | rechts        |
| unten/oben | links         |
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- cardWidth: 150
  $name: Kartenbreite (px bei 100 % Skalierung)
- maxWindows: 5
  $name: Maximale Anzahl Fenster im Stage Manager
  $description: Ältere Fenster werden minimiert
- side: auto
  $name: Seite
  $options:
  - auto: Automatisch (abhängig von der Taskleiste)
  - left: Immer links
  - right: Immer rechts
- dockIcons: true
  $name: Symbole aus dem macOS Dock verwenden
  $description: Nimmt die PNG-Symbole, die im Mod "macOS Dock" den Apps zugewiesen sind
- tilt: true
  $name: 3D-Neigung der Karten
- moveAside: true
  $name: Aktives Fenster automatisch neben den Stage Manager schieben
- hotkey: true
  $name: Tastenkürzel Win+Alt+S zum Ein-/Ausschalten
- trayIcon: true
  $name: Symbol im Infobereich der Taskleiste anzeigen
  $description: Ausschalten, wenn der Knopf in der Menüleiste genutzt wird
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propsys.h>
#include <wincodec.h>
#include <limits.h>
#include <string>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <set>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

#define WM_APP_EXIT      (WM_APP + 1)
#define WM_APP_RELOAD    (WM_APP + 2)
#define WM_APP_FG        (WM_APP + 3)
#define WM_APP_DESTROYED (WM_APP + 4)
#define WM_APP_HIDDEN    (WM_APP + 5)
#define WM_TRAYICON      (WM_APP + 20)
#define WM_APP_SM_CONTROL (WM_APP + 30)

enum { TIMER_CHECK = 1, TIMER_POLL, TIMER_ANIM, TIMER_SNAP };
enum { ID_TOGGLE = 100 };
enum { HOTKEY_ID = 1 };

struct Img {
    int w = 0, h = 0;
    std::vector<uint32_t> px;  // BGRA, vormultipliert
};

// Everything a drawn card depends on besides its snapshot and icon.
struct LayerKey {
    RECT card;
    float hover;
    int edge, surfW, surfH, tilt, cardW, scale100;
};

struct StagedWindow {
    HWND hwnd;
    WINDOWPLACEMENT wp;
    RECT rect;
    Img snap;
    Img icon;
    RECT card;   // Kartenposition im Streifen (Client-Koordinaten)
    float hover; // 0..1
    // The card and its icon, drawn once into their own premultiplied layer and
    // reused until the key changes or the snapshot is retaken (see EnsureLayer).
    std::vector<uint32_t> layer;
    RECT layerBox;
    LayerKey layerKey;
    bool layerValid;
};

struct {
    int cardWidth;
    int maxWindows;
    int side;  // 0 auto, 1 links, 2 rechts
    bool tilt;
    bool dockIcons;
    bool moveAside;
    bool hotkey;
    bool trayIcon;
} g_settings;

static HANDLE g_thread, g_readyEvent;
static HWND g_strip, g_trayWnd, g_active;
static std::vector<StagedWindow> g_staged;  // [0] = zuletzt benutzt
static std::set<HWND> g_seen;
static UINT g_edge = ABE_LEFT;
static bool g_started, g_enabled = true, g_trayAdded, g_hotkeyRegistered;
static HWINEVENTHOOK g_hookFg, g_hookObj;
static UINT g_msgTaskbarCreated;
static HICON g_trayIcon;

static HDC g_memDC;
static HBITMAP g_dib;
static HGDIOBJ g_oldBmp;
static uint32_t* g_bits;
static int g_W, g_H;
static POINT g_pos;
static float g_slide = 0, g_slideTarget = 0;
static bool g_peek, g_trackingMouse, g_animRunning;
static int g_hoverIdx = -1;
static size_t g_snapCursor;

static const wchar_t kStripClass[] = L"WhStageManagerStrip";
static const wchar_t kTrayClass[] = L"WhStageManagerTray";

// ================================================================ Hilfen

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline float Sat(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static inline float SmoothStep(float e0, float e1, float x) {
    float t = Sat((x - e0) / (e1 - e0));
    return t * t * (3 - 2 * t);
}

static void LoadSettings() {
    g_settings.cardWidth = Wh_GetIntSetting(L"cardWidth");
    g_settings.cardWidth = imax(80, imin(300, g_settings.cardWidth));
    g_settings.maxWindows = imax(1, imin(12, Wh_GetIntSetting(L"maxWindows")));
    PCWSTR side = Wh_GetStringSetting(L"side");
    g_settings.side = !wcscmp(side, L"left") ? 1 : !wcscmp(side, L"right") ? 2 : 0;
    Wh_FreeStringSetting(side);
    g_settings.tilt = Wh_GetIntSetting(L"tilt") != 0;
    g_settings.dockIcons = Wh_GetIntSetting(L"dockIcons") != 0;
    g_settings.moveAside = Wh_GetIntSetting(L"moveAside") != 0;
    g_settings.hotkey = Wh_GetIntSetting(L"hotkey") != 0;
    g_settings.trayIcon = Wh_GetIntSetting(L"trayIcon") != 0;
}

static int Scale(int v) {
    UINT dpi = g_strip ? GetDpiForWindow(g_strip) : 96;
    return MulDiv(v, dpi ? dpi : 96, 96);
}

static HMONITOR StripMonitor() {
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) return MonitorFromWindow(tray, MONITOR_DEFAULTTOPRIMARY);
    POINT pt{0, 0};
    return MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
}

static MONITORINFO Mon() {
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(StripMonitor(), &mi);
    return mi;
}

static int FindStaged(HWND h) {
    for (size_t i = 0; i < g_staged.size(); i++)
        if (g_staged[i].hwnd == h) return (int)i;
    return -1;
}

static bool IsCloaked(HWND h) {
    DWORD c = 0;
    DwmGetWindowAttribute(h, DWMWA_CLOAKED, &c, sizeof(c));
    return c != 0;
}

static RECT FrameRect(HWND h) {
    RECT r{};
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r))))
        GetWindowRect(h, &r);
    return r;
}

static bool IsShellClass(HWND h) {
    wchar_t cls[128] = {};
    GetClassNameW(h, cls, ARRAYSIZE(cls));
    static const wchar_t* excluded[] = {
        L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd", L"Progman", L"WorkerW",
        L"Windows.UI.Core.CoreWindow", L"XamlExplorerHostIslandWindow",
        L"MultitaskingViewFrame", L"ForegroundStaging",
        L"TopLevelWindowForOverflowXamlIsland", kStripClass, kTrayClass,
    };
    for (auto e : excluded)
        if (!wcscmp(cls, e)) return true;
    return false;
}

static bool IsManageable(HWND h) {
    if (!h || !IsWindow(h) || h == g_strip || h == g_trayWnd) return false;
    if (!IsWindowVisible(h) || IsCloaked(h)) return false;
    if (GetWindow(h, GW_OWNER)) return false;
    LONG st = GetWindowLongW(h, GWL_STYLE);
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    if (st & WS_CHILD) return false;
    if (ex & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) return false;
    if (!(st & (WS_CAPTION | WS_THICKFRAME))) return false;
    if (IsShellClass(h)) return false;
    return MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST) == StripMonitor();
}

static UINT ComputeEdge() {
    if (g_settings.side == 1) return ABE_LEFT;
    if (g_settings.side == 2) return ABE_RIGHT;
    APPBARDATA abd{sizeof(abd)};
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        if (abd.uEdge == ABE_LEFT) return ABE_RIGHT;
        if (abd.uEdge == ABE_RIGHT) return ABE_LEFT;
    }
    HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (tray) {
        RECT tr;
        GetWindowRect(tray, &tr);
        MONITORINFO mi = Mon();
        if (tr.bottom - tr.top > tr.right - tr.left) {
            int cx = (tr.left + tr.right) / 2;
            int mcx = (mi.rcMonitor.left + mi.rcMonitor.right) / 2;
            return cx < mcx ? ABE_RIGHT : ABE_LEFT;
        }
    }
    return ABE_LEFT;
}

static void ForceForeground(HWND h) {
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    if (!SetForegroundWindow(h)) {
        INPUT in[2] = {};
        in[0].type = INPUT_KEYBOARD;
        in[0].ki.wVk = VK_MENU;
        in[1] = in[0];
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(2, in, sizeof(INPUT));
        SetForegroundWindow(h);
    }
}

static int SnapWidth() { return Scale(g_settings.cardWidth) * 3 / 2; }
static int IconSize() { return Scale(40); }

// ================================================================ Bilder

static HBITMAP CreateDib(int w, int h, void** bits) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, bits, nullptr, 0);
}

static bool CaptureWindow(HWND h, Img& out, int maxW) {
    if (IsHungAppWindow(h)) return false;
    RECT wr;
    if (!GetWindowRect(h, &wr)) return false;
    RECT fb = FrameRect(h);
    int w = wr.right - wr.left, hh = wr.bottom - wr.top;
    if (w <= 0 || hh <= 0 || w > 16384 || hh > 16384) return false;
    int cx = fb.left - wr.left, cy = fb.top - wr.top;
    int cw = fb.right - fb.left, ch = fb.bottom - fb.top;
    if (cw <= 0 || ch <= 0 || cx < 0 || cy < 0 || cx + cw > w || cy + ch > hh) {
        cx = cy = 0;
        cw = w;
        ch = hh;
    }

    HDC sdc = GetDC(nullptr);
    HDC src = CreateCompatibleDC(sdc);
    HBITMAP sb = CreateCompatibleBitmap(sdc, w, hh);
    HGDIOBJ so = SelectObject(src, sb);
    bool ok = PrintWindow(h, src, PW_RENDERFULLCONTENT) != FALSE;
    if (ok) {
        int ow = imin(maxW, cw), oh = imax(1, MulDiv(ow, ch, cw));
        void* bits = nullptr;
        HBITMAP db = CreateDib(ow, oh, &bits);
        HDC dst = CreateCompatibleDC(sdc);
        HGDIOBJ dob = SelectObject(dst, db);
        SetStretchBltMode(dst, HALFTONE);
        SetBrushOrgEx(dst, 0, 0, nullptr);
        StretchBlt(dst, 0, 0, ow, oh, src, cx, cy, cw, ch, SRCCOPY);
        GdiFlush();
        uint32_t* p = (uint32_t*)bits;
        uint64_t sum = 0;
        for (int i = 0; i < ow * oh; i++) {
            p[i] |= 0xFF000000;
            sum += (p[i] & 0xFF) + ((p[i] >> 8) & 0xFF) + ((p[i] >> 16) & 0xFF);
        }
        bool black = sum < (uint64_t)ow * oh * 6;
        if (!(black && !out.px.empty())) {
            out.w = ow;
            out.h = oh;
            out.px.assign(p, p + ow * oh);
        }
        SelectObject(dst, dob);
        DeleteObject(db);
        DeleteDC(dst);
    }
    SelectObject(src, so);
    DeleteObject(sb);
    DeleteDC(src);
    ReleaseDC(nullptr, sdc);
    return ok;
}

static HICON LoadWindowIcon(HWND h, int size, bool& owned) {
    owned = false;
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (proc) {
        wchar_t path[MAX_PATH];
        DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(proc, 0, path, &n) &&
            !wcsstr(path, L"ApplicationFrameHost")) {
            HICON ic = nullptr;
            UINT id = 0;
            UINT got = PrivateExtractIconsW(path, 0, size, size, &ic, &id, 1, 0);
            if (got > 0 && got != 0xFFFFFFFF && ic) {
                CloseHandle(proc);
                owned = true;
                return ic;
            }
        }
        CloseHandle(proc);
    }
    DWORD_PTR r = 0;
    SendMessageTimeoutW(h, WM_GETICON, ICON_BIG, 0, SMTO_ABORTIFHUNG, 100, &r);
    if (!r) r = GetClassLongPtrW(h, GCLP_HICON);
    if (!r) SendMessageTimeoutW(h, WM_GETICON, ICON_SMALL2, 0, SMTO_ABORTIFHUNG, 100, &r);
    if (!r) r = (DWORD_PTR)LoadIconW(nullptr, IDI_APPLICATION);
    return (HICON)r;
}

static void IconToImg(HICON ic, int size, Img& out) {
    int n = size * size;
    void* bits = nullptr;
    HBITMAP b = CreateDib(size, size, &bits);
    if (!b) return;
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ ob = SelectObject(dc, b);
    uint32_t* p = (uint32_t*)bits;
    memset(p, 0, n * 4);
    DrawIconEx(dc, 0, 0, ic, size, size, 0, nullptr, DI_NORMAL);
    GdiFlush();
    bool hasAlpha = false;
    for (int i = 0; i < n && !hasAlpha; i++) hasAlpha = (p[i] >> 24) != 0;
    if (!hasAlpha) {
        std::vector<uint32_t> col(p, p + n);
        memset(p, 0, n * 4);
        DrawIconEx(dc, 0, 0, ic, size, size, 0, nullptr, DI_MASK);
        GdiFlush();
        for (int i = 0; i < n; i++)
            p[i] = (p[i] & 0xFFFFFF) ? 0 : (col[i] | 0xFF000000);
    }
    out.w = out.h = size;
    out.px.assign(p, p + n);
    SelectObject(dc, ob);
    DeleteObject(b);
    DeleteDC(dc);
}

// PKEY_AppUserModel_ID
static const PROPERTYKEY kPkeyAumid = {
    {0x9F4C2855, 0x9F79, 0x4B39, {0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3}}, 5};

static bool BitmapToImg(HBITMAP bmp, Img& out) {
    BITMAP bm{};
    if (!GetObjectW(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight == 0)
        return false;
    int w = bm.bmWidth, h = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    std::vector<uint32_t> px((size_t)w * h);
    HDC dc = GetDC(nullptr);
    int ok = GetDIBits(dc, bmp, 0, h, px.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (!ok) return false;

    bool anyAlpha = false, straight = false;
    for (uint32_t p : px) {
        uint32_t a = p >> 24;
        if (a) anyAlpha = true;
        if (((p >> 16) & 255) > a || ((p >> 8) & 255) > a || (p & 255) > a)
            straight = true;
    }
    for (uint32_t& p : px) {
        if (!anyAlpha) {
            p |= 0xFF000000;
        } else if (straight) {
            uint32_t a = p >> 24;
            p = (a << 24) | ((((p >> 16) & 255) * a / 255) << 16) |
                ((((p >> 8) & 255) * a / 255) << 8) | ((p & 255) * a / 255);
        }
    }
    out.w = w;
    out.h = h;
    out.px = std::move(px);
    return true;
}

// Holt dasselbe Symbol, das Taskleiste/Dock und Startmenü verwenden
// (über die AppUserModelID, auch für Store-Apps), sonst das Symbol der .exe.
static bool GetShellIcon(HWND h, int size, Img& out) {
    std::wstring parsing;
    IPropertyStore* ps = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(h, IID_PPV_ARGS(&ps)))) {
        PROPVARIANT pv;
        PropVariantInit(&pv);
        if (SUCCEEDED(ps->GetValue(kPkeyAumid, &pv)) && pv.vt == VT_LPWSTR &&
            pv.pwszVal && *pv.pwszVal)
            parsing = std::wstring(L"shell:AppsFolder\\") + pv.pwszVal;
        PropVariantClear(&pv);
        ps->Release();
    }
    if (parsing.empty()) {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (proc) {
            wchar_t path[MAX_PATH];
            DWORD n = MAX_PATH;
            if (QueryFullProcessImageNameW(proc, 0, path, &n)) parsing = path;
            CloseHandle(proc);
        }
    }
    if (parsing.empty()) return false;

    IShellItemImageFactory* f = nullptr;
    if (FAILED(SHCreateItemFromParsingName(parsing.c_str(), nullptr, IID_PPV_ARGS(&f))))
        return false;
    HBITMAP bmp = nullptr;
    SIZE sz{size, size};
    bool ok = SUCCEEDED(f->GetImage(sz, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &bmp)) &&
              bmp && BitmapToImg(bmp, out);
    if (bmp) DeleteObject(bmp);
    f->Release();
    return ok;
}

// ---------------------------------------------------------------- Dock-Symbole
// Liest die Symbol-Zuordnung des Mods "macOS Dock" direkt aus der
// Windhawk-Registry (Einstellungen + Werte aus dem Rechtsklick-Menü).

struct IApplicationResolver8 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetAppIDForShortcut(IShellItem*, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAppIDForShortcutObject(IShellLinkW*, IShellItem*, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetAppIDForWindow(HWND, LPWSTR*, void*, void*, void*) = 0;
};
static const CLSID kClsidAppResolver = {
    0x660b90c8, 0x73a9, 0x4b58, {0x8c, 0xae, 0x35, 0x5b, 0x7f, 0x55, 0x34, 0x1b}};
static const IID kIidAppResolver8 = {
    0xde25675a, 0x72de, 0x44b4, {0x93, 0x73, 0x05, 0x17, 0x04, 0x50, 0xc1, 0x40}};
static const CLSID kClsidWicFactory = {
    0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};

// Dieselbe App-ID, die die Taskleiste (und damit das Dock) verwendet.
static std::wstring WindowAppId(HWND h) {
    std::wstring id;
    IApplicationResolver8* r = nullptr;
    if (SUCCEEDED(CoCreateInstance(kClsidAppResolver, nullptr, CLSCTX_INPROC_SERVER,
                                   kIidAppResolver8, (void**)&r)) && r) {
        LPWSTR s = nullptr;
        if (SUCCEEDED(r->GetAppIDForWindow(h, &s, nullptr, nullptr, nullptr)) && s) {
            id = s;
            CoTaskMemFree(s);
        }
        r->Release();
    }
    if (id.empty()) {
        IPropertyStore* ps = nullptr;
        if (SUCCEEDED(SHGetPropertyStoreForWindow(h, IID_PPV_ARGS(&ps)))) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(ps->GetValue(kPkeyAumid, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal)
                id = pv.pwszVal;
            PropVariantClear(&pv);
            ps->Release();
        }
    }
    return id;
}

static std::wstring RegStr(HKEY key, const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ |
                     RRF_NOEXPAND, nullptr, nullptr, &size) != ERROR_SUCCESS || size < 2)
        return L"";
    std::wstring v(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ |
                     RRF_NOEXPAND, nullptr, v.data(), &size) != ERROR_SUCCESS)
        return L"";
    v.resize(wcslen(v.c_str()));
    return v;
}

static std::wstring TrimWs(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n\"");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n\"");
    return s.substr(a, b - a + 1);
}

// Sucht die PNG-Datei, die das Dock für diese App-ID verwendet ("" = keine).
static std::wstring DockIconPath(const std::wstring& appId) {
    if (appId.empty()) return L"";
    static const wchar_t* kMods[] = {L"local@macos-dock-taskbar", L"macos-dock-taskbar"};
    for (auto mod : kMods) {
        std::wstring base = std::wstring(L"SOFTWARE\\Windhawk\\Engine\\");
        HKEY hs = nullptr, hl = nullptr;
        RegOpenKeyExW(HKEY_LOCAL_MACHINE, (base + L"Mods\\" + mod + L"\\Settings").c_str(),
                      0, KEY_READ, &hs);
        RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      (base + L"ModsWritable\\" + mod + L"\\LocalStorage").c_str(), 0,
                      KEY_READ, &hl);
        if (!hs && !hl) continue;

        std::wstring folder = hl ? RegStr(hl, L"g.iconFolder") : L"";
        if (folder.empty() && hs) folder = RegStr(hs, L"iconFolder");
        if (folder.empty()) folder = L"%USERPROFILE%\\Documents\\MacOs-Dock\\Icons";

        // Rechtsklick-Zuordnung ("<App-ID>\t<Datei>" pro Zeile) hat Vorrang.
        std::wstring file;
        bool found = false;
        std::wstring all = hl ? RegStr(hl, L"g.icons") : L"";
        for (size_t st = 0; st < all.size() && !found;) {
            size_t e = all.find(L'\n', st);
            if (e == std::wstring::npos) e = all.size();
            std::wstring line = all.substr(st, e - st);
            size_t tab = line.find(L'\t');
            if (tab != std::wstring::npos &&
                !_wcsicmp(TrimWs(line.substr(0, tab)).c_str(), appId.c_str())) {
                file = TrimWs(line.substr(tab + 1));
                found = true;
            }
            st = e + 1;
        }
        // Sonst die Liste "Apps und Symbole" aus den Mod-Einstellungen.
        for (int i = 0; hs && !found && i < 200; i++) {
            wchar_t n[64];
            swprintf(n, 64, L"apps[%d].ids", i);
            std::wstring ids = RegStr(hs, n);
            swprintf(n, 64, L"apps[%d].file", i);
            std::wstring f = TrimWs(RegStr(hs, n));
            swprintf(n, 64, L"apps[%d].name", i);
            if (ids.empty() && f.empty() && RegStr(hs, n).empty()) break;
            for (size_t st = 0; st <= ids.size() && !found;) {
                size_t e = ids.find(L',', st);
                if (e == std::wstring::npos) e = ids.size();
                if (!_wcsicmp(TrimWs(ids.substr(st, e - st)).c_str(), appId.c_str())) {
                    file = f;
                    found = true;
                }
                st = e + 1;
            }
        }
        if (hs) RegCloseKey(hs);
        if (hl) RegCloseKey(hl);
        if (!found || file.empty() || file == L"-") return L"";

        std::wstring path = (file.find(L':') != std::wstring::npos ||
                             file.rfind(L"\\\\", 0) == 0)
                                ? file
                                : TrimWs(folder) + L"\\" + file;
        wchar_t exp[MAX_PATH * 2];
        DWORD len = ExpandEnvironmentStringsW(path.c_str(), exp, ARRAYSIZE(exp));
        if (len > 0 && len <= ARRAYSIZE(exp)) path = exp;
        return GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES ? L"" : path;
    }
    return L"";
}

// Lädt ein Bild (PNG, ICO, JPG …) quadratisch eingepasst, vormultipliert.
static bool LoadImageFile(const std::wstring& path, int size, Img& out) {
    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(kClsidWicFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&f))))
        return false;
    bool ok = false;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* fr = nullptr;
    IWICBitmapScaler* sc = nullptr;
    IWICFormatConverter* cv = nullptr;
    UINT w = 0, h = 0;
    if (SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &fr)) && SUCCEEDED(fr->GetSize(&w, &h)) && w && h) {
        UINT tw = w >= h ? size : imax(1, MulDiv(size, w, h));
        UINT th = h >= w ? size : imax(1, MulDiv(size, h, w));
        if (SUCCEEDED(f->CreateBitmapScaler(&sc)) &&
            SUCCEEDED(sc->Initialize(fr, tw, th, WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(f->CreateFormatConverter(&cv)) &&
            SUCCEEDED(cv->Initialize(sc, GUID_WICPixelFormat32bppPBGRA,
                                     WICBitmapDitherTypeNone, nullptr, 0,
                                     WICBitmapPaletteTypeCustom))) {
            std::vector<uint32_t> px((size_t)tw * th);
            if (SUCCEEDED(cv->CopyPixels(nullptr, tw * 4, tw * th * 4, (BYTE*)px.data()))) {
                out.w = out.h = size;
                out.px.assign((size_t)size * size, 0);
                int ox = (size - (int)tw) / 2, oy = (size - (int)th) / 2;
                for (UINT y = 0; y < th; y++)
                    memcpy(&out.px[(size_t)(y + oy) * size + ox], &px[(size_t)y * tw], tw * 4);
                ok = true;
            }
        }
    }
    if (cv) cv->Release();
    if (sc) sc->Release();
    if (fr) fr->Release();
    if (dec) dec->Release();
    f->Release();
    return ok;
}

static bool GetDockIcon(HWND h, int size, Img& out) {
    if (!g_settings.dockIcons) return false;
    std::wstring path = DockIconPath(WindowAppId(h));
    return !path.empty() && LoadImageFile(path, size, out);
}

static void LoadIconFor(StagedWindow& s) {
    if (GetDockIcon(s.hwnd, IconSize() * 2, s.icon)) return;
    if (GetShellIcon(s.hwnd, IconSize() * 2, s.icon)) return;
    bool owned;
    HICON ic = LoadWindowIcon(s.hwnd, IconSize(), owned);
    if (ic) IconToImg(ic, IconSize(), s.icon);
    if (owned && ic) DestroyIcon(ic);
}

// ================================================================ Rendering

static float SdRoundRect(float x, float y, float w, float h, float r) {
    r = fminf(r, fminf(w, h) * 0.5f);
    float qx = fabsf(x - w * 0.5f) - (w * 0.5f - r);
    float qy = fabsf(y - h * 0.5f) - (h * 0.5f - r);
    float ax = fmaxf(qx, 0.f), ay = fmaxf(qy, 0.f);
    return sqrtf(ax * ax + ay * ay) + fminf(fmaxf(qx, qy), 0.f) - r;
}

static void Sample(const Img& im, float fx, float fy, float c[4]) {
    if (im.px.empty() || im.w <= 0 || im.h <= 0) return;
    fx -= 0.5f;
    fy -= 0.5f;
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > im.w - 1) fx = (float)(im.w - 1);
    if (fy > im.h - 1) fy = (float)(im.h - 1);
    int x0 = (int)fx, y0 = (int)fy;
    int x1 = x0 + 1 < im.w ? x0 + 1 : x0, y1 = y0 + 1 < im.h ? y0 + 1 : y0;
    float tx = fx - x0, ty = fy - y0;
    uint32_t p00 = im.px[y0 * im.w + x0], p10 = im.px[y0 * im.w + x1];
    uint32_t p01 = im.px[y1 * im.w + x0], p11 = im.px[y1 * im.w + x1];
    for (int k = 0; k < 4; k++) {
        int sh = k * 8;
        float a = (float)((p00 >> sh) & 255), b = (float)((p10 >> sh) & 255);
        float cc = (float)((p01 >> sh) & 255), d = (float)((p11 >> sh) & 255);
        c[k] = ((a + (b - a) * tx) * (1 - ty) + (cc + (d - cc) * tx) * ty) / 255.f;
    }
}

// Where DrawCard / DrawAppIcon draw: a card's layer, placed at g_dstX/g_dstY in
// surface coordinates.
static uint32_t* g_dst;
static int g_dstX, g_dstY, g_dstW, g_dstH;

static inline void BlendPx(int x, int y, float b, float g, float r, float a) {
    x -= g_dstX;
    y -= g_dstY;
    if (a <= 0.002f || x < 0 || y < 0 || x >= g_dstW || y >= g_dstH) return;
    uint32_t& d = g_dst[y * g_dstW + x];
    float ia = 1.f - a;
    auto ch = [&](float s, int sh) {
        int v = (int)(s * 255.f + ((d >> sh) & 255) * ia + 0.5f);
        return (uint32_t)(v > 255 ? 255 : v);
    };
    uint32_t na = ch(a, 24), nr = ch(r, 16), ng = ch(g, 8), nb = ch(b, 0);
    d = (na << 24) | (nr << 16) | (ng << 8) | nb;
}

struct CardGeo {
    float x0, cw, ch, cx, cy;
};

static CardGeo Geo(const StagedWindow& s) {
    float sc = 1.f + 0.07f * s.hover;
    CardGeo g;
    g.cw = (s.card.right - s.card.left) * sc;
    g.ch = (s.card.bottom - s.card.top) * sc;
    g.cx = (s.card.left + s.card.right) * 0.5f;
    g.cy = (s.card.top + s.card.bottom) * 0.5f;
    // Außenkante bleibt fest, damit alle Karten auf einer Linie fluchten
    g.x0 = g_edge == ABE_LEFT ? (float)s.card.left : (float)s.card.right - g.cw;
    g.cx = g.x0 + g.cw * 0.5f;
    return g;
}

// 3D-Drehung wie bei macOS: Drehachse ist die Außenkante der Karte (zum
// Bildschirmrand). Sie bleibt voll hoch und liegt bei allen Karten auf einer
// Linie, die Innenseite liegt weiter hinten und wird perspektivisch kleiner.
// Alle Karten teilen sich eine Kamera auf Höhe der Bildschirmmitte, deshalb
// laufen benachbarte Kanten parallel (oben fallen sie, unten steigen sie).
struct Persp {
    float sn, cs, D;
};

static Persp CardPersp(const StagedWindow& s, const CardGeo& g) {
    float st = g_settings.tilt ? 0.15f * (1.f - s.hover) : 0.f;
    Persp p;
    p.sn = st;
    p.cs = sqrtf(1.f - st * st);
    p.D = 1.6f * (float)Scale(g_settings.cardWidth);
    return p;
}

static void DrawCard(const StagedWindow& s) {
    CardGeo g = Geo(s);
    Persp P = CardPersp(s, g);
    bool left = g_edge == ABE_LEFT;
    float outer = left ? g.x0 : g.x0 + g.cw;  // Bildschirm-x der Außenkante
    float rad = (float)Scale(10) * (1.f + 0.07f * s.hover);
    float sr = (float)Scale(18), soff = (float)Scale(6);
    float shadowBase = 0.24f + 0.14f * s.hover;
    float depthShade = 0.9f * P.sn;
    float Yc = g_H * 0.5f;                 // Höhe der Kamera (Fluchtpunkt)
    float top = g.cy - g.ch * 0.5f;

    int bx0 = imax(0, (int)floorf(g.x0 - sr));
    int bx1 = imin(g_W, (int)ceilf(g.x0 + g.cw + sr));
    int by0 = imax(0, (int)floorf(g.cy - g.ch * 0.5f - sr));
    int by1 = imin(g_H, (int)ceilf(g.cy + g.ch * 0.5f + sr + soff));

    for (int y = by0; y < by1; y++) {
        for (int x = bx0; x < bx1; x++) {
            float px = x + 0.5f;
            float sx = left ? px - outer : outer - px;  // Abstand von der Außenkante
            float sy = y + 0.5f - Yc;
            float den = P.D * P.cs - sx * P.sn;
            if (den <= 1.f) continue;
            float Xo = sx * P.D / den;                  // Kartenabstand von der Außenkante
            float k = P.D / (P.D + Xo * P.sn);          // Pixel pro Karteneinheit
            float uo = Xo / g.cw;                       // 0 außen … 1 innen
            float u = left ? uo : 1.f - uo;
            float lx = u * g.cw, ly = Yc + sy / k - top;

            float d = SdRoundRect(lx, ly, g.cw, g.ch, rad) * k;
            float a = Sat(0.5f - d);
            float ds = SdRoundRect(lx, Yc + (sy - soff) / k - top, g.cw, g.ch, rad) * k;
            float sa = shadowBase * (1.f - SmoothStep(-sr * 0.25f, sr, ds));

            float c[4] = {0.86f, 0.86f, 0.88f, 1.f};
            if (a > 0) {
                Sample(s.snap, u * s.snap.w, ly / g.ch * s.snap.h, c);
                float shade = 1.f - depthShade * Sat(uo);
                float border = Sat(1.5f + d) * 0.28f;
                for (int q = 0; q < 3; q++)
                    c[q] = c[q] * shade * (1.f - border) + border;
            }
            float outA = a + sa * (1.f - a);
            BlendPx(x, y, c[0] * a, c[1] * a, c[2] * a, outA);
        }
    }
}

static void DrawAppIcon(const StagedWindow& s) {
    if (s.icon.px.empty()) return;
    CardGeo g = Geo(s);
    bool left = g_edge == ABE_LEFT;
    // Untere Ecke der Außenkante (dort ist die Karte nicht verkleinert)
    float ex = left ? g.x0 : g.x0 + g.cw;
    float ey = g.cy + g.ch * 0.5f;
    float isz = (float)Scale(34) * (1.f + 0.07f * s.hover);
    float ix = left ? ex - isz * 0.3f : ex - isz * 0.7f;
    float iy = ey - isz * 0.65f;
    int x0 = imax(0, (int)floorf(ix)), x1 = imin(g_W, (int)ceilf(ix + isz));
    int y0 = imax(0, (int)floorf(iy)), y1 = imin(g_H, (int)ceilf(iy + isz));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            float fx = (x + 0.5f - ix) / isz * s.icon.w;
            float fy = (y + 0.5f - iy) / isz * s.icon.h;
            if (fx < 0 || fy < 0 || fx > s.icon.w || fy > s.icon.h) continue;
            float c[4] = {0, 0, 0, 0};
            Sample(s.icon, fx, fy, c);
            BlendPx(x, y, c[0], c[1], c[2], c[3]);
        }
}

static void EnsureSurface(int w, int h) {
    if (w == g_W && h == g_H && g_dib) return;
    if (g_dib) {
        SelectObject(g_memDC, g_oldBmp);
        DeleteObject(g_dib);
        g_dib = nullptr;
    }
    void* bits = nullptr;
    g_dib = CreateDib(w, h, &bits);
    if (!g_dib) {
        g_bits = nullptr;
        g_W = g_H = 0;
        return;
    }
    g_oldBmp = SelectObject(g_memDC, g_dib);
    g_bits = (uint32_t*)bits;
    g_W = w;
    g_H = h;
}

static void Layout() {
    MONITORINFO mi = Mon();
    RECT wk = mi.rcWork;
    int cardW = Scale(g_settings.cardWidth);
    int margin = Scale(26), gap = Scale(28);
    int W = cardW + 2 * margin + Scale(24);
    int H = wk.bottom - wk.top;
    EnsureSurface(W, H);
    bool left = g_edge == ABE_LEFT;
    g_pos.x = left ? wk.left : wk.right - W;
    g_pos.y = wk.top;

    int n = (int)g_staged.size();
    if (!n) return;
    std::vector<int> hs(n);
    int sumH = 0;
    for (int i = 0; i < n; i++) {
        const Img& im = g_staged[i].snap;
        int h = im.w > 0 ? MulDiv(cardW, im.h, im.w) : cardW * 10 / 16;
        h = imax(cardW / 3, imin(h, cardW));
        hs[i] = h;
        sumH += h;
    }
    int avail = H - 2 * margin - gap * (n - 1);
    float f = sumH > avail ? (float)avail / sumH : 1.f;
    int total = gap * (n - 1);
    for (int i = 0; i < n; i++) total += (int)(hs[i] * f);
    int y = imax(margin, (H - total) / 2);
    int cw = (int)(cardW * f);
    for (int i = 0; i < n; i++) {
        int ch = (int)(hs[i] * f);
        int x = left ? margin : W - margin - cw;
        g_staged[i].card = {x, y, x + cw, y + ch};
        y += ch + gap;
    }
}

static void ApplyPosition() {
    if (!g_strip || !g_dib) return;
    if (!g_enabled || g_staged.empty() || g_slide >= 0.999f) {
        if (IsWindowVisible(g_strip)) ShowWindow(g_strip, SW_HIDE);
        return;
    }
    float e = g_slide * g_slide * (3 - 2 * g_slide);
    POINT pt = g_pos;
    int off = (int)(e * g_W);
    pt.x += g_edge == ABE_LEFT ? -off : off;
    SIZE sz{g_W, g_H};
    POINT zero{0, 0};
    BLENDFUNCTION bf{AC_SRC_OVER, 0, (BYTE)(255 * (1 - e)), AC_SRC_ALPHA};
    GdiFlush();
    UpdateLayeredWindow(g_strip, nullptr, &pt, &sz, g_memDC, &zero, 0, &bf, ULW_ALPHA);
    if (!IsWindowVisible(g_strip))
        SetWindowPos(g_strip, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

// The surface area a card's drawing can touch: its shadow box and its icon.
static RECT LayerBox(const StagedWindow& s) {
    CardGeo g = Geo(s);
    float sr = (float)Scale(18), soff = (float)Scale(6);
    RECT box{(int)floorf(g.x0 - sr), (int)floorf(g.cy - g.ch * 0.5f - sr),
             (int)ceilf(g.x0 + g.cw + sr), (int)ceilf(g.cy + g.ch * 0.5f + sr + soff)};
    bool left = g_edge == ABE_LEFT;
    float isz = (float)Scale(34) * (1.f + 0.07f * s.hover);
    float ex = left ? g.x0 : g.x0 + g.cw, ey = g.cy + g.ch * 0.5f;
    float ix = left ? ex - isz * 0.3f : ex - isz * 0.7f, iy = ey - isz * 0.65f;
    RECT icon{(int)floorf(ix), (int)floorf(iy), (int)ceilf(ix + isz), (int)ceilf(iy + isz)};
    UnionRect(&box, &box, &icon);
    RECT surf{0, 0, g_W, g_H};
    IntersectRect(&box, &box, &surf);
    return box;
}

static bool SameKey(const LayerKey& a, const LayerKey& b) {
    return EqualRect(&a.card, &b.card) && a.hover == b.hover && a.edge == b.edge &&
           a.surfW == b.surfW && a.surfH == b.surfH && a.tilt == b.tilt &&
           a.cardW == b.cardW && a.scale100 == b.scale100;
}

// Redraws a card's layer only when something it depends on changed, so a hover
// animation repaints just the cards that are moving.
static void EnsureLayer(StagedWindow& s) {
    LayerKey key{s.card, s.hover, (int)g_edge, g_W, g_H, g_settings.tilt ? 1 : 0,
                 Scale(g_settings.cardWidth), Scale(100)};
    if (s.layerValid && SameKey(key, s.layerKey)) return;
    s.layerKey = key;
    s.layerValid = true;
    s.layerBox = LayerBox(s);
    int w = s.layerBox.right - s.layerBox.left, h = s.layerBox.bottom - s.layerBox.top;
    s.layer.assign(w > 0 && h > 0 ? (size_t)w * h : 0, 0);
    if (s.layer.empty()) return;
    g_dst = s.layer.data();
    g_dstX = s.layerBox.left;
    g_dstY = s.layerBox.top;
    g_dstW = w;
    g_dstH = h;
    DrawCard(s);
    DrawAppIcon(s);
}

// Premultiplied "over" of a card's layer onto the surface.
static void CompositeLayer(const StagedWindow& s) {
    int w = s.layerBox.right - s.layerBox.left, h = s.layerBox.bottom - s.layerBox.top;
    if (s.layer.empty()) return;
    for (int y = 0; y < h; y++) {
        const uint32_t* src = s.layer.data() + (size_t)y * w;
        uint32_t* dst = g_bits + (size_t)(y + s.layerBox.top) * g_W + s.layerBox.left;
        for (int x = 0; x < w; x++) {
            uint32_t p = src[x];
            uint32_t sa = p >> 24;
            if (!sa) continue;
            if (sa == 255) {
                dst[x] = p;
                continue;
            }
            uint32_t d = dst[x], inv = 255 - sa, out = 0;
            for (int sh = 0; sh < 32; sh += 8) {
                uint32_t v = ((p >> sh) & 255) + (((d >> sh) & 255) * inv + 127) / 255;
                out |= (v > 255 ? 255 : v) << sh;
            }
            dst[x] = out;
        }
    }
}

static void Render() {
    if (!g_bits) return;
    memset(g_bits, 0, (size_t)g_W * g_H * 4);
    for (int i = (int)g_staged.size() - 1; i >= 0; i--) {
        if (i == g_hoverIdx) continue;
        EnsureLayer(g_staged[i]);
        CompositeLayer(g_staged[i]);
    }
    if (g_hoverIdx >= 0 && g_hoverIdx < (int)g_staged.size()) {
        EnsureLayer(g_staged[g_hoverIdx]);
        CompositeLayer(g_staged[g_hoverIdx]);
    }
    ApplyPosition();
}

static void Refresh() {
    Layout();
    Render();
}

static void StartAnim() {
    if (!g_animRunning) {
        SetTimer(g_strip, TIMER_ANIM, 15, nullptr);
        g_animRunning = true;
    }
}

static void AnimTick() {
    bool busy = false, redraw = false;
    for (size_t i = 0; i < g_staged.size(); i++) {
        auto& s = g_staged[i];
        float target = (int)i == g_hoverIdx ? 1.f : 0.f;
        float d = target - s.hover;
        if (fabsf(d) > 0.01f) {
            s.hover += d * 0.3f;
            busy = redraw = true;
        } else if (s.hover != target) {
            s.hover = target;
            redraw = true;
        }
    }
    if (g_slide != g_slideTarget) {
        const float step = 0.09f;
        g_slide += g_slideTarget > g_slide ? step : -step;
        if (fabsf(g_slide - g_slideTarget) < step) g_slide = g_slideTarget;
        if (g_slide != g_slideTarget) busy = true;
        if (!redraw) ApplyPosition();
    }
    if (redraw) Render();
    if (!busy) {
        KillTimer(g_strip, TIMER_ANIM);
        g_animRunning = false;
    }
}

static void SetSlideTarget(float t) {
    if (t != g_slideTarget) {
        g_slideTarget = t;
        StartAnim();
    }
}

// ================================================================ Zonen

static void ZoneX(int& zl, int& zr) {
    int zoneW = Scale(26) + Scale(g_settings.cardWidth) + Scale(8);
    if (g_edge == ABE_LEFT) {
        zl = g_pos.x;
        zr = g_pos.x + zoneW;
    } else {
        zr = g_pos.x + g_W;
        zl = zr - zoneW;
    }
}

static RECT CardsZoneScreen() {
    int zl, zr;
    ZoneX(zl, zr);
    int top = INT_MAX, bot = INT_MIN;
    for (auto& s : g_staged) {
        top = imin(top, (int)s.card.top);
        bot = imax(bot, (int)s.card.bottom);
    }
    if (top == INT_MAX) {
        top = g_pos.y;
        bot = g_pos.y + g_H;
    } else {
        top += g_pos.y - Scale(10);
        bot += g_pos.y + Scale(26);
    }
    return RECT{zl, top, zr, bot};
}

static bool ActiveCovers() {
    HWND a = g_active;
    if (!a || !IsWindow(a) || IsIconic(a) || !IsWindowVisible(a) || IsCloaked(a))
        return false;
    if (IsZoomed(a)) return true;
    RECT r = FrameRect(a), z = CardsZoneScreen(), i;
    return IntersectRect(&i, &r, &z) != 0;
}

static bool ForegroundIsFullscreen() {
    HWND fg = GetForegroundWindow();
    if (!fg || fg == g_strip || fg == g_trayWnd || IsShellClass(fg)) return false;
    if (MonitorFromWindow(fg, MONITOR_DEFAULTTONEAREST) != StripMonitor()) return false;
    RECT r;
    GetWindowRect(fg, &r);
    MONITORINFO mi = Mon();
    return r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top &&
           r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
}

static void MoveAside(HWND h) {
    if (!g_settings.moveAside || !h || g_staged.empty() || IsZoomed(h) || IsIconic(h))
        return;
    RECT r = FrameRect(h);
    MONITORINFO mi = Mon();
    int zl, zr;
    ZoneX(zl, zr);
    int w = r.right - r.left, dx = 0;
    if (g_edge == ABE_LEFT) {
        int b = zr + Scale(12);
        if (r.left < b && b + w <= mi.rcWork.right) dx = b - r.left;
    } else {
        int b = zl - Scale(12);
        if (r.right > b && b - w >= mi.rcWork.left) dx = b - r.right;
    }
    if (!dx) return;
    RECT wr;
    GetWindowRect(h, &wr);
    SetWindowPos(h, nullptr, wr.left + dx, wr.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

static void Poll() {
    // Runs every 60 ms; with nothing staged the strip is hidden anyway.
    if (!g_started || !g_enabled || g_staged.empty()) return;
    bool full = ForegroundIsFullscreen();
    bool covered = full || ActiveCovers();
    if (!covered || full) {
        g_peek = false;
    } else {
        POINT pt;
        GetCursorPos(&pt);
        MONITORINFO mi = Mon();
        bool inY = pt.y >= mi.rcMonitor.top && pt.y < mi.rcMonitor.bottom;
        bool atEdge = inY && (g_edge == ABE_LEFT ? pt.x <= mi.rcMonitor.left + 1
                                                 : pt.x >= mi.rcMonitor.right - 2);
        int btn = GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON;
        bool pressed = (GetAsyncKeyState(btn) & 0x8000) != 0;
        if (!g_peek) {
            if (atEdge && !pressed) g_peek = true;
        } else {
            RECT z = CardsZoneScreen();
            int dist = g_edge == ABE_LEFT ? pt.x - z.right : z.left - pt.x;
            if (dist > Scale(40) || !inY) g_peek = false;
        }
    }
    SetSlideTarget(covered && !g_peek ? 1.f : 0.f);
}

// ================================================================ Fenster

static bool StageOut(HWND h) {
    if (FindStaged(h) >= 0) return true;
    if (IsIconic(h) || IsHungAppWindow(h)) return false;

    StagedWindow s{};
    s.hwnd = h;
    s.wp.length = sizeof(s.wp);
    if (!GetWindowPlacement(h, &s.wp)) return false;

    CaptureWindow(h, s.snap, SnapWidth());
    LoadIconFor(s);

    if (IsZoomed(h)) {
        WINDOWPLACEMENT n = s.wp;
        n.showCmd = SW_SHOWNOACTIVATE;
        n.flags = 0;
        SetWindowPlacement(h, &n);
    }
    GetWindowRect(h, &s.rect);

    int x = GetSystemMetrics(SM_XVIRTUALSCREEN) +
            GetSystemMetrics(SM_CXVIRTUALSCREEN) + 200;
    if (!SetWindowPos(h, nullptr, x, s.rect.top, 0, 0,
                      SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER)) {
        SetWindowPlacement(h, &s.wp);
        return false;
    }
    g_staged.insert(g_staged.begin(), std::move(s));
    return true;
}

static void RestoreWindow(StagedWindow& s, bool minimize) {
    if (!IsWindow(s.hwnd)) return;
    if (IsHungAppWindow(s.hwnd)) {
        SetWindowPos(s.hwnd, nullptr, s.rect.left, s.rect.top, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
        return;
    }
    WINDOWPLACEMENT wp = s.wp;
    bool wasMax = wp.showCmd == SW_SHOWMAXIMIZED;
    if (minimize) {
        wp.showCmd = SW_SHOWMINNOACTIVE;
        wp.flags = wasMax ? WPF_RESTORETOMAXIMIZED : 0;
    } else if (!wasMax) {
        wp.showCmd = SW_SHOWNOACTIVATE;
    }
    SetWindowPlacement(s.hwnd, &wp);
}

static void Trim() {
    while ((int)g_staged.size() > g_settings.maxWindows) {
        RestoreWindow(g_staged.back(), true);
        g_staged.pop_back();
    }
}

static void StageOutPrevious(HWND old, HWND now) {
    if (old && old != now && IsManageable(old)) StageOut(old);
}

static void Promote(int i, bool activate) {
    StagedWindow s = std::move(g_staged[i]);
    g_staged.erase(g_staged.begin() + i);
    RestoreWindow(s, false);

    HWND old = g_active;
    g_active = s.hwnd;
    g_seen.insert(s.hwnd);
    g_peek = false;
    g_hoverIdx = -1;
    StageOutPrevious(old, s.hwnd);
    Trim();
    Layout();
    MoveAside(s.hwnd);
    Render();
    if (activate) ForceForeground(s.hwnd);
    Poll();
}

static void OnForeground(HWND h) {
    if (!h || !g_enabled) return;
    h = GetAncestor(h, GA_ROOTOWNER);
    if (!h || h == g_active || h == g_strip || h == g_trayWnd) return;

    int i = FindStaged(h);
    if (i >= 0) {
        Promote(i, false);
        return;
    }
    if (!IsManageable(h)) return;

    HWND old = g_active;
    g_active = h;
    bool firstTime = g_seen.insert(h).second;
    StageOutPrevious(old, h);
    Trim();
    Layout();
    if (firstTime) MoveAside(h);
    Render();
    Poll();
}

static void OnGone(HWND h, bool hidden) {
    if (!hidden) g_seen.erase(h);
    if (h == g_active) g_active = nullptr;
    int i = FindStaged(h);
    if (i < 0) return;
    RECT r = g_staged[i].rect;
    g_staged.erase(g_staged.begin() + i);
    g_hoverIdx = -1;
    if (hidden && IsWindow(h))
        SetWindowPos(h, nullptr, r.left, r.top, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    Refresh();
}

static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD ev, HWND h, LONG idObj,
                                  LONG idChild, DWORD, DWORD) {
    if (!h || !g_strip) return;
    if (ev == EVENT_SYSTEM_FOREGROUND) {
        PostMessageW(g_strip, WM_APP_FG, (WPARAM)h, 0);
        return;
    }
    if (idObj != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (ev == EVENT_OBJECT_DESTROY || ev == EVENT_OBJECT_HIDE) {
        if (h == g_active || FindStaged(h) >= 0)
            PostMessageW(g_strip,
                         ev == EVENT_OBJECT_DESTROY ? WM_APP_DESTROYED : WM_APP_HIDDEN,
                         (WPARAM)h, 0);
    }
}

static BOOL CALLBACK CollectProc(HWND h, LPARAM lp) {
    if (IsManageable(h) && !IsIconic(h)) ((std::vector<HWND>*)lp)->push_back(h);
    return TRUE;
}

static void InitialCollect() {
    std::vector<HWND> list;
    EnumWindows(CollectProc, (LPARAM)&list);
    HWND fg = GetForegroundWindow();
    if (fg) fg = GetAncestor(fg, GA_ROOTOWNER);
    g_active = IsManageable(fg) ? fg : (list.empty() ? nullptr : list[0]);
    for (auto it = list.rbegin(); it != list.rend(); ++it)
        if (*it != g_active) StageOut(*it);
    for (HWND h : list) g_seen.insert(h);
    Trim();
    Layout();
    MoveAside(g_active);
    Render();
    Poll();
}

static void SnapTick() {
    // PrintWindow makes the app redraw its whole window and DWM read it back, so
    // it's left out while cards are animating, where it would make them stutter.
    if (!g_started || !g_enabled || g_staged.empty() || g_slide >= 0.999f || g_animRunning)
        return;
    if (g_snapCursor >= g_staged.size()) g_snapCursor = 0;
    auto& s = g_staged[g_snapCursor++];
    int ow = s.snap.w, oh = s.snap.h;
    if (CaptureWindow(s.hwnd, s.snap, SnapWidth())) {
        s.layerValid = false;
        if (s.snap.w != ow || s.snap.h != oh) Layout();
        Render();
    }
}

// ================================================================ Tray / Kürzel

static bool TaskbarIsLight() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v,
                     &sz) == ERROR_SUCCESS)
        return v != 0;
    return false;
}

static HICON MakeTrayIcon(bool on) {
    int S = GetSystemMetrics(SM_CXSMICON);
    void* bits = nullptr;
    HBITMAP color = CreateDib(S, S, &bits);
    if (!color) return nullptr;
    uint32_t rgb = TaskbarIsLight() ? 0x000000 : 0xFFFFFF;
    uint32_t* p = (uint32_t*)bits;
    struct R { float x, y, w, h; } rs[] = {
        {0.04f, 0.17f, 0.22f, 0.18f}, {0.04f, 0.41f, 0.22f, 0.18f},
        {0.04f, 0.65f, 0.22f, 0.18f}, {0.34f, 0.17f, 0.62f, 0.66f}};
    for (int y = 0; y < S; y++)
        for (int x = 0; x < S; x++) {
            float a = 0;
            for (int k = 0; k < 4; k++) {
                float d = SdRoundRect(x + 0.5f - rs[k].x * S, y + 0.5f - rs[k].y * S,
                                      rs[k].w * S, rs[k].h * S, S * 0.07f);
                float c = Sat(0.5f - d);
                if (k == 3 && !on) c *= Sat(d + 1.6f);  // aus: nur Umriss
                if (k < 3 && !on) c *= 0.55f;
                a = fmaxf(a, c);
            }
            p[y * S + x] = ((uint32_t)(a * 255.f + 0.5f) << 24) | rgb;
        }
    std::vector<BYTE> zeros(((S + 15) / 16) * 2 * S, 0);
    HBITMAP mask = CreateBitmap(S, S, 1, 1, zeros.data());
    ICONINFO ii{TRUE, 0, 0, mask, color};
    HICON ic = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(color);
    return ic;
}

static void AddTray() {
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    if (!g_settings.trayIcon) {
        if (g_trayAdded) Shell_NotifyIconW(NIM_DELETE, &nid);
        g_trayAdded = false;
        return;
    }
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = g_trayIcon;
    lstrcpynW(nid.szTip, g_enabled ? L"Stage Manager: an" : L"Stage Manager: aus",
              ARRAYSIZE(nid.szTip));
    if (Shell_NotifyIconW(g_trayAdded ? NIM_MODIFY : NIM_ADD, &nid)) {
        if (!g_trayAdded) {
            nid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &nid);
        }
        g_trayAdded = true;
    }
}

static void UpdateTray() {
    HICON old = g_trayIcon;
    g_trayIcon = MakeTrayIcon(g_enabled);
    AddTray();
    if (old) DestroyIcon(old);
}

static void RemoveTray() {
    if (!g_trayAdded) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_trayWnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_trayAdded = false;
}

static void UpdateHotkey() {
    if (g_hotkeyRegistered) {
        UnregisterHotKey(g_trayWnd, HOTKEY_ID);
        g_hotkeyRegistered = false;
    }
    if (g_settings.hotkey)
        g_hotkeyRegistered =
            RegisterHotKey(g_trayWnd, HOTKEY_ID, MOD_WIN | MOD_ALT | MOD_NOREPEAT, 'S') != 0;
}

static void SetEnabled(bool en) {
    if (en == g_enabled) return;
    g_enabled = en;
    Wh_SetIntValue(L"enabled", en ? 1 : 0);
    if (g_started) {
        if (en) {
            g_slide = g_slideTarget = 0;
            InitialCollect();
        } else {
            for (auto& s : g_staged) RestoreWindow(s, false);
            g_staged.clear();
            g_active = nullptr;
            g_peek = false;
            g_hoverIdx = -1;
            ShowWindow(g_strip, SW_HIDE);
        }
    }
    UpdateTray();
    // Andere Mods (Menüleiste) über die Änderung informieren
    static UINT msgChanged = RegisterWindowMessageW(L"WhStageManagerChanged");
    PostMessageW(HWND_BROADCAST, msgChanged, g_enabled ? 1 : 0, 0);
}

static void ShowTrayMenu(int x, int y) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_enabled ? MF_CHECKED : 0), ID_TOGGLE, L"Stage Manager");
    if (g_settings.hotkey) {
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(m, MF_STRING | MF_GRAYED, 0, L"Tastenkürzel: Win+Alt+S");
    }
    SetForegroundWindow(g_trayWnd);
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, x, y, 0, g_trayWnd, nullptr);
    PostMessageW(g_trayWnd, WM_NULL, 0, 0);
    DestroyMenu(m);
}

// ================================================================ Start / Ende

static void Start() {
    g_started = true;
    g_edge = ComputeEdge();
    g_hookFg = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                               nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    g_hookObj = SetWinEventHook(EVENT_OBJECT_DESTROY, EVENT_OBJECT_HIDE, nullptr,
                                WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
    g_enabled = Wh_GetIntValue(L"enabled", 1) != 0;
    UpdateTray();
    UpdateHotkey();
    Layout();
    if (g_enabled) InitialCollect();
    SetTimer(g_strip, TIMER_POLL, 60, nullptr);
    // One card per tick, so each snapshot refreshes every n × 2 s.
    SetTimer(g_strip, TIMER_SNAP, 2000, nullptr);
}

static void Shutdown() {
    KillTimer(g_strip, TIMER_CHECK);
    KillTimer(g_strip, TIMER_POLL);
    KillTimer(g_strip, TIMER_SNAP);
    KillTimer(g_strip, TIMER_ANIM);
    if (g_hookFg) UnhookWinEvent(g_hookFg);
    if (g_hookObj) UnhookWinEvent(g_hookObj);
    g_hookFg = g_hookObj = nullptr;
    for (auto& s : g_staged) RestoreWindow(s, false);
    g_staged.clear();
    RemoveTray();
    if (g_hotkeyRegistered) UnregisterHotKey(g_trayWnd, HOTKEY_ID);
    g_hotkeyRegistered = false;
    if (g_trayIcon) DestroyIcon(g_trayIcon);
    g_trayIcon = nullptr;
    if (g_active && IsWindow(g_active)) SetForegroundWindow(g_active);
}

static void OnCheckTimer() {
    if (!g_started) {
        HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        DWORD pid = 0;
        if (tray) GetWindowThreadProcessId(tray, &pid);
        if (pid == GetCurrentProcessId()) Start();
        return;
    }
    UINT e = ComputeEdge();
    bool changed = false;
    if (e != g_edge) {
        g_edge = e;
        changed = true;
    }
    for (int i = (int)g_staged.size() - 1; i >= 0; i--)
        if (!IsWindow(g_staged[i].hwnd)) {
            g_staged.erase(g_staged.begin() + i);
            changed = true;
        }
    if (changed) {
        g_hoverIdx = -1;
        Refresh();
    }
}

static int HitTest(POINT pt) {
    for (size_t i = 0; i < g_staged.size(); i++) {
        RECT r = g_staged[i].card;
        InflateRect(&r, Scale(8), Scale(8));
        if (PtInRect(&r, pt)) return (int)i;
    }
    return -1;
}

static LRESULT CALLBACK StripProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
        case WM_TIMER:
            if (w == TIMER_CHECK) OnCheckTimer();
            else if (w == TIMER_POLL) Poll();
            else if (w == TIMER_ANIM) AnimTick();
            else if (w == TIMER_SNAP) SnapTick();
            return 0;
        case WM_APP_FG:
            if (g_started) OnForeground((HWND)w);
            return 0;
        case WM_APP_DESTROYED:
            OnGone((HWND)w, false);
            return 0;
        case WM_APP_HIDDEN:
            OnGone((HWND)w, true);
            return 0;
        case WM_APP_RELOAD:
            LoadSettings();
            if (g_started) {
                UpdateHotkey();
                UpdateTray();
                g_edge = ComputeEdge();
                for (auto& s : g_staged) {
                    LoadIconFor(s);
                    s.layerValid = false;
                }
                Trim();
                Refresh();
            }
            return 0;
        case WM_APP_EXIT:
            Shutdown();
            DestroyWindow(g_trayWnd);
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            if (g_started) Refresh();
            return 0;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_MOUSEMOVE: {
            if (!g_trackingMouse) {
                TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
                TrackMouseEvent(&t);
                g_trackingMouse = true;
            }
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            int idx = HitTest(pt);
            if (idx != g_hoverIdx) {
                g_hoverIdx = idx;
                StartAnim();
            }
            return 0;
        }
        case WM_MOUSELEAVE:
            g_trackingMouse = false;
            g_hoverIdx = -1;
            StartAnim();
            return 0;
        case WM_LBUTTONUP: {
            POINT pt{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            int idx = HitTest(pt);
            if (idx >= 0) Promote(idx, true);
            return 0;
        }
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK TrayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (g_msgTaskbarCreated && m == g_msgTaskbarCreated) {
        g_trayAdded = false;
        if (g_started) AddTray();
        return 0;
    }
    switch (m) {
        case WM_TRAYICON:
            switch (LOWORD(l)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    SetEnabled(!g_enabled);
                    break;
                case WM_CONTEXTMENU:
                    ShowTrayMenu(GET_X_LPARAM(w), GET_Y_LPARAM(w));
                    break;
            }
            return 0;
        case WM_HOTKEY:
            if (w == HOTKEY_ID) SetEnabled(!g_enabled);
            return 0;
        case WM_COMMAND:
            if (LOWORD(w) == ID_TOGGLE) SetEnabled(!g_enabled);
            return 0;
        // Schnittstelle für andere Mods (z. B. Menüleiste):
        // wParam 0 = Status abfragen, 1 = an, 2 = aus, 3 = umschalten
        // Rückgabe: 1 = an, 0 = aus
        case WM_APP_SM_CONTROL:
            if (w == 1) SetEnabled(true);
            else if (w == 2) SetEnabled(false);
            else if (w == 3) SetEnabled(!g_enabled);
            return g_enabled ? 1 : 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static DWORD WINAPI ThreadProc(void*) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    HINSTANCE inst = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&StripProc, &inst);

    WNDCLASSW wc{};
    wc.lpfnWndProc = StripProc;
    wc.hInstance = inst;
    wc.lpszClassName = kStripClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    RegisterClassW(&wc);

    WNDCLASSW tc{};
    tc.lpfnWndProc = TrayProc;
    tc.hInstance = inst;
    tc.lpszClassName = kTrayClass;
    RegisterClassW(&tc);

    g_strip = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kStripClass, L"Stage Manager", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
        inst, nullptr);
    g_trayWnd = CreateWindowExW(WS_EX_TOOLWINDOW, kTrayClass, L"Stage Manager Tray",
                                WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    g_memDC = CreateCompatibleDC(nullptr);
    SetEvent(g_readyEvent);
    if (!g_strip || !g_trayWnd) return 1;

    SetTimer(g_strip, TIMER_CHECK, 2000, nullptr);
    PostMessageW(g_strip, WM_TIMER, TIMER_CHECK, 0);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    g_strip = g_trayWnd = nullptr;
    if (g_dib) {
        SelectObject(g_memDC, g_oldBmp);
        DeleteObject(g_dib);
        g_dib = nullptr;
    }
    DeleteDC(g_memDC);
    UnregisterClassW(kStripClass, inst);
    UnregisterClassW(kTrayClass, inst);
    CoUninitialize();
    return 0;
}

// ================================================================ Windhawk

BOOL Wh_ModInit() {
    LoadSettings();
    g_readyEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, ThreadProc, nullptr, 0, nullptr);
    if (!g_thread) return FALSE;
    WaitForSingleObject(g_readyEvent, 5000);
    return TRUE;
}

void Wh_ModUninit() {
    if (g_strip) PostMessageW(g_strip, WM_APP_EXIT, 0, 0);
    if (g_thread) {
        WaitForSingleObject(g_thread, 10000);
        CloseHandle(g_thread);
    }
    if (g_readyEvent) CloseHandle(g_readyEvent);
}

void Wh_ModSettingsChanged() {
    if (g_strip) PostMessageW(g_strip, WM_APP_RELOAD, 0, 0);
}