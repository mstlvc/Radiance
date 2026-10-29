using System.IO.Pipes;
using System.Text;
using System.Text.Json;

namespace Radiance.Core;

/// <summary>
/// Discord rich presence over the local IPC socket. Nothing leaves the machine
/// except the activity payload the Discord client itself sends.
/// </summary>
public sealed class DiscordPresence : IDisposable
{
    private const int OpHandshake = 0;
    private const int OpFrame = 1;
    private const int OpClose = 2;

    private readonly object _sync = new();
    private NamedPipeClientStream? _pipe;
    private readonly string _clientId;
    private bool _ready;
    private DateTime _nextRetry = DateTime.MinValue;
    private readonly long _startedAt = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

    public DiscordPresence(string? clientId) => _clientId = (clientId ?? "").Trim();

    public bool IsConfigured => _clientId.Length > 0;
    public bool Connected
    {
        get
        {
            lock (_sync)
            {
                return _ready && _pipe is not null && _pipe.IsConnected;
            }
        }
    }

    public string? LastError { get; private set; }
    public string? UserName { get; private set; }

    /// <summary>Discord application ids are 17-20 digit snowflakes.</summary>
    public static bool LooksLikeAppId(string? id)
    {
        var text = (id ?? "").Trim();
        return text.Length is >= 17 and <= 20 && text.All(char.IsAsciiDigit);
    }

    /// <summary>Tries to connect and complete the handshake. Safe to call repeatedly.</summary>
    public bool Connect()
    {
        lock (_sync)
        {
            if (!IsConfigured) { LastError = "No application id set."; return false; }
            if (!LooksLikeAppId(_clientId)) { LastError = "That does not look like a Discord application id."; return false; }
            if (_ready && _pipe is not null && _pipe.IsConnected) return true;
            if (DateTime.UtcNow < _nextRetry) return false;

            for (int i = 0; i < 10; i++)
            {
                try
                {
                    var pipe = new NamedPipeClientStream(".", $"discord-ipc-{i}",
                        PipeDirection.InOut, PipeOptions.Asynchronous);
                    pipe.Connect(1500);

                    _pipe = pipe;
                    Write(OpHandshake, $"{{\"v\":1,\"client_id\":\"{_clientId}\"}}");

                    string? reply = ReadFrame(TimeSpan.FromSeconds(5));

                    if (reply is null)
                    {
                        LastError = "Discord did not answer the handshake.";
                        Disconnect();
                        continue;
                    }

                    if (reply.Contains("\"evt\":\"ERROR\"", StringComparison.Ordinal) ||
                        reply.Contains("\"evt\": \"ERROR\"", StringComparison.Ordinal))
                    {
                        LastError = "Discord refused the application id: " + reply;
                        Disconnect();
                        continue;
                    }

                    UserName = TryReadUser(reply);
                    _ready = true;
                    LastError = null;
                    return true;
                }
                catch
                {
                    try { _pipe?.Dispose(); } catch { }
                    _pipe = null;
                }
            }

            _nextRetry = DateTime.UtcNow.AddSeconds(10);
            LastError = "Discord is not running. Start the Discord desktop app.";
            return false;
        }
    }

