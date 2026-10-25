// ==WindhawkMod==
// @id              macos-traffic-lights-right
// @name            macOS Traffic Lights
// @description     macOS traffic light buttons on the right of the title bar, replacing the native caption buttons – Explorer included
// @version         3.2
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

- Green/yellow/red buttons (maximize, minimize, close) anchored to the **right** of the title bar
- The native caption buttons are removed (the window loses its system menu style, which is what makes
  Windows draw them; Alt+Space still opens the window menu). Apps that draw their own buttons have
  them blocked and their symbols painted over with the title bar's own colors
- Also works with apps that draw their own title bar (Explorer, Chrome, Edge, Store apps, Office …)
- Symbols appear on hover; inactive windows are shown gray (can be turned off)
- Empty space next to the buttons behaves like the title bar: drag moves the window, double-click maximizes

**Note:** If an app looks wrong, add its .exe under *Advanced → Custom process exclusion list*.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- size: 12
  $name: Button diameter (px at 100 %)
- spacing: 8
  $name: Spacing between buttons (px)
- marginRight: 12
  $name: Distance from the right window edge (px)
- inactiveGray: true
  $name: Show inactive windows gray
- hideNative: true
  $name: Hide the native caption buttons
  $description: When off, the traffic lights sit just left of the native buttons
- clearApps: ""
  $name: Apps with see-through title bars
  $description: >-
    Comma-separated .exe names (e.g. explorer.exe) whose title bar a theme has made
    see-through and whose native button symbols it already hides. Nothing is painted over
    the native buttons there, only the lights; the buttons stay disabled.
*/
// ==/WindhawkModSettings==

#include <windhawk_utils.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

enum { BTN_CLOSE = 0, BTN_MIN = 1, BTN_MAX = 2 };
enum { TIMER_HOVER = 1, TIMER_POLL = 2, TIMER_SAMPLE = 3 };
// Left to right: green (maximize), yellow (minimize), red (close).
static const int kOrder[3] = {BTN_MAX, BTN_MIN, BTN_CLOSE};

struct Settings {
    int size = 12;
    int spacing = 8;
    int marginRight = 12;
    bool inactiveGray = true;
    bool hideNative = true;
    std::vector<std::wstring> clearApps;  // lower case
} g_settings;

struct Sig {
    RECT ov;
    float lightsX, lightsCy;
    int hoverSlot, pressedSlot, size, spacing, marginRight;
    LONG style;
    unsigned bgVersion;
    bool active, hover, gray, hideNative, zoomed;
};

