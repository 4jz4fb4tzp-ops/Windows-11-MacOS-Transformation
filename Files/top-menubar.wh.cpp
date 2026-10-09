// ==WindhawkMod==
// @id              top-menubar
// @name            Top Menu Bar
// @description     macOS-style menu bar at the top of the screen with app menus, status items and Control Center
// @version         0.29.1
// @author          Okan
// @include         explorer.exe
// @compilerOptions -lshell32 -lgdi32 -luser32 -lversion -lole32 -lwlanapi -liphlpapi -lcomctl32 -lcomdlg32 -ladvapi32 -ldwmapi -lgdiplus -lbthprops -luxtheme
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Top Menu Bar
A macOS-style menu bar for Windows 11. It sits at the top of the screen and
reserves its space, so maximized windows start below it.

**Right-click the bar → "Menu bar settings…"** opens the settings window.
All changes are applied and saved immediately. The UI language follows the
Windows display language (German or English).

## Left side
- **Logo:** Windows logo, a circle or your own image (PNG, ICO, JPG). Custom
  images are always drawn in the bar's text color.
- **App name** of the active window.
- **App menus** like on macOS: *File · Edit · View · Go · Window · Help*.
  - Classic programs with a real menu bar show their own menus (items,
    shortcuts, check marks and disabled states included).
  - Modern apps get standard menus that send the usual keyboard shortcuts.
  - Desktop and File Explorer get Finder-like menus, including **Go** with
    quick access to Desktop, Documents, Downloads, This PC, Recycle Bin …

## Right side
Every item can be shown or hidden and reordered:
Clock, calendar week, Control Center, search, battery, volume, Wi-Fi,
Bluetooth, tray icons, keyboard layout, network speed, RAM, CPU, disk space
and uptime.

Each item opens its own menu on click (or on hover, if enabled):
- **Clock:** calendar with week numbers, notifications
- **Wi-Fi:** available networks, connect / disconnect
- **Volume:** slider, mute, switch output device
- **Bluetooth:** paired devices and their status
- **Battery, keyboard layout, system** (CPU, RAM, drives)
- **Tray:** all tray icons; click, right-click and double-click are passed to the app
- **Control Center:** tiles for Wi-Fi, Bluetooth, media playback, lock,
  screenshot, project, night light, notifications, plus sound and battery

Scroll over the speaker to change the volume, middle-click to mute.

## Appearance
- **Theme:** light, dark, like Windows (switches automatically) or custom colors
- **Scaling:** 75–200 % for the bar and all menus
- **Font weight:** automatic, light, regular, medium, semibold or bold
- **Effects:** opacity from 0–100 %, blur, acrylic or adjustable blur
- **Icon style:** Windows 11 (Segoe icons) or macOS style (custom vector icons)
- **Menus:** translucent with tiles/cards, adjustable highlight color
  (Windows accent color, custom color or off) and intensity
- **Monitors:** all monitors, main monitor only or a specific monitor
*/
// ==/WindhawkModReadme==

// Windhawk bindet windows.h bereits vorher ein – Zielversion auf Windows 10
// anheben, damit GetIfTable2 (netioapi.h) verfügbar ist
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#undef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000000
#undef _WIN32_IE
#define _WIN32_IE 0x0A00

#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wlanapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <netlistmgr.h>
#include <wincodec.h>
#include <propsys.h>
#include <dwmapi.h>
#include <functional>
#include <algorithm>
#include <cmath>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <uxtheme.h>
#include <bluetoothapis.h>
#include <string>
#include <vector>
#include <cwchar>
#include <initializer_list>
#include <unordered_map>

extern "C" IMAGE_DOS_HEADER __ImageBase;

// ======================================================================
// Sprache: Deutsch bei deutscher Systemsprache, sonst Englisch
// ======================================================================
bool g_de = true;

struct TrEntry { const wchar_t* de; const wchar_t* en; };
static const TrEntry kTr[] = {
    // Module / Statusanzeigen
    {L"Uptime", L"Uptime"}, {L"Speicherplatz", L"Disk space"}, {L"Netzwerk-Speed", L"Network speed"},
    {L"Tastaturlayout", L"Keyboard layout"}, {L"Tray-Symbole", L"Tray icons"}, {L"WLAN", L"Wi-Fi"},
    {L"Lautstärke", L"Volume"}, {L"Akku", L"Battery"}, {L"Suche", L"Search"},
    {L"Kontrollzentrum", L"Control Center"}, {L"Kalenderwoche", L"Calendar week"}, {L"Uhr", L"Clock"},
    {L"Windows-Logo", L"Windows logo"}, {L"Name der aktiven App", L"Name of the active app"},
    {L"Uhr anzeigen", L"Show clock"}, {L"WLAN / Netzwerk", L"Wi-Fi / network"}, {L"Akku-Prozent", L"Battery percentage"},
    {L"Akku-Restzeit", L"Battery time remaining"}, {L"Netzwerk-Geschwindigkeit", L"Network speed"},
    {L"CPU-Auslastung", L"CPU usage"}, {L"RAM-Auslastung", L"RAM usage"}, {L"Freier Speicherplatz", L"Free disk space"},
    {L"Tray-Symbole (Menü)", L"Tray icons (menu)"}, {L"Eigene Menüs beim Klicken", L"Custom menus on click"},
    {L"Menüs schon beim Überfahren öffnen", L"Open menus on hover"}, {L"Suche (Lupe)", L"Search (magnifier)"},
    {L"App-Menüs (Datei, Bearbeiten …)", L"App menus (File, Edit …)"}, {L"Datumsformat", L"Date format"},
    {L"Zeitformat", L"Time format"}, {L"Laufwerk", L"Drive"}, {L"Bilddatei", L"Image file"},
    {L"KW", L"W"},
    // Einstellungsfenster
    {L"Menüleiste", L"Menu Bar"}, {L"Menüleiste – Einstellungen", L"Menu Bar – Settings"},
    {L"Allgemein", L"General"}, {L"Darstellung", L"Appearance"}, {L"Leiste", L"Bar"},
    {L"Statusanzeigen", L"Status items"}, {L"Menüs", L"Menus"}, {L"Info", L"About"},
    {L"Verhalten", L"Behavior"}, {L"Leiste anzeigen auf", L"Show bar on"}, {L"Zurücksetzen", L"Reset"},
    {L"Alle Einstellungen auf Standard zurücksetzen", L"Reset all settings to default"},
    {L"Alle Einstellungen auf Standard zurücksetzen?", L"Reset all settings to default?"},
    {L"Farben", L"Colors"}, {L"Design", L"Theme"}, {L"Schriftfarbe der Leiste", L"Bar text color"},
    {L"Hintergrundfarbe", L"Background color"}, {L"Textfarbe", L"Text color"}, {L"Höhe", L"Height"},
    {L"Deckkraft", L"Opacity"}, {L"Skalierung (Leiste und Menüs)", L"Scaling (bar and menus)"}, {L"Schriftstärke", L"Font weight"},
    {L"Dünn", L"Light"}, {L"Normal", L"Regular"}, {L"Mittel", L"Medium"}, {L"Halbfett", L"Semibold"}, {L"Fett", L"Bold"}, {L"Effekt", L"Effect"},
    {L"Blur-Stärke (nur „Blur einstellbar“)", L"Blur strength (only “Adjustable blur”)"},
    {L"Links", L"Left"}, {L"Logo anzeigen", L"Show logo"}, {L"Logo", L"Logo"},
    {L"Eigenes Logo (Bilddatei)", L"Custom logo (image file)"}, {L"Rechts", L"Right"}, {L"Symbol-Stil", L"Icon style"},
    {L"Abstand zwischen den Symbolen", L"Spacing between icons"},
    {L"Reihenfolge der Symbole (links → rechts)", L"Icon order (left → right)"}, {L"Anzeigen", L"Show"},
    {L"Kalenderwoche anzeigen", L"Show calendar week"},
    {L"Beispiele:  dd.MM.yyyy  ·  ddd d. MMM  ·  dddd, d. MMMM\nHH:mm  ·  HH:mm:ss  ·  h:mm tt   (Datum leer = nur Uhrzeit)",
     L"Examples:  MM/dd/yyyy  ·  ddd d MMM  ·  dddd, MMMM d\nHH:mm  ·  HH:mm:ss  ·  h:mm tt   (empty date = time only)"},
    {L"Aussehen", L"Look"}, {L"Kacheln", L"Tiles"}, {L"Hervorhebung", L"Highlight"}, {L"Farbe", L"Color"},
    {L"Eigene Farbe", L"Custom color"}, {L"Intensität", L"Intensity"},
    {L"macOS-artige Menüleiste für Windows 11\nAlle Änderungen werden sofort übernommen und gespeichert.",
     L"macOS-style menu bar for Windows 11\nAll changes are applied and saved immediately."},
    {L"Rechtsklick auf die Leiste öffnet dieses Fenster.\nMausrad über dem Lautsprecher ändert die Lautstärke, Mittelklick schaltet stumm.",
     L"Right-click the bar to open this window.\nScroll over the speaker to change the volume, middle-click to mute."},
    {L"Allen Monitoren", L"All monitors"}, {L"Hauptmonitor", L"Main monitor"},
    {L"Monitor %d  (%d × %d)%s", L"Display %d  (%d × %d)%s"}, {L" – Haupt", L" – main"},
    {L"Eigene Farben", L"Custom colors"}, {L"Hell", L"Light"}, {L"Dunkel", L"Dark"}, {L"Wie Windows", L"Like Windows"},
    {L"Automatisch", L"Automatic"}, {L"Weiß", L"White"}, {L"Schwarz", L"Black"}, {L"Kein Effekt", L"No effect"},
    {L"Unschärfe (Blur)", L"Blur"}, {L"Acryl", L"Acrylic"}, {L"Blur einstellbar", L"Adjustable blur"},
    {L"Kreis", L"Circle"}, {L"Eigenes Bild", L"Custom image"}, {L"macOS-Stil", L"macOS style"},
    {L"Windows-Akzentfarbe", L"Windows accent color"}, {L"Aus", L"Off"}, {L"Ein", L"On"},
    {L"▲  Nach oben", L"▲  Move up"}, {L"▼  Nach unten", L"▼  Move down"}, {L"Standard", L"Default"},
    // Menüs
    {L"Klick: öffnen  ·  Rechtsklick: Menü", L"Click: open  ·  Right-click: menu"},
    {L"Über diesen PC", L"About this PC"}, {L"Systemeinstellungen…", L"System settings…"},
    {L"Menüleiste einstellen…", L"Menu bar settings…"}, {L"Task-Manager", L"Task Manager"},
    {L"Energie sparen", L"Sleep"}, {L"Neu starten…", L"Restart…"}, {L"Neu starten", L"Restart"},
    {L"Möchtest du den Computer jetzt neu starten?", L"Do you want to restart the computer now?"},
    {L"Herunterfahren…", L"Shut down…"}, {L"Herunterfahren", L"Shut down"},
    {L"Möchtest du den Computer jetzt herunterfahren?", L"Do you want to shut down the computer now?"},
    {L"Bildschirm sperren", L"Lock screen"}, {L"Abmelden…", L"Sign out…"}, {L"Abmelden", L"Sign out"},
    {L"Möchtest du dich jetzt abmelden?", L"Do you want to sign out now?"},
    {L"Datei-Explorer öffnen", L"Open File Explorer"}, {L"Alle Fenster anzeigen", L"Show all windows"},
    {L"Minimieren", L"Minimize"}, {L"Wiederherstellen", L"Restore"}, {L"Maximieren", L"Maximize"},
    {L"Immer im Vordergrund", L"Always on top"}, {L"Dateispeicherort öffnen", L"Open file location"},
    {L"Fenster schließen", L"Close window"}, {L"Sofort beenden…", L"Force quit…"}, {L"Sofort beenden", L"Force quit"},
    {L"„%s“ sofort beenden?\nNicht gespeicherte Daten gehen verloren.", L"Force quit “%s”?\nUnsaved data will be lost."},
    {L"Benachrichtigungen", L"Notifications"}, {L"Datum und Uhrzeit…", L"Date & time…"},
    {L"Kein Akku gefunden", L"No battery found"}, {L"Voll geladen", L"Fully charged"}, {L"Wird geladen", L"Charging"},
    {L"Akkubetrieb", L"On battery"}, {L"Restlaufzeit", L"Time remaining"}, {L"Energiequelle", L"Power source"},
    {L"Netzteil", L"Power adapter"}, {L"Energiesparmodus", L"Energy saver"}, {L"Energiesparmodus…", L"Energy saver…"},
    {L"Netzbetrieb und Energiesparen…", L"Power & sleep…"}, {L"Ton", L"Sound"}, {L"Kein Ausgabegerät", L"No output device"},
    {L"Stumm", L"Mute"}, {L"Ausgabegerät", L"Output device"}, {L"Lautstärkemixer…", L"Volume mixer…"},
    {L"Soundeinstellungen…", L"Sound settings…"}, {L"Verbunden", L"Connected"}, {L"Trennen", L"Disconnect"},
    {L"Nicht verbunden", L"Not connected"}, {L"Andere Netzwerke", L"Other networks"}, {L"Netzwerk", L"Network"},
    {L"Status", L"Status"}, {L"Verbunden (LAN)", L"Connected (LAN)"}, {L"Weitere Netzwerke…", L"More networks…"},
    {L"Netzwerkeinstellungen…", L"Network settings…"}, {L"Eingabequelle", L"Input source"},
    {L"Nächste Eingabequelle", L"Next input source"}, {L"Spracheinstellungen…", L"Language settings…"},
    {L"Arbeitsspeicher", L"Memory"}, {L" von ", L" of "}, {L"Laufzeit", L"Uptime"}, {L"Laufwerke", L"Drives"},
    {L" frei von ", L" free of "}, {L"Ressourcenmonitor", L"Resource Monitor"},
    {L"Tray konnte nicht gelesen werden", L"Could not read the tray"}, {L"Keine Symbole vorhanden", L"No icons"},
    {L"Taskleisten-Einstellungen…", L"Taskbar settings…"}, {L" verbunden", L" connected"}, {L"Medien", L"Media"},
    {L"Wiedergabe steuern", L"Playback"}, {L"Sperren", L"Lock"}, {L"Mitteilungen", L"Notifications"},
    {L"Akku (lädt)", L"Battery (charging)"}, {L"Getrennt", L"Disconnected"}, {L"Einstellungen", L"Settings"},
    {L"Windows-Schnelleinstellungen", L"Windows Quick Settings"}, {L"Unbekanntes Gerät", L"Unknown device"},
    {L"Bluetooth ist aus oder nicht vorhanden", L"Bluetooth is off or unavailable"},
    {L"Keine gekoppelten Geräte", L"No paired devices"}, {L"Gekoppelt", L"Paired"},
    {L"Gerät hinzufügen…", L"Add device…"}, {L"Bluetooth-Einstellungen…", L"Bluetooth settings…"},
    {L"Audiogerät", L"Audio device"},
    // App-Menüs
    {L"Datei", L"File"}, {L"Bearbeiten", L"Edit"}, {L"Ansicht", L"View"}, {L"Gehe zu", L"Go"},
    {L"Fenster", L"Window"}, {L"Hilfe", L"Help"}, {L"Zurück", L"Back"}, {L"Vorwärts", L"Forward"},
    {L"Übergeordneter Ordner", L"Enclosing folder"}, {L"Startseite", L"Home"}, {L"Dokumente", L"Documents"},
    {L"Bilder", L"Pictures"}, {L"Musik", L"Music"}, {L"Dieser PC", L"This PC"}, {L"Papierkorb", L"Recycle Bin"},
    {L"Gehe zu Ordner…", L"Go to folder…"}, {L"Neues Fenster", L"New window"}, {L"Neuer Ordner", L"New folder"},
    {L"Öffnen", L"Open"}, {L"Eigenschaften", L"Properties"}, {L"Papierkorb leeren…", L"Empty Recycle Bin…"},
    {L"Rückgängig", L"Undo"}, {L"Wiederholen", L"Redo"}, {L"Ausschneiden", L"Cut"}, {L"Kopieren", L"Copy"},
    {L"Einfügen", L"Paste"}, {L"Alles auswählen", L"Select all"}, {L"Umbenennen", L"Rename"}, {L"Löschen", L"Delete"},
    {L"Suchen…", L"Find…"}, {L"Große Symbole", L"Large icons"}, {L"Mittelgroße Symbole", L"Medium icons"},
    {L"Kleine Symbole", L"Small icons"}, {L"Liste", L"List"}, {L"Vorschaufenster", L"Preview pane"},
    {L"Detailbereich", L"Details pane"}, {L"Aktualisieren", L"Refresh"}, {L"Explorer-Fenster", L"Explorer windows"},
    {L"Desktop anzeigen", L"Show desktop"}, {L"Windows-Hilfe", L"Windows help"}, {L"(leer)", L"(empty)"},
    {L"Neu", L"New"}, {L"Öffnen…", L"Open…"}, {L"Schließen", L"Close"}, {L"Speichern", L"Save"},
    {L"Speichern unter…", L"Save as…"}, {L"Drucken…", L"Print…"}, {L"Beenden", L"Quit"}, {L"Ersetzen…", L"Replace…"},
    {L"Vergrößern", L"Zoom in"}, {L"Verkleinern", L"Zoom out"}, {L"Originalgröße", L"Actual size"},
    {L"Vollbild", L"Full screen"}, {L"Links andocken", L"Snap left"}, {L"Rechts andocken", L"Snap right"},
    {L"Online-Hilfe suchen", L"Search online help"}, {L"Tastenkombinationen", L"Keyboard shortcuts"},
    {L"Über Windows", L"About Windows"}, {L"Windows-Einstellungen", L"Windows settings"},
    {L" Hilfe", L" help"}, {L" Tastenkombinationen", L" keyboard shortcuts"},
    // Zeitangaben
    {L"%d Tage, %d Std.", L"%d days, %d h"}, {L"%d Std., %d Min.", L"%d h, %d min"}, {L"%d:%02d Std.", L"%d:%02d h"},
};

const wchar_t* T(const wchar_t* de) {
    if (g_de || !de) return de;
    static std::unordered_map<std::wstring, const wchar_t*> map;
    if (map.empty())
        for (auto& t : kTr) map[t.de] = t.en;
    auto it = map.find(de);
    return it != map.end() ? it->second : de;
}

std::wstring T(const std::wstring& de) {
    if (g_de || de.empty()) return de;
    const wchar_t* r = T(de.c_str());
    if (r != de.c_str()) return r;
    // Tastenkürzel übersetzen (Strg+Umschalt+S → Ctrl+Shift+S …)
    std::wstring s = de;
    static const TrEntry keys[] = {{L"Strg", L"Ctrl"}, {L"Umschalt", L"Shift"}, {L"Leertaste", L"Space"},
                                   {L"Eingabe", L"Enter"}, {L"Entf", L"Del"}};
    for (auto& k : keys) {
        size_t pos = 0, n = wcslen(k.de);
        while ((pos = s.find(k.de, pos)) != std::wstring::npos) {
            s.replace(pos, n, k.en);
            pos += wcslen(k.en);
        }
    }
    return s;
}


#define WM_APPBAR_CB   (WM_APP + 1)
#define TIMER_TICK     1
#define TIMER_TRAY     2
#define TIMER_HOVER    3
#define TIMER_HOVERCLOSE 4
#define TIMER_SYNC     5
#define TIMER_KEYS     6

static const wchar_t* kClassName = L"WhTopMenuBar";
static const wchar_t* kSettingsClass = L"WhTopMenuBarSettings";
static const wchar_t* kCtrlClass = L"WhTopMenuBarCtrl";

static const GUID kCLSID_MMDeviceEnumerator =
    {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID kIID_IMMDeviceEnumerator =
    {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID kIID_IAudioEndpointVolume =
    {0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}};
static const GUID kCLSID_NetworkListManager =
    {0xDCB00C01, 0x570F, 0x4A9B, {0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B}};
static const GUID kIID_INetworkListManager =
    {0xDCB00000, 0x570F, 0x4A9B, {0x8D, 0x69, 0x19, 0x9F, 0xDB, 0xA5, 0x72, 0x3B}};

static const GUID kCLSID_WICImagingFactory =
    {0xcacaf262, 0x9370, 0x4615, {0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a}};
static const GUID kIID_IWICImagingFactory =
    {0xec5ec8a9, 0xc395, 0x4314, {0x9c, 0x77, 0x54, 0xd7, 0xa9, 0x35, 0xff, 0x70}};
static const GUID kFmt32bppPBGRA =
    {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x10}};

enum Action { ACT_NONE, ACT_LOGO, ACT_APP, ACT_CLOCK, ACT_BATTERY, ACT_VOLUME, ACT_WIFI, ACT_TRAY, ACT_KBD, ACT_SYSTEM,
              ACT_SEARCH, ACT_CONTROL, ACT_BLUETOOTH, ACT_APPMENU };

// Module der rechten Seite (Reihenfolge einstellbar)
enum ModId { M_UPTIME, M_DISK, M_CPU, M_RAM, M_NET, M_KBD, M_TRAY, M_BT, M_WIFI, M_VOLUME, M_BATTERY,
             M_SEARCH, M_CONTROL, M_WEEK, M_CLOCK, M_COUNT };
static const wchar_t* kModNames[M_COUNT] = {L"Uptime", L"Speicherplatz", L"CPU", L"RAM", L"Netzwerk-Speed",
    L"Tastaturlayout", L"Tray-Symbole", L"Bluetooth", L"WLAN", L"Lautstärke", L"Akku", L"Suche",
    L"Kontrollzentrum", L"Kalenderwoche", L"Uhr"};
std::vector<int> g_order;  // links → rechts
enum Effect { FX_NONE = 0, FX_BLUR = 1, FX_ACRYLIC = 2, FX_CUSTOM = 3 };
enum Theme { THEME_CUSTOM = 0, THEME_LIGHT = 1, THEME_DARK = 2, THEME_SYSTEM = 3 };

// ======================================================================
// Einstellungen
// ======================================================================
struct Settings {
    int height;
    COLORREF bg, fg;              // wirksame Farben (aus dem Design)
    COLORREF customBg, customFg;  // eigene Farben
    int theme;
    int barText;  // 0 = automatisch, 1 = weiß, 2 = schwarz
    int highlightMode;  // 0 = Windows-Akzentfarbe, 1 = eigene Farbe, 2 = aus
    COLORREF highlightColor;
    int highlightOpacity;  // 0..100 %
    int cardOpacity;       // Deckkraft der Kacheln/Karten in den Menüs, 0..100 %
    int opacity;   // 10..100 %
    int effect;    // Effect
    int blurStrength;  // 0..40 (nur FX_CUSTOM)
    int menuOpacity;   // 0..100 %
    int menuEffect;    // FX_NONE / FX_BLUR / FX_ACRYLIC
    bool trayButton;
    bool customMenus;
    bool search, controlCenter, bluetooth, appMenus;
    int iconGap;          // Abstand zwischen den Symbolen (DIP)
    int scale;            // Skalierung von Leiste und Menüs in %
    int fontWeight;       // 0 = automatisch, 1 = dünn … 5 = fett
    std::wstring order;   // Reihenfolge der Module, z. B. "0,1,2,..."
    bool menuOnHover;
    bool showLogo, showAppName, showClock, showWeek;
    std::wstring dateFormat, timeFormat;
    bool wifi, volume, battery, batteryPercent, batteryTime;
    bool keyboard, netSpeed, cpu, ram, disk, uptime;
    std::wstring diskDrive;
    std::wstring logoPath;
    int style;     // 0 = Windows 11, 1 = macOS-Stil
    int logoMode;  // 0 = Windows-Logo, 1 = Kreis, 2 = eigenes Bild
    std::wstring monitor;  // "all", "primary" oder Gerätename (\\.\DISPLAY2)
} g_s;

void SetDefaults(Settings& s);

// System-DPI × eigene Skalierung (für Leiste und Menüs)
UINT ScaledDpi(HWND h) {
    return (UINT)MulDiv((int)GetDpiForWindow(h), g_s.scale, 100);
}

void SetDefaults(Settings& s) {
    s.height = 32;
    s.bg = s.customBg = RGB(243, 243, 243);
    s.fg = s.customFg = RGB(255, 255, 255);
    s.theme = THEME_DARK;
    s.barText = 1;
    s.highlightMode = 1;
    s.highlightColor = RGB(0, 0, 0);
    s.highlightOpacity = 5;
    s.cardOpacity = 15;
    s.opacity = 0;
    s.effect = FX_NONE;
    s.blurStrength = 12;
    s.menuOpacity = 21;
    s.menuEffect = FX_ACRYLIC;
    s.trayButton = true;
    s.customMenus = true;
    s.search = true;
    s.controlCenter = true;
    s.bluetooth = true;
    s.appMenus = true;
    s.iconGap = 22;
    s.scale = 88;
    s.fontWeight = 4;
    s.order = L"0,1,2,3,4,5,6,7,8,9,10,11,12,13,14";
    s.menuOnHover = false;
    s.showLogo = s.showAppName = s.showClock = true;
    s.showWeek = false;
    s.dateFormat = g_de ? L"ddd d. MMM" : L"ddd d MMM";
    s.timeFormat = L"HH:mm";
    s.wifi = s.volume = s.battery = s.batteryPercent = true;
    s.batteryTime = s.keyboard = s.netSpeed = s.cpu = s.ram = s.disk = s.uptime = false;
    s.diskDrive = L"C:";
    s.monitor = L"all";
    s.style = 1;
    // 1 = circle. A custom logo image (mode 2) can be picked in the bar's settings.
    s.logoMode = 1;
    s.logoPath = L"";
}

struct CheckDef { int id; const wchar_t* key; const wchar_t* label; bool Settings::*field; };
static const CheckDef kChecks[] = {
    {201, L"showLogo",       L"Windows-Logo",              &Settings::showLogo},
    {202, L"showAppName",    L"Name der aktiven App",      &Settings::showAppName},
    {203, L"showClock",      L"Uhr anzeigen",              &Settings::showClock},
    {204, L"showWeek",       L"Kalenderwoche",             &Settings::showWeek},
    {210, L"wifi",           L"WLAN / Netzwerk",           &Settings::wifi},
    {211, L"volume",         L"Lautstärke",                &Settings::volume},
    {212, L"battery",        L"Akku",                      &Settings::battery},
    {213, L"batteryPercent", L"Akku-Prozent",              &Settings::batteryPercent},
    {214, L"batteryTime",    L"Akku-Restzeit",             &Settings::batteryTime},
    {215, L"keyboard",       L"Tastaturlayout",            &Settings::keyboard},
    {216, L"netSpeed",       L"Netzwerk-Geschwindigkeit",  &Settings::netSpeed},
    {217, L"cpu",            L"CPU-Auslastung",            &Settings::cpu},
    {218, L"ram",            L"RAM-Auslastung",            &Settings::ram},
    {219, L"disk",           L"Freier Speicherplatz",      &Settings::disk},
    {220, L"uptime",         L"Uptime",                    &Settings::uptime},
    {221, L"trayButton",     L"Tray-Symbole (Menü)",       &Settings::trayButton},
    {222, L"customMenus",    L"Eigene Menüs beim Klicken", &Settings::customMenus},
    {223, L"menuOnHover",    L"Menüs schon beim Überfahren öffnen", &Settings::menuOnHover},
    {224, L"search",         L"Suche (Lupe)",              &Settings::search},
    {225, L"controlCenter",  L"Kontrollzentrum",           &Settings::controlCenter},
    {226, L"bluetooth",      L"Bluetooth",                 &Settings::bluetooth},
    {227, L"appMenus",       L"App-Menüs (Datei, Bearbeiten …)", &Settings::appMenus},
};

struct StrDef { int id; const wchar_t* key; const wchar_t* label; std::wstring Settings::*field; };
static const StrDef kStrs[] = {
    {301, L"dateFormat", L"Datumsformat", &Settings::dateFormat},
    {302, L"timeFormat", L"Zeitformat",   &Settings::timeFormat},
    {303, L"diskDrive",  L"Laufwerk",     &Settings::diskDrive},
    {304, L"logoPath",   L"Bilddatei",    &Settings::logoPath},
};

const CheckDef* FindCheck(int id) {
    for (auto& c : kChecks) if (c.id == id) return &c;
    return nullptr;
}
const StrDef* FindStr(int id) {
    for (auto& s : kStrs) if (s.id == id) return &s;
    return nullptr;
}

bool WindowsUsesLightTheme() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &v, &sz) != ERROR_SUCCESS)
        return false;
    return v != 0;
}

// Wirksame Farben aus dem gewählten Design bestimmen
void ResolveTheme() {
    int t = g_s.theme;
    if (t == THEME_SYSTEM) t = WindowsUsesLightTheme() ? THEME_LIGHT : THEME_DARK;
    if (t == THEME_LIGHT) {
        g_s.bg = RGB(243, 243, 243);
        g_s.fg = RGB(28, 28, 30);
    } else if (t == THEME_DARK) {
        g_s.bg = RGB(30, 30, 30);
        g_s.fg = RGB(255, 255, 255);
    } else {
        g_s.bg = g_s.customBg;
        g_s.fg = g_s.customFg;
    }
    // Schriftfarbe der Leiste fest vorgeben
    if (g_s.barText == 1) g_s.fg = RGB(255, 255, 255);
    else if (g_s.barText == 2) g_s.fg = RGB(0, 0, 0);
}

// Akzentfarbe von Windows (Einstellungen → Personalisierung → Farben)
COLORREF WindowsAccent() {
    DWORD v = 0, sz = sizeof(v);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor",
                     RRF_RT_REG_DWORD, nullptr, &v, &sz) == ERROR_SUCCESS)
        return RGB(v & 255, (v >> 8) & 255, (v >> 16) & 255);  // gespeichert als 0xAABBGGRR
    DWORD col = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&col, &opaque)))
        return RGB((col >> 16) & 255, (col >> 8) & 255, col & 255);
    return RGB(0, 103, 192);
}

COLORREF HighlightColor() {
    return g_s.highlightMode == 1 ? g_s.highlightColor : WindowsAccent();
}

bool HighlightOn() { return g_s.highlightMode != 2; }

// gut lesbare Textfarbe auf einer Hintergrundfarbe
COLORREF TextOn(COLORREF bg) {
    int lum = (GetRValue(bg) * 299 + GetGValue(bg) * 587 + GetBValue(bg) * 114) / 1000;
    return lum > 160 ? RGB(0, 0, 0) : RGB(255, 255, 255);
}

void Normalize() {
    if (g_s.height < 16) g_s.height = 16;
    if (g_s.height > 100) g_s.height = 100;
    if (g_s.opacity < 0) g_s.opacity = 0;
    if (g_s.opacity > 100) g_s.opacity = 100;
    if (g_s.effect < 0 || g_s.effect > 3) g_s.effect = FX_NONE;
    if (g_s.blurStrength < 0) g_s.blurStrength = 0;
    if (g_s.blurStrength > 40) g_s.blurStrength = 40;
    if (g_s.menuOpacity < 0) g_s.menuOpacity = 0;
    if (g_s.menuOpacity > 100) g_s.menuOpacity = 100;
    if (g_s.menuEffect < 0 || g_s.menuEffect > 2) g_s.menuEffect = FX_ACRYLIC;
    if (g_s.theme < 0 || g_s.theme > 3) g_s.theme = THEME_SYSTEM;
    if (g_s.style < 0) g_s.style = 0;
    if (g_s.style > 1) g_s.style = 1;  // frühere Nerd-Font-/Phosphor-Stile → macOS-Stil
    if (g_s.iconGap < 4) g_s.iconGap = 4;
    if (g_s.iconGap > 48) g_s.iconGap = 48;
    if (g_s.scale < 50) g_s.scale = 50;
    if (g_s.scale > 250) g_s.scale = 250;
    if (g_s.fontWeight < 0 || g_s.fontWeight > 5) g_s.fontWeight = 0;
    {
        // Reihenfolge lesen, Ungültiges entfernen, fehlende Module an ihrer Standardstelle ergänzen
        std::vector<int> o;
        int cur = -1;
        for (wchar_t ch : g_s.order + L",") {
            if (ch >= L'0' && ch <= L'9') cur = (cur < 0 ? 0 : cur * 10) + (ch - L'0');
            else {
                if (cur >= 0 && cur < M_COUNT && std::find(o.begin(), o.end(), cur) == o.end()) o.push_back(cur);
                cur = -1;
            }
        }
        for (int id = 0; id < M_COUNT; id++) {
            if (std::find(o.begin(), o.end(), id) != o.end()) continue;
            auto it = id > 0 ? std::find(o.begin(), o.end(), id - 1) : o.end();
            o.insert(it == o.end() ? (id == 0 ? o.begin() : o.end()) : it + 1, id);
        }
        g_order = o;
        std::wstring str;
        for (size_t i = 0; i < o.size(); i++) str += (i ? L"," : L"") + std::to_wstring(o[i]);
        g_s.order = str;
    }
    if (g_s.logoMode < 0 || g_s.logoMode > 2) g_s.logoMode = 0;
    if (g_s.barText < 0 || g_s.barText > 2) g_s.barText = 0;
    if (g_s.highlightMode < 0 || g_s.highlightMode > 2) g_s.highlightMode = 0;
    if (g_s.highlightOpacity < 5) g_s.highlightOpacity = 5;
    if (g_s.highlightOpacity > 100) g_s.highlightOpacity = 100;
    if (g_s.cardOpacity < 0) g_s.cardOpacity = 0;
    if (g_s.cardOpacity > 100) g_s.cardOpacity = 100;
    ResolveTheme();
}

