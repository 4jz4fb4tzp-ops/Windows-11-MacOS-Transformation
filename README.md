# MacOS-Dock

Turns Windows 11 into a macOS-style desktop with [Windhawk](https://windhawk.net):
a floating glass dock with macOS icons and dot indicators, a top menu bar, rounded window corners, genie minimize
animations and macOS system icons.

<img width="3072" height="1920" alt="Screenshot 2026-10-09 173246" src="https://github.com/user-attachments/assets/43fc9d7d-18f7-4ce3-a2d3-27c20dfdbdfd" />

## Folder layout

The folder **must** be located at `Documents\MacOs-Dock`, because the mods load
the icons from there.

```
Documents\MacOs-Dock\
├── README.md
├── Icons\                         put your icons here (not included, see below)
└── Files\
    ├── macos-dock-taskbar.wh.cpp  Windhawk mod: macOS Dock
    ├── top-menubar.wh.cpp         Windhawk mod: Top Menu Bar
    ├── macos-traffic-lights.wh.cpp Windhawk mod: macOS Traffic Lights
    ├── Dock-Settings.cmd          Starts the Dock Settings app
    ├── Dock-Settings.ps1          Dock Settings app
    ├── Trash.cmd                  Creates the Trash shortcut for the dock
    └── Trash.ps1
```
<img width="1232" height="491" alt="Screenshot 2026-10-09 173430" src="https://github.com/user-attachments/assets/9296bee7-60ef-4821-b952-16ba6466a49e" />

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
`textedit.png`, `terminal.png`. You can change them any time in the Dock
Settings app.

## 3. Install the mods from the Windhawk catalog

In Windhawk, click **Explore**, search for each mod, click **Install**, then set
the settings under **Details → Settings**.

| Mod | Settings |
|---|---|
| **Resource Redirect** | *Icon theme*: `macOS LightMode` |
| **Custom Window Corner Radius** | *Corner radius*: `12`, *Small corner radius*: `6` |
| **MacOS Minimize Animation** | Defaults are fine |

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


> **Important:** Do not enable **Windows 11 Taskbar Styler** or **Taskbar height
> and icon size** together with **macOS Dock**. macOS Dock already includes
> both and they would conflict.

## 5. Restart Explorer

Open the **Dock Settings** app (next step) and click **Restart Explorer**, or
sign out and back in. The taskbar height and icon sizes are fully applied only
after this restart.

## 6. Dock Settings app

Double-click `Files\Dock-Settings.cmd` and confirm the admin prompt (needed
because Windhawk stores its settings in a protected part of the registry).

- **Size:** one slider that scales the whole dock. 100% equals the values under
  *Advanced*.
- **Taskbar icons:** lists all apps currently on the taskbar. Click
  **Choose icon …** to pick a PNG from the `Icons` folder, or **Original** to
  remove a custom icon. Click **Rescan** after pinning new apps.
- **Advanced:** every single value (dock height, radius, spacing, icon sizes,
  indicator dot, colors, icon folder, app list).
- **My defaults / Save current as default:** restore or save your preferred
  setup.

All changes are applied immediately.

## 7. Trash in the dock

1. Double-click `Files\Trash.cmd`. It creates `Trash.lnk` and opens the folder
   with it selected.
2. Right-click `Trash.lnk` → **Show more options** → **Pin to taskbar**.
3. Drag it to the far right of your pinned apps.

Windows 11 does not allow scripts to pin items automatically, so step 2 has to
be done by hand. Apps that are running but not pinned always appear to the
right of the Trash; pin the apps you use regularly to keep it at the end.

## Custom icons for other apps

The easiest way is the Dock Settings app (**Choose icon …**). To do it by hand,
add an entry under *Advanced → Apps and icons* with:

- **App IDs:** find them with `Get-StartApps` in PowerShell. Several IDs can be
  separated with commas.
- **PNG file:** a file name from the `Icons` folder.

 
## Known issues

- Multiple monitors: with two or more monitors connected, the dock can look too wide on some screens.
- Missing icons on first start: many apps show their original Windows icon at first, because App IDs differ between PCs. Open the Dock Settings app, go to Taskbar icons and use Choose icon … for     each app (use Rescan if an app is missing from the list).

  
## Troubleshooting

| Problem | Fix |
|---|---|
| Dock is cut off at the top | Increase *Taskbar height* or lower *Size* in the Dock Settings app |
| An icon does not change | The app uses a different App ID. Use **Choose icon …** in the Dock Settings app, it detects the correct ID |
| Taskbar looks broken after a Windows update | Disable the mod in Windhawk and wait for an update, or recompile it |
| Settings app says "Mod settings not found" | Install the **macOS Dock** mod first (step 4) |

## License

GPL-3.0, see [LICENSE](LICENSE).

## Credits

- **macOS Dock** is based on *Windows 11 Taskbar Styler* and *Taskbar height and
  icon size* by m417z (GPL v3). The `WindhawkBlur` brush is based on code from
  the TranslucentTB project.
- **Resource Redirect**, **Custom Window Corner Radius** by m417z;
  **MacOS Minimize Animation** by Abdullah Masood (Windhawk catalog).
- Top Menu Bar, macOS Traffic Lights, Dock Settings app: Okan.