struct WinState {
    HWND hwnd = nullptr;
    HWND overlay = nullptr;  // traffic lights (+ native button cover), interactive
    RECT ovRect = {};        // overlay, screen coords
    RECT lightsRect = {};    // the three lights, screen coords
    RECT frame = {};
    float scale = 1;
    float lightsX = 0;   // left edge of the first light, relative to the overlay
    float lightsCy = 0;  // vertical center of the lights, relative to the overlay
    // What the title bar looks like under the overlay, BGRA, anchored to the overlay's right
    // edge. Alpha 255 marks the patches over the native button glyphs; elsewhere (alpha 0) the
    // overlay stays clear and the live title bar shows through. One pixel wide and fully
    // opaque (a color per row) when only the strip beside the overlay could be read.
    std::vector<DWORD> bgPx;
    int bgW = 0, bgH = 0;
    unsigned bgVersion = 0;
    int pollCount = 0;
    int settle = 0;           // quick follow-up samples left until the title bar holds still
    bool probePending = false;
    ULONGLONG lastSample = 0;  // GetTickCount64 of the last screen copy
    ULONGLONG sampleCost = 0;  // how long it took, in ms
    // Where the window itself reports its caption buttons (see ProbeNativeButtons),
    // in physical pixels: distance from the right window edge and from the frame
    // top. 0 when the window reports none.
    int nativeWidth = 0;
    int nativeHeight = 0;
    int nativeLeft = 0;   // screen x where the native buttons start, from ComputeGeometry
    int nativeCells = 3;  // how many native buttons there are
    bool clear = false;   // see-through title bar: paint nothing over the native buttons
    // WS_SYSMENU taken off the window, so Windows doesn't draw the native buttons at all; it's
    // put back on detach. noButtons: the probe found no buttons left after that, so there's
    // nothing to paint over either.
    bool sysMenuRemoved = false;
    bool noButtons = false;
    // Windows drew the caption buttons when the style came off (DWM reported them). Apps
    // that draw their own buttons without telling Windows (Discord) keep them either way,
    // so they're painted over instead.
    bool systemButtons = false;
    // For such apps, the height of their title bar, worked out from where their button symbols
    // sit (buttons are centred on them), so the cover reaches the buttons' bottom edge and
    // none of them can be clicked below it. 0 until measured.
    int appCaptionH = 0;
    // Child windows that answer for the caption buttons (apps that draw their own title bar),
    // subclassed so their buttons stop being buttons too.
    std::vector<HWND> buttonChildren;
    bool probing = false;
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

// The window's style with WS_SYSMENU as the app set it, whether or not it's been taken off.
static LONG AppStyle(WinState* s) {
    LONG st = GetWindowLongW(s->hwnd, GWL_STYLE);
    return s->sysMenuRemoved ? (st | WS_SYSMENU) : st;
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

    LONG st = AppStyle(s);
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
    // Fullscreen (video, game, a browser's F11) drops the resizable frame; a window that's
    // merely as large as the screen (Discord restores to that) keeps it.
    if (!zoomed && !(st & WS_THICKFRAME) && fr.left <= mi.rcMonitor.left &&
        fr.top <= mi.rcMonitor.top && fr.right >= mi.rcMonitor.right &&
        fr.bottom >= mi.rcMonitor.bottom)
        return false;

    UINT dpi = GetDpiForWindow(h);
    if (!dpi) dpi = 96;
    float scale = dpi / 96.0f;
    if (fr.right - fr.left < 200 * scale || fr.bottom - fr.top < 60 * scale) return false;

    // --- native caption buttons
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
    // Buttons wider or taller than the estimate above would stick out of the
    // cover and stay clickable, so the area the window reports is always covered.
    if (s->nativeWidth > 0) {
        int frameTop = zoomed ? top : fr.top;
        out.left = std::min<LONG>(out.left, wr.right - s->nativeWidth);
        out.bottom = std::max<LONG>(out.bottom, frameTop + s->nativeHeight);
    } else if (s->appCaptionH > 0) {
        out.bottom = std::max<LONG>(out.bottom, top + s->appCaptionH);
    }
    s->nativeLeft = out.left;
    s->nativeCells = cells;
    int captionH = out.bottom - out.top;
    // Generous, as some apps draw a button a few pixels past where it reports it. The cover
    // shows the real title bar, so the extra width doesn't show.
    int m = (int)(8 * scale + 0.5f);
    out.left -= m;
    out.bottom += (int)(2 * scale + 0.5f);  // below is the app's content, often clickable
    if (out.right > fr.right) out.right = fr.right;
    if (out.top < top) out.top = top;

    // --- traffic lights, anchored to the right
    float d = g_settings.size * scale;
    float gap = g_settings.spacing * scale;
    float mr = g_settings.marginRight * scale;
    int lw = (int)ceilf(2 * mr + 3 * d + 2 * gap);
    RECT ov;
    if (g_settings.hideNative) {
        // One opaque, clickable surface over the whole native button area, so the hidden
        // buttons can't be hovered or clicked through it.
        ov = out;
        if (ov.right - ov.left < lw) ov.left = ov.right - lw;
    } else {
        ov = {out.left - lw, out.top, out.left, out.top + captionH};
    }
    int left = fr.left;
    if (left < mi.rcMonitor.left) left = mi.rcMonitor.left;
    if (ov.left <= left + (int)(50 * scale)) return false;  // window too narrow

    s->ovRect = ov;
    s->lightsX = (ov.right - ov.left) - lw + mr;
    s->lightsCy = captionH / 2.0f;
    float pad = gap / 2;
    s->lightsRect = {ov.left + (int)floorf(s->lightsX - pad),
                     ov.top + (int)floorf(s->lightsCy - d / 2 - pad),
                     ov.left + (int)ceilf(s->lightsX + 3 * d + 2 * gap + pad),
                     ov.top + (int)ceilf(s->lightsCy + d / 2 + pad)};
    s->frame = fr;
    s->scale = scale;
    return true;
}

static bool IsCaptionButtonHit(LRESULT ht) {
    return ht == HTMINBUTTON || ht == HTMAXBUTTON || ht == HTCLOSE || ht == HTHELP;
}

static LRESULT CALLBACK ButtonChildSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id,
                                                DWORD_PTR ref);

// What the window answers for a screen point. Apps that draw their own title bar
// still report their buttons here (Windows needs it for snap layouts), some from
// a child window covering the title bar; such a child is subclassed (see
// ButtonChildSubclassProc).
static bool HitsCaptionButton(WinState* s, int x, int y) {
    HWND h = s->hwnd;
    LPARAM lp = MAKELPARAM((WORD)x, (WORD)y);
    if (IsCaptionButtonHit(SendMessageW(h, WM_NCHITTEST, 0, lp))) return true;
    HWND target = h;
    for (int depth = 0; depth < 8; depth++) {
        POINT c = {x, y};
        ScreenToClient(target, &c);
        HWND child = ChildWindowFromPointEx(target, c, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT);
        if (!child || child == target) break;
        target = child;
    }
    // Only children on this thread: a busy thread elsewhere would stall the probe.
    if (target == h || GetWindowThreadProcessId(target, nullptr) != GetCurrentThreadId() ||
        !IsCaptionButtonHit(SendMessageW(target, WM_NCHITTEST, 0, lp)))
        return false;
    if (std::find(s->buttonChildren.begin(), s->buttonChildren.end(), target) ==
            s->buttonChildren.end() &&
        SetWindowSubclass(target, ButtonChildSubclassProc, 1, (DWORD_PTR)s))
        s->buttonChildren.push_back(target);
    return true;
}

