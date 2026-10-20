# Windows 11 macOS Transformation - one-click setup
#
# Installs Windhawk (if needed), all catalog mods and the custom mods from this
# repository with the recommended settings, copies the project to
# Documents\MacOs-Dock, applies a few taskbar settings and restarts Explorer.
#
# Start it with Setup.cmd in the repository root.
#
#   -ResetSettings   overwrite the settings of mods that are already installed
#   -All             install the optional mods without asking
#   -NoPrompt        don't ask anything, use the defaults
#   -IconPack <path> ZIP file or folder with the macOS icons
#   -Optional <ids>  comma-separated optional mods to install ("none" = none)
#   -Gui             used by Setup.exe: no questions, progress as "@@" lines

param(
    [switch]$ResetSettings,
    [switch]$All,
    [switch]$NoPrompt,
    [switch]$Elevated,
    [string]$IconPack = '',
    [string]$Optional = '',
    [switch]$Gui
)

if ($Gui) {
    $NoPrompt = $true
    [Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
}

# Progress for Setup.exe: one line per event, e.g. "@@STEP|mods".
function Send-Gui([string]$line) {
    if ($Gui) { [Console]::Out.WriteLine('@@' + $line); [Console]::Out.Flush() }
}

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# ---------------------------------------------------------------- Admin rights
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Elevated')
    if ($ResetSettings) { $argList += '-ResetSettings' }
    if ($All) { $argList += '-All' }
    if ($NoPrompt) { $argList += '-NoPrompt' }
    if ($IconPack) { $argList += @('-IconPack', "`"$IconPack`"") }
    if ($Optional) { $argList += @('-Optional', $Optional) }
    Start-Process powershell.exe -Verb RunAs -ArgumentList $argList
    exit
}

[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
$Host.UI.RawUI.WindowTitle = 'macOS Transformation - Setup'

$RepoRoot   = Split-Path -Parent $PSScriptRoot
$Documents  = [Environment]::GetFolderPath('MyDocuments')
$TargetRoot = Join-Path $Documents 'MacOs-Dock'
$ModsUrl    = 'https://mods.windhawk.net/mods/'
$TempRoot   = Join-Path $env:TEMP ('macos-setup-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null

$script:Failed = New-Object System.Collections.ArrayList

function Write-Step($text) { Write-Host ''; Write-Host "==> $text" -ForegroundColor Cyan }
function Write-Ok($text)   { Write-Host "    $text" -ForegroundColor Green }
function Write-Info($text) { Write-Host "    $text" }
function Write-Warn($text) { Write-Host "    $text" -ForegroundColor Yellow }

function Ask-YesNo($question, [bool]$default) {
    if ($NoPrompt) { return $default }
    $hint = if ($default) { '[Y/n]' } else { '[y/N]' }
    while ($true) {
        $answer = (Read-Host "    $question $hint").Trim().ToLower()
        if ($answer -eq '') { return $default }
        if ($answer -in @('y', 'yes', 'j', 'ja')) { return $true }
        if ($answer -in @('n', 'no', 'nein')) { return $false }
    }
}

# Quotes one argument for the Windows command line (CommandLineToArgvW rules).
function Quote-Arg([string]$arg) {
    if ($arg -ne '' -and $arg -notmatch '[\s"]') { return $arg }
    $result = '"'
    $backslashes = 0
    foreach ($c in $arg.ToCharArray()) {
        if ($c -eq '\') { $backslashes++; continue }
        if ($c -eq '"') { $result += ('\' * ($backslashes * 2 + 1)) + '"' }
        else { $result += ('\' * $backslashes) + $c }
        $backslashes = 0
    }
    return $result + ('\' * ($backslashes * 2)) + '"'
}

# Splits compiler options like Windhawk does (spaces, single and double quotes).
function Split-Args([string]$text) {
    $list = New-Object System.Collections.ArrayList
    $buffer = ''
    $single = $false; $double = $false
    foreach ($c in $text.ToCharArray()) {
        if ($c -eq "'" -and -not $double) { $single = -not $single; continue }
        if ($c -eq '"' -and -not $single) { $double = -not $double; continue }
        if (-not $single -and -not $double -and [char]::IsWhiteSpace($c)) {
            if ($buffer.Length -gt 0) { [void]$list.Add($buffer); $buffer = '' }
        } else { $buffer += $c }
    }
    if ($buffer.Length -gt 0) { [void]$list.Add($buffer) }
    return , $list.ToArray()
}

function Download($url, $path) {
    Invoke-WebRequest -Uri $url -OutFile $path -UseBasicParsing -TimeoutSec 120
}

try {

Write-Host ''
Write-Host '  macOS Transformation for Windows 11 - Setup' -ForegroundColor White
Write-Host '  https://github.com/4jz4fb4tzp-ops/Windows-11-MacOS-Transformation' -ForegroundColor DarkGray

$manifestPath = Join-Path $PSScriptRoot 'mods.json'
if (-not (Test-Path $manifestPath)) { throw "mods.json not found next to Setup.ps1." }
$manifest = Get-Content $manifestPath -Raw -Encoding UTF8 | ConvertFrom-Json

# ---------------------------------------------------------------- 1. Project folder
Send-Gui 'STEP|project'
Write-Step "Copying the project to $TargetRoot"
$sameFolder = (Resolve-Path $RepoRoot).Path.TrimEnd('\') -ieq $TargetRoot.TrimEnd('\')
if ($sameFolder) {
    Write-Info 'Setup already runs from that folder.'
} else {
    New-Item -ItemType Directory -Path $TargetRoot -Force | Out-Null
    foreach ($item in @('Files', 'setup', 'tools', 'README.md', 'LICENSE', 'Setup.cmd')) {
        $src = Join-Path $RepoRoot $item
        if (Test-Path $src) { Copy-Item $src -Destination $TargetRoot -Recurse -Force }
    }
    Get-ChildItem $TargetRoot -Recurse -File | Unblock-File -ErrorAction SilentlyContinue
    Write-Ok 'Done.'
}
$IconsDir = Join-Path $TargetRoot 'Icons'
New-Item -ItemType Directory -Path $IconsDir -Force | Out-Null

# ---------------------------------------------------------------- 2. Icons
Send-Gui 'STEP|icons'
Write-Step 'macOS icons'
$iconCount = @(Get-ChildItem $IconsDir -File -Include *.png, *.ico -Recurse -ErrorAction SilentlyContinue).Count
if ($IconPack -or ($iconCount -eq 0 -and -not $NoPrompt)) {
    if ($IconPack) {
        $iconSource = $IconPack.Trim().Trim('"')
    } else {
        Write-Info 'The macOS icons are not included (Apple copyright). If you already downloaded an'
        Write-Info 'icon pack, drag the ZIP file or folder into this window and press Enter.'
        Write-Info 'Press Enter without a path to skip - you can copy them into the Icons folder later.'
        $iconSource = (Read-Host '    Icon pack').Trim().Trim('"')
    }
    if ($iconSource -and (Test-Path $iconSource)) {
        $iconRoot = $iconSource
        if ((Get-Item $iconSource) -is [System.IO.FileInfo]) {
            $iconRoot = Join-Path $TempRoot 'icons'
            Expand-Archive -Path $iconSource -DestinationPath $iconRoot -Force
        }
        $copied = 0
        foreach ($f in Get-ChildItem $iconRoot -Recurse -File -Include *.png, *.ico) {
            Copy-Item $f.FullName -Destination (Join-Path $IconsDir $f.Name) -Force
            $copied++
        }
        Write-Ok "$copied icons copied."
    } else {
        Write-Warn 'Skipped. The dock works without them, but shows the Windows icons.'
    }
} elseif ($iconCount -gt 0) {
    Write-Ok "$iconCount icons found in the Icons folder."
}

# ---------------------------------------------------------------- 3. Windhawk
Send-Gui 'STEP|windhawk'
Write-Step 'Windhawk'
function Find-Windhawk {
    $candidates = New-Object System.Collections.ArrayList
    foreach ($key in @('HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Windhawk',
                       'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Windhawk')) {
        $loc = (Get-ItemProperty $key -ErrorAction SilentlyContinue).InstallLocation
        if ($loc) { [void]$candidates.Add($loc.Trim('"')) }
    }
    [void]$candidates.Add((Join-Path $env:ProgramFiles 'Windhawk'))
    foreach ($dir in $candidates) {
        if (Test-Path (Join-Path $dir 'windhawk.exe')) { return $dir }
    }
    return $null
}

$WindhawkDir = Find-Windhawk
if ($WindhawkDir) {
    Write-Ok "Found in $WindhawkDir"
} else {
    Write-Info 'Not installed - installing Windhawk ...'
    $installed = $false
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        & winget install --id RamenSoftware.Windhawk -e --silent --accept-package-agreements --accept-source-agreements | Out-Host
        $WindhawkDir = Find-Windhawk
        $installed = [bool]$WindhawkDir
    }
    if (-not $installed) {
        $setupExe = Join-Path $TempRoot 'windhawk_setup.exe'
        Download 'https://github.com/ramensoftware/windhawk/releases/latest/download/windhawk_setup.exe' $setupExe
        Start-Process $setupExe -ArgumentList '/S' -Wait
        for ($i = 0; $i -lt 30 -and -not $WindhawkDir; $i++) { Start-Sleep 1; $WindhawkDir = Find-Windhawk }
    }
    if (-not $WindhawkDir) { throw 'Windhawk could not be installed. Install it from https://windhawk.net and run the setup again.' }
    Write-Ok "Installed in $WindhawkDir"
}

# Storage paths from windhawk.ini
$ini = @{}
foreach ($line in Get-Content (Join-Path $WindhawkDir 'windhawk.ini')) {
    if ($line -match '^\s*([^=;\[]+?)\s*=\s*(.*?)\s*$') { $ini[$Matches[1]] = $Matches[2] }
}
function Resolve-StoragePath($value) {
    $expanded = [Environment]::ExpandEnvironmentVariables($value)
    if ([System.IO.Path]::IsPathRooted($expanded)) { return $expanded }
    return [System.IO.Path]::GetFullPath((Join-Path $WindhawkDir $expanded))
}
if ($ini['Portable'] -eq '1') { throw 'Portable Windhawk is not supported by this setup. Please use the installed version.' }
$AppDataPath  = Resolve-StoragePath $ini['AppDataPath']
$EnginePath   = Resolve-StoragePath $ini['EnginePath']
$CompilerPath = Resolve-StoragePath $ini['CompilerPath']
$RegistryKey  = $ini['RegistryKey'] -replace '^(HKEY_LOCAL_MACHINE|HKLM)\\', 'HKLM:\' -replace '^(HKEY_CURRENT_USER|HKCU)\\', 'HKCU:\'
$ModsRegPath  = "$RegistryKey\Engine\Mods"
$ModsDllPath  = Join-Path $AppDataPath 'Engine\Mods'
$ModsSrcPath  = Join-Path $AppDataPath 'ModsSource'
$Clang        = Join-Path $CompilerPath 'bin\clang++.exe'
New-Item -ItemType Directory -Path $ModsSrcPath -Force | Out-Null

$Subfolders = @{ '32' = 'i686-w64-mingw32'; '64' = 'x86_64-w64-mingw32' }

# Runtime libraries the mods need (Windhawk normally copies them when its UI starts).
foreach ($sub in $Subfolders.Keys) {
    $target = $Subfolders[$sub]
    $dest = Join-Path $ModsDllPath $sub
    New-Item -ItemType Directory -Path $dest -Force | Out-Null
    foreach ($pair in @(@('libc++.dll', 'libc++.whl'), @('libunwind.dll', 'libunwind.whl'), @('windhawk-mod-shim.dll', 'windhawk-mod-shim.dll'))) {
        $from = Join-Path $CompilerPath "$target\bin\$($pair[0])"
        $to = Join-Path $dest $pair[1]
        if ((Test-Path $from) -and -not ((Test-Path $to) -and (Get-Item $to).LastWriteTime -eq (Get-Item $from).LastWriteTime)) {
            try { Copy-Item $from $to -Force } catch { Write-Warn "Could not update $to (in use)." }
        }
    }
}

# ---------------------------------------------------------------- Mod helpers
function Get-Subfolders($architecture) {
    $archs = @($architecture)
    if ($archs.Count -eq 0) { $archs = @('x86', 'x86-64') }
    $subs = New-Object System.Collections.ArrayList
    foreach ($a in $archs) {
        if ($a -eq 'x86' -and -not $subs.Contains('32')) { [void]$subs.Add('32') }
        if ($a -in @('x86-64', 'amd64') -and -not $subs.Contains('64')) { [void]$subs.Add('64') }
    }
    return , $subs.ToArray()
}

function New-DllName($regId, $version) {
    while ($true) {
        $name = "{0}_{1}_{2}.dll" -f $regId, $version, (Get-Random -Minimum 100000 -Maximum 999999)
        if (-not (Test-Path (Join-Path $ModsDllPath "64\$name")) -and -not (Test-Path (Join-Path $ModsDllPath "32\$name"))) { return $name }
    }
}

function Invoke-Compile($regId, $mod, $sourceFile, $dllName) {
    if (-not (Test-Path $Clang)) { throw "Windhawk compiler not found ($Clang)." }
    foreach ($sub in (Get-Subfolders $mod.architecture)) {
        $target = $Subfolders[$sub]
        $out = Join-Path $ModsDllPath "$sub\$dllName"
        $clangArgs = @(
            '-std=c++23', '-O2', '-shared', '-DUNICODE', '-D_UNICODE',
            '-DWINVER=0x0A00', '-D_WIN32_WINNT=0x0A00', '-D_WIN32_IE=0x0A00', '-DNTDDI_VERSION=0x0A000008',
            '-D__USE_MINGW_ANSI_STDIO=0', '-DWH_MOD',
            ('-DWH_MOD_ID=L"' + $regId + '"'), ('-DWH_MOD_VERSION=L"' + $mod.version + '"'),
            (Join-Path $EnginePath "$sub\windhawk.lib"),
            '-x', 'c++', $sourceFile,
            '-include', 'windhawk_api.h', '-target', $target, '-Wl,--export-all-symbols',
            '-o', $out
        ) + (Split-Args $mod.compilerOptions)

        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Clang
        $psi.Arguments = ($clangArgs | ForEach-Object { Quote-Arg $_ }) -join ' '
        $psi.WorkingDirectory = $CompilerPath
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $p = [System.Diagnostics.Process]::Start($psi)
        $stdout = $p.StandardOutput.ReadToEndAsync()
        $stderr = $p.StandardError.ReadToEndAsync()
        $p.WaitForExit()
        if ($p.ExitCode -ne 0) {
            $log = Join-Path $TargetRoot "setup-error-$($mod.id).log"
            ($stdout.Result + "`r`n" + $stderr.Result) | Set-Content $log -Encoding UTF8
            throw "Compilation failed for $sub-bit (exit code $($p.ExitCode)). Details: $log"
        }
    }
}

function Install-Precompiled($mod, $dllName) {
    foreach ($sub in (Get-Subfolders $mod.architecture)) {
        Download ("{0}{1}/{2}_{3}.dll" -f $ModsUrl, $mod.id, $mod.version, $sub) (Join-Path $ModsDllPath "$sub\$dllName")
    }
}

function Remove-OldDlls($regId, $keepName) {
    foreach ($sub in @('32', '64', 'arm64')) {
        $dir = Join-Path $ModsDllPath $sub
        if (-not (Test-Path $dir)) { continue }
        Get-ChildItem $dir -Filter "$regId`_*.dll" -File | Where-Object { $_.Name -ne $keepName } | ForEach-Object {
            try { Remove-Item $_.FullName -Force } catch { }
        }
    }
}

