using System;
using System.Globalization;
using System.Text.Json.Nodes;

namespace SimulatorLib.Protocol
{
    public static class UsmRegistrationJsonBuilder
    {
        // external/IEG_Code/code/include/CmdWord/WLCmdWordDef.h
        public const int CmdClientRegistry = 1;

        // external/IEG_Code/code/WLSetUp/AccessServer.cpp uses cmdType=0x01, cmdId=CMD_CLIENT_REGISTRY
        public const int SetupCmdType = 0x01;

        public static string BuildClientLoginJson(
            string computerId,
            string username,
            string computerName,
            string computerIp,
            string computerMac,
            string windowsVersion,
            bool windowsX64,
            int proxying,
            bool licenseRecycle,
            int clientType,
            string clientVersion)
        {
            var rootArray = new JsonArray();

            var cmdContent = new JsonObject
            {
                ["ComputerName"] = computerName,
                ["ComputerIP"] = computerIp,
                ["ComputerMAC"] = string.IsNullOrWhiteSpace(computerMac) ? "" : computerMac,
                ["WindowsVersion"] = windowsVersion,
                ["WindowsX64"] = windowsX64 ? 1 : 0,
                // iIEG switch: only SRS uses srsVersion; everything else uses szCurVersion
                ["szCurVersion"] = clientVersion,
                ["srsVersion"] = "",
                ["licenseRecycle"] = licenseRecycle ? 1 : 0,
            };

            var person = new JsonObject
            {
                ["CMDContent"] = cmdContent,
                ["ComputerID"] = computerId,
                ["CMDTYPE"] = SetupCmdType,
                ["CMDID"] = CmdClientRegistry,
                ["Username"] = username,
                ["Proxying"] = proxying,
            };

            // NOTE: 原项目还传了 iIEG(EDR/IEG/SRS) 来决定 szCurVersion/srsVersion，这里默认走 IEG 分支。
            _ = clientType; // keep signature aligned for future

            rootArray.Add(person);
            return rootArray.ToJsonString() + "\n";
        }

        public static string BuildClientResultJson(string computerId, string username, int cmdType, int cmdId, int dealResult)
        {
            var rootArray = new JsonArray();

            var cmdContent = new JsonObject
            {
                ["RESULT"] = dealResult == 0 ? "SUC" : "FAIL",
            };

            var person = new JsonObject
            {
                ["CMDContent"] = cmdContent,
                ["ComputerID"] = computerId,
                ["Username"] = username,
                ["CMDTYPE"] = cmdType,
                ["CMDID"] = cmdId,
            };

            rootArray.Add(person);
            return rootArray.ToJsonString() + "\n";
        }
    }
}
