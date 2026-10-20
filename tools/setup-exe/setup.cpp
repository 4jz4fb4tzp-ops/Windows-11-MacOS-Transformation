// Setup.exe - graphical installer for the Windows 11 macOS Transformation.
//
// A small wizard (welcome, options, installation, finish) in front of
// setup\Setup.ps1, which does the actual work. The script is started hidden
// with -Gui and reports its progress as "@@" lines on stdout.
//
// Build (Linux, MinGW-w64): tools/setup-exe/build.sh

#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Gdiplus {
using std::max;
using std::min;
}  // namespace Gdiplus
#include <gdiplus.h>

// ---------------------------------------------------------------- text
bool g_de;
const wchar_t* T(const wchar_t* de, const wchar_t* en) { return g_de ? de : en; }

// ---------------------------------------------------------------- mods.json
// Only the optional mods are needed: id, name and the question text.
struct OptionalMod {
    std::wstring id, name, question;
    bool selected;
};
std::vector<OptionalMod> g_optional;

struct JsonValue {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool b = false;
    std::wstring s;
    std::vector<JsonValue> items;
    std::vector<std::pair<std::wstring, JsonValue>> fields;
    const JsonValue* Get(const wchar_t* key) const {
        for (auto& f : fields) {
            if (f.first == key) return &f.second;
        }
        return nullptr;
    }
};

struct JsonParser {
    const std::wstring& t;
    size_t p = 0;
    explicit JsonParser(const std::wstring& text) : t(text) {}
    void Ws() {
        while (p < t.size() && iswspace(t[p])) p++;
    }
    bool Parse(JsonValue& v) {
        Ws();
        if (p >= t.size()) return false;
        wchar_t c = t[p];
        if (c == L'{') {
            v.type = JsonValue::Object;
            p++;
            Ws();
            if (p < t.size() && t[p] == L'}') {
                p++;
                return true;
            }
            for (;;) {
                Ws();
                JsonValue key;
                if (!Parse(key) || key.type != JsonValue::String) return false;
                Ws();
                if (p >= t.size() || t[p] != L':') return false;
                p++;
                JsonValue val;
                if (!Parse(val)) return false;
                v.fields.push_back({key.s, std::move(val)});
                Ws();
                if (p < t.size() && t[p] == L',') {
                    p++;
                    continue;
                }
                if (p < t.size() && t[p] == L'}') {
                    p++;
                    return true;
                }
                return false;
            }
        }
        if (c == L'[') {
            v.type = JsonValue::Array;
            p++;
            Ws();
            if (p < t.size() && t[p] == L']') {
                p++;
                return true;
            }
            for (;;) {
                JsonValue item;
                if (!Parse(item)) return false;
                v.items.push_back(std::move(item));
                Ws();
                if (p < t.size() && t[p] == L',') {
                    p++;
                    continue;
                }
                if (p < t.size() && t[p] == L']') {
                    p++;
                    return true;
                }
                return false;
            }
        }
        if (c == L'"') {
            v.type = JsonValue::String;
            p++;
            while (p < t.size() && t[p] != L'"') {
                if (t[p] == L'\\' && p + 1 < t.size()) {
                    p++;
                    wchar_t e = t[p];
                    if (e == L'n') v.s += L'\n';
                    else if (e == L't') v.s += L'\t';
                    else if (e == L'u' && p + 4 < t.size()) {
                        v.s += (wchar_t)wcstoul(t.substr(p + 1, 4).c_str(), nullptr, 16);
                        p += 4;
                    } else v.s += e;
                } else {
                    v.s += t[p];
                }
                p++;
            }
            p++;
            return true;
        }
        if (!wcsncmp(t.c_str() + p, L"true", 4)) {
            v.type = JsonValue::Bool;
            v.b = true;
            p += 4;
            return true;
        }
        if (!wcsncmp(t.c_str() + p, L"false", 5)) {
            v.type = JsonValue::Bool;
            p += 5;
            return true;
        }
        if (!wcsncmp(t.c_str() + p, L"null", 4)) {
            p += 4;
            return true;
        }
        v.type = JsonValue::Number;
        while (p < t.size() && (iswdigit(t[p]) || wcschr(L"+-.eE", t[p]))) p++;
        return true;
    }
};

std::wstring ReadUtf8File(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0,
                           nullptr);
    if (f == INVALID_HANDLE_VALUE) return L"";
    std::string bytes;
    char buf[65536];
    DWORD got = 0;
    while (ReadFile(f, buf, sizeof(buf), &got, nullptr) && got) bytes.append(buf, got);
    CloseHandle(f);
    if (bytes.size() >= 3 && (unsigned char)bytes[0] == 0xEF) bytes.erase(0, 3);
    int n = MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, bytes.data(), (int)bytes.size(), out.data(), n);
    return out;
}

// German questions for the optional mods (mods.json has English ones).
std::wstring GermanQuestion(const std::wstring& id, const std::wstring& fallback) {
    if (id == L"stage-manager-taskbar-aware")
        return L"Fenster als Karten neben dem aktiven Fenster, Win+Alt+S";
    if (id == L"three-finger-drag")
        return L"Mit drei Fingern ziehen, nur Laptops mit Touchpad";
    if (id == L"turbo-boost-tray-toggle")
        return L"CPU-Turbo-Boost per Symbol im Infobereich schalten";
    return fallback;
}

std::wstring g_baseDir, g_scriptPath;

