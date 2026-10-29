using System.Runtime.InteropServices;

namespace Radiance.Core;

/// <summary>Registers one global hotkey and raises an event when it fires.</summary>
public sealed class HotkeyManager : IDisposable
{
    private const int WM_HOTKEY = 0x0312;
    private const int Id = 0xA11C;

    [DllImport("user32.dll")] private static extern bool RegisterHotKey(IntPtr hWnd, int id, uint mods, uint vk);
    [DllImport("user32.dll")] private static extern bool UnregisterHotKey(IntPtr hWnd, int id);

    private sealed class Window : NativeWindow
    {
        public Action? Pressed;
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == WM_HOTKEY) Pressed?.Invoke();
            base.WndProc(ref m);
        }
    }

    private readonly Window _window = new();
    private bool _registered;

    public Action? Pressed { get => _window.Pressed; set => _window.Pressed = value; }

    public HotkeyManager()
    {
        _window.CreateHandle(new CreateParams { Caption = "RadianceHotkey" });
    }

    public bool Register(HotkeyConfig cfg)
    {
        Unregister();
        if (!cfg.Enabled || cfg.Key == Keys.None) return false;

        uint mods = 0;
        if (cfg.Ctrl) mods |= 0x2;
        if (cfg.Alt) mods |= 0x1;
        if (cfg.Shift) mods |= 0x4;

        _registered = RegisterHotKey(_window.Handle, Id, mods, (uint)cfg.Key);
        return _registered;
    }

    public void Unregister()
    {
        if (!_registered) return;
        UnregisterHotKey(_window.Handle, Id);
        _registered = false;
    }

    public void Dispose()
    {
        Unregister();
        _window.DestroyHandle();
    }
}
