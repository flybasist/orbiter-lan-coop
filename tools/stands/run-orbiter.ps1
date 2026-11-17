# Starts Orbiter on the visible desktop of a Windows test rig (docs/STANDS.md, section 6).
#
# A program started over SSH runs in an invisible service session. So the agent does not
# start Orbiter directly but puts a job into C:\olc\run-orbiter.json and starts the scheduled
# task "OLC Run Orbiter", which runs this script in the session of the auto-logged-on
# user. The task is created once by register-run-task.ps1.
#
# Job format (all fields except instances are optional):
#   {
#     "instances": [
#       { "dir": "C:\\olc\\host",   "args": "-s \"Checklists\\Quickstart\" -T 60 -x",
#         "window": [0, 0] },
#       { "dir": "C:\\olc\\client", "args": "-s \"Checklists\\Quickstart\" -T 60 -x",
#         "window": [642, 0] }
#     ],
#     "screenshots": [10, 30],   - how many seconds after start to take a screenshot
#     "ui": [                    - presses in Orbiter windows, by the element's name
#       { "at": 8,  "click": "Extra" },             - a button: press it
#       { "at": 9,  "select": "OLC - LAN co-op" },  - a list item: select it
#       { "at": 10, "click": "Edit" }
#     ],
#     "timeout": 120             - after how many seconds to close instances that did not exit by themselves
#   }
# window - where to move the window with the picture (x, y), so that the windows of two Orbiter
# copies do not cover each other. Coordinates and window size (WindowWidth, WindowHeight in the
# copy's Orbiter.cfg) are in logical pixels: at 200% screen scaling a 640 pixel window takes 1280 dots.
# The window is found by the title "OpenOrbiter [...]", that is, only for Orbiter with a graphics client.
#
# The result is in C:\olc\run-orbiter: run.log (what was started, exit codes) and screenshots shot-<sec>.png.
# The file done.txt appears last: by it the agent knows the run is over.
#
# "ui" lets the agent drive Launchpad and plugin dialogs without a person (for example, the
# "Connect" button of the OLC item). It uses Windows UI Automation: the element is looked up by its
# name (spaces at the ends do not count) in all windows of the started instances.

$ErrorActionPreference = 'Stop'
$jobFile = 'C:\olc\run-orbiter.json'
$outDir = 'C:\olc\run-orbiter'

New-Item -ItemType Directory -Force $outDir | Out-Null
Remove-Item "$outDir\*" -Force -ErrorAction SilentlyContinue
$log = Join-Path $outDir 'run.log'

function Write-Log($text) {
    $line = '{0:HH:mm:ss.fff} {1}' -f (Get-Date), $text
    Add-Content -Path $log -Value $line
}

# A screenshot of the whole screen as PNG.
function Save-Screenshot($path) {
    Add-Type -AssemblyName System.Windows.Forms, System.Drawing
    $bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bitmap = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $graphics.Dispose()
    $bitmap.Dispose()
}

