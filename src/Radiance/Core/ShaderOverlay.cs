using System.Runtime.InteropServices;
using SharpDX;
using SharpDX.D3DCompiler;
using SharpDX.Direct3D;
using SharpDX.Direct3D11;
using SharpDX.DXGI;
using SharpDX.Mathematics.Interop;
using D3D11Device = SharpDX.Direct3D11.Device;
using DxgiDevice = SharpDX.DXGI.Device;

namespace Radiance.Core;

/// <summary>
/// Full-screen D3D11 colour overlay. One overlay window, swap chain and DXGI
/// duplication per active output, so every monitor gets the same per-pixel
/// matrix. This is what makes saturation above the driver ceiling and true
/// LMS colour-vision mixing possible.
/// </summary>
public sealed class ShaderOverlay : IDisposable
{
    private const int WS_EX_TOPMOST = 0x8;
    private const int WS_EX_TRANSPARENT = 0x20;
    private const int WS_EX_TOOLWINDOW = 0x80;
    private const int WS_EX_NOACTIVATE = 0x8000000;
    private const uint WDA_EXCLUDEFROMCAPTURE = 0x11;

    [DllImport("user32.dll")] private static extern bool SetWindowDisplayAffinity(IntPtr hwnd, uint affinity);

    private const string PsSource = """
        Texture2D<float4> desktopTex : register(t0);
        SamplerState linearSampler : register(s0);
        cbuffer ColorMatrix : register(b0) { float4 row0; float4 row1; float4 row2; float4 row3; float4 row4; };
        struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
        float4 main(PSInput input) : SV_Target
        {
            float4 c = desktopTex.Sample(linearSampler, input.uv);
            float4 o;
            o.r = dot(row0, float4(c.rgb, 1.0));
            o.g = dot(row1, float4(c.rgb, 1.0));
            o.b = dot(row2, float4(c.rgb, 1.0));
            o.a = c.a;
            return saturate(o);
        }
        """;

    private const string VsSource = """
        struct VS_IN { float4 pos:POSITION; float2 uv:TEXCOORD; };
        struct VS_OUT { float4 pos:SV_POSITION; float2 uv:TEXCOORD; };
        VS_OUT main(VS_IN i) { VS_OUT o; o.pos = i.pos; o.uv = i.uv; return o; }
        """;

    /// <summary>Borderless, click-through host window for one output.</summary>
    private sealed class OverlayForm : Form
    {
        public OverlayForm(Rectangle bounds)
        {
            FormBorderStyle = FormBorderStyle.None;
            ShowInTaskbar = false;
            StartPosition = FormStartPosition.Manual;
            Bounds = bounds;
            TopMost = true;
            BackColor = Color.Black;
        }

        protected override bool ShowWithoutActivation => true;

        protected override CreateParams CreateParams
        {
            get
            {
                var cp = base.CreateParams;
                cp.ExStyle |= WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
                return cp;
            }
        }
    }

    /// <summary>Everything tied to one monitor.</summary>
    private sealed class OutputOverlay : IDisposable
    {
        public required OverlayForm Form;
        public required SwapChain1 Swap;
        public OutputDuplication? Duplication;
        public Texture2D? Frame;
        public ShaderResourceView? Srv;
        public int Width, Height;
        public string Name = "";

        public void Dispose()
        {
            try { Srv?.Dispose(); } catch { }
            try { Frame?.Dispose(); } catch { }
            try { Duplication?.Dispose(); } catch { }
            try { Swap.Dispose(); } catch { }
            try { Form.Hide(); Form.Dispose(); } catch { }
        }
    }

    private readonly List<OutputOverlay> _outputs = new();

    private D3D11Device? _device;
    private DeviceContext? _ctx;
    private Factory2? _factory;

    private VertexShader? _vs;
    private PixelShader? _ps;
    private InputLayout? _layout;
    private SamplerState? _sampler;
    private SharpDX.Direct3D11.Buffer? _constants;
    private SharpDX.Direct3D11.Buffer? _vertices;

    private float[] _matrix = Matrix5x5.Identity;
    private Thread? _thread;
    private volatile bool _running;
    private volatile bool _ready;

    private DateTime _startedUtc;

