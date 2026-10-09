# macOS Dock – Settings
# Reads and writes the settings of the Windhawk mod "macOS Dock"
# (local@macos-dock-taskbar) directly in the registry. Windhawk applies
# changes immediately via the SettingsChangeTime timestamp.

param([switch]$Elevated)

# ---------------------------------------------------------------- Admin-Rechte
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process powershell.exe -Verb RunAs -WindowStyle Hidden -ArgumentList @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass', '-STA', '-WindowStyle', 'Hidden',
        '-File', "`"$PSCommandPath`"", '-Elevated')
    exit
}

Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase, UIAutomationClient, UIAutomationTypes, System.Windows.Forms, System.Drawing

Add-Type -TypeDefinition @'
namespace MacDock {
    public class AppEntry {
        public string Name { get; set; }
        public string Ids { get; set; }
        public string Datei { get; set; }
    }
}
'@
function New-AppEntry($n, $i, $f) {
    $a = New-Object MacDock.AppEntry
    $a.Name = [string]$n; $a.Ids = [string]$i; $a.Datei = [string]$f
    return $a
}

# Hide the console window
Add-Type -Name Win -Namespace Native -MemberDefinition @'
[DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
[DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
'@
[Native.Win]::ShowWindow([Native.Win]::GetConsoleWindow(), 0) | Out-Null

# ---------------------------------------------------------------- Konfiguration
$ModId       = 'local@macos-dock-taskbar'
$ModKeyPath  = "SOFTWARE\Windhawk\Engine\Mods\$ModId"
$SettingsKeyPath = "$ModKeyPath\Settings"
$AppDataDir  = Join-Path $env:APPDATA 'MacDockEinstellungen'
$DefaultsFile = Join-Path $AppDataDir 'meine-standardwerte.json'

# Numbers: key, label, min, max, group, mod default
$NumberDefs = @(
    @{K='taskbarHeight';  L='Taskbar height';              Min=40;  Max=100; G='Dock';  D=60}
    @{K='dockHeight';     L='Dock height';                 Min=30;  Max=90;  G='Dock';  D=52}
    @{K='cornerRadius';   L='Corner radius';                 Min=0;   Max=40;  G='Dock';  D=17}
    @{K='paddingX';       L='Padding left/right';          Min=0;   Max=30;  G='Dock';  D=3}
    @{K='dockBottom';     L='Distance to bottom edge';     Min=0;   Max=30;  G='Dock';  D=6}
    @{K='blurAmount';     L='Blur amount';                 Min=0;   Max=30;  G='Dock';  D=8}
    @{K='buttonWidth';    L='Space per icon';              Min=30;  Max=80;  G='Icons'; D=44}
    @{K='iconSize';       L='Custom icon size';            Min=20;  Max=70;  G='Icons'; D=38}
    @{K='nativeIconSize'; L='Other icon size (0 = default)'; Min=0; Max=64;  G='Icons'; D=29}
    @{K='iconOffsetY';    L='Icon position (+ = lower)';  Min=-10; Max=10;  G='Icons'; D=1}
    @{K='dotSize';        L='Dot size';                    Min=2;   Max=10;  G='Indicator'; D=4}
    @{K='dotOffset';      L='Dot offset (+ = lower)';      Min=-8;  Max=10;  G='Indicator'; D=1}
)
$TextDefs = @(
    @{K='tintColor';         L='Tint (#AARRGGBB)';           D='#10FFFFFF'}
    @{K='edgeColor';         L='Edge highlight (#AARRGGBB)';       D='#90FFFFFF'}
    @{K='dotActiveColor';    L='Dot active';                  D='#FFFFFFFF'}
    @{K='dotInactiveColor';  L='Dot background app';        D='#B0FFFFFF'}
    @{K='dotAttentionColor'; L='Dot attention';         D='#FFFF9F0A'}
    @{K='iconFolder';        L='Icon folder';                  D='%USERPROFILE%\Documents\MacOs-Dock\Icons'}
)
$BoolDefs = @(
    @{K='autoTaskbarHeight';   L='Fit taskbar height to dock (no gap above maximized windows)'; D=1}
    @{K='hideStart';           L='Hide Start button';                D=1}
    @{K='hideTray';            L='Hide system tray and clock';             D=1}
    @{K='hideHover';           L='Hide hover background';           D=1}
    @{K='clickThroughTaskbar'; L='Click-through empty taskbar areas'; D=1}
)

# ---------------------------------------------------------------- Registry
function Open-SettingsKey([bool]$write) {
    if ($write) { return [Microsoft.Win32.Registry]::LocalMachine.CreateSubKey($SettingsKeyPath) }
    return [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($SettingsKeyPath)
}

function Read-State {
    $s = @{ scale = 120; apps = New-Object System.Collections.ArrayList }
    foreach ($d in $NumberDefs) { $s[$d.K] = $d.D }
    foreach ($d in $TextDefs)   { $s[$d.K] = $d.D }
    foreach ($d in $BoolDefs)   { $s[$d.K] = $d.D }

    $key = Open-SettingsKey $false
    if ($key) {
        try {
            $v = $key.GetValue('scale'); if ($v -ne $null) { $s.scale = [int]$v }
            foreach ($d in $NumberDefs + $BoolDefs) { $v = $key.GetValue($d.K); if ($v -ne $null) { $s[$d.K] = [int]$v } }
            foreach ($d in $TextDefs) { $v = $key.GetValue($d.K); if ($v -ne $null) { $s[$d.K] = [string]$v } }
            for ($i = 0; $i -lt 200; $i++) {
                $n = $key.GetValue("apps[$i].name"); $ids = $key.GetValue("apps[$i].ids"); $f = $key.GetValue("apps[$i].file")
                if ($n -eq $null -and $ids -eq $null -and $f -eq $null) { break }
                [void]$s.apps.Add((New-AppEntry $n $ids $f))
            }
        } finally { $key.Close() }
    }
    return $s
}

function Write-State($s) {
    $key = Open-SettingsKey $true
    try {
        $key.SetValue('scale', [int]$s.scale, [Microsoft.Win32.RegistryValueKind]::DWord)
        foreach ($d in $NumberDefs + $BoolDefs) { $key.SetValue($d.K, [int]$s[$d.K], [Microsoft.Win32.RegistryValueKind]::DWord) }
        foreach ($d in $TextDefs) { $key.SetValue($d.K, [string]$s[$d.K], [Microsoft.Win32.RegistryValueKind]::String) }

        foreach ($name in $key.GetValueNames()) { if ($name -like 'apps`[*') { $key.DeleteValue($name) } }
        $i = 0
        foreach ($a in $s.apps) {
            if ([string]::IsNullOrWhiteSpace($a.Name) -and [string]::IsNullOrWhiteSpace($a.Ids) -and [string]::IsNullOrWhiteSpace($a.Datei)) { continue }
            $key.SetValue("apps[$i].name", [string]$a.Name, [Microsoft.Win32.RegistryValueKind]::String)
            $key.SetValue("apps[$i].ids",  [string]$a.Ids,  [Microsoft.Win32.RegistryValueKind]::String)
            $key.SetValue("apps[$i].file", [string]$a.Datei, [Microsoft.Win32.RegistryValueKind]::String)
            $i++
        }
    } finally { $key.Close() }

    # Windhawk benachrichtigen
    $modKey = [Microsoft.Win32.Registry]::LocalMachine.CreateSubKey($ModKeyPath)
    try {
        $t = [int]([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() -band 0x7fffffff)
        $modKey.SetValue('SettingsChangeTime', $t, [Microsoft.Win32.RegistryValueKind]::DWord)
    } finally { $modKey.Close() }
}

