using System;
using System.Collections.Generic;
using System.Text.Json;

namespace SimulatorLib.Protocol;

public static class LogJsonBuilder
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = false,
        Encoder = System.Text.Encodings.Web.JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
    };

    private static string NowLocalTimeString()
    {
        // external: CStrUtil::convertTimeTToStr -> localtime + "%Y-%m-%d %H:%M:%S"
        return DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss");
    }

    private static long NowUnixSeconds()
    {
        return DateTimeOffset.UtcNow.ToUnixTimeSeconds();
    }

    private static string Envelope(string computerId, int cmdType, int cmdId, IReadOnlyList<Dictionary<string, object?>> cmdContent)
    {
        var person = new Dictionary<string, object?>
        {
            ["ComputerID"] = computerId,
            ["CMDTYPE"] = cmdType,
            ["CMDID"] = cmdId,
            ["CMDContent"] = cmdContent,
        };

        var root = new object[] { person };
        return JsonSerializer.Serialize(root, JsonOptions);
    }

    // 带 CMDVER 字段的 Envelope（用于 USB 插拔/访问告警等需要区分协议版本的日志）
    private static string EnvelopeWithVer(string computerId, int cmdType, int cmdId, int cmdVer, IReadOnlyList<Dictionary<string, object?>> cmdContent)
    {
        var person = new Dictionary<string, object?>
        {
            ["ComputerID"] = computerId,
            ["CMDTYPE"] = cmdType,
            ["CMDID"] = cmdId,
            ["CMDVER"] = cmdVer,
            ["CMDContent"] = cmdContent,
        };

        var root = new object[] { person };
        return JsonSerializer.Serialize(root, JsonOptions);
    }

    public static string BuildClientAdminLog(string computerId, string userName, string logContent, bool success)
    {
        // external: UserActionLog_GetJsonByVector
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["UserName"] = string.IsNullOrWhiteSpace(userName) ? "-" : userName,
            ["LogContent"] = string.IsNullOrWhiteSpace(logContent) ? "-" : logContent,
            ["dwIsSuccess"] = success ? 1 : 0,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.ClientAdminLog, new[] { item });
    }

    public static string BuildProcessAlertLog(
        string computerId,
        string fullPath,
        string parentProcess,
        string userName,
        int holdBack = 0,
        int integrityCheck = 0,
        int certCheck = 0,
        int type = 0,
        int subType = 0,
        string companyName = "-",
        string productName = "-",
        string version = "-",
        string hash = "-",
        string iegHash = "-",
        string defIntegrity = "-",
        string clientIp = "-",
        string clientName = "-",
        string machineCode = "-",
        string os = "Windows 10",
        LogFieldHelper.LogCategory category = LogFieldHelper.LogCategory.NonWhitelist
    )
    {
        // external: WarningLog_GetJsonByVector
        // 像病毒告警一样，保持简单的字段结构
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["HoldBack"] = holdBack,
            ["IntegrityCheck"] = integrityCheck,
            ["CertCheck"] = certCheck,
            ["Type"] = type,
            ["SubType"] = subType,
            ["FullPath"] = string.IsNullOrWhiteSpace(fullPath) ? "-" : fullPath,
            ["ParentProcess"] = string.IsNullOrWhiteSpace(parentProcess) ? "-" : parentProcess,
            ["CompanyName"] = string.IsNullOrWhiteSpace(companyName) ? "-" : companyName,
            ["ProductName"] = string.IsNullOrWhiteSpace(productName) ? "-" : productName,
            ["Version"] = string.IsNullOrWhiteSpace(version) ? "-" : version,
            ["UserName"] = string.IsNullOrWhiteSpace(userName) ? "-" : userName,
            ["Hash"] = string.IsNullOrWhiteSpace(hash) ? "-" : hash,
            ["IEGHash"] = string.IsNullOrWhiteSpace(iegHash) ? "-" : iegHash,
            ["DefIntegrity"] = string.IsNullOrWhiteSpace(defIntegrity) ? "-" : defIntegrity,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.ProcessAlertLog, new[] { item });
    }

    public static string BuildWhitelistAlertLog(
        string computerId,
        string fullPath,
        string parentProcess,
        string userName,
        int holdBack = 0,
        string companyName = "-",
        string productName = "-",
        string version = "-",
        string hash = "-",
        string iegHash = "-",
        string defIntegrity = "-",
        string clientIp = "-",
        string clientName = "-",
        string machineCode = "-",
        string os = "Windows 10"
    )
    {
        // 白名单相关的告警（非白名单告警）映射到 ProcessAlert，Type/SubType 使用映射器决定
        var (type, subType) = LogTypeMapper.GetProcessTypeForWhitelist(isModify: false);
        return BuildProcessAlertLog(
            computerId,
            fullPath,
            parentProcess,
            userName,
            holdBack: holdBack,
            integrityCheck: 0,
            certCheck: 0,
            type: type,
            subType: subType,
            companyName: companyName,
            productName: productName,
            version: version,
            hash: hash,
            iegHash: iegHash,
            defIntegrity: defIntegrity,
            clientIp: clientIp,
            clientName: clientName,
            machineCode: machineCode,
            os: os,
            category: LogFieldHelper.LogCategory.NonWhitelist
        );
    }

    public static string BuildWhitelistModifyLog(
        string computerId,
        string fullPath,
        string parentProcess,
        string userName,
        string companyName = "-",
        string productName = "-",
        string version = "-",
        string hash = "-",
        string iegHash = "-",
        string defIntegrity = "-",
        string clientIp = "-",
        string clientName = "-",
        string machineCode = "-",
        string os = "Windows 10"
    )
    {
        // 白名单防篡改：标记 integrityCheck 并使用映射器决定 Type/SubType
        var (type, subType) = LogTypeMapper.GetProcessTypeForWhitelist(isModify: true);
        return BuildProcessAlertLog(
            computerId,
            fullPath,
            parentProcess,
            userName,
            holdBack: 0,
            integrityCheck: 1,
            certCheck: 0,
            type: type,
            subType: subType,
            companyName: companyName,
            productName: productName,
            version: version,
            hash: hash,
            iegHash: iegHash,
            defIntegrity: defIntegrity,
            clientIp: clientIp,
            clientName: clientName,
            machineCode: machineCode,
            os: os,
            category: LogFieldHelper.LogCategory.WhitelistTamper
        );
    }

    public static string BuildVulDefenseLog(string computerId, string srcIp, int srcPort, string dstIp, int dstPort)
    {
        // external: VulLog_GetJsonByVector
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["VulType"] = 1,
            ["VulLevel"] = 1,
            ["ControlMode"] = 0,
            ["SrcPort"] = srcPort,
            ["DstPort"] = dstPort,
            ["Protocol"] = "TCP",
            ["SrcIp"] = string.IsNullOrWhiteSpace(srcIp) ? "127.0.0.1" : srcIp,
            ["DstIp"] = string.IsNullOrWhiteSpace(dstIp) ? "127.0.0.1" : dstIp,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.VulDefenseLog, new[] { item });
    }

    public static string BuildHostDefenceLog(
        string computerId,
        string fullPath,
        string processName,
        string userName,
        string logContent,
        int detailLogTypeLevel2,
        string clientIp = "-",
        string clientName = "-",
        string machineCode = "-",
        string os = "Windows 10",
        bool blocked = false)
    {
        // external: hostDefence_GetJsonByVector
        // 原代码: CMDContent["LogType"] = (int)pipclogcomm->dwDetailLogTypeLevel2
        // DetailLogTypeLevel2/LogType: 文件保护=1, 注册表保护=2, 加载文件=3, 强制访问控制(MAC)=4
        // 实际日志格式（从docs/WLDefender.log）：只包含Block、FullPath、LogContent、LogType、ProcessName、Time、Username
        var item = new Dictionary<string, object?>
        {
            ["Block"] = blocked ? 1 : 0,
            ["FullPath"] = string.IsNullOrWhiteSpace(fullPath) ? "-" : fullPath,
            ["LogContent"] = string.IsNullOrWhiteSpace(logContent) ? "-" : logContent,
            ["LogType"] = detailLogTypeLevel2,
            ["ProcessName"] = string.IsNullOrWhiteSpace(processName) ? "-" : processName,
            ["Time"] = NowLocalTimeString(),
            ["Username"] = string.IsNullOrWhiteSpace(userName) ? "-" : userName,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.HostDefenceLog, new[] { item });
    }

    public static string BuildFireWallLog(string computerId, string logContent, int type = 0)
    {
        // external: baseLine_FireWallLog_GetJson
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["Type"] = type,
            ["LogContent"] = string.IsNullOrWhiteSpace(logContent) ? "-" : logContent,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.FireWallLog, new[] { item });
    }

    public static string BuildIllegalConnectLog(string computerId, string host, string ip, int state = 1)
    {
        // external: Illegal_Connect_GetJsonLog
        var item = new Dictionary<string, object?>
        {
            ["time"] = NowLocalTimeString(),
            ["host"] = string.IsNullOrWhiteSpace(host) ? "-" : host,
            ["ip"] = string.IsNullOrWhiteSpace(ip) ? "-" : ip,
            ["state"] = state,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.IllegalLog, new[] { item });
    }

    public static string BuildThreatEventProcStartLog(string computerId, int processId, string processGuid, string processPath, string commandLine)
    {
        // external: ThreatLog_ProcStart_GetJson
        // external constants: THREAT_EVENT_TYPE_PROCSTART=60, THREAT_EVENT_UPLOAD_CMDID=21
        var cmdContent = new Dictionary<string, object?>
        {
            ["EventType"] = 60,
            ["Process.TimeStamp"] = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            ["Process.ProcessId"] = processId,
            ["Process.ProcessGuid"] = string.IsNullOrWhiteSpace(processGuid) ? Guid.NewGuid().ToString("B") : processGuid,
            ["Process.ProcessFileName"] = System.IO.Path.GetFileName(string.IsNullOrWhiteSpace(processPath) ? "unknown.exe" : processPath),
            ["Process.ProcessName"] = string.IsNullOrWhiteSpace(processPath) ? "-" : processPath,
            ["Process.CommandLine"] = string.IsNullOrWhiteSpace(commandLine) ? "-" : commandLine,
            ["Process.User"] = "user",
            ["Process.UserSid"] = "S-1-5-18",
            ["Process.TerminalSessionId"] = 0,
            ["Process.FileVersion"] = "-",
            ["Process.Description"] = "-",
            ["Process.Product"] = "-",
            ["Process.Company"] = "-",
            ["Process.OriginalFileName"] = "-",
            ["Process.ParentProcessId"] = 0,
            ["Process.ParentProcessGuid"] = Guid.NewGuid().ToString("B"),
            ["Process.ParentProcessFileName"] = "explorer.exe",
            ["Process.ParentProcessName"] = "C:\\Windows\\explorer.exe",
            ["Process.ParentCommandLine"] = "explorer.exe",
            ["Process.ParentUser"] = "user",
        };

        var person = new Dictionary<string, object?>
        {
            ["ComputerID"] = computerId,
            ["CMDTYPE"] = 200,
            ["CMDID"] = 21,
            ["CMDContent"] = cmdContent,
        };

        var root = new object[] { person };
        return JsonSerializer.Serialize(root, JsonOptions);
    }

    public static string BuildThreatFakeLog(string computerId, string fakeIp, int fakePort, string protocol, int type = 1)
    {
        // external: ThreatFake_ServicesInfo_GetJson
        var item = new Dictionary<string, object?>
        {
            ["FakeIP"] = string.IsNullOrWhiteSpace(fakeIp) ? "127.0.0.1" : fakeIp,
            ["FakePort"] = fakePort,
            ["Protocol"] = string.IsNullOrWhiteSpace(protocol) ? "RDP" : protocol,
            ["StartTime"] = NowLocalTimeString(),
            ["Type"] = type,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.ThreatFakeLog, new[] { item });
    }

    public static string BuildVirusLog(string computerId, string virusPath, string virusName)
    {
        // external: VirusLog_GetJsonByVector
        var item = new Dictionary<string, object?>
        {
            ["Time"] = (ulong)NowUnixSeconds(),
            ["VirusPath"] = string.IsNullOrWhiteSpace(virusPath) ? "-" : virusPath,
            ["VirusName"] = string.IsNullOrWhiteSpace(virusName) ? "-" : virusName,
            ["VirusType"] = 1,
            ["Score"] = 10,
            ["Source"] = 0,
            ["RiskLevel"] = 1,
            ["Result"] = 0L,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.VirusLog, new[] { item });
    }

    public static string BuildOsResourceLog(string computerId, string message, int resourceType)
    {
        // external: OSResource_GetJsonByVector
        var item = new Dictionary<string, object?>
        {
            ["Message"] = string.IsNullOrWhiteSpace(message) ? "-" : message,
            ["ResourceType"] = resourceType,
            ["Time"] = (ulong)NowUnixSeconds(),
        };

        // external: st_OSResource_Info.nCMDID = PLY_SET_OSRESOURCE
        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.PlySetOsResource, new[] { item });
    }

    public static string BuildDataProtectLog(string computerId, string @operator, string operationObject, uint action, uint result)
    {
        // external: WLJsonParse.cpp::DPLog_GetJsonByVector(vector<CWLMetaData*>) — 主路径版本
        // Fields: Time(UInt64), ProcessName(string), FileName(string), Operation(uint), Result(uint)
        var item = new Dictionary<string, object?>
        {
            ["Time"] = (ulong)NowUnixSeconds(),
            ["ProcessName"] = string.IsNullOrWhiteSpace(@operator) ? "-" : @operator,
            ["FileName"] = string.IsNullOrWhiteSpace(operationObject) ? "-" : operationObject,
            ["Operation"] = action,
            ["Result"] = result,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.DataProtectLog, new[] { item });
    }

    public static string BuildSysGuardLog(string computerId, string subject, string @object, uint action, uint result)
    {
        // external: WLJsonParse.cpp::SysGuardLog_GetJsonByVector(vector<CWLMetaData*>) — 主路径版本
        // Fields: Time(UInt64), Subject(string), Object(string), Action(uint), Result(uint)
        var item = new Dictionary<string, object?>
        {
            ["Time"] = (ulong)NowUnixSeconds(),
            ["Subject"] = string.IsNullOrWhiteSpace(subject) ? "-" : subject,
            ["Object"] = string.IsNullOrWhiteSpace(@object) ? "-" : @object,
            ["Action"] = action,
            ["Result"] = result,
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.SysGuardLog, new[] { item });
    }

    public static string BuildUsbDeviceLog(string computerId, string deviceType, string deviceName, int state, int usbType = 2)
    {
        // external: ExtDevLog_GetJsonByVector → /USM/clientULog.do
        // 字段: Time, UsbType(int=dwLogType), LogContent, UserName, FullPath
        // usbType 对应 UDISK_LOG_TYPE_* 枚举值，区分具体禁用的外设类型
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["UsbType"] = usbType,
            ["LogContent"] = string.IsNullOrWhiteSpace(deviceName) ? "-" : deviceName,
            ["UserName"] = "-",
            ["FullPath"] = "-",
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.UDiskLog, new[] { item });
    }

    public static string BuildUsbWarningLog(string computerId, string filePath, string operation, string userName)
    {
        // external: UsbDiskWarningLog_GetJsonByVector → /USM/clientUSBLog.do (CMDVER=2)
        // 字段: Time, UserName, UDiskType, serialID, FullPath, ProcessName, OperationContent, Block
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["UserName"] = string.IsNullOrWhiteSpace(userName) ? "-" : userName,
            ["UDiskType"] = 1,   // 1=普通U盘
            ["serialID"] = "SIM-USB-001",
            ["FullPath"] = string.IsNullOrWhiteSpace(filePath) ? "-" : filePath,
            ["ProcessName"] = "explorer.exe",
            ["OperationContent"] = 1,  // 1=读取
            ["Block"] = 1,  // 1=拦截
        };

        // CMDVER=2 区别旧版本 USB 访问告警 JSON 规格
        return EnvelopeWithVer(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.UDiskLog, 2, new[] { item });
    }

    public static string BuildUDiskPlugLog(string computerId, string diskType, string diskName, int action)
    {
        // external: UsbDiskPlugLog_GetJsonByVector → /USM/hotplugDevLog.do (CMDVER=1, 普通U盘/移动硬盘)
        // 字段: Time, UDiskType, serialID, registerStatus, DiskDriverLetter(array), plugEvent
        var item = new Dictionary<string, object?>
        {
            ["Time"] = NowLocalTimeString(),
            ["UDiskType"] = 1,   // 1=普通U盘
            ["serialID"] = string.IsNullOrWhiteSpace(diskName) ? "SIM-001" : diskName,
            ["registerStatus"] = 0,  // 0=未注册
            ["DiskDriverLetter"] = new[] { "E:\\" },
            ["plugEvent"] = action,  // 1=插入, 0=拔出
        };

        // CMDVER=1 表示普通/安全U盘/移动硬盘的插拔格式
        return EnvelopeWithVer(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.UDiskLog, 1, new[] { item });
    }

    public static string BuildSafetyStoreLog(string computerId, string softwareName, string softwarePath, int installType)
    {
        // external: SafetyStoreLog_GetJsonByVector (TRACEINSTALL_LOG_STRUCT*)
        // 字段: userName, StartTime, EndTime, TraceType, installPackageWLCode,
        //       installPackageNameConfiged, InstallorUnisntallState, stat, Version, installDir, DisplayIcon, Publisher
        var now = NowLocalTimeString();
        var item = new Dictionary<string, object?>
        {
            ["userName"] = "admin",
            ["StartTime"] = now,
            ["EndTime"] = now,
            ["TraceType"] = 1,   // 1=安装追踪
            ["installPackageWLCode"] = "SIM00000000000000000000000000001",
            ["installPackageNameConfiged"] = string.IsNullOrWhiteSpace(softwareName) ? "SimApp" : softwareName,
            ["InstallorUnisntallState"] = installType, // 0=未知, 1=安装, 2=卸载
            ["stat"] = 1,
            ["Version"] = "1.0.0",
            ["installDir"] = string.IsNullOrWhiteSpace(softwarePath) ? "C:\\Program Files\\SimApp" : softwarePath,
            ["DisplayIcon"] = string.IsNullOrWhiteSpace(softwarePath) ? "-" : softwarePath,
            ["Publisher"] = "SimulatorCo",
        };

        return Envelope(computerId, CmdWords.CmdTypeDataToServer, CmdWords.DataToServerCmdId.SafetyStoreLog, new[] { item });
    }
}

