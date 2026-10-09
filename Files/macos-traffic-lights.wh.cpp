// ==WindhawkMod==
// @id              macos-traffic-lights-right
// @name            macOS Traffic Lights
// @description     macOS-Ampelknöpfe links in der Titelleiste, native Knöpfe rechts werden verdeckt – auch im Explorer
// @version         2.0
// @author          Okan
// @include         *
// @exclude         dwm.exe
// @exclude         ShellExperienceHost.exe
// @exclude         StartMenuExperienceHost.exe
// @exclude         SearchHost.exe
// @exclude         SearchApp.exe
// @exclude         TextInputHost.exe
// @exclude         LockApp.exe
// @compilerOptions -ldwmapi -lcomctl32 -lgdi32 -ladvapi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# macOS Traffic Lights

- Rote/gelbe/grüne Knöpfe **links** in der Titelleiste (Schließen, Minimieren, Maximieren)
- Die nativen Knöpfe rechts werden mit der Titelleistenfarbe überdeckt
- Funktioniert auch bei Apps mit eigener Titelleiste (Explorer, Chrome, Edge, Store-Apps, Office …)
- Symbole erscheinen beim Hovern, inaktive Fenster sind grau (abschaltbar)
- Freie Fläche neben den Knöpfen: ziehen verschiebt das Fenster, Doppelklick maximiert

**Hinweis:** Die Knöpfe liegen über dem linken Rand der Titelleiste. Dort sitzende
Inhalte (App-Symbol, Anfang des Titels, erster Tab in Explorer/Chrome) werden
teilweise verdeckt. Sieht eine App schlecht aus: *Advanced → Custom process
exclusion list* und die .exe eintragen.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- size: 12
  $name: Knopfdurchmesser (px bei 100 %)
- spacing: 8
  $name: Abstand zwischen den Knöpfen (px)
- marginLeft: 12
  $name: Abstand zum linken Fensterrand (px)
- inactiveGray: true
  $name: Inaktive Fenster grau darstellen
- hideNative: true
  $name: Native Knöpfe rechts verdecken
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

enum { BTN_CLOSE = 0, BTN_MIN = 1, BTN_MAX = 2 };
enum { TIMER_HOVER = 1, TIMER_POLL = 2, TIMER_SAMPLE = 3 };
static const int kOrder[3] = {BTN_CLOSE, BTN_MIN, BTN_MAX};

struct Settings {
    int size = 12;
    int spacing = 8;
    int marginLeft = 12;
    bool inactiveGray = true;
    bool hideNative = true;
} g_settings;

struct Sig {
    RECT cover, btns;
    int hoverSlot, pressedSlot, size, spacing, marginLeft;
    LONG style;
    COLORREF bg;
    bool active, hover, gray, hideNative, zoomed;
};

struct WinState {
    HWND hwnd = nullptr;
    HWND cover = nullptr;  // right: hides native buttons, click-through
    HWND btns = nullptr;   // left: traffic lights, interactive
    RECT coverRect = {};
    RECT btnRect = {};
    RECT frame = {};
    float scale = 1;
    COLORREF bg = 0;
    bool bgValid = false;
    bool fastTimer = false;
    bool inSizeMove = false;
    bool shown = false;
    int pressedSlot = -1;
    Sig last = {};
};

static const wchar_t kOverlayClass[] = L"WhTrafficLightsOverlay";
static HINSTANCE g_hInst;
static UINT g_msgAttach, g_msgDetach;
static std::mutex g_mutex;
static std::unordered_map<HWND, WinState*> g_windows;
static std::atomic<bool> g_unloading{false};

// ---------------------------------------------------------------- helpers

struct DpiGuard {
    DPI_AWARENESS_CONTEXT old;
    DpiGuard() { old = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2); }
    ~DpiGuard() { if (old) SetThreadDpiAwarenessContext(old); }
};

static float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static bool IsWin11() {
    return *(volatile ULONG*)0x7FFE0260 >= 22000;  // KUSER_SHARED_DATA.NtBuildNumber
}

static COLORREF DefaultBg() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER,
                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v ? RGB(243, 243, 243) : RGB(32, 32, 32);
}

static bool IsCandidateStyle(HWND h) {
    if (!h || GetAncestor(h, GA_PARENT) != GetDesktopWindow()) return false;
    LONG st = GetWindowLongW(h, GWL_STYLE);
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    if (st & WS_CHILD) return false;
    if (ex & WS_EX_TOOLWINDOW) return false;
    if (!(st & (WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX))) return false;
    if ((st & WS_CAPTION) != WS_CAPTION && !(st & WS_THICKFRAME)) return false;
    return true;
}