bool LoadManifest() {
    std::wstring text = ReadUtf8File(g_baseDir + L"\\setup\\mods.json");
    if (text.empty()) return false;
    JsonValue root;
    JsonParser parser(text);
    if (!parser.Parse(root)) return false;
    const JsonValue* mods = root.Get(L"mods");
    if (!mods) return false;
    for (auto& m : mods->items) {
        const JsonValue* opt = m.Get(L"optional");
        if (!opt || opt->type != JsonValue::Bool || !opt->b) continue;
        OptionalMod om;
        if (auto v = m.Get(L"id")) om.id = v->s;
        if (auto v = m.Get(L"name")) om.name = v->s;
        if (auto v = m.Get(L"question")) om.question = v->s;
        // Strip "Install X (...)?" down to the part in brackets for the UI.
        std::wstring q = om.question;
        size_t a = q.find(L'('), b = q.rfind(L')');
        if (a != std::wstring::npos && b != std::wstring::npos && b > a) q = q.substr(a + 1, b - a - 1);
        if (!q.empty()) q[0] = towupper(q[0]);
        om.question = g_de ? GermanQuestion(om.id, q) : q;
        om.selected = om.id == L"three-finger-drag" || om.id == L"stage-manager-taskbar-aware";
        g_optional.push_back(om);
    }
    return true;
}

// ---------------------------------------------------------------- state
enum Page { P_WELCOME, P_OPTIONS, P_INSTALL, P_FINISH };
int g_page = P_WELCOME;
bool g_resetSettings = false;
std::wstring g_iconPack;

struct StepInfo {
    const wchar_t* key;
    const wchar_t* de;
    const wchar_t* en;
};
const StepInfo kSteps[] = {
    {L"project", L"Projekt nach Dokumente kopieren", L"Copy the project to Documents"},
    {L"icons", L"macOS-Icons übernehmen", L"Import the macOS icons"},
    {L"windhawk", L"Windhawk prüfen und installieren", L"Check and install Windhawk"},
    {L"mods", L"Mods installieren und kompilieren", L"Install and compile the mods"},
    {L"taskbar", L"Taskleiste einstellen", L"Adjust the taskbar"},
    {L"trash", L"Papierkorb fürs Dock anlegen", L"Create the Trash shortcut"},
    {L"restart", L"Windhawk und Explorer neu starten", L"Restart Windhawk and Explorer"}};
constexpr int kStepCount = ARRAYSIZE(kSteps);

std::mutex g_mutex;  // guards the fields below (reader thread vs. UI)
int g_currentStep = -1;
int g_modIndex = 0, g_modCount = 0;
std::wstring g_modName;
std::wstring g_log;
std::vector<std::wstring> g_failedMods;
std::wstring g_fatal;
bool g_finished = false, g_running = false;
HANDLE g_process;

// ---------------------------------------------------------------- colors
struct Colors {
    COLORREF bg, card, text, sub, border, accent, ok, warn, error;
    bool dark;
} C;
HBRUSH g_brBg, g_brCard;

bool WindowsDark() {
    DWORD v = 1, sz = sizeof(v);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz);
    return v == 0;
}

void InitColors() {
    bool d = WindowsDark();
    C.dark = d;
    C.bg = d ? RGB(30, 30, 32) : RGB(246, 246, 248);
    C.card = d ? RGB(44, 44, 47) : RGB(255, 255, 255);
    C.text = d ? RGB(242, 242, 247) : RGB(29, 29, 31);
    C.sub = d ? RGB(160, 160, 168) : RGB(110, 110, 115);
    C.border = d ? RGB(62, 62, 66) : RGB(225, 225, 230);
    C.accent = RGB(10, 132, 255);  // macOS blue
    C.ok = RGB(48, 209, 88);
    C.warn = RGB(255, 159, 10);
    C.error = RGB(255, 69, 58);
    g_brBg = CreateSolidBrush(C.bg);
    g_brCard = CreateSolidBrush(C.card);
}

Gdiplus::Color Gp(COLORREF c, int a = 255) {
    return Gdiplus::Color((BYTE)a, GetRValue(c), GetGValue(c), GetBValue(c));
}