void LoadSettings() {
    SetDefaults(g_s);
    if (!Wh_GetIntValue(L"saved", 0)) {
        ResolveTheme();
        return;
    }
    g_s.height  = Wh_GetIntValue(L"height", g_s.height);
    g_s.customBg = (COLORREF)Wh_GetIntValue(L"bg", (int)g_s.customBg);
    g_s.customFg = (COLORREF)Wh_GetIntValue(L"fg", (int)g_s.customFg);
    g_s.theme = Wh_GetIntValue(L"theme", THEME_CUSTOM);
    g_s.barText = Wh_GetIntValue(L"barText", 0);
    g_s.highlightMode = Wh_GetIntValue(L"highlightMode", 0);
    g_s.highlightColor = (COLORREF)Wh_GetIntValue(L"highlightColor", (int)g_s.highlightColor);
    g_s.highlightOpacity = Wh_GetIntValue(L"highlightOpacity", g_s.highlightOpacity);
    g_s.cardOpacity = Wh_GetIntValue(L"cardOpacity", g_s.cardOpacity);
    g_s.opacity = Wh_GetIntValue(L"opacity", g_s.opacity);
    g_s.effect  = Wh_GetIntValue(L"effect", g_s.effect);
    g_s.blurStrength = Wh_GetIntValue(L"blurStrength", g_s.blurStrength);
    g_s.menuOpacity = Wh_GetIntValue(L"menuOpacity", g_s.menuOpacity);
    g_s.menuEffect = Wh_GetIntValue(L"menuEffect", g_s.menuEffect);
    for (auto& c : kChecks)
        g_s.*(c.field) = Wh_GetIntValue(c.key, g_s.*(c.field) ? 1 : 0) != 0;
    {
        wchar_t mb[64] = L"";
        Wh_GetStringValue(L"monitor", mb, 64);
        if (mb[0]) g_s.monitor = mb;
        wchar_t ob[256] = L"";
        Wh_GetStringValue(L"order", ob, 256);
        g_s.order = ob;
        g_s.iconGap = Wh_GetIntValue(L"iconGap", g_s.iconGap);
        g_s.scale = Wh_GetIntValue(L"scale", g_s.scale);
        g_s.fontWeight = Wh_GetIntValue(L"fontWeight", g_s.fontWeight);
        g_s.style = Wh_GetIntValue(L"style", 0);
        g_s.logoMode = Wh_GetIntValue(L"logoMode", 0);
    }
    for (auto& s : kStrs) {
        wchar_t buf[256] = L"";
        Wh_GetStringValue(s.key, buf, 256);
        g_s.*(s.field) = buf;
    }
    Normalize();
}

void SaveSettings() {
    Wh_SetIntValue(L"height", g_s.height);
    Wh_SetIntValue(L"bg", (int)g_s.customBg);
    Wh_SetIntValue(L"fg", (int)g_s.customFg);
    Wh_SetIntValue(L"theme", g_s.theme);
    Wh_SetIntValue(L"barText", g_s.barText);
    Wh_SetIntValue(L"highlightMode", g_s.highlightMode);
    Wh_SetIntValue(L"highlightColor", (int)g_s.highlightColor);
    Wh_SetIntValue(L"highlightOpacity", g_s.highlightOpacity);
    Wh_SetIntValue(L"cardOpacity", g_s.cardOpacity);
    Wh_SetIntValue(L"opacity", g_s.opacity);
    Wh_SetIntValue(L"effect", g_s.effect);
    Wh_SetIntValue(L"blurStrength", g_s.blurStrength);
    Wh_SetIntValue(L"menuOpacity", g_s.menuOpacity);
    Wh_SetIntValue(L"menuEffect", g_s.menuEffect);
    for (auto& c : kChecks) Wh_SetIntValue(c.key, g_s.*(c.field) ? 1 : 0);
    for (auto& s : kStrs) Wh_SetStringValue(s.key, (g_s.*(s.field)).c_str());
    Wh_SetStringValue(L"monitor", g_s.monitor.c_str());
    Wh_SetStringValue(L"order", g_s.order.c_str());
    Wh_SetIntValue(L"iconGap", g_s.iconGap);
    Wh_SetIntValue(L"scale", g_s.scale);
    Wh_SetIntValue(L"fontWeight", g_s.fontWeight);
    Wh_SetIntValue(L"style", g_s.style);
    Wh_SetIntValue(L"logoMode", g_s.logoMode);
    Wh_SetIntValue(L"saved", 1);
}

std::wstring DriveName() {
    std::wstring d = g_s.diskDrive;
    if (d.empty()) d = L"C:";
    if (d.size() == 1) d += L":";
    return d;
}

// ======================================================================
// Globaler Zustand
// ======================================================================
struct Status {
    bool hasBattery = false;
    int battery = 0;
    bool charging = false;
    DWORD batteryTime = (DWORD)-1;
    bool wifi = false;
    int signal = 0;
    bool online = false;
    bool hasAudio = false;
    float volume = 0.f;
    bool muted = false;
    int cpu = 0;
    int ram = 0;
    double down = 0, up = 0;
    std::wstring kbd;
    bool diskValid = false;
    bool btRadio = false;
    int btConnected = 0;
    ULONGLONG diskFree = 0;
} g_st;

struct Hit { int l, r; Action act; int idx = -1; };

HANDLE g_thread = nullptr;
HANDLE g_stop = nullptr;
HWND g_hwnd = nullptr;
HWND g_settingsWnd = nullptr;
HWINEVENTHOOK g_hook = nullptr;
HFONT g_font = nullptr, g_fontBold = nullptr, g_iconFont = nullptr;
HFONT g_fontBig = nullptr, g_iconSmall = nullptr;
HFONT g_fontSmall = nullptr;  // z. B. Akku-Prozent
HWND g_popup = nullptr;
Action g_popupAct = ACT_NONE;

bool MenuOpen(Action a) { return g_popupAct == a; }
void PopupRefresh();
void ShowSettings();
HANDLE g_wlan = nullptr;
IMMDeviceEnumerator* g_audioEnum = nullptr;
INetworkListManager* g_nlm = nullptr;

std::wstring g_appName;
// App-Menüs neben dem App-Namen (Datei, Bearbeiten, …)
std::vector<std::wstring> g_appMenuTitles;
bool g_appMenuNative = false;  // true = echte Menüleiste der App
bool g_appMenuExplorer = false;  // Desktop / Explorer-Fenster (wie Finder)
int g_appMenuNativeCount = 0;   // Anzahl echter Menüs (danach folgt "Gehe zu")
HWND g_appMenuHwnd = nullptr;
int g_menuIdx = -1;            // welches App-Menü gerade offen ist
// Zwischenspeicher für den eigenen Blur (pro Leiste)
struct BlurCache {
    std::vector<DWORD> wall;  // oberer Streifen des Hintergrundbilds (unverwischt)
    int wallW = 0, wallH = 0;
    bool wallDirty = true;
    std::vector<DWORD> blur;  // verwischter Hintergrund in Leistengröße
    int blurW = 0, blurH = 0, blurR = -1;
};

// eine Leiste pro Monitor
struct BarWin {
    HWND hwnd = nullptr;
    std::wstring device;
    std::vector<Hit> hits;
    RECT startRect{};
    int statusLeft = 0;
    BlurCache blur;
};
std::vector<BarWin*> g_bars;
BarWin* g_cur = nullptr;    // gerade gezeichnete Leiste
HWND g_popupBar = nullptr;  // Leiste, zu der das offene Menü gehört
UINT g_fontDpi = 0;

BarWin* BarOf(HWND h) {
    for (auto b : g_bars)
        if (b->hwnd == h) return b;
    return nullptr;
}

void MarkWallDirty() {
    for (auto b : g_bars) b->blur.wallDirty = true;
}

struct MonInfo { std::wstring device; RECT rc; bool primary; };

BOOL CALLBACK EnumMonProc(HMONITOR m, HDC, LPRECT, LPARAM lp) {
    MONITORINFOEXW mi;
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(m, &mi))
        ((std::vector<MonInfo>*)lp)->push_back(
            {mi.szDevice, mi.rcMonitor, (mi.dwFlags & MONITORINFOF_PRIMARY) != 0});
    return TRUE;
}

// alle Monitore, von links nach rechts sortiert
std::vector<MonInfo> ListMonitors() {
    std::vector<MonInfo> v;
    EnumDisplayMonitors(nullptr, nullptr, EnumMonProc, (LPARAM)&v);
    std::sort(v.begin(), v.end(), [](const MonInfo& a, const MonInfo& b) {
        return a.rc.left != b.rc.left ? a.rc.left < b.rc.left : a.rc.top < b.rc.top;
    });
    return v;
}

bool MonitorRect(const std::wstring& dev, RECT& rc) {
    for (auto& m : ListMonitors())
        if (m.device == dev) { rc = m.rc; return true; }
    return false;
}

int MonitorHeightOf(HWND h) {
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), &mi);
    return mi.rcMonitor.bottom - mi.rcMonitor.top;
}

ULONGLONG g_lastIdle = 0, g_lastKernel = 0, g_lastUser = 0;
ULONG64 g_lastIn = 0, g_lastOut = 0;
ULONGLONG g_lastNetTick = 0;
int g_tickCount = 0;

// ======================================================================
// Helpers
// ======================================================================
std::wstring GetAppName(HWND w) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return L"";
    wchar_t path[MAX_PATH];
    DWORD n = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(p, 0, path, &n);
    CloseHandle(p);
    if (!ok) return L"";

    std::wstring name;
    DWORD dummy = 0, size = GetFileVersionInfoSizeW(path, &dummy);
    if (size) {
        std::vector<BYTE> buf(size);
        if (GetFileVersionInfoW(path, 0, size, buf.data())) {
            struct LANG { WORD lang, cp; }* tr = nullptr;
            UINT len = 0;
            if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation",
                               (void**)&tr, &len) && len >= sizeof(LANG)) {
                wchar_t q[80];
                swprintf(q, 80, L"\\StringFileInfo\\%04x%04x\\FileDescription",
                         tr->lang, tr->cp);
                wchar_t* desc = nullptr;
                UINT dl = 0;
                if (VerQueryValueW(buf.data(), q, (void**)&desc, &dl) && dl > 1)
                    name = desc;
            }
        }
    }
    if (name.empty()) {
        name = path;
        size_t pos = name.find_last_of(L'\\');
        if (pos != std::wstring::npos) name = name.substr(pos + 1);
        if (name.size() > 4 &&
            _wcsicmp(name.c_str() + name.size() - 4, L".exe") == 0)
            name.resize(name.size() - 4);
    }
    return name;
}

void PressCombo(std::initializer_list<WORD> keys) {
    std::vector<INPUT> in;
    for (WORD k : keys) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = k;
        in.push_back(i);
    }
    for (auto it = keys.end(); it != keys.begin();) {
        --it;
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = *it;
        i.ki.dwFlags = KEYEVENTF_KEYUP;
        in.push_back(i);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

COLORREF Blend(COLORREF a, COLORREF b, int pctA) {
    return RGB((GetRValue(a) * pctA + GetRValue(b) * (100 - pctA)) / 100,
               (GetGValue(a) * pctA + GetGValue(b) * (100 - pctA)) / 100,
               (GetBValue(a) * pctA + GetBValue(b) * (100 - pctA)) / 100);
}

void FillRound(HDC dc, RECT r, int rad, COLORREF col) {
    HBRUSH b = CreateSolidBrush(col);
    HPEN p = CreatePen(PS_SOLID, 1, col);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

// Halbtransparente Hervorhebungen: beim Zeichnen nur die Flächen merken,
// beim Zusammensetzen werden sie mit eigener Deckkraft eingemischt
struct HiRect { RECT r; int rad; };
std::vector<HiRect> g_hiRects;

void HiFill(RECT r, int rad) { g_hiRects.push_back({r, rad}); }

// Karten (halbtransparente Flächen in den Menüs, Kachel-Design)
std::vector<HiRect> g_cardRects;
void CardFill(RECT r, int rad) { g_cardRects.push_back({r, rad}); }

void AddRoundRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r);

// Maske (0..255) aus abgerundeten Flächen, geglättet
std::vector<BYTE> BuildMask(const std::vector<HiRect>& rects, int w, int h) {
    std::vector<BYTE> mask;
    if (rects.empty()) return mask;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screen);
    if (!bmp) return mask;
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(dc, bmp);
    RECT all{0, 0, w, h};
    FillRect(dc, &all, (HBRUSH)GetStockObject(BLACK_BRUSH));
    {
        Gdiplus::Graphics g(dc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
        for (auto& hr : rects) {
            Gdiplus::GraphicsPath path;
            float rad = (float)hr.rad / 2.0f;  // rad ist wie bei RoundRect ein Durchmesser
            AddRoundRect(path, (float)hr.r.left, (float)hr.r.top, (float)(hr.r.right - hr.r.left),
                         (float)(hr.r.bottom - hr.r.top), rad);
            g.FillPath(&white, &path);
        }
    }
    GdiFlush();
    mask.resize((size_t)w * h);
    DWORD* px = (DWORD*)bits;
    for (size_t i = 0; i < mask.size(); i++) mask[i] = (BYTE)(px[i] & 255);
    SelectObject(dc, old);
    DeleteDC(dc);
    DeleteObject(bmp);
    return mask;
}

std::vector<BYTE> BuildHiMask(int w, int h) { return BuildMask(g_hiRects, w, h); }

std::wstring Comma(std::wstring s) {
    if (!g_de) return s;  // Englisch: Dezimalpunkt
    for (auto& c : s) if (c == L'.') c = L',';
    return s;
}

std::wstring FmtRate(double bps) {
    wchar_t b[32];
    if (bps < 1024.0 * 1024.0) swprintf(b, 32, L"%.0f KB/s", bps / 1024.0);
    else                       swprintf(b, 32, L"%.1f MB/s", bps / (1024.0 * 1024.0));
    return Comma(b);
}

std::wstring FmtSize(ULONGLONG bytes) {
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    wchar_t b[32];
    if (gb >= 1000)    swprintf(b, 32, L"%.1f TB", gb / 1024.0);
    else if (gb >= 10) swprintf(b, 32, L"%.0f GB", gb);
    else               swprintf(b, 32, L"%.1f GB", gb);
    return Comma(b);
}

static int P(int y) { return (y + y / 4 - y / 100 + y / 400) % 7; }
static int WeeksInYear(int y) { return 52 + ((P(y) == 4 || P(y - 1) == 3) ? 1 : 0); }
int IsoWeek(const SYSTEMTIME& st) {
    static const int cum[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int y = st.wYear;
    bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    int yday = cum[st.wMonth - 1] + st.wDay - 1 + ((st.wMonth > 2 && leap) ? 1 : 0);
    int wday = (st.wDayOfWeek + 6) % 7;
    int w = (yday - wday + 10) / 7;
    if (w < 1) return WeeksInYear(y - 1);
    if (w > WeeksInYear(y)) return 1;
    return w;
}

HFONT CreateIconFont(int px) {
    const wchar_t* faces[] = {L"Segoe Fluent Icons", L"Segoe MDL2 Assets"};
    for (auto face : faces) {
        HFONT f = CreateFontW(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                              0, 0, ANTIALIASED_QUALITY, 0, face);
        HDC dc = GetDC(nullptr);
        HGDIOBJ old = SelectObject(dc, f);
        wchar_t got[LF_FACESIZE] = L"";
        GetTextFaceW(dc, LF_FACESIZE, got);
        SelectObject(dc, old);
        ReleaseDC(nullptr, dc);
        if (_wcsicmp(got, face) == 0) return f;
        DeleteObject(f);
    }
    return CreateFontW(-px, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                       0, 0, ANTIALIASED_QUALITY, 0, L"Segoe MDL2 Assets");
}

int g_fontStyle = -1;
int g_fontWeightFor = -1;

// Schriftstärke aus der Einstellung (automatisch: macOS-Stil etwas kräftiger)
int BarWeight() {
    static const int w[] = {0, FW_LIGHT, FW_NORMAL, FW_MEDIUM, FW_SEMIBOLD, FW_BOLD};
    if (g_s.fontWeight == 0) return g_s.style == 1 ? FW_MEDIUM : FW_NORMAL;
    return w[g_s.fontWeight];
}

void EnsureFonts(UINT dpi) {
    if (g_font && dpi == g_fontDpi && g_fontStyle == g_s.style && g_fontWeightFor == g_s.fontWeight) return;
    g_fontDpi = dpi;
    g_fontStyle = g_s.style;
    g_fontWeightFor = g_s.fontWeight;
    int wt = BarWeight();
    int wtBold = wt + 200 > FW_HEAVY ? FW_HEAVY : (wt + 200 < FW_SEMIBOLD ? FW_SEMIBOLD : wt + 200);
    if (g_font) DeleteObject(g_font);
    if (g_fontBold) DeleteObject(g_fontBold);
    if (g_iconFont) DeleteObject(g_iconFont);
    int px = -MulDiv(10, dpi, 72);
    // macOS-Stil: etwas kräftigere Schrift
    g_font = CreateFontW(px, 0, 0, 0, wt, 0, 0, 0, DEFAULT_CHARSET,
                         0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI Variable Text");
    g_fontBold = CreateFontW(px, 0, 0, 0, wtBold, 0, 0, 0, DEFAULT_CHARSET,
                             0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI Variable Text");
    g_iconFont = CreateIconFont(MulDiv(15, dpi, 96));
    if (g_fontBig) DeleteObject(g_fontBig);
    if (g_iconSmall) DeleteObject(g_iconSmall);
    g_fontBig = CreateFontW(-MulDiv(20, dpi, 72), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                            0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI Variable Display");
    g_iconSmall = CreateIconFont(MulDiv(13, dpi, 96));
    if (g_fontSmall) DeleteObject(g_fontSmall);
    g_fontSmall = CreateFontW(-MulDiv(17, dpi, 144), 0, 0, 0, wt, 0, 0, 0,
                              DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI Variable Text");
}

// ======================================================================
// Blur / Acryl
// ======================================================================
struct ACCENTPOLICY { int AccentState; int AccentFlags; DWORD GradientColor; int AnimationId; };
struct WINCOMPATTRDATA { int Attribute; PVOID Data; SIZE_T SizeOfData; };
typedef BOOL(WINAPI* SetWindowCompositionAttribute_t)(HWND, WINCOMPATTRDATA*);

void ApplyAccent(HWND h) {
    static auto pSWCA = (SetWindowCompositionAttribute_t)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    if (!pSWCA) return;
    ACCENTPOLICY ap{};
    if (g_s.effect == FX_BLUR) {
        ap.AccentState = 3;  // ACCENT_ENABLE_BLURBEHIND
    } else if (g_s.effect == FX_ACRYLIC) {
        ap.AccentState = 4;  // ACCENT_ENABLE_ACRYLICBLURBEHIND
        DWORD a = (DWORD)(g_s.opacity * 255 / 100);
        ap.GradientColor = (a << 24) | (GetBValue(g_s.bg) << 16) |
                           (GetGValue(g_s.bg) << 8) | GetRValue(g_s.bg);
    }
    WINCOMPATTRDATA d{19, &ap, sizeof(ap)};  // WCA_ACCENT_POLICY
    pSWCA(h, &d);
}

// ======================================================================
// Eigener Blur (einstellbare Stärke) – verwischt das Hintergrundbild
// ======================================================================

bool LoadWallpaperStrip(int monW, int monH, int stripH, std::vector<DWORD>& out) {
    wchar_t path[MAX_PATH] = L"";
    SystemParametersInfoW(SPI_GETDESKWALLPAPER, MAX_PATH, path, 0);
    if (!path[0]) return false;

    wchar_t styleBuf[16] = L"10";
    DWORD sz = sizeof(styleBuf);
    RegGetValueW(HKEY_CURRENT_USER, L"Control Panel\\Desktop", L"WallpaperStyle",
                 RRF_RT_REG_SZ, nullptr, styleBuf, &sz);
    int style = _wtoi(styleBuf);

    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(kCLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                kIID_IWICImagingFactory, (void**)&f)) || !f)
        return false;

    bool ok = false;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapSource* src = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* conv = nullptr;

    if (SUCCEEDED(f->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame))) {
        UINT iw = 0, ih = 0;
        frame->GetSize(&iw, &ih);
        src = frame;
        src->AddRef();

        // "Füllen" (Standard): mittig zuschneiden; "Strecken" (2): nicht zuschneiden
        if (style != 2 && iw && ih) {
            double s = (double)monW / iw > (double)monH / ih ? (double)monW / iw : (double)monH / ih;
            int cw = (int)(monW / s), ch = (int)(monH / s);
            if (cw > (int)iw) cw = iw;
            if (ch > (int)ih) ch = ih;
            WICRect cr{((int)iw - cw) / 2, ((int)ih - ch) / 2, cw, ch};
            IWICBitmapClipper* clip = nullptr;
            if (SUCCEEDED(f->CreateBitmapClipper(&clip)) && SUCCEEDED(clip->Initialize(src, &cr))) {
                src->Release();
                src = clip;
            } else if (clip) {
                clip->Release();
            }
        }

        if (SUCCEEDED(f->CreateBitmapScaler(&scaler)) &&
            SUCCEEDED(scaler->Initialize(src, monW, monH, WICBitmapInterpolationModeFant)) &&
            SUCCEEDED(f->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(scaler, kFmt32bppPBGRA, WICBitmapDitherTypeNone,
                                       nullptr, 0, WICBitmapPaletteTypeCustom))) {
            out.assign((size_t)monW * stripH, 0);
            WICRect r{0, 0, monW, stripH};
            ok = SUCCEEDED(conv->CopyPixels(&r, monW * 4, (UINT)(out.size() * 4), (BYTE*)out.data()));
        }
    }

    if (conv) conv->Release();
    if (scaler) scaler->Release();
    if (src) src->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    f->Release();
    return ok;
}

void BoxBlur(std::vector<DWORD>& img, int w, int h, int r) {
    if (r <= 0) return;
    int win = 2 * r + 1;
    std::vector<DWORD> line((w > h ? w : h));
    auto clampI = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
    // horizontal
    for (int y = 0; y < h; y++) {
        DWORD* row = &img[(size_t)y * w];
        int sr = 0, sg = 0, sb = 0;
        for (int i = -r; i <= r; i++) {
            DWORD p = row[clampI(i, 0, w - 1)];
            sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255;
        }
        for (int x = 0; x < w; x++) {
            line[x] = 0xFF000000 | ((sr / win) << 16) | ((sg / win) << 8) | (sb / win);
            DWORD po = row[clampI(x - r, 0, w - 1)], pi = row[clampI(x + r + 1, 0, w - 1)];
            sr += ((pi >> 16) & 255) - ((po >> 16) & 255);
            sg += ((pi >> 8) & 255) - ((po >> 8) & 255);
            sb += (pi & 255) - (po & 255);
        }
        for (int x = 0; x < w; x++) row[x] = line[x];
    }
    // vertikal
    for (int x = 0; x < w; x++) {
        int sr = 0, sg = 0, sb = 0;
        for (int i = -r; i <= r; i++) {
            DWORD p = img[(size_t)clampI(i, 0, h - 1) * w + x];
            sr += (p >> 16) & 255; sg += (p >> 8) & 255; sb += p & 255;
        }
        for (int y = 0; y < h; y++) {
            line[y] = 0xFF000000 | ((sr / win) << 16) | ((sg / win) << 8) | (sb / win);
            DWORD po = img[(size_t)clampI(y - r, 0, h - 1) * w + x];
            DWORD pi = img[(size_t)clampI(y + r + 1, 0, h - 1) * w + x];
            sr += ((pi >> 16) & 255) - ((po >> 16) & 255);
            sg += ((pi >> 8) & 255) - ((po >> 8) & 255);
            sb += (pi & 255) - (po & 255);
        }
        for (int y = 0; y < h; y++) img[(size_t)y * w + x] = line[y];
    }
}

// Sorgt dafür, dass c.blur zur aktuellen Leistengröße und Stärke passt
void EnsureBlur(BlurCache& c, int monH, int w, int h, int r) {
    int need = h + 3 * r + 2;
    if (need > monH) need = monH;

    if (c.wallDirty || c.wallW != w || c.wallH < need) {
        if (!LoadWallpaperStrip(w, monH, need, c.wall)) {
            COLORREF dc = GetSysColor(COLOR_DESKTOP);
            c.wall.assign((size_t)w * need,
                          0xFF000000 | (GetRValue(dc) << 16) | (GetGValue(dc) << 8) | GetBValue(dc));
        }
        c.wallW = w;
        c.wallH = need;
        c.wallDirty = false;
        c.blurR = -1;
    }
    if (c.blurR == r && c.blurW == w && c.blurH == h) return;

    std::vector<DWORD> tmp(c.wall);
    for (int pass = 0; pass < 3; pass++) BoxBlur(tmp, c.wallW, c.wallH, r);
    c.blur.assign(tmp.begin(), tmp.begin() + (size_t)w * h);
    c.blurW = w;
    c.blurH = h;
    c.blurR = r;
}

// ======================================================================
// Audio
// ======================================================================
IAudioEndpointVolume* GetEndpointVolume() {
    if (!g_audioEnum) return nullptr;
    IMMDevice* dev = nullptr;
    if (FAILED(g_audioEnum->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) || !dev)
        return nullptr;
    IAudioEndpointVolume* ep = nullptr;
    dev->Activate(kIID_IAudioEndpointVolume, CLSCTX_ALL, nullptr, (void**)&ep);
    dev->Release();
    return ep;
}

void ChangeVolume(float delta) {
    IAudioEndpointVolume* ep = GetEndpointVolume();
    if (!ep) return;
    float v = 0;
    ep->GetMasterVolumeLevelScalar(&v);
    v += delta;
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    ep->SetMasterVolumeLevelScalar(v, nullptr);
    if (delta > 0) ep->SetMute(FALSE, nullptr);
    ep->Release();
}

void ToggleMute() {
    IAudioEndpointVolume* ep = GetEndpointVolume();
    if (!ep) return;
    BOOL m = FALSE;
    ep->GetMute(&m);
    ep->SetMute(!m, nullptr);
    ep->Release();
}

// ======================================================================
// Status
// ======================================================================
static ULONGLONG FtToU(const FILETIME& f) {
    return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime;
}

void UpdateStatus() {
    g_tickCount++;

    if (g_s.battery || MenuOpen(ACT_BATTERY) || MenuOpen(ACT_CONTROL)) {
        SYSTEM_POWER_STATUS sps{};
        if (GetSystemPowerStatus(&sps)) {
            g_st.hasBattery = !(sps.BatteryFlag & 128) && sps.BatteryFlag != 255;
            g_st.battery = sps.BatteryLifePercent <= 100 ? sps.BatteryLifePercent : 0;
            g_st.charging = sps.ACLineStatus == 1;
            g_st.batteryTime = sps.BatteryLifeTime;
        }
    }

    // Bluetooth (alle ~2 s)
    if ((g_s.bluetooth || MenuOpen(ACT_BLUETOOTH) || MenuOpen(ACT_CONTROL)) && (g_tickCount % 4 == 1 || MenuOpen(ACT_BLUETOOTH))) {
        BLUETOOTH_FIND_RADIO_PARAMS rp{sizeof(rp)};
        HANDLE radio = nullptr;
        HBLUETOOTH_RADIO_FIND rf = BluetoothFindFirstRadio(&rp, &radio);
        g_st.btRadio = rf != nullptr;
        if (radio) CloseHandle(radio);
        if (rf) BluetoothFindRadioClose(rf);
        g_st.btConnected = 0;
        if (g_st.btRadio) {
            BLUETOOTH_DEVICE_SEARCH_PARAMS sp{sizeof(sp)};
            sp.fReturnConnected = TRUE;
            BLUETOOTH_DEVICE_INFO di{sizeof(di)};
            HBLUETOOTH_DEVICE_FIND df = BluetoothFindFirstDevice(&sp, &di);
            if (df) {
                do {
                    if (di.fConnected) g_st.btConnected++;
                    di.dwSize = sizeof(di);
                } while (BluetoothFindNextDevice(df, &di));
                BluetoothFindDeviceClose(df);
            }
        }
    }

    if (g_s.wifi || MenuOpen(ACT_WIFI) || MenuOpen(ACT_CONTROL)) {
        g_st.wifi = false;
        g_st.signal = 0;
        if (!g_wlan) {
            DWORD ver = 0;
            if (WlanOpenHandle(2, nullptr, &ver, &g_wlan) != ERROR_SUCCESS) g_wlan = nullptr;
        }
        if (g_wlan) {
            PWLAN_INTERFACE_INFO_LIST list = nullptr;
            if (WlanEnumInterfaces(g_wlan, nullptr, &list) == ERROR_SUCCESS && list) {
                for (DWORD i = 0; i < list->dwNumberOfItems; i++) {
                    auto& itf = list->InterfaceInfo[i];
                    if (itf.isState != wlan_interface_state_connected) continue;
                    DWORD sz = 0;
                    PWLAN_CONNECTION_ATTRIBUTES conn = nullptr;
                    if (WlanQueryInterface(g_wlan, &itf.InterfaceGuid,
                                           wlan_intf_opcode_current_connection, nullptr,
                                           &sz, (PVOID*)&conn, nullptr) == ERROR_SUCCESS && conn) {
                        g_st.wifi = true;
                        g_st.signal = (int)conn->wlanAssociationAttributes.wlanSignalQuality;
                        WlanFreeMemory(conn);
                        break;
                    }
                }
                WlanFreeMemory(list);
            }
        }
        g_st.online = false;
        if (g_nlm) {
            NLM_CONNECTIVITY c = NLM_CONNECTIVITY_DISCONNECTED;
            if (SUCCEEDED(g_nlm->GetConnectivity(&c))) {
                g_st.online = (c & (NLM_CONNECTIVITY_IPV4_INTERNET | NLM_CONNECTIVITY_IPV6_INTERNET |
                                    NLM_CONNECTIVITY_IPV4_LOCALNETWORK |
                                    NLM_CONNECTIVITY_IPV6_LOCALNETWORK)) != 0;
            }
        }
    }

    if (g_s.volume || MenuOpen(ACT_VOLUME) || MenuOpen(ACT_CONTROL)) {
        g_st.hasAudio = false;
        IAudioEndpointVolume* ep = GetEndpointVolume();
        if (ep) {
            BOOL m = FALSE;
            if (SUCCEEDED(ep->GetMasterVolumeLevelScalar(&g_st.volume)) &&
                SUCCEEDED(ep->GetMute(&m))) {
                g_st.hasAudio = true;
                g_st.muted = m != FALSE;
            }
            ep->Release();
        }
    }

    if (g_s.cpu || MenuOpen(ACT_SYSTEM)) {
        FILETIME fi, fk, fu;
        if (GetSystemTimes(&fi, &fk, &fu)) {
            ULONGLONG i = FtToU(fi), k = FtToU(fk), u = FtToU(fu);
            ULONGLONG di = i - g_lastIdle, dt = (k - g_lastKernel) + (u - g_lastUser);
            if (g_lastKernel && dt) g_st.cpu = (int)(100.0 * (1.0 - (double)di / dt) + 0.5);
            if (g_st.cpu < 0) g_st.cpu = 0;
            if (g_st.cpu > 100) g_st.cpu = 100;
            g_lastIdle = i; g_lastKernel = k; g_lastUser = u;
        }
    }

    if (g_s.ram || MenuOpen(ACT_SYSTEM)) {
        MEMORYSTATUSEX ms{sizeof(ms)};
        if (GlobalMemoryStatusEx(&ms)) g_st.ram = (int)ms.dwMemoryLoad;
    }

    if (g_s.netSpeed || MenuOpen(ACT_SYSTEM)) {
        PMIB_IF_TABLE2 tbl = nullptr;
        if (GetIfTable2(&tbl) == NO_ERROR && tbl) {
            ULONG64 in = 0, out = 0;
            for (ULONG i = 0; i < tbl->NumEntries; i++) {
                auto& r = tbl->Table[i];
                if (r.Type == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                if (!r.InterfaceAndOperStatusFlags.HardwareInterface) continue;
                if (r.InterfaceAndOperStatusFlags.FilterInterface) continue;
                if (r.OperStatus != IfOperStatusUp) continue;
                in += r.InOctets;
                out += r.OutOctets;
            }
            FreeMibTable(tbl);
            ULONGLONG now = GetTickCount64();
            if (g_lastNetTick && now > g_lastNetTick && in >= g_lastIn && out >= g_lastOut) {
                double sec = (now - g_lastNetTick) / 1000.0;
                g_st.down = (in - g_lastIn) / sec;
                g_st.up = (out - g_lastOut) / sec;
            }
            g_lastIn = in; g_lastOut = out; g_lastNetTick = now;
        }
    }

    if (g_s.keyboard) {
        HWND fg = GetForegroundWindow();
        DWORD tid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
        HKL hkl = GetKeyboardLayout(tid);
        LANGID lang = LOWORD((UINT_PTR)hkl);
        wchar_t loc[LOCALE_NAME_MAX_LENGTH] = L"";
        wchar_t iso[16] = L"";
        if (LCIDToLocaleName(MAKELCID(lang, SORT_DEFAULT), loc, LOCALE_NAME_MAX_LENGTH, 0) &&
            GetLocaleInfoEx(loc, LOCALE_SISO639LANGNAME, iso, 16)) {
            for (wchar_t* c = iso; *c; c++) *c = towupper(*c);
            g_st.kbd = iso;
        }
    }

    if (g_s.disk && (g_tickCount % 20 == 1 || !g_st.diskValid)) {
        ULARGE_INTEGER freeAvail{}, total{};
        std::wstring root = DriveName() + L"\\";
        g_st.diskValid = GetDiskFreeSpaceExW(root.c_str(), &freeAvail, &total, nullptr) != 0;
        g_st.diskFree = freeAvail.QuadPart;
    }
}

// ======================================================================
// AppBar
// ======================================================================
void PositionAppBar(HWND h) {
    BarWin* b = BarOf(h);
    RECT mr;
    if (!b || !MonitorRect(b->device, mr)) {
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
        mr = mi.rcMonitor;
    }
    int ht = MulDiv(g_s.height, ScaledDpi(h), 96);

    APPBARDATA abd{sizeof(abd)};
    abd.hWnd = h;
    abd.uEdge = ABE_TOP;
    abd.rc = mr;
    abd.rc.bottom = abd.rc.top + ht;
    SHAppBarMessage(ABM_QUERYPOS, &abd);
    abd.rc.bottom = abd.rc.top + ht;
    SHAppBarMessage(ABM_SETPOS, &abd);

    MoveWindow(h, abd.rc.left, abd.rc.top, abd.rc.right - abd.rc.left, ht, FALSE);
}

// ======================================================================
// Zeichnen
// ======================================================================
// ======================================================================
// Vektor-Icons für den macOS-Stil (eigene Zeichnungen, geglättet mit GDI+)
// ======================================================================
Gdiplus::Color GpCol(COLORREF c, int a = 255) {
    return Gdiplus::Color((BYTE)a, GetRValue(c), GetGValue(c), GetBValue(c));
}

void AddRoundRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = 2 * r;
    if (d > w) d = w;
    if (d > h) d = h;
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

void SetupGraphics(Gdiplus::Graphics& g) {
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
}

// Akku: liegendes, abgerundetes Rechteck mit Füllstand (Breite ≈ 25 * s)
void IconBattery(HDC dc, float x, float cy, float s, int pct, bool charging, COLORREF fg,
                 COLORREF fill, COLORREF bg) {
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    float w = 23 * s, h = 11.5f * s, y = cy - h / 2;
    Gdiplus::GraphicsPath body;
    AddRoundRect(body, x, y, w, h, 3.3f * s);
    Gdiplus::Pen outline(GpCol(fg, 115), 1.0f * s);
    g.DrawPath(&outline, &body);
    float in = 1.8f * s;
    float fw = (w - 2 * in) * (pct < 0 ? 0 : (pct > 100 ? 100 : pct)) / 100.f;
    if (fw > 0.5f) {
        Gdiplus::GraphicsPath lvl;
        AddRoundRect(lvl, x + in, y + in, fw, h - 2 * in, 1.9f * s);
        Gdiplus::SolidBrush fb(GpCol(fill));
        g.FillPath(&fb, &lvl);
    }
    Gdiplus::GraphicsPath nub;
    AddRoundRect(nub, x + w + 1.0f * s, cy - 2.1f * s, 1.7f * s, 4.2f * s, 0.8f * s);
    Gdiplus::SolidBrush nb(GpCol(fg, 115));
    g.FillPath(&nb, &nub);
    if (charging) {
        float cx = x + w / 2;
        Gdiplus::PointF bolt[] = {
            {cx + 1.0f * s, cy - 5.0f * s}, {cx - 3.0f * s, cy + 0.8f * s}, {cx - 0.3f * s, cy + 0.8f * s},
            {cx - 1.0f * s, cy + 5.0f * s}, {cx + 3.0f * s, cy - 0.8f * s}, {cx + 0.3f * s, cy - 0.8f * s}};
        Gdiplus::SolidBrush bb(GpCol(fg));
        Gdiplus::Pen bp(GpCol(bg), 1.0f * s);
        g.DrawPolygon(&bp, bolt, 6);
        g.FillPolygon(&bb, bolt, 6);
    }
}

// WLAN: Punkt mit drei Bögen darüber (Breite ≈ 19 * s), bars = 0..3
void IconWifi(HDC dc, float cx, float cy, float s, int bars, COLORREF fg) {
    // eigenes Design: kleiner Sektor unten, darüber zwei Bögen mit runden Enden
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    // mehr Abstand zwischen den Bögen und etwas höher → wirkt nicht gestaucht
    float apex = cy + 7.0f * s;
    {
        float r = 3.6f * s;
        Gdiplus::SolidBrush b(GpCol(fg, bars >= 1 ? 255 : 70));
        g.FillPie(&b, cx - r, apex - r, 2 * r, 2 * r, 224.0f, 92.0f);
    }
    const float radii[2] = {7.3f, 11.6f};
    for (int i = 0; i < 2; i++) {
        float r = radii[i] * s;
        Gdiplus::Pen p(GpCol(fg, bars >= i + 2 ? 255 : 70), 2.2f * s);
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&p, cx - r, apex - r, 2 * r, 2 * r, 228.0f, 84.0f);
    }
    if (bars == 0) {  // getrennt: Schrägstrich
        Gdiplus::Pen p(GpCol(fg), 1.5f * s);
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        g.DrawLine(&p, cx - 7.5f * s, cy - 5.5f * s, cx + 7.5f * s, cy + 7.0f * s);
    }
}

// LAN: drei verbundene Knoten (Breite ≈ 14 * s)
void IconEthernet(HDC dc, float x, float cy, float s, COLORREF fg) {
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    Gdiplus::Pen p(GpCol(fg), 1.3f * s);
    float tx = x + 7 * s, ty = cy - 4.2f * s, lx = x + 2.4f * s, rx = x + 11.6f * s, by = cy + 4.2f * s;
    g.DrawLine(&p, tx, ty, tx, cy);
    g.DrawLine(&p, lx, cy, rx, cy);
    g.DrawLine(&p, lx, cy, lx, by);
    g.DrawLine(&p, rx, cy, rx, by);
    Gdiplus::SolidBrush b(GpCol(fg));
    float sq = 4.2f * s;
    float pts[3][2] = {{tx, ty}, {lx, by}, {rx, by}};
    for (auto& c : pts) {
        Gdiplus::GraphicsPath box;
        AddRoundRect(box, c[0] - sq / 2, c[1] - sq / 2, sq, sq, 1.1f * s);
        g.FillPath(&b, &box);
    }
}

// Lautsprecher mit Schallwellen (Breite ≈ 19 * s), level = 0 (stumm) .. 3
void IconVolume(HDC dc, float x, float cy, float s, int level, COLORREF fg) {
    // eigenes Design: Lautsprecher mit weichen Ecken, Wellen mit runden Enden
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    Gdiplus::PointF sp[] = {{x + 0.6f * s, cy - 2.4f * s}, {x + 3.4f * s, cy - 2.4f * s}, {x + 7.0f * s, cy - 5.6f * s},
                            {x + 7.0f * s, cy + 5.6f * s}, {x + 3.4f * s, cy + 2.4f * s}, {x + 0.6f * s, cy + 2.4f * s}};
    Gdiplus::SolidBrush b(GpCol(fg));
    Gdiplus::Pen edge(GpCol(fg), 1.0f * s);
    edge.SetLineJoin(Gdiplus::LineJoinRound);
    g.FillPolygon(&b, sp, 6);
    g.DrawPolygon(&edge, sp, 6);
    if (level == 0) {
        Gdiplus::Pen p(GpCol(fg), 1.4f * s);
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        float ox = x + 10.6f * s;
        g.DrawLine(&p, ox, cy - 2.8f * s, ox + 5.6f * s, cy + 2.8f * s);
        g.DrawLine(&p, ox, cy + 2.8f * s, ox + 5.6f * s, cy - 2.8f * s);
        return;
    }
    float wx = x + 7.0f * s;
    for (int i = 0; i < 3; i++) {
        float r = (3.6f + 3.1f * i) * s;
        Gdiplus::Pen p(GpCol(fg, i < level ? 255 : 65), 1.45f * s);
        p.SetStartCap(Gdiplus::LineCapRound);
        p.SetEndCap(Gdiplus::LineCapRound);
        g.DrawArc(&p, wx - r, cy - r, 2 * r, 2 * r, -44.0f, 88.0f);
    }
}

// Lupe (Breite ≈ 15 * s)
void IconSearch(HDC dc, float x, float cy, float s, COLORREF fg) {
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    float r = 4.7f * s, ccx = x + 6.2f * s, ccy = cy - 1.2f * s;
    Gdiplus::Pen p(GpCol(fg), 1.5f * s);
    g.DrawEllipse(&p, ccx - r, ccy - r, 2 * r, 2 * r);
    Gdiplus::Pen h(GpCol(fg), 1.9f * s);
    h.SetStartCap(Gdiplus::LineCapRound);
    h.SetEndCap(Gdiplus::LineCapRound);
    float d = r * 0.7071f + 0.6f * s;
    g.DrawLine(&h, ccx + d, ccy + d, x + 13.0f * s, cy + 5.4f * s);
}

// zwei Schalter übereinander (Breite ≈ 15 * s)
void IconControl(HDC dc, float x, float cy, float s, COLORREF fg) {
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    float w = 13.5f * s, h = 5.6f * s, gap = 2.0f * s;
    for (int i = 0; i < 2; i++) {
        float y = cy - h - gap / 2 + i * (h + gap);
        Gdiplus::GraphicsPath pill;
        AddRoundRect(pill, x, y, w, h, h / 2);
        Gdiplus::Pen p(GpCol(fg), 1.2f * s);
        g.DrawPath(&p, &pill);
        float kd = h - 2.0f * s;
        float kx = i == 0 ? x + w - h + 1.0f * s : x + 1.0f * s;
        Gdiplus::SolidBrush b(GpCol(fg));
        g.FillEllipse(&b, kx, y + 1.0f * s, kd, kd);
    }
}

// kleiner Pfeil nach unten (Breite ≈ 10 * s)
void IconChevron(HDC dc, float x, float cy, float s, COLORREF fg) {
    Gdiplus::Graphics g(dc);
    SetupGraphics(g);
    Gdiplus::Pen p(GpCol(fg), 1.3f * s);
    p.SetStartCap(Gdiplus::LineCapRound);
    p.SetEndCap(Gdiplus::LineCapRound);
    p.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::PointF pts[] = {{x + 1 * s, cy - 2 * s}, {x + 5 * s, cy + 2 * s}, {x + 9 * s, cy - 2 * s}};
    g.DrawLines(&p, pts, 3);
}

// ---------- eigenes Logo (Bilddatei) ----------
// Das eigene Logo wird als Maske gespeichert und immer in der Schriftfarbe gezeichnet
std::vector<BYTE> g_logoMask;  // size * size, 0..255
int g_logoSize = 0;
std::wstring g_logoFor;
bool g_logoOk = false;

bool LoadLogoMask(const std::wstring& path, int size, std::vector<BYTE>& out) {
    if (path.empty() || size <= 0) return false;
    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(kCLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                kIID_IWICImagingFactory, (void**)&f)) || !f)
        return false;
    bool ok = false;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* conv = nullptr;
    std::vector<DWORD> px;
    int bw = 0, bh = 0;
    if (SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame))) {
        UINT iw = 0, ih = 0;
        frame->GetSize(&iw, &ih);
        if (iw && ih) {
            // erst auf max. 160 px bringen, dort Inhalt suchen
            const int big = 160;
            bw = big, bh = big;
            if (iw > ih) bh = (int)((double)big * ih / iw);
            else bw = (int)((double)big * iw / ih);
            if (bw < 1) bw = 1;
            if (bh < 1) bh = 1;
            if (SUCCEEDED(f->CreateBitmapScaler(&scaler)) &&
                SUCCEEDED(scaler->Initialize(frame, bw, bh, WICBitmapInterpolationModeFant)) &&
                SUCCEEDED(f->CreateFormatConverter(&conv)) &&
                SUCCEEDED(conv->Initialize(scaler, kFmt32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                           WICBitmapPaletteTypeCustom))) {
                px.resize((size_t)bw * bh);
                ok = SUCCEEDED(conv->CopyPixels(nullptr, bw * 4, (UINT)(px.size() * 4), (BYTE*)px.data()));
            }
        }
    }
    if (conv) conv->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (dec) dec->Release();
    f->Release();
    if (!ok) return false;

    // Maske: Transparenz – oder bei Bildern ohne Transparenz der Kontrast zum Hintergrund (Ecke)
    std::vector<int> m((size_t)bw * bh);
    bool hasAlpha = false;
    for (DWORD p : px)
        if ((p >> 24) < 250) { hasAlpha = true; break; }
    auto lum = [](DWORD p) { return (int)(((p >> 16) & 255) * 299 + ((p >> 8) & 255) * 587 + (p & 255) * 114) / 1000; };
    int l0 = lum(px[0]);
    for (size_t i = 0; i < px.size(); i++) {
        int v = hasAlpha ? (int)(px[i] >> 24) : std::abs(lum(px[i]) - l0) * 2;
        m[i] = v > 255 ? 255 : v;
    }
    // leeren Rand abschneiden
    int x0 = bw, y0 = bh, x1 = -1, y1 = -1;
    for (int y = 0; y < bh; y++)
        for (int x = 0; x < bw; x++)
            if (m[(size_t)y * bw + x] > 12) {
                if (x < x0) x0 = x;
                if (y < y0) y0 = y;
                if (x > x1) x1 = x;
                if (y > y1) y1 = y;
            }
    if (x1 < 0) return false;
    int cwSrc = x1 - x0 + 1, chSrc = y1 - y0 + 1;
    // ins Quadrat einpassen und mit Flächenmittel verkleinern
    int cw = size, ch = size;
    if (cwSrc > chSrc) ch = (int)std::lround((double)size * chSrc / cwSrc);
    else cw = (int)std::lround((double)size * cwSrc / chSrc);
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    int ox = (size - cw) / 2, oy = (size - ch) / 2;
    out.assign((size_t)size * size, 0);
    for (int ty = 0; ty < ch; ty++) {
        int sy0 = y0 + ty * chSrc / ch, sy1 = y0 + (ty + 1) * chSrc / ch;
        if (sy1 <= sy0) sy1 = sy0 + 1;
        for (int tx = 0; tx < cw; tx++) {
            int sx0 = x0 + tx * cwSrc / cw, sx1 = x0 + (tx + 1) * cwSrc / cw;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            int sum = 0, n = 0;
            for (int yy = sy0; yy < sy1 && yy < bh; yy++)
                for (int xx = sx0; xx < sx1 && xx < bw; xx++) {
                    sum += m[(size_t)yy * bw + xx];
                    n++;
                }
            out[(size_t)(oy + ty) * size + ox + tx] = (BYTE)(n ? sum / n : 0);
        }
    }
    return true;
}