static bool IsButtonEnabled(LONG style, int b) {
    if (b == BTN_MIN) return (style & WS_MINIMIZEBOX) != 0;
    if (b == BTN_MAX) return (style & WS_MAXIMIZEBOX) != 0;
    return true;
}

// Must be called with a per-monitor-v2 DPI context (physical pixels).
static bool ComputeGeometry(WinState* s) {
    HWND h = s->hwnd;
    if (!IsWindowVisible(h) || IsIconic(h)) return false;

    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
        return false;

    LONG st = GetWindowLongW(h, GWL_STYLE);
    LONG ex = GetWindowLongW(h, GWL_EXSTYLE);
    if (!(st & (WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX))) return false;

    int count = (st & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX)) ? 3 : 1;
    int cells = count + ((count == 1 && (ex & WS_EX_CONTEXTHELP)) ? 1 : 0);

    RECT wr;
    if (!GetWindowRect(h, &wr)) return false;
    RECT fr = wr;
    if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &fr, sizeof(fr)))) fr = wr;

    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    bool zoomed = IsZoomed(h);
    if (!zoomed && fr.left <= mi.rcMonitor.left && fr.top <= mi.rcMonitor.top &&
        fr.right >= mi.rcMonitor.right && fr.bottom >= mi.rcMonitor.bottom)
        return false;  // fullscreen (video, game)

    UINT dpi = GetDpiForWindow(h);
    if (!dpi) dpi = 96;
    float scale = dpi / 96.0f;
    if (fr.right - fr.left < 200 * scale || fr.bottom - fr.top < 60 * scale) return false;

    // --- right: native caption buttons
    RECT out = {};
    bool ok = false;
    RECT b;
    if (SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CAPTION_BUTTON_BOUNDS, &b, sizeof(b))) &&
        b.right > b.left && b.bottom > b.top) {
        out = {wr.left + b.left, wr.top + b.top, wr.left + b.right, wr.top + b.bottom};
        float cellW = (out.right - out.left) / (float)cells;
        int bh = out.bottom - out.top;
        ok = cellW >= 24 * scale && cellW <= 80 * scale && bh >= 16 * scale && bh <= 80 * scale &&
             out.left >= fr.left && out.right <= fr.right + 4;
    }

    int top = fr.top;
    if (top < mi.rcMonitor.top) top = mi.rcMonitor.top;
    if (zoomed && mi.rcWork.top > top) top = mi.rcWork.top;
    int bh = (int)((zoomed ? 30 : 32) * scale + 0.5f);
    int sysH = GetSystemMetricsForDpi(SM_CYCAPTION, dpi);
    if (!zoomed)
        sysH += GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    if (sysH > bh) bh = sysH;
    RECT heur = {fr.right - cells * (int)(46 * scale + 0.5f), top, fr.right, top + bh};
    if (ok) {
        if (heur.left < out.left) out.left = heur.left;
        if (heur.top < out.top) out.top = heur.top;
        if (heur.right > out.right) out.right = heur.right;
        if (heur.bottom > out.bottom) out.bottom = heur.bottom;
    } else {
        out = heur;
    }
    int captionH = out.bottom - out.top;
    int m = (int)(2 * scale + 0.5f);
    out.left -= m;
    out.bottom += m;
    if (out.right > fr.right) out.right = fr.right;
    if (out.top < top) out.top = top;

    // --- left: traffic lights
    float d = g_settings.size * scale;
    float gap = g_settings.spacing * scale;
    float ml = g_settings.marginLeft * scale;
    int lw = (int)ceilf(2 * ml + 3 * d + 2 * gap);
    int left = fr.left;
    if (left < mi.rcMonitor.left) left = mi.rcMonitor.left;
    RECT lr = {left, out.top, left + lw, out.top + captionH};
    if (lr.right >= out.left) return false;  // window too narrow

    s->coverRect = out;
    s->btnRect = lr;
    s->frame = fr;
    s->scale = scale;
    return true;
}