function Save-MyDefaults($s) {
    if (-not (Test-Path $AppDataDir)) { New-Item -ItemType Directory -Path $AppDataDir | Out-Null }
    $copy = @{}
    foreach ($k in $s.Keys) { if ($k -ne 'apps') { $copy[$k] = $s[$k] } }
    $copy.apps = @($s.apps | ForEach-Object { @{ Name = $_.Name; Ids = $_.Ids; Datei = $_.Datei } })
    $copy | ConvertTo-Json -Depth 5 | Set-Content -Path $DefaultsFile -Encoding UTF8
}

function Load-MyDefaults {
    if (-not (Test-Path $DefaultsFile)) { return $null }
    $j = Get-Content $DefaultsFile -Raw -Encoding UTF8 | ConvertFrom-Json
    $s = @{ apps = New-Object System.Collections.ArrayList }
    foreach ($p in $j.PSObject.Properties) { if ($p.Name -ne 'apps') { $s[$p.Name] = $p.Value } }
    foreach ($a in $j.apps) { [void]$s.apps.Add((New-AppEntry $a.Name $a.Ids $a.Datei)) }
    return $s
}

# ---------------------------------------------------------------- UI
[xml]$xaml = @'
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="macOS Dock – Settings" Width="560" SizeToContent="Height" MaxHeight="900"
        WindowStartupLocation="CenterScreen" Background="#F4F5F7" FontFamily="Segoe UI" FontSize="13"
        ResizeMode="CanMinimize">
  <Window.Resources>
    <Style TargetType="Button">
      <Setter Property="Padding" Value="12,6"/>
      <Setter Property="Margin" Value="0,0,8,0"/>
      <Setter Property="Background" Value="White"/>
      <Setter Property="BorderBrush" Value="#D0D4DA"/>
    </Style>
    <Style TargetType="TextBlock" x:Key="Head">
      <Setter Property="FontSize" Value="11"/>
      <Setter Property="FontWeight" Value="SemiBold"/>
      <Setter Property="Foreground" Value="#5D6675"/>
      <Setter Property="Margin" Value="0,14,0,6"/>
    </Style>
  </Window.Resources>
  <ScrollViewer VerticalScrollBarVisibility="Auto">
    <StackPanel Margin="20">
      <TextBlock Text="macOS Dock" FontSize="20" FontWeight="SemiBold"/>
      <TextBlock Name="Status" Foreground="#5D6675" Margin="0,2,0,14" TextWrapping="Wrap"/>

      <Border Background="White" CornerRadius="10" Padding="16" BorderBrush="#DDE1E6" BorderThickness="1">
        <StackPanel>
          <DockPanel>
            <TextBlock Text="Size" FontWeight="SemiBold" FontSize="14"/>
            <TextBlock Name="ScaleText" DockPanel.Dock="Right" HorizontalAlignment="Right" FontSize="14" FontWeight="SemiBold"/>
          </DockPanel>
          <Slider Name="ScaleSlider" Minimum="50" Maximum="200" TickFrequency="5" IsSnapToTickEnabled="True" Margin="0,10,0,4"/>
          <DockPanel>
            <TextBlock Text="smaller" Foreground="#8A92A0" FontSize="11"/>
            <TextBlock Text="larger" Foreground="#8A92A0" FontSize="11" HorizontalAlignment="Right"/>
          </DockPanel>
          <TextBlock Foreground="#5D6675" FontSize="12" Margin="0,10,0,0" TextWrapping="Wrap"
                     Text="100% equals your configured values. Changes are applied immediately."/>
        </StackPanel>
      </Border>

      <WrapPanel Margin="0,14,0,0">
        <Button Name="BtnReset" Content="My defaults"/>
        <Button Name="BtnSaveDefaults" Content="Save current as default"/>
        <Button Name="BtnRestart" Content="Restart Explorer"/>
      </WrapPanel>

      <DockPanel Margin="0,22,0,6">
        <Button Name="BtnScan" Content="Rescan" DockPanel.Dock="Right" Margin="0"/>
        <TextBlock Text="Taskbar icons" FontWeight="SemiBold" FontSize="14" VerticalAlignment="Center"/>
      </DockPanel>
      <TextBlock Foreground="#5D6675" FontSize="12" TextWrapping="Wrap" Margin="0,0,0,8"
                 Text="All apps currently on the taskbar. Use “Choose icon” to pick a PNG from your icon folder."/>
      <Border Background="White" CornerRadius="10" Padding="6" BorderBrush="#DDE1E6" BorderThickness="1">
        <StackPanel Name="TaskbarAppsPanel"/>
      </Border>

      <Expander Name="AdvExpander" Header="Advanced" Margin="0,18,0,0" FontWeight="SemiBold">
        <StackPanel Name="AdvPanel" Margin="0,6,0,0">
          <TextBlock Text="These values apply at 100% and are scaled together with the size." TextWrapping="Wrap"
                     FontWeight="Normal" Foreground="#5D6675"/>
        </StackPanel>
      </Expander>
    </StackPanel>
  </ScrollViewer>