function Set-RegValue($path, $name, $value) {
    if ($value -is [string]) {
        Set-ItemProperty -Path $path -Name $name -Value $value -Type String
    } else {
        $n = [long]$value
        if ($n -gt [int]::MaxValue) { $n = $n - 4294967296 }
        Set-ItemProperty -Path $path -Name $name -Value ([int]$n) -Type DWord
    }
}

function Get-ArrayPrefix($name) { return ($name -replace '\[\d+\].*$', '[0]') }

function Write-ModConfig($regId, $mod, $dllName) {
    $key = "$ModsRegPath\$regId"
    $existed = Test-Path $key
    # Note: New-Item -Force on an existing registry key would wipe it.
    if (-not $existed) { New-Item -Path $key -Force | Out-Null }
    Set-RegValue $key 'LibraryFileName' $dllName
    Set-RegValue $key 'Disabled' 0
    Set-RegValue $key 'Include' ((@($mod.include)) -join '|')
    Set-RegValue $key 'Exclude' ((@($mod.exclude)) -join '|')
    Set-RegValue $key 'Architecture' ((@($mod.architecture)) -join '|')
    Set-RegValue $key 'Version' ([string]$mod.version)
    if (-not $existed) { Set-RegValue $key 'LoggingEnabled' 0 }

    $settingsKey = "$key\Settings"
    $existing = @{}
    if (Test-Path $settingsKey) {
        $props = Get-ItemProperty $settingsKey
        foreach ($p in $props.PSObject.Properties) {
            if ($p.Name -notlike 'PS*') { $existing[$p.Name] = $p.Value }
        }
    }

    $settings = @{}
    foreach ($p in $mod.settings.PSObject.Properties) { $settings[$p.Name] = $p.Value }
    if ($regId -eq 'local@macos-dock-taskbar') { $settings['iconFolder'] = $IconsDir }

    if ($ResetSettings -or $existing.Count -eq 0) {
        if (Test-Path $settingsKey) { Remove-Item $settingsKey -Recurse -Force }
        New-Item -Path $settingsKey -Force | Out-Null
        foreach ($name in $settings.Keys) { Set-RegValue $settingsKey $name $settings[$name] }
        $result = 'recommended settings applied'
    } else {
        # Keep the user's settings, only add new ones (same rule as Windhawk).
        $prefixes = @{}
        foreach ($name in $existing.Keys) { $prefixes[(Get-ArrayPrefix $name)] = $true }
        $added = 0
        foreach ($name in $settings.Keys) {
            if (-not $prefixes.ContainsKey((Get-ArrayPrefix $name))) { Set-RegValue $settingsKey $name $settings[$name]; $added++ }
        }
        $result = 'your settings kept'
        if ($added) { $result += ", $added new" }
    }
    Set-RegValue $key 'SettingsChangeTime' ([int]([DateTimeOffset]::UtcNow.ToUnixTimeSeconds() -band 0x7fffffff))
    return $result
}