bool EnsureLogo(int size) {
    if (g_logoSize == size && g_logoFor == g_s.logoPath) return g_logoOk;
    g_logoOk = LoadLogoMask(g_s.logoPath, size, g_logoMask);
    g_logoSize = size;
    g_logoFor = g_s.logoPath;
    return g_logoOk;
}

// Logo-Maske in einer Farbe zeichnen
void DrawLogoMask(HDC dc, int x, int y, int size, COLORREF col) {
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = size;
    bi.bmiHeader.biHeight = -size;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bmp || !bits) return;
    DWORD* p = (DWORD*)bits;
    for (size_t i = 0; i < (size_t)size * size; i++) {
        DWORD a = g_logoMask[i];
        p[i] = (a << 24) | ((GetRValue(col) * a / 255) << 16) | ((GetGValue(col) * a / 255) << 8) |
               (GetBValue(col) * a / 255);
    }
    HDC src = CreateCompatibleDC(dc);
    HGDIOBJ old = SelectObject(src, bmp);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    GdiAlphaBlend(dc, x, y, size, size, src, 0, 0, size, size, bf);
    SelectObject(src, old);
    DeleteDC(src);
    DeleteObject(bmp);
}

// Zeichnet den Inhalt der Leiste (ohne Hintergrund) auf eine Fläche in "clear"
void DrawContent(HDC mem, RECT rc, COLORREF clear) {
    UINT dpi = ScaledDpi(g_cur->hwnd);
    int pad = MulDiv(12, dpi, 96);
    int iconGap = MulDiv(g_s.iconGap, dpi, 96);
    COLORREF dim = Blend(g_s.fg, g_s.bg, 55);

    HBRUSH cb = CreateSolidBrush(clear);
    FillRect(mem, &rc, cb);
    DeleteObject(cb);
    SetBkMode(mem, TRANSPARENT);

    // Abstand von Zeilenoberkante zur optischen Mitte der Großbuchstaben/Ziffern
    auto capOffset = [&](HFONT f) -> float {
        SelectObject(mem, f);
        OUTLINETEXTMETRICW otm{};
        otm.otmSize = sizeof(otm);
        if (GetOutlineTextMetricsW(mem, sizeof(otm), &otm) && otm.otmsCapEmHeight)
            return otm.otmTextMetrics.tmAscent - otm.otmsCapEmHeight / 2.0f;
        TEXTMETRICW tm{};
        GetTextMetricsW(mem, &tm);
        return tm.tmAscent * 0.62f;
    };
    // optische Mitte einer vertikal zentrierten Textzeile in der Leiste
    auto textCenterY = [&](HFONT f) -> float {
        SelectObject(mem, f);
        TEXTMETRICW tm{};
        GetTextMetricsW(mem, &tm);
        return (rc.bottom - tm.tmHeight) / 2.0f + capOffset(f);
    };

    // Bereich des offenen Menüs hervorheben (Positionen vom letzten Zeichnen)
    if (g_popup && HighlightOn() && g_popupBar == g_cur->hwnd) {
        for (auto& hh : g_cur->hits) {
            if (hh.act != g_popupAct || (hh.act == ACT_APPMENU && hh.idx != g_menuIdx)) continue;
            int m = MulDiv(3, dpi, 96);
            RECT hr{hh.l, m, hh.r, rc.bottom - m};
            HiFill(hr, MulDiv(8, dpi, 96));
            break;
        }
    }

    // ---- Links: Logo ----
    int leftX = pad;
    g_cur->startRect = {};
    if (g_s.showLogo) {
        int logo = MulDiv(12, dpi, 96);
        bool img = false;
        if (g_s.logoMode == 2) {
            logo = MulDiv(15, dpi, 96);
            img = EnsureLogo(logo);
        }
        // Logo auf die optische Mitte des Textes ausrichten
        // Logo genau in der Leistenmitte (vorher etwas zu tief)
        float lc = rc.bottom / 2.0f - MulDiv(1, dpi, 96) * 0.5f;
        int ly = (int)std::lround(lc - logo / 2.0f);
        if (img) {
            // eigenes Bild – immer in der Schriftfarbe
            DrawLogoMask(mem, leftX, ly, logo, g_s.fg);
        } else if (g_s.logoMode == 1 || g_s.logoMode == 2) {
            // schlichter Kreis (auch Ersatz, wenn das Bild nicht geladen werden kann)
            logo = MulDiv(13, dpi, 96);
            ly = (int)std::lround(lc - logo / 2.0f);
            Gdiplus::Graphics g(mem);
            SetupGraphics(g);
            Gdiplus::SolidBrush b(GpCol(g_s.fg));
            g.FillEllipse(&b, (float)leftX, (float)ly, (float)logo, (float)logo);
        } else {
            int gap = MulDiv(1, dpi, 96);
            if (gap < 1) gap = 1;
            int sq = (logo - gap) / 2;
            HBRUSH fgBr = CreateSolidBrush(g_s.fg);
            for (int i = 0; i < 4; i++) {
                int x = leftX + (i % 2) * (sq + gap);
                int y = ly + (i / 2) * (sq + gap);
                RECT r{x, y, x + sq, y + sq};
                FillRect(mem, &r, fgBr);
            }
            DeleteObject(fgBr);
        }
        // gleicher Abstand links und rechts → Hervorhebung zentriert
        g_cur->startRect = {leftX - pad / 2, 0, leftX + logo + pad / 2, rc.bottom};
        leftX = g_cur->startRect.right + pad / 2;
    }

    // ---- Rechts: Module von rechts nach links ----
    g_cur->hits.clear();
    int x = rc.right - pad;

    auto textW = [&](const std::wstring& s, HFONT f) {
        SelectObject(mem, f);
        SIZE sz{};
        GetTextExtentPoint32W(mem, s.c_str(), (int)s.size(), &sz);
        return (int)sz.cx;
    };
    auto drawText = [&](const std::wstring& s, HFONT f, COLORREF col, const wchar_t* sample = nullptr) {
        int w = textW(s, f);
        if (sample) { int sw = textW(sample, f); if (sw > w) w = sw; }
        x -= w;
        RECT r{x, 0, x + w, rc.bottom};
        SelectObject(mem, f);
        SetTextColor(mem, col);
        DrawTextW(mem, s.c_str(), -1, &r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    };
    auto drawGlyph = [&](wchar_t g, COLORREF col) {
        wchar_t s[2] = {g, 0};
        drawText(s, g_iconFont, col);
    };
    auto space = [&](int v) { x -= MulDiv(v, dpi, 96); };
    bool mac = g_s.style == 1;
    float sc = dpi / 96.f;
    float cy = rc.bottom / 2.f;
    // Platz für ein Vektor-Icon reservieren, fn(linkeKante) zeichnet es
    auto drawShape = [&](float wDip, auto fn) {
        int w = (int)(wDip * sc + 0.5f);
        x -= w;
        fn((float)x);
    };
    auto module = [&](Action act, auto fn) {
        int right = x;
        fn();
        g_cur->hits.push_back({x - iconGap / 2, right + iconGap / 2, act});
        x -= iconGap;
    };
    auto labeled = [&](Action act, const wchar_t* label, const std::wstring& value,
                       const wchar_t* sample) {
        module(act, [&] {
            drawText(value, g_font, g_s.fg, sample);
            space(4);
            drawText(T(std::wstring(label)), g_font, dim);
        });
    };

    SYSTEMTIME st;
    GetLocalTime(&st);

    // Module zeichnen – von rechts nach links, Reihenfolge aus den Einstellungen
    auto drawMod = [&](int id) {
        switch (id) {
        case M_CLOCK: {
            if (g_s.showClock) {
                module(ACT_CLOCK, [&] {
                    wchar_t date[128] = L"", time[64] = L"";
                    if (!g_s.dateFormat.empty())
                        GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, g_s.dateFormat.c_str(),
                                        date, 128, nullptr);
                    std::wstring tf = g_s.timeFormat.empty() ? L"HH:mm" : g_s.timeFormat;
                    GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, tf.c_str(), time, 64);
                    std::wstring clock = date[0] ? std::wstring(date) + (mac ? L"  " : L"   ") + time : std::wstring(time);
                    drawText(clock, g_font, g_s.fg);
                });
            }
            break;
        }
        case M_WEEK: {
            if (g_s.showWeek)
                labeled(ACT_CLOCK, L"KW", std::to_wstring(IsoWeek(st)), nullptr);
            break;
        }
        case M_CONTROL: {
            if (g_s.controlCenter) {
                module(ACT_CONTROL, [&] {
                    if (mac) drawShape(15, [&](float lx) { IconControl(mem, lx, cy, sc, g_s.fg); });
                    else drawGlyph(0xE713, g_s.fg);
                });
            }
            break;
        }
        case M_SEARCH: {
            if (g_s.search) {
                module(ACT_SEARCH, [&] {
                    if (mac) drawShape(15, [&](float lx) { IconSearch(mem, lx, cy, sc, g_s.fg); });
                    else drawGlyph(0xE721, g_s.fg);
                });
            }
            break;
        }
        case M_BATTERY: {
            if (g_s.battery && g_st.hasBattery) {
                module(ACT_BATTERY, [&] {
                    COLORREF col = (!g_st.charging && g_st.battery <= 20) ? RGB(255, 69, 58) : g_s.fg;
                    if (mac) {
                        drawShape(27, [&](float lx) {
                            IconBattery(mem, lx, cy, sc, g_st.battery, g_st.charging, g_s.fg, col, g_s.bg);
                        });
                    } else {
                        int idx = (g_st.battery + 5) / 10;
                        if (idx > 10) idx = 10;
                        wchar_t glyph = (wchar_t)((g_st.charging ? 0xEBAB : 0xEBA0) + idx);
                        drawGlyph(glyph, col);
                    }
                    if (g_s.batteryPercent) {
                        space(5);
                        // Ziffern optisch mittig zum Akku-Symbol
                        std::wstring pt = std::to_wstring(g_st.battery) + L" %";
                        int w = textW(pt, g_fontSmall);
                        x -= w;
                        TEXTMETRICW stm{};
                        SelectObject(mem, g_fontSmall);
                        GetTextMetricsW(mem, &stm);
                        float topCentered = (rc.bottom - stm.tmHeight) / 2.0f;          // ganze Zeile mittig
                        float topDigits = rc.bottom / 2.0f - capOffset(g_fontSmall);    // Ziffern mittig
                        int top = (int)std::lround((topCentered + topDigits) / 2.0f);
                        RECT pr{x, top, x + w, rc.bottom};
                        SelectObject(mem, g_fontSmall);
                        SetTextColor(mem, g_s.fg);
                        DrawTextW(mem, pt.c_str(), -1, &pr, DT_RIGHT | DT_TOP | DT_SINGLELINE | DT_NOPREFIX);
                    }
                    if (g_s.batteryTime && !g_st.charging && g_st.batteryTime != (DWORD)-1) {
                        wchar_t t[16];
                        swprintf(t, 16, L"%d:%02d", (int)(g_st.batteryTime / 3600),
                                 (int)((g_st.batteryTime % 3600) / 60));
                        space(6);
                        drawText(t, g_font, dim);
                    }
                });
            }
            break;
        }
        case M_VOLUME: {
            if (g_s.volume && g_st.hasAudio) {
                module(ACT_VOLUME, [&] {
                    int level = (g_st.muted || g_st.volume < 0.005f) ? 0
                                : g_st.volume < 0.34f ? 1 : g_st.volume < 0.67f ? 2 : 3;
                    if (mac) {
                        drawShape(19, [&](float lx) { IconVolume(mem, lx, cy, sc, level, g_s.fg); });
                    } else {
                        static const wchar_t glyphs[] = {0xE74F, 0xE993, 0xE994, 0xE995};
                        drawGlyph(glyphs[level], g_s.fg);
                    }
                });
            }
            break;
        }
        case M_WIFI: {
            if (g_s.wifi) {
                module(ACT_WIFI, [&] {
                    if (mac && !g_st.wifi && g_st.online) {
                        drawShape(14, [&](float lx) { IconEthernet(mem, lx, cy, sc, g_s.fg); });
                    } else if (mac) {
                        int bars = !g_st.wifi ? 0 : g_st.signal >= 67 ? 3 : g_st.signal >= 34 ? 2 : 1;
                        drawShape(20, [&](float lx) { IconWifi(mem, lx + 10.0f * sc, cy, sc, bars, g_s.fg); });
                    } else if (g_st.wifi) {
                        wchar_t glyph = g_st.signal >= 75 ? 0xE701 :
                                        g_st.signal >= 50 ? 0xE874 :
                                        g_st.signal >= 25 ? 0xE873 : 0xE872;
                        drawGlyph(glyph, g_s.fg);
                    } else if (g_st.online) {
                        drawGlyph(0xE839, g_s.fg);
                    } else {
                        drawGlyph(0xE701, dim);
                    }
                });
            }
            break;
        }
        case M_TRAY: {
            if (g_s.trayButton)
                module(ACT_TRAY, [&] {
                    if (mac) drawShape(10, [&](float lx) { IconChevron(mem, lx, cy, sc, g_s.fg); });
                    else drawGlyph(0xE70D, g_s.fg);  // ChevronDown
                });
            break;
        }
        case M_KBD: {
            if (g_s.keyboard && !g_st.kbd.empty())
                module(ACT_KBD, [&] { drawText(g_st.kbd, g_fontBold, g_s.fg); });
            break;
        }
        case M_NET: {
            if (g_s.netSpeed) {
                module(ACT_SYSTEM, [&] {
                    drawText(L"\u2191 " + FmtRate(g_st.up), g_font, g_s.fg, L"\u2191 999,9 MB/s");
                    space(6);
                    drawText(L"\u2193 " + FmtRate(g_st.down), g_font, g_s.fg, L"\u2193 999,9 MB/s");
                });
            }
            break;
        }
        case M_RAM: {
            if (g_s.ram) labeled(ACT_SYSTEM, L"RAM", std::to_wstring(g_st.ram) + L" %", L"100 %");
            break;
        }
        case M_CPU: {
            if (g_s.cpu) labeled(ACT_SYSTEM, L"CPU", std::to_wstring(g_st.cpu) + L" %", L"100 %");
            break;
        }
        case M_DISK: {
            if (g_s.disk && g_st.diskValid) {
                std::wstring dn = DriveName();
                labeled(ACT_SYSTEM, dn.c_str(), FmtSize(g_st.diskFree), nullptr);
            }
            break;
        }
        case M_UPTIME: {
            if (g_s.uptime) {
                ULONGLONG secs = GetTickCount64() / 1000;
                int d = (int)(secs / 86400), hh = (int)((secs % 86400) / 3600), mm = (int)((secs % 3600) / 60);
                wchar_t t[32];
                if (d > 0) swprintf(t, 32, L"%dd %dh", d, hh);
                else       swprintf(t, 32, L"%dh %02dm", hh, mm);
                labeled(ACT_SYSTEM, L"Up", t, nullptr);
            }
            break;
        }
        case M_BT: {
            if (g_s.bluetooth && g_st.btRadio) {
                module(ACT_BLUETOOTH, [&] {
                    bool con = g_st.btConnected > 0;
                    (void)con;
                    drawGlyph(0xE702, g_s.fg);  // Segoe Bluetooth
                });
            }
            break;
        }
        }
    };
    for (int k = (int)g_order.size() - 1; k >= 0; k--) drawMod(g_order[k]);

    g_cur->statusLeft = x + iconGap;

    // Klickbereiche links
    if (g_s.showLogo) g_cur->hits.push_back({g_cur->startRect.left, g_cur->startRect.right, ACT_LOGO});
    int menuX = leftX;
    if (g_s.showAppName) {
        int avail = g_cur->statusLeft - pad - leftX;
        int tw = g_appName.empty() ? MulDiv(60, dpi, 96) : textW(g_appName, g_fontBold);
        if (tw > avail) tw = avail;
        if (tw > 0) {
            SetTextColor(mem, g_s.fg);
            SelectObject(mem, g_fontBold);
            RECT ar{leftX, 0, leftX + tw, rc.bottom};
            DrawTextW(mem, g_appName.c_str(), -1, &ar,
                      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            g_cur->hits.push_back({leftX - pad / 2, leftX + tw + pad / 2, ACT_APP});
            menuX = leftX + tw + MulDiv(18, dpi, 96);
        }
    }

    // App-Menüs (Datei, Bearbeiten, …) neben dem App-Namen
    if (g_s.appMenus && g_s.customMenus) {
        int gap = MulDiv(18, dpi, 96);
        for (size_t i = 0; i < g_appMenuTitles.size(); i++) {
            std::wstring t = T(g_appMenuTitles[i]);
            int tw = textW(t, g_font);
            if (menuX + tw > g_cur->statusLeft - pad) break;
            RECT tr{menuX, 0, menuX + tw, rc.bottom};
            SelectObject(mem, g_font);
            SetTextColor(mem, g_s.fg);
            DrawTextW(mem, t.c_str(), -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            Hit hm{menuX - gap / 2, menuX + tw + gap / 2, ACT_APPMENU};
            hm.idx = (int)i;
            g_cur->hits.push_back(hm);
            menuX += tw + gap;
        }
    }
}

// Rendert die Leiste mit echter Transparenz (Inhalt einmal auf Schwarz und einmal
// auf Weiß zeichnen → daraus Alpha pro Pixel berechnen)
void Render(HWND h) {
    if (!h) return;
    g_cur = BarOf(h);
    if (!g_cur) return;
    EnsureFonts(ScaledDpi(h));
    RECT wr;
    GetWindowRect(h, &wr);
    int w = wr.right - wr.left, ht = wr.bottom - wr.top;
    if (w <= 0 || ht <= 0) return;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -ht;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    void* pk = nullptr;
    void* pw = nullptr;
    HBITMAP bk = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &pk, nullptr, 0);
    HBITMAP bw = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &pw, nullptr, 0);
    if (!bk || !bw) {
        if (bk) DeleteObject(bk);
        if (bw) DeleteObject(bw);
        ReleaseDC(nullptr, screen);
        return;
    }
    HDC dk = CreateCompatibleDC(screen);
    HDC dw = CreateCompatibleDC(screen);
    HGDIOBJ ok_ = SelectObject(dk, bk);
    HGDIOBJ ow_ = SelectObject(dw, bw);

    RECT rc{0, 0, w, ht};
    g_hiRects.clear();
    DrawContent(dk, rc, RGB(0, 0, 0));
    DrawContent(dw, rc, RGB(255, 255, 255));
    GdiFlush();
    std::vector<BYTE> hiMask = BuildHiMask(w, ht);
    COLORREF hiCol = HighlightColor();
    int hiA = g_s.highlightOpacity * 255 / 100;

    bool custom = g_s.effect == FX_CUSTOM;
    BlurCache& bc = g_cur->blur;
    if (custom) EnsureBlur(bc, MonitorHeightOf(h), w, ht, MulDiv(g_s.blurStrength, ScaledDpi(h), 96));
    custom = custom && bc.blurW == w && bc.blurH == ht;

    int op = g_s.opacity * 255 / 100;  // Tönung über dem eigenen Blur
    int bgA = (g_s.effect == FX_ACRYLIC) ? 1 : op;
    if (custom) bgA = 255;
    if (bgA < 1) bgA = 1;  // nie 0, sonst klickt man durch die Leiste
    int tR = GetRValue(g_s.bg), tG = GetGValue(g_s.bg), tB = GetBValue(g_s.bg);
    int bR = tR * bgA / 255;
    int bG = tG * bgA / 255;
    int bB = tB * bgA / 255;

    DWORD* K = (DWORD*)pk;
    DWORD* W = (DWORD*)pw;
    int n = w * ht;
    for (int i = 0; i < n; i++) {
        if (custom) {
            DWORD p = bc.blur[i];
            bR = (((p >> 16) & 255) * (255 - op) + tR * op) / 255;
            bG = (((p >> 8) & 255) * (255 - op) + tG * op) / 255;
            bB = ((p & 255) * (255 - op) + tB * op) / 255;
        }
        DWORD k = K[i], v = W[i];
        int kr = (k >> 16) & 255, kg = (k >> 8) & 255, kb = k & 255;
        int wr_ = (v >> 16) & 255, wg = (v >> 8) & 255, wb = v & 255;
        int diff = ((wr_ - kr) + (wg - kg) + (wb - kb)) / 3;
        if (diff < 0) diff = 0;
        if (diff > 255) diff = 255;
        int a = 255 - diff;
        if (kr > a) kr = a;
        if (kg > a) kg = a;
        if (kb > a) kb = a;
        int inv = 255 - a;
        // Untergrund = Hintergrund + halbtransparente Hervorhebung
        int uA = bgA, uR = bR, uG = bG, uB = bB;
        if (!hiMask.empty() && hiMask[i]) {
            int hA = hiMask[i] * hiA / 255, ih = 255 - hA;
            uA = hA + uA * ih / 255;
            uR = GetRValue(hiCol) * hA / 255 + uR * ih / 255;
            uG = GetGValue(hiCol) * hA / 255 + uG * ih / 255;
            uB = GetBValue(hiCol) * hA / 255 + uB * ih / 255;
        }
        int oa = a + uA * inv / 255;
        int orr = kr + uR * inv / 255;
        int og = kg + uG * inv / 255;
        int ob = kb + uB * inv / 255;
        K[i] = ((DWORD)oa << 24) | ((DWORD)orr << 16) | ((DWORD)og << 8) | (DWORD)ob;
    }

    POINT dst{wr.left, wr.top}, src{0, 0};
    SIZE sz{w, ht};
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(h, screen, &dst, &sz, dk, &src, 0, &bf, ULW_ALPHA);

    SelectObject(dk, ok_);
    SelectObject(dw, ow_);
    DeleteDC(dk);
    DeleteDC(dw);
    DeleteObject(bk);
    DeleteObject(bw);
    ReleaseDC(nullptr, screen);
}