    /// <summary>A test run must never outlive the experiment.</summary>
    private static readonly TimeSpan TestLifetime = TimeSpan.FromSeconds(120);

    private bool _testMode;
    private bool _frozen;
    private int _copied;
    private int _loopErrors;

    public bool IsReady => _ready;
    public int OutputCount => _outputs.Count;
    public string? LastError { get; private set; }

    /// <summary>Test hook: freeze the captured frame so a screenshot can see the result.</summary>
    public static bool TestHook => Environment.GetEnvironmentVariable("RADIANCE_SHADER_TEST") == "1";

    /// <summary>
    /// Written while the overlay is live. If it survives to the next launch the
    /// previous run was killed mid-overlay, and the screen may have been left
    /// colour-shifted or frozen.
    /// </summary>
    public static string ActiveMarker => Path.Combine(Path.GetTempPath(), "radiance-overlay-active");

    public static bool RecoveredFromUncleanRun()
    {
        try
        {
            if (!File.Exists(ActiveMarker)) return false;
            File.Delete(ActiveMarker);
            return true;
        }
        catch { return false; }
    }

    public bool Start()
    {
        if (_running) return true;

        try
        {
            _testMode = TestHook;

            _device = new D3D11Device(DriverType.Hardware, DeviceCreationFlags.BgraSupport);
            _ctx = _device.ImmediateContext;

            using (var dxgi = _device.QueryInterface<DxgiDevice>())
            using (var adapter = dxgi.Adapter)
            using (var adapter1 = adapter.QueryInterface<Adapter1>())
            {
                using var factory1 = adapter.GetParent<Factory1>();
                _factory = factory1.QueryInterface<Factory2>();

                int outputs = adapter1.GetOutputCount();
                Log($"adapter reports {outputs} output(s)");

                for (int i = 0; i < outputs; i++)
                {
                    if (!TryCreateOutput(adapter1, i, out string reason))
                        Log($"output {i} skipped: {reason}");
                }
            }

            if (_outputs.Count == 0)
            {
                LastError = "No active display could be captured.";
                Log("FAILED: " + LastError);
                Stop();
                return false;
            }

            BuildPipeline();

            if (!_testMode)
                foreach (var o in _outputs) o.Form.Show();

            _running = true;
            _ready = true;
            _startedUtc = DateTime.UtcNow;

            try { File.WriteAllText(ActiveMarker, Environment.ProcessId.ToString()); } catch { }

            _thread = new Thread(RenderLoop) { IsBackground = true, Name = "Radiance overlay" };
            _thread.Start();

            Log($"started on {_outputs.Count} output(s)");
            return true;
        }
        catch (Exception ex)
        {
            LastError = ex.Message;
            Log("FAILED: " + ex);
            Stop();
            return false;
        }
    }

