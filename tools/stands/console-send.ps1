# Types a command line into another process's console - "Orbiter Server Console" (Orbiter without
# a graphics client). This way a host without graphics gets console commands (pause on, pause off,
# tacc 10, etc.) from SSH: for example, to check pause and time acceleration (docs/STANDS.md, section 6).
#
#   powershell -ExecutionPolicy Bypass -File console-send.ps1 -ProcessId <pid> -Command "pause on"
#
# The script detaches from its own console, attaches to the process's console and writes
# key presses into its input. The result (sent N or an error) is appended to C:\olc\console-send.log.
param([int]$ProcessId, [string]$Command)
$code = @"
using System;
using System.Runtime.InteropServices;
public static class ConsoleSend {
  [StructLayout(LayoutKind.Explicit, CharSet=CharSet.Unicode)]
  public struct KEY_EVENT_RECORD {
    [FieldOffset(0)] public int bKeyDown; [FieldOffset(4)] public ushort wRepeatCount;
    [FieldOffset(6)] public ushort wVirtualKeyCode; [FieldOffset(8)] public ushort wVirtualScanCode;
    [FieldOffset(10)] public char UnicodeChar; [FieldOffset(12)] public uint dwControlKeyState; }
  [StructLayout(LayoutKind.Explicit)]
  public struct INPUT_RECORD { [FieldOffset(0)] public ushort EventType; [FieldOffset(4)] public KEY_EVENT_RECORD KeyEvent; }
  [DllImport("kernel32.dll")] public static extern bool FreeConsole();
  [DllImport("kernel32.dll")] public static extern bool AttachConsole(uint pid);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr CreateFileW(string n, uint a, uint s, IntPtr sa, uint c, uint f, IntPtr t);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WriteConsoleInputW(IntPtr h, INPUT_RECORD[] r, uint n, out uint w);
  public static string Send(uint pid, string text) {
    FreeConsole();
    if (!AttachConsole(pid)) return "attach failed " + Marshal.GetLastWin32Error();
    IntPtr h = CreateFileW("CONIN$", 0xC0000000, 3, IntPtr.Zero, 3, 0, IntPtr.Zero);
    text += "\r";
    var recs = new INPUT_RECORD[text.Length * 2];
    for (int i = 0; i < text.Length; i++) {
      for (int k = 0; k < 2; k++) {
        recs[i*2+k].EventType = 1;
        recs[i*2+k].KeyEvent.bKeyDown = k == 0 ? 1 : 0;
        recs[i*2+k].KeyEvent.wRepeatCount = 1;
        recs[i*2+k].KeyEvent.UnicodeChar = text[i];
        recs[i*2+k].KeyEvent.wVirtualKeyCode = (ushort)(text[i] == '\r' ? 13 : 0);
      }
    }
    uint written; bool ok = WriteConsoleInputW(h, recs, (uint)recs.Length, out written);
    FreeConsole();
    return ok ? "sent " + written : "write failed " + Marshal.GetLastWin32Error();
  }
}
"@
Add-Type -TypeDefinition $code
# The script cannot see its own output (it detached from the console), so the result goes to a file.
[ConsoleSend]::Send([uint32]$ProcessId, $Command) | Out-File C:\olc\console-send.log -Append