void RoundPath(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = std::min({2 * r, w, h});
    if (d <= 0) {
        p.AddRectangle(Gdiplus::RectF(x, y, w, h));
        return;
    }
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

void FillRound(Gdiplus::Graphics& g, float x, float y, float w, float h, float r, Gdiplus::Color c) {
    Gdiplus::GraphicsPath p;
    RoundPath(p, x, y, w, h, r);
    Gdiplus::SolidBrush b(c);
    g.FillPath(&b, &p);
}

// ---------------------------------------------------------------- layout
HWND g_wnd, g_btnBack, g_btnNext, g_log_edit, g_btnDetails;
std::vector<HWND> g_optionToggles;
HWND g_toggleReset, g_btnIcons, g_btnIconFolder;
HFONT g_font, g_fontBold, g_fontTitle, g_fontHero, g_fontSmall, g_fontIcon;
bool g_showDetails = false;
UINT g_dpi = 96;
int S(int v) { return MulDiv(v, g_dpi, 96); }

constexpr int kW = 820, kH = 560, kSide = 280;
enum { ID_BACK = 100, ID_NEXT, ID_DETAILS, ID_ICONS, ID_ICONFOLDER, ID_RESET, ID_OPT0 = 200 };
constexpr UINT WM_SETUP_UPDATE = WM_APP + 1;

HFONT MakeFont(int px, int weight, const wchar_t* face) {
    return CreateFontW(-S(px), 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                       0, face);
}

HFONT MakeIconFont(int px) {
    for (auto face : {L"Segoe Fluent Icons", L"Segoe MDL2 Assets"}) {
        HFONT f = MakeFont(px, FW_NORMAL, face);
        HDC dc = GetDC(nullptr);
        HGDIOBJ old = SelectObject(dc, f);
        wchar_t got[LF_FACESIZE] = L"";
        GetTextFaceW(dc, LF_FACESIZE, got);
        SelectObject(dc, old);
        ReleaseDC(nullptr, dc);
        if (!_wcsicmp(got, face)) return f;
        DeleteObject(f);
    }
    return MakeFont(px, FW_NORMAL, L"Segoe MDL2 Assets");
}

void CreateFonts() {
    g_font = MakeFont(14, FW_NORMAL, L"Segoe UI Variable Text");
    g_fontBold = MakeFont(14, FW_SEMIBOLD, L"Segoe UI Variable Text");
    g_fontSmall = MakeFont(12, FW_NORMAL, L"Segoe UI Variable Text");
    g_fontTitle = MakeFont(26, FW_SEMIBOLD, L"Segoe UI Variable Display");
    g_fontHero = MakeFont(19, FW_BOLD, L"Segoe UI Variable Display");
    g_fontIcon = MakeIconFont(14);
}

RECT OptionRect(int i) {
    int x0 = S(kSide + 36), x1 = S(kW - 36), y = S(140) + i * S(64);
    return {x0, y, x1, y + S(58)};
}

RECT ResetRect() {
    RECT r = OptionRect((int)g_optional.size());
    OffsetRect(&r, 0, S(14));
    return r;
}

RECT IconRowRect() {
    RECT r = ResetRect();
    OffsetRect(&r, 0, S(64));
    return r;
}

// ---------------------------------------------------------------- painting
void DrawLabel(HDC dc, const std::wstring& s, RECT r, HFONT f, COLORREF c, UINT fmt) {
    SelectObject(dc, f);
    SetTextColor(dc, c);
    DrawTextW(dc, s.c_str(), -1, &r, fmt | DT_NOPREFIX);
}

// Rounded app icon: a little dock on a blue-violet gradient.
void DrawAppIcon(Gdiplus::Graphics& g, float x, float y, float size) {
    Gdiplus::GraphicsPath p;
    RoundPath(p, x, y, size, size, size * 0.23f);
    Gdiplus::LinearGradientBrush grad(Gdiplus::PointF(x, y), Gdiplus::PointF(x + size, y + size),
                                      Gdiplus::Color(255, 94, 92, 230), Gdiplus::Color(255, 10, 132, 255));
    g.FillPath(&grad, &p);
    Gdiplus::Pen edge(Gdiplus::Color(90, 255, 255, 255), size / 64);
    g.DrawPath(&edge, &p);
    // dock
    float dw = size * 0.72f, dh = size * 0.2f, dx = x + (size - dw) / 2, dy = y + size * 0.62f;
    FillRound(g, dx, dy, dw, dh, dh * 0.35f, Gdiplus::Color(110, 255, 255, 255));
    Gdiplus::Color tiles[] = {Gdiplus::Color(255, 255, 214, 10), Gdiplus::Color(255, 48, 209, 88),
                              Gdiplus::Color(255, 255, 69, 58), Gdiplus::Color(255, 255, 255, 255)};
    float ts = dh * 0.66f, gap = (dw - 4 * ts) / 5;
    for (int i = 0; i < 4; i++) {
        FillRound(g, dx + gap + i * (ts + gap), dy + (dh - ts) / 2, ts, ts, ts * 0.25f, tiles[i]);
    }
    // menu bar
    FillRound(g, x + size * 0.14f, y + size * 0.18f, size * 0.72f, size * 0.07f, size * 0.035f,
              Gdiplus::Color(140, 255, 255, 255));
}

void PaintSide(Gdiplus::Graphics& g, HDC dc, RECT rc) {
    float w = (float)S(kSide), h = (float)rc.bottom;
    Gdiplus::LinearGradientBrush grad(Gdiplus::PointF(0, 0), Gdiplus::PointF(w, h),
                                      Gdiplus::Color(255, 52, 36, 112), Gdiplus::Color(255, 16, 74, 160));
    g.FillRectangle(&grad, 0.0f, 0.0f, w, h);
    // soft light blobs like the macOS wallpaper
    Gdiplus::GraphicsPath blob;
    blob.AddEllipse(-w * 0.4f, h * 0.55f, w * 1.3f, h * 0.7f);
    Gdiplus::PathGradientBrush pb(&blob);
    pb.SetCenterColor(Gdiplus::Color(120, 191, 90, 242));
    Gdiplus::Color surround(0, 191, 90, 242);
    int count = 1;
    pb.SetSurroundColors(&surround, &count);
    g.FillPath(&pb, &blob);

    DrawAppIcon(g, (float)S(36), (float)S(44), (float)S(72));
    DrawLabel(dc, L"macOS Transformation", {S(36), S(132), S(kSide - 12), S(164)}, g_fontHero,
             RGB(255, 255, 255), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    DrawLabel(dc, T(L"für Windows 11", L"for Windows 11"), {S(36), S(164), S(kSide - 20), S(186)},
             g_font, RGB(214, 214, 240), DT_LEFT | DT_SINGLELINE | DT_VCENTER);

    const wchar_t* pages[][2] = {{L"Willkommen", L"Welcome"},
                                 {L"Optionen", L"Options"},
                                 {L"Installation", L"Installation"},
                                 {L"Fertig", L"Finish"}};
    for (int i = 0; i < 4; i++) {
        int y = S(232) + i * S(40);
        bool cur = i == g_page, done = i < g_page;
        if (cur) {
            FillRound(g, (float)S(24), (float)y - S(4), (float)S(kSide - 48), (float)S(34), (float)S(9),
                      Gdiplus::Color(48, 255, 255, 255));
        }
        float cx = (float)S(44), cy = (float)y + S(13), r = (float)S(9);
        if (done || cur) {
            Gdiplus::SolidBrush b(done ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 255, 255, 255));
            g.FillEllipse(&b, cx - r, cy - r, 2 * r, 2 * r);
        } else {
            Gdiplus::Pen pen(Gdiplus::Color(150, 255, 255, 255), (float)S(1));
            g.DrawEllipse(&pen, cx - r, cy - r, 2 * r, 2 * r);
        }
        if (done) {
            DrawLabel(dc, L"\uE73E", {(int)(cx - r), (int)(cy - r), (int)(cx + r), (int)(cy + r)}, g_fontIcon,
                     RGB(52, 36, 112), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        } else if (cur) {
            Gdiplus::SolidBrush b(Gdiplus::Color(255, 52, 36, 112));
            g.FillEllipse(&b, cx - r / 2.5f, cy - r / 2.5f, r / 1.25f, r / 1.25f);
        }
        DrawLabel(dc, T(pages[i][0], pages[i][1]), {S(64), y - S(4), S(kSide - 24), y + S(30)},
                 cur ? g_fontBold : g_font, cur || done ? RGB(255, 255, 255) : RGB(200, 200, 230),
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
    DrawLabel(dc, L"github.com/4jz4fb4tzp-ops", {S(36), (int)h - S(44), S(kSide - 20), (int)h - S(20)},
             g_fontSmall, RGB(190, 190, 225), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void PaintCard(Gdiplus::Graphics& g, RECT r) {
    Gdiplus::GraphicsPath p;
    RoundPath(p, (float)r.left, (float)r.top, (float)(r.right - r.left - 1), (float)(r.bottom - r.top - 1),
              (float)S(10));
    Gdiplus::SolidBrush b(Gp(C.card));
    g.FillPath(&b, &p);
    Gdiplus::Pen pen(Gp(C.border), 1.0f);
    g.DrawPath(&pen, &p);
}

std::wstring Fmt(const wchar_t* f, ...) {
    wchar_t buf[512];
    va_list a;
    va_start(a, f);
    vswprintf(buf, 512, f, a);
    va_end(a);
    return buf;
}

void PaintContent(Gdiplus::Graphics& g, HDC dc, RECT rc) {
    int x0 = S(kSide + 36), x1 = S(kW - 36);
    auto title = [&](const std::wstring& t, const std::wstring& sub) {
        DrawLabel(dc, t, {x0, S(36), x1, S(76)}, g_fontTitle, C.text, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        DrawLabel(dc, sub, {x0, S(80), x1, S(124)}, g_font, C.sub, DT_LEFT | DT_WORDBREAK);
    };

    if (g_page == P_WELCOME) {
        title(T(L"Willkommen", L"Welcome"),
              T(L"Dieses Setup verwandelt Windows 11 in einen Mac-Look: Dock, Menüleiste, Animationen und mehr.",
                L"This setup turns Windows 11 into a Mac look: dock, menu bar, animations and more."));
        struct Item {
            const wchar_t* glyph;
            const wchar_t *de, *en, *subDe, *subEn;
        } items[] = {
            {L"\uE7C4", L"macOS-Dock", L"macOS dock", L"Schwebendes Glas-Dock, unten, links oder rechts",
             L"Floating glass dock at the bottom, left or right"},
            {L"\uE700", L"Menüleiste mit Kontrollzentrum", L"Menu bar with Control Center",
             L"App-Menüs, Uhr, WLAN, Ton, Akku", L"App menus, clock, Wi-Fi, sound, battery"},
            {L"\uE7B8", L"Animationen", L"Animations",
             L"Genie-Minimieren und Dock-Vergrößerung", L"Genie minimize and dock magnification"},
            {L"\uE771", L"Fenster und Icons", L"Windows and icons",
             L"Abgerundete Ecken, macOS-Icons im Explorer", L"Rounded corners, macOS icons in Explorer"},
        };
        for (int i = 0; i < 4; i++) {
            RECT r{x0, S(136) + i * S(66), x1, S(136) + i * S(66) + S(58)};
            PaintCard(g, r);
            FillRound(g, (float)r.left + S(14), (float)r.top + S(13), (float)S(32), (float)S(32), (float)S(8),
                      Gp(C.accent, 40));
            DrawLabel(dc, items[i].glyph, {r.left + S(14), r.top + S(13), r.left + S(46), r.top + S(45)},
                     g_fontIcon, C.accent, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawLabel(dc, T(items[i].de, items[i].en), {r.left + S(60), r.top + S(9), r.right - S(12), r.top + S(30)},
                     g_fontBold, C.text, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            DrawLabel(dc, T(items[i].subDe, items[i].subEn),
                     {r.left + S(60), r.top + S(30), r.right - S(12), r.bottom - S(6)}, g_fontSmall, C.sub,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        }
        DrawLabel(dc,
                 T(L"Windhawk wird bei Bedarf mitinstalliert. Die Installation braucht Admin-Rechte, dauert ein "
                   L"paar Minuten und startet am Ende den Explorer neu.",
                   L"Windhawk is installed if needed. The installation needs admin rights, takes a few minutes "
                   L"and restarts Explorer at the end."),
                 {x0, S(410), x1, S(470)}, g_fontSmall, C.sub, DT_LEFT | DT_WORDBREAK);
        DrawLabel(dc,
                  T(L"Unabhängiges Fanprojekt, nicht mit Apple Inc. verbunden. macOS ist eine Marke von Apple Inc.",
                    L"Independent fan project, not affiliated with Apple Inc. macOS is a trademark of Apple Inc."),
                  {x0, S(450), x1, S(490)}, g_fontSmall, C.sub, DT_LEFT | DT_WORDBREAK);
    } else if (g_page == P_OPTIONS) {
        title(T(L"Optionen", L"Options"),
              T(L"Dock, Menüleiste und Animationen werden immer installiert. Dazu kannst du wählen:",
                L"Dock, menu bar and animations are always installed. You can add:"));
        for (size_t i = 0; i < g_optional.size(); i++) PaintCard(g, OptionRect((int)i));
        PaintCard(g, ResetRect());
        RECT ir = IconRowRect();
        PaintCard(g, ir);
        DrawLabel(dc, T(L"macOS-Icon-Paket", L"macOS icon pack"), {ir.left + S(16), ir.top + S(8), ir.right - S(214), ir.top + S(30)},
                 g_fontBold, C.text, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        std::wstring sub = g_iconPack.empty()
                               ? T(L"Optional, auch per Drag & Drop", L"Optional, drag & drop works too")
                               : g_iconPack.substr(g_iconPack.find_last_of(L"\\/") + 1);
        DrawLabel(dc, sub, {ir.left + S(16), ir.top + S(30), ir.right - S(214), ir.bottom - S(6)}, g_fontSmall,
                 g_iconPack.empty() ? C.sub : C.accent, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    } else if (g_page == P_INSTALL || g_page == P_FINISH) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_page == P_INSTALL) {
            title(T(L"Installation läuft …", L"Installing …"),
                  T(L"Das dauert ein paar Minuten. Die eigenen Mods werden dabei kompiliert.",
                    L"This takes a few minutes while the custom mods are compiled."));
        } else if (!g_fatal.empty()) {
            title(T(L"Setup fehlgeschlagen", L"Setup failed"), g_fatal);
        } else if (!g_failedMods.empty()) {
            std::wstring list;
            for (auto& m : g_failedMods) list += (list.empty() ? L"" : L", ") + m;
            title(T(L"Fertig – mit Fehlern", L"Done – with errors"),
                  std::wstring(T(L"Nicht installiert: ", L"Not installed: ")) + list);
        } else {
            title(T(L"Fertig!", L"All done!"),
                  T(L"Viel Spaß mit deinem Mac-Look. Rechtsklick auf ein App-Icon im Dock öffnet die Dock-Einstellungen.",
                    L"Enjoy your Mac look. Right-click an app in the dock to open the dock settings."));
        }

        if (!g_showDetails) {
            // Step list
            for (int i = 0; i < kStepCount; i++) {
                int y = S(136) + i * S(36);
                bool done = g_finished ? (g_fatal.empty() || i < g_currentStep) : i < g_currentStep;
                bool cur = !g_finished && i == g_currentStep;
                float cx = (float)x0 + S(11), cy = (float)y + S(14), r = (float)S(10);
                if (done) {
                    Gdiplus::SolidBrush b(Gp(C.ok));
                    g.FillEllipse(&b, cx - r, cy - r, 2 * r, 2 * r);
                    DrawLabel(dc, L"\uE73E", {(int)(cx - r), (int)(cy - r), (int)(cx + r), (int)(cy + r)}, g_fontIcon,
                             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                } else if (cur) {
                    // spinner
                    Gdiplus::Pen track(Gp(C.border), (float)S(3));
                    g.DrawEllipse(&track, cx - r + S(1), cy - r + S(1), 2 * r - S(2), 2 * r - S(2));
                    Gdiplus::Pen arc(Gp(C.accent), (float)S(3));
                    arc.SetStartCap(Gdiplus::LineCapRound);
                    arc.SetEndCap(Gdiplus::LineCapRound);
                    float start = (float)((GetTickCount() / 3) % 360);
                    g.DrawArc(&arc, cx - r + S(1), cy - r + S(1), 2 * r - S(2), 2 * r - S(2), start, 100.0f);
                } else if (g_finished && !g_fatal.empty() && i == g_currentStep) {
                    Gdiplus::SolidBrush b(Gp(C.error));
                    g.FillEllipse(&b, cx - r, cy - r, 2 * r, 2 * r);
                    DrawLabel(dc, L"\uE711", {(int)(cx - r), (int)(cy - r), (int)(cx + r), (int)(cy + r)}, g_fontIcon,
                             RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                } else {
                    Gdiplus::Pen pen(Gp(C.border), (float)S(2));
                    g.DrawEllipse(&pen, cx - r + S(1), cy - r + S(1), 2 * r - S(2), 2 * r - S(2));
                }
                std::wstring label = T(kSteps[i].de, kSteps[i].en);
                if (cur && i == 3 && g_modCount) {
                    label += Fmt(L"  ·  %d/%d  %ls", g_modIndex, g_modCount, g_modName.c_str());
                }
                DrawLabel(dc, label, {x0 + S(34), y, x1, y + S(28)}, cur ? g_fontBold : g_font,
                         done || cur ? C.text : C.sub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }

        // Progress bar
        float frac = 0;
        if (g_finished) {
            frac = 1;
        } else if (g_currentStep >= 0) {
            frac = (float)g_currentStep / kStepCount;
            if (g_currentStep == 3 && g_modCount) frac += (float)(g_modIndex - 1) / g_modCount / kStepCount;
        }
        float bx = (float)x0, by = (float)S(rc.bottom * 96 / (int)g_dpi - 112), bw = (float)(x1 - x0), bh = (float)S(6);
        FillRound(g, bx, by, bw, bh, bh / 2, Gp(C.border));
        if (frac > 0)
            FillRound(g, bx, by, std::max(bh, bw * frac), bh, bh / 2,
                      Gp(g_finished && (!g_fatal.empty()) ? C.error : C.accent));
    }
}

void Paint(HWND w) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(w, &ps);
    RECT rc;
    GetClientRect(w, &rc);
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);
    FillRect(mem, &rc, g_brBg);
    {
        Gdiplus::Graphics g(mem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        PaintSide(g, mem, rc);
        PaintContent(g, mem, rc);
    }
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(w, &ps);
}

// Owner-drawn controls: pill buttons and switch rows.
void DrawOwner(const DRAWITEMSTRUCT* d) {
    RECT r = d->rcItem;
    int w = r.right - r.left, h = r.bottom - r.top;
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, h);
    HGDIOBJ old = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);
    int id = (int)d->CtlID;
    bool isToggle = id >= ID_OPT0 || id == ID_RESET;
    RECT lr{0, 0, w, h};
    bool onCard = id == ID_ICONS || id == ID_ICONFOLDER;
    FillRect(mem, &lr, onCard ? g_brCard : g_brBg);
    {
        Gdiplus::Graphics g(mem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        bool pressed = d->itemState & ODS_SELECTED;
        bool disabled = d->itemState & ODS_DISABLED;
        if (isToggle) {
            PaintCard(g, lr);
            bool on;
            std::wstring label, sub;
            if (id == ID_RESET) {
                on = g_resetSettings;
                label = T(L"Einstellungen zurücksetzen", L"Reset settings");
                sub = T(L"Schon installierte Mods bekommen wieder die empfohlenen Werte",
                        L"Mods that are already installed get the recommended values again");
            } else {
                auto& m = g_optional[id - ID_OPT0];
                on = m.selected;
                label = m.name;
                sub = m.question;
            }
            DrawLabel(mem, label, {S(16), S(8), w - S(80), S(30)}, g_fontBold, C.text,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            DrawLabel(mem, sub, {S(16), S(30), w - S(80), h - S(6)}, g_fontSmall, C.sub,
                     DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            float sw = (float)S(44), sh = (float)S(24), sx = (float)w - S(16) - sw, sy = (h - sh) / 2;
            FillRound(g, sx, sy, sw, sh, sh / 2, on ? Gp(C.ok) : Gp(C.border));
            float kd = sh - S(4);
            Gdiplus::SolidBrush knob(Gdiplus::Color(255, 255, 255, 255));
            g.FillEllipse(&knob, on ? sx + sw - kd - S(2) : sx + S(2), sy + S(2), kd, kd);
        } else {
            bool primary = id == ID_NEXT;
            wchar_t text[64];
            GetWindowTextW(d->hwndItem, text, 64);
            COLORREF fill = primary  ? C.accent
                            : onCard ? (C.dark ? RGB(64, 64, 68) : RGB(236, 236, 240))
                                     : (C.dark ? RGB(56, 56, 60) : RGB(255, 255, 255));
            if (pressed) fill = RGB(GetRValue(fill) * 85 / 100, GetGValue(fill) * 85 / 100, GetBValue(fill) * 85 / 100);
            float rad = (float)S(8);
            FillRound(g, 0, 0, (float)w, (float)h, rad, Gp(fill, disabled ? 110 : 255));
            if (!primary && !onCard) {
                Gdiplus::GraphicsPath p;
                RoundPath(p, 0.5f, 0.5f, (float)w - 1, (float)h - 1, rad);
                Gdiplus::Pen pen(Gp(C.border), 1.0f);
                g.DrawPath(&pen, &p);
            }
            DrawLabel(mem, text, lr, onCard ? g_font : g_fontBold, primary ? RGB(255, 255, 255) : (disabled ? C.sub : C.text),
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    BitBlt(d->hDC, r.left, r.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// ---------------------------------------------------------------- install
void AppendLog(const std::wstring& line) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_log += line + L"\r\n";
}

void HandleLine(const std::wstring& line) {
    if (line.rfind(L"@@", 0) != 0) {
        AppendLog(line);
        return;
    }
    std::vector<std::wstring> parts;
    size_t start = 2;
    for (;;) {
        size_t bar = line.find(L'|', start);
        parts.push_back(line.substr(start, bar == std::wstring::npos ? std::wstring::npos : bar - start));
        if (bar == std::wstring::npos) break;
        start = bar + 1;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::wstring& cmd = parts[0];
    if (cmd == L"STEP" && parts.size() > 1) {
        for (int i = 0; i < kStepCount; i++) {
            if (parts[1] == kSteps[i].key) g_currentStep = i;
        }
    } else if (cmd == L"MOD" && parts.size() > 3) {
        g_modIndex = _wtoi(parts[1].c_str());
        g_modCount = _wtoi(parts[2].c_str());
        g_modName = parts[3];
    } else if (cmd == L"MODFAIL" && parts.size() > 1) {
        g_failedMods.push_back(parts[1]);
    } else if (cmd == L"DONE") {
        g_currentStep = kStepCount;
    } else if (cmd == L"FAIL" && parts.size() > 1) {
        g_fatal = parts[1];
    }
}

std::wstring Quote(const std::wstring& s) {
    std::wstring r = L"\"";
    for (wchar_t c : s) {
        if (c == L'"') r += L'\\';
        r += c;
    }
    return r + L"\"";
}

void RunSetup() {
    std::wstring optional;
    for (auto& m : g_optional) {
        if (m.selected) optional += (optional.empty() ? L"" : L",") + m.id;
    }
    if (optional.empty()) optional = L"none";

    WCHAR ps[MAX_PATH];
    ExpandEnvironmentStringsW(L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe", ps, MAX_PATH);
    std::wstring cmd = Quote(ps) + L" -NoProfile -ExecutionPolicy Bypass -File " + Quote(g_scriptPath) +
                       L" -Gui -Optional " + optional;
    if (g_resetSettings) cmd += L" -ResetSettings";
    if (!g_iconPack.empty()) cmd += L" -IconPack " + Quote(g_iconPack);

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readPipe, writePipe;
    CreatePipe(&readPipe, &writePipe, &sa, 0);
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);
    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                             g_baseDir.c_str(), &si, &pi);
    CloseHandle(writePipe);
    if (!ok) {
        CloseHandle(readPipe);
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_fatal = T(L"PowerShell konnte nicht gestartet werden.", L"PowerShell could not be started.");
            g_finished = true;
            g_running = false;
        }
        PostMessageW(g_wnd, WM_SETUP_UPDATE, 1, 0);
        return;
    }
    g_process = pi.hProcess;
    CloseHandle(pi.hThread);

    std::string pending;
    char buf[4096];
    DWORD got = 0;
    while (ReadFile(readPipe, buf, sizeof(buf), &got, nullptr) && got) {
        pending.append(buf, got);
        size_t nl;
        while ((nl = pending.find('\n')) != std::string::npos) {
            std::string raw = pending.substr(0, nl);
            pending.erase(0, nl + 1);
            if (!raw.empty() && raw.back() == '\r') raw.pop_back();
            int n = MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), nullptr, 0);
            std::wstring line(n, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, raw.data(), (int)raw.size(), line.data(), n);
            HandleLine(line);
            PostMessageW(g_wnd, WM_SETUP_UPDATE, 0, 0);
        }
    }
    CloseHandle(readPipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_currentStep < kStepCount && g_fatal.empty())
            g_fatal = T(L"Das Setup wurde unerwartet beendet. Details siehe Protokoll.",
                        L"The setup ended unexpectedly. See the log for details.");
        g_finished = true;
        g_running = false;
    }
    CloseHandle(pi.hProcess);
    g_process = nullptr;
    PostMessageW(g_wnd, WM_SETUP_UPDATE, 1, 0);
}

// ---------------------------------------------------------------- UI logic
void LayoutControls() {
    int bw = S(116), bh = S(34), by = S(kH - 62);
    MoveWindow(g_btnNext, S(kW - 36) - bw, by, bw, bh, TRUE);
    MoveWindow(g_btnBack, S(kW - 36) - 2 * bw - S(10), by, bw, bh, TRUE);
    MoveWindow(g_btnDetails, S(kSide + 36), by, S(150), bh, TRUE);
    for (size_t i = 0; i < g_optionToggles.size(); i++) {
        RECT r = OptionRect((int)i);
        MoveWindow(g_optionToggles[i], r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
    }
    RECT r = ResetRect();
    MoveWindow(g_toggleReset, r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);
    RECT ir = IconRowRect();
    MoveWindow(g_btnIcons, ir.right - S(200), ir.top + S(14), S(88), S(30), TRUE);
    MoveWindow(g_btnIconFolder, ir.right - S(104), ir.top + S(14), S(88), S(30), TRUE);
    MoveWindow(g_log_edit, S(kSide + 36), S(136), S(kW - kSide - 72), S(kH - 136 - 130), TRUE);
}

void UpdatePage() {
    bool opt = g_page == P_OPTIONS;
    for (HWND h : g_optionToggles) ShowWindow(h, opt ? SW_SHOW : SW_HIDE);
    ShowWindow(g_toggleReset, opt ? SW_SHOW : SW_HIDE);
    ShowWindow(g_btnIcons, opt ? SW_SHOW : SW_HIDE);
    ShowWindow(g_btnIconFolder, opt ? SW_SHOW : SW_HIDE);
    bool progress = g_page == P_INSTALL || g_page == P_FINISH;
    ShowWindow(g_btnDetails, progress ? SW_SHOW : SW_HIDE);
    ShowWindow(g_log_edit, progress && g_showDetails ? SW_SHOW : SW_HIDE);
    SetWindowTextW(g_btnDetails, g_showDetails ? T(L"Schritte anzeigen", L"Show steps")
                                               : T(L"Details anzeigen", L"Show details"));
    ShowWindow(g_btnBack, g_page == P_OPTIONS ? SW_SHOW : SW_HIDE);
    const wchar_t* next = g_page == P_WELCOME   ? T(L"Weiter", L"Continue")
                          : g_page == P_OPTIONS ? T(L"Installieren", L"Install")
                          : g_page == P_INSTALL ? T(L"Bitte warten", L"Please wait")
                                                : T(L"Fertig", L"Done");
    SetWindowTextW(g_btnNext, next);
    EnableWindow(g_btnNext, g_page != P_INSTALL);
    InvalidateRect(g_wnd, nullptr, FALSE);
    InvalidateRect(g_btnNext, nullptr, FALSE);
}

void PickIconPack(bool folder) {
    if (folder) {
        BROWSEINFOW bi{};
        bi.hwndOwner = g_wnd;
        bi.lpszTitle = T(L"Ordner mit den macOS-Icons wählen", L"Choose the folder with the macOS icons");
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
        if (!pidl) return;
        WCHAR path[MAX_PATH];
        if (SHGetPathFromIDListW(pidl, path)) g_iconPack = path;
        CoTaskMemFree(pidl);
    } else {
        WCHAR file[MAX_PATH] = L"";
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = g_wnd;
        ofn.lpstrFilter = g_de ? L"ZIP-Archiv (*.zip)\0*.zip\0" : L"ZIP archive (*.zip)\0*.zip\0";
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return;
        g_iconPack = file;
    }
    InvalidateRect(g_wnd, nullptr, FALSE);
}

LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: {
            g_wnd = w;
            g_dpi = GetDpiForWindow(w);
            BOOL dark = C.dark;
            DwmSetWindowAttribute(w, 20, &dark, sizeof(dark));
            int backdrop = 2;  // Mica where available
            DwmSetWindowAttribute(w, 38, &backdrop, sizeof(backdrop));
            CreateFonts();
            HINSTANCE inst = GetModuleHandleW(nullptr);
            auto button = [&](int id, const wchar_t* text) {
                HWND b = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0,
                                         10, 10, w, (HMENU)(INT_PTR)id, inst, nullptr);
                SendMessageW(b, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
                return b;
            };
            g_btnBack = button(ID_BACK, T(L"Zurück", L"Back"));
            g_btnNext = button(ID_NEXT, L"");
            g_btnDetails = button(ID_DETAILS, L"");
            g_btnIcons = button(ID_ICONS, T(L"ZIP …", L"ZIP …"));
            g_btnIconFolder = button(ID_ICONFOLDER, T(L"Ordner …", L"Folder …"));
            for (size_t i = 0; i < g_optional.size(); i++) {
                g_optionToggles.push_back(button(ID_OPT0 + (int)i, g_optional[i].name.c_str()));
            }
            g_toggleReset = button(ID_RESET, L"");
            g_log_edit = CreateWindowExW(0, L"EDIT", L"",
                                         WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 10,
                                         10, w, nullptr, inst, nullptr);
            HFONT mono = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0,
                                     CLEARTYPE_QUALITY, 0, L"Cascadia Mono");
            SendMessageW(g_log_edit, WM_SETFONT, (WPARAM)mono, TRUE);
            if (C.dark) SetWindowTheme(g_log_edit, L"DarkMode_Explorer", nullptr);
            DragAcceptFiles(w, TRUE);
            LayoutControls();
            UpdatePage();
            SetTimer(w, 1, 40, nullptr);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
            Paint(w);
            return 0;
        case WM_DRAWITEM:
            DrawOwner((const DRAWITEMSTRUCT*)lp);
            return TRUE;
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            SetBkColor((HDC)wp, C.card);
            SetTextColor((HDC)wp, C.text);
            return (LRESULT)g_brCard;
        case WM_TIMER:
            if (g_page == P_INSTALL) {
                // spinner
                RECT r{S(kSide), S(130), S(kW), S(kH - 90)};
                InvalidateRect(w, &r, FALSE);
            }
            return 0;
        case WM_DROPFILES: {
            WCHAR path[MAX_PATH];
            if (g_page == P_OPTIONS && DragQueryFileW((HDROP)wp, 0, path, MAX_PATH)) {
                g_iconPack = path;
                InvalidateRect(w, nullptr, FALSE);
            }
            DragFinish((HDROP)wp);
            return 0;
        }
        case WM_SETUP_UPDATE: {
            if (wp == 1) {
                g_page = P_FINISH;
                UpdatePage();
            }
            std::wstring log;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                log = g_log;
            }
            int len = GetWindowTextLengthW(g_log_edit);
            if (len != (int)log.size()) {
                SetWindowTextW(g_log_edit, log.c_str());
                SendMessageW(g_log_edit, EM_SETSEL, log.size(), log.size());
                SendMessageW(g_log_edit, EM_SCROLLCARET, 0, 0);
            }
            InvalidateRect(w, nullptr, FALSE);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id >= ID_OPT0 && id < ID_OPT0 + (int)g_optional.size()) {
                g_optional[id - ID_OPT0].selected = !g_optional[id - ID_OPT0].selected;
                InvalidateRect((HWND)lp, nullptr, FALSE);
            } else if (id == ID_RESET) {
                g_resetSettings = !g_resetSettings;
                InvalidateRect((HWND)lp, nullptr, FALSE);
            } else if (id == ID_ICONS || id == ID_ICONFOLDER) {
                PickIconPack(id == ID_ICONFOLDER);
            } else if (id == ID_DETAILS) {
                g_showDetails = !g_showDetails;
                UpdatePage();
            } else if (id == ID_BACK) {
                g_page = P_WELCOME;
                UpdatePage();
            } else if (id == ID_NEXT) {
                if (g_page == P_WELCOME) {
                    g_page = P_OPTIONS;
                } else if (g_page == P_OPTIONS) {
                    g_page = P_INSTALL;
                    g_running = true;
                    std::thread(RunSetup).detach();
                } else if (g_page == P_FINISH) {
                    DestroyWindow(w);
                    return 0;
                }
                UpdatePage();
            }
            return 0;
        }
        case WM_CLOSE:
            if (g_running &&
                MessageBoxW(w,
                            T(L"Die Installation läuft noch. Wirklich abbrechen?\nHalb installierte Mods kannst du "
                              L"mit einem neuen Setup reparieren.",
                              L"The installation is still running. Cancel it?\nRun the setup again to repair "
                              L"half-installed mods."),
                            L"macOS Transformation", MB_YESNO | MB_ICONWARNING) != IDYES) {
                return 0;
            }
            if (g_running && g_process) TerminateProcess(g_process, 1);
            DestroyWindow(w);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    g_de = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Gdiplus::GdiplusStartupInput gin;
    ULONG_PTR gtoken;
    Gdiplus::GdiplusStartup(&gtoken, &gin, nullptr);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WCHAR exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    g_baseDir = exe;
    g_baseDir = g_baseDir.substr(0, g_baseDir.find_last_of(L"\\/"));
    g_scriptPath = g_baseDir + L"\\setup\\Setup.ps1";
    if (GetFileAttributesW(g_scriptPath.c_str()) == INVALID_FILE_ATTRIBUTES || !LoadManifest()) {
        MessageBoxW(nullptr,
                    T(L"Die Setup-Dateien fehlen. Entpacke zuerst das ganze ZIP von GitHub und starte Setup.exe "
                      L"aus dem entpackten Ordner.",
                      L"The setup files are missing. Extract the whole ZIP from GitHub first and start "
                      L"Setup.exe from the extracted folder."),
                    L"macOS Transformation", MB_ICONERROR);
        return 1;
    }

    InitColors();
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = L"MacOsTransformationSetup";
    RegisterClassExW(&wc);

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    HWND w = CreateWindowExW(0, wc.lpszClassName, L"macOS Transformation – Setup", style, CW_USEDEFAULT,
                             CW_USEDEFAULT, 100, 100, nullptr, nullptr, inst, nullptr);
    UINT dpi = GetDpiForWindow(w);
    RECT r{0, 0, MulDiv(kW, dpi, 96), MulDiv(kH, dpi, 96)};
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTOPRIMARY), &mi);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    SetWindowPos(w, nullptr, mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2,
                 mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2, ww, wh, SWP_NOZORDER);
    ShowWindow(w, SW_SHOW);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(w, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    Gdiplus::GdiplusShutdown(gtoken);
    return 0;
}
