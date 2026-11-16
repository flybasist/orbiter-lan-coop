# Подготовка Windows-машины к сборке Orbiter и OLC (docs/STANDS.md, раздел 8.1).
#
# Ставит то, чего ещё нет:
#   1. Git для Windows;
#   2. Visual Studio 2022 Community с C++, MFC и средствами сборки x64
#      (если Visual Studio уже стоит - только добавляет недостающие компоненты);
#   3. DirectX SDK (June 2010);
#   4. правила брандмауэра для порта OLC 47800 (TCP и UDP, только частная сеть);
#   5. автозапуск службы OpenSSH Server, если она установлена.
#
# Запуск: PowerShell от имени администратора, из каталога со скриптом:
#   powershell -ExecutionPolicy Bypass -File .\setup-windows.ps1
#
# Скрипт можно запускать повторно: уже сделанное он пропускает. Visual Studio и
# DirectX SDK скачиваются из интернета (несколько гигабайт), установка идёт долго.

$ErrorActionPreference = 'Stop'

# Порт OLC по умолчанию (core/protocol.h, kDefaultPort).
$olcPort = 47800

# Компоненты Visual Studio: набор «Desktop development with C++», MFC и средства сборки
# x64 (на Windows ARM они не ставятся по умолчанию).
$vsComponents = @(
    'Microsoft.VisualStudio.Workload.NativeDesktop',
    'Microsoft.VisualStudio.Component.VC.ATLMFC',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
)

# Официальный установщик DirectX SDK (June 2010) с сайта Microsoft.
$dxsdkUrl = 'https://download.microsoft.com/download/A/E/7/AE743F1F-632B-4809-87A9-AA1BB3458E31/DXSDK_Jun10.exe'
$dxsdkDir = "${env:ProgramFiles(x86)}\Microsoft DirectX SDK (June 2010)"

function Write-Step($text) {
    Write-Host ''
    Write-Host "== $text" -ForegroundColor Cyan
}

# ---------- Проверки перед началом ----------

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
    # Visual Studio уже есть: через её установщик добавляем недостающие компоненты.
    Write-Host "Found Visual Studio at $vsPath, adding missing components."
    $installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"
    $process = Start-Process -FilePath $installer -Wait -PassThru -ArgumentList `
        "modify --installPath `"$vsPath`" $addArgs --includeRecommended --quiet --norestart"
    # 3010 - установка прошла, нужна перезагрузка.
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
# Проверяем по заголовку, а не по каталогу: неудачная установка оставляет пустой каталог
# с одной папкой system.
$dxsdkHeader = Join-Path $dxsdkDir 'Include\d3dx9.h'
if (Test-Path $dxsdkHeader) {
    Write-Host 'DirectX SDK is already installed, skipping.'
} else {
    # Установщик SDK падает с ошибкой S1023 и не ставит SDK, если в системе уже есть
    # Visual C++ 2010 Redistributable новее, чем в нём самом. Обход от Microsoft
    # (KB2728613): удалить Visual C++ 2010 Redistributable, поставить SDK, вернуть
    # Redistributable последней версии.
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

    $setup = Join-Path $env:TEMP 'DXSDK_Jun10.exe'
    Write-Host 'Downloading DirectX SDK (about 570 MB)...'
    Invoke-WebRequest -Uri $dxsdkUrl -OutFile $setup -UseBasicParsing
    # /U - установка без вопросов.
    Start-Process -FilePath $setup -ArgumentList '/U' -Wait
    Remove-Item $setup -ErrorAction SilentlyContinue

    # Возвращаем только те разрядности, что были удалены.
    foreach ($package in $vc2010) {
        $id = if ($package.DisplayName -match 'x64') { 'Microsoft.VCRedist.2010.x64' } else { 'Microsoft.VCRedist.2010.x86' }
        Write-Host "Restoring $id."
        winget install --id $id --exact --silent --accept-package-agreements --accept-source-agreements
    }
    if (-not (Test-Path $dxsdkHeader)) { throw "DirectX SDK was not installed: $dxsdkHeader not found." }
}

# ---------- 4. Брандмауэр ----------

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