static void SampleBackground(WinState* s) {
    DpiGuard g;
    if (!ComputeGeometry(s)) return;
    POINT pt = {s->coverRect.left - 3, (s->coverRect.top + s->btnRect.bottom) / 2};
    HWND under = WindowFromPoint(pt);
    if (!under || GetAncestor(under, GA_ROOT) != s->hwnd) return;  // covered by another window
    HDC dc = GetDC(nullptr);
    COLORREF c = GetPixel(dc, pt.x, pt.y);
    ReleaseDC(nullptr, dc);
    if (c != CLR_INVALID) {
        s->bg = c;
        s->bgValid = true;
    }
}

static int HitSlot(WinState* s, int x, int y) {
    float d = g_settings.size * s->scale;
    float gap = g_settings.spacing * s->scale;
    float ml = g_settings.marginLeft * s->scale;
    int h = s->btnRect.bottom - s->btnRect.top;
    float cy = h / 2.0f;
    if (y < cy - d / 2 - gap / 2 || y > cy + d / 2 + gap / 2) return -1;
    for (int i = 0; i < 3; i++) {
        float l = ml + i * (d + gap) - gap / 2;
        if (x >= l && x < l + d + gap) return i;
    }
    return -1;
}

// ---------------------------------------------------------------- drawing

struct Canvas {
    int w, h;
    DWORD* px;
};

static void BlendPx(Canvas& c, int x, int y, COLORREF col, float a) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || a <= 0) return;
    a = Clamp01(a);
    DWORD& p = c.px[y * c.w + x];
    float inv = 1 - a;
    int da = (p >> 24) & 0xFF, dr = (p >> 16) & 0xFF, dg = (p >> 8) & 0xFF, db = p & 0xFF;
    int na = (int)(a * 255 + da * inv + 0.5f);
    int nr = (int)(GetRValue(col) * a + dr * inv + 0.5f);
    int ng = (int)(GetGValue(col) * a + dg * inv + 0.5f);
    int nb = (int)(GetBValue(col) * a + db * inv + 0.5f);
    p = ((DWORD)na << 24) | ((DWORD)nr << 16) | ((DWORD)ng << 8) | (DWORD)nb;
}

static void FillCircle(Canvas& c, float cx, float cy, float r, COLORREF col, float alpha) {
    int x0 = (int)floorf(cx - r - 1), x1 = (int)ceilf(cx + r + 1);
    int y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            BlendPx(c, x, y, col, Clamp01(r - sqrtf(dx * dx + dy * dy) + 0.5f) * alpha);
        }
}

static void DrawLine(Canvas& c, float ax, float ay, float bx, float by, float hw, COLORREF col,
                     float alpha) {
    int x0 = (int)floorf(fminf(ax, bx) - hw - 1), x1 = (int)ceilf(fmaxf(ax, bx) + hw + 1);
    int y0 = (int)floorf(fminf(ay, by) - hw - 1), y1 = (int)ceilf(fmaxf(ay, by) + hw + 1);
    float vx = bx - ax, vy = by - ay, len2 = vx * vx + vy * vy;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f - ax, py = y + 0.5f - ay;
            float t = len2 > 0 ? Clamp01((px * vx + py * vy) / len2) : 0;
            float dx = px - t * vx, dy = py - t * vy;
            BlendPx(c, x, y, col, Clamp01(hw - sqrtf(dx * dx + dy * dy) + 0.5f) * alpha);
        }
}

static void FillBg(Canvas& c, COLORREF bg) {
    DWORD px = 0xFF000000u | ((DWORD)GetRValue(bg) << 16) | ((DWORD)GetGValue(bg) << 8) |
               GetBValue(bg);
    for (int i = 0; i < c.w * c.h; i++) c.px[i] = px;
}

// Makes pixels outside a rounded corner (center ccx/ccy, radius R) transparent.
static void ClipCorner(Canvas& c, float ccx, float ccy, float R, bool rightSide) {
    for (int y = 0; y < c.h && y <= (int)ceilf(ccy); y++)
        for (int x = 0; x < c.w; x++) {
            float dx = x + 0.5f - ccx, dy = y + 0.5f - ccy;
            if (dy >= 0 || (rightSide ? dx <= 0 : dx >= 0)) continue;
            float cov = Clamp01(R - sqrtf(dx * dx + dy * dy) + 0.5f);
            if (cov >= 1) continue;
            DWORD& p = c.px[y * c.w + x];
            int a = (int)(((p >> 24) & 0xFF) * cov), r = (int)(((p >> 16) & 0xFF) * cov);
            int g = (int)(((p >> 8) & 0xFF) * cov), b = (int)((p & 0xFF) * cov);
            p = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
        }
}

