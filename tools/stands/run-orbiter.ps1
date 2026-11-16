# Запуск Orbiter на видимом рабочем столе Windows-стенда (docs/STANDS.md, раздел 6).
#
# Программа, запущенная по SSH, работает в невидимой служебной сессии. Поэтому агент не
# запускает Orbiter напрямую, а кладёт задание в C:\olc\run-orbiter.json и запускает задачу
# планировщика «OLC Run Orbiter», которая выполняет этот скрипт в сессии пользователя с
# автовходом. Задача создаётся один раз скриптом register-run-task.ps1.
#
# Формат задания (все поля, кроме instances, необязательны):
#   {
#     "instances": [
#       { "dir": "C:\\olc\\host",   "args": "-s \"Checklists\\Quickstart\" -T 60 -x" },
#       { "dir": "C:\\olc\\client", "args": "-s \"Checklists\\Quickstart\" -T 60 -x" }
#     ],
#     "screenshots": [10, 30],   - через сколько секунд после запуска снять экран
#     "timeout": 120             - через сколько секунд закрыть экземпляры, если не закрылись сами
#   }
#
# Результат - в C:\olc\run-orbiter: run.log (что запущено, коды выхода) и снимки shot-<сек>.png.
# Файл done.txt появляется последним: по нему агент понимает, что запуск закончен.

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

# Снимок всего экрана в PNG.
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

try {
    $job = Get-Content $jobFile -Raw | ConvertFrom-Json
    $timeout = if ($job.timeout) { [int]$job.timeout } else { 120 }
    $shots = @()
    if ($job.screenshots) { $shots = @($job.screenshots | ForEach-Object { [int]$_ } | Sort-Object) }

    $processes = @()
    foreach ($instance in $job.instances) {
        $exe = Join-Path $instance.dir 'Orbiter.exe'
        Write-Log "start $exe $($instance.args)"
        $processes += Start-Process -FilePath $exe -ArgumentList $instance.args `
            -WorkingDirectory $instance.dir -PassThru
    }

    $start = Get-Date
    while ($true) {
        $elapsed = ((Get-Date) - $start).TotalSeconds
        while ($shots.Count -gt 0 -and $elapsed -ge $shots[0]) {
            $file = Join-Path $outDir ('shot-{0}.png' -f $shots[0])
            Save-Screenshot $file
            Write-Log "screenshot $file"
            $shots = @($shots | Select-Object -Skip 1)
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