// Finds where the window's own caption buttons are by asking it, along a few
// rows of the title bar from the right edge inwards. Runs in the window's own
// DPI context, as an unaware app answers in its own coordinates, and the result
// is scaled to physical pixels afterwards.
static void ProbeNativeButtons(WinState* s) {
    HWND h = s->hwnd;
    if (!IsWindowVisible(h) || IsIconic(h)) return;
    s->probing = true;
    DPI_AWARENESS_CONTEXT old = SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(h));

    int width = 0, height = 0;
    RECT wr;
    if (GetWindowRect(h, &wr) && wr.right > wr.left) {
        UINT dpi = GetDpiForWindow(h);
        if (!dpi) dpi = 96;
        float scale = dpi / 96.0f;
        // Windows have no invisible border at the top, except when maximized.
        int frameTop = wr.top;
        if (IsZoomed(h))
            frameTop += GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
                        GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        int step = std::max(2, (int)(3 * scale));
        int limit = (int)(400 * scale);
        int left = INT_MAX, hitX = 0;
        for (int row : {6, 14, 22}) {
            int y = frameTop + (int)(row * scale);
            int misses = 0;
            bool found = false;
            for (int x = wr.right - 1; x > wr.right - limit && x > wr.left; x -= step) {
                if (HitsCaptionButton(s, x, y)) {
                    found = true;
                    misses = 0;
                    if (x < left) {
                        left = x;
                        hitX = x + step;
                    }
                } else if (found && ++misses > 2) {
                    break;  // past the leftmost button
                }
            }
        }
        if (left != INT_MAX) {
            width = wr.right - left + step;
            int bottom = frameTop;
            for (int y = frameTop; y < frameTop + (int)(80 * scale); y += 2) {
                if (HitsCaptionButton(s, hitX, y)) bottom = y + 2;
            }
            height = bottom - frameTop;
        }
    }
    SetThreadDpiAwarenessContext(old);

    // To physical pixels, by the ratio of the window's two sizes.
    RECT pr;
    {
        DpiGuard g;
        if (width && GetWindowRect(h, &pr) && wr.right > wr.left) {
            float ratio = (float)(pr.right - pr.left) / (wr.right - wr.left);
            width = (int)ceilf(width * ratio);
            height = (int)ceilf(height * ratio);
        }
    }
    s->nativeWidth = width;
    s->nativeHeight = height;
    s->probing = false;
}

// Whether another window above this one overlaps r, so the screen there isn't the title bar.
static bool IsCovered(WinState* s, const RECT& r) {
    for (HWND w = GetWindow(s->hwnd, GW_HWNDPREV); w; w = GetWindow(w, GW_HWNDPREV)) {
        if (w == s->overlay || !IsWindowVisible(w)) continue;
        LONG ex = GetWindowLongW(w, GWL_EXSTYLE);
        if ((ex & (WS_EX_LAYERED | WS_EX_TRANSPARENT)) == (WS_EX_LAYERED | WS_EX_TRANSPARENT))
            continue;  // click-through overlays
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(w, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
            continue;
        RECT wr, x;
        if (FAILED(DwmGetWindowAttribute(w, DWMWA_EXTENDED_FRAME_BOUNDS, &wr, sizeof(wr))) &&
            !GetWindowRect(w, &wr))
            continue;
        if (IntersectRect(&x, &wr, &r)) return true;
    }
    return false;
}

// Copies a screen rectangle, top-down BGRA.
static bool CaptureScreen(const RECT& r, std::vector<DWORD>& out) {
    int w = r.right - r.left, h = r.bottom - r.top;
    if (w <= 0 || h <= 0) return false;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    bool ok = false;
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        if (BitBlt(mem, 0, 0, w, h, screen, r.left, r.top, SRCCOPY)) {
            GdiFlush();
            out.assign((const DWORD*)bits, (const DWORD*)bits + w * h);
            ok = true;
        }
        SelectObject(mem, old);
        DeleteObject(bmp);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return ok;
}

// One direction of RemoveGlyphs: a pixel that stands out from the median of the pixels around
// it along a row (or a column) takes that median, and so does the faint antialiased fringe next
// to such a pixel. Strokes much shorter than the window go, while the smooth gradients of mica
// and acrylic stay, as does the faint acrylic noise. Only rows y0..y1 and the columns left of
// keepRight are touched.
static void MedianPass(std::vector<DWORD>& px, int w, int h, bool vertical, int radius, int y0,
                       int y1, int keepRight) {
    std::vector<DWORD> med(px);
    std::vector<uint8_t> diff(w * h, 0);
    std::vector<int> ch[3];
    for (auto& v : ch) v.reserve(2 * radius + 1);
    for (int y = y0; y < y1; y++) {
        for (int x = 0; x < w - keepRight; x++) {
            int pos = vertical ? y : x;
            int lo = std::max(vertical ? y0 : 0, pos - radius);
            int hi = std::min(vertical ? y1 - 1 : w - 1, pos + radius);
            for (auto& v : ch) v.clear();
            for (int i = lo; i <= hi; i++) {
                DWORD p = vertical ? px[i * w + x] : px[y * w + i];
                for (int k = 0; k < 3; k++) ch[k].push_back((p >> (k * 8)) & 0xFF);
            }
            DWORD self = px[y * w + x], m = 0;
            int d = 0;
            for (int k = 0; k < 3; k++) {
                auto mid = ch[k].begin() + ch[k].size() / 2;
                std::nth_element(ch[k].begin(), mid, ch[k].end());
                m |= (DWORD)*mid << (k * 8);
                d = std::max(d, abs((int)((self >> (k * 8)) & 0xFF) - *mid));
            }
            med[y * w + x] = m;
            diff[y * w + x] = (uint8_t)std::min(d, 255);
        }
    }
    const int kStrong = 6, kFringe = 2;
    for (int y = y0; y < y1; y++)
        for (int x = 0; x < w - keepRight; x++) {
            int d = diff[y * w + x];
            bool replace = d > kStrong;
            for (int dy = -1; !replace && d > kFringe && dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy >= y0 && yy < y1 && xx >= 0 && xx < w && diff[yy * w + xx] > kStrong) {
                        replace = true;
                        break;
                    }
                }
            if (replace) px[y * w + x] = med[y * w + x];
        }
}