# The Windows function for moving a window.
Add-Type -Namespace Olc -Name Win32 -MemberDefinition @'
[DllImport("user32.dll")]
public static extern bool SetWindowPos(IntPtr hWnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
[DllImport("user32.dll")]
public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
'@
$SWP_NOSIZE = 0x0001
$SWP_NOZORDER = 0x0004

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes

# Presses (click) or selects (select) the element with this name in the windows of the given processes.
function Invoke-UiStep($step, $processIds) {
    $name = if ($step.click) { $step.click } else { $step.select }
    $root = [System.Windows.Automation.AutomationElement]::RootElement
    $all = $root.FindAll([System.Windows.Automation.TreeScope]::Descendants,
        [System.Windows.Automation.Condition]::TrueCondition)
    foreach ($element in $all) {
        if ($processIds -notcontains $element.Current.ProcessId) { continue }
        if ($element.Current.Name.Trim() -ne $name.Trim()) { continue }
        $handle = [IntPtr]$element.Current.NativeWindowHandle
        if ($step.click -and $handle -ne [IntPtr]::Zero) {
            # An ordinary Windows button: BM_CLICK is posted and does not wait. Invoke would wait
            # until a modal dialog opened by the button is closed.
            [Olc.Win32]::PostMessage($handle, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
        } elseif ($step.click) {
            $pattern = $element.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern)
            $pattern.Invoke()
        } else {
            $pattern = $element.GetCurrentPattern([System.Windows.Automation.SelectionItemPattern]::Pattern)
            $pattern.Select()
        }
        Write-Log "ui: $(if ($step.click) { 'clicked' } else { 'selected' }) '$name'"
        return
    }
    Write-Log "ui: '$name' not found"
}

try {
    $job = Get-Content $jobFile -Raw | ConvertFrom-Json
    $timeout = if ($job.timeout) { [int]$job.timeout } else { 120 }
    $shots = @()
    if ($job.screenshots) { $shots = @($job.screenshots | ForEach-Object { [int]$_ } | Sort-Object) }
    $uiSteps = @()
    if ($job.ui) { $uiSteps = @($job.ui | Sort-Object { [double]$_.at }) }

    $processes = @()
    $placements = @()   # windows still to be moved: process and coordinates
    foreach ($instance in $job.instances) {
        $exe = Join-Path $instance.dir 'Orbiter.exe'
        Write-Log "start $exe $($instance.args)"
        $process = Start-Process -FilePath $exe -ArgumentList $instance.args `
            -WorkingDirectory $instance.dir -PassThru
        $processes += $process
        if ($instance.window) {
            $placements += @{ process = $process; x = [int]$instance.window[0]; y = [int]$instance.window[1] }
        }
    }

    $start = Get-Date
    while ($true) {
        $elapsed = ((Get-Date) - $start).TotalSeconds
        # A window does not appear right after start: move it as soon as it exists.
        $waiting = @()
        foreach ($placement in $placements) {
            $process = $placement.process
            $process.Refresh()
            if ($process.HasExited) { continue }
            # The main window is the launcher at first; the window to move is the one with the picture, titled
            # "OpenOrbiter [graphics client name]".
            if ($process.MainWindowTitle -notlike 'OpenOrbiter `[*') { $waiting += $placement; continue }
            [Olc.Win32]::SetWindowPos($process.MainWindowHandle, [IntPtr]::Zero, $placement.x,
                $placement.y, 0, 0, $SWP_NOSIZE -bor $SWP_NOZORDER) | Out-Null
            Write-Log "pid $($process.Id) window '$($process.MainWindowTitle)' moved to $($placement.x),$($placement.y)"
        }
        $placements = $waiting
        while ($shots.Count -gt 0 -and $elapsed -ge $shots[0]) {
            $file = Join-Path $outDir ('shot-{0}.png' -f $shots[0])
            Save-Screenshot $file
            Write-Log "screenshot $file"
            $shots = @($shots | Select-Object -Skip 1)
        }
        while ($uiSteps.Count -gt 0 -and $elapsed -ge [double]$uiSteps[0].at) {
            try {
                Invoke-UiStep $uiSteps[0] @($processes | ForEach-Object { $_.Id })
            } catch {
                Write-Log "ui: error: $_"
            }
            $uiSteps = @($uiSteps | Select-Object -Skip 1)
        }
        $running = @($processes | Where-Object { -not $_.HasExited })
        if ($running.Count -eq 0 -and $shots.Count -eq 0) { break }
        if ($elapsed -ge $timeout) {
            Write-Log "timeout ${timeout}s, stopping $($running.Count) instance(s)"
            $running | Stop-Process -Force
            break
        }
        Start-Sleep -Milliseconds 200
    }

    foreach ($process in $processes) {
        $process.WaitForExit(5000) | Out-Null
        Write-Log "instance pid $($process.Id) exit code $($process.ExitCode)"
    }
} catch {
    Write-Log "error: $_"
} finally {
    Set-Content -Path (Join-Path $outDir 'done.txt') -Value (Get-Date -Format o)
}
