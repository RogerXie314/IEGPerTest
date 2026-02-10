using System;
using System.Text.Json;

namespace SimulatorLib.Protocol
{
    public sealed record UsmConfigInfo(uint DeviceId, int TcpPort, bool UploadEnabled, int UploadFileMax);

    public static class UsmResponseParsers
    {
        // Ported from external/IEG_Code/code/WLUtilities/WLJsonParse.cpp:
        // - Setup_CheckResultByJson
        // - GetConfigInfoByJson

        public static bool TryCheckSetupResult(string json, out int errorCode, out string? message)
        {
            errorCode = 0;
            message = null;

            if (string.IsNullOrWhiteSpace(json))
            {
                errorCode = -1;
                return false;
            }

            try
            {
                using var doc = JsonDocument.Parse(json);
                if (doc.RootElement.ValueKind != JsonValueKind.Array || doc.RootElement.GetArrayLength() < 1)
                {
                    errorCode = -3;
                    return false;
                }

                var first = doc.RootElement[0];
                if (!first.TryGetProperty("CMDContent", out var cmdContent))
                {
                    errorCode = -3;
                    return false;
                }

                if (cmdContent.TryGetProperty("MESSAGE", out var msgEl) && msgEl.ValueKind == JsonValueKind.String)
                {
                    var s = msgEl.GetString();
                    if (!string.IsNullOrEmpty(s)) message = s;
                }

                int cmdId = 0;
                if (first.TryGetProperty("CMDID", out var cmdIdEl) && cmdIdEl.ValueKind == JsonValueKind.Number)
                {
                    _ = cmdIdEl.TryGetInt32(out cmdId);
                }

                // cmdid==1 注册，若有 ERR_CODE 则返回该错误码
                if (cmdId == 1 && cmdContent.TryGetProperty("ERR_CODE", out var errEl) && errEl.ValueKind == JsonValueKind.Number)
                {
                    if (errEl.TryGetInt32(out var ec)) errorCode = ec;
                    return false;
                }

                // 原逻辑：若存在 RESULT 字段则认为失败（nError=-5）
                if (cmdContent.TryGetProperty("RESULT", out _))
                {
                    errorCode = -5;
                    return false;
                }

                return true;
            }
            catch
            {
                errorCode = -2;
                return false;
            }
        }

        public static bool TryParseConfigInfo(string json, out UsmConfigInfo info)
        {
            info = new UsmConfigInfo(DeviceId: 0, TcpPort: 0, UploadEnabled: false, UploadFileMax: 0);

            if (string.IsNullOrWhiteSpace(json)) return false;

            try
            {
                using var doc = JsonDocument.Parse(json);
                if (doc.RootElement.ValueKind != JsonValueKind.Array || doc.RootElement.GetArrayLength() < 1) return false;

                var first = doc.RootElement[0];
                if (!first.TryGetProperty("CMDContent", out var cmdContent)) return false;

                // uploadFileMax (optional)
                int uploadMax = 0;
                if (cmdContent.TryGetProperty("uploadFileMax", out var uploadMaxEl) && uploadMaxEl.ValueKind == JsonValueKind.Number)
                {
                    _ = uploadMaxEl.TryGetInt32(out uploadMax);
                }

                // uploadFile (required in C++; 9 on, 10 off)
                bool uploadEnabled = false;
                if (cmdContent.TryGetProperty("uploadFile", out var uploadEl) && uploadEl.ValueKind == JsonValueKind.Number)
                {
                    _ = uploadEl.TryGetInt32(out var uploadVal);
                    uploadEnabled = uploadVal == 9;
                }

                // tcpPort (optional)
                int tcpPort = 0;
                if (cmdContent.TryGetProperty("tcpPort", out var tcpPortEl) && tcpPortEl.ValueKind == JsonValueKind.Number)
                {
                    _ = tcpPortEl.TryGetInt32(out tcpPort);
                }

                // devid (optional)
                uint deviceId = 0;
                if (cmdContent.TryGetProperty("devid", out var devidEl) && devidEl.ValueKind == JsonValueKind.Number)
                {
                    // server may return int
                    if (devidEl.TryGetUInt32(out var u)) deviceId = u;
                    else if (devidEl.TryGetInt32(out var i) && i >= 0) deviceId = unchecked((uint)i);
                }

                info = new UsmConfigInfo(DeviceId: deviceId, TcpPort: tcpPort, UploadEnabled: uploadEnabled, UploadFileMax: uploadMax);
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}
