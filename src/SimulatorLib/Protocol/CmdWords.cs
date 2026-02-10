namespace SimulatorLib.Protocol;

public static class CmdWords
{
    // external/IEG_Code/code/WLMainDataHandle/AccessServer.cpp
    public const int CmdTypeDataToServer = 200;

    // external/IEG_Code/code/include/CmdWord/WLCmdWordDef.h
    public static class DataToServerCmdId
    {
        public const int ClientAdminLog = 202;
        public const int ProcessAlertLog = 203;
        public const int UDiskLog = 204;
        public const int HostDefenceLog = 209;
        public const int FireWallLog = 210;
        public const int IllegalLog = 214;
        public const int VulDefenseLog = 250;
        public const int VirusLog = 290;
        public const int ThreatFakeLog = 326;
        public const int PlySetOsResource = 361;
        public const int SysGuardLog = 601;
        public const int DataProtectLog = 602;
        public const int SafetyStoreLog = 18; // CMD_APPSTORE_CLIENT_UPLOAD_LOG
    }

    // external/IEG_Code/code/include/CmdWord/WLCmdWordDef_Socket.h
    public static class SocketCmd
    {
        public const uint Heartbeat = 1;

        public const uint LogProcess = 2;
        public const uint LogUsb = 3;
        public const uint LogHostDefence = 4;
        public const uint LogIllegal = 5;
        public const uint LogFireWall = 6;
        public const uint LogOs = 7;
        public const uint LogAdmin = 8;
        public const uint LogVulDefense = 13;
        public const uint LogSafetyAppStore = 14;
        public const uint LogUsbWarning = 15;
        public const uint LogUsbDiskPlug = 16;
        public const uint LogThreat = 21;
        public const uint LogVirus = 26;
        public const uint LogThreatFake = 27;
        public const uint LogOsResource = 28;
    }
}
