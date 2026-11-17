# Prepares a Windows machine for building Orbiter and OLC (docs/STANDS.md, section 8.1).
#
# Installs what is not there yet:
#   1. Git for Windows;
#   2. Visual Studio 2022 Community with C++, MFC and x64 build tools
#      (if Visual Studio is already installed - only adds the missing components);
#   3. DirectX SDK (June 2010) and .NET Framework 3.5 that its installer needs;
#   4. firewall rules for the OLC port 47800 (TCP and UDP, private network only);
#   5. automatic start of the OpenSSH Server service, if it is installed.
#
# Run: PowerShell as administrator, from the script's folder:
#   powershell -ExecutionPolicy Bypass -File .\setup-windows.ps1
#
# The script can be run again: it skips what is already done. Visual Studio and
# DirectX SDK are downloaded from the internet (several gigabytes), installation takes long.

$ErrorActionPreference = 'Stop'

# Default OLC port (core/protocol.h, kDefaultPort).
$olcPort = 47800

# Visual Studio components: the "Desktop development with C++" workload, MFC and x64
# build tools (on Windows ARM they are not installed by default).
$vsComponents = @(
    'Microsoft.VisualStudio.Workload.NativeDesktop',
    'Microsoft.VisualStudio.Component.VC.ATLMFC',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
)

# The official DirectX SDK (June 2010) installer from Microsoft's site.
$dxsdkUrl = 'https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe'
$dxsdkDir = "${env:ProgramFiles(x86)}\Microsoft DirectX SDK (June 2010)"

function Write-Step($text) {
    Write-Host ''
    Write-Host "== $text" -ForegroundColor Cyan
}

# ---------- Checks before starting ----------

$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script in PowerShell as Administrator.'
}
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw 'winget not found. Install "App Installer" from Microsoft Store and run the script again.'
}

# ---------- 1. Git ----------

Write-Step 'Git'
if (Get-Command git -ErrorAction SilentlyContinue) {
    Write-Host 'Git is already installed, skipping.'
} else {
    winget install --id Git.Git --exact --silent --accept-package-agreements --accept-source-agreements
    if ($LASTEXITCODE -ne 0) { throw "Git installation failed (exit code $LASTEXITCODE)." }
}

# ---------- 2. Visual Studio ----------

Write-Step 'Visual Studio 2022'
$addArgs = ($vsComponents | ForEach-Object { "--add $_" }) -join ' '
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -property installationPath
}

if ($vsPath) {
    # Visual Studio is already there: add the missing components through its installer.
    Write-Host "Found Visual Studio at $vsPath, adding missing components."
    $installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"
    $process = Start-Process -FilePath $installer -Wait -PassThru -ArgumentList `
        "modify --installPath `"$vsPath`" $addArgs --includeRecommended --quiet --norestart"
    # 3010 - installation succeeded, a reboot is needed.
    if ($process.ExitCode -ne 0 -and $process.ExitCode -ne 3010) {
        throw "Visual Studio modify failed (exit code $($process.ExitCode))."
    }
} else {
    winget install --id Microsoft.VisualStudio.2022.Community --exact --silent `
        --accept-package-agreements --accept-source-agreements `
        --override "--wait --quiet --norestart $addArgs --includeRecommended"
    if ($LASTEXITCODE -ne 0) { throw "Visual Studio installation failed (exit code $LASTEXITCODE)." }
}

# ---------- 3. DirectX SDK ----------

Write-Step 'DirectX SDK (June 2010)'
# Check by the header, not by the folder: a failed installation leaves an empty folder
# with a single system subfolder.
$dxsdkHeader = Join-Path $dxsdkDir 'Include\d3dx9.h'
if (Test-Path $dxsdkHeader) {
    Write-Host 'DirectX SDK is already installed, skipping.'
} else {
    # The SDK installer fails with error S1023 and does not install the SDK if the system already has
    # a Visual C++ 2010 Redistributable newer than its own. Microsoft's workaround
    # (KB2728613): remove Visual C++ 2010 Redistributable, install the SDK, put back the
    # latest Redistributable.
    $uninstallKeys = @(
        'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )
    $vc2010 = Get-ItemProperty $uninstallKeys -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -match 'Visual C\+\+ 2010 .*Redistributable' }
    foreach ($package in $vc2010) {
        Write-Host "Removing $($package.DisplayName) (restored after the SDK)."
        Start-Process msiexec.exe -ArgumentList "/x $($package.PSChildName) /quiet /norestart" -Wait
    }

    # The SDK installer needs .NET Framework 3.5. A clean Windows 10/11 does not have it, and
    # the installer then fails with "LoadLibraryShim() 0x80131700" in C:\Windows\Logs\DXSDKError.log.
    # Enabling the feature downloads it from Windows Update.
    if ((Get-WindowsOptionalFeature -Online -FeatureName NetFx3).State -ne 'Enabled') {
        Write-Host 'Enabling .NET Framework 3.5 (needed by the SDK installer)...'
        Enable-WindowsOptionalFeature -Online -FeatureName NetFx3 -All -NoRestart | Out-Null
    }

    $setup = Join-Path $env:TEMP 'DXSDK_Jun10.exe'
    Write-Host 'Downloading DirectX SDK (about 570 MB)...'
    # WebClient writes straight to the file. Invoke-WebRequest in Windows PowerShell 5.1 is
    # very slow here and may fail with "not enough system resources" on a 570 MB file.
    (New-Object System.Net.WebClient).DownloadFile($dxsdkUrl, $setup)
    # /U - silent installation.
    Start-Process -FilePath $setup -ArgumentList '/U' -Wait
    Remove-Item $setup -ErrorAction SilentlyContinue

    # Put back only the architectures that were removed.
    foreach ($package in $vc2010) {
        $id = if ($package.DisplayName -match 'x64') { 'Microsoft.VCRedist.2010.x64' } else { 'Microsoft.VCRedist.2010.x86' }
        Write-Host "Restoring $id."
        winget install --id $id --exact --silent --accept-package-agreements --accept-source-agreements
    }
    if (-not (Test-Path $dxsdkHeader)) { throw "DirectX SDK was not installed: $dxsdkHeader not found." }
}

# ---------- 4. Firewall ----------

Write-Step "Firewall rules for port $olcPort"
foreach ($protocol in 'TCP', 'UDP') {
    $name = "OLC $olcPort $protocol"
    if (Get-NetFirewallRule -DisplayName $name -ErrorAction SilentlyContinue) {
        Write-Host "Rule '$name' already exists, skipping."
    } else {
        New-NetFirewallRule -DisplayName $name -Direction Inbound -Protocol $protocol `
            -LocalPort $olcPort -Profile Private -Action Allow | Out-Null
        Write-Host "Rule '$name' created."
    }
}

# ---------- 5. OpenSSH Server ----------

Write-Step 'OpenSSH Server'
if (Get-Service sshd -ErrorAction SilentlyContinue) {
    Set-Service sshd -StartupType Automatic
    Write-Host 'sshd starts automatically.'
} else {
    Write-Host 'OpenSSH Server is not installed. Install it in Settings > System > Optional features.'
}

Write-Step 'Done'
Write-Host 'If Visual Studio was installed for the first time, reboot the machine.'
