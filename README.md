# MacOS-Dock

Turns Windows 11 into a macOS-style desktop with [Windhawk](https://windhawk.net):
a floating glass dock with macOS icons and dot indicators, a top menu bar, rounded window corners, genie minimize
animations and macOS system icons.

> Not affiliated with or endorsed by Apple Inc. macOS and Apple are trademarks of Apple Inc., registered in the U.S. and other countries. No Apple icons or other Apple files are included.

<img width="3072" height="1920" alt="Screenshot 2026-10-09 173246" src="https://github.com/user-attachments/assets/43fc9d7d-18f7-4ce3-a2d3-27c20dfdbdfd" />

## Folder layout

The folder **must** be located at `Documents\MacOs-Dock`, because the mods load
the icons from there.

```
Documents\MacOs-Dock\
├── README.md
├── Setup.exe                      Setup with a window (recommended)
├── Setup.cmd                      Same setup in the console
├── setup\                         Setup script and mod list (mods.json)
├── tools\                         Build scripts (setup-exe = source of Setup.exe)
├── Icons\                         put your icons here (not included, see below)
└── Files\
    ├── macos-dock-taskbar.wh.cpp  Windhawk mod: macOS Dock
    ├── top-menubar.wh.cpp         Windhawk mod: Top Menu Bar
    ├── stage-manager-taskbar-aware.wh.cpp Windhawk mod: Stage Manager
    ├── macos-minimize-animation-dock.wh.cpp Windhawk mod: genie animation (fork)
    ├── macos-traffic-lights.wh.cpp Windhawk mod: macOS Traffic Lights
    ├── three-finger-drag.wh.cpp   Windhawk mod: Three Finger Drag
    ├── turbo-boost-tray-toggle.wh.cpp Windhawk mod: Turbo Boost Tray Toggle
    ├── Trash.cmd                  Creates the Trash shortcut for the dock
    └── Trash.ps1
```
<img width="1232" height="491" alt="Screenshot 2026-10-09 173430" src="https://github.com/user-attachments/assets/9296bee7-60ef-4821-b952-16ba6466a49e" />

## Quick install (recommended)

1. Download this repository (**Code → Download ZIP**) and extract it anywhere.
2. Optional: download a macOS icon pack (see [Icons](#icons)).
3. Double-click **`Setup.exe`** and confirm the admin prompt.
4. Choose the optional mods and your icon pack (ZIP or folder, or drag it into
   the window), then click **Install**.

`Setup.cmd` runs the same setup in a console window, if you prefer that.

The setup does everything else automatically:

- installs **Windhawk** if it isn't installed yet
- copies the project to `Documents\MacOs-Dock` and imports your icon pack
- installs all catalog mods listed below with the recommended settings
- compiles and installs the custom mods (macOS Dock, Top Menu Bar, genie
  animation, and optionally Stage Manager, Three Finger Drag and Turbo Boost
  Tray Toggle)
- disables mods that conflict with the dock, centers the taskbar and hides
  search, task view and widgets
- restarts Windhawk and Explorer

Compiling the custom mods takes a few minutes. Mods that are already installed
keep their settings; turn on **Reset settings** in `Setup.exe` (or run
`Setup.cmd -ResetSettings`) to apply the recommended settings again. After the setup, pin `Files\Trash.lnk` to the taskbar and
right-click the apps in the dock → **Change icon …** to assign icons.

### Updating

Download the new release and run `Setup.exe` again. It detects the existing
installation and only updates: your dock and mod settings, icons, taskbar
options and mods you turned off stay as they are. The optional mods you already
use are preselected.

The manual steps below do the same by hand.

## Requirements

- Windows 11
- Windhawk (free): <https://windhawk.net>

## 1. Install Windhawk

Download Windhawk from <https://windhawk.net> and install it with the default
options.

## 2. Download and copy the folder

Download this repository (**Code → Download ZIP**), extract it and rename the
folder to `MacOs-Dock`. Move it to your **Documents** folder, so you get
`C:\Users\<your name>\Documents\MacOs-Dock`.

### Icons

Icons are **not included** because the macOS icons are copyrighted by Apple.
Download icons Pack from [(for example from <https://www.reddit.com/r/MacOS/comments/1u9f8x9/macos_27_icon_pack_1024x1024_png/> and put
them into the `Icons` folder (All Icons directly in to the `Icons` folder not in to a folder in the `Icons` folder !!!):

- **PNG** files (ideally 1024×1024 with a transparent margin) for the dock
- **ICO** files only if you want custom icons for shortcuts or the Trash
  (`trash (empty).ico`)

The default app list expects file names like `finder.png`, `safari.png`,
`app store.png`, `system settings.png`, `photos.png`, `calculator.png`,
`textedit.png`, `terminal.png`. You can change them any time: right-click the
app in the dock → **Change icon …**.

## 3. Install the mods from the Windhawk catalog

In Windhawk, click **Explore**, search for each mod, click **Install**, then set
the settings under **Details → Settings**.

| Mod | Settings |
|---|---|
| **Resource Redirect** | *Icon theme*: `macOS Blue (by niivu)`, *Disable folder thumbnails*: on, *Redirect all loaded resources*: on |
| **Custom Window Corner Radius** | *Corner radius*: `12`, *Small corner radius*: `6` |
| **Windows 11 File Explorer Styler** | *Theme*: `Minimal Explorer11`, rest default |
| **Center New Windows** | Defaults are fine |
| **Invisible Window Borders** | Defaults are fine |
| **Monitor Rounded Edges** | Defaults are fine |
| **Slick Window Arrangement** | Defaults are fine |
| **Dark mode for menus** (*Dunkler Modus für Menüs*) | Defaults are fine |

## 4. Install the custom mods

These mods are not in the catalog. Install each one like this:

1. In Windhawk, click **Create a New Mod**.
2. Delete everything in the editor and paste the full content of the `.wh.cpp`
   file from the `Files` folder.
3. Click **Compile Mod**, then **Exit Editing Mode**.

| File | Mod | Note |
|---|---|---|
| `macos-dock-taskbar.wh.cpp` | **macOS Dock** | Required |
| `top-menubar.wh.cpp` | **Top Menu Bar** | Optional |
| `macos-minimize-animation-dock.wh.cpp` | **MacOS Minimize Animation (Dock left/right)** | Genie minimize animation, also works with the dock on the left/right. Fork of *MacOS Minimize Animation* by Abdullah Masood; disable the original if you have it |
| `stage-manager-taskbar-aware.wh.cpp` | **Stage Manager (taskbar aware)** | Optional, recent windows as cards next to the active window (Win+Alt+S). Toggle it from the Control Center in the Top Menu Bar |
| `three-finger-drag.wh.cpp` | **Three Finger Drag (macOS-Style)** | Optional, drag with three fingers on a Precision Touchpad (laptops only) |
| `turbo-boost-tray-toggle.wh.cpp` | **Turbo Boost Tray Toggle** | Optional, tray icon to turn CPU Turbo Boost on/off |


> **Important:** Do not enable **Windows 11 Taskbar Styler** or **Taskbar height
> and icon size** together with **macOS Dock**. macOS Dock already includes
> both and they would conflict.

## 5. Restart Explorer

Sign out and back in, or restart Explorer in Task Manager. The taskbar height
and icon sizes are fully applied only after this restart.

## 6. Dock settings and icons

Right-click any app in the dock. The menu has:

- **Show jump list:** the normal Windows menu of the app (recent files, pin,
  close window).
- **Change icon …:** pick a PNG for exactly this app. It starts in the `Icons`
  folder.
- **Use Windows icon:** removes the custom icon of this app.
- **Dock settings …:** opens the settings window.

The settings window looks like the one of the Top Menu Bar and has the pages
*General* (position left/bottom/right, size, hidden elements), *Dock*,
*Appearance* (light/dark, tint and edge colors), *Icons* and *Indicator*. The
dock at the bottom and on the left/right keep separate settings; the window
always edits the current position. All changes apply immediately, no admin
prompt needed.

## 7. Magnification and Recycle Bin

Both are built into macOS Dock (since 2.2); turn them on or off in the dock
settings window under **Effects**.

- **Magnification:** icons grow under the pointer and push the others aside,
  like on a Mac. Size, range and animation speed are adjustable. The taskbar
  keeps room above the dock for it; maximized windows still end right above
  the dock, because the dock turns on *Automatically hide the taskbar* (the
  taskbar never actually hides) and keeps the dock's strip free itself. It's
  set back when the mod is turned off. Don't use *Taskbar Dock Animation Plus*
  at the same time.
- **Recycle Bin:** sits at the end of the dock after a divider, shows whether
  it's full, opens on click, deletes files you drop on it (they can be
  restored) and empties with right-click. Its icon comes from the icon folder
  of the active icon set: `trash (empty).png` and `trash (full).png`; the
  *Windows* set (or a missing file) shows the Windows Recycle Bin. The old
  pinned `Trash.lnk` is hidden automatically, and *Taskbar Icon Separators* is
  no longer needed.

## Custom icons for other apps

The easiest way: right-click the app in the dock → **Change icon …**. To do it
by hand, add an entry in Windhawk under *macOS Dock → Settings → Apps and icons*
with:

- **App IDs:** find them with `Get-StartApps` in PowerShell. Several IDs can be
  separated with commas.
- **PNG file:** a file name from the `Icons` folder.

 
## Known issues

- Missing icons on first start: every app shows its Windows icon at first. Right-click each app in the dock → **Change icon …**.
- Minimize animation with the dock on the left or right: use the included fork **MacOS Minimize Animation (Dock left/right)** instead of the catalog mod. The *Classic* animation style only works at the bottom; on the left/right the fork always uses the *Modern* style.

  
## Changelog

- **Changed (macOS Dock 2.3.1):** the dock grows like on a Mac again: the end away from the pointer's half stays put.
- **Changed (macOS Dock 2.3):** the magnification now runs entirely in the compositor (expressions per icon), so it's as smooth as the screen's refresh rate; the dock's white edge is drawn by the compositor and stays visible while the dock grows.
- **Changed (macOS Dock 2.2.2):** smoother magnification: the glass grows with a composition clip instead of a new layout every frame.
- **Fixed (macOS Dock 2.2.1):** maximized windows end right above the dock again with magnification on, the dot stays below a magnified icon, Windows icons sit at the same height as custom ones, smoother magnification.
- **New (macOS Dock 2.2):** built-in macOS magnification (the dock grows with the icons) and a real Recycle Bin at the end of the dock with a divider: full/empty icon, drop files to delete them, right-click to empty. Both are adapted from the *MacOS Style Dock* mod by Zach Vaughn. The setup no longer installs *Taskbar Dock Animation Plus* and *Taskbar Icon Separators* and turns them off.
- **New (macOS Dock 2.1):** icon sets. Switch between **macOS** (your chosen icons), **Windows** (original icons) and your own sets, e.g. "Minecraft", with right-click → **Icon set** or in the settings window under **Icon sets**. Every set keeps its own icons and icon folder (new sets get `Documents\MacOs-Dock\Icon-Sets\<name>`).
- **Fixed (macOS Dock 2.0.7):** after moving the dock back to the bottom, opened apps that aren't pinned could keep a turned icon.
- **Fixed (macOS Dock 2.0.6):** with the dock on the left or right, an opened app that isn't pinned could be cut off at the top and bottom (it reused the short button of the separator).
- **New:** running `Setup.exe` again updates an existing installation and keeps all settings, icons, taskbar options and turned-off mods.
- **New:** `Setup.exe`, a setup with a window: pick the optional mods and the icon pack, then watch the progress of every step. `Setup.cmd` still works.
- **New:** Stage Manager mod (`stage-manager-taskbar-aware.wh.cpp`) and Top Menu Bar 0.32.0 with a Stage Manager tile in the Control Center.
- **New:** `macos-minimize-animation-dock.wh.cpp`, a fork of *MacOS Minimize Animation* whose genie flows sideways into the dock when it's on the left or right. The setup installs it instead of the catalog mod and disables the original.
- **New (macOS Dock 2.0):** right-click any app in the dock for a menu with **Show jump list**, **Change icon …**, **Use Windows icon** and **Dock settings …**. The new settings window lives inside the mod (same style as the Top Menu Bar settings, no admin prompt) and replaces the old PowerShell Dock Settings app. Your old settings are taken over automatically. The separator is turned by the dock itself now.
- **Changed (macOS Dock 1.12):** new installs start with the author's tuned settings, separately for bottom and left/right. No custom app icons are preset anymore, so every app keeps its Windows icon until you pick one with **Choose icon …** in the Dock Settings app.
- **New (macOS Dock 1.9):** bottom and left/right keep separate settings in the Dock Settings app; changes only apply to the current position. On the left or right the size goes up to 100%.
- **New:** the Dock Settings app turns the separator of the *Taskbar Icon Separators* mod by 90° when the dock is on the left or right (and back at the bottom).
- **Fixed (macOS Dock 1.6):** on the left or right the taskbar itself now gets wider with the size slider (same thickness as at the bottom), so icons are no longer cut off and the dot keeps its distance to the icon.
- **Changed (macOS Dock 1.5):** on the left or right the taskbar now gets the same width as its height at the bottom, so the dock keeps all sizes and spacing and the size slider works exactly as at the bottom. The automatic shrinking from 1.4 was removed.
- **New (macOS Dock 1.4):** on the left or right the dock shrinks automatically when not all apps fit on the screen, instead of hiding them behind "…". Separator icons (file name contains "separator") are turned by 90° there.
- **New (macOS Dock 1.3):** choose **Left**, **Bottom** or **Right** under *Position* in the Dock Settings app. It moves the Windows taskbar, restarts Explorer and the dock keeps its look on the side (it shrinks proportionally to fit the narrower taskbar).
- **Fixed (macOS Dock 1.2.1, Top Menu Bar 0.31.2):** with the dock on the left or right, the icons were cut off and the menu bar was shifted. Icons now shrink to fit the taskbar width and the menu bar always spans the full screen width.
- **New (macOS Dock 1.2):** the dock now works with the taskbar on the left or right (Windows Settings → Personalization → Taskbar → Taskbar behaviors). The dock runs vertically and the dots sit on the screen-edge side. Restart Explorer once after changing the taskbar position.
- **New:** one-click setup (`Setup.cmd`) that installs Windhawk, all mods and settings automatically.
- **New (macOS Dock 1.1):** light and dark mode for the dock. In the Dock Settings app under *Advanced → Appearance* choose **Follow Windows**, **Light** or **Dark**. "Follow Windows" switches automatically with the Windows mode. Both modes have their own tint and edge colors.
- **Fixed:** the dock background was too wide on external monitors and Full HD screens (the icons were fine). The glass background now only spans the icons on every monitor. Copy the new `macos-dock-taskbar.wh.cpp` into the mod and click **Compile Mod**.

## Troubleshooting

| Problem | Fix |
|---|---|
| Dock is cut off at the top | Lower *Overall size* in the dock settings (right-click an app → **Dock settings …**) |
| An icon does not change | Right-click the app → **Change icon …**, this always uses the correct App ID |
| Taskbar looks broken after a Windows update | Disable the mod in Windhawk and wait for an update, or recompile it |
| Icon not showing up | Convert Icon from .PNG to .ICO |

## License

GPL-3.0, see [LICENSE](LICENSE).

This is an independent fan project. It is not affiliated with, sponsored or endorsed by Apple Inc. macOS and Apple are trademarks of Apple Inc.

## Credits

- **macOS Dock** is based on *Windows 11 Taskbar Styler* and *Taskbar height and
  icon size* by m417z (GPL v3). The `WindhawkBlur` brush is based on code from
  the TranslucentTB project.
  Its magnification and Recycle Bin are adapted from *MacOS Style Dock* by Zach
  Vaughn (GPL v3).
- **Resource Redirect**, **Custom Window Corner Radius** by m417z (Windhawk
  catalog).
- **MacOS Minimize Animation (Dock left/right)** is a fork of *MacOS Minimize
  Animation* by Abdullah Masood with the genie engine by Potassiumuncher (MIT);
  only the left/right dock support was added.
- Top Menu Bar, macOS Traffic Lights, dock settings window: Okan.