// Removes the native button glyphs from a copy of the title bar. Down the columns first, which
// takes out horizontal strokes (minimize's dash, the edges of maximize's square) whatever their
// length; that pass leaves the top and bottom rows alone, where the window's edge and a title
// bar's bottom separator run. Then along the rows for the vertical strokes left over. The
// rightmost columns hold the window border and are kept as they are.
static void RemoveGlyphs(std::vector<DWORD>& px, int w, int h, float scale) {
    int edge = (int)(4 * scale + 0.5f);
    int keepRight = (int)(2 * scale + 0.5f);
    if (h - 2 * edge > 4)
        MedianPass(px, w, h, true, (int)(8 * scale + 0.5f), edge, h - edge, keepRight);
    MedianPass(px, w, h, false, (int)(16 * scale + 0.5f), 0, h, keepRight);
}

// The title bar strip just left of the overlay, one color per row. Several columns are
// sampled and the per-channel median taken, so a stray glyph of the title text doesn't leak in.
static bool CaptureBesideOverlay(const RECT& ov, std::vector<DWORD>& out) {
    const int kCols = 5, kStep = 3;
    int w = (kCols - 1) * kStep + 1, h = ov.bottom - ov.top;
    RECT strip = {ov.left - 2 - (w - 1), ov.top, ov.left - 1, ov.bottom};
    std::vector<DWORD> px;
    if (!CaptureScreen(strip, px)) return false;
    out.resize(h);
    for (int y = 0; y < h; y++) {
        DWORD p = 0;
        for (int k = 0; k < 3; k++) {
            int v[kCols];
            for (int i = 0; i < kCols; i++) v[i] = (px[y * w + i * kStep] >> (k * 8)) & 0xFF;
            std::sort(v, v + kCols);
            p |= (DWORD)v[kCols / 2] << (k * 8);
        }
        out[y] = p;
    }
    return true;
}

static void ScheduleSample(WinState* s, UINT ms, bool probe = true);

