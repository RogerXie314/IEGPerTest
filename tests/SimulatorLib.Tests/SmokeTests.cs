using SimulatorLib.Models;
using SimulatorLib.Protocol;
using Xunit;

namespace SimulatorLib.Tests;

public class LogCategoryHelperTests
{
    [Fact]
    public void GetDisplayName_Process_ReturnsExpected()
    {
        var result = LogCategoryHelper.GetDisplayName(LogCategory.Process);
        Assert.Equal("进程控制", result);
    }

    [Fact]
    public void GetDisplayName_AllDefinedValues_NeverReturnsUnknown()
    {
        foreach (LogCategory cat in Enum.GetValues<LogCategory>())
        {
            var name = LogCategoryHelper.GetDisplayName(cat);
            Assert.NotEqual("未知", name);
        }
    }

    [Theory]
    [InlineData("进程控制", LogCategory.Process)]
    [InlineData("客户端操作", LogCategory.Admin)]
    [InlineData("防火墙", LogCategory.FireWall)]
    [InlineData("病毒告警", LogCategory.VPScan)]
    public void ParseDisplayName_KnownNames_ReturnsCorrectEnum(string displayName, LogCategory expected)
    {
        var result = LogCategoryHelper.ParseDisplayName(displayName);
        Assert.Equal(expected, result);
    }

    [Fact]
    public void ParseDisplayName_UnknownName_ReturnsNull()
    {
        var result = LogCategoryHelper.ParseDisplayName("不存在的分类");
        Assert.Null(result);
    }

    [Fact]
    public void IsExtDevCategory_ExtDevCdrom_ReturnsTrue()
    {
        Assert.True(LogCategoryHelper.IsExtDevCategory(LogCategory.ExtDevCdrom));
    }

    [Fact]
    public void IsExtDevCategory_Process_ReturnsFalse()
    {
        Assert.False(LogCategoryHelper.IsExtDevCategory(LogCategory.Process));
    }

    [Fact]
    public void IsThreatDataCategory_TFWarning_ReturnsTrue()
    {
        Assert.True(LogCategoryHelper.IsThreatDataCategory(LogCategory.TFWarning));
    }

    [Fact]
    public void IsThreatDataCategory_Process_ReturnsFalse()
    {
        Assert.False(LogCategoryHelper.IsThreatDataCategory(LogCategory.Process));
    }
}

public class LogTypeMapperTests
{
    [Fact]
    public void GetProcessTypeForWhitelist_NotModify_Returns1And6()
    {
        var (type, subType) = LogTypeMapper.GetProcessTypeForWhitelist(false);
        Assert.Equal(1, type);
        Assert.Equal(6, subType);
    }

    [Fact]
    public void GetProcessTypeForWhitelist_IsModify_Returns3And3()
    {
        var (type, subType) = LogTypeMapper.GetProcessTypeForWhitelist(true);
        Assert.Equal(3, type);
        Assert.Equal(3, subType);
    }

    [Fact]
    public void GetProcessAuditType_Returns2And6()
    {
        var (type, subType) = LogTypeMapper.GetProcessAuditType();
        Assert.Equal(2, type);
        Assert.Equal(6, subType);
    }

    [Fact]
    public void GetDefaultProcessType_Returns0And0()
    {
        var (type, subType) = LogTypeMapper.GetDefaultProcessType();
        Assert.Equal(0, type);
        Assert.Equal(0, subType);
    }
}

public class WarningTypeMapperTests
{
    [Fact]
    public void GetTypeSubTypeForWhitelistAlert_Returns2And1()
    {
        var (type, subType) = WarningTypeMapper.GetTypeSubTypeForWhitelistAlert();
        Assert.Equal(2, type);
        Assert.Equal(1, subType);
    }

    [Fact]
    public void GetTypeSubTypeForWhitelistModify_Returns2And2()
    {
        var (type, subType) = WarningTypeMapper.GetTypeSubTypeForWhitelistModify();
        Assert.Equal(2, type);
        Assert.Equal(2, subType);
    }

    [Fact]
    public void GetTypeSubTypeForProcessDefault_Returns0And0()
    {
        var (type, subType) = WarningTypeMapper.GetTypeSubTypeForProcessDefault();
        Assert.Equal(0, type);
        Assert.Equal(0, subType);
    }

    [Theory]
    [InlineData(0)]
    [InlineData(1)]
    [InlineData(15)]
    public void GetTypeSubTypeForHostDefenceDetail_Returns3AndPassedValue(int detailLevel2)
    {
        var (type, subType) = WarningTypeMapper.GetTypeSubTypeForHostDefenceDetail(detailLevel2);
        Assert.Equal(3, type);
        Assert.Equal(detailLevel2, subType);
    }
}

public class CmdWordsTests
{
    [Fact]
    public void CmdTypeDataToServer_Equals200()
    {
        Assert.Equal(200, CmdWords.CmdTypeDataToServer);
    }

    [Fact]
    public void SocketCmd_Heartbeat_Equals1()
    {
        Assert.Equal(1u, CmdWords.SocketCmd.Heartbeat);
    }

    [Fact]
    public void SocketCmd_LogProcess_Equals2()
    {
        Assert.Equal(2u, CmdWords.SocketCmd.LogProcess);
    }
}