</Window>
'@

$window = [Windows.Markup.XamlReader]::Load((New-Object System.Xml.XmlNodeReader $xaml))
$ui = @{}
foreach ($n in 'Status','ScaleText','ScaleSlider','BtnReset','BtnSaveDefaults','BtnRestart','AdvPanel','BtnScan','TaskbarAppsPanel') { $ui[$n] = $window.FindName($n) }

$state = Read-State

# Switch the icon folder to the new default automatically if the old one no longer exists
$script:folderMigrated = $false
$defaultFolder = ($TextDefs | Where-Object { $_.K -eq 'iconFolder' }).D
if (-not (Test-Path -LiteralPath ([Environment]::ExpandEnvironmentVariables([string]$state.iconFolder))) -and
    (Test-Path -LiteralPath ([Environment]::ExpandEnvironmentVariables($defaultFolder)))) {
    $state.iconFolder = $defaultFolder
    $script:folderMigrated = $true
}
$script:loading = $true

# First start: save the current values as "my defaults"
if (-not (Test-Path $DefaultsFile)) { Save-MyDefaults $state }

# Delayed saving so we don't write on every slider tick
$saveTimer = New-Object System.Windows.Threading.DispatcherTimer
$saveTimer.Interval = [TimeSpan]::FromMilliseconds(350)
$saveTimer.Add_Tick({
    $saveTimer.Stop()
    try {
        Write-State $state
        $ui.Status.Text = "Saved – $(Get-Date -Format 'HH:mm:ss')"
    } catch {
        $ui.Status.Text = "Error while saving: $($_.Exception.Message)"
    }
})
function Request-Save { if (-not $script:loading) { $saveTimer.Stop(); $saveTimer.Start() } }