static COLORREF Darken(COLORREF c, float f) {
    return RGB((int)(GetRValue(c) * f), (int)(GetGValue(c) * f), (int)(GetBValue(c) * f));
}

static int Luma(COLORREF c) {
    return (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
}

static void DrawCover(Canvas& c, WinState* s, const Sig& sg) {
    FillBg(c, sg.bg);
    if (IsWin11() && !sg.zoomed && sg.cover.right >= s->frame.right - 1 &&
        sg.cover.top <= s->frame.top + 1) {
        float R = 8 * s->scale;
        ClipCorner(c, (s->frame.right - sg.cover.left) - R, (s->frame.top - sg.cover.top) + R, R,
                   true);
    }
}

static void DrawButtons(Canvas& c, WinState* s, const Sig& sg) {
    static const COLORREF fill[3]   = {RGB(255, 95, 87), RGB(254, 188, 46), RGB(40, 200, 64)};
    static const COLORREF border[3] = {RGB(224, 68, 62), RGB(222, 161, 35), RGB(30, 170, 50)};
    static const COLORREF glyph[3]  = {RGB(77, 0, 0), RGB(153, 87, 0), RGB(0, 100, 0)};

    FillBg(c, sg.bg);
    if (IsWin11() && !sg.zoomed && sg.btns.left <= s->frame.left + 1 &&
        sg.btns.top <= s->frame.top + 1) {
        float R = 8 * s->scale;
        ClipCorner(c, (s->frame.left - sg.btns.left) + R, (s->frame.top - sg.btns.top) + R, R,
                   false);
    }

    float d = sg.size * s->scale;
    float gap = sg.spacing * s->scale;
    float ml = sg.marginLeft * s->scale;
    float r = d / 2;
    float cy = c.h / 2.0f;
    bool lightBg = Luma(sg.bg) > 128;

    for (int i = 0; i < 3; i++) {
        int b = kOrder[i];
        float cx = ml + i * (d + gap) + r;
        bool enabled = IsButtonEnabled(sg.style, b);
        if (!enabled || sg.gray) {
            FillCircle(c, cx, cy, r, lightBg ? RGB(205, 205, 205) : RGB(85, 85, 85), 1);
            continue;
        }
        COLORREF f = fill[b];
        if (sg.pressedSlot == i && sg.hoverSlot == i) f = Darken(f, 0.8f);
        float bw = fmaxf(d / 12.0f, 0.75f);
        FillCircle(c, cx, cy, r, border[b], 1);
        FillCircle(c, cx, cy, r - bw, f, 1);

        if (!sg.hover) continue;
        float hw = fmaxf(d * 0.05f, 0.55f);
        float k = r * 0.42f;
        COLORREF g = glyph[b];
        if (b == BTN_CLOSE) {
            DrawLine(c, cx - k, cy - k, cx + k, cy + k, hw, g, 0.9f);
            DrawLine(c, cx - k, cy + k, cx + k, cy - k, hw, g, 0.9f);
        } else if (b == BTN_MIN) {
            DrawLine(c, cx - k * 1.2f, cy, cx + k * 1.2f, cy, hw, g, 0.9f);
        } else {
            DrawLine(c, cx - k * 1.2f, cy, cx + k * 1.2f, cy, hw, g, 0.9f);
            DrawLine(c, cx, cy - k * 1.2f, cx, cy + k * 1.2f, hw, g, 0.9f);
        }
    }
}

template <typename F>
static void BlitLayered(HWND wnd, const RECT& r, F draw) {
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        Canvas c = {w, h, (DWORD*)bits};
        draw(c);
        POINT dst = {r.left, r.top}, src = {0, 0};
        SIZE sz = {w, h};
        BLENDFUNCTION bf = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(wnd, screen, &dst, &sz, mem, &src, 0, &bf, ULW_ALPHA);
        SelectObject(mem, old);
        DeleteObject(bmp);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    if (!IsWindowVisible(wnd)) ShowWindow(wnd, SW_SHOWNOACTIVATE);
}

static void HideAll(WinState* s) {
    if (s->cover && IsWindowVisible(s->cover)) ShowWindow(s->cover, SW_HIDE);
    if (s->btns && IsWindowVisible(s->btns)) ShowWindow(s->btns, SW_HIDE);
    s->shown = false;
}

static void Render(WinState* s, bool force = false) {
    if (!s->cover || !s->btns) return;
    DpiGuard g;

    if (!ComputeGeometry(s)) {
        HideAll(s);
        if (s->fastTimer) {
            KillTimer(s->cover, TIMER_HOVER);
            s->fastTimer = false;
        }
        return;
    }
    if (!s->bgValid) s->bg = DefaultBg();

    POINT pt;
    GetCursorPos(&pt);
    bool hover = PtInRect(&s->btnRect, pt) != FALSE;

    Sig sg;
    memset(&sg, 0, sizeof(sg));
    sg.cover = s->coverRect;
    sg.btns = s->btnRect;
    sg.hover = hover;
    sg.hoverSlot = hover ? HitSlot(s, pt.x - s->btnRect.left, pt.y - s->btnRect.top) : -1;
    sg.pressedSlot = s->pressedSlot;
    sg.active = GetForegroundWindow() == s->hwnd;
    sg.gray = g_settings.inactiveGray && !sg.active && !hover;
    sg.style = GetWindowLongW(s->hwnd, GWL_STYLE) & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    sg.bg = s->bg;
    sg.size = g_settings.size;
    sg.spacing = g_settings.spacing;
    sg.marginLeft = g_settings.marginLeft;
    sg.hideNative = g_settings.hideNative;
    sg.zoomed = IsZoomed(s->hwnd) != FALSE;

    if (hover && !s->fastTimer) {
        SetTimer(s->cover, TIMER_HOVER, 30, nullptr);
        s->fastTimer = true;
    } else if (!hover && s->fastTimer && s->pressedSlot < 0) {
        KillTimer(s->cover, TIMER_HOVER);
        s->fastTimer = false;
    }

    if (!force && s->shown && memcmp(&sg, &s->last, sizeof(sg)) == 0) return;
    s->last = sg;

    if (sg.hideNative)
        BlitLayered(s->cover, sg.cover, [&](Canvas& c) { DrawCover(c, s, sg); });
    else if (IsWindowVisible(s->cover))
        ShowWindow(s->cover, SW_HIDE);
    BlitLayered(s->btns, sg.btns, [&](Canvas& c) { DrawButtons(c, s, sg); });
    s->shown = true;
}

static void ScheduleSample(WinState* s, UINT ms) {
    if (s->cover) SetTimer(s->cover, TIMER_SAMPLE, ms, nullptr);
}

static void DoAction(WinState* s, int slot) {
    int b = kOrder[slot];
    if (!IsButtonEnabled(GetWindowLongW(s->hwnd, GWL_STYLE), b)) return;
    WPARAM cmd = b == BTN_CLOSE ? SC_CLOSE
               : b == BTN_MIN   ? SC_MINIMIZE
               : (IsZoomed(s->hwnd) ? SC_RESTORE : SC_MAXIMIZE);
    PostMessageW(s->hwnd, WM_SYSCOMMAND, cmd, 0);
}

// ---------------------------------------------------------------- overlay windows

static LRESULT CALLBACK OverlayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    WinState* s = (WinState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    bool isBtns = s && h == s->btns;
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);
            break;
        case WM_NCHITTEST:
            return isBtns ? HTCLIENT : HTTRANSPARENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_MOUSEMOVE:
            if (isBtns) Render(s);
            return 0;
        case WM_LBUTTONDOWN:
            if (isBtns) {
                int slot = HitSlot(s, GET_X_LPARAM(l), GET_Y_LPARAM(l));
                if (slot >= 0) {
                    s->pressedSlot = slot;
                    SetCapture(h);
                    Render(s);
                } else {
                    // Empty area: drag the window like a title bar.
                    SetForegroundWindow(s->hwnd);
                    PostMessageW(s->hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
                }
            }
            return 0;
        case WM_LBUTTONUP:
            if (isBtns && s->pressedSlot >= 0) {
                int slot = HitSlot(s, GET_X_LPARAM(l), GET_Y_LPARAM(l));
                int p = s->pressedSlot;
                s->pressedSlot = -1;
                if (GetCapture() == h) ReleaseCapture();
                if (slot == p) DoAction(s, slot);
                Render(s);
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            if (isBtns && HitSlot(s, GET_X_LPARAM(l), GET_Y_LPARAM(l)) < 0 &&
                (GetWindowLongW(s->hwnd, GWL_STYLE) & WS_MAXIMIZEBOX))
                PostMessageW(s->hwnd, WM_SYSCOMMAND, IsZoomed(s->hwnd) ? SC_RESTORE : SC_MAXIMIZE,
                             0);
            return 0;
        case WM_CAPTURECHANGED:
            if (isBtns && s->pressedSlot >= 0) {
                s->pressedSlot = -1;
                Render(s);
            }
            return 0;
        case WM_TIMER:
            if (!s) return 0;
            if (w == TIMER_SAMPLE) {
                KillTimer(h, TIMER_SAMPLE);
                SampleBackground(s);
                Render(s, true);
            } else {
                Render(s);
            }
            return 0;
        case WM_DESTROY:
            if (s) {
                if (s->cover == h) {
                    s->cover = nullptr;
                    s->fastTimer = false;
                }
                if (s->btns == h) s->btns = nullptr;
                s->shown = false;
            }
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            break;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------------------------------------------------------- attach / detach

static void Detach(WinState* s) {
    if (s->btns) DestroyWindow(s->btns);
    if (s->cover) DestroyWindow(s->cover);
    s->btns = s->cover = nullptr;
}

static void Attach(WinState* s) {
    if (s->cover || g_unloading || !IsWindowVisible(s->hwnd)) return;
    {
        DpiGuard g;  // overlays must be per-monitor aware to use physical pixels
        s->cover = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, s->hwnd, nullptr, g_hInst, s);
        s->btns = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                  kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, s->hwnd, nullptr,
                                  g_hInst, s);
    }
    if (!s->cover || !s->btns) {
        Wh_Log(L"Overlay konnte nicht erstellt werden: %u", GetLastError());
        Detach(s);
        return;
    }
    SetTimer(s->cover, TIMER_POLL, 1000, nullptr);
    SampleBackground(s);
    Render(s, true);
}

static void OnCursorActivity(WinState* s) {
    if (!s->btns || s->fastTimer) return;
    POINT pt;
    {
        DpiGuard g;
        GetCursorPos(&pt);
    }
    if (PtInRect(&s->btnRect, pt)) Render(s);
}

static LRESULT CALLBACK WindowSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l, DWORD_PTR ref) {
    WinState* s = (WinState*)ref;

    if (m == g_msgAttach) {
        if (s->cover) {
            SampleBackground(s);
            Render(s, true);
        } else {
            Attach(s);
        }
        return 0;
    }
    if (m == g_msgDetach) {
        Detach(s);
        return 0;
    }

    switch (m) {
        case WM_WINDOWPOSCHANGED: {
            LRESULT r = DefSubclassProc(h, m, w, l);
            auto* wp = (WINDOWPOS*)l;
            if (!s->cover) {
                Attach(s);
            } else {
                Render(s);
                if ((!(wp->flags & SWP_NOSIZE) && !s->inSizeMove) || (wp->flags & SWP_SHOWWINDOW))
                    ScheduleSample(s, 300);
            }
            return r;
        }
        case WM_ENTERSIZEMOVE:
            s->inSizeMove = true;
            break;
        case WM_EXITSIZEMOVE:
            s->inSizeMove = false;
            ScheduleSample(s, 50);
            break;
        case WM_SHOWWINDOW:
            if (!w) HideAll(s);
            break;
        case WM_NCACTIVATE:
        case WM_ACTIVATEAPP: {
            LRESULT r = DefSubclassProc(h, m, w, l);
            if (s->cover) {
                Render(s);
                ScheduleSample(s, 300);  // active/inactive title colors differ
            }
            return r;
        }
        case WM_THEMECHANGED:
        case WM_SETTINGCHANGE:
        case WM_DWMCOLORIZATIONCOLORCHANGED:
        case WM_DPICHANGED:
            ScheduleSample(s, 300);
            break;
        case WM_NCHITTEST:
        case WM_SETCURSOR:
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            OnCursorActivity(s);
            break;
        case WM_NCDESTROY: {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_windows.erase(h);
            }
            if (s->btns && IsWindow(s->btns)) DestroyWindow(s->btns);
            if (s->cover && IsWindow(s->cover)) DestroyWindow(s->cover);
            LRESULT r = DefSubclassProc(h, m, w, l);
            delete s;
            return r;
        }
    }
    return DefSubclassProc(h, m, w, l);
}

