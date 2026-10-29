using NvAPIWrapper;
using NvAPIWrapper.Native;
using NvAPIWrapper.Native.Display.Structures;

namespace Radiance.Core;

/// <summary>
/// Digital Vibrance through the NVIDIA driver (NVAPI DVC).
/// Levels are 0-100; 50 is the driver default.
/// </summary>
public sealed class NvidiaVibrance : IDisposable
{
    private DisplayHandle[] _displays = Array.Empty<DisplayHandle>();
    private bool _ready;

    public bool IsAvailable => _ready;
    public int DisplayCount => _displays.Length;
    public string? LastError { get; private set; }

    public bool Initialize()
    {
        try
        {
            NVIDIA.Initialize();
            _displays = DisplayApi.EnumNvidiaDisplayHandle();
            _ready = _displays.Length > 0;
            if (!_ready) LastError = "No NVIDIA display was found.";
        }
        catch (Exception ex)
        {
            _ready = false;
            LastError = ex.Message;
        }
        return _ready;
    }

    public int ReadCurrent()
    {
        if (!_ready || _displays.Length == 0) return 50;
        try { return DisplayApi.GetDVCInfoEx(_displays[0]).CurrentLevel; }
        catch { return 50; }
    }

    public int ReadDefault()
    {
        if (!_ready || _displays.Length == 0) return 50;
        try { return DisplayApi.GetDVCInfoEx(_displays[0]).DefaultLevel; }
        catch { return 50; }
    }

    /// <summary>Current level of every NVIDIA display, for the status list.</summary>
    public IReadOnlyList<int> ReadAll()
    {
        var levels = new List<int>(_displays.Length);
        if (!_ready) return levels;

        foreach (var handle in _displays)
        {
            try { levels.Add(DisplayApi.GetDVCInfoEx(handle).CurrentLevel); }
            catch { levels.Add(-1); }
        }
        return levels;
    }

    /// <summary>Applies a level to every NVIDIA display. Returns how many accepted it.</summary>
    public int SetLevel(int level)
    {
        if (!_ready) return 0;
        level = Math.Clamp(level, 0, 100);

        int applied = 0;
        foreach (var handle in _displays)
        {
            try
            {
                DisplayApi.SetDVCLevelEx(handle, level);
                applied++;
            }
            catch (Exception ex)
            {
                LastError = ex.Message;
            }
        }
        return applied;
    }

    public void Dispose()
    {
        try { NVIDIA.Unload(); } catch { }
        _ready = false;
    }
}