# ----- Size
$ui.ScaleSlider.Value = $state.scale
$ui.ScaleText.Text = "$($state.scale) %"
$ui.ScaleSlider.Add_ValueChanged({
    $state.scale = [int]$ui.ScaleSlider.Value
    $ui.ScaleText.Text = "$($state.scale) %"
    Request-Save
})

# ----- Advanced: build controls
$controls = @{}
function Add-Header($text) {
    $tb = New-Object System.Windows.Controls.TextBlock
    $tb.Text = $text.ToUpper(); $tb.Style = $window.Resources['Head']
    [void]$ui.AdvPanel.Children.Add($tb)
}
function New-Row($label) {
    $g = New-Object System.Windows.Controls.Grid
    $g.Margin = '0,3,0,3'
    foreach ($w in '190','*','48') {
        $c = New-Object System.Windows.Controls.ColumnDefinition
        if ($w -eq '*') {
            $c.Width = New-Object System.Windows.GridLength(1, [System.Windows.GridUnitType]::Star)
        } else {
            $c.Width = New-Object System.Windows.GridLength([double]$w)
        }
        $g.ColumnDefinitions.Add($c)
    }
    $l = New-Object System.Windows.Controls.TextBlock
    $l.Text = $label; $l.FontWeight = 'Normal'; $l.VerticalAlignment = 'Center'
    [void]$g.Children.Add($l)
    return $g
}

$currentGroup = ''
foreach ($d in $NumberDefs) {
    if ($d.G -ne $currentGroup) { Add-Header $d.G; $currentGroup = $d.G }
    $row = New-Row $d.L
    $sl = New-Object System.Windows.Controls.Slider
    $sl.Minimum = $d.Min; $sl.Maximum = $d.Max; $sl.IsSnapToTickEnabled = $true; $sl.TickFrequency = 1
    $sl.VerticalAlignment = 'Center'; $sl.Margin = '8,0,8,0'
    [System.Windows.Controls.Grid]::SetColumn($sl, 1)
    $val = New-Object System.Windows.Controls.TextBlock
    $val.FontWeight = 'Normal'; $val.VerticalAlignment = 'Center'; $val.HorizontalAlignment = 'Right'
    [System.Windows.Controls.Grid]::SetColumn($val, 2)
    [void]$row.Children.Add($sl); [void]$row.Children.Add($val)
    [void]$ui.AdvPanel.Children.Add($row)
    $sl.Tag = @{ Key = $d.K; Label = $val }
    $sl.Add_ValueChanged({
        param($sender, $e)
        $t = $sender.Tag
        $state[$t.Key] = [int]$sender.Value
        $t.Label.Text = "$([int]$sender.Value) px"
        Request-Save
    })
    $controls[$d.K] = $sl
}

# ----- Color helpers (#AARRGGBB)
function ConvertFrom-Argb([string]$hex) {
    $h = ($hex -replace '[^0-9A-Fa-f]', '')
    if ($h.Length -eq 6) { $h = 'FF' + $h }
    if ($h.Length -ne 8) { $h = 'FFFFFFFF' }
    return @(
        [Convert]::ToByte($h.Substring(0, 2), 16), [Convert]::ToByte($h.Substring(2, 2), 16),
        [Convert]::ToByte($h.Substring(4, 2), 16), [Convert]::ToByte($h.Substring(6, 2), 16))
}
function ConvertTo-Argb($a, $r, $g, $b) { return ('#{0:X2}{1:X2}{2:X2}{3:X2}' -f [int]$a, [int]$r, [int]$g, [int]$b) }