static void OnWindowCreated(HWND h) {
    if (g_unloading || !IsCandidateStyle(h)) return;

    auto* s = new WinState();
    s->hwnd = h;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_windows.count(h)) {
            delete s;
            return;
        }
        g_windows[h] = s;
    }
    if (!WindhawkUtils::SetWindowSubclassFromAnyThread(h, WindowSubclassProc, (DWORD_PTR)s)) {
        Wh_Log(L"Subclass fehlgeschlagen: %p", h);
        std::lock_guard<std::mutex> lock(g_mutex);
        g_windows.erase(h);
        delete s;
        return;
    }
    if (IsWindowVisible(h)) PostMessageW(h, g_msgAttach, 0, 0);
}

// ---------------------------------------------------------------- hooks

using CreateWindowExW_t = decltype(&CreateWindowExW);
static CreateWindowExW_t CreateWindowExW_Orig;
static HWND WINAPI CreateWindowExW_Hook(DWORD ex, LPCWSTR cls, LPCWSTR name, DWORD style, int x,
                                        int y, int w, int h, HWND parent, HMENU menu,
                                        HINSTANCE inst, LPVOID param) {
    HWND hwnd = CreateWindowExW_Orig(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    OnWindowCreated(hwnd);
    return hwnd;
}

using CreateWindowExA_t = decltype(&CreateWindowExA);
static CreateWindowExA_t CreateWindowExA_Orig;
static HWND WINAPI CreateWindowExA_Hook(DWORD ex, LPCSTR cls, LPCSTR name, DWORD style, int x,
                                        int y, int w, int h, HWND parent, HMENU menu,
                                        HINSTANCE inst, LPVOID param) {
    HWND hwnd = CreateWindowExA_Orig(ex, cls, name, style, x, y, w, h, parent, menu, inst, param);
    OnWindowCreated(hwnd);
    return hwnd;
}

// ---------------------------------------------------------------- mod lifecycle

static void LoadSettings() {
    g_settings.size = ClampInt(Wh_GetIntSetting(L"size"), 6, 30);
    g_settings.spacing = ClampInt(Wh_GetIntSetting(L"spacing"), 0, 30);
    g_settings.marginLeft = ClampInt(Wh_GetIntSetting(L"marginLeft"), 0, 100);
    g_settings.inactiveGray = Wh_GetIntSetting(L"inactiveGray") != 0;
    g_settings.hideNative = Wh_GetIntSetting(L"hideNative") != 0;
}

static std::vector<HWND> SnapshotWindows() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::vector<HWND> list;
    for (auto& kv : g_windows) list.push_back(kv.first);
    return list;
}

