# Создаёт задачу планировщика «OLC Run Orbiter» (docs/STANDS.md, раздел 6).
#
# Задача запускает C:\olc\run-orbiter.ps1 в сессии вошедшего пользователя - так Orbiter
# появляется на видимом рабочем столе, даже если его запускают по SSH:
#   schtasks /run /tn "OLC Run Orbiter"
# Расписания у задачи нет: она выполняется только по такой команде.
# Нужен автовход пользователя, иначе рабочего стола нет и задача не запустится.
#
# Запуск один раз от имени того пользователя, на чьём рабочем столе будет Orbiter:
#   powershell -ExecutionPolicy Bypass -File .\register-run-task.ps1
# Удаление задачи: Unregister-ScheduledTask -TaskName 'OLC Run Orbiter'

$ErrorActionPreference = 'Stop'
$taskName = 'OLC Run Orbiter'
$script = 'C:\olc\run-orbiter.ps1'

$action = New-ScheduledTaskAction -Execute 'powershell.exe' `
    -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`""
# Interactive - только в уже открытой сессии пользователя, пароль не хранится.
# Имя берётся из токена, а не из USERDOMAIN: в сессии SSH эта переменная бывает другой.
$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Hours 2)

Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal `
    -Settings $settings -Description 'orbiter-lan-coop: runs Orbiter on the visible desktop' -Force |
    Out-Null
Write-Host "Task '$taskName' registered for $user."