function Update-ColorUI([string]$key) {
    $c = $controls[$key]
    $v = ConvertFrom-Argb $state[$key]
    $c.Swatch.Background = New-Object System.Windows.Media.SolidColorBrush ([System.Windows.Media.Color]::FromArgb($v[0], $v[1], $v[2], $v[3]))
    $c.Busy = $true
    $c.Slider.Value = $v[0]
    $c.Busy = $false
    $c.Pct.Text = "$([math]::Round($v[0] / 2.55)) %"
    $c.Hex.Text = $state[$key]
}

Add-Header 'Colors'
foreach ($d in ($TextDefs | Where-Object { $_.K -like '*Color' })) {
    $row = New-Row ($d.L -replace '\s*\(#AARRGGBB\)', '')
    $panel = New-Object System.Windows.Controls.StackPanel
    $panel.Orientation = 'Horizontal'; $panel.Margin = '8,0,0,0'
    [System.Windows.Controls.Grid]::SetColumn($panel, 1); [System.Windows.Controls.Grid]::SetColumnSpan($panel, 2)

    # Checkerboard behind the swatch so transparency is visible
    $checker = New-Object System.Windows.Controls.Border
    $checker.Width = 34; $checker.Height = 22; $checker.CornerRadius = 5
    $checker.BorderBrush = '#C9CED6'; $checker.BorderThickness = 1; $checker.Cursor = 'Hand'
    $checker.Background = New-Object System.Windows.Media.DrawingBrush
    $checker.Background.TileMode = 'Tile'; $checker.Background.ViewportUnits = 'Absolute'
    $checker.Background.Viewport = New-Object System.Windows.Rect(0, 0, 8, 8)
    $grp = New-Object System.Windows.Media.DrawingGroup
    $grp.Children.Add((New-Object System.Windows.Media.GeometryDrawing([System.Windows.Media.Brushes]::White, $null, (New-Object System.Windows.Media.RectangleGeometry((New-Object System.Windows.Rect(0, 0, 8, 8)))))))
    $grp.Children.Add((New-Object System.Windows.Media.GeometryDrawing([System.Windows.Media.Brushes]::LightGray, $null, (New-Object System.Windows.Media.RectangleGeometry((New-Object System.Windows.Rect(0, 0, 4, 4)))))))
    $grp.Children.Add((New-Object System.Windows.Media.GeometryDrawing([System.Windows.Media.Brushes]::LightGray, $null, (New-Object System.Windows.Media.RectangleGeometry((New-Object System.Windows.Rect(4, 4, 4, 4)))))))
    $checker.Background.Drawing = $grp
    $swatch = New-Object System.Windows.Controls.Border
    $swatch.CornerRadius = 4
    $checker.Child = $swatch
    $checker.ToolTip = 'Choose color'

    $pick = New-Object System.Windows.Controls.Button
    $pick.Content = 'Choose …'; $pick.Margin = '8,0,8,0'; $pick.Padding = '10,3'; $pick.FontWeight = 'Normal'

    $op = New-Object System.Windows.Controls.Slider
    $op.Minimum = 0; $op.Maximum = 255; $op.Width = 120; $op.VerticalAlignment = 'Center'
    $op.ToolTip = 'Opacity'
    $pct = New-Object System.Windows.Controls.TextBlock
    $pct.Width = 42; $pct.Margin = '6,0,6,0'; $pct.VerticalAlignment = 'Center'; $pct.FontWeight = 'Normal'; $pct.Foreground = '#5D6675'

    $hex = New-Object System.Windows.Controls.TextBox
    $hex.Width = 92; $hex.FontFamily = 'Consolas'; $hex.FontWeight = 'Normal'; $hex.Padding = '4,2'; $hex.VerticalAlignment = 'Center'

    foreach ($el in $checker, $pick, $op, $pct, $hex) { [void]$panel.Children.Add($el) }
    [void]$row.Children.Add($panel)
    [void]$ui.AdvPanel.Children.Add($row)

    $controls[$d.K] = @{ Swatch = $swatch; Slider = $op; Pct = $pct; Hex = $hex; Busy = $false }
    foreach ($el in $checker, $pick, $op, $hex) { $el.Tag = $d.K }

    $openPicker = {
        param($sender, $e)
        $k = $sender.Tag
        $v = ConvertFrom-Argb $state[$k]
        $dlg = New-Object System.Windows.Forms.ColorDialog
        $dlg.FullOpen = $true
        $dlg.Color = [System.Drawing.Color]::FromArgb($v[1], $v[2], $v[3])
        if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
            $state[$k] = ConvertTo-Argb $v[0] $dlg.Color.R $dlg.Color.G $dlg.Color.B
            Update-ColorUI $k
            Request-Save
        }
    }
    $pick.Add_Click($openPicker)
    $checker.Add_MouseLeftButtonUp($openPicker)
    $op.Add_ValueChanged({
        param($sender, $e)
        $k = $sender.Tag
        if ($controls[$k].Busy) { return }
        $v = ConvertFrom-Argb $state[$k]
        $state[$k] = ConvertTo-Argb ([int]$sender.Value) $v[1] $v[2] $v[3]
        Update-ColorUI $k
        Request-Save
    })
    $hex.Add_LostFocus({
        param($sender, $e)
        $k = $sender.Tag
        $v = ConvertFrom-Argb $sender.Text
        $state[$k] = ConvertTo-Argb $v[0] $v[1] $v[2] $v[3]
        Update-ColorUI $k
        Request-Save
    })
}