// Reads what the title bar looks like under the overlay. The overlay is excluded from screen
// capture only for the moment of the copy, so screenshots and recordings still show the lights.
// probe: also measure the native buttons again (shown, resized, activated), not for the
// periodic refresh. Returns whether the sample changed.
static bool SampleBackground(WinState* s, bool probe = true) {
    if (probe) ProbeNativeButtons(s);
    DpiGuard g;
    if (!ComputeGeometry(s)) return false;
    const RECT ov = s->ovRect;
    RECT area = {ov.left - 16, ov.top, ov.right, ov.bottom};
    if (IsCovered(s, area)) return false;  // keep the last sample
    s->lastSample = GetTickCount64();

    std::vector<DWORD> px;
    int w = ov.right - ov.left, h = ov.bottom - ov.top;
    if (probe) s->noButtons = s->sysMenuRemoved && s->systemButtons && s->nativeWidth == 0;
    if (s->clear || s->noButtons) {
        // Nothing to cover: the overlay stays clear apart from the lights.
        px.assign(w * h, 0);
        if (w == s->bgW && h == s->bgH && px == s->bgPx) return false;
        s->bgPx.swap(px);
        s->bgW = w;
        s->bgH = h;
        s->bgVersion++;
        return true;
    }
    bool visible = s->overlay && IsWindowVisible(s->overlay);
    bool under = !visible || SetWindowDisplayAffinity(s->overlay, WDA_EXCLUDEFROMCAPTURE);
    if (under) {
        under = CaptureScreen(ov, px);
        if (visible) SetWindowDisplayAffinity(s->overlay, WDA_NONE);
    }
    if (under) {
        std::vector<DWORD> orig(px);
        RemoveGlyphs(px, w, h, s->scale);
        // Patch only what the glyph removal changed, and only in a box around the middle of
        // each native button, where its symbol sits; plus a margin for the antialiased edges
        // and for the frame the overlay can trail a dragged window by. The rest of the copy is
        // just the title bar and doesn't need painting, so it can't go stale when the window
        // moves over something else.
        int grow = (int)(3 * s->scale + 0.5f);
        int cells = std::max(1, s->nativeCells);
        float cellW = (float)(ov.right - s->nativeLeft) / cells;
        int boxW = (int)std::min(cellW * 0.6f, 24 * s->scale), boxH = (int)(20 * s->scale);
        int boxTop = (int)(s->lightsCy - boxH / 2.0f);
        std::vector<uint8_t> changed(w * h, 0);
        auto mark = [&](int x0, int y0, int x1, int y1) {
            for (int y = std::max(0, y0); y < std::min(h, y1); y++)
                for (int x = std::max(0, x0); x < std::min(w, x1); x++)
                    changed[y * w + x] = ((orig[y * w + x] ^ px[y * w + x]) & 0xFFFFFF) != 0;
        };
        if (s->nativeWidth > 0 || s->systemButtons) {
            for (int i = 0; i < cells; i++) {
                int cx = (int)(s->nativeLeft - ov.left + (i + 0.5f) * cellW);
                mark(cx - boxW / 2, boxTop, cx + boxW / 2, boxTop + boxH);
            }
        } else {
            // The app draws its buttons without reporting where (Discord): their size and
            // layout are its own, so the whole corner is searched, and its title bar height
            // taken from where the symbols sit.
            mark(0, 0, w, h);
            long long sumY = 0, n = 0;
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    if (changed[y * w + x]) {
                        sumY += y;
                        n++;
                    }
            if (n >= (int)(20 * s->scale)) {
                int captionH = (int)(2.0 * sumY / n + 0.5);
                captionH = ClampInt(captionH, (int)(24 * s->scale), (int)(64 * s->scale));
                if (abs(captionH - s->appCaptionH) > (int)(2 * s->scale)) {
                    s->appCaptionH = captionH;
                    ScheduleSample(s, 50, false);  // again, with the cover at its new size
                }
            }
        }
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                bool patch = false;
                for (int dy = -grow; dy <= grow && !patch; dy++)
                    for (int dx = -grow; dx <= grow; dx++) {
                        int yy = y + dy, xx = x + dx;
                        if (yy >= 0 && yy < h && xx >= 0 && xx < w && changed[yy * w + xx]) {
                            patch = true;
                            break;
                        }
                    }
                px[y * w + x] = (px[y * w + x] & 0xFFFFFF) | (patch ? 0xFF000000u : 0);
            }
    } else {
        if (!CaptureBesideOverlay(ov, px)) return false;
        for (auto& p : px) p |= 0xFF000000u;
        w = 1;
    }
    s->sampleCost = GetTickCount64() - s->lastSample;
    if (w == s->bgW && h == s->bgH && px == s->bgPx) return false;
    s->bgPx.swap(px);
    s->bgW = w;
    s->bgH = h;
    s->bgVersion++;
    return true;
}

static COLORREF BgColor(WinState* s, int x, int y) {
    if (s->bgPx.empty()) return DefaultBg();
    DWORD p = s->bgPx[ClampInt(y, 0, s->bgH - 1) * s->bgW + ClampInt(x, 0, s->bgW - 1)];
    return RGB((p >> 16) & 0xFF, (p >> 8) & 0xFF, p & 0xFF);
}

