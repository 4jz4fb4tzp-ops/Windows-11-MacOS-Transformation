# Creates a Recycle Bin shortcut with a macOS icon and its own App ID (MacDock.Trash),
# so the taskbar doesn't group it with File Explorer.
$base   = Join-Path $env:USERPROFILE "Documents\MacOs-Dock"
$lnk    = Join-Path $base "Files\Trash.lnk"
$icon   = Join-Path $base "Icons\trash (empty).ico"

$ws = New-Object -ComObject WScript.Shell
$sc = $ws.CreateShortcut($lnk)
$sc.TargetPath   = "$env:WINDIR\explorer.exe"
$sc.Arguments    = "shell:RecycleBinFolder"
$sc.IconLocation = "$icon,0"
$sc.Description  = "Trash"
$sc.Save()

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;

[StructLayout(LayoutKind.Sequential)]
public struct PROPERTYKEY { public Guid fmtid; public uint pid; }

[StructLayout(LayoutKind.Explicit)]
public struct PROPVARIANT {
    [FieldOffset(0)] public ushort vt;
    [FieldOffset(8)] public IntPtr p;
    [FieldOffset(16)] public long pad;
}

[ComImport, Guid("886D8EEB-8CF2-4446-8D02-CDBA1DBDCF99"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface IPropertyStore {
    [PreserveSig] int GetCount(out uint c);
    [PreserveSig] int GetAt(uint i, out PROPERTYKEY k);
    [PreserveSig] int GetValue(ref PROPERTYKEY k, out PROPVARIANT v);
    [PreserveSig] int SetValue(ref PROPERTYKEY k, ref PROPVARIANT v);
    [PreserveSig] int Commit();
}

public static class AppIdSetter {
    public static void Set(string path, string id) {
        Type t = Type.GetTypeFromCLSID(new Guid("00021401-0000-0000-C000-000000000046"));
        object link = Activator.CreateInstance(t);
        ((IPersistFile)link).Load(path, 2);
        IPropertyStore ps = (IPropertyStore)link;
        PROPERTYKEY key = new PROPERTYKEY();
        key.fmtid = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3");
        key.pid = 5;
        PROPVARIANT pv = new PROPVARIANT();
        pv.vt = 31;
        pv.p = Marshal.StringToCoTaskMemUni(id);
        int hr = ps.SetValue(ref key, ref pv);
        if (hr != 0) throw new Exception("SetValue failed: " + hr);
        ps.Commit();
        ((IPersistFile)link).Save(path, true);
        Marshal.FreeCoTaskMem(pv.p);
    }
}
"@

[AppIdSetter]::Set($lnk, "MacDock.Trash")
Write-Host "Done: $lnk"
Write-Host "Now right-click Trash.lnk -> Show more options -> Pin to taskbar"
# Open the folder with Trash.lnk selected
Start-Process explorer.exe "/select,`"$lnk`""
Read-Host "Press Enter to close"