    /// <summary>Publishes the current look as the Discord activity.</summary>
    public bool SetActivity(int processId, string? presetName)
    {
        lock (_sync)
        {
            if (!Connect()) return false;

            try
            {
                string details = presetName is { Length: > 0 } ? presetName : "Tuning colour";
                string state = Branding.Tagline;

                var payloadObj = new Dictionary<string, object?>
                {
                    ["cmd"] = "SET_ACTIVITY",
                    ["args"] = new Dictionary<string, object?>
                    {
                        ["pid"] = processId,
                        ["activity"] = new Dictionary<string, object?>
                        {
                            ["details"] = details,
                            ["state"] = state,
                            ["timestamps"] = new Dictionary<string, object>
                            {
                                ["start"] = _startedAt
                            },
                            ["assets"] = new Dictionary<string, string>
                            {
                                ["large_image"] = "f0d505e3780a17945d1c0d26cc328e22",
                                ["large_text"] = Branding.Name
                            },
                            ["instance"] = true
                        }
                    },
                    ["nonce"] = Guid.NewGuid().ToString("N")
                };

                string payload = JsonSerializer.Serialize(payloadObj);
                Write(OpFrame, payload);

                string? reply = ReadFrame(TimeSpan.FromSeconds(3));

                if (reply is not null &&
                    (reply.Contains("\"code\":", StringComparison.Ordinal) ||
                     reply.Contains("\"evt\":\"ERROR\"", StringComparison.Ordinal) ||
                     reply.Contains("\"evt\": \"ERROR\"", StringComparison.Ordinal)))
                {
                    LastError = "Discord rejected the activity: " + reply;
                    return false;
                }

                LastError = null;
                return true;
            }
            catch (Exception ex)
            {
                LastError = ex.Message;
                Disconnect();
                return false;
            }
        }
    }

    public void ClearActivity(int processId)
    {
        lock (_sync)
        {
            if (!Connected) return;
            try
            {
                var clearObj = new Dictionary<string, object?>
                {
                    ["cmd"] = "SET_ACTIVITY",
                    ["args"] = new Dictionary<string, object?>
                    {
                        ["pid"] = processId
                    },
                    ["nonce"] = Guid.NewGuid().ToString("N")
                };
                Write(OpFrame, JsonSerializer.Serialize(clearObj));
                ReadFrame(TimeSpan.FromSeconds(1));
            }
            catch { Disconnect(); }
        }
    }

    private static string? TryReadUser(string ready)
    {
        try
        {
            using var doc = JsonDocument.Parse(ready);
            if (doc.RootElement.TryGetProperty("data", out var data) &&
                data.TryGetProperty("user", out var user) &&
                user.TryGetProperty("username", out var name))
                return name.GetString();
        }
        catch { }
        return null;
    }

    private void Write(int opcode, string json)
    {
        if (_pipe is null) throw new InvalidOperationException("not connected");

        byte[] payload = Encoding.UTF8.GetBytes(json);
        var frame = new byte[8 + payload.Length];
        BitConverter.TryWriteBytes(frame.AsSpan(0, 4), opcode);
        BitConverter.TryWriteBytes(frame.AsSpan(4, 4), payload.Length);
        payload.CopyTo(frame, 8);

        _pipe.Write(frame, 0, frame.Length);
        _pipe.Flush();
    }

    private string? ReadFrame(TimeSpan timeout)
    {
        if (_pipe is null) return null;

        try
        {
            using var cts = new CancellationTokenSource(timeout);
            var header = new byte[8];
            if (!ReadExact(_pipe, header, 8, cts.Token)) return null;

            int op = BitConverter.ToInt32(header, 0);
            int length = BitConverter.ToInt32(header, 4);
            if (length <= 0 || length > 65536) return null;

            var body = new byte[length];
            if (!ReadExact(_pipe, body, length, cts.Token)) return null;

            if (op == OpClose)
            {
                LastError = "Discord closed the connection: " + Encoding.UTF8.GetString(body);
                Disconnect();
                return null;
            }

            return Encoding.UTF8.GetString(body);
        }
        catch { return null; }
    }

    private static bool ReadExact(Stream stream, byte[] buffer, int count, CancellationToken ct)
    {
        int total = 0;
        while (total < count)
        {
            var task = stream.ReadAsync(buffer.AsMemory(total, count - total), ct).AsTask();
            task.Wait(ct);
            int read = task.Result;
            if (read <= 0) return false;
            total += read;
        }
        return true;
    }

    private static string Escape(string s) => s.Replace("\\", "\\\\").Replace("\"", "\\\"");

    private void Disconnect()
    {
        _ready = false;
        try { _pipe?.Dispose(); } catch { }
        _pipe = null;
    }

    public void Dispose()
    {
        lock (_sync)
        {
            Disconnect();
        }
    }
}