    private bool TryCreateOutput(Adapter1 adapter, int index, out string reason)
    {
        reason = "";
        Output1? output1 = null;

        try
        {
            using var output = adapter.GetOutput(index);
            output1 = output.QueryInterface<Output1>();

            var bounds = output.Description.DesktopBounds;
            int w = bounds.Right - bounds.Left;
            int h = bounds.Bottom - bounds.Top;
            if (w <= 0 || h <= 0) { reason = "empty desktop bounds"; return false; }

            var form = new OverlayForm(new Rectangle(bounds.Left, bounds.Top, w, h));
            if (!_testMode) form.Show();
            IntPtr hwnd = form.Handle;

            if (!_testMode) SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);

            var desc = new SwapChainDescription1
            {
                Width = w,
                Height = h,
                Format = Format.B8G8R8A8_UNorm,
                Stereo = false,
                SampleDescription = new SampleDescription(1, 0),
                Usage = Usage.RenderTargetOutput,
                BufferCount = 2,
                Scaling = Scaling.Stretch,
                SwapEffect = SwapEffect.FlipSequential,
                AlphaMode = AlphaMode.Ignore,
                Flags = SwapChainFlags.None
            };

            var swap = new SwapChain1(_factory, _device, hwnd, ref desc);
            var duplication = output1.DuplicateOutput(_device);

            var frame = new Texture2D(_device, new Texture2DDescription
            {
                Width = w,
                Height = h,
                MipLevels = 1,
                ArraySize = 1,
                Format = Format.B8G8R8A8_UNorm,
                SampleDescription = new SampleDescription(1, 0),
                Usage = ResourceUsage.Default,
                BindFlags = BindFlags.ShaderResource,
                CpuAccessFlags = CpuAccessFlags.None,
                OptionFlags = ResourceOptionFlags.None
            });

            _outputs.Add(new OutputOverlay
            {
                Form = form,
                Swap = swap,
                Duplication = duplication,
                Frame = frame,
                Srv = new ShaderResourceView(_device, frame),
                Width = w,
                Height = h,
                Name = output.Description.DeviceName ?? $"output {index}"
            });

            return true;
        }
        catch (Exception ex)
        {
            reason = ex.Message;
            try { output1?.Dispose(); } catch { }
            return false;
        }
    }

    private void BuildPipeline()
    {
        using var psBytecode = ShaderBytecode.Compile(PsSource, "main", "ps_5_0");
        _ps = new PixelShader(_device, psBytecode);

        using var vsBytecode = ShaderBytecode.Compile(VsSource, "main", "vs_5_0");
        _vs = new VertexShader(_device, vsBytecode);
        _layout = new InputLayout(_device, vsBytecode, new[]
        {
            new InputElement("POSITION", 0, Format.R32G32B32A32_Float, 0, 0),
            new InputElement("TEXCOORD", 0, Format.R32G32_Float, 16, 0)
        });

        _sampler = new SamplerState(_device, new SamplerStateDescription
        {
            Filter = Filter.MinMagMipLinear,
            AddressU = TextureAddressMode.Clamp,
            AddressV = TextureAddressMode.Clamp,
            AddressW = TextureAddressMode.Clamp,
            ComparisonFunction = Comparison.Never,
            MinimumLod = 0f,
            MaximumLod = float.MaxValue
        });

        _constants = new SharpDX.Direct3D11.Buffer(_device, new BufferDescription
        {
            SizeInBytes = 80,
            Usage = ResourceUsage.Default,
            BindFlags = BindFlags.ConstantBuffer
        });

        float[] quad =
        {
            -1f,  1f, 0f, 1f, 0f, 0f,
             1f,  1f, 0f, 1f, 1f, 0f,
            -1f, -1f, 0f, 1f, 0f, 1f,
             1f,  1f, 0f, 1f, 1f, 0f,
             1f, -1f, 0f, 1f, 1f, 1f,
            -1f, -1f, 0f, 1f, 0f, 1f
        };
        _vertices = SharpDX.Direct3D11.Buffer.Create(_device, BindFlags.VertexBuffer, quad);
    }

    public void SetMatrix(float[] matrix) => _matrix = matrix;

    private void RenderLoop()
    {
        while (_running)
        {
            try
            {
                bool drew = false;

                foreach (var o in _outputs)
                {
                    if (_frozen)
                    {
                        Draw(o);
                        o.Swap.Present(1, PresentFlags.None);
                        drew = true;
                        continue;
                    }

                    if (o.Duplication is null) continue;

                    if (o.Duplication.TryAcquireNextFrame(0, out _, out var desktop).Failure)
                        continue;

                    try
                    {
                        using (var texture = desktop.QueryInterface<Texture2D>())
                            _ctx!.CopyResource(texture, o.Frame);
                        _copied++;
                    }
                    finally
                    {
                        o.Duplication.ReleaseFrame();
                    }

                    Draw(o);
                    o.Swap.Present(1, PresentFlags.None);
                    drew = true;
                }

                if (_testMode && _copied >= 3 && !_frozen)
                {
                    _frozen = true;
                    foreach (var o in _outputs) { o.Form.Show(); o.Form.BringToFront(); }
                    Log("test mode: frames frozen");
                }

                if (_testMode && DateTime.UtcNow - _startedUtc > TestLifetime)
                {
                    Log("test mode: lifetime elapsed, stopping");
                    break;
                }

                if (!drew) Thread.Sleep(4);
            }
            catch (Exception ex)
            {
                if (_loopErrors++ < 3) Log("loop error: " + ex.GetType().Name + ": " + ex.Message);
                Thread.Sleep(100);
            }
        }
    }

    private void Draw(OutputOverlay o)
    {
        var ctx = _ctx!;

        using var back = o.Swap.GetBackBuffer<Texture2D>(0);
        using var rtv = new RenderTargetView(_device, back);

        ctx.OutputMerger.SetRenderTargets(rtv);
        ctx.Rasterizer.SetViewport(0, 0, o.Width, o.Height);

        ctx.UpdateSubresource(Matrix5x5.PackForShader(_matrix), _constants);

        ctx.InputAssembler.InputLayout = _layout;
        ctx.InputAssembler.PrimitiveTopology = PrimitiveTopology.TriangleList;
        ctx.InputAssembler.SetVertexBuffers(0, new VertexBufferBinding(_vertices, 24, 0));

        ctx.VertexShader.Set(_vs);
        ctx.PixelShader.Set(_ps);
        ctx.PixelShader.SetConstantBuffer(0, _constants);
        ctx.PixelShader.SetShaderResource(0, o.Srv);
        ctx.PixelShader.SetSampler(0, _sampler);

        ctx.Draw(6, 0);
    }

    public void Stop()
    {
        _running = false;
        _ready = false;

        try { File.Delete(ActiveMarker); } catch { }

        try { _thread?.Join(1000); } catch { }
        _thread = null;

        foreach (var o in _outputs) o.Dispose();
        _outputs.Clear();

        SafeDispose(ref _vertices);
        SafeDispose(ref _constants);
        SafeDispose(ref _sampler);
        SafeDispose(ref _layout);
        SafeDispose(ref _ps);
        SafeDispose(ref _vs);
        SafeDispose(ref _factory);
        SafeDispose(ref _ctx);
        SafeDispose(ref _device);
    }

    private static void Log(string message)
    {
        try
        {
            File.AppendAllText(Path.Combine(Path.GetTempPath(), "radiance-shader.log"),
                $"{DateTime.Now:HH:mm:ss} {message}{Environment.NewLine}");
        }
        catch { }
    }

    private static void SafeDispose<T>(ref T? value) where T : class, IDisposable
    {
        try { value?.Dispose(); } catch { }
        value = null;
    }

    public void Dispose() => Stop();
}