static BOOL CALLBACK EnumWindowsProc(HWND h, LPARAM) {
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId()) OnWindowCreated(h);
    return TRUE;
}

BOOL Wh_ModInit() {
    LoadSettings();

    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)(void*)&Wh_ModInit, (HMODULE*)&g_hInst);

    g_msgAttach = RegisterWindowMessageW(L"WhTrafficLights_Attach");
    g_msgDetach = RegisterWindowMessageW(L"WhTrafficLights_Detach");

    WNDCLASSEXW wc = {sizeof(wc)};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kOverlayClass;
    RegisterClassExW(&wc);

    Wh_SetFunctionHook((void*)CreateWindowExW, (void*)CreateWindowExW_Hook,
                       (void**)&CreateWindowExW_Orig);
    Wh_SetFunctionHook((void*)CreateWindowExA, (void*)CreateWindowExA_Hook,
                       (void**)&CreateWindowExA_Orig);
    return TRUE;
}

void Wh_ModAfterInit() {
    EnumWindows(EnumWindowsProc, 0);
}

void Wh_ModSettingsChanged() {
    LoadSettings();
    for (HWND h : SnapshotWindows()) PostMessageW(h, g_msgAttach, 0, 0);
}

void Wh_ModBeforeUninit() {
    g_unloading = true;
    for (HWND h : SnapshotWindows()) {
        DWORD_PTR res;
        SendMessageTimeoutW(h, g_msgDetach, 0, 0, SMTO_ABORTIFHUNG, 2000, &res);
        WindhawkUtils::RemoveWindowSubclassFromAnyThread(h, WindowSubclassProc);
        WinState* s = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            auto it = g_windows.find(h);
            if (it != g_windows.end()) {
                s = it->second;
                g_windows.erase(it);
            }
        }
        delete s;
    }
}

void Wh_ModUninit() {
    UnregisterClassW(kOverlayClass, g_hInst);
}