Add-Header 'Icon folder'
foreach ($d in ($TextDefs | Where-Object { $_.K -notlike '*Color' })) {
    $row = New-Row $d.L
    $tb = New-Object System.Windows.Controls.TextBox
    $tb.FontWeight = 'Normal'; $tb.Margin = '8,0,0,0'; $tb.Padding = '4,2'
    [System.Windows.Controls.Grid]::SetColumn($tb, 1); [System.Windows.Controls.Grid]::SetColumnSpan($tb, 2)
    [void]$row.Children.Add($tb)
    [void]$ui.AdvPanel.Children.Add($row)
    $tb.Tag = $d.K
    $tb.Add_LostFocus({ param($sender, $e) $state[$sender.Tag] = $sender.Text; Request-Save })
    $controls[$d.K] = $tb
}

Add-Header 'Elements'
foreach ($d in $BoolDefs) {
    $cb = New-Object System.Windows.Controls.CheckBox
    $cb.Content = $d.L; $cb.FontWeight = 'Normal'; $cb.Margin = '0,4,0,4'
    $cb.Tag = $d.K
    $cb.Add_Click({ param($sender, $e) $state[$sender.Tag] = [int][bool]$sender.IsChecked; Request-Save })
    [void]$ui.AdvPanel.Children.Add($cb)
    $controls[$d.K] = $cb
}

Add-Header 'Apps and icons'
$grid = New-Object System.Windows.Controls.DataGrid
$grid.AutoGenerateColumns = $false; $grid.CanUserAddRows = $true; $grid.CanUserDeleteRows = $true
$grid.HeadersVisibility = 'Column'; $grid.FontWeight = 'Normal'; $grid.MaxHeight = 320
$grid.Background = 'White'; $grid.RowBackground = 'White'; $grid.GridLinesVisibility = 'Horizontal'
foreach ($c in @(@('Name','Name',120), @('App IDs (comma)','Ids',260), @('PNG file','Datei',130))) {
    $col = New-Object System.Windows.Controls.DataGridTextColumn
    $col.Header = $c[0]; $col.Binding = New-Object System.Windows.Data.Binding($c[1])
    $col.Width = New-Object System.Windows.Controls.DataGridLength([double]$c[2])
    $grid.Columns.Add($col)
}
$hint = New-Object System.Windows.Controls.TextBlock
$hint.Text = 'Click the empty row at the bottom to add, select a row and press Delete to remove. App IDs: run "Get-StartApps" in PowerShell.'
$hint.TextWrapping = 'Wrap'; $hint.FontWeight = 'Normal'; $hint.Foreground = '#5D6675'; $hint.Margin = '0,6,0,0'
[void]$ui.AdvPanel.Children.Add($grid)
[void]$ui.AdvPanel.Children.Add($hint)
$grid.Add_RowEditEnding({ $window.Dispatcher.BeginInvoke([action]{ Request-Save }, 'Background') | Out-Null })
$grid.Add_PreviewKeyUp({ param($s, $e) if ($e.Key -eq 'Delete') { Request-Save } })