/// <summary>Builds the 5x5 colour matrix the shader consumes.</summary>
public static class Matrix5x5
{
    public static readonly float[] Identity =
    {
        1,0,0,0,0,
        0,1,0,0,0,
        0,0,1,0,0,
        0,0,0,1,0,
        0,0,0,0,1
    };

    private const float Lr = 0.2126f, Lg = 0.7152f, Lb = 0.0722f;

    /// <summary>Saturation (1 = neutral) combined with an optional 3x3 vision matrix.</summary>
    public static float[] Build(float saturation, double[,]? vision)
    {
        var s = new float[3, 3];
        float t = 1f - saturation;
        s[0, 0] = Lr * t + saturation; s[0, 1] = Lg * t; s[0, 2] = Lb * t;
        s[1, 0] = Lr * t; s[1, 1] = Lg * t + saturation; s[1, 2] = Lb * t;
        s[2, 0] = Lr * t; s[2, 1] = Lg * t; s[2, 2] = Lb * t + saturation;

        var m = vision is null ? s : Multiply(vision, s);

        return new[]
        {
            m[0,0], m[0,1], m[0,2], 0f, 0f,
            m[1,0], m[1,1], m[1,2], 0f, 0f,
            m[2,0], m[2,1], m[2,2], 0f, 0f,
            0f, 0f, 0f, 1f, 0f,
            0f, 0f, 0f, 0f, 1f
        };
    }

    private static float[,] Multiply(double[,] a, float[,] b)
    {
        var r = new float[3, 3];
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                r[i, j] = (float)(a[i, 0] * b[0, j] + a[i, 1] * b[1, j] + a[i, 2] * b[2, j]);
        return r;
    }

    /// <summary>The shader constant buffer is five float4 rows: drop the 5th column.</summary>
    public static float[] PackForShader(float[] m)
    {
        var packed = new float[20];
        for (int row = 0; row < 5; row++)
            for (int col = 0; col < 4; col++)
                packed[row * 4 + col] = m[row * 5 + col];
        return packed;
    }
}