void RenderAll() {
    for (auto b : g_bars) Render(b->hwnd);
}

const Hit* HitAt(HWND h, int px) {
    BarWin* b = BarOf(h);
    if (!b) return nullptr;
    for (auto& hit : b->hits)
        // Logo bleibt bis zum linken Bildschirmrand klickbar
        if ((px >= hit.l || hit.act == ACT_LOGO) && px < hit.r) return &hit;
    return nullptr;
}

Action HitTest(HWND h, int px) {
    const Hit* hit = HitAt(h, px);
    return hit ? hit->act : ACT_NONE;
}

void Refresh() {
    if (g_bars.empty()) return;
    UpdateStatus();
    // Hintergrundbild jede Minute neu laden (z. B. bei Diashow)
    if (g_s.effect == FX_CUSTOM && g_tickCount % 120 == 0) MarkWallDirty();
    RenderAll();
    if (g_popupAct != ACT_APPMENU) PopupRefresh();  // App-Menüs nicht ständig neu einlesen
}

// Einstellungen übernehmen + speichern
void ApplyPopupAccent(HWND h);
void SyncBars();

void Commit() {
    Normalize();
    if (g_popup) ApplyPopupAccent(g_popup);
    SaveSettings();
    if (g_hwnd) {
        SyncBars();
        for (auto b : g_bars) {
            ApplyAccent(b->hwnd);
            PositionAppBar(b->hwnd);
        }
        g_st.diskValid = false;
        Refresh();
    }
}

// ======================================================================
// Einstellungsfenster
// ======================================================================
enum { ID_HEIGHT = 101, ID_OPACITY, ID_BG, ID_FG, ID_EFFECT, ID_RESET, ID_CLOSE, ID_BLUR,
       ID_MENUOPACITY, ID_MENUEFFECT, ID_THEME, ID_BARTEXT, ID_HLMODE, ID_HLCOLOR, ID_HLOPACITY,
       ID_CARDOPACITY, ID_MONITOR, ID_STYLE, ID_LOGOMODE, ID_LOGOBROWSE, ID_GAP, ID_ORDERLIST,
       ID_ORDERUP, ID_ORDERDOWN, ID_ORDERRESET, ID_SCALE, ID_FONTWEIGHT };

#define WM_SETTINGS_REBUILD (WM_APP + 10)

// ---------- Aufbau der Seiten ----------
enum UiType { UI_SECTION, UI_TOGGLE, UI_TOGGLE2, UI_SLIDER, UI_COMBO, UI_COLOR, UI_EDIT, UI_EDITBROWSE,
              UI_BUTTON, UI_ORDER, UI_NOTE };
struct UiRow { int page; UiType type; std::wstring label; int id; int h; RECT rc; };
struct UiCtl { HWND hwnd; int page; };

std::vector<UiRow> g_ui;
std::vector<UiCtl> g_uiCtls;
std::vector<std::wstring> g_monChoices;
int g_page = 0, g_hoverNav = -1;
bool g_navTracking = false;
bool g_syncing = false;
int g_settingsClientW = 0, g_settingsClientH = 0;
HFONT g_uiFont = nullptr, g_uiFontBold = nullptr, g_uiFontTitle = nullptr, g_uiFontSmall = nullptr,
      g_uiIcon = nullptr;

struct NavPage { wchar_t glyph; const wchar_t* name; };
static const NavPage kPages[] = {
    {0xE713, L"Allgemein"}, {0xE771, L"Darstellung"}, {0xE8FD, L"Leiste"}, {0xE9D9, L"Statusanzeigen"},
    {0xE787, L"Uhr"},       {0xE700, L"Menüs"},       {0xE946, L"Info"}};
static const int kPageCount = 7;

// Farben des Einstellungsfensters (wie Windows 11, hell/dunkel)
struct UiColors { COLORREF bg, side, card, text, sub, border, input, accent; bool dark; };
UiColors g_uc;
HBRUSH g_brCard = nullptr, g_brInput = nullptr;

bool SettingsDark() {
    if (g_s.theme == THEME_LIGHT) return false;
    if (g_s.theme == THEME_DARK) return true;
    return !WindowsUsesLightTheme();
}

void UpdateUiColors() {
    bool d = SettingsDark();
    g_uc.dark = d;
    g_uc.bg = d ? RGB(32, 32, 32) : RGB(243, 243, 243);
    g_uc.side = d ? RGB(26, 26, 26) : RGB(235, 235, 235);
    g_uc.card = d ? RGB(43, 43, 43) : RGB(251, 251, 251);
    g_uc.text = d ? RGB(255, 255, 255) : RGB(26, 26, 26);
    g_uc.sub = d ? RGB(190, 190, 190) : RGB(96, 96, 96);
    g_uc.border = d ? RGB(56, 56, 56) : RGB(226, 226, 226);
    g_uc.input = d ? RGB(55, 55, 55) : RGB(255, 255, 255);
    g_uc.accent = WindowsAccent();
    if (g_brCard) DeleteObject(g_brCard);
    if (g_brInput) DeleteObject(g_brInput);
    g_brCard = CreateSolidBrush(g_uc.card);
    g_brInput = CreateSolidBrush(g_uc.input);
}

void AddUi(int page, UiType t, const std::wstring& label, int id = 0, int h = 0) {
    if (!h) h = t == UI_SECTION ? 36 : t == UI_ORDER ? 300 : t == UI_NOTE ? 60 : 50;
    g_ui.push_back({page, t, label, id, h, {}});
}

void DefineUi() {
    g_ui.clear();
    // Allgemein
    AddUi(0, UI_SECTION, L"Verhalten");
    AddUi(0, UI_TOGGLE, L"Eigene Menüs beim Klicken", 222);
    AddUi(0, UI_TOGGLE, L"Menüs schon beim Überfahren öffnen", 223);
    AddUi(0, UI_COMBO, L"Leiste anzeigen auf", ID_MONITOR);
    AddUi(0, UI_SECTION, L"Zurücksetzen");
    AddUi(0, UI_BUTTON, L"Alle Einstellungen auf Standard zurücksetzen", ID_RESET);
    // Darstellung
    AddUi(1, UI_SECTION, L"Farben");
    AddUi(1, UI_COMBO, L"Design", ID_THEME);
    AddUi(1, UI_COMBO, L"Schriftfarbe der Leiste", ID_BARTEXT);
    AddUi(1, UI_COLOR, L"Hintergrundfarbe", ID_BG);
    AddUi(1, UI_COLOR, L"Textfarbe", ID_FG);
    AddUi(1, UI_SECTION, L"Leiste");
    AddUi(1, UI_SLIDER, L"Skalierung (Leiste und Menüs)", ID_SCALE);
    AddUi(1, UI_COMBO, L"Schriftstärke", ID_FONTWEIGHT);
    AddUi(1, UI_SLIDER, L"Höhe", ID_HEIGHT);
    AddUi(1, UI_SLIDER, L"Deckkraft", ID_OPACITY);
    AddUi(1, UI_COMBO, L"Effekt", ID_EFFECT);
    AddUi(1, UI_SLIDER, L"Blur-Stärke (nur „Blur einstellbar“)", ID_BLUR);
    // Leiste
    AddUi(2, UI_SECTION, L"Links");
    AddUi(2, UI_TOGGLE, L"Logo anzeigen", 201);
    AddUi(2, UI_COMBO, L"Logo", ID_LOGOMODE);
    AddUi(2, UI_EDITBROWSE, L"Eigenes Logo (Bilddatei)", 304);
    AddUi(2, UI_TOGGLE, L"Name der aktiven App", 202);
    AddUi(2, UI_TOGGLE, L"App-Menüs (Datei, Bearbeiten …)", 227);
    AddUi(2, UI_SECTION, L"Rechts");
    AddUi(2, UI_COMBO, L"Symbol-Stil", ID_STYLE);
    AddUi(2, UI_SLIDER, L"Abstand zwischen den Symbolen", ID_GAP);
    AddUi(2, UI_ORDER, L"Reihenfolge der Symbole (links → rechts)", ID_ORDERLIST);
    // Statusanzeigen
    AddUi(3, UI_SECTION, L"Anzeigen");
    static const int statusIds[] = {210, 211, 226, 212, 213, 214, 224, 225, 221, 215, 216, 217, 218, 219, 220};
    for (int id : statusIds) AddUi(3, UI_TOGGLE2, FindCheck(id)->label, id);
    AddUi(3, UI_SECTION, L"Speicherplatz");
    AddUi(3, UI_EDIT, L"Laufwerk", 303);
    // Uhr
    AddUi(4, UI_SECTION, L"Uhr");
    AddUi(4, UI_TOGGLE, L"Uhr anzeigen", 203);
    AddUi(4, UI_TOGGLE, L"Kalenderwoche anzeigen", 204);
    AddUi(4, UI_EDIT, L"Datumsformat", 301);
    AddUi(4, UI_EDIT, L"Zeitformat", 302);
    AddUi(4, UI_NOTE, L"Beispiele:  dd.MM.yyyy  ·  ddd d. MMM  ·  dddd, d. MMMM\n"
                      L"HH:mm  ·  HH:mm:ss  ·  h:mm tt   (Datum leer = nur Uhrzeit)");
    // Menüs
    AddUi(5, UI_SECTION, L"Aussehen");
    AddUi(5, UI_SLIDER, L"Deckkraft", ID_MENUOPACITY);
    AddUi(5, UI_COMBO, L"Effekt", ID_MENUEFFECT);
    AddUi(5, UI_SLIDER, L"Kacheln", ID_CARDOPACITY);
    AddUi(5, UI_SECTION, L"Hervorhebung");
    AddUi(5, UI_COMBO, L"Farbe", ID_HLMODE);
    AddUi(5, UI_COLOR, L"Eigene Farbe", ID_HLCOLOR);
    AddUi(5, UI_SLIDER, L"Intensität", ID_HLOPACITY);
    // Info
    AddUi(6, UI_SECTION, L"Top Menu Bar");
    AddUi(6, UI_NOTE, L"macOS-artige Menüleiste für Windows 11\n"
                      L"Alle Änderungen werden sofort übernommen und gespeichert.");
    AddUi(6, UI_NOTE, L"Rechtsklick auf die Leiste öffnet dieses Fenster.\n"
                      L"Mausrad über dem Lautsprecher ändert die Lautstärke, Mittelklick schaltet stumm.");
}

// Positionen berechnen (Pixel), liefert die benötigte Höhe
int LayoutUi(UINT dpi) {
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    int cx0 = S(248), cx1 = S(852), mid = (cx0 + cx1) / 2;
    int maxY = 0;
    for (int p = 0; p < kPageCount; p++) {
        int y = S(84), col = 0;
        bool first = true;
        for (auto& r : g_ui) {
            if (r.page != p) continue;
            int h = S(r.h);
            if (r.type != UI_TOGGLE2 && col == 1) {  // halbe Zeile abschließen
                y += S(52);
                col = 0;
            }
            if (r.type == UI_SECTION) {
                if (!first) y += S(14);
                r.rc = {cx0, y, cx1, y + h};
                y += h;
            } else if (r.type == UI_TOGGLE2) {
                r.rc = col == 0 ? RECT{cx0, y, mid - S(3), y + h} : RECT{mid + S(3), y, cx1, y + h};
                if (col == 1) y += h + S(2);
                col ^= 1;
            } else {
                r.rc = {cx0, y, cx1, y + h};
                y += h + S(2);
            }
            first = false;
        }
        if (col == 1) y += S(52);
        if (y > maxY) maxY = y;
    }
    return maxY + S(24);
}

void SetPage(HWND w, int page) {
    g_page = page;
    for (auto& c : g_uiCtls) ShowWindow(c.hwnd, c.page == page ? SW_SHOW : SW_HIDE);
    InvalidateRect(w, nullptr, FALSE);
}

void FillOrderList(HWND w, int sel) {
    HWND lb = GetDlgItem(w, ID_ORDERLIST);
    if (!lb) return;
    SendMessageW(lb, LB_RESETCONTENT, 0, 0);
    for (int id : g_order) SendMessageW(lb, LB_ADDSTRING, 0, (LPARAM)T(kModNames[id]));
    if (sel >= 0) SendMessageW(lb, LB_SETCURSEL, sel, 0);
}

void FillCombo(HWND cb, int id) {
    auto add = [&](const wchar_t* t) { SendMessageW(cb, CB_ADDSTRING, 0, (LPARAM)T(t)); };
    switch (id) {
    case ID_MONITOR: {
        g_monChoices = {L"all", L"primary"};
        add(L"Allen Monitoren");
        add(L"Hauptmonitor");
        auto mons = ListMonitors();
        for (size_t i = 0; i < mons.size(); i++) {
            wchar_t t[128];
            swprintf(t, 128, T(L"Monitor %d  (%d × %d)%s"), (int)i + 1, (int)(mons[i].rc.right - mons[i].rc.left),
                     (int)(mons[i].rc.bottom - mons[i].rc.top), mons[i].primary ? T(L" – Haupt") : L"");
            add(t);
            g_monChoices.push_back(mons[i].device);
        }
        break;
    }
    case ID_THEME: add(L"Eigene Farben"); add(L"Hell"); add(L"Dunkel"); add(L"Wie Windows"); break;
    case ID_BARTEXT: add(L"Automatisch"); add(L"Weiß"); add(L"Schwarz"); break;
    case ID_EFFECT: add(L"Kein Effekt"); add(L"Unschärfe (Blur)"); add(L"Acryl"); add(L"Blur einstellbar"); break;
    case ID_LOGOMODE: add(L"Windows-Logo"); add(L"Kreis"); add(L"Eigenes Bild"); break;
    case ID_STYLE: add(L"Windows 11"); add(L"macOS-Stil"); break;
    case ID_MENUEFFECT: add(L"Kein Effekt"); add(L"Unschärfe (Blur)"); add(L"Acryl"); break;
    case ID_HLMODE: add(L"Windows-Akzentfarbe"); add(L"Eigene Farbe"); add(L"Aus"); break;
    case ID_FONTWEIGHT: add(L"Automatisch"); add(L"Dünn"); add(L"Normal"); add(L"Mittel"); add(L"Halbfett"); add(L"Fett"); break;
    }
}

void SliderRange(int id, int& mn, int& mx) {
    switch (id) {
    case ID_HEIGHT: mn = 20; mx = 48; break;
    case ID_SCALE: mn = 75; mx = 200; break;
    case ID_BLUR: mn = 0; mx = 40; break;
    case ID_GAP: mn = 4; mx = 48; break;
    case ID_HLOPACITY: mn = 5; mx = 100; break;
    default: mn = 0; mx = 100; break;
    }
}

int* SliderField(int id) {
    switch (id) {
    case ID_HEIGHT: return &g_s.height;
    case ID_SCALE: return &g_s.scale;
    case ID_OPACITY: return &g_s.opacity;
    case ID_BLUR: return &g_s.blurStrength;
    case ID_GAP: return &g_s.iconGap;
    case ID_MENUOPACITY: return &g_s.menuOpacity;
    case ID_HLOPACITY: return &g_s.highlightOpacity;
    case ID_CARDOPACITY: return &g_s.cardOpacity;
    }
    return nullptr;
}

std::wstring SliderText(int id) {
    int* f = SliderField(id);
    if (!f) return L"";
    wchar_t b[32];
    if (id == ID_HEIGHT || id == ID_GAP) swprintf(b, 32, L"%d px", *f);
    else if (id == ID_BLUR) swprintf(b, 32, L"%d", *f);
    else swprintf(b, 32, L"%d %%", *f);
    return b;
}

int* ComboField(int id) {
    switch (id) {
    case ID_THEME: return &g_s.theme;
    case ID_BARTEXT: return &g_s.barText;
    case ID_EFFECT: return &g_s.effect;
    case ID_LOGOMODE: return &g_s.logoMode;
    case ID_STYLE: return &g_s.style;
    case ID_MENUEFFECT: return &g_s.menuEffect;
    case ID_HLMODE: return &g_s.highlightMode;
    case ID_FONTWEIGHT: return &g_s.fontWeight;
    }
    return nullptr;
}

void SyncControls(HWND w) {
    g_syncing = true;
    for (auto& r : g_ui) {
        HWND c = GetDlgItem(w, r.id);
        if (!c) continue;
        if (r.type == UI_SLIDER) {
            if (int* f = SliderField(r.id)) SendMessageW(c, TBM_SETPOS, TRUE, *f);
        } else if (r.type == UI_COMBO) {
            int sel = 0;
            if (r.id == ID_MONITOR) {
                sel = 1;
                for (size_t i = 0; i < g_monChoices.size(); i++)
                    if (g_monChoices[i] == g_s.monitor) sel = (int)i;
            } else if (int* f = ComboField(r.id)) {
                sel = *f;
            }
            SendMessageW(c, CB_SETCURSEL, sel, 0);
        } else if (r.type == UI_EDIT || r.type == UI_EDITBROWSE) {
            if (const StrDef* sd = FindStr(r.id)) SetWindowTextW(c, (g_s.*(sd->field)).c_str());
        } else {
            InvalidateRect(c, nullptr, FALSE);
        }
    }
    EnableWindow(GetDlgItem(w, ID_BLUR), g_s.effect == FX_CUSTOM);
    FillOrderList(w, -1);
    InvalidateRect(w, nullptr, FALSE);
    g_syncing = false;
}

void ApplyDarkTheme(HWND c, const wchar_t* cls) {
    if (!g_uc.dark) return;
    if (!_wcsicmp(cls, L"COMBOBOX") || !_wcsicmp(cls, L"EDIT")) SetWindowTheme(c, L"DarkMode_CFD", nullptr);
    else SetWindowTheme(c, L"DarkMode_Explorer", nullptr);
}

void BuildSettingsControls(HWND w) {
    HINSTANCE inst = (HINSTANCE)&__ImageBase;
    UINT dpi = GetDpiForWindow(w);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };

    g_uiFont = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                           L"Segoe UI Variable Text");
    g_uiFontBold = CreateFontW(-S(14), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                               L"Segoe UI Variable Text");
    g_uiFontSmall = CreateFontW(-S(12), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0,
                                L"Segoe UI Variable Text");
    g_uiFontTitle = CreateFontW(-S(28), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY,
                                0, L"Segoe UI Variable Display");
    g_uiIcon = CreateIconFont(S(16));

    DefineUi();
    g_settingsClientW = S(880);
    g_settingsClientH = LayoutUi(dpi);
    if (g_settingsClientH < S(600)) g_settingsClientH = S(600);

    auto mk = [&](int page, const wchar_t* cls, const wchar_t* text, DWORD style, RECT r, int id, DWORD ex = 0) {
        HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | style, r.left, r.top, r.right - r.left, r.bottom - r.top,
                                 w, (HMENU)(INT_PTR)id, inst, nullptr);
        SendMessageW(c, WM_SETFONT, (WPARAM)g_uiFont, TRUE);
        ApplyDarkTheme(c, cls);
        g_uiCtls.push_back({c, page});
        return c;
    };

    for (auto& r : g_ui) {
        RECT rc = r.rc;
        int right = rc.right - S(16), cy = (rc.top + rc.bottom) / 2;
        switch (r.type) {
        case UI_TOGGLE:
        case UI_TOGGLE2:
            mk(r.page, L"BUTTON", T(r.label).c_str(), BS_OWNERDRAW | WS_TABSTOP, rc, r.id);
            break;
        case UI_SLIDER: {
            HWND t = mk(r.page, TRACKBAR_CLASSW, L"", TBS_HORZ | TBS_NOTICKS | WS_TABSTOP,
                        {right - S(264), cy - S(14), right - S(60), cy + S(14)}, r.id);
            int mn, mx;
            SliderRange(r.id, mn, mx);
            SendMessageW(t, TBM_SETRANGE, TRUE, MAKELPARAM(mn, mx));
            break;
        }
        case UI_COMBO: {
            HWND cb = mk(r.page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,
                         {right - S(240), cy - S(15), right, cy + S(260)}, r.id);
            FillCombo(cb, r.id);
            SendMessageW(cb, CB_SETDROPPEDWIDTH, S(260), 0);
            break;
        }
        case UI_COLOR:
            mk(r.page, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, {right - S(56), cy - S(14), right, cy + S(14)}, r.id);
            break;
        case UI_EDIT:
            mk(r.page, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
               {right - S(240), cy - S(14), right, cy + S(14)}, r.id);
            break;
        case UI_EDITBROWSE:
            mk(r.page, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP | WS_BORDER,
               {right - S(240), cy - S(14), right - S(44), cy + S(14)}, r.id);
            mk(r.page, L"BUTTON", L"…", BS_PUSHBUTTON | WS_TABSTOP, {right - S(38), cy - S(15), right, cy + S(15)},
               ID_LOGOBROWSE);
            break;
        case UI_BUTTON:
            mk(r.page, L"BUTTON", T(L"Zurücksetzen"), BS_PUSHBUTTON | WS_TABSTOP,
               {right - S(160), cy - S(16), right, cy + S(16)}, r.id);
            break;
        case UI_ORDER: {
            int x0 = rc.left + S(16), top = rc.top + S(44);
            mk(r.page, L"LISTBOX", L"", LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP | WS_BORDER,
               {x0, top, x0 + S(320), rc.bottom - S(16)}, ID_ORDERLIST);
            int bx = x0 + S(336);
            mk(r.page, L"BUTTON", T(L"▲  Nach oben"), BS_PUSHBUTTON | WS_TABSTOP, {bx, top, bx + S(150), top + S(32)},
               ID_ORDERUP);
            mk(r.page, L"BUTTON", T(L"▼  Nach unten"), BS_PUSHBUTTON | WS_TABSTOP,
               {bx, top + S(40), bx + S(150), top + S(72)}, ID_ORDERDOWN);
            mk(r.page, L"BUTTON", T(L"Standard"), BS_PUSHBUTTON | WS_TABSTOP,
               {bx, rc.bottom - S(48), bx + S(150), rc.bottom - S(16)}, ID_ORDERRESET);
            break;
        }
        default:
            break;
        }
    }
    SetPage(w, g_page);
}

// ---------- Zeichnen ----------
void FillRoundAA(HDC dc, RECT r, float rad, COLORREF fill, COLORREF border = CLR_INVALID) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
    Gdiplus::GraphicsPath p;
    AddRoundRect(p, (float)r.left, (float)r.top, (float)(r.right - r.left - 1), (float)(r.bottom - r.top - 1), rad);
    Gdiplus::SolidBrush b(GpCol(fill));
    g.FillPath(&b, &p);
    if (border != CLR_INVALID) {
        Gdiplus::Pen pen(GpCol(border), 1.0f);
        g.DrawPath(&pen, &p);
    }
}

RECT NavRect(int i, UINT dpi) {
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    return {S(12), S(76) + i * S(42), S(208), S(76) + i * S(42) + S(38)};
}