function Show-State {
    $script:loading = $true
    $ui.ScaleSlider.Value = $state.scale
    $ui.ScaleText.Text = "$($state.scale) %"
    foreach ($d in $NumberDefs) { $controls[$d.K].Value = [int]$state[$d.K]; $controls[$d.K].Tag.Label.Text = "$([int]$state[$d.K]) px" }
    foreach ($d in $TextDefs) { if ($d.K -like '*Color') { Update-ColorUI $d.K } else { $controls[$d.K].Text = [string]$state[$d.K] } }
    foreach ($d in $BoolDefs) { $controls[$d.K].IsChecked = [bool][int]$state[$d.K] }
    $list = New-Object 'System.Collections.ObjectModel.ObservableCollection[MacDock.AppEntry]'
    foreach ($a in $state.apps) { $list.Add($a) }
    $state.apps = $list
    $grid.ItemsSource = $list
    $script:loading = $false
}


# ----- Detect apps on the taskbar (UI Automation)
function Get-TaskbarApps {
    $result = New-Object System.Collections.ArrayList
    $seen = @{}
    try {
        $root = [System.Windows.Automation.AutomationElement]::RootElement
        $cond = New-Object System.Windows.Automation.PropertyCondition(
            [System.Windows.Automation.AutomationElement]::ClassNameProperty, 'Shell_TrayWnd')
        $tray = $root.FindFirst([System.Windows.Automation.TreeScope]::Children, $cond)
        if (-not $tray) { return $result }
        $all = $tray.FindAll([System.Windows.Automation.TreeScope]::Descendants,
            [System.Windows.Automation.Condition]::TrueCondition)
        foreach ($el in $all) {
            $id = $el.Current.AutomationId
            if (-not $id -or -not $id.StartsWith('Appid:')) { continue }
            $appId = $id.Substring(6).Trim()
            if (-not $appId -or $seen.ContainsKey($appId)) { continue }
            $seen[$appId] = $true
            $name = ($el.Current.Name -replace '\s[-–]\s.*$', '').Trim()
            if (-not $name) { $name = $appId }
            [void]$result.Add([pscustomobject]@{ Name = $name; AppId = $appId })
        }
    } catch {
        $ui.Status.Text = "Could not read the taskbar: $($_.Exception.Message)"
    }
    return $result
}

function Get-IdList([string]$ids) {
    if (-not $ids) { return @() }
    return @($ids.Split(',') | ForEach-Object { $_.Trim() } | Where-Object { $_ })
}

function Find-Entry([string]$appId) {
    foreach ($a in $state.apps) {
        foreach ($i in (Get-IdList $a.Ids)) { if ($i -ieq $appId) { return $a } }
    }
    return $null
}

function Get-IconFolder { return [Environment]::ExpandEnvironmentVariables([string]$state.iconFolder) }

function Get-IconPath($entry) {
    if (-not $entry -or -not $entry.Datei) { return $null }
    $p = Join-Path (Get-IconFolder) $entry.Datei
    if (Test-Path -LiteralPath $p) { return $p }
    return $null
}

