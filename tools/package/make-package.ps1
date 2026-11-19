# Builds the test package archives from a ready Orbiter installation (docs/PACKAGE.md).
#
# Usage (Windows, from any folder):
#   powershell -File make-package.ps1 -OrbiterDir C:\olc\orbiter -PluginDll C:\olc\orbiter-lan-coop\orbiter-lan-coop.out\plugin\OrbiterLanCoop.dll -OutDir C:\olc\package
#
# Result in OutDir (the OLC repository folder is found relative to this script):
#   olc-1-orbiter.zip  - Orbiter with D3D9Client, without SPICE kernels, local settings and test leftovers
#   olc-2-kernels.zip  - SPICE kernels (the Kernels folder, the biggest part)
#   olc-3-plugin.zip   - the OLC plugin, its settings template, the test scenarios and a starting
#                        Orbiter.cfg (D3D9Client and the plugin on, vessel collisions off)
# All three are unpacked into the same folder, in this order (INSTALL text is in docs/PACKAGE.md).
param(
    [Parameter(Mandatory = $true)][string]$OrbiterDir,
    [Parameter(Mandatory = $true)][string]$PluginDll,
    [Parameter(Mandatory = $true)][string]$OutDir
)
$ErrorActionPreference = 'Stop'

$repo = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$work = Join-Path $OutDir 'staging'
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory -Force -Path $work, $OutDir | Out-Null

# Packs a folder into a zip with the system tar (it keeps empty folders, Compress-Archive does not).
function Pack($folder, $zipName) {
    $zip = Join-Path $OutDir $zipName
    if (Test-Path $zip) { Remove-Item $zip -Force }
    tar -a -c -f $zip -C $folder .
    if ($LASTEXITCODE -ne 0) { throw "tar failed for $zipName" }
}

# 1. Orbiter without kernels, SDK and everything that belongs to one particular machine.
$orb = Join-Path $work 'orbiter'
$skipDirs = @('Kernels', 'Orbitersdk')
$skipFiles = @('Orbiter.cfg', 'imgui.ini', 'olc-trace.csv', 'OlcRigTest.dll', 'OlcRigTest.cfg',
               'OlcHostOnly.cfg', 'OrbiterLanCoop.dll', 'OrbiterLanCoop.cfg', '*.log',
               'CMakeLists.txt', '*.lib')   # build leftovers of cmake --install
robocopy $OrbiterDir $orb /E /NFL /NDL /NJH /NJS /XD $skipDirs 'OLC' 'OLC-test' /XF $skipFiles | Out-Null
if ($LASTEXITCODE -ge 8) { throw 'robocopy failed for Orbiter' }
# Earth textures are not shipped (docs/TEXTURES.md): an empty folder stops the "textures missing" window.
New-Item -ItemType Directory -Force -Path (Join-Path $orb 'Textures\Earth') | Out-Null
Pack $orb 'olc-1-orbiter.zip'

# 2. Kernels.
$kz = Join-Path $OutDir 'olc-2-kernels.zip'
if (Test-Path $kz) { Remove-Item $kz -Force }
tar -a -c -f $kz -C $OrbiterDir Kernels   # the zip root holds the Kernels folder itself
if ($LASTEXITCODE -ne 0) { throw 'tar failed for the kernels' }

# 3. Plugin, settings template, scenarios.
$plug = Join-Path $work 'plugin'
New-Item -ItemType Directory -Force -Path (Join-Path $plug 'Modules\Plugin'), (Join-Path $plug 'Config'),
    (Join-Path $plug 'Scenarios\OLC-test') | Out-Null
Copy-Item $PluginDll (Join-Path $plug 'Modules\Plugin\OrbiterLanCoop.dll')
Copy-Item (Join-Path $repo 'plugin\OrbiterLanCoop.cfg') (Join-Path $plug 'Config\OrbiterLanCoop.cfg')
Copy-Item (Join-Path $repo 'tools\scenarios\*.scn') (Join-Path $plug 'Scenarios\OLC-test') -Exclude '._*'   # skip macOS leftovers
Copy-Item (Join-Path $repo 'LICENSE') (Join-Path $plug 'OLC-LICENSE.txt')
# The plugin links Orbitersdk.lib, which contains Orbiter, Dear ImGui and ImPlot code (MIT): their
# notices must go with the DLL (docs/CREDITS.md).
Copy-Item (Join-Path $PSScriptRoot 'THIRD-PARTY-NOTICES.txt') (Join-Path $plug 'OLC-THIRD-PARTY-NOTICES.txt')
# A starting Orbiter.cfg: the tester does not have to switch modules on by hand. Vessel collisions are
# off: on a client they would argue with the host's states, and near vessels the core drops time
# acceleration to x1 (docs/QUICKSTART.md, section 6; docs/ORBITER-INTERNALS.md, section 22). Orbiter adds the rest of its settings on exit.
$orbiterCfg = @(
    '; Starting settings of the OLC test package (tools/package/make-package.ps1).',
    'VesselCollision = FALSE',
    'ACTIVE_MODULES',
    '  D3D9Client',
    '  OrbiterLanCoop',
    'END_MODULES'
)
Set-Content -Path (Join-Path $plug 'Orbiter.cfg') -Value $orbiterCfg -Encoding Ascii
Pack $plug 'olc-3-plugin.zip'

Remove-Item $work -Recurse -Force
Get-ChildItem $OutDir -Filter *.zip | ForEach-Object { '{0} {1:N1} MB' -f $_.Name, ($_.Length / 1MB) }