void PaintSettings(HWND w, HDC hdc) {
    RECT rc;
    GetClientRect(w, &rc);
    UINT dpi = GetDpiForWindow(w);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    HDC mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    HBRUSH bg = CreateSolidBrush(g_uc.bg);
    FillRect(mem, &rc, bg);
    DeleteObject(bg);
    RECT side{0, 0, S(220), rc.bottom};
    HBRUSH sb = CreateSolidBrush(g_uc.side);
    FillRect(mem, &side, sb);
    DeleteObject(sb);

    auto text = [&](const std::wstring& s, RECT r, HFONT f, COLORREF c, UINT fmt) {
        SelectObject(mem, f);
        SetTextColor(mem, c);
        DrawTextW(mem, s.c_str(), -1, &r, fmt | DT_NOPREFIX);
    };

    // Seitenleiste
    text(T(L"Menüleiste"), {S(24), S(22), S(208), S(56)}, g_uiFontBold, g_uc.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    for (int i = 0; i < kPageCount; i++) {
        RECT nr = NavRect(i, dpi);
        if (i == g_page || i == g_hoverNav)
            FillRoundAA(mem, nr, (float)S(6), i == g_page ? g_uc.card : Blend(g_uc.card, g_uc.side, 55));
        if (i == g_page) {
            RECT bar{nr.left, nr.top + S(10), nr.left + S(3), nr.bottom - S(10)};
            FillRoundAA(mem, bar, S(3) / 2.0f, g_uc.accent);
        }
        wchar_t gs[2] = {kPages[i].glyph, 0};
        text(gs, {nr.left + S(12), nr.top, nr.left + S(40), nr.bottom}, g_uiIcon, g_uc.text,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        text(T(kPages[i].name), {nr.left + S(46), nr.top, nr.right, nr.bottom}, i == g_page ? g_uiFontBold : g_uiFont,
             g_uc.text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Inhalt
    text(T(kPages[g_page].name), {S(248), S(20), S(852), S(64)}, g_uiFontTitle, g_uc.text,
         DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    for (auto& r : g_ui) {
        if (r.page != g_page) continue;
        RECT c = r.rc;
        if (r.type == UI_SECTION) {
            text(T(r.label), {c.left + S(2), c.top, c.right, c.bottom - S(6)}, g_uiFontBold, g_uc.text,
                 DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
            continue;
        }
        if (r.type == UI_TOGGLE || r.type == UI_TOGGLE2) continue;  // zeichnen sich selbst
        FillRoundAA(mem, c, (float)S(6), g_uc.card, g_uc.border);
        if (r.type == UI_NOTE) {
            text(T(r.label), {c.left + S(16), c.top + S(10), c.right - S(16), c.bottom - S(8)}, g_uiFont, g_uc.sub,
                 DT_LEFT | DT_WORDBREAK);
            continue;
        }
        if (r.type == UI_ORDER) {
            text(T(r.label), {c.left + S(16), c.top, c.right - S(16), c.top + S(40)}, g_uiFont, g_uc.text,
                 DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            continue;
        }
        text(T(r.label), {c.left + S(16), c.top, c.right - S(280), c.bottom}, g_uiFont, g_uc.text,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (r.type == UI_SLIDER)
            text(SliderText(r.id), {c.right - S(70), c.top, c.right - S(16), c.bottom}, g_uiFont, g_uc.sub,
                 DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// Schalter (wie in den Windows-11-Einstellungen) und Farbfelder
void DrawSettingsItem(const DRAWITEMSTRUCT* d) {
    RECT r = d->rcItem;
    int w = r.right - r.left, h = r.bottom - r.top;
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, h);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);
    UINT dpi = GetDpiForWindow(d->hwndItem);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    RECT lr{0, 0, w, h};

    if (const CheckDef* cd = FindCheck((int)d->CtlID)) {
        HBRUSH bg = CreateSolidBrush(g_uc.bg);
        FillRect(mem, &lr, bg);
        DeleteObject(bg);
        FillRoundAA(mem, lr, (float)S(6), g_uc.card, g_uc.border);
        bool on = g_s.*(cd->field);
        wchar_t label[128] = L"";
        GetWindowTextW(d->hwndItem, label, 128);
        SelectObject(mem, g_uiFont);
        SetTextColor(mem, g_uc.text);
        RECT tr{S(16), 0, w - S(110), h};
        DrawTextW(mem, label, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SetTextColor(mem, g_uc.sub);
        RECT st{w - S(110), 0, w - S(66), h};
        DrawTextW(mem, on ? T(L"Ein") : T(L"Aus"), -1, &st, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        // Schalter
        float sw = (float)S(40), sh = (float)S(20);
        float sx = (float)(w - S(16)) - sw, sy = (h - sh) / 2;
        Gdiplus::Graphics g(mem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        Gdiplus::GraphicsPath pill;
        AddRoundRect(pill, sx, sy, sw, sh, sh / 2);
        float kd = sh - (float)S(8);
        if (on) {
            Gdiplus::SolidBrush ab(GpCol(g_uc.accent));
            g.FillPath(&ab, &pill);
            Gdiplus::SolidBrush kb(GpCol(TextOn(g_uc.accent)));
            g.FillEllipse(&kb, sx + sw - kd - S(4), sy + S(4), kd, kd);
        } else {
            Gdiplus::Pen pp(GpCol(g_uc.sub), 1.0f);
            g.DrawPath(&pp, &pill);
            Gdiplus::SolidBrush kb(GpCol(g_uc.sub));
            g.FillEllipse(&kb, sx + S(4), sy + S(4), kd, kd);
        }
        if (d->itemState & ODS_FOCUS) {
            Gdiplus::GraphicsPath fp;
            AddRoundRect(fp, sx - 3, sy - 3, sw + 6, sh + 6, sh / 2 + 3);
            Gdiplus::Pen fpen(GpCol(g_uc.text, 120), 1.0f);
            g.DrawPath(&fpen, &fp);
        }
    } else {
        // Farbfeld
        COLORREF col = d->CtlID == ID_BG ? g_s.bg : d->CtlID == ID_FG ? g_s.fg : HighlightColor();
        HBRUSH bg = CreateSolidBrush(g_uc.card);
        FillRect(mem, &lr, bg);
        DeleteObject(bg);
        FillRoundAA(mem, lr, (float)S(5), col, g_uc.sub);
    }
    BitBlt(d->hDC, r.left, r.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
}

int NavHit(HWND w, POINT p) {
    UINT dpi = GetDpiForWindow(w);
    for (int i = 0; i < kPageCount; i++) {
        RECT nr = NavRect(i, dpi);
        if (PtInRect(&nr, p)) return i;
    }
    return -1;
}

void SaveOrder() {
    std::wstring str;
    for (size_t i = 0; i < g_order.size(); i++) str += (i ? L"," : L"") + std::to_wstring(g_order[i]);
    g_s.order = str;
}

LRESULT CALLBACK SettingsProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        UpdateUiColors();
        BOOL dark = g_uc.dark;
        DwmSetWindowAttribute(w, 20, &dark, sizeof(dark));  // dunkle Titelleiste
        COLORREF cap = g_uc.side, capText = g_uc.text;
        DwmSetWindowAttribute(w, 35, &cap, sizeof(cap));      // Farbe der Titelleiste
        DwmSetWindowAttribute(w, 36, &capText, sizeof(capText));
        BuildSettingsControls(w);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        PaintSettings(w, dc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wp, g_uc.card);
        SetTextColor((HDC)wp, g_uc.text);
        return (LRESULT)g_brCard;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        SetBkColor((HDC)wp, g_uc.input);
        SetTextColor((HDC)wp, g_uc.text);
        return (LRESULT)g_brInput;
    case WM_DRAWITEM:
        DrawSettingsItem((const DRAWITEMSTRUCT*)lp);
        return TRUE;
    case WM_MOUSEMOVE: {
        if (!g_navTracking) {
            TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, w, 0};
            TrackMouseEvent(&t);
            g_navTracking = true;
        }
        int hv = NavHit(w, {(short)LOWORD(lp), (short)HIWORD(lp)});
        if (hv != g_hoverNav) {
            g_hoverNav = hv;
            InvalidateRect(w, nullptr, FALSE);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_navTracking = false;
        if (g_hoverNav != -1) {
            g_hoverNav = -1;
            InvalidateRect(w, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        int nv = NavHit(w, {(short)LOWORD(lp), (short)HIWORD(lp)});
        if (nv >= 0 && nv != g_page) SetPage(w, nv);
        return 0;
    }
    case WM_HSCROLL: {
        HWND t = (HWND)lp;
        int id = GetDlgCtrlID(t);
        int* f = SliderField(id);
        if (!f) return 0;
        *f = (int)SendMessageW(t, TBM_GETPOS, 0, 0);
        InvalidateRect(w, nullptr, FALSE);
        // Höhe/Skalierung erst beim Loslassen anwenden (sonst springen alle Fenster)
        if (!g_syncing && ((id != ID_HEIGHT && id != ID_SCALE) || LOWORD(wp) == TB_ENDTRACK)) Commit();
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp), code = HIWORD(wp);
        if (g_syncing) return 0;
        if (const CheckDef* c = FindCheck(id)) {
            if (code == BN_CLICKED) {
                g_s.*(c->field) = !(g_s.*(c->field));
                Commit();
                InvalidateRect((HWND)lp, nullptr, FALSE);
            }
            return 0;
        }
        if (const StrDef* s = FindStr(id)) {
            if (code == EN_CHANGE) {
                wchar_t buf[256] = L"";
                GetDlgItemTextW(w, id, buf, 256);
                g_s.*(s->field) = buf;
                Commit();
            }
            return 0;
        }
        if (code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW((HWND)lp, CB_GETCURSEL, 0, 0);
            if (id == ID_MONITOR) {
                if (sel >= 0 && sel < (int)g_monChoices.size()) g_s.monitor = g_monChoices[sel];
                Commit();
            } else if (int* f = ComboField(id)) {
                bool darkBefore = g_uc.dark;
                *f = sel;
                Commit();
                if (id == ID_EFFECT) EnableWindow(GetDlgItem(w, ID_BLUR), g_s.effect == FX_CUSTOM);
                InvalidateRect(GetDlgItem(w, ID_BG), nullptr, FALSE);
                InvalidateRect(GetDlgItem(w, ID_FG), nullptr, FALSE);
                InvalidateRect(GetDlgItem(w, ID_HLCOLOR), nullptr, FALSE);
                if (id == ID_THEME && SettingsDark() != darkBefore) PostMessageW(w, WM_SETTINGS_REBUILD, 0, 0);
            }
            return 0;
        }
        switch (id) {
        case ID_BG:
        case ID_FG:
        case ID_HLCOLOR:
            if (code == BN_CLICKED) {
                static COLORREF cust[16];
                CHOOSECOLORW cc{sizeof(cc)};
                cc.hwndOwner = w;
                cc.rgbResult = id == ID_BG ? g_s.bg : id == ID_FG ? g_s.fg : HighlightColor();
                cc.lpCustColors = cust;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    if (id == ID_HLCOLOR) {
                        g_s.highlightColor = cc.rgbResult;
                        g_s.highlightMode = 1;  // eigene Farbe
                    } else {
                        g_s.customBg = g_s.bg;
                        g_s.customFg = g_s.fg;
                        (id == ID_BG ? g_s.customBg : g_s.customFg) = cc.rgbResult;
                        g_s.theme = THEME_CUSTOM;  // eigene Farbe → Design "Eigene Farben"
                    }
                    Commit();
                    SyncControls(w);
                }
            }
            break;
        case ID_LOGOBROWSE:
            if (code == BN_CLICKED) {
                wchar_t file[MAX_PATH] = L"";
                OPENFILENAMEW ofn{sizeof(ofn)};
                ofn.hwndOwner = w;
                ofn.lpstrFilter = g_de ? L"Bilder (*.png;*.ico;*.jpg;*.bmp)\0*.png;*.ico;*.jpg;*.jpeg;*.bmp\0Alle Dateien\0*.*\0"
                                       : L"Images (*.png;*.ico;*.jpg;*.bmp)\0*.png;*.ico;*.jpg;*.jpeg;*.bmp\0All files\0*.*\0";
                ofn.lpstrFile = file;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
                if (GetOpenFileNameW(&ofn)) {
                    g_s.logoMode = 2;
                    g_s.logoPath = file;
                    Commit();
                    SyncControls(w);
                }
            }
            break;
        case ID_ORDERUP:
        case ID_ORDERDOWN:
            if (code == BN_CLICKED) {
                int sel = (int)SendDlgItemMessageW(w, ID_ORDERLIST, LB_GETCURSEL, 0, 0);
                int to = id == ID_ORDERUP ? sel - 1 : sel + 1;
                if (sel >= 0 && to >= 0 && to < (int)g_order.size()) {
                    std::swap(g_order[sel], g_order[to]);
                    SaveOrder();
                    Commit();
                    FillOrderList(w, to);
                }
                SetFocus(GetDlgItem(w, ID_ORDERLIST));
            }
            break;
        case ID_ORDERRESET:
            if (code == BN_CLICKED) {
                g_s.order = L"";
                Commit();
                FillOrderList(w, -1);
            }
            break;
        case ID_RESET:
            if (code == BN_CLICKED &&
                MessageBoxW(w, T(L"Alle Einstellungen auf Standard zurücksetzen?"), T(L"Zurücksetzen"),
                            MB_YESNO | MB_ICONQUESTION) == IDYES) {
                bool darkBefore = g_uc.dark;
                SetDefaults(g_s);
                Commit();
                SyncControls(w);
                if (SettingsDark() != darkBefore) PostMessageW(w, WM_SETTINGS_REBUILD, 0, 0);
            }
            break;
        case IDCANCEL:
            DestroyWindow(w);
            break;
        }
        return 0;
    }
    case WM_SETTINGS_REBUILD:
        // Hell/Dunkel gewechselt → Fenster mit neuen Farben neu aufbauen
        DestroyWindow(w);
        ShowSettings();
        return 0;
    case WM_DESTROY:
        g_settingsWnd = nullptr;
        g_uiCtls.clear();
        for (HFONT* f : {&g_uiFont, &g_uiFontBold, &g_uiFontTitle, &g_uiFontSmall, &g_uiIcon})
            if (*f) { DeleteObject(*f); *f = nullptr; }
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

void ShowSettings() {
    if (g_settingsWnd) {
        ShowWindow(g_settingsWnd, SW_RESTORE);
        SetForegroundWindow(g_settingsWnd);
        return;
    }
    HINSTANCE inst = (HINSTANCE)&__ImageBase;
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    g_settingsWnd = CreateWindowExW(0, kSettingsClass, T(L"Menüleiste – Einstellungen"), style,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
                                    nullptr, nullptr, inst, nullptr);
    if (!g_settingsWnd) return;

    UINT dpi = GetDpiForWindow(g_settingsWnd);
    RECT r{0, 0, g_settingsClientW, g_settingsClientH};
    AdjustWindowRectExForDpi(&r, style, FALSE, 0, dpi);
    int ww = r.right - r.left, wh = r.bottom - r.top;

    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
    int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2;
    int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2;
    if (y < mi.rcWork.top) y = mi.rcWork.top;
    SetWindowPos(g_settingsWnd, nullptr, x, y, ww, wh, SWP_NOZORDER);

    SyncControls(g_settingsWnd);
    ShowWindow(g_settingsWnd, SW_SHOW);
    SetForegroundWindow(g_settingsWnd);
}

// ======================================================================
// Kleine Helfer für Menüs
// ======================================================================
void OpenUri(const wchar_t* uri) {
    ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
}

void Run(const wchar_t* file, const wchar_t* args = nullptr, int show = SW_SHOWNORMAL) {
    ShellExecuteW(nullptr, L"open", file, args, nullptr, show);
}

bool Confirm(const std::wstring& text, const wchar_t* title) {
    return MessageBoxW(nullptr, T(text).c_str(), T(title),
                       MB_YESNO | MB_ICONQUESTION | MB_TOPMOST | MB_SETFOREGROUND) == IDYES;
}

std::wstring ProcessPathOf(HWND w) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return L"";
    wchar_t path[MAX_PATH];
    DWORD n = MAX_PATH;
    BOOL ok = QueryFullProcessImageNameW(p, 0, path, &n);
    CloseHandle(p);
    return ok ? std::wstring(path) : std::wstring();
}

bool IsShellWindow(HWND w) {
    wchar_t cls[64] = L"";
    GetClassNameW(w, cls, 64);
    return !wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") ||
           !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd");
}

std::wstring FmtUptime() {
    ULONGLONG secs = GetTickCount64() / 1000;
    int d = (int)(secs / 86400), hh = (int)((secs % 86400) / 3600), mm = (int)((secs % 3600) / 60);
    wchar_t t[48];
    if (d > 0) swprintf(t, 48, T(L"%d Tage, %d Std."), d, hh);
    else       swprintf(t, 48, T(L"%d Std., %d Min."), hh, mm);
    return t;
}

void SystemSleep() {
    typedef BOOLEAN(WINAPI * SetSuspendState_t)(BOOLEAN, BOOLEAN, BOOLEAN);
    HMODULE m = LoadLibraryW(L"powrprof.dll");
    if (!m) return;
    auto f = (SetSuspendState_t)GetProcAddress(m, "SetSuspendState");
    if (f) f(FALSE, FALSE, FALSE);
    FreeLibrary(m);
}

void SetVolume(float v) {
    IAudioEndpointVolume* ep = GetEndpointVolume();
    if (!ep) return;
    ep->SetMasterVolumeLevelScalar(v, nullptr);
    if (v > 0.005f) ep->SetMute(FALSE, nullptr);
    ep->Release();
    g_st.volume = v;
}

// ---------- Audiogeräte ----------
static const PROPERTYKEY kPKEY_FriendlyName =
    {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};
static const GUID kCLSID_PolicyConfig =
    {0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};
static const GUID kIID_IPolicyConfig =
    {0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

// Undokumentiertes Interface zum Umschalten des Standard-Ausgabegeräts
struct IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};

struct AudioDev { std::wstring id, name; bool def = false; };

std::vector<AudioDev> ListAudioDevices() {
    std::vector<AudioDev> out;
    if (!g_audioEnum) return out;
    std::wstring defId;
    IMMDevice* def = nullptr;
    if (SUCCEEDED(g_audioEnum->GetDefaultAudioEndpoint(eRender, eConsole, &def)) && def) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(def->GetId(&id)) && id) { defId = id; CoTaskMemFree(id); }
        def->Release();
    }
    IMMDeviceCollection* col = nullptr;
    if (FAILED(g_audioEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col)) || !col)
        return out;
    UINT n = 0;
    col->GetCount(&n);
    for (UINT i = 0; i < n; i++) {
        IMMDevice* d = nullptr;
        if (FAILED(col->Item(i, &d)) || !d) continue;
        AudioDev ad;
        LPWSTR id = nullptr;
        if (SUCCEEDED(d->GetId(&id)) && id) { ad.id = id; CoTaskMemFree(id); }
        IPropertyStore* ps = nullptr;
        if (SUCCEEDED(d->OpenPropertyStore(STGM_READ, &ps)) && ps) {
            PROPVARIANT pv;
            PropVariantInit(&pv);
            if (SUCCEEDED(ps->GetValue(kPKEY_FriendlyName, &pv)) && pv.vt == VT_LPWSTR && pv.pwszVal)
                ad.name = pv.pwszVal;
            PropVariantClear(&pv);
            ps->Release();
        }
        if (ad.name.empty()) ad.name = L"Audiogerät";
        ad.def = !defId.empty() && ad.id == defId;
        out.push_back(ad);
        d->Release();
    }
    col->Release();
    return out;
}

void SetDefaultAudio(const std::wstring& id) {
    IPolicyConfig* pc = nullptr;
    if (SUCCEEDED(CoCreateInstance(kCLSID_PolicyConfig, nullptr, CLSCTX_ALL, kIID_IPolicyConfig,
                                   (void**)&pc)) && pc) {
        pc->SetDefaultEndpoint(id.c_str(), eConsole);
        pc->SetDefaultEndpoint(id.c_str(), eMultimedia);
        pc->SetDefaultEndpoint(id.c_str(), eCommunications);
        pc->Release();
    }
}

// ---------- WLAN ----------
bool EnsureWlan() {
    if (!g_wlan) {
        DWORD ver = 0;
        if (WlanOpenHandle(2, nullptr, &ver, &g_wlan) != ERROR_SUCCESS) g_wlan = nullptr;
    }
    return g_wlan != nullptr;
}

void WifiScan() {
    if (!EnsureWlan()) return;
    PWLAN_INTERFACE_INFO_LIST list = nullptr;
    if (WlanEnumInterfaces(g_wlan, nullptr, &list) == ERROR_SUCCESS && list) {
        for (DWORD i = 0; i < list->dwNumberOfItems; i++)
            WlanScan(g_wlan, &list->InterfaceInfo[i].InterfaceGuid, nullptr, nullptr, nullptr);
        WlanFreeMemory(list);
    }
}

std::wstring SsidStr(const DOT11_SSID& s) {
    if (!s.uSSIDLength) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.ucSSID, (int)s.uSSIDLength, nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, (LPCCH)s.ucSSID, (int)s.uSSIDLength, &w[0], n);
    return w;
}

wchar_t WifiGlyph(int q) {
    return q >= 75 ? 0xE701 : q >= 50 ? 0xE874 : q >= 25 ? 0xE873 : 0xE872;
}

// ======================================================================
// Systemtray mitlesen
// ======================================================================
#ifndef NIF_GUID
#define NIF_GUID 0x00000020
#endif
#ifndef NIS_HIDDEN
#define NIS_HIDDEN 0x00000001
#endif
#ifndef NIM_SETVERSION
#define NIM_SETVERSION 0x00000004
#endif
#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif

// Format, in dem Shell_NotifyIcon die Daten per WM_COPYDATA an die Taskleiste schickt
struct NID32 {
    DWORD cbSize;
    DWORD hWnd;
    UINT uID;
    UINT uFlags;
    UINT uCallbackMessage;
    DWORD hIcon;
    WCHAR szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    WCHAR szInfo[256];
    UINT uVersion;
    WCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID guidItem;
    DWORD hBalloonIcon;
};
struct TRAYDATA32 { DWORD dwSignature; DWORD dwMessage; NID32 nid; };

struct TrayIcon {
    HWND hwnd = nullptr;
    UINT uID = 0;
    bool hasGuid = false;
    GUID guid{};
    UINT cbMsg = 0;
    UINT version = 0;
    HICON icon = nullptr;
    std::wstring tip;
    bool hidden = false;
};

struct TrayRef { HWND hwnd; UINT uID; UINT cbMsg; UINT version; };

SRWLOCK g_trayLock = SRWLOCK_INIT;
std::vector<TrayIcon> g_tray;
HHOOK g_trayHook = nullptr;
HWND g_trayWnd = nullptr;

static int FindTrayIdx(HWND hw, UINT id, bool hasGuid, const GUID& g) {
    for (size_t i = 0; i < g_tray.size(); i++) {
        const TrayIcon& t = g_tray[i];
        if (hasGuid && t.hasGuid) {
            if (IsEqualGUID(t.guid, g)) return (int)i;
        } else if (t.hwnd == hw && t.uID == id) {
            return (int)i;
        }
    }
    return -1;
}

void HandleTrayData(const BYTE* data, DWORD cb) {
    const DWORD hdr = (DWORD)offsetof(TRAYDATA32, nid);
    if (cb < hdr + offsetof(NID32, szTip)) return;
    const TRAYDATA32* d = (const TRAYDATA32*)data;
    const NID32& n = d->nid;
    DWORD avail = cb - hdr;
    auto has = [&](size_t off, size_t sz) { return avail >= off + sz; };
    HWND hw = (HWND)(UINT_PTR)n.hWnd;
    bool hasGuid = (n.uFlags & NIF_GUID) && has(offsetof(NID32, guidItem), sizeof(GUID));
    GUID g = hasGuid ? n.guidItem : GUID{};

    AcquireSRWLockExclusive(&g_trayLock);
    int idx = FindTrayIdx(hw, n.uID, hasGuid, g);
    switch (d->dwMessage) {
    case NIM_ADD:
    case NIM_MODIFY: {
        if (idx < 0) {
            g_tray.push_back(TrayIcon{});
            idx = (int)g_tray.size() - 1;
        }
        TrayIcon& t = g_tray[idx];
        if (hw) t.hwnd = hw;
        t.uID = n.uID;
        if (hasGuid) { t.hasGuid = true; t.guid = g; }
        if (n.uFlags & NIF_MESSAGE) t.cbMsg = n.uCallbackMessage;
        if (n.uFlags & NIF_ICON) {
            HICON c = n.hIcon ? CopyIcon((HICON)(UINT_PTR)n.hIcon) : nullptr;
            if (t.icon) DestroyIcon(t.icon);
            t.icon = c;
        }
        if ((n.uFlags & NIF_TIP) && has(offsetof(NID32, szTip), sizeof(n.szTip)))
            t.tip.assign(n.szTip, wcsnlen(n.szTip, 128));
        if ((n.uFlags & NIF_STATE) && has(offsetof(NID32, dwStateMask), sizeof(DWORD)) &&
            (n.dwStateMask & NIS_HIDDEN))
            t.hidden = (n.dwState & NIS_HIDDEN) != 0;
        break;
    }
    case NIM_DELETE:
        if (idx >= 0) {
            if (g_tray[idx].icon) DestroyIcon(g_tray[idx].icon);
            g_tray.erase(g_tray.begin() + idx);
        }
        break;
    case NIM_SETVERSION:
        if (idx >= 0 && has(offsetof(NID32, uVersion), sizeof(UINT)))
            g_tray[idx].version = n.uVersion;
        break;
    }
    ReleaseSRWLockExclusive(&g_trayLock);
}

LRESULT CALLBACK TrayCallWndProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION) {
        const CWPSTRUCT* cwp = (const CWPSTRUCT*)lp;
        if (cwp && cwp->message == WM_COPYDATA && cwp->hwnd == g_trayWnd) {
            const COPYDATASTRUCT* cds = (const COPYDATASTRUCT*)cwp->lParam;
            if (cds && cds->dwData == 1 && cds->lpData)
                HandleTrayData((const BYTE*)cds->lpData, cds->cbData);
        }
    }
    return CallNextHookEx(nullptr, code, wp, lp);
}

void InstallTrayHook() {
    if (g_trayHook || !g_trayWnd) return;
    DWORD tid = GetWindowThreadProcessId(g_trayWnd, nullptr);
    g_trayHook = SetWindowsHookExW(WH_CALLWNDPROC, TrayCallWndProc, nullptr, tid);
    // Apps bitten, ihre Tray-Symbole neu zu melden, damit die Leiste sie kennt
    if (g_trayHook)
        PostMessageW(HWND_BROADCAST, RegisterWindowMessageW(L"TaskbarCreated"), 0, 0);
}

void RemoveTrayHook() {
    if (g_trayHook) {
        UnhookWindowsHookEx(g_trayHook);
        g_trayHook = nullptr;
        Sleep(50);
    }
    AcquireSRWLockExclusive(&g_trayLock);
    for (auto& t : g_tray)
        if (t.icon) DestroyIcon(t.icon);
    g_tray.clear();
    ReleaseSRWLockExclusive(&g_trayLock);
}

// nur unter (Shared-)Lock aufrufen
void VisibleTray(std::vector<const TrayIcon*>& out) {
    for (auto& t : g_tray)
        if (t.icon && !t.hidden && t.hwnd && IsWindow(t.hwnd)) out.push_back(&t);
}

int VisibleTrayCount() {
    std::vector<const TrayIcon*> v;
    AcquireSRWLockShared(&g_trayLock);
    VisibleTray(v);
    ReleaseSRWLockShared(&g_trayLock);
    return (int)v.size();
}

// Klick an die App weiterleiten (button: 0 = links, 1 = rechts, 2 = Doppelklick)
void TrayForward(const TrayRef& t, int button, POINT anchor) {
    if (!t.cbMsg || !IsWindow(t.hwnd)) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(t.hwnd, &pid);
    AllowSetForegroundWindow(pid);
    auto send = [&](UINT m) {
        if (t.version >= 4)
            PostMessageW(t.hwnd, t.cbMsg, MAKEWPARAM((WORD)anchor.x, (WORD)anchor.y),
                         MAKELPARAM(m, t.uID));
        else
            PostMessageW(t.hwnd, t.cbMsg, t.uID, m);
    };
    if (button == 0) {
        send(WM_LBUTTONDOWN);
        send(WM_LBUTTONUP);
        if (t.version >= 4) send(NIN_SELECT);
    } else if (button == 1) {
        send(WM_RBUTTONDOWN);
        send(WM_RBUTTONUP);
        if (t.version >= 4) send(WM_CONTEXTMENU);
    } else {
        send(WM_LBUTTONDBLCLK);
        send(WM_LBUTTONUP);
    }
}

// ======================================================================
// Dropdown-Menüs
// ======================================================================
enum RowType { ROW_HEADER, ROW_ITEM, ROW_SEP, ROW_SLIDER, ROW_BAR, ROW_INFO, ROW_BIG, ROW_CUSTOM };

struct Row {
    RowType type = ROW_ITEM;
    std::wstring text, right;
    wchar_t glyph = 0, rightGlyph = 0;
    bool checked = false, enabled = true, keepOpen = false;
    float value = 0;
    COLORREF color = CLR_INVALID;
    int height = 0;  // nur ROW_CUSTOM (DIP)
    bool noCard = false;  // Gruppe nicht als Karte zeichnen (z. B. Kacheln)
    std::function<void()> onClick;
    std::function<void(float)> onChange;
    std::function<void(HDC, RECT, POINT, bool)> paint;
    std::function<int(RECT, POINT, int)> click;  // 0 = offen lassen, 1 = schließen, 2 = neu aufbauen
};

static const wchar_t* kPopupClass = L"WhTopMenuBarPopup";
std::vector<Row> g_rows;
std::vector<RECT> g_rowRects;
int g_popupW = 260;
int g_anchorL = 0, g_anchorR = 0;
POINT g_mouse{-1, -1};
bool g_mouseIn = false;
int g_dragRow = -1;
int g_calOffset = 0;
Action g_hoverAct = ACT_NONE;
bool g_barTracking = false;
ULONGLONG g_outsideSince = 0;
HWND g_lastFg = nullptr;

struct PopColors { COLORREF bg, fg, dim, faint, sep, accent, hover, onHover; bool dark, hoverOn; };

PopColors GetPopColors() {
    PopColors c;
    int lum = (GetRValue(g_s.bg) * 299 + GetGValue(g_s.bg) * 587 + GetBValue(g_s.bg) * 114) / 1000;
    c.dark = lum < 128;
    c.bg = c.dark ? Blend(g_s.bg, RGB(255, 255, 255), 90) : Blend(g_s.bg, RGB(0, 0, 0), 96);
    // Menüs: Schrift und Symbole immer reinweiß bzw. reinschwarz
    c.fg = c.dark ? RGB(255, 255, 255) : RGB(0, 0, 0);
    c.dim = c.fg;
    c.faint = Blend(c.fg, c.bg, 45);  // nur für Deaktiviertes / andere Monate
    c.sep = Blend(c.fg, c.bg, 18);
    c.accent = HighlightColor();  // Regler, Balken, heutiger Tag
    c.hoverOn = HighlightOn();
    c.hover = c.accent;
    c.onHover = TextOn(Blend(c.hover, c.bg, g_s.highlightOpacity));
    return c;
}

void Txt(HDC dc, const std::wstring& s, RECT r, HFONT f, COLORREF col, UINT fmt) {
    SelectObject(dc, f);
    SetTextColor(dc, col);
    DrawTextW(dc, s.c_str(), -1, &r, fmt | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void Glyph(HDC dc, wchar_t g, RECT r, HFONT f, COLORREF col) {
    wchar_t s[2] = {g, 0};
    SelectObject(dc, f);
    SetTextColor(dc, col);
    DrawTextW(dc, s, 1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

void FillEllipse(HDC dc, RECT r, COLORREF col, COLORREF border) {
    HBRUSH b = CreateSolidBrush(col);
    HPEN p = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, r.left, r.top, r.right, r.bottom);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

// ---------- Zeilen-Bausteine ----------
Row Header(const std::wstring& t) { Row r; r.type = ROW_HEADER; r.text = t; return r; }
Row Sep() { Row r; r.type = ROW_SEP; return r; }
Row Info(const std::wstring& t, const std::wstring& v) { Row r; r.type = ROW_INFO; r.text = t; r.right = v; return r; }
Row Big(const std::wstring& t, const std::wstring& v = L"") { Row r; r.type = ROW_BIG; r.text = t; r.right = v; return r; }
Row Item(const std::wstring& t, std::function<void()> fn, wchar_t glyph = 0, const std::wstring& right = L"") {
    Row r;
    r.type = ROW_ITEM;
    r.text = t;
    r.onClick = fn;
    r.glyph = glyph;
    r.right = right;
    return r;
}
Row Bar(const std::wstring& t, const std::wstring& v, float value, COLORREF col = CLR_INVALID) {
    Row r;
    r.type = ROW_BAR;
    r.text = t;
    r.right = v;
    r.value = value < 0 ? 0 : (value > 1 ? 1 : value);
    r.color = col;
    return r;
}

// Kachel-/Karten-Design für alle Stile außer Windows 11
bool PopCards() { return g_s.style != 0; }

int RowH(const Row& r) {
    switch (r.type) {
    case ROW_HEADER: return 26;
    case ROW_ITEM:   return 28;
    case ROW_SEP:    return PopCards() ? 6 : 9;
    case ROW_SLIDER: return 34;
    case ROW_BAR:    return r.text.empty() ? 18 : 38;
    case ROW_INFO:   return 24;
    case ROW_BIG:    return 46;
    case ROW_CUSTOM: return r.height;
    }
    return 28;
}

bool RowClickable(const Row& r) {
    return r.enabled && r.onClick && (r.type == ROW_ITEM || r.type == ROW_BAR);
}

int RowAt(POINT p) {
    for (size_t i = 0; i < g_rowRects.size(); i++)
        if (PtInRect(&g_rowRects[i], p)) return (int)i;
    return -1;
}

void LayoutPopup(UINT dpi, int& outW, int& outH) {
    int w = MulDiv(g_popupW, dpi, 96);
    bool cards = PopCards();
    int edge = MulDiv(cards ? 8 : 6, dpi, 96);
    int pad = cards ? MulDiv(5, dpi, 96) : 0;  // Innenabstand der Karten
    int y = edge;
    bool inGroup = false;
    g_rowRects.clear();
    for (auto& r : g_rows) {
        if (r.type == ROW_SEP) {
            if (inGroup) y += pad;
            inGroup = false;
        } else if (!inGroup) {
            y += pad;
            inGroup = true;
        }
        int h = MulDiv(RowH(r), dpi, 96);
        g_rowRects.push_back({0, y, w, y + h});
        y += h;
    }
    if (inGroup) y += pad;
    outW = w;
    outH = y + edge;
}

void SliderTrack(RECT rr, UINT dpi, int& l, int& r) {
    int extra = PopCards() ? 8 : 0;
    l = rr.left + MulDiv(42 + extra, dpi, 96);
    r = rr.right - MulDiv(58 + extra, dpi, 96);
}

// ---------- Kalender ----------
static ULONGLONG StToU(const SYSTEMTIME& st) {
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    return FtToU(ft);
}
static SYSTEMTIME UToSt(ULONGLONG u) {
    FILETIME ft{(DWORD)u, (DWORD)(u >> 32)};
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    return st;
}
static const ULONGLONG kDay = 864000000000ULL;

void CalendarMonth(int& y, int& m) {
    SYSTEMTIME now;
    GetLocalTime(&now);
    int idx = now.wYear * 12 + (now.wMonth - 1) + g_calOffset;
    y = idx / 12;
    m = idx % 12 + 1;
}

void PaintCalendar(HDC dc, RECT rc, POINT ms, bool in) {
    UINT dpi = ScaledDpi(g_popup);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    PopColors c = GetPopColors();
    int left = rc.left + S(14), right = rc.right - S(14);
    int colW = (right - left) / 8;
    int y, m;
    CalendarMonth(y, m);

    // Kopfzeile: ‹ Monat Jahr ›
    SYSTEMTIME first{};
    first.wYear = (WORD)y;
    first.wMonth = (WORD)m;
    first.wDay = 1;
    first = UToSt(StToU(first));  // füllt wDayOfWeek
    wchar_t title[64] = L"";
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &first, L"MMMM yyyy", title, 64, nullptr);
    RECT hr{left, rc.top, right, rc.top + S(30)};
    RECT la{left, hr.top + S(3), left + S(28), hr.bottom - S(3)};
    RECT ra{right - S(28), hr.top + S(3), right, hr.bottom - S(3)};
    bool hotL = c.hoverOn && in && PtInRect(&la, ms);
    bool hotR = c.hoverOn && in && PtInRect(&ra, ms);
    if (hotL) HiFill(la, S(6));
    if (hotR) HiFill(ra, S(6));
    Glyph(dc, 0xE76B, la, g_iconSmall, hotL ? c.onHover : c.fg);
    Glyph(dc, 0xE76C, ra, g_iconSmall, hotR ? c.onHover : c.fg);
    Txt(dc, title, hr, g_fontBold, c.fg, DT_CENTER);

    // Wochentage
    int wy = hr.bottom;
    RECT kw{left, wy, left + colW, wy + S(22)};
    Txt(dc, L"KW", kw, g_fontBold, c.fg, DT_CENTER);
    for (int i = 0; i < 7; i++) {
        wchar_t dn[16] = L"";
        GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SSHORTESTDAYNAME1 + i, dn, 16);
        RECT r{left + (i + 1) * colW, wy, left + (i + 2) * colW, wy + S(22)};
        Txt(dc, dn, r, g_font, c.dim, DT_CENTER);
    }

    // Tage
    SYSTEMTIME today;
    GetLocalTime(&today);
    int offset = (first.wDayOfWeek + 6) % 7;
    ULONGLONG start = StToU(first) - (ULONGLONG)offset * kDay;
    int rowH = S(28);
    int gy = wy + S(22);
    for (int row = 0; row < 6; row++) {
        SYSTEMTIME mon = UToSt(start + (ULONGLONG)(row * 7) * kDay);
        RECT kr{left, gy + row * rowH, left + colW, gy + (row + 1) * rowH};
        Txt(dc, std::to_wstring(IsoWeek(mon)), kr, g_font, c.fg, DT_CENTER);
        for (int col = 0; col < 7; col++) {
            SYSTEMTIME d = UToSt(start + (ULONGLONG)(row * 7 + col) * kDay);
            RECT cr{left + (col + 1) * colW, gy + row * rowH, left + (col + 2) * colW, gy + (row + 1) * rowH};
            bool isToday = d.wYear == today.wYear && d.wMonth == today.wMonth && d.wDay == today.wDay;
            COLORREF tc = d.wMonth == m ? c.fg : c.faint;
            if (isToday) {
                int sz = (rowH < colW ? rowH : colW) - S(4);
                int cx = (cr.left + cr.right) / 2, cy = (cr.top + cr.bottom) / 2;
                RECT er{cx - sz / 2, cy - sz / 2, cx + sz / 2, cy + sz / 2};
                FillEllipse(dc, er, c.accent, c.accent);
                tc = TextOn(c.accent);
            }
            Txt(dc, std::to_wstring(d.wDay), cr, isToday ? g_fontBold : g_font, tc, DT_CENTER);
        }
    }
}

Row CalendarRow() {
    Row r;
    r.type = ROW_CUSTOM;
    r.height = 30 + 22 + 6 * 28 + 4;
    r.paint = [](HDC dc, RECT rc, POINT m, bool in) { PaintCalendar(dc, rc, m, in); };
    r.click = [](RECT rc, POINT p, int) -> int {
        UINT dpi = ScaledDpi(g_popup);
        if (p.y > rc.top + MulDiv(30, dpi, 96)) return 0;
        if (p.x < rc.left + MulDiv(50, dpi, 96)) g_calOffset--;
        else if (p.x > rc.right - MulDiv(50, dpi, 96)) g_calOffset++;
        else g_calOffset = 0;
        return 2;
    };
    return r;
}

// ---------- Tray-Raster ----------
void PaintTray(HDC dc, RECT rc, POINT ms, bool in, int cols) {
    UINT dpi = ScaledDpi(g_popup);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    PopColors c = GetPopColors();
    int cell = S(40), left = rc.left + S(14), top = rc.top + S(2);
    int ico = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    std::wstring tip = T(L"Klick: öffnen  ·  Rechtsklick: Menü");

    AcquireSRWLockShared(&g_trayLock);
    std::vector<const TrayIcon*> vis;
    VisibleTray(vis);
    for (size_t i = 0; i < vis.size(); i++) {
        int cx = left + (int)(i % cols) * cell, cy = top + (int)(i / cols) * cell;
        RECT cr{cx + S(2), cy + S(2), cx + cell - S(2), cy + cell - S(2)};
        if (in && PtInRect(&cr, ms)) {
            if (c.hoverOn) HiFill(cr, S(8));
            if (!vis[i]->tip.empty()) tip = vis[i]->tip;
        }
        DrawIconEx(dc, cx + (cell - ico) / 2, cy + (cell - ico) / 2, vis[i]->icon, ico, ico, 0,
                   nullptr, DI_NORMAL);
    }
    ReleaseSRWLockShared(&g_trayLock);

    for (auto& ch : tip)
        if (ch == L'\n' || ch == L'\r') ch = L' ';
    RECT tr{left, rc.bottom - S(26), rc.right - S(14), rc.bottom};
    Txt(dc, tip, tr, g_font, c.dim, DT_LEFT);
}

int ClickTray(RECT rc, POINT p, int btn, int cols) {
    UINT dpi = ScaledDpi(g_popup);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    int cell = S(40), left = rc.left + S(14), top = rc.top + S(2);
    if (p.x < left || p.y < top) return 0;
    int col = (p.x - left) / cell, row = (p.y - top) / cell;
    if (col >= cols) return 0;
    size_t idx = (size_t)(row * cols + col);

    TrayRef ref{};
    bool found = false;
    AcquireSRWLockShared(&g_trayLock);
    std::vector<const TrayIcon*> vis;
    VisibleTray(vis);
    if (idx < vis.size()) {
        ref = {vis[idx]->hwnd, vis[idx]->uID, vis[idx]->cbMsg, vis[idx]->version};
        found = true;
    }
    ReleaseSRWLockShared(&g_trayLock);
    if (!found) return 0;

    RECT wr;
    GetWindowRect(g_popup, &wr);
    POINT anchor{wr.left + left + col * cell + cell / 2, wr.top + top + row * cell + cell / 2};
    TrayForward(ref, btn, anchor);
    if (btn == 0) {
        // kurz offen lassen, damit ein Doppelklick noch ankommt
        SetTimer(g_popup, 1, 350, nullptr);
        return 0;
    }
    return 1;
}

// ---------- Menüinhalte ----------
void BuildLogoMenu(std::vector<Row>& rows) {
    g_popupW = 270;
    rows.push_back(Item(L"Über diesen PC", [] { OpenUri(L"ms-settings:about"); }, 0xE946));
    rows.push_back(Sep());
    rows.push_back(Item(L"Systemeinstellungen…", [] { OpenUri(L"ms-settings:"); }, 0xE713));
    rows.push_back(Item(L"Menüleiste einstellen…", [] { ShowSettings(); }, 0xE771));
    rows.push_back(Item(L"Task-Manager", [] { Run(L"taskmgr.exe"); }, 0xE9D9, L"Strg+Umschalt+Esc"));
    rows.push_back(Sep());
    rows.push_back(Item(L"Energie sparen", [] { SystemSleep(); }, 0xE708));
    rows.push_back(Item(L"Neu starten…", [] {
        if (Confirm(L"Möchtest du den Computer jetzt neu starten?", L"Neu starten"))
            Run(L"shutdown.exe", L"/r /t 0", SW_HIDE);
    }, 0xE72C));
    rows.push_back(Item(L"Herunterfahren…", [] {
        if (Confirm(L"Möchtest du den Computer jetzt herunterfahren?", L"Herunterfahren"))
            Run(L"shutdown.exe", L"/s /hybrid /t 0", SW_HIDE);
    }, 0xE7E8));
    rows.push_back(Sep());
    rows.push_back(Item(L"Bildschirm sperren", [] { LockWorkStation(); }, 0xE72E, L"Win+L"));
    rows.push_back(Item(L"Abmelden…", [] {
        if (Confirm(L"Möchtest du dich jetzt abmelden?", L"Abmelden")) ExitWindowsEx(EWX_LOGOFF, 0);
    }, 0xE77B));
}

void BuildAppMenu(std::vector<Row>& rows) {
    g_popupW = 270;
    HWND w = g_lastFg;
    bool ok = w && IsWindow(w) && !IsShellWindow(w);
    rows.push_back(Header(ok && !g_appName.empty() ? g_appName : L"Desktop"));
    if (!ok) {
        rows.push_back(Item(L"Datei-Explorer öffnen", [] { Run(L"explorer.exe"); }, 0xE838));
        rows.push_back(Item(L"Alle Fenster anzeigen", [] { PressCombo({VK_LWIN, VK_TAB}); }, 0, L"Win+Tab"));
        return;
    }
    rows.push_back(Item(L"Minimieren", [w] { ShowWindow(w, SW_MINIMIZE); }, 0xE921));
    bool zoomed = IsZoomed(w) != FALSE;
    rows.push_back(Item(zoomed ? L"Wiederherstellen" : L"Maximieren", [w, zoomed] {
        ShowWindow(w, zoomed ? SW_RESTORE : SW_MAXIMIZE);
        SetForegroundWindow(w);
    }, zoomed ? 0xE923 : 0xE922));
    bool top = (GetWindowLongPtrW(w, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    Row t = Item(L"Immer im Vordergrund", [w, top] {
        SetWindowPos(w, top ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }, 0xE718);
    t.checked = top;
    t.keepOpen = true;
    rows.push_back(t);
    rows.push_back(Item(L"Alle Fenster anzeigen", [] { PressCombo({VK_LWIN, VK_TAB}); }, 0, L"Win+Tab"));

    std::wstring path = ProcessPathOf(w);
    if (!path.empty()) {
        rows.push_back(Sep());
        rows.push_back(Item(L"Dateispeicherort öffnen", [path] {
            std::wstring a = L"/select,\"" + path + L"\"";
            Run(L"explorer.exe", a.c_str());
        }, 0xE838));
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Fenster schließen", [w] { PostMessageW(w, WM_SYSCOMMAND, SC_CLOSE, 0); }, 0xE8BB, L"Alt+F4"));
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    std::wstring name = g_appName;
    Row k = Item(L"Sofort beenden…", [pid, name] {
        wchar_t q[512];
        swprintf(q, 512, T(L"„%s“ sofort beenden?\nNicht gespeicherte Daten gehen verloren."), name.c_str());
        if (!Confirm(q,
                     L"Sofort beenden"))
            return;
        HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (p) { TerminateProcess(p, 1); CloseHandle(p); }
    });
    k.enabled = pid != GetCurrentProcessId();
    rows.push_back(k);
}

void BuildClockMenu(std::vector<Row>& rows) {
    g_popupW = 300;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t t[64] = L"", d[128] = L"";
    GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, L"HH:mm:ss", t, 64);
    GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, L"dddd, d. MMMM yyyy", d, 128, nullptr);
    rows.push_back(Big(t, T(std::wstring(L"KW")) + L" " + std::to_wstring(IsoWeek(st))));
    rows.push_back(Info(d, L""));
    rows.push_back(Sep());
    rows.push_back(CalendarRow());
    rows.push_back(Sep());
    rows.push_back(Item(L"Benachrichtigungen", [] { PressCombo({VK_LWIN, 'N'}); }, 0xEA8F, L"Win+N"));
    rows.push_back(Item(L"Datum und Uhrzeit…", [] { OpenUri(L"ms-settings:dateandtime"); }, 0xE787));
}

void BuildBatteryMenu(std::vector<Row>& rows) {
    g_popupW = 280;
    rows.push_back(Header(L"Akku"));
    SYSTEM_POWER_STATUS sps{};
    GetSystemPowerStatus(&sps);
    if (!g_st.hasBattery) {
        rows.push_back(Info(L"Kein Akku gefunden", L""));
    } else {
        std::wstring state = g_st.charging ? (g_st.battery >= 100 ? L"Voll geladen" : L"Wird geladen")
                                           : L"Akkubetrieb";
        rows.push_back(Big(std::to_wstring(g_st.battery) + L" %", state));
        COLORREF col = (!g_st.charging && g_st.battery <= 20) ? RGB(255, 69, 58) : RGB(48, 209, 88);
        rows.push_back(Bar(L"", L"", g_st.battery / 100.f, col));
        if (!g_st.charging && g_st.batteryTime != (DWORD)-1) {
            wchar_t b[32];
            swprintf(b, 32, T(L"%d:%02d Std."), (int)(g_st.batteryTime / 3600),
                     (int)((g_st.batteryTime % 3600) / 60));
            rows.push_back(Info(L"Restlaufzeit", b));
        }
        rows.push_back(Info(L"Energiequelle", g_st.charging ? L"Netzteil" : L"Akku"));
        rows.push_back(Info(L"Energiesparmodus", sps.SystemStatusFlag ? L"Ein" : L"Aus"));
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Energiesparmodus…", [] { OpenUri(L"ms-settings:batterysaver"); }, 0xE713));
    rows.push_back(Item(L"Netzbetrieb und Energiesparen…", [] { OpenUri(L"ms-settings:powersleep"); }, 0xE7E8));
}

void BuildVolumeMenu(std::vector<Row>& rows) {
    g_popupW = 300;
    rows.push_back(Header(L"Ton"));
    if (!g_st.hasAudio) {
        rows.push_back(Info(L"Kein Ausgabegerät", L""));
    } else {
        Row s;
        s.type = ROW_SLIDER;
        s.value = g_st.volume;
        s.glyph = (g_st.muted || g_st.volume < 0.005f) ? 0xE74F
                  : g_st.volume < 0.34f ? 0xE993 : g_st.volume < 0.67f ? 0xE994 : 0xE995;
        s.right = std::to_wstring((int)(g_st.volume * 100 + 0.5f)) + L" %";
        s.onChange = [](float v) { SetVolume(v); };
        rows.push_back(s);
        Row m = Item(L"Stumm", [] { ToggleMute(); UpdateStatus(); }, 0xE74F);
        m.checked = g_st.muted;
        m.keepOpen = true;
        rows.push_back(m);
    }
    auto devs = ListAudioDevices();
    if (!devs.empty()) {
        rows.push_back(Sep());
        rows.push_back(Header(L"Ausgabegerät"));
        for (auto& d : devs) {
            std::wstring id = d.id;
            Row r = Item(d.name, [id] { SetDefaultAudio(id); UpdateStatus(); }, 0xE7F5);
            r.checked = d.def;
            r.keepOpen = true;
            rows.push_back(r);
        }
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Lautstärkemixer…", [] { OpenUri(L"ms-settings:apps-volume"); }, 0xE767));
    rows.push_back(Item(L"Soundeinstellungen…", [] { OpenUri(L"ms-settings:sound"); }, 0xE713));
}

void BuildWifiMenu(std::vector<Row>& rows) {
    g_popupW = 300;
    bool anyItf = false;
    PWLAN_INTERFACE_INFO_LIST list = nullptr;
    if (EnsureWlan() && WlanEnumInterfaces(g_wlan, nullptr, &list) == ERROR_SUCCESS && list &&
        list->dwNumberOfItems > 0) {
        anyItf = true;
        auto& itf = list->InterfaceInfo[0];
        GUID guid = itf.InterfaceGuid;
        rows.push_back(Header(L"WLAN"));
        std::wstring connected;
        if (itf.isState == wlan_interface_state_connected) {
            DWORD sz = 0;
            PWLAN_CONNECTION_ATTRIBUTES conn = nullptr;
            if (WlanQueryInterface(g_wlan, &guid, wlan_intf_opcode_current_connection, nullptr, &sz,
                                   (PVOID*)&conn, nullptr) == ERROR_SUCCESS && conn) {
                connected = SsidStr(conn->wlanAssociationAttributes.dot11Ssid);
                int q = (int)conn->wlanAssociationAttributes.wlanSignalQuality;
                WlanFreeMemory(conn);
                Row r = Item(connected, nullptr, WifiGlyph(q), L"Verbunden");
                rows.push_back(r);
                rows.push_back(Item(L"Trennen", [guid] { WlanDisconnect(g_wlan, &guid, nullptr); }));
            }
        } else {
            rows.push_back(Info(L"Nicht verbunden", L""));
        }

        PWLAN_AVAILABLE_NETWORK_LIST nets = nullptr;
        if (WlanGetAvailableNetworkList(g_wlan, &guid, 0, nullptr, &nets) == ERROR_SUCCESS && nets) {
            struct Net { std::wstring ssid, profile; int q; bool secure; DOT11_BSS_TYPE bss; };
            std::vector<Net> v;
            for (DWORD k = 0; k < nets->dwNumberOfItems; k++) {
                auto& n = nets->Network[k];
                std::wstring s = SsidStr(n.dot11Ssid);
                if (s.empty() || s == connected) continue;
                std::wstring prof = n.strProfileName;
                bool merged = false;
                for (auto& e : v) {
                    if (e.ssid != s) continue;
                    if ((int)n.wlanSignalQuality > e.q) e.q = (int)n.wlanSignalQuality;
                    if (e.profile.empty()) e.profile = prof;
                    merged = true;
                    break;
                }
                if (!merged)
                    v.push_back({s, prof, (int)n.wlanSignalQuality, n.bSecurityEnabled != FALSE,
                                 n.dot11BssType});
            }
            WlanFreeMemory(nets);
            std::sort(v.begin(), v.end(), [](const Net& a, const Net& b) { return a.q > b.q; });
            if (v.size() > 10) v.resize(10);
            if (!v.empty()) {
                rows.push_back(Sep());
                rows.push_back(Header(L"Andere Netzwerke"));
            }
            for (auto& n : v) {
                std::wstring prof = n.profile;
                DOT11_BSS_TYPE bss = n.bss;
                Row r = Item(n.ssid, [guid, prof, bss] {
                    if (prof.empty()) {
                        OpenUri(L"ms-availablenetworks:");  // Passwort-Eingabe von Windows
                        return;
                    }
                    WLAN_CONNECTION_PARAMETERS p{};
                    p.wlanConnectionMode = wlan_connection_mode_profile;
                    p.strProfile = prof.c_str();
                    p.dot11BssType = bss;
                    WlanConnect(g_wlan, &guid, &p, nullptr);
                }, WifiGlyph(n.q));
                if (n.secure) r.rightGlyph = 0xE72E;
                rows.push_back(r);
            }
        }
    }
    if (list) WlanFreeMemory(list);

    if (!anyItf) {
        rows.push_back(Header(L"Netzwerk"));
        rows.push_back(Info(L"Status", g_st.online ? L"Verbunden (LAN)" : L"Nicht verbunden"));
    }
    rows.push_back(Sep());
    if (anyItf) rows.push_back(Item(L"Weitere Netzwerke…", [] { OpenUri(L"ms-availablenetworks:"); }));
    rows.push_back(Item(L"Netzwerkeinstellungen…", [] { OpenUri(L"ms-settings:network"); }, 0xE713));
}

void BuildKbdMenu(std::vector<Row>& rows) {
    g_popupW = 290;
    rows.push_back(Header(L"Eingabequelle"));
    HKL list[32];
    int n = GetKeyboardLayoutList(32, list);
    HWND target = g_lastFg;
    DWORD tid = target ? GetWindowThreadProcessId(target, nullptr) : 0;
    HKL cur = GetKeyboardLayout(tid);
    for (int i = 0; i < n; i++) {
        LANGID lang = LOWORD((UINT_PTR)list[i]);
        wchar_t loc[LOCALE_NAME_MAX_LENGTH] = L"", name[128] = L"", iso[16] = L"";
        LCIDToLocaleName(MAKELCID(lang, SORT_DEFAULT), loc, LOCALE_NAME_MAX_LENGTH, 0);
        GetLocaleInfoEx(loc, LOCALE_SLOCALIZEDDISPLAYNAME, name, 128);
        GetLocaleInfoEx(loc, LOCALE_SISO639LANGNAME, iso, 16);
        for (wchar_t* c = iso; *c; c++) *c = towupper(*c);
        HKL h = list[i];
        Row r = Item(name[0] ? name : loc, [h, target] {
            if (target && IsWindow(target))
                PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)h);
        }, 0xE765, iso);
        r.checked = h == cur;
        rows.push_back(r);
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Nächste Eingabequelle", [] { PressCombo({VK_LWIN, VK_SPACE}); }, 0, L"Win+Leertaste"));
    rows.push_back(Item(L"Spracheinstellungen…", [] { OpenUri(L"ms-settings:regionlanguage"); }, 0xE774));
}

void BuildSystemMenu(std::vector<Row>& rows) {
    g_popupW = 320;
    rows.push_back(Header(L"System"));
    rows.push_back(Bar(L"CPU", std::to_wstring(g_st.cpu) + L" %", g_st.cpu / 100.f,
                       g_st.cpu >= 90 ? RGB(255, 69, 58) : CLR_INVALID));
    MEMORYSTATUSEX ms{sizeof(ms)};
    GlobalMemoryStatusEx(&ms);
    rows.push_back(Bar(L"Arbeitsspeicher",
                       FmtSize(ms.ullTotalPhys - ms.ullAvailPhys) + T(L" von ") + FmtSize(ms.ullTotalPhys),
                       ms.dwMemoryLoad / 100.f, ms.dwMemoryLoad >= 90 ? RGB(255, 69, 58) : CLR_INVALID));
    rows.push_back(Info(L"Netzwerk", L"↓ " + FmtRate(g_st.down) + L"    ↑ " + FmtRate(g_st.up)));
    rows.push_back(Info(L"Laufzeit", FmtUptime()));
    rows.push_back(Sep());
    rows.push_back(Header(L"Laufwerke"));

    DWORD oldMode = 0;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, &oldMode);
    wchar_t drives[256] = L"";
    GetLogicalDriveStringsW(255, drives);
    for (wchar_t* d = drives; *d; d += wcslen(d) + 1) {
        UINT t = GetDriveTypeW(d);
        if (t != DRIVE_FIXED && t != DRIVE_REMOVABLE) continue;
        ULARGE_INTEGER fr{}, tot{};
        if (!GetDiskFreeSpaceExW(d, &fr, &tot, nullptr) || !tot.QuadPart) continue;
        wchar_t label[MAX_PATH] = L"";
        GetVolumeInformationW(d, label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
        std::wstring root = d;
        std::wstring name = root.substr(0, 2);
        if (label[0]) name += std::wstring(L"  ") + label;
        float used = 1.f - (float)((double)fr.QuadPart / (double)tot.QuadPart);
        Row r = Bar(name, FmtSize(fr.QuadPart) + T(L" frei von ") + FmtSize(tot.QuadPart), used,
                    used > 0.9f ? RGB(255, 69, 58) : CLR_INVALID);
        r.onClick = [root] { ShellExecuteW(nullptr, L"open", root.c_str(), nullptr, nullptr, SW_SHOWNORMAL); };
        rows.push_back(r);
    }
    SetThreadErrorMode(oldMode, nullptr);

    rows.push_back(Sep());
    rows.push_back(Item(L"Task-Manager", [] { Run(L"taskmgr.exe"); }, 0xE9D9, L"Strg+Umschalt+Esc"));
    rows.push_back(Item(L"Ressourcenmonitor", [] { Run(L"resmon.exe"); }));
}

void BuildTrayMenu(std::vector<Row>& rows) {
    int n = VisibleTrayCount();
    int cols = n < 4 ? 4 : (n > 6 ? 6 : n);
    g_popupW = cols * 40 + 28;
    if (g_popupW < 250) g_popupW = 250;
    cols = (g_popupW - 28) / 40;
    rows.push_back(Header(L"Tray-Symbole"));
    if (!g_trayHook) {
        rows.push_back(Info(L"Tray konnte nicht gelesen werden", L""));
    } else if (n == 0) {
        rows.push_back(Info(L"Keine Symbole vorhanden", L""));
    } else {
        int gridRows = (n + cols - 1) / cols;
        Row g;
        g.type = ROW_CUSTOM;
        g.height = gridRows * 40 + 30;
        g.paint = [cols](HDC dc, RECT rc, POINT m, bool in) { PaintTray(dc, rc, m, in, cols); };
        g.click = [cols](RECT rc, POINT p, int btn) -> int { return ClickTray(rc, p, btn, cols); };
        rows.push_back(g);
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Taskleisten-Einstellungen…", [] { OpenUri(L"ms-settings:taskbar"); }, 0xE713));
}

// ---------- Kacheln (Kontrollzentrum im Karten-Design) ----------
struct TileDef {
    int x, y, w, h;   // DIP, relativ zur Zeile
    int kind;         // 0 = breit mit Icon-Kreis, 1 = rund, 2 = Medien
    wchar_t glyph;
    std::wstring title, sub;
    bool active;
    std::function<void()> act;
    int result;       // 0 = offen lassen, 1 = schließen, 2 = neu aufbauen
};

RECT TileRect(const TileDef& t, RECT rc, UINT dpi) {
    return {rc.left + MulDiv(t.x, dpi, 96), rc.top + MulDiv(t.y, dpi, 96),
            rc.left + MulDiv(t.x + t.w, dpi, 96), rc.top + MulDiv(t.y + t.h, dpi, 96)};
}

void PaintTiles(HDC dc, RECT rc, POINT ms, bool in, const std::vector<TileDef>& tiles) {
    UINT dpi = ScaledDpi(g_popup);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    PopColors c = GetPopColors();
    for (auto& t : tiles) {
        RECT r = TileRect(t, rc, dpi);
        int h = r.bottom - r.top;
        int rad = t.kind == 1 ? h : S(36);  // Durchmesser der Rundung
        CardFill(r, rad);
        bool hot = c.hoverOn && in && PtInRect(&r, ms) && t.kind != 2;
        if (hot) HiFill(r, rad);
        COLORREF fg = hot ? c.onHover : c.fg;
        if (t.kind == 1) {
            Glyph(dc, t.glyph, r, g_iconFont, fg);
            continue;
        }
        if (t.kind == 0) {
            int d = S(32);
            RECT cr{r.left + S(12), (r.top + r.bottom - d) / 2, r.left + S(12) + d, (r.top + r.bottom + d) / 2};
            if (t.active) {
                Gdiplus::Graphics g(dc);
                SetupGraphics(g);
                Gdiplus::SolidBrush wb(Gdiplus::Color(255, 255, 255, 255));
                g.FillEllipse(&wb, (float)cr.left, (float)cr.top, (float)d, (float)d);
                Glyph(dc, t.glyph, cr, g_iconSmall, HighlightColor());
            } else {
                CardFill(cr, d);
                Glyph(dc, t.glyph, cr, g_iconSmall, fg);
            }
            int mid = (r.top + r.bottom) / 2;
            Txt(dc, T(t.title), {cr.right + S(10), r.top + S(6), r.right - S(8), mid + S(2)}, g_fontBold, fg, DT_LEFT);
            Txt(dc, T(t.sub), {cr.right + S(10), mid, r.right - S(8), r.bottom - S(8)}, g_fontSmall, fg, DT_LEFT);
            continue;
        }
        // Medien: Titel oben, drei Knöpfe unten
        Txt(dc, T(t.title), {r.left + S(14), r.top + S(10), r.right - S(10), r.top + S(32)}, g_fontBold, c.fg, DT_LEFT);
        Txt(dc, T(t.sub), {r.left + S(14), r.top + S(32), r.right - S(10), r.top + S(50)}, g_fontSmall, c.fg, DT_LEFT);
        static const wchar_t mg[3] = {0xE892, 0xE768, 0xE893};  // Zurück, Play/Pause, Weiter
        int bw = (r.right - r.left - S(12)) / 3;
        for (int k = 0; k < 3; k++) {
            RECT br{r.left + S(6) + k * bw, r.bottom - S(46), r.left + S(6) + (k + 1) * bw, r.bottom - S(8)};
            bool bh = c.hoverOn && in && PtInRect(&br, ms);
            if (bh) HiFill(br, S(24));
            Glyph(dc, mg[k], br, k == 1 ? g_iconFont : g_iconSmall, bh ? c.onHover : c.fg);
        }
    }
}

Row TileRow(int height, std::vector<TileDef> tiles) {
    auto tl = std::make_shared<std::vector<TileDef>>(std::move(tiles));
    Row r;
    r.type = ROW_CUSTOM;
    r.height = height;
    r.noCard = true;
    r.paint = [tl](HDC dc, RECT rc, POINT m, bool in) { PaintTiles(dc, rc, m, in, *tl); };
    r.click = [tl](RECT rc, POINT p, int) -> int {
        UINT dpi = ScaledDpi(g_popup);
        for (auto& t : *tl) {
            RECT tr = TileRect(t, rc, dpi);
            if (!PtInRect(&tr, p)) continue;
            if (t.kind == 2) {
                if (p.y < tr.bottom - MulDiv(46, dpi, 96)) return 0;
                int third = (p.x - tr.left) * 3 / (tr.right - tr.left);
                WORD keys[3] = {VK_MEDIA_PREV_TRACK, VK_MEDIA_PLAY_PAUSE, VK_MEDIA_NEXT_TRACK};
                PressCombo({keys[third < 0 ? 0 : (third > 2 ? 2 : third)]});
                return 0;
            }
            if (t.act) t.act();
            return t.result;
        }
        return 0;
    };
    return r;
}

Row Spacer(int h) {
    Row r;
    r.type = ROW_CUSTOM;
    r.height = h;
    r.noCard = true;
    return r;
}

std::wstring CurrentSsid() {
    std::wstring ssid;
    PWLAN_INTERFACE_INFO_LIST list = nullptr;
    if (!EnsureWlan() || WlanEnumInterfaces(g_wlan, nullptr, &list) != ERROR_SUCCESS || !list) return ssid;
    for (DWORD i = 0; i < list->dwNumberOfItems && ssid.empty(); i++) {
        auto& itf = list->InterfaceInfo[i];
        if (itf.isState != wlan_interface_state_connected) continue;
        DWORD sz = 0;
        PWLAN_CONNECTION_ATTRIBUTES conn = nullptr;
        if (WlanQueryInterface(g_wlan, &itf.InterfaceGuid, wlan_intf_opcode_current_connection, nullptr, &sz,
                               (PVOID*)&conn, nullptr) == ERROR_SUCCESS && conn) {
            ssid = SsidStr(conn->wlanAssociationAttributes.dot11Ssid);
            WlanFreeMemory(conn);
        }
    }
    WlanFreeMemory(list);
    return ssid;
}

void BuildControlMenuCards(std::vector<Row>& rows) {
    g_popupW = 330;
    std::wstring ssid = CurrentSsid();
    std::wstring wsub = g_st.wifi ? (ssid.empty() ? L"Verbunden" : ssid) : (g_st.online ? L"LAN" : L"Aus");
    rows.push_back(TileRow(130, {
        {10, 0, 150, 60, 0, 0xE701, L"WLAN", wsub, g_st.wifi || g_st.online,
         [] { g_popupAct = ACT_WIFI; WifiScan(); }, 2},
        {10, 70, 150, 60, 0, 0xE702, L"Bluetooth",
         !g_st.btRadio ? L"Aus" : g_st.btConnected ? std::to_wstring(g_st.btConnected) + T(L" verbunden") : L"Ein",
         g_st.btRadio, [] { g_popupAct = ACT_BLUETOOTH; }, 2},
        {170, 0, 150, 130, 2, 0, L"Medien", L"Wiedergabe steuern", false, nullptr, 0},
    }));
    rows.push_back(Spacer(10));
    rows.push_back(TileRow(60, {
        {10, 0, 150, 60, 0, 0xE72E, L"Sperren", L"Win+L", false, [] { LockWorkStation(); }, 1},
        {170, 0, 60, 60, 1, 0xE722, L"", L"", false, [] { PressCombo({VK_LWIN, VK_SHIFT, 'S'}); }, 1},
        {260, 0, 60, 60, 1, 0xE7F4, L"", L"", false, [] { PressCombo({VK_LWIN, 'P'}); }, 1},
    }));
    rows.push_back(Spacer(10));
    rows.push_back(TileRow(60, {
        {10, 0, 60, 60, 1, 0xE706, L"", L"", false, [] { OpenUri(L"ms-settings:nightlight"); }, 1},
        {100, 0, 60, 60, 1, 0xE713, L"", L"", false, [] { OpenUri(L"ms-settings:"); }, 1},
        {170, 0, 150, 60, 0, 0xEA8F, L"Mitteilungen", L"Win+N", false, [] { PressCombo({VK_LWIN, 'N'}); }, 1},
    }));
    if (g_st.hasAudio) {
        rows.push_back(Sep());
        rows.push_back(Header(L"Ton"));
        Row sl;
        sl.type = ROW_SLIDER;
        sl.value = g_st.volume;
        sl.glyph = (g_st.muted || g_st.volume < 0.005f) ? 0xE74F
                   : g_st.volume < 0.34f ? 0xE993 : g_st.volume < 0.67f ? 0xE994 : 0xE995;
        sl.right = std::to_wstring((int)(g_st.volume * 100 + 0.5f)) + L" %";
        sl.onChange = [](float v) { SetVolume(v); };
        rows.push_back(sl);
        std::wstring dev;
        for (auto& d : ListAudioDevices())
            if (d.def) dev = d.name;
        Row o = Item(L"Ausgabegerät", [] { g_popupAct = ACT_VOLUME; }, 0xE7F5, dev);
        o.keepOpen = true;
        rows.push_back(o);
    }
    if (g_st.hasBattery) {
        rows.push_back(Sep());
        COLORREF col = (!g_st.charging && g_st.battery <= 20) ? RGB(255, 69, 58) : RGB(48, 209, 88);
        rows.push_back(Bar(g_st.charging ? L"Akku (lädt)" : L"Akku", std::to_wstring(g_st.battery) + L" %",
                           g_st.battery / 100.f, col));
    }
}

void BuildControlMenu(std::vector<Row>& rows) {
    if (PopCards()) {
        BuildControlMenuCards(rows);
        return;
    }
    g_popupW = 300;
    rows.push_back(Header(L"Kontrollzentrum"));
    std::wstring wl = g_st.wifi ? L"Verbunden" : (g_st.online ? L"LAN" : L"Getrennt");
    rows.push_back(Item(L"WLAN", [] { OpenUri(L"ms-availablenetworks:"); }, 0xE701, wl));
    rows.push_back(Item(L"Bluetooth", [] { OpenUri(L"ms-settings:bluetooth"); }, 0xE702));
    rows.push_back(Item(L"Benachrichtigungen", [] { PressCombo({VK_LWIN, 'N'}); }, 0xEA8F, L"Win+N"));
    if (g_st.hasAudio) {
        rows.push_back(Sep());
        rows.push_back(Header(L"Ton"));
        Row sl;
        sl.type = ROW_SLIDER;
        sl.value = g_st.volume;
        sl.glyph = (g_st.muted || g_st.volume < 0.005f) ? 0xE74F
                   : g_st.volume < 0.34f ? 0xE993 : g_st.volume < 0.67f ? 0xE994 : 0xE995;
        sl.right = std::to_wstring((int)(g_st.volume * 100 + 0.5f)) + L" %";
        sl.onChange = [](float v) { SetVolume(v); };
        rows.push_back(sl);
        Row m = Item(L"Stumm", [] { ToggleMute(); UpdateStatus(); }, 0xE74F);
        m.checked = g_st.muted;
        m.keepOpen = true;
        rows.push_back(m);
    }
    if (g_st.hasBattery) {
        rows.push_back(Sep());
        COLORREF col = (!g_st.charging && g_st.battery <= 20) ? RGB(255, 69, 58) : RGB(48, 209, 88);
        rows.push_back(Bar(g_st.charging ? L"Akku (lädt)" : L"Akku", std::to_wstring(g_st.battery) + L" %",
                           g_st.battery / 100.f, col));
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Bildschirm sperren", [] { LockWorkStation(); }, 0xE72E, L"Win+L"));
    rows.push_back(Item(L"Windows-Schnelleinstellungen", [] { PressCombo({VK_LWIN, 'A'}); }, 0xE713, L"Win+A"));
}

// ---------- Bluetooth ----------
struct BtDev { std::wstring name; bool connected; ULONG cod; };

std::vector<BtDev> ListBtDevices() {
    std::vector<BtDev> v;
    BLUETOOTH_DEVICE_SEARCH_PARAMS sp{sizeof(sp)};
    sp.fReturnAuthenticated = TRUE;
    sp.fReturnRemembered = TRUE;
    sp.fReturnConnected = TRUE;
    sp.fReturnUnknown = FALSE;
    sp.fIssueInquiry = FALSE;
    BLUETOOTH_DEVICE_INFO di{sizeof(di)};
    HBLUETOOTH_DEVICE_FIND df = BluetoothFindFirstDevice(&sp, &di);
    if (!df) return v;
    do {
        if (di.fRemembered || di.fAuthenticated || di.fConnected) {
            std::wstring n = di.szName[0] ? di.szName : L"Unbekanntes Gerät";
            bool dup = false;
            for (auto& e : v)
                if (e.name == n) { e.connected = e.connected || di.fConnected; dup = true; }
            if (!dup) v.push_back({n, di.fConnected != FALSE, di.ulClassofDevice});
        }
        di.dwSize = sizeof(di);
    } while (BluetoothFindNextDevice(df, &di));
    BluetoothFindDeviceClose(df);
    std::sort(v.begin(), v.end(), [](const BtDev& a, const BtDev& b) {
        return a.connected != b.connected ? a.connected : a.name < b.name;
    });
    return v;
}

// passendes Symbol zur Geräteklasse
wchar_t BtGlyph(ULONG cod) {
    int major = (cod >> 8) & 0x1F, minor = (cod >> 2) & 0x3F;
    if (major == 0x04) return (minor == 0x01 || minor == 0x02 || minor == 0x06) ? 0xE7F6 : 0xE7F5;  // Kopfhörer/Lautsprecher
    if (major == 0x05) return (minor & 0x10) ? 0xE765 : (minor & 0x20) ? 0xE962 : 0xE7FC;          // Tastatur/Maus/Controller
    if (major == 0x02) return 0xE8EA;                                                              // Telefon
    if (major == 0x01) return 0xE7F8;                                                              // Computer
    return 0xE702;
}

void BuildBluetoothMenu(std::vector<Row>& rows) {
    g_popupW = 300;
    rows.push_back(Header(L"Bluetooth"));
    if (!g_st.btRadio) {
        rows.push_back(Info(L"Bluetooth ist aus oder nicht vorhanden", L""));
    } else {
        auto devs = ListBtDevices();
        if (devs.empty()) rows.push_back(Info(L"Keine gekoppelten Geräte", L""));
        for (auto& d : devs) {
            Row r = Item(d.name, [] { OpenUri(L"ms-settings:bluetooth"); }, BtGlyph(d.cod),
                         d.connected ? L"Verbunden" : L"Gekoppelt");
            r.checked = d.connected;
            rows.push_back(r);
        }
    }
    rows.push_back(Sep());
    rows.push_back(Item(L"Gerät hinzufügen…", [] { OpenUri(L"ms-settings-connectabledevices:devicediscovery"); }, 0xE710));
    rows.push_back(Item(L"Bluetooth-Einstellungen…", [] { OpenUri(L"ms-settings:bluetooth"); }, 0xE713));
}

// ---------- App-Menüs ----------
std::vector<WORD> g_pendingKeys;
HWND g_pendingTarget = nullptr;

void PressKeys(const std::vector<WORD>& keys) {
    std::vector<INPUT> in;
    for (WORD k : keys) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = k;
        in.push_back(i);
    }
    for (size_t n = keys.size(); n-- > 0;) {
        INPUT i{};
        i.type = INPUT_KEYBOARD;
        i.ki.wVk = keys[n];
        i.ki.dwFlags = KEYEVENTF_KEYUP;
        in.push_back(i);
    }
    SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

// App nach vorne holen und kurz danach ein Tastenkürzel senden
void SendKeysTo(HWND w, std::vector<WORD> keys) {
    if (w && IsWindow(w)) SetForegroundWindow(w);
    g_pendingKeys = std::move(keys);
    g_pendingTarget = w;
    SetTimer(g_hwnd, TIMER_KEYS, 90, nullptr);
}

std::wstring CleanMenuText(const std::wstring& s, std::wstring* accel) {
    std::wstring t, a;
    size_t tab = s.find(L'\t');
    std::wstring main = tab == std::wstring::npos ? s : s.substr(0, tab);
    if (tab != std::wstring::npos) a = s.substr(tab + 1);
    for (size_t i = 0; i < main.size(); i++) {
        if (main[i] == L'&') {
            if (i + 1 < main.size() && main[i + 1] == L'&') { t += L'&'; i++; }
            continue;
        }
        t += main[i];
    }
    if (accel) *accel = a;
    return t;
}

// Titel der App-Menüs für das aktive Fenster bestimmen
bool IsTaskbarWindow(HWND w) {
    wchar_t cls[64] = L"";
    GetClassNameW(w, cls, 64);
    return !wcscmp(cls, L"Shell_TrayWnd") || !wcscmp(cls, L"Shell_SecondaryTrayWnd");
}

bool IsExplorerWindow(HWND w) {
    wchar_t cls[64] = L"";
    GetClassNameW(w, cls, 64);
    return !wcscmp(cls, L"CabinetWClass") || !wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW");
}

bool IsDesktopWindow(HWND w) {
    wchar_t cls[64] = L"";
    GetClassNameW(w, cls, 64);
    return !wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW");
}

void UpdateAppMenuTitles(HWND w) {
    g_appMenuTitles.clear();
    g_appMenuNative = false;
    g_appMenuExplorer = false;
    g_appMenuHwnd = w;
    if (!w || !IsWindow(w)) return;
    if (IsExplorerWindow(w)) {
        // Desktop und Explorer: Menüs wie beim Finder
        g_appMenuExplorer = true;
        g_appMenuTitles = {L"Datei", L"Bearbeiten", L"Ansicht", L"Gehe zu", L"Fenster", L"Hilfe"};
        return;
    }
    if (IsShellWindow(w)) return;
    HMENU m = GetMenu(w);
    int n = m && IsMenu(m) ? GetMenuItemCount(m) : 0;
    if (n > 0) {
        for (int i = 0; i < n && i < 12; i++) {
            wchar_t buf[128] = L"";
            GetMenuStringW(m, i, buf, 128, MF_BYPOSITION);
            std::wstring t = CleanMenuText(buf, nullptr);
            if (!t.empty()) g_appMenuTitles.push_back(t);
        }
        if (!g_appMenuTitles.empty()) {
            g_appMenuNative = true;
            g_appMenuNativeCount = (int)g_appMenuTitles.size();
            g_appMenuTitles.push_back(L"Gehe zu");  // immer vorhanden
            return;
        }
    }
    g_appMenuTitles = {L"Datei", L"Bearbeiten", L"Ansicht", L"Gehe zu", L"Fenster", L"Hilfe"};
}

void AddNativeMenuItems(std::vector<Row>& rows, HWND w, HMENU sub, int depth) {
    int n = GetMenuItemCount(sub);
    for (int i = 0; i < n; i++) {
        MENUITEMINFOW mii{sizeof(mii)};
        mii.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU;
        if (!GetMenuItemInfoW(sub, i, TRUE, &mii)) continue;
        if (mii.fType & MFT_SEPARATOR) {
            if (!rows.empty() && rows.back().type != ROW_SEP) rows.push_back(Sep());
            continue;
        }
        wchar_t buf[256] = L"";
        GetMenuStringW(sub, i, buf, 256, MF_BYPOSITION);
        std::wstring accel;
        std::wstring text = CleanMenuText(buf, &accel);
        if (text.empty()) continue;
        if (mii.hSubMenu) {
            if (depth > 0) continue;  // tiefere Untermenüs werden nicht angezeigt
            if (!rows.empty() && rows.back().type != ROW_SEP) rows.push_back(Sep());
            rows.push_back(Header(text));
            AddNativeMenuItems(rows, w, mii.hSubMenu, depth + 1);
            if (!rows.empty() && rows.back().type != ROW_SEP) rows.push_back(Sep());
            continue;
        }
        UINT id = mii.wID;
        Row r = Item(text, [w, id] {
            if (IsWindow(w)) {
                SetForegroundWindow(w);
                PostMessageW(w, WM_COMMAND, MAKEWPARAM(id, 0), 0);
            }
        }, 0, accel);
        r.enabled = !(mii.fState & (MFS_DISABLED | MFS_GRAYED));
        r.checked = (mii.fState & MFS_CHECKED) != 0;
        rows.push_back(r);
    }
}

// Fenster einer App (gleicher Prozess bzw. alle Explorer-Fenster) für das Fenster-Menü
struct WinEntry { HWND hwnd; std::wstring title; };
struct WinEnumCtx { DWORD pid; bool explorer; std::vector<WinEntry>* out; };

BOOL CALLBACK EnumAppWinProc(HWND h, LPARAM lp) {
    auto ctx = (WinEnumCtx*)lp;
    if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
    if (GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    wchar_t title[256] = L"";
    GetWindowTextW(h, title, 256);
    if (!title[0]) return TRUE;
    if (ctx->explorer) {
        wchar_t cls[64] = L"";
        GetClassNameW(h, cls, 64);
        if (wcscmp(cls, L"CabinetWClass")) return TRUE;
    } else {
        DWORD pid = 0;
        GetWindowThreadProcessId(h, &pid);
        if (pid != ctx->pid) return TRUE;
    }
    ctx->out->push_back({h, title});
    return TRUE;
}

std::vector<WinEntry> ListAppWindows(HWND w, bool explorer) {
    std::vector<WinEntry> v;
    WinEnumCtx ctx{0, explorer, &v};
    GetWindowThreadProcessId(w, &ctx.pid);
    EnumWindows(EnumAppWinProc, (LPARAM)&ctx);
    if (v.size() > 12) v.resize(12);
    return v;
}

void BringToFront(HWND h) {
    if (!IsWindow(h)) return;
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    SetForegroundWindow(h);
}

void OpenShellFolder(const wchar_t* target) {
    ShellExecuteW(nullptr, L"open", L"explorer.exe", target, nullptr, SW_SHOWNORMAL);
}

// "Gehe zu": Navigation und schnelle Ordner – für alle Apps
void BuildGoMenu(std::vector<Row>& rows, HWND w) {
    bool desk = IsDesktopWindow(w);
    bool exp = !desk && IsExplorerWindow(w);
    auto key = [w](const wchar_t* t, std::vector<WORD> k, const wchar_t* acc) {
        return Item(t, [w, k] { SendKeysTo(w, k); }, 0, acc);
    };
    if (!desk) {
        rows.push_back(key(L"Zurück", {VK_MENU, VK_LEFT}, L"Alt+\u2190"));
        rows.push_back(key(L"Vorwärts", {VK_MENU, VK_RIGHT}, L"Alt+\u2192"));
        if (exp) rows.push_back(key(L"Übergeordneter Ordner", {VK_MENU, VK_UP}, L"Alt+\u2191"));
        rows.push_back(Sep());
    }
    struct Place { const wchar_t* name; const wchar_t* target; wchar_t glyph; };
    static const Place places[] = {
        {L"Startseite", L"shell:::{679f85cb-0220-4080-b29b-5540cc05aab6}", 0xE80F},
        {L"Desktop", L"shell:Desktop", 0xE7F4},
        {L"Dokumente", L"shell:Personal", 0xE8A5},
        {L"Downloads", L"shell:Downloads", 0xE896},
        {L"Bilder", L"shell:My Pictures", 0xEB9F},
        {L"Musik", L"shell:My Music", 0xE8D6},
        {L"Videos", L"shell:My Video", 0xE714},
        {L"Dieser PC", L"shell:MyComputerFolder", 0xE7F8},
        {L"Netzwerk", L"shell:NetworkPlacesFolder", 0xE968},
        {L"Papierkorb", L"shell:RecycleBinFolder", 0xE74D},
    };
    for (auto& pl : places) {
        const wchar_t* t = pl.target;
        rows.push_back(Item(pl.name, [t] { OpenShellFolder(t); }, pl.glyph));
    }
    rows.push_back(Sep());
    if (exp) rows.push_back(key(L"Gehe zu Ordner…", {VK_CONTROL, 'L'}, L"Strg+L"));
    else rows.push_back(Item(L"Gehe zu Ordner…", [] { PressCombo({VK_LWIN, 'R'}); }, 0, L"Win+R"));
}

// Menüs für Desktop und Explorer-Fenster (wie beim Finder)
void BuildExplorerMenu(std::vector<Row>& rows, HWND w, int idx) {
    bool desk = IsDesktopWindow(w);
    auto key = [w](const wchar_t* t, std::vector<WORD> k, const wchar_t* acc, wchar_t g = 0) {
        return Item(t, [w, k] { SendKeysTo(w, k); }, g, acc);
    };
    switch (idx) {
    case 0: {  // Datei
        rows.push_back(Item(L"Neues Fenster", [] { OpenShellFolder(L"shell:MyComputerFolder"); }, 0xE8A7, L"Win+E"));
        rows.push_back(key(L"Neuer Ordner", {VK_CONTROL, VK_SHIFT, 'N'}, L"Strg+Umschalt+N", 0xE8F4));
        rows.push_back(Sep());
        rows.push_back(key(L"Öffnen", {VK_RETURN}, L"Eingabe"));
        rows.push_back(key(L"Eigenschaften", {VK_MENU, VK_RETURN}, L"Alt+Eingabe"));
        rows.push_back(Sep());
        Row c = key(L"Fenster schließen", {VK_CONTROL, 'W'}, L"Strg+W");
        c.enabled = !desk;
        rows.push_back(c);
        rows.push_back(Item(L"Papierkorb leeren…", [] { SHEmptyRecycleBinW(nullptr, nullptr, 0); }, 0xE74D));
        break;
    }
    case 1:  // Bearbeiten
        rows.push_back(key(L"Rückgängig", {VK_CONTROL, 'Z'}, L"Strg+Z"));
        rows.push_back(key(L"Wiederholen", {VK_CONTROL, 'Y'}, L"Strg+Y"));
        rows.push_back(Sep());
        rows.push_back(key(L"Ausschneiden", {VK_CONTROL, 'X'}, L"Strg+X"));
        rows.push_back(key(L"Kopieren", {VK_CONTROL, 'C'}, L"Strg+C"));
        rows.push_back(key(L"Einfügen", {VK_CONTROL, 'V'}, L"Strg+V"));
        rows.push_back(key(L"Alles auswählen", {VK_CONTROL, 'A'}, L"Strg+A"));
        rows.push_back(Sep());
        rows.push_back(key(L"Umbenennen", {VK_F2}, L"F2"));
        rows.push_back(key(L"Löschen", {VK_DELETE}, L"Entf"));
        rows.push_back(Sep());
        if (desk) rows.push_back(Item(L"Suchen…", [] { PressCombo({VK_LWIN, 'S'}); }, 0, L"Win+S"));
        else rows.push_back(key(L"Suchen…", {VK_CONTROL, 'F'}, L"Strg+F"));
        break;
    case 2:  // Ansicht
        rows.push_back(key(L"Große Symbole", {VK_CONTROL, VK_SHIFT, '2'}, L"Strg+Umschalt+2"));
        rows.push_back(key(L"Mittelgroße Symbole", {VK_CONTROL, VK_SHIFT, '3'}, L"Strg+Umschalt+3"));
        rows.push_back(key(L"Kleine Symbole", {VK_CONTROL, VK_SHIFT, '4'}, L"Strg+Umschalt+4"));
        if (!desk) {
            rows.push_back(key(L"Liste", {VK_CONTROL, VK_SHIFT, '5'}, L"Strg+Umschalt+5"));
            rows.push_back(key(L"Details", {VK_CONTROL, VK_SHIFT, '6'}, L"Strg+Umschalt+6"));
            rows.push_back(Sep());
            rows.push_back(key(L"Vorschaufenster", {VK_MENU, 'P'}, L"Alt+P"));
            rows.push_back(key(L"Detailbereich", {VK_MENU, VK_SHIFT, 'P'}, L"Alt+Umschalt+P"));
        }
        rows.push_back(Sep());
        rows.push_back(key(L"Aktualisieren", {VK_F5}, L"F5"));
        break;
    case 3:  // Gehe zu
        BuildGoMenu(rows, w);
        break;
    case 4: {  // Fenster
        if (!desk) {
            rows.push_back(Item(L"Minimieren", [w] { ShowWindow(w, SW_MINIMIZE); }, 0));
            bool zoomed = IsZoomed(w) != FALSE;
            rows.push_back(Item(zoomed ? L"Wiederherstellen" : L"Maximieren", [w, zoomed] {
                ShowWindow(w, zoomed ? SW_RESTORE : SW_MAXIMIZE);
                SetForegroundWindow(w);
            }, 0));
            rows.push_back(Sep());
        }
        auto wins = ListAppWindows(w, true);
        if (!wins.empty()) {
            rows.push_back(Header(L"Explorer-Fenster"));
            for (auto& e : wins) {
                HWND hw = e.hwnd;
                Row r = Item(e.title, [hw] { BringToFront(hw); }, 0xE8A7);
                r.checked = hw == w;
                rows.push_back(r);
            }
            rows.push_back(Sep());
        }
        rows.push_back(Item(L"Desktop anzeigen", [] { PressCombo({VK_LWIN, 'D'}); }, 0, L"Win+D"));
        rows.push_back(Item(L"Alle Fenster anzeigen", [] { PressCombo({VK_LWIN, VK_TAB}); }, 0, L"Win+Tab"));
        break;
    }
    default:  // Hilfe
        rows.push_back(Item(L"Windows-Hilfe", [] { OpenUri(L"https://support.microsoft.com/windows"); }, 0xE897));
        rows.push_back(Item(L"Tastenkombinationen", [] {
            OpenUri(L"https://www.bing.com/search?q=Windows+11+Tastenkombinationen+Explorer");
        }, 0xE765));
        rows.push_back(Sep());
        rows.push_back(Item(L"Über Windows", [] { Run(L"winver.exe"); }));
        break;
    }
}

void BuildAppMenuIdx(std::vector<Row>& rows) {
    g_popupW = 280;
    HWND w = g_appMenuHwnd;
    int idx = g_menuIdx;
    if (!w || !IsWindow(w)) return;
    if (g_appMenuExplorer) {
        BuildExplorerMenu(rows, w, idx);
        return;
    }
    if (g_appMenuNative && idx >= g_appMenuNativeCount) {
        BuildGoMenu(rows, w);
        return;
    }
    if (g_appMenuNative) {
        HMENU m = GetMenu(w);
        HMENU sub = m ? GetSubMenu(m, idx) : nullptr;
        if (sub) {
            // App ihr Menü aktualisieren lassen (aktiv/grau, Häkchen)
            DWORD_PTR res = 0;
            SendMessageTimeoutW(w, WM_INITMENUPOPUP, (WPARAM)sub, idx, SMTO_ABORTIFHUNG, 200, &res);
            AddNativeMenuItems(rows, w, sub, 0);
            while (!rows.empty() && rows.back().type == ROW_SEP) rows.pop_back();
            if (rows.empty()) rows.push_back(Info(L"(leer)", L""));
            return;
        }
    }
    auto key = [w](const wchar_t* t, std::vector<WORD> k, const wchar_t* acc) {
        return Item(t, [w, k] { SendKeysTo(w, k); }, 0, acc);
    };
    switch (idx) {
    case 0:  // Datei
        rows.push_back(key(L"Neu", {VK_CONTROL, 'N'}, L"Strg+N"));
        rows.push_back(key(L"Öffnen…", {VK_CONTROL, 'O'}, L"Strg+O"));
        rows.push_back(key(L"Schließen", {VK_CONTROL, 'W'}, L"Strg+W"));
        rows.push_back(Sep());
        rows.push_back(key(L"Speichern", {VK_CONTROL, 'S'}, L"Strg+S"));
        rows.push_back(key(L"Speichern unter…", {VK_CONTROL, VK_SHIFT, 'S'}, L"Strg+Umschalt+S"));
        rows.push_back(Sep());
        rows.push_back(key(L"Drucken…", {VK_CONTROL, 'P'}, L"Strg+P"));
        rows.push_back(Sep());
        rows.push_back(Item(L"Beenden", [w] { PostMessageW(w, WM_SYSCOMMAND, SC_CLOSE, 0); }, 0, L"Alt+F4"));
        break;
    case 1:  // Bearbeiten
        rows.push_back(key(L"Rückgängig", {VK_CONTROL, 'Z'}, L"Strg+Z"));
        rows.push_back(key(L"Wiederholen", {VK_CONTROL, 'Y'}, L"Strg+Y"));
        rows.push_back(Sep());
        rows.push_back(key(L"Ausschneiden", {VK_CONTROL, 'X'}, L"Strg+X"));
        rows.push_back(key(L"Kopieren", {VK_CONTROL, 'C'}, L"Strg+C"));
        rows.push_back(key(L"Einfügen", {VK_CONTROL, 'V'}, L"Strg+V"));
        rows.push_back(key(L"Alles auswählen", {VK_CONTROL, 'A'}, L"Strg+A"));
        rows.push_back(Sep());
        rows.push_back(key(L"Suchen…", {VK_CONTROL, 'F'}, L"Strg+F"));
        rows.push_back(key(L"Ersetzen…", {VK_CONTROL, 'H'}, L"Strg+H"));
        break;
    case 2:  // Ansicht
        rows.push_back(key(L"Vergrößern", {VK_CONTROL, VK_OEM_PLUS}, L"Strg++"));
        rows.push_back(key(L"Verkleinern", {VK_CONTROL, VK_OEM_MINUS}, L"Strg+-"));
        rows.push_back(key(L"Originalgröße", {VK_CONTROL, '0'}, L"Strg+0"));
        rows.push_back(Sep());
        rows.push_back(key(L"Vollbild", {VK_F11}, L"F11"));
        rows.push_back(key(L"Aktualisieren", {VK_F5}, L"F5"));
        break;
    case 3:  // Gehe zu
        BuildGoMenu(rows, w);
        break;
    case 4: {  // Fenster
        rows.push_back(Item(L"Minimieren", [w] { ShowWindow(w, SW_MINIMIZE); }, 0));
        bool zoomed = IsZoomed(w) != FALSE;
        rows.push_back(Item(zoomed ? L"Wiederherstellen" : L"Maximieren", [w, zoomed] {
            ShowWindow(w, zoomed ? SW_RESTORE : SW_MAXIMIZE);
            SetForegroundWindow(w);
        }, 0));
        rows.push_back(Sep());
        rows.push_back(key(L"Vollbild", {VK_F11}, L"F11"));
        rows.push_back(Sep());
        rows.push_back(key(L"Links andocken", {VK_LWIN, VK_LEFT}, L"Win+←"));
        rows.push_back(key(L"Rechts andocken", {VK_LWIN, VK_RIGHT}, L"Win+→"));
        rows.push_back(Sep());
        bool top = (GetWindowLongPtrW(w, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        Row t = Item(L"Immer im Vordergrund", [w, top] {
            SetWindowPos(w, top ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        });
        t.checked = top;
        t.keepOpen = true;
        rows.push_back(t);
        auto wins = ListAppWindows(w, false);
        if (wins.size() > 1) {
            rows.push_back(Sep());
            for (auto& e : wins) {
                HWND hw = e.hwnd;
                Row r = Item(e.title, [hw] { BringToFront(hw); });
                r.checked = hw == w;
                rows.push_back(r);
            }
        }
        rows.push_back(Sep());
        rows.push_back(Item(L"Alle Fenster anzeigen", [] { PressCombo({VK_LWIN, VK_TAB}); }, 0, L"Win+Tab"));
        break;
    }
    default: {  // Hilfe
        std::wstring app = g_appName.empty() ? std::wstring(L"App") : g_appName;
        rows.push_back(key((g_de ? app + L"-Hilfe" : app + L" Help").c_str(), {VK_F1}, L"F1"));
        std::wstring q = L"https://www.bing.com/search?q=";
        for (wchar_t ch : app + T(L" Hilfe")) q += ch == L' ' ? L'+' : ch;
        rows.push_back(Item(L"Online-Hilfe suchen", [q] { OpenUri(q.c_str()); }, 0xE721));
        std::wstring q2 = L"https://www.bing.com/search?q=";
        for (wchar_t ch : app + T(L" Tastenkombinationen")) q2 += ch == L' ' ? L'+' : ch;
        rows.push_back(Item(L"Tastenkombinationen", [q2] { OpenUri(q2.c_str()); }, 0xE765));
        rows.push_back(Sep());
        rows.push_back(Item(L"Über Windows", [] { Run(L"winver.exe"); }));
        break;
    }
    }
}

void BuildMenu(Action a, std::vector<Row>& rows) {
    switch (a) {
    case ACT_LOGO:    BuildLogoMenu(rows); break;
    case ACT_APP:     BuildAppMenu(rows); break;
    case ACT_CLOCK:   BuildClockMenu(rows); break;
    case ACT_BATTERY: BuildBatteryMenu(rows); break;
    case ACT_VOLUME:  BuildVolumeMenu(rows); break;
    case ACT_WIFI:    BuildWifiMenu(rows); break;
    case ACT_TRAY:    BuildTrayMenu(rows); break;
    case ACT_KBD:     BuildKbdMenu(rows); break;
    case ACT_SYSTEM:  BuildSystemMenu(rows); break;
    case ACT_CONTROL: BuildControlMenu(rows); break;
    case ACT_BLUETOOTH: BuildBluetoothMenu(rows); break;
    case ACT_APPMENU: BuildAppMenuIdx(rows); break;
    default: break;
    }
}

// ---------- Popup-Fenster ----------
// Zeichnet den Menüinhalt (ohne Hintergrund) auf eine Fläche in "clear"
void DrawPopupContent(HWND w, HDC mem, RECT rc, COLORREF clear) {
    UINT dpi = ScaledDpi(w);
    auto S = [&](int v) { return MulDiv(v, dpi, 96); };
    PopColors c = GetPopColors();

    HBRUSH bg = CreateSolidBrush(clear);
    FillRect(mem, &rc, bg);
    DeleteObject(bg);
    SetBkMode(mem, TRANSPARENT);

    bool cards = PopCards();
    if (cards) {
        // jede Gruppe (zwischen Trennern) wird eine abgerundete Karte
        size_t i = 0;
        while (i < g_rows.size() && i < g_rowRects.size()) {
            if (g_rows[i].type == ROW_SEP) { i++; continue; }
            size_t j = i;
            bool skip = false;
            while (j < g_rows.size() && j < g_rowRects.size() && g_rows[j].type != ROW_SEP) {
                if (g_rows[j].noCard) skip = true;
                j++;
            }
            if (!skip) {
                RECT cr{S(8), g_rowRects[i].top - S(5), rc.right - S(8), g_rowRects[j - 1].bottom + S(5)};
                CardFill(cr, S(28));
            }
            i = j;
        }
    }
    int inset = cards ? S(22) : S(14), hin = cards ? S(12) : S(6);

    int hovered = g_mouseIn ? RowAt(g_mouse) : -1;
    for (size_t i = 0; i < g_rows.size() && i < g_rowRects.size(); i++) {
        Row r = g_rows[i];
        r.text = T(r.text);    // Übersetzung beim Zeichnen
        r.right = T(r.right);
        RECT rr = g_rowRects[i];
        int x0 = rr.left + inset, x1 = rr.right - inset;
        bool hot = (int)i == hovered && RowClickable(r);
        RECT hl{rr.left + hin, rr.top + S(1), rr.right - hin, rr.bottom - S(1)};

        switch (r.type) {
        case ROW_HEADER:
            Txt(mem, r.text, {x0, rr.top, x1, rr.bottom}, g_fontBold, c.dim, DT_LEFT);
            break;
        case ROW_SEP:
            // keine Linie – nur Abstand zwischen den Gruppen
            break;
        case ROW_ITEM: {
            hot = hot && c.hoverOn;
            if (hot) HiFill(hl, S(6));
            COLORREF tc = hot ? c.onHover : (r.enabled ? c.fg : c.faint);
            COLORREF dc = hot ? c.onHover : c.dim;
            RECT gr{x0, rr.top, x0 + S(20), rr.bottom};
            if (r.checked) Glyph(mem, 0xE73E, gr, g_iconSmall, tc);
            else if (r.glyph) Glyph(mem, r.glyph, gr, g_iconSmall, r.enabled ? (hot ? tc : c.dim) : c.faint);
            int rightEdge = x1;
            if (!r.right.empty()) {
                SelectObject(mem, g_font);
                SIZE sz{};
                GetTextExtentPoint32W(mem, r.right.c_str(), (int)r.right.size(), &sz);
                Txt(mem, r.right, {x1 - sz.cx, rr.top, x1, rr.bottom}, g_font, dc, DT_RIGHT);
                rightEdge = x1 - sz.cx - S(10);
            }
            if (r.rightGlyph) {
                Glyph(mem, r.rightGlyph, {rightEdge - S(18), rr.top, rightEdge, rr.bottom}, g_iconSmall, dc);
                rightEdge -= S(24);
            }
            Txt(mem, r.text, {x0 + S(28), rr.top, rightEdge, rr.bottom}, g_font, tc, DT_LEFT);
            break;
        }
        case ROW_INFO:
            Txt(mem, r.text, {x0, rr.top, x1, rr.bottom}, g_font, c.dim, DT_LEFT);
            if (!r.right.empty())
                Txt(mem, r.right, {x0, rr.top, x1, rr.bottom}, g_font, c.fg, DT_RIGHT);
            break;
        case ROW_BIG:
            Txt(mem, r.text, {x0, rr.top, x1, rr.bottom}, g_fontBig, c.fg, DT_LEFT);
            if (!r.right.empty())
                Txt(mem, r.right, {x0, rr.top, x1, rr.bottom}, g_font, c.dim, DT_RIGHT);
            break;
        case ROW_BAR: {
            hot = hot && c.hoverOn;
            if (hot) HiFill(hl, S(6));
            RECT bar;
            if (!r.text.empty()) {
                RECT tl{x0, rr.top + S(3), x1, rr.top + S(22)};
                Txt(mem, r.text, tl, g_font, hot ? c.onHover : c.fg, DT_LEFT);
                Txt(mem, r.right, tl, g_font, hot ? c.onHover : c.dim, DT_RIGHT);
                bar = {x0, rr.bottom - S(12), x1, rr.bottom - S(6)};
            } else {
                int mid = (rr.top + rr.bottom) / 2;
                bar = {x0, mid - S(3), x1, mid + S(3)};
            }
            FillRound(mem, bar, S(6), c.sep);
            RECT fill = bar;
            fill.right = bar.left + (int)((bar.right - bar.left) * r.value);
            if (fill.right - fill.left > S(4))
                FillRound(mem, fill, S(6), r.color != CLR_INVALID ? r.color : c.accent);
            break;
        }
        case ROW_SLIDER: {
            Glyph(mem, r.glyph, {x0, rr.top, x0 + S(22), rr.bottom}, g_iconFont, c.fg);
            int l, rt;
            SliderTrack(rr, dpi, l, rt);
            int mid = (rr.top + rr.bottom) / 2;
            RECT track{l, mid - S(2), rt, mid + S(2)};
            FillRound(mem, track, S(4), c.sep);
            int kx = l + (int)((rt - l) * r.value);
            RECT fill{l, mid - S(2), kx, mid + S(2)};
            if (kx - l > S(2)) FillRound(mem, fill, S(4), c.accent);
            RECT knob{kx - S(8), mid - S(8), kx + S(8), mid + S(8)};
            FillEllipse(mem, knob, RGB(255, 255, 255), c.sep);
            Txt(mem, r.right, {rt + S(10), rr.top, x1, rr.bottom}, g_font, c.fg, DT_RIGHT);
            break;
        }
        case ROW_CUSTOM:
            if (r.paint) r.paint(mem, rr, g_mouse, g_mouseIn);
            break;
        }
    }

    // dezenter Rahmen
    HPEN pen = CreatePen(PS_SOLID, 1, c.sep);
    HGDIOBJ op = SelectObject(mem, pen), ob = SelectObject(mem, GetStockObject(NULL_BRUSH));
    RoundRect(mem, 0, 0, rc.right, rc.bottom, S(cards ? 32 : 16), S(cards ? 32 : 16));
    SelectObject(mem, op);
    SelectObject(mem, ob);
    DeleteObject(pen);
}

void ApplyPopupAccent(HWND h) {
    static auto pSWCA = (SetWindowCompositionAttribute_t)GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    if (!pSWCA || !h) return;
    PopColors c = GetPopColors();
    ACCENTPOLICY ap{};
    if (g_s.menuEffect == FX_BLUR) {
        ap.AccentState = 3;
    } else if (g_s.menuEffect == FX_ACRYLIC) {
        ap.AccentState = 4;
        DWORD a = (DWORD)(g_s.menuOpacity * 255 / 100);
        ap.GradientColor = (a << 24) | (GetBValue(c.bg) << 16) | (GetGValue(c.bg) << 8) | GetRValue(c.bg);
    }
    WINCOMPATTRDATA d{19, &ap, sizeof(ap)};
    pSWCA(h, &d);
}

int g_rgnW = 0, g_rgnH = 0;

// Menü mit echter Transparenz rendern (wie die Leiste: schwarz/weiß → Alpha)
void RenderPopup() {
    HWND h = g_popup;
    if (!h) return;
    RECT wr;
    GetWindowRect(h, &wr);
    int w = wr.right - wr.left, ht = wr.bottom - wr.top;
    if (w <= 0 || ht <= 0) return;
    UINT dpi = ScaledDpi(h);
    EnsureFonts(dpi);
    int rad = MulDiv(PopCards() ? 16 : 8, dpi, 96);

    if (w != g_rgnW || ht != g_rgnH) {
        SetWindowRgn(h, CreateRoundRectRgn(0, 0, w + 1, ht + 1, rad * 2, rad * 2), FALSE);
        g_rgnW = w;
        g_rgnH = ht;
    }

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -ht;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(nullptr);
    void* pk = nullptr;
    void* pw = nullptr;
    HBITMAP bk = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &pk, nullptr, 0);
    HBITMAP bw = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &pw, nullptr, 0);
    if (!bk || !bw) {
        if (bk) DeleteObject(bk);
        if (bw) DeleteObject(bw);
        ReleaseDC(nullptr, screen);
        return;
    }
    HDC dk = CreateCompatibleDC(screen);
    HDC dw = CreateCompatibleDC(screen);
    HGDIOBJ ok_ = SelectObject(dk, bk);
    HGDIOBJ ow_ = SelectObject(dw, bw);

    RECT rc{0, 0, w, ht};
    g_hiRects.clear();
    g_cardRects.clear();
    DrawPopupContent(h, dk, rc, RGB(0, 0, 0));
    DrawPopupContent(h, dw, rc, RGB(255, 255, 255));
    GdiFlush();
    std::vector<BYTE> hiMask = BuildHiMask(w, ht);
    std::vector<BYTE> cardMask = BuildMask(g_cardRects, w, ht);
    COLORREF hiCol = HighlightColor();
    int hiA = g_s.highlightOpacity * 255 / 100;

    PopColors c = GetPopColors();
    // im Dunkeln wirken helle Flächen stärker → dort schwächer dosieren
    int cardA = g_s.cardOpacity * (c.dark ? 64 : 255) / 100;
    int bgA = g_s.menuEffect == FX_ACRYLIC ? 1 : g_s.menuOpacity * 255 / 100;
    if (bgA < 1) bgA = 1;  // nie 0, sonst klickt man durchs Menü
    int bR = GetRValue(c.bg) * bgA / 255;
    int bG = GetGValue(c.bg) * bgA / 255;
    int bB = GetBValue(c.bg) * bgA / 255;

    DWORD* K = (DWORD*)pk;
    DWORD* W = (DWORD*)pw;
    for (int y = 0; y < ht; y++) {
        for (int x = 0; x < w; x++) {
            int i = y * w + x;
            DWORD k = K[i], v = W[i];
            int kr = (k >> 16) & 255, kg = (k >> 8) & 255, kb = k & 255;
            int wr_ = (v >> 16) & 255, wg = (v >> 8) & 255, wb = v & 255;
            int diff = ((wr_ - kr) + (wg - kg) + (wb - kb)) / 3;
            if (diff < 0) diff = 0;
            if (diff > 255) diff = 255;
            int a = 255 - diff;
            if (kr > a) kr = a;
            if (kg > a) kg = a;
            if (kb > a) kb = a;
            int inv = 255 - a;
            int uA = bgA, uR = bR, uG = bG, uB = bB;
            if (!cardMask.empty() && cardMask[i]) {  // Karten: helle, halbtransparente Fläche
                int kA = cardMask[i] * cardA / 255, ik = 255 - kA;
                uA = kA + uA * ik / 255;
                uR = kA + uR * ik / 255;
                uG = kA + uG * ik / 255;
                uB = kA + uB * ik / 255;
            }
            if (!hiMask.empty() && hiMask[i]) {
                int hA = hiMask[i] * hiA / 255, ih = 255 - hA;
                uA = hA + uA * ih / 255;
                uR = GetRValue(hiCol) * hA / 255 + uR * ih / 255;
                uG = GetGValue(hiCol) * hA / 255 + uG * ih / 255;
                uB = GetBValue(hiCol) * hA / 255 + uB * ih / 255;
            }
            int oa = a + uA * inv / 255;
            int orr = kr + uR * inv / 255;
            int og = kg + uG * inv / 255;
            int ob = kb + uB * inv / 255;

            // abgerundete Ecken weich ausblenden
            float dx = 0, dy = 0;
            if (x < rad) dx = rad - (x + 0.5f);
            else if (x >= w - rad) dx = (x + 0.5f) - (w - rad);
            if (y < rad) dy = rad - (y + 0.5f);
            else if (y >= ht - rad) dy = (y + 0.5f) - (ht - rad);
            if (dx > 0 && dy > 0) {
                float cov = rad - std::sqrt(dx * dx + dy * dy) + 0.5f;
                if (cov <= 0) {
                    oa = orr = og = ob = 0;
                } else if (cov < 1) {
                    oa = (int)(oa * cov);
                    orr = (int)(orr * cov);
                    og = (int)(og * cov);
                    ob = (int)(ob * cov);
                }
            }
            K[i] = ((DWORD)oa << 24) | ((DWORD)orr << 16) | ((DWORD)og << 8) | (DWORD)ob;
        }
    }

    POINT dst{wr.left, wr.top}, src{0, 0};
    SIZE sz{w, ht};
    BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    UpdateLayeredWindow(h, screen, &dst, &sz, dk, &src, 0, &bf, ULW_ALPHA);

    SelectObject(dk, ok_);
    SelectObject(dw, ow_);
    DeleteDC(dk);
    DeleteDC(dw);
    DeleteObject(bk);
    DeleteObject(bw);
    ReleaseDC(nullptr, screen);
}

int PopupX(int w) {
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(MonitorFromWindow(g_popupBar, MONITOR_DEFAULTTOPRIMARY), &mi);
    int margin = MulDiv(6, ScaledDpi(g_popupBar), 96);
    int x = g_anchorL;
    if (x + w > mi.rcWork.right - margin) x = g_anchorR - w;
    if (x + w > mi.rcWork.right - margin) x = mi.rcWork.right - margin - w;
    if (x < mi.rcWork.left + margin) x = mi.rcWork.left + margin;
    return x;
}

void ClosePopup() {
    if (g_popup) DestroyWindow(g_popup);
}

void PopupRefresh() {
    if (!g_popup || g_dragRow >= 0) return;
    g_rows.clear();
    BuildMenu(g_popupAct, g_rows);
    UINT dpi = ScaledDpi(g_popup);
    int w, h;
    LayoutPopup(dpi, w, h);
    RECT wr;
    GetWindowRect(g_popup, &wr);
    if (wr.right - wr.left != w || wr.bottom - wr.top != h)
        SetWindowPos(g_popup, nullptr, PopupX(w), wr.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    RenderPopup();
}

void OpenPopup(HWND bar, Action a, int hitL, int hitR) {
    if (g_popup) DestroyWindow(g_popup);
    g_popupBar = bar;
    g_popupAct = a;
    g_calOffset = 0;
    g_dragRow = -1;
    g_mouseIn = false;
    if (a == ACT_WIFI) WifiScan();
    UpdateStatus();
    g_rows.clear();
    BuildMenu(a, g_rows);
    if (g_rows.empty()) {
        g_popupAct = ACT_NONE;
        return;
    }

    RECT br;
    GetWindowRect(bar, &br);
    UINT dpi = ScaledDpi(bar);
    int w, h;
    LayoutPopup(dpi, w, h);
    g_anchorL = br.left + hitL;
    g_anchorR = br.left + hitR;
    int x = PopupX(w);
    int y = br.bottom + MulDiv(4, dpi, 96);

    g_rgnW = g_rgnH = 0;
    g_popup = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED, kPopupClass, L"", WS_POPUP,
                              x, y, w, h, bar, nullptr, (HINSTANCE)&__ImageBase, nullptr);
    if (!g_popup) {
        g_popupAct = ACT_NONE;
        g_popupBar = nullptr;
        return;
    }
    DWORD corner = 2;  // DWMWCP_ROUND
    DwmSetWindowAttribute(g_popup, 33, &corner, sizeof(corner));
    BOOL dark = GetPopColors().dark;
    DwmSetWindowAttribute(g_popup, 20, &dark, sizeof(dark));
    ApplyPopupAccent(g_popup);
    RenderPopup();
    ShowWindow(g_popup, SW_SHOW);
    SetForegroundWindow(g_popup);
    RenderAll();
    g_outsideSince = 0;
    if (g_s.menuOnHover) SetTimer(g_hwnd, TIMER_HOVERCLOSE, 100, nullptr);
}

void SliderSet(HWND w, int i, int x) {
    UINT dpi = ScaledDpi(w);
    int l, r;
    SliderTrack(g_rowRects[i], dpi, l, r);
    float v = r > l ? (float)(x - l) / (float)(r - l) : 0.f;
    if (v < 0) v = 0;
    if (v > 1) v = 1;
    g_rows[i].value = v;
    g_rows[i].right = std::to_wstring((int)(v * 100 + 0.5f)) + L" %";
    if (g_rows[i].onChange) g_rows[i].onChange(v);
    RenderPopup();
}

void PopupClick(POINT p, int btn) {
    int i = RowAt(p);
    if (i < 0 || i >= (int)g_rows.size()) return;
    Row& r = g_rows[i];
    if (r.type == ROW_CUSTOM) {
        if (!r.click) return;
        int res = r.click(g_rowRects[i], p, btn);
        if (res == 1) ClosePopup();
        else if (res == 2) PopupRefresh();
        return;
    }
    if (btn == 2 || !RowClickable(r)) return;
    std::function<void()> fn = r.onClick;
    if (r.keepOpen) {
        fn();
        PopupRefresh();
    } else {
        ClosePopup();
        fn();
    }
}

LRESULT CALLBACK PopupProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(w, &ps);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEMOVE: {
        POINT p{(short)LOWORD(lp), (short)HIWORD(lp)};
        if (!g_mouseIn) {
            TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, w, 0};
            TrackMouseEvent(&t);
        }
        g_mouse = p;
        g_mouseIn = true;
        if (g_dragRow >= 0) SliderSet(w, g_dragRow, p.x);
        RenderPopup();
        return 0;
    }
    case WM_MOUSELEAVE:
        g_mouseIn = false;
        RenderPopup();
        return 0;
    case WM_LBUTTONDOWN: {
        POINT p{(short)LOWORD(lp), (short)HIWORD(lp)};
        int i = RowAt(p);
        if (i >= 0 && i < (int)g_rows.size() && g_rows[i].type == ROW_SLIDER) {
            g_dragRow = i;
            SetCapture(w);
            SliderSet(w, i, p.x);
        }
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_LBUTTONDBLCLK: {
        POINT p{(short)LOWORD(lp), (short)HIWORD(lp)};
        if (msg == WM_LBUTTONUP && g_dragRow >= 0) {
            g_dragRow = -1;
            ReleaseCapture();
            return 0;
        }
        PopupClick(p, msg == WM_RBUTTONUP ? 1 : (msg == WM_LBUTTONDBLCLK ? 2 : 0));
        return 0;
    }
    case WM_MOUSEWHEEL:
        if (g_popupAct == ACT_VOLUME) {
            ChangeVolume(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 0.02f : -0.02f);
            UpdateStatus();
            PopupRefresh();
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) ClosePopup();
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) PostMessageW(w, WM_CLOSE, 0, 0);
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(w, 1);
            ClosePopup();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        if (g_popup == w) {
            g_popup = nullptr;
            g_popupAct = ACT_NONE;
            g_rows.clear();
            g_rowRects.clear();
            g_dragRow = -1;
            g_mouseIn = false;
            g_popupBar = nullptr;
            RenderAll();
        }
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

// ======================================================================
// Leiste
// ======================================================================
void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD, HWND w, LONG, LONG, DWORD, DWORD) {
    if (!w || !g_hwnd) return;
    // eigene Fenster (Menüs, Einstellungen, Dialoge) ignorieren
    if (GetWindowThreadProcessId(w, nullptr) == GetCurrentThreadId()) return;
    if (IsTaskbarWindow(w)) return;  // Taskleiste: Menüs der letzten App behalten
    g_lastFg = w;
    g_appName = IsExplorerWindow(w) ? std::wstring(L"Explorer") : GetAppName(w);
    UpdateAppMenuTitles(w);
    RenderAll();
}

void ShowContextMenu(HWND h) {
    ClosePopup();
    POINT pt;
    GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, T(L"Menüleiste einstellen…"));
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 2, T(L"Task-Manager"));
    AppendMenuW(m, MF_STRING, 3, T(L"Windows-Einstellungen"));
    SetForegroundWindow(h);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN,
                             pt.x, pt.y, 0, h, nullptr);
    PostMessageW(h, WM_NULL, 0, 0);
    DestroyMenu(m);
    switch (cmd) {
    case 1: ShowSettings(); break;
    case 2: Run(L"taskmgr.exe"); break;
    case 3: OpenUri(L"ms-settings:"); break;
    }
}

// Verhalten ohne eigene Menüs (Windows-Flyouts)
void LegacyAction(HWND h, Action a) {
    switch (a) {
    case ACT_LOGO:    PressCombo({VK_LWIN}); break;
    case ACT_CLOCK:   PressCombo({VK_LWIN, 'N'}); break;
    case ACT_BATTERY:
    case ACT_WIFI:
    case ACT_VOLUME:  PressCombo({VK_LWIN, 'A'}); break;
    case ACT_SYSTEM:  Run(L"taskmgr.exe"); break;
    case ACT_KBD:     PressCombo({VK_LWIN, VK_SPACE}); break;
    case ACT_SEARCH:  PressCombo({VK_LWIN, 'S'}); break;
    case ACT_CONTROL: PressCombo({VK_LWIN, 'A'}); break;
    case ACT_BLUETOOTH: OpenUri(L"ms-settings:bluetooth"); break;
    case ACT_TRAY:
        // Win+B fokussiert den Pfeil "Ausgeblendete Symbole", Enter öffnet ihn
        PressCombo({VK_LWIN, 'B'});
        SetTimer(g_hwnd, TIMER_TRAY, 150, nullptr);
        break;
    default: break;
    }
}

// ---------- eine Leiste pro Monitor ----------
LRESULT CALLBACK BarProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        APPBARDATA abd{sizeof(abd)};
        abd.hWnd = h;
        abd.uCallbackMessage = WM_APPBAR_CB;
        SHAppBarMessage(ABM_NEW, &abd);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_TIMER:
        if (wp == TIMER_HOVER) {
            // Maus ist lange genug über einem Bereich → Menü öffnen
            KillTimer(h, TIMER_HOVER);
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(h, &pt);
            RECT cr;
            GetClientRect(h, &cr);
            const Hit* hit = PtInRect(&cr, pt) ? HitAt(h, pt.x) : nullptr;
            if (!g_popup && hit && hit->act == g_hoverAct) {
                g_menuIdx = hit->idx;
                OpenPopup(h, hit->act, hit->l, hit->r);
            }
        }
        return 0;
    case WM_LBUTTONUP: {
        const Hit* hit = HitAt(h, (short)LOWORD(lp));
        Action a = hit ? hit->act : ACT_NONE;
        if (!g_s.customMenus) {
            LegacyAction(h, a);
            return 0;
        }
        if (a == ACT_SEARCH) {  // Suche hat kein Menü
            ClosePopup();
            PressCombo({VK_LWIN, 'S'});
            return 0;
        }
        if (a == ACT_NONE || (g_popup && g_popupAct == a && g_popupBar == h && g_menuIdx == hit->idx)) {
            ClosePopup();
            return 0;
        }
        g_menuIdx = hit->idx;
        OpenPopup(h, a, hit->l, hit->r);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!g_barTracking) {
            TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
            TrackMouseEvent(&t);
            g_barTracking = true;
        }
        const Hit* hit = HitAt(h, (short)LOWORD(lp));
        Action a = hit ? hit->act : ACT_NONE;
        if (g_popup) {
            // Nur im Überfahren-Modus wechselt ein offenes Menü beim Drüberfahren
            if (g_s.menuOnHover && g_dragRow < 0 && a != ACT_NONE && a != ACT_SEARCH &&
                (a != g_popupAct || g_popupBar != h || hit->idx != g_menuIdx)) {
                g_menuIdx = hit->idx;
                OpenPopup(h, a, hit->l, hit->r);
            }
        } else if (g_s.customMenus && g_s.menuOnHover && a != g_hoverAct) {
            g_hoverAct = a;
            KillTimer(h, TIMER_HOVER);
            if (a != ACT_NONE && a != ACT_SEARCH) SetTimer(h, TIMER_HOVER, 200, nullptr);
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        g_barTracking = false;
        g_hoverAct = ACT_NONE;
        KillTimer(h, TIMER_HOVER);
        return 0;
    case WM_RBUTTONUP:
        ShowContextMenu(h);
        return 0;
    case WM_MBUTTONUP:
        if (HitTest(h, (short)LOWORD(lp)) == ACT_VOLUME) {
            ToggleMute();
            Refresh();
        }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt{(short)LOWORD(lp), (short)HIWORD(lp)};
        ScreenToClient(h, &pt);
        if (HitTest(h, pt.x) == ACT_VOLUME) {
            ChangeVolume(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 0.02f : -0.02f);
            Refresh();
        }
        return 0;
    }
    case WM_APPBAR_CB:
        if (wp == ABN_POSCHANGED) {
            PositionAppBar(h);
            Render(h);
        } else if (wp == ABN_FULLSCREENAPP) {
            SetWindowPos(h, lp ? HWND_BOTTOM : HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        return 0;
    case WM_DPICHANGED:
        PositionAppBar(h);
        Render(h);
        return 0;
    case WM_DESTROY: {
        KillTimer(h, TIMER_HOVER);
        APPBARDATA abd{sizeof(abd)};
        abd.hWnd = h;
        SHAppBarMessage(ABM_REMOVE, &abd);
        return 0;
    }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

BarWin* CreateBar(const MonInfo& m) {
    BarWin* b = new BarWin;
    b->device = m.device;
    g_bars.push_back(b);
    b->hwnd = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
                              kClassName, L"", WS_POPUP, m.rc.left, m.rc.top, 100, 28,
                              nullptr, nullptr, (HINSTANCE)&__ImageBase, nullptr);
    if (!b->hwnd) {
        g_bars.pop_back();
        delete b;
        return nullptr;
    }
    PositionAppBar(b->hwnd);
    ApplyAccent(b->hwnd);
    Render(b->hwnd);
    ShowWindow(b->hwnd, SW_SHOWNOACTIVATE);
    return b;
}

void DestroyBar(BarWin* b) {
    if (g_popup && g_popupBar == b->hwnd) ClosePopup();
    for (size_t i = 0; i < g_bars.size(); i++)
        if (g_bars[i] == b) { g_bars.erase(g_bars.begin() + i); break; }
    if (g_cur == b) g_cur = nullptr;
    if (b->hwnd) DestroyWindow(b->hwnd);
    delete b;
}

// Leisten passend zur Monitor-Auswahl anlegen bzw. entfernen
void SyncBars() {
    std::vector<MonInfo> mons = ListMonitors();
    if (mons.empty()) return;
    std::vector<MonInfo> targets;
    if (g_s.monitor == L"all") {
        targets = mons;
    } else {
        for (auto& m : mons)
            if (m.device == g_s.monitor) targets.push_back(m);
        if (targets.empty())  // "primary" oder Monitor nicht angeschlossen
            for (auto& m : mons)
                if (m.primary) targets.push_back(m);
        if (targets.empty()) targets.push_back(mons[0]);
    }
    for (size_t i = g_bars.size(); i-- > 0;) {
        bool keep = false;
        for (auto& t : targets)
            if (t.device == g_bars[i]->device) keep = true;
        if (!keep) DestroyBar(g_bars[i]);
    }
    for (auto& t : targets) {
        bool have = false;
        for (auto b : g_bars)
            if (b->device == t.device) have = true;
        if (!have) CreateBar(t);
    }
}

// ---------- unsichtbares Steuerfenster (Timer, Systemmeldungen) ----------
LRESULT CALLBACK CtrlProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_TIMER:
        if (wp == TIMER_TICK) {
            Refresh();
        } else if (wp == TIMER_TRAY) {
            KillTimer(h, TIMER_TRAY);
            PressCombo({VK_RETURN});
        } else if (wp == TIMER_KEYS) {
            KillTimer(h, TIMER_KEYS);
            if (!g_pendingKeys.empty()) PressKeys(g_pendingKeys);
            g_pendingKeys.clear();
        } else if (wp == TIMER_SYNC) {
            KillTimer(h, TIMER_SYNC);
            SyncBars();
            for (auto b : g_bars) PositionAppBar(b->hwnd);
            RenderAll();
        } else if (wp == TIMER_HOVERCLOSE) {
            // Überfahren-Modus: Menü schließen, wenn die Maus Leiste und Menü verlässt
            if (!g_popup || !g_s.menuOnHover || !g_popupBar) {
                KillTimer(h, TIMER_HOVERCLOSE);
                return 0;
            }
            if (g_dragRow >= 0) {
                g_outsideSince = 0;
                return 0;
            }
            POINT pt;
            GetCursorPos(&pt);
            RECT pr, br;
            GetWindowRect(g_popup, &pr);
            GetWindowRect(g_popupBar, &br);
            int m = MulDiv(10, ScaledDpi(g_popupBar), 96);
            RECT zone = pr;
            InflateRect(&zone, m, m);
            RECT gap{pr.left, br.bottom, pr.right, pr.top};
            bool inside = PtInRect(&zone, pt) || PtInRect(&br, pt) || PtInRect(&gap, pt);
            if (inside) g_outsideSince = 0;
            else if (!g_outsideSince) g_outsideSince = GetTickCount64();
            else if (GetTickCount64() - g_outsideSince > 400) ClosePopup();
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (wp == SPI_SETDESKWALLPAPER) {
            MarkWallDirty();
            RenderAll();
        }
        // Windows wechselt zwischen hell und dunkel
        if (lp && !wcscmp((LPCWSTR)lp, L"ImmersiveColorSet") && g_s.theme == THEME_SYSTEM) {
            ResolveTheme();
            for (auto b : g_bars) ApplyAccent(b->hwnd);
            if (g_popup) ApplyPopupAccent(g_popup);
            RenderAll();
            PopupRefresh();
            if (g_settingsWnd) PostMessageW(g_settingsWnd, WM_SETTINGS_REBUILD, 0, 0);
        }
        return 0;
    case WM_DISPLAYCHANGE:
        // Monitor angesteckt/abgesteckt/umgestellt – kurz warten, bis Windows fertig ist
        SetTimer(h, TIMER_SYNC, 800, nullptr);
        return 0;
    case WM_DESTROY:
        ClosePopup();
        if (g_settingsWnd) DestroyWindow(g_settingsWnd);
        KillTimer(h, TIMER_TICK);
        while (!g_bars.empty()) DestroyBar(g_bars.back());
        for (HFONT* f : {&g_font, &g_fontBold, &g_iconFont, &g_fontBig, &g_iconSmall, &g_fontSmall})
            if (*f) { DeleteObject(*f); *f = nullptr; }
        g_fontDpi = 0;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

DWORD WINAPI BarThread(LPVOID) {
    // Nur im Explorer-Prozess laufen, dem die Taskleiste gehört
    HWND tray = nullptr;
    for (int i = 0; i < 240 && !tray; i++) {
        tray = FindWindowW(L"Shell_TrayWnd", nullptr);
        if (!tray && WaitForSingleObject(g_stop, 250) == WAIT_OBJECT_0) return 0;
    }
    if (!tray) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(tray, &pid);
    if (pid != GetCurrentProcessId()) return 0;
    g_trayWnd = tray;

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ULONG_PTR gdipToken = 0;
    Gdiplus::GdiplusStartupInput gdipInput;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);
    CoCreateInstance(kCLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                     kIID_IMMDeviceEnumerator, (void**)&g_audioEnum);
    CoCreateInstance(kCLSID_NetworkListManager, nullptr, CLSCTX_ALL,
                     kIID_INetworkListManager, (void**)&g_nlm);

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    HINSTANCE inst = (HINSTANCE)&__ImageBase;
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = BarProc;
    wc.hInstance = inst;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    WNDCLASSEXW sc{sizeof(sc)};
    sc.lpfnWndProc = SettingsProc;
    sc.hInstance = inst;
    sc.lpszClassName = kSettingsClass;
    sc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    sc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    sc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    RegisterClassExW(&sc);

    WNDCLASSEXW cw{sizeof(cw)};
    cw.lpfnWndProc = CtrlProc;
    cw.hInstance = inst;
    cw.lpszClassName = kCtrlClass;
    RegisterClassExW(&cw);

    WNDCLASSEXW pc{sizeof(pc)};
    pc.style = CS_DBLCLKS;
    pc.lpfnWndProc = PopupProc;
    pc.hInstance = inst;
    pc.lpszClassName = kPopupClass;
    pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&pc);

    // unsichtbares Steuerfenster; die Leisten selbst legt SyncBars() an
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kCtrlClass, L"", WS_POPUP,
                             0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    if (g_hwnd) {
        g_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                 nullptr, WinEventProc, 0, 0, WINEVENT_OUTOFCONTEXT);
        g_lastFg = GetForegroundWindow();
        g_appName = GetAppName(g_lastFg);
        UpdateAppMenuTitles(g_lastFg);
        UpdateStatus();
        SyncBars();
        SetTimer(g_hwnd, TIMER_TICK, 500, nullptr);
        InstallTrayHook();
        if (WaitForSingleObject(g_stop, 0) == WAIT_OBJECT_0) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            if (g_settingsWnd && IsDialogMessageW(g_settingsWnd, &msg)) continue;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    RemoveTrayHook();
    if (g_hook) { UnhookWinEvent(g_hook); g_hook = nullptr; }
    g_hwnd = nullptr;
    UnregisterClassW(kClassName, inst);
    UnregisterClassW(kSettingsClass, inst);
    UnregisterClassW(kPopupClass, inst);
    UnregisterClassW(kCtrlClass, inst);

    if (g_wlan) { WlanCloseHandle(g_wlan, nullptr); g_wlan = nullptr; }
    if (g_audioEnum) { g_audioEnum->Release(); g_audioEnum = nullptr; }
    if (g_nlm) { g_nlm->Release(); g_nlm = nullptr; }
    g_logoMask.clear();
    g_logoSize = 0;
    if (gdipToken) Gdiplus::GdiplusShutdown(gdipToken);
    CoUninitialize();
    return 0;
}

// ======================================================================
// Windhawk entry points
// ======================================================================
BOOL Wh_ModInit() {
    g_de = PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN;
    LoadSettings();
    g_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, BarThread, nullptr, 0, nullptr);
    return TRUE;
}

void Wh_ModUninit() {
    if (g_stop) SetEvent(g_stop);
    if (g_thread) {
        if (g_hwnd) PostMessageW(g_hwnd, WM_CLOSE, 0, 0);
        WaitForSingleObject(g_thread, 5000);
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    if (g_stop) { CloseHandle(g_stop); g_stop = nullptr; }
}