# Turns on automatic sign-in of the current user on a Windows test stand (docs/STANDS.md, section 8.1).
#
# Why: Orbiter with graphics and screenshots need a desktop session. After a reboot
# nobody is signed in, so the "OLC Run Orbiter" task has no desktop to start on.
# With automatic sign-in the session comes back by itself after every reboot.
#
# The password is typed by the machine owner: the script asks for it, checks it and
# writes it to the registry (Winlogon\DefaultPassword). The password is stored there
# in plain text - use this only on test machines, never on a personal computer.
#
# Run: PowerShell as administrator, signed in as the user who must sign in automatically:
#   powershell -ExecutionPolicy Bypass -File .\setup-autologon.ps1
#
# Turn off again:
#   powershell -ExecutionPolicy Bypass -File .\setup-autologon.ps1 -Disable

param(
    [switch]$Disable
)

$ErrorActionPreference = 'Stop'

$winlogon = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon'

$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script in PowerShell as Administrator.'
}

if ($Disable) {
    Set-ItemProperty -Path $winlogon -Name AutoAdminLogon -Value '0'
    Remove-ItemProperty -Path $winlogon -Name DefaultPassword -ErrorAction SilentlyContinue
    Write-Host 'Automatic sign-in is off, the stored password is removed.'
    exit 0
}

# Only local accounts are supported: a Microsoft account signs in differently.
$user = $env:USERNAME
$domain = $env:COMPUTERNAME

$secure = Read-Host "Password of local user '$user'" -AsSecureString
$plain = [Runtime.InteropServices.Marshal]::PtrToStringBSTR(
    [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secure))

# Check the password before writing it: a wrong one would leave the machine
# stuck at the sign-in screen after the next reboot.
Add-Type -AssemblyName System.DirectoryServices.AccountManagement
$context = New-Object System.DirectoryServices.AccountManagement.PrincipalContext('Machine')
if (-not $context.ValidateCredentials($user, $plain)) {
    throw 'Wrong password, nothing changed.'
}

Set-ItemProperty -Path $winlogon -Name AutoAdminLogon -Value '1'
Set-ItemProperty -Path $winlogon -Name DefaultUserName -Value $user
Set-ItemProperty -Path $winlogon -Name DefaultDomainName -Value $domain
Set-ItemProperty -Path $winlogon -Name DefaultPassword -Value $plain
# A sign-in count left from other tools would turn automatic sign-in off after N reboots.
Remove-ItemProperty -Path $winlogon -Name AutoLogonCount -ErrorAction SilentlyContinue

Write-Host "Automatic sign-in is on for $domain\$user. It takes effect after the next reboot."