function New-Thumb([string]$path) {
    $img = New-Object System.Windows.Controls.Image
    $img.Width = 32; $img.Height = 32; $img.Margin = '4,0,10,0'
    if ($path) {
        try {
            $bmp = New-Object System.Windows.Media.Imaging.BitmapImage
            $bmp.BeginInit()
            $bmp.CacheOption = [System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad
            $bmp.DecodePixelWidth = 64
            $bmp.UriSource = New-Object System.Uri($path)
            $bmp.EndInit()
            $img.Source = $bmp
        } catch {}
    }
    return $img
}

function Set-AppIcon([string]$appId, [string]$name) {
    $dlg = New-Object Microsoft.Win32.OpenFileDialog
    $dlg.Title = "Choose icon for $name"
    $dlg.Filter = 'PNG images (*.png)|*.png'
    if (Test-Path -LiteralPath (Get-IconFolder)) { $dlg.InitialDirectory = Get-IconFolder }
    if (-not $dlg.ShowDialog($window)) { return }

    $src = $dlg.FileName
    $file = Split-Path $src -Leaf
    $folder = (Get-IconFolder).TrimEnd('\')
    if ((Split-Path $src -Parent).TrimEnd('\') -ine $folder) {
        if (-not (Test-Path -LiteralPath $folder)) { New-Item -ItemType Directory -Path $folder | Out-Null }
        Copy-Item -LiteralPath $src -Destination (Join-Path $folder $file) -Force
    }

    $entry = Find-Entry $appId
    if ($entry) {
        $entry.Datei = $file
    } else {
        $state.apps.Add((New-AppEntry $name $appId $file))
    }
    $grid.Items.Refresh()
    Write-State $state
    $ui.Status.Text = "Icon set for $name."
    Show-TaskbarApps
}

function Remove-AppIcon([string]$appId, [string]$name) {
    $entry = Find-Entry $appId
    if (-not $entry) { return }
    $rest = @(Get-IdList $entry.Ids | Where-Object { $_ -ine $appId })
    if ($rest.Count -eq 0) { [void]$state.apps.Remove($entry) } else { $entry.Ids = ($rest -join ', ') }
    $grid.Items.Refresh()
    Write-State $state
    $ui.Status.Text = "Custom icon removed for $name."
    Show-TaskbarApps
}

function Show-TaskbarApps {
    $panel = $ui.TaskbarAppsPanel
    $panel.Children.Clear()
    $apps = Get-TaskbarApps
    if ($apps.Count -eq 0) {
        $t = New-Object System.Windows.Controls.TextBlock
        $t.Text = 'No apps found. Click “Rescan”.'
        $t.Margin = '8'; $t.Foreground = '#5D6675'
        [void]$panel.Children.Add($t)
        return
    }
    foreach ($a in $apps) {
        $entry = Find-Entry $a.AppId
        $row = New-Object System.Windows.Controls.DockPanel
        $row.Margin = '4,5,4,5'

        $btnPanel = New-Object System.Windows.Controls.StackPanel
        $btnPanel.Orientation = 'Horizontal'
        [System.Windows.Controls.DockPanel]::SetDock($btnPanel, 'Right')
        $pick = New-Object System.Windows.Controls.Button
        $pick.Content = 'Choose icon …'
        $pick.Tag = @{ AppId = $a.AppId; Name = $a.Name }
        $pick.Add_Click({ param($sender, $e) Set-AppIcon $sender.Tag.AppId $sender.Tag.Name })
        [void]$btnPanel.Children.Add($pick)
        if ($entry) {
            $del = New-Object System.Windows.Controls.Button
            $del.Content = 'Original'
            $del.ToolTip = 'Remove custom icon'
            $del.Tag = @{ AppId = $a.AppId; Name = $a.Name }
            $del.Add_Click({ param($sender, $e) Remove-AppIcon $sender.Tag.AppId $sender.Tag.Name })
            [void]$btnPanel.Children.Add($del)
        }
        [void]$row.Children.Add($btnPanel)

        [void]$row.Children.Add((New-Thumb (Get-IconPath $entry)))

        $txt = New-Object System.Windows.Controls.StackPanel
        $txt.VerticalAlignment = 'Center'
        $n = New-Object System.Windows.Controls.TextBlock
        $n.Text = $a.Name; $n.FontWeight = 'SemiBold'
        $sub = New-Object System.Windows.Controls.TextBlock
        $sub.FontSize = 11; $sub.Foreground = '#8A92A0'; $sub.TextTrimming = 'CharacterEllipsis'
        if ($entry) { $sub.Text = "$($entry.Datei)  ·  $($a.AppId)" } else { $sub.Text = "Original icon  ·  $($a.AppId)" }
        $sub.ToolTip = $a.AppId
        [void]$txt.Children.Add($n); [void]$txt.Children.Add($sub)
        [void]$row.Children.Add($txt)

        [void]$panel.Children.Add($row)
    }
}

$ui.BtnScan.Add_Click({ Show-TaskbarApps; $ui.Status.Text = 'Taskbar rescanned.' })

# ----- Buttons
$ui.BtnReset.Add_Click({
    $d = Load-MyDefaults
    if (-not $d) { $ui.Status.Text = 'No saved defaults found.'; return }
    foreach ($k in @($d.Keys)) { $state[$k] = $d[$k] }
    Show-State
    Write-State $state
    Show-TaskbarApps
    $ui.Status.Text = 'My defaults restored.'
})
$ui.BtnSaveDefaults.Add_Click({
    Save-MyDefaults $state
    $ui.Status.Text = 'Current settings saved as “My defaults”.'
})
$ui.BtnRestart.Add_Click({
    Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 800
    if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }
    $ui.Status.Text = 'Explorer restarted.'
})

$window.Add_Closing({ if ($saveTimer.IsEnabled) { $saveTimer.Stop(); try { Write-State $state } catch {} } })

Show-State
Show-TaskbarApps
if ($script:folderMigrated) { try { Write-State $state } catch {} }
if (-not ([Microsoft.Win32.Registry]::LocalMachine.OpenSubKey($SettingsKeyPath))) {
    $ui.Status.Text = 'Mod settings not found – is the “macOS Dock” mod installed in Windhawk?'
} else {
    $ui.Status.Text = 'Connected to the Windhawk mod “macOS Dock”.'
}

[void]$window.ShowDialog()