static int HitSlot(WinState* s, int x, int y) {
    float d = g_settings.size * s->scale;
    float gap = g_settings.spacing * s->scale;
    float cy = s->lightsCy;
    if (y < cy - d / 2 - gap / 2 || y > cy + d / 2 + gap / 2) return -1;
    for (int i = 0; i < 3; i++) {
        float l = s->lightsX + i * (d + gap) - gap / 2;
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

// The sample is anchored to the right edge, so it still lines up while a resize has made the
// overlay wider or narrower than when it was taken. Outside the glyph patches the overlay is
// nearly clear: alpha 1 still takes the mouse, so the native buttons below stay unreachable.
static void FillBackground(Canvas& c, WinState* s) {
    if (s->bgPx.empty()) {
        COLORREF bg = DefaultBg();
        DWORD px = 0xFF000000u | ((DWORD)GetRValue(bg) << 16) | ((DWORD)GetGValue(bg) << 8) |
                   GetBValue(bg);
        std::fill(c.px, c.px + c.w * c.h, px);
        return;
    }
    int dx = s->bgW - c.w;
    for (int y = 0; y < c.h; y++) {
        const DWORD* src = s->bgPx.data() + ClampInt(y, 0, s->bgH - 1) * s->bgW;
        DWORD* row = c.px + y * c.w;
        for (int x = 0; x < c.w; x++) {
            DWORD p = src[ClampInt(x + dx, 0, s->bgW - 1)];
            row[x] = (p >> 24) ? (0xFF000000u | p) : 0x01000000u;
        }
    }
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

static void DrawOverlay(Canvas& c, WinState* s, const Sig& sg) {
    static const COLORREF fill[3]   = {RGB(255, 95, 87), RGB(254, 188, 46), RGB(40, 200, 64)};
    static const COLORREF border[3] = {RGB(224, 68, 62), RGB(222, 161, 35), RGB(30, 170, 50)};
    static const COLORREF glyph[3]  = {RGB(77, 0, 0), RGB(153, 87, 0), RGB(0, 100, 0)};

    FillBackground(c, s);
    if (IsWin11() && !sg.zoomed && sg.ov.right >= s->frame.right - 1 &&
        sg.ov.top <= s->frame.top + 1) {
        float R = 8 * s->scale;
        ClipCorner(c, (s->frame.right - sg.ov.left) - R, (s->frame.top - sg.ov.top) + R, R, true);
    }

    float d = sg.size * s->scale;
    float gap = sg.spacing * s->scale;
    float r = d / 2;
    float cy = sg.lightsCy;
    int bgX = (int)(sg.lightsX + 1.5f * d + gap) + s->bgW - c.w;
    bool lightBg = Luma(BgColor(s, bgX, (int)cy)) > 128;

    for (int i = 0; i < 3; i++) {
        int b = kOrder[i];
        float cx = sg.lightsX + i * (d + gap) + r;
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
    if (s->overlay && IsWindowVisible(s->overlay)) ShowWindow(s->overlay, SW_HIDE);
    s->shown = false;
}

static void Render(WinState* s, bool force = false) {
    if (!s->overlay) return;
    DpiGuard g;

    if (!ComputeGeometry(s)) {
        HideAll(s);
        if (s->fastTimer) {
            KillTimer(s->overlay, TIMER_HOVER);
            s->fastTimer = false;
        }
        return;
    }

    POINT pt;
    GetCursorPos(&pt);
    bool inOverlay = PtInRect(&s->ovRect, pt) != FALSE;
    bool hover = PtInRect(&s->lightsRect, pt) != FALSE;

    Sig sg;
    memset(&sg, 0, sizeof(sg));
    sg.ov = s->ovRect;
    sg.lightsX = s->lightsX;
    sg.lightsCy = s->lightsCy;
    sg.hover = hover;
    sg.hoverSlot = hover ? HitSlot(s, pt.x - s->ovRect.left, pt.y - s->ovRect.top) : -1;
    sg.pressedSlot = s->pressedSlot;
    sg.active = GetForegroundWindow() == s->hwnd;
    sg.gray = g_settings.inactiveGray && !sg.active && !hover;
    sg.style = GetWindowLongW(s->hwnd, GWL_STYLE) & (WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
    sg.bgVersion = s->bgVersion;
    sg.size = g_settings.size;
    sg.spacing = g_settings.spacing;
    sg.marginRight = g_settings.marginRight;
    sg.hideNative = g_settings.hideNative;
    sg.zoomed = IsZoomed(s->hwnd) != FALSE;

    if (inOverlay && !s->fastTimer) {
        SetTimer(s->overlay, TIMER_HOVER, 30, nullptr);
        s->fastTimer = true;
    } else if (!inOverlay && s->fastTimer && s->pressedSlot < 0) {
        KillTimer(s->overlay, TIMER_HOVER);
        s->fastTimer = false;
    }

    if (!force && s->shown && memcmp(&sg, &s->last, sizeof(sg)) == 0) return;
    s->last = sg;

    BlitLayered(s->overlay, sg.ov, [&](Canvas& c) { DrawOverlay(c, s, sg); });
    s->shown = true;
}

// Samples again after ms, then every 80 ms while the sample keeps changing: right after an
// event the screen can still show an animation (a window opening, mica fading between active
// and inactive) instead of the settled title bar.
static void ScheduleSample(WinState* s, UINT ms, bool probe) {
    if (!s->overlay) return;
    s->settle = 12;
    s->probePending = s->probePending || probe;
    SetTimer(s->overlay, TIMER_SAMPLE, ms, nullptr);
}

static void DoAction(WinState* s, int slot) {
    int b = kOrder[slot];
    if (!IsButtonEnabled(GetWindowLongW(s->hwnd, GWL_STYLE), b)) return;
    WPARAM cmd = b == BTN_CLOSE ? SC_CLOSE
               : b == BTN_MIN   ? SC_MINIMIZE
               : (IsZoomed(s->hwnd) ? SC_RESTORE : SC_MAXIMIZE);
    PostMessageW(s->hwnd, WM_SYSCOMMAND, cmd, 0);
}

// ---------------------------------------------------------------- overlay window

static LRESULT CALLBACK OverlayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    WinState* s = (WinState*)GetWindowLongPtrW(h, GWLP_USERDATA);
    bool isOv = s && h == s->overlay;
    switch (m) {
        case WM_NCCREATE:
            SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW*)l)->lpCreateParams);
            break;
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;
        case WM_MOUSEMOVE:
            if (isOv) Render(s);
            return 0;
        case WM_LBUTTONDOWN:
            if (isOv) {
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
            if (isOv && s->pressedSlot >= 0) {
                int slot = HitSlot(s, GET_X_LPARAM(l), GET_Y_LPARAM(l));
                int p = s->pressedSlot;
                s->pressedSlot = -1;
                if (GetCapture() == h) ReleaseCapture();
                if (slot == p) DoAction(s, slot);
                Render(s);
            }
            return 0;
        case WM_LBUTTONDBLCLK:
            if (isOv && HitSlot(s, GET_X_LPARAM(l), GET_Y_LPARAM(l)) < 0 &&
                (GetWindowLongW(s->hwnd, GWL_STYLE) & WS_MAXIMIZEBOX))
                PostMessageW(s->hwnd, WM_SYSCOMMAND, IsZoomed(s->hwnd) ? SC_RESTORE : SC_MAXIMIZE,
                             0);
            return 0;
        case WM_CAPTURECHANGED:
            if (isOv && s->pressedSlot >= 0) {
                s->pressedSlot = -1;
                Render(s);
            }
            return 0;
        case WM_TIMER:
            if (!s) return 0;
            if (w == TIMER_SAMPLE) {
                KillTimer(h, TIMER_SAMPLE);
                bool probe = s->probePending;
                s->probePending = false;
                bool changed = SampleBackground(s, probe);
                Render(s, true);
                if (changed && s->settle-- > 0) SetTimer(h, TIMER_SAMPLE, 80, nullptr);
            } else {
                // Acrylic follows whatever is behind the window, and a window that was covered
                // has no event for being uncovered, so the sample is refreshed every few
                // seconds. It only redraws when it changed.
                if (w == TIMER_POLL && ++s->pollCount % 3 == 0 && s->shown && !s->inSizeMove &&
                    SampleBackground(s, false))
                    ScheduleSample(s, 250, false);
                Render(s);
            }
            return 0;
        case WM_DESTROY:
            if (s && s->overlay == h) {
                s->overlay = nullptr;
                s->fastTimer = false;
                s->shown = false;
            }
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            break;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---------------------------------------------------------------- attach / detach

static bool IsClearApp() {
    if (g_settings.clearApps.empty()) return false;
    wchar_t path[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (!n || n >= MAX_PATH) return false;
    const wchar_t* name = wcsrchr(path, L'\\');
    std::wstring exe = name ? name + 1 : path;
    for (auto& c : exe) c = towlower(c);
    return std::find(g_settings.clearApps.begin(), g_settings.clearApps.end(), exe) !=
           g_settings.clearApps.end();
}

// Hit tests that land on a native caption button of the child pass through to the window
// itself, which turns them into title bar hits (see WM_NCHITTEST in WindowSubclassProc): no
// hover highlight, no click. HTCAPTION in a child would drag the child, not the window.
static LRESULT CALLBACK ButtonChildSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id,
                                                DWORD_PTR ref) {
    WinState* s = (WinState*)ref;
    if (m == WM_NCHITTEST) {
        LRESULT r = DefSubclassProc(h, m, w, l);
        if (!s->probing && g_settings.hideNative && s->overlay && IsCaptionButtonHit(r))
            return HTTRANSPARENT;
        return r;
    }
    if (m == WM_NCDESTROY) {
        RemoveWindowSubclass(h, ButtonChildSubclassProc, id);
        auto& v = s->buttonChildren;
        v.erase(std::remove(v.begin(), v.end(), h), v.end());
    }
    return DefSubclassProc(h, m, w, l);
}

// Without WS_SYSMENU Windows draws no caption buttons (nor the window icon), so there's
// nothing left to hover, click or flash, while minimize and maximize keep working through
// their own styles (taskbar, Win+arrows, snapping) and Alt+F4 still closes.
static void RemoveSysMenu(WinState* s) {
    LONG st = GetWindowLongW(s->hwnd, GWL_STYLE);
    if (!g_settings.hideNative || !(st & WS_SYSMENU)) return;
    RECT b;
    s->systemButtons =
        SUCCEEDED(DwmGetWindowAttribute(s->hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &b, sizeof(b))) &&
        b.right > b.left && b.bottom > b.top;
    s->sysMenuRemoved = true;
    SetWindowLongW(s->hwnd, GWL_STYLE, st & ~WS_SYSMENU);
    SetWindowPos(s->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

static void RestoreSysMenu(WinState* s) {
    if (!s->sysMenuRemoved) return;
    s->sysMenuRemoved = false;
    if (!IsWindow(s->hwnd)) return;
    SetWindowLongW(s->hwnd, GWL_STYLE, GetWindowLongW(s->hwnd, GWL_STYLE) | WS_SYSMENU);
    SetWindowPos(s->hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// The window menu, which Windows only opens by itself for windows with WS_SYSMENU.
static void ShowWindowMenu(WinState* s, int x, int y) {
    HMENU menu = GetSystemMenu(s->hwnd, FALSE);
    if (!menu) return;
    SetForegroundWindow(s->hwnd);
    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, s->hwnd, nullptr);
    if (cmd) PostMessageW(s->hwnd, WM_SYSCOMMAND, cmd, 0);
}

static void ReleaseButtonChildren(WinState* s) {
    for (HWND c : s->buttonChildren)
        if (IsWindow(c)) RemoveWindowSubclass(c, ButtonChildSubclassProc, 1);
    s->buttonChildren.clear();
}

static void Detach(WinState* s) {
    ReleaseButtonChildren(s);
    RestoreSysMenu(s);
    if (s->overlay) DestroyWindow(s->overlay);
    s->overlay = nullptr;
}

static void Attach(WinState* s) {
    if (s->overlay || g_unloading || !IsWindowVisible(s->hwnd)) return;
    {
        DpiGuard g;  // overlay must be per-monitor aware to use physical pixels
        s->overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                     kOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, s->hwnd, nullptr,
                                     g_hInst, s);
    }
    if (!s->overlay) {
        Wh_Log(L"Failed to create overlay: %u", GetLastError());
        return;
    }
    s->clear = IsClearApp();
    RemoveSysMenu(s);
    SetTimer(s->overlay, TIMER_POLL, 1000, nullptr);
    SampleBackground(s);
    Render(s, true);
    ScheduleSample(s, 250, false);
}

static void OnCursorActivity(WinState* s) {
    if (!s->overlay || s->fastTimer) return;
    POINT pt;
    {
        DpiGuard g;
        GetCursorPos(&pt);
    }
    if (PtInRect(&s->ovRect, pt)) Render(s);
}

static LRESULT CALLBACK WindowSubclassProc(HWND h, UINT m, WPARAM w, LPARAM l, DWORD_PTR ref) {
    WinState* s = (WinState*)ref;

    if (m == g_msgAttach) {
        if (s->overlay) {
            s->clear = IsClearApp();  // settings may have changed
            if (g_settings.hideNative) RemoveSysMenu(s);
            else RestoreSysMenu(s);
            SampleBackground(s);
            Render(s, true);
            ScheduleSample(s, 250, false);
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
            if (!s->overlay) {
                Attach(s);
            } else {
                // While dragged, a see-through title bar changes with every step, so the cover
                // follows it, at most every 60 ms and never more than a fifth of the time, as
                // copies cost more on high-DPI screens (no probe: the buttons don't change).
                if (s->inSizeMove && !(wp->flags & SWP_NOMOVE) &&
                    GetTickCount64() - s->lastSample >= std::max<ULONGLONG>(60, 5 * s->sampleCost))
                    SampleBackground(s, false);
                Render(s);
                // Moves count too: mica shows the wallpaper behind the window.
                if (((wp->flags & (SWP_NOSIZE | SWP_NOMOVE)) != (SWP_NOSIZE | SWP_NOMOVE) &&
                     !s->inSizeMove) ||
                    (wp->flags & SWP_SHOWWINDOW))
                    ScheduleSample(s, 100);
            }
            return r;
        }
        case WM_ENTERSIZEMOVE:
            s->inSizeMove = true;
            break;
        case WM_EXITSIZEMOVE:
            s->inSizeMove = false;
            ScheduleSample(s, 20, false);
            break;
        case WM_SHOWWINDOW:
            if (!w) HideAll(s);
            break;
        case WM_STYLECHANGED:
            // The app put WS_SYSMENU back (some reset their whole style): take it off again.
            if (w == (WPARAM)GWL_STYLE && s->overlay && g_settings.hideNative && !g_unloading &&
                (((STYLESTRUCT*)l)->styleNew & WS_SYSMENU)) {
                s->sysMenuRemoved = false;
                PostMessageW(h, g_msgAttach, 0, 0);
            }
            break;
        case WM_SYSCOMMAND:
            // Alt+Space
            if (s->sysMenuRemoved && (w & 0xFFF0) == SC_KEYMENU && l == VK_SPACE) {
                RECT r;
                GetWindowRect(h, &r);
                ShowWindowMenu(s, r.left + 8, r.top + 32);
                return 0;
            }
            break;
        case WM_NCACTIVATE:
        case WM_ACTIVATEAPP: {
            LRESULT r = DefSubclassProc(h, m, w, l);
            if (s->overlay) {
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
        case WM_NCHITTEST: {
            // The native buttons stop being buttons: no hover, no clicks and no
            // snap layouts flyout, while the window keeps its styles, so minimize
            // and maximize still work everywhere else (taskbar, Win+arrows).
            // The probe still needs the real answer.
            LRESULT r = DefSubclassProc(h, m, w, l);
            if (!s->probing) {
                OnCursorActivity(s);
                if (g_settings.hideNative && s->overlay && IsCaptionButtonHit(r)) r = HTCAPTION;
            }
            return r;
        }
        case WM_SETCURSOR:
        case WM_NCMOUSEMOVE:
        case WM_MOUSEMOVE:
            // Not for the probe's own hit tests.
            if (!s->probing) OnCursorActivity(s);
            break;
        case WM_NCDESTROY: {
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                g_windows.erase(h);
            }
            ReleaseButtonChildren(s);
            if (s->overlay && IsWindow(s->overlay)) DestroyWindow(s->overlay);
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
        Wh_Log(L"Subclassing failed: %p", h);
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
    g_settings.marginRight = ClampInt(Wh_GetIntSetting(L"marginRight"), 0, 100);
    g_settings.inactiveGray = Wh_GetIntSetting(L"inactiveGray") != 0;
    g_settings.hideNative = Wh_GetIntSetting(L"hideNative") != 0;
    g_settings.clearApps.clear();
    PCWSTR list = Wh_GetStringSetting(L"clearApps");
    std::wstring cur;
    for (const wchar_t* p = list ? list : L"";; p++) {
        if (!*p || *p == L',' || *p == L';') {
            size_t a = cur.find_first_not_of(L" \t"), b = cur.find_last_not_of(L" \t");
            if (a != std::wstring::npos) g_settings.clearApps.push_back(cur.substr(a, b - a + 1));
            cur.clear();
            if (!*p) break;
        } else {
            cur += (wchar_t)towlower(*p);
        }
    }
    Wh_FreeStringSetting(list);
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
