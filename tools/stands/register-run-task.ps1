# Creates the scheduled task "OLC Run Orbiter" (docs/STANDS.md, section 6).
#
# The task runs C:\olc\run-orbiter.ps1 in the logged-on user's session - so Orbiter
# appears on the visible desktop even when it is started over SSH:
#   schtasks /run /tn "OLC Run Orbiter"
# The task has no schedule: it runs only on such a command.
# The user must be logged on automatically, otherwise there is no desktop and the task does not start.
#
# Run once as the user on whose desktop Orbiter will appear:
#   powershell -ExecutionPolicy Bypass -File .\register-run-task.ps1
# Removing the task: Unregister-ScheduledTask -TaskName 'OLC Run Orbiter'

$ErrorActionPreference = 'Stop'
$taskName = 'OLC Run Orbiter'
$script = 'C:\olc\run-orbiter.ps1'

$action = New-ScheduledTaskAction -Execute 'powershell.exe' `
    -Argument "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File `"$script`""
# Interactive - only in an already open user session, no password is stored.
# The name comes from the token, not from USERDOMAIN: in an SSH session that variable can differ.
$user = [Security.Principal.WindowsIdentity]::GetCurrent().Name
$principal = New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Hours 2)

Register-ScheduledTask -TaskName $taskName -Action $action -Principal $principal `
    -Settings $settings -Description 'orbiter-lan-coop: runs Orbiter on the visible desktop' -Force |
    Out-Null
Write-Host "Task '$taskName' registered for $user."