function Set-ProfileVersions($versions) {
    $path = Join-Path $AppDataPath 'userprofile.json'
    try {
        $json = if (Test-Path $path) { Get-Content $path -Raw -Encoding UTF8 | ConvertFrom-Json } else { New-Object psobject }
        if (-not $json.PSObject.Properties['mods']) { $json | Add-Member -NotePropertyName mods -NotePropertyValue (New-Object psobject) }
        foreach ($id in $versions.Keys) {
            $entry = $json.mods.PSObject.Properties[$id]
            if (-not $entry) { $json.mods | Add-Member -NotePropertyName $id -NotePropertyValue (New-Object psobject); $entry = $json.mods.PSObject.Properties[$id] }
            $entry.Value | Add-Member -NotePropertyName version -NotePropertyValue $versions[$id] -Force
        }
        [System.IO.File]::WriteAllText($path, ($json | ConvertTo-Json -Depth 10), (New-Object System.Text.UTF8Encoding($false)))
    } catch { Write-Warn "Could not update userprofile.json: $($_.Exception.Message)" }
}

# ---------------------------------------------------------------- 4. Choose optional mods
$selected = New-Object System.Collections.ArrayList
$optional = @($manifest.mods | Where-Object { $_.optional })
if ($optional.Count -gt 0) {
    Write-Step 'Optional mods'
    $chosen = @($Optional -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    foreach ($mod in $optional) {
        if ($Optional) {
            if ($chosen -contains $mod.id) { [void]$selected.Add($mod); Write-Ok "Selected: $($mod.name)" }
            continue
        }
        $default = [bool]$All -or ($mod.id -eq 'three-finger-drag')
        if ($All -or (Ask-YesNo $mod.question $default)) { [void]$selected.Add($mod) }
    }
}
foreach ($mod in $manifest.mods) { if (-not $mod.optional) { [void]$selected.Add($mod) } }
$selected = @($selected | Sort-Object { if ($_.source -eq 'local') { 0 } else { 1 } })

# ---------------------------------------------------------------- 5. Install mods
Send-Gui 'STEP|mods'
Write-Step 'Installing mods (the custom mods are compiled, this can take a few minutes)'
$profileVersions = @{}
$i = 0
foreach ($mod in $selected) {
    $i++
    $regId = if ($mod.source -eq 'local') { "local@$($mod.id)" } else { $mod.id }
    Send-Gui ("MOD|{0}|{1}|{2}" -f $i, $selected.Count, $mod.name)
    Write-Host ("    [{0,2}/{1}] {2} {3} ... " -f $i, $selected.Count, $mod.name, $mod.version) -NoNewline
    try {
        $dllName = New-DllName $regId $mod.version
        $sourceFile = Join-Path $TempRoot "$regId.wh.cpp"

        if ($mod.source -eq 'local') {
            Copy-Item (Join-Path $RepoRoot "Files\$($mod.file)") $sourceFile -Force
            Invoke-Compile $regId $mod $sourceFile $dllName
        } else {
            try {
                Download ("{0}{1}/{2}.wh.cpp" -f $ModsUrl, $mod.id, $mod.version) $sourceFile
            } catch {
                # Fallback: latest version from the windhawk-mods repository on GitHub.
                Download ("https://raw.githubusercontent.com/ramensoftware/windhawk-mods/main/mods/{0}.wh.cpp" -f $mod.id) $sourceFile
                $latest = Select-String -Path $sourceFile -Pattern '^//\s+@version\s+(\S+)' | Select-Object -First 1
                $mod = $mod | Select-Object *
                $mod.version = $latest.Matches[0].Groups[1].Value
                $dllName = New-DllName $regId $mod.version
            }
            try {
                Install-Precompiled $mod $dllName
            } catch {
                Invoke-Compile $regId $mod $sourceFile $dllName
            }
            $profileVersions[$mod.id] = [string]$mod.version
        }

        Copy-Item $sourceFile (Join-Path $ModsSrcPath "$regId.wh.cpp") -Force
        $result = Write-ModConfig $regId $mod $dllName
        Remove-OldDlls $regId $dllName
        Write-Host "OK ($result)" -ForegroundColor Green
        Send-Gui "MODOK|$($mod.name)"
    } catch {
        Write-Host 'FAILED' -ForegroundColor Red
        Write-Warn $_.Exception.Message
        [void]$script:Failed.Add($mod.name)
        Send-Gui "MODFAIL|$($mod.name)|$($_.Exception.Message -replace '[\r\n]+', ' ')"
    }
}
if ($profileVersions.Count) { Set-ProfileVersions $profileVersions }

# Disable mods that conflict with macOS Dock.
foreach ($id in $manifest.conflicting) {
    $key = "$ModsRegPath\$id"
    if ((Test-Path $key) -and (Get-ItemProperty $key -ErrorAction SilentlyContinue).Disabled -ne 1) {
        Set-RegValue $key 'Disabled' 1
        Write-Warn "Disabled '$id' because it conflicts with macOS Dock."
    }
}

# ---------------------------------------------------------------- 6. Windows taskbar settings
Send-Gui 'STEP|taskbar'
Write-Step 'Taskbar settings (centered, no search box, no task view, no widgets)'
$adv = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced'
foreach ($pair in @(@($adv, 'TaskbarAl', 1), @($adv, 'ShowTaskViewButton', 0), @($adv, 'TaskbarDa', 0),
                    @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Search', 'SearchboxTaskbarMode', 0))) {
    try {
        if (-not (Test-Path $pair[0])) { New-Item -Path $pair[0] -Force | Out-Null }
        Set-ItemProperty -Path $pair[0] -Name $pair[1] -Value $pair[2] -Type DWord
    } catch { Write-Warn "Could not set $($pair[1]) - change it in Settings > Personalization > Taskbar." }
}
Write-Ok 'Done.'

# ---------------------------------------------------------------- 7. Trash shortcut
Send-Gui 'STEP|trash'
Write-Step 'Trash shortcut'
try {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $TargetRoot 'Files\Trash.ps1') -NoPause | Out-Null
    Write-Ok 'Created Files\Trash.lnk - right-click it > Show more options > Pin to taskbar.'
} catch { Write-Warn "Could not create the Trash shortcut: $($_.Exception.Message)" }

# ---------------------------------------------------------------- 8. Restart
Send-Gui 'STEP|restart'
Write-Step 'Restarting Windhawk and Explorer'
try { Start-Process (Join-Path $WindhawkDir 'windhawk.exe') -ArgumentList '-restart -tray-only -timeout 60000' -Wait } catch { Write-Warn 'Please restart Windhawk manually.' }
Start-Sleep 2
Get-Process explorer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
if (-not (Get-Process explorer -ErrorAction SilentlyContinue)) { Start-Process explorer.exe }
Write-Ok 'Done.'

Send-Gui ("DONE|" + ($script:Failed -join ', '))
Write-Host ''
if ($script:Failed.Count) {
    Write-Host "  Setup finished with errors. Not installed: $($script:Failed -join ', ')" -ForegroundColor Yellow
    Write-Host '  Run Setup.cmd again, or install these mods by hand (see README).' -ForegroundColor Yellow
} else {
    Write-Host '  Setup finished!' -ForegroundColor Green
}
Write-Host '  Next steps:'
Write-Host '   - Pin Files\Trash.lnk to the taskbar and drag it to the far right.'
Write-Host '   - Right-click an app in the dock -> "Change icon ..." for apps that still show Windows icons.'

} catch {
    Write-Host ''
    Write-Host "  Setup failed: $($_.Exception.Message)" -ForegroundColor Red
    Send-Gui ("FAIL|" + ($_.Exception.Message -replace '[\r\n]+', ' '))
} finally {
    Remove-Item $TempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

if (-not $NoPrompt) { Write-Host ''; Read-Host '  Press Enter to close' | Out-Null }
