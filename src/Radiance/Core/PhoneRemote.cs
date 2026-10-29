using System.Net;
using System.Net.Sockets;
using System.Text;

namespace Radiance.Core;

/// <summary>
/// Small local web server so a phone on the same network can change vibrance.
/// Everything stays on the machine: no cloud, no account. A random token in the
/// URL is the only access control, so it is only advertised on the LAN address.
/// </summary>
public sealed class PhoneRemote : IDisposable
{
    private TcpListener? _listener;
    private CancellationTokenSource? _cts;
    private Action<int>? _onVibrance;
    private Func<int>? _currentVibrance;
    private Action<string>? _onTheme;

    public int Port { get; private set; }
    public string Token { get; private set; } = "";
    public bool Running => _listener is not null;
    public string Url => Running ? $"http://{LanAddress()}:{Port}/?t={Token}" : "";

    public bool Start(Action<int> onVibrance, Func<int> currentVibrance, Action<string>? onTheme = null)
    {
        _onTheme = onTheme;
        if (Running) return true;

        try
        {
            _onVibrance = onVibrance;
            _currentVibrance = currentVibrance;
            Token = Convert.ToHexString(Guid.NewGuid().ToByteArray())[..8].ToLowerInvariant();

            _listener = new TcpListener(IPAddress.Any, 0);
            _listener.Start();
            Port = ((IPEndPoint)_listener.LocalEndpoint).Port;

            _cts = new CancellationTokenSource();
            _ = Task.Run(() => AcceptLoop(_listener, _cts.Token));

            try
            {
                File.WriteAllText(Path.Combine(Path.GetTempPath(), "radiance-phone.txt"), Url);
            }
            catch { }

            return true;
        }
        catch
        {
            Stop();
            return false;
        }
    }

    public void Stop()
    {
        try { _cts?.Cancel(); } catch { }
        try { _listener?.Stop(); } catch { }
        _listener = null;
        _cts?.Dispose();
        _cts = null;
        Port = 0;
        Token = "";
    }

    private async Task AcceptLoop(TcpListener listener, CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            TcpClient client;
            try { client = await listener.AcceptTcpClientAsync(token); }
            catch { return; }

            _ = Task.Run(() => Serve(client, token), token);
        }
    }

    private async Task Serve(TcpClient client, CancellationToken token)
    {
        using (client)
        {
            try
            {
                using var stream = client.GetStream();
                using var deadline = CancellationTokenSource.CreateLinkedTokenSource(token);
                deadline.CancelAfter(TimeSpan.FromSeconds(10));

                var head = await ReadHead(stream, deadline.Token);
                if (head is null) return;

                var (method, target) = head.Value;
                if (!target.Contains("t=" + Token, StringComparison.OrdinalIgnoreCase))
                {
                    await Write(stream, "403 Forbidden", "text/plain", "Bad token");
                    return;
                }

                int queryStart = target.IndexOf("v=", StringComparison.OrdinalIgnoreCase);
                if (queryStart >= 0)
                {
                    string rest = target[(queryStart + 2)..];
                    int end = rest.IndexOfAny(new[] { '&', ' ' });
                    if (end >= 0) rest = rest[..end];

                    if (int.TryParse(rest, out int v))
                        _onVibrance?.Invoke(Math.Clamp(v, 0, 100));
                }

                int themeAt = target.IndexOf("theme=", StringComparison.OrdinalIgnoreCase);
                if (themeAt >= 0)
                {
                    string rest = target[(themeAt + 6)..];
                    int end = rest.IndexOfAny(new[] { '&', ' ' });
                    if (end >= 0) rest = rest[..end];

                    if (rest.Equals("light", StringComparison.OrdinalIgnoreCase) ||
                        rest.Equals("dark", StringComparison.OrdinalIgnoreCase))
                        _onTheme?.Invoke(rest.ToLowerInvariant());
                }

                await Write(stream, "200 OK", "text/html; charset=utf-8", Page());
            }
            catch { /* one bad request must not kill the listener */ }
        }
    }

    private string Page()
    {
        int current = _currentVibrance?.Invoke() ?? 50;

        const string html = """
            <!doctype html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">
            <title>{NAME}</title>
            <style>
              body{font-family:system-ui;background:#111;color:#eee;margin:0;padding:24px}
              h1{font-size:20px;margin:0 0 4px} p{color:#8b93a1;margin:0 0 24px}
              input[type=range]{width:100%;height:44px}
              .v{font-size:56px;font-weight:700;text-align:center;margin:8px 0 24px}
              button{width:100%;padding:14px;font-size:16px;border-radius:10px;border:0;
                     background:#5b8cff;color:#fff;margin-top:12px}
            </style></head><body>
            <h1>{NAME}</h1><p>Vibrance</p>
            <div class="v" id="v">{CUR}</div>
            <input id="s" type="range" min="0" max="100" value="{CUR}">
            <button onclick="set()">Apply</button>
            <script>
              const s=document.getElementById('s'),v=document.getElementById('v');
              s.oninput=()=>{v.textContent=s.value};
              function set(){fetch('?t={TOKEN}&v='+s.value).then(()=>v.textContent=s.value)}
            </script></body></html>
            """;

        return html.Replace("{NAME}", Branding.Name)
                   .Replace("{CUR}", current.ToString())
                   .Replace("{TOKEN}", Token);
    }

    private static async Task<(string Method, string Target)?> ReadHead(NetworkStream stream, CancellationToken token)
    {
        var buffer = new byte[2048];
        int read = await stream.ReadAsync(buffer, token);
        if (read <= 0) return null;

        string text = Encoding.ASCII.GetString(buffer, 0, read);
        var line = text.Split('\n')[0].Trim();
        var parts = line.Split(' ');
        if (parts.Length < 2) return null;

        return (parts[0], parts[1]);
    }

    private static async Task Write(NetworkStream stream, string status, string contentType, string body)
    {
        byte[] payload = Encoding.UTF8.GetBytes(body);
        string header = $"HTTP/1.1 {status}\r\nContent-Type: {contentType}\r\nContent-Length: {payload.Length}\r\nConnection: close\r\n\r\n";
        byte[] head = Encoding.ASCII.GetBytes(header);
        await stream.WriteAsync(head);
        await stream.WriteAsync(payload);
    }

    public static string LanAddress()
    {
        try
        {
            foreach (var ni in System.Net.NetworkInformation.NetworkInterface.GetAllNetworkInterfaces())
            {
                if (ni.OperationalStatus != System.Net.NetworkInformation.OperationalStatus.Up) continue;
                if (ni.NetworkInterfaceType == System.Net.NetworkInformation.NetworkInterfaceType.Loopback) continue;

                foreach (var addr in ni.GetIPProperties().UnicastAddresses)
                {
                    if (addr.Address.AddressFamily != AddressFamily.InterNetwork) continue;
                    string ip = addr.Address.ToString();
                    if (ip.StartsWith("169.254.")) continue;
                    return ip;
                }
            }
        }
        catch { }
        return "127.0.0.1";
    }

    public void Dispose() => Stop();
}