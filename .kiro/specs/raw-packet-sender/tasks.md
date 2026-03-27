# 实现计划：原始报文发送模块（raw-packet-sender）

## 概述

按以下顺序实现：C++ DLL 基础设施 → .etc 解析器 → 内置报文加载 → 报文模板构造 → P/Invoke 封装 → 业务逻辑层 → WPF 子窗口 → 主界面入口 → 配置持久化 → 单文件打包集成 → 属性测试。每个阶段均可独立编译验证，最终通过发布脚本集成为单文件 EXE。

## 任务

- [x] 1. C++ DLL 基础设施（RawPacketEngine）
  - [x] 1.1 创建 `src/RawPacketEngine/` 目录，编写 `CMakeLists.txt`
    - 参照 `src/NativeSender/CMakeLists.txt` 结构，项目名 `RawPacketEngine`，输出到 `build/Release/RawPacketEngine.dll`
    - 链接 Npcap SDK（`Packet.lib`、`wpcap.lib`），头文件路径指向 Npcap SDK `Include/`
    - 设置 `RUNTIME_OUTPUT_DIRECTORY_RELEASE` 为 `${CMAKE_SOURCE_DIR}/build/Release`
    - _需求: 7.1_
  - [x] 1.2 编写 `raw_packet_engine.h`，声明所有 `RPE_*` 导出接口和 `RPE_Rule` 结构体
    - 包含：`RPE_Init`、`RPE_Cleanup`、`RPE_GetAdapterCount`、`RPE_GetAdapterInfo`、`RPE_SelectAdapter`、`RPE_AddStream`（含 rules/checksumFlags 参数）、`RPE_ClearStreams`、`RPE_SetStreamEnabled`、`RPE_SetRateConfig`、`RPE_Start`、`RPE_Stop`、`RPE_GetStats`
    - `RPE_Rule` 字段：`valid`、`flags`、`offset`、`width`、`bits_from`、`bits_len`、`base_value[8]`、`max_value[8]`、`step_size`（对齐 xb-ether-tester `t_rule`）
    - _需求: 7.1_
  - [x] 1.3 实现 `raw_packet_engine.cpp`：Npcap 适配器枚举与选择
    - 实现 `RPE_Init`（`pcap_findalldevs_ex`）、`RPE_Cleanup`、`RPE_GetAdapterCount`、`RPE_GetAdapterInfo`、`RPE_SelectAdapter`
    - 全局状态：适配器列表、当前选中适配器句柄、互斥锁
    - _需求: 1.1, 1.2, 7.1, 7.2_
  - [x] 1.4 实现发包循环核心：`RPE_AddStream`、`RPE_ClearStreams`、`RPE_SetStreamEnabled`、`RPE_SetRateConfig`、`RPE_Start`、`RPE_Stop`、`RPE_GetStats`
    - `RPE_Start` 在 DLL 内部 OS 线程启动 Round-Robin 发包循环（`pcap_sendpacket`）
    - 速率控制：PPS 模式换算微秒间隔，Interval 模式直接用，Fastest 模式不延时
    - 连续失败计数器：连续 100 次失败后设置错误标志并退出循环
    - `RPE_GetStats` 使用原子变量保证线程安全
    - _需求: 5.1, 5.2, 6.1, 6.2, 7.1, 7.2, 7.3, 9.2_
  - [x] 1.5 实现 FieldRule 引擎：`rule_fields_update` 和校验和更新
    - 对齐 xb-ether-tester `rule_fields_update` 逻辑：按步长递增字段值，超过 maxValue 回绕到 baseValue
    - 校验和更新：IP 头校验和（`checksumFlags` bit 0）、TCP（bit 1）、UDP（bit 2）、ICMP（bit 3）、ICMPv6（bit 4）
    - 在每次 `pcap_sendpacket` 后调用，修改 Stream 字节数组副本
    - _需求: 3.1, 3.2, 3.3, 3.4_

- [x] 2. .etc 文件解析器（EtcFileParser）
  - [x] 2.1 在 `src/SimulatorLib/RawPacket/` 下创建 `EtcFileParser.cs`，实现 `Parse(Stream input)` 方法
    - 读取版本头（4 字节）、`t_fc_cfg`（速率配置）、`t_pkt_cap_cfg`（抓包配置，跳过）、Stream 数量
    - 逐条读取 Stream：`STREAM_HDR_LEN` 字节头（含名称、len、checksumFlags、ruleCount）+ `len` 字节报文数据 + `ruleCount * sizeof(RPE_Rule)` 字节规则数据
    - 返回 `(RateConfig rate, List<StreamConfig> streams)`
    - _需求: 2c.3_
  - [x] 2.2 实现 `EtcFileParser.Write(Stream output, RateConfig rate, IEnumerable<StreamConfig> streams)` 方法
    - 按相同格式序列化，保证与 `Parse` 往返等价
    - _需求: 2c.8_
  - [ ]* 2.3 为 `EtcFileParser` 编写属性测试（Property 4：.etc 文件格式往返）
    - **Property 4: .etc 文件格式往返**
    - **Validates: Requirements 2c.3, 2c.8**
    - 测试文件：`src/SimulatorLib.Tests/EtcFileParserTests.cs`
    - 使用 FsCheck 生成随机 `RateConfig` 和 `StreamConfig[]`（Stream 数量 0~100，FrameData 非空），验证 Write→Parse 往返等价

- [x] 3. 内置攻击报文加载（BuiltinPacketLoader）
  - [x] 3.1 修改 `src/SimulatorLib/SimulatorLib.csproj`，将三个 `.etc` 文件作为 `EmbeddedResource` 打包
    - 添加 `<EmbeddedResource Include="..\..\external\攻击报文\ms08067-winXP.etc" LogicalName="ms08067-winXP.etc" />`
    - 同样添加 `ms17010-win7.etc` 和 `ms20796-win10.etc`
    - _需求: 2b.1_
  - [x] 3.2 创建 `src/SimulatorLib/RawPacket/BuiltinPacketLoader.cs`
    - 定义 `BuiltinPacketDef` 记录（Name、TargetOs、ResourceName）
    - 静态 `Definitions` 数组：MS08-067/MS17-010/MS20-796 三条定义
    - `Load(string resourceName)` 方法：通过 `Assembly.GetManifestResourceStream` 读取嵌入资源，调用 `EtcFileParser.Parse` 返回 `List<StreamConfig>`
    - _需求: 2b.1, 2b.2_
  - [ ]* 3.3 为 `BuiltinPacketLoader` 编写单元测试
    - 验证三种内置报文资源加载成功，返回非空 Stream 列表，FrameData 非空
    - 测试文件：`src/SimulatorLib.Tests/BuiltinPacketLoaderTests.cs`

- [x] 4. 报文模板构造（PacketTemplateBuilder）
  - [x] 4.1 创建 `src/SimulatorLib/RawPacket/PacketTemplateBuilder.cs`，实现五种协议帧构造方法
    - `BuildIcmpEchoRequest(byte[] srcMac, byte[] dstMac, uint srcIp, uint dstIp)`：以太网头 + IPv4 头 + ICMP Echo Request，计算 IP 校验和和 ICMP 校验和
    - `BuildTcpSyn(byte[] srcMac, byte[] dstMac, uint srcIp, uint dstIp, ushort srcPort, ushort dstPort)`：以太网头 + IPv4 头 + TCP SYN，计算 IP 和 TCP 校验和（伪头部）
    - `BuildUdp(byte[] srcMac, byte[] dstMac, uint srcIp, uint dstIp, ushort srcPort, ushort dstPort, byte[] payload)`：以太网头 + IPv4 头 + UDP，计算 IP 和 UDP 校验和
    - `BuildArpRequest(byte[] srcMac, uint srcIp, uint targetIp)`：以太网头 + ARP Request（广播目的 MAC）
    - `BuildIcmpv6EchoRequest(byte[] srcMac, byte[] dstMac, byte[] srcIp6, byte[] dstIp6)`：以太网头 + IPv6 头 + ICMPv6 Echo Request，计算 ICMPv6 校验和（伪头部）
    - _需求: 2.1, 2.2_
  - [x] 4.2 实现字段编辑方法：`SetDestinationIp`、`SetDestinationMac`、`RecalculateChecksums`
    - `SetDestinationIp(byte[] frame, uint dstIp)`：修改 IPv4 头 daddr（偏移 30），调用 `RecalculateChecksums`
    - `SetDestinationMac(byte[] frame, byte[] dstMac)`：修改以太网头 dst[6]（偏移 0）
    - `RecalculateChecksums(byte[] frame, uint checksumFlags)`：根据 flags 重算 IP/TCP/UDP/ICMP 校验和
    - _需求: 2b.3, 2b.6, 2c.6_
  - [ ]* 4.3 为 `PacketTemplateBuilder` 编写属性测试（Property 1：构造帧校验和正确性）
    - **Property 1: 构造帧校验和正确性**
    - **Validates: Requirements 2.2, 2b.3, 2c.6, 3.3**
    - 测试文件：`src/SimulatorLib.Tests/PacketTemplateBuilderTests.cs`
    - 使用 FsCheck 生成随机 srcMac/dstMac/srcIp/dstIp，验证构造帧及字段编辑后的 IP/TCP/UDP/ICMP 校验和通过独立 `ChecksumVerifier` 验证

- [x] 5. 数据模型与输入验证
  - [x] 5.1 创建 `src/SimulatorLib/RawPacket/Models.cs`，定义所有数据模型
    - `StreamConfig`（Id、Name、PacketType、Enabled、FrameData、Rules、ChecksumFlags；`[JsonIgnore]` SrcIp/DstIp/DstMac）
    - `FieldRuleConfig`（Valid、Flags、Offset、Width、BitsFrom、BitsLen、BaseValue[8]、MaxValue[8]、StepSize）
    - `RateConfig`（SpeedType、SpeedValue、SendMode、BurstCount）
    - `RawPacketSenderConfig`（LastAdapterName、Rate、Streams）
    - `NicAdapterInfo`（Index、PcapName、FriendlyName、Ipv4；`DisplayText` 属性）
    - `SendStats`（SendTotal、SendBytes、SendFail、CurrentPps）
    - 枚举：`PacketType`、`SpeedType`、`SendMode`、`SendTaskStatus`
    - _需求: 1.4, 4.1, 4.4, 6.3, 8.1_
  - [x] 5.2 创建 `src/SimulatorLib/RawPacket/InputValidator.cs`，实现输入验证方法
    - `IsValidIpv4(string s)`：点分十进制格式验证（四段，每段 0~255）
    - `IsValidMac(string s)`：`XX:XX:XX:XX:XX:XX` 或 `XX-XX-XX-XX-XX-XX` 格式验证
    - `IsValidPps(long pps)`：范围 `[1, 1_000_000]`
    - `IsValidIpRange(string startIp, string endIp)`：两者均合法且 startIp <= endIp
    - _需求: 2.4, 2.5, 2.6, 4.3_
  - [ ]* 5.3 为 `InputValidator` 编写属性测试（Property 3：输入验证拒绝无效输入）
    - **Property 3: 输入验证拒绝无效输入**
    - **Validates: Requirements 2.4, 2.5, 2.6, 4.3**
    - 测试文件：`src/SimulatorLib.Tests/InputValidatorTests.cs`
    - 使用 FsCheck 生成随机字符串验证 `IsValidIpv4`/`IsValidMac` 拒绝无效输入；生成超出范围整数验证 `IsValidPps` 返回 false

- [x] 6. P/Invoke 封装（RawPacketEngineInterop）
  - [x] 6.1 创建 `src/SimulatorLib/RawPacket/RawPacketEngineInterop.cs`
    - 参照 `NativeEngineInterop.cs` 模式，声明所有 `RPE_*` P/Invoke（`CallingConvention.Cdecl`）
    - 定义 `RPE_Rule` 托管结构体（`[StructLayout(LayoutKind.Sequential)]`，字段与 C++ 结构体对齐）
    - 实现托管包装方法：`Init()`、`Cleanup()`、`GetAdapterCount()`、`GetAdapterInfo(int index)`、`SelectAdapter(int index)`、`AddStream(StreamConfig config)`、`ClearStreams()`、`SetStreamEnabled(int streamId, bool enabled)`、`SetRateConfig(RateConfig config)`、`Start()`、`Stop()`、`GetStats()`
    - `AddStream` 内部将 `FieldRuleConfig[]` 转换为 `RPE_Rule[]` 并 pin 传入 DLL
    - 实现 `IDisposable`，Dispose 时调用 `RPE_Cleanup`
    - _需求: 7.1, 7.2, 7.3_

- [x] 7. 业务逻辑层（RawPacketSenderService）
  - [x] 7.1 创建 `src/SimulatorLib/RawPacket/RawPacketSenderService.cs`
    - 构造函数注入 `RawPacketEngineInterop`
    - `Initialize()`：调用 `Interop.Init()`，枚举适配器填充 `Adapters` 列表，失败时设置错误状态
    - `SelectAdapter(int index)`：调用 `Interop.SelectAdapter`，更新 `LastAdapterName`
    - `AddStream(StreamConfig config)`：调用 `Interop.AddStream`，追加到 `Streams` 列表
    - `RemoveStream(int streamId)`：调用 `Interop.SetStreamEnabled(id, false)`，从列表移除
    - `SetStreamEnabled(int streamId, bool enabled)`：调用 `Interop.SetStreamEnabled`
    - `SetRateConfig(RateConfig config)`：调用 `Interop.SetRateConfig`
    - `Start()`：验证已选适配器和非空 Stream 列表，调用 `Interop.Start()`，更新 `Status`
    - `Stop()`：调用 `Interop.Stop()`，更新 `Status`
    - `RefreshStats()`：调用 `Interop.GetStats()`，计算 `CurrentPps`（与上次采样差值/间隔秒数）
    - _需求: 1.2, 5.3, 6.1, 6.2, 6.3, 9.3_
  - [x] 7.2 实现 `ExpandIpRange(uint startIp, uint endIp)` 方法
    - 为范围内每个 IP 生成一条 `StreamConfig`，克隆模板帧并调用 `PacketTemplateBuilder.SetDestinationIp`
    - _需求: 2.3_
  - [ ]* 7.3 为 `ExpandIpRange` 编写属性测试（Property 2：IP 范围展开完整性）
    - **Property 2: IP 范围展开完整性**
    - **Validates: Requirements 2.3**
    - 测试文件：`src/SimulatorLib.Tests/RawPacketSenderServiceTests.cs`
    - 使用 FsCheck 生成 `startIp <= endIp`，验证返回列表长度 = `endIp - startIp + 1`，第 i 条源 IP = `startIp + i`，所有 IP 互不相同

- [x] 8. 检查点 — 确保所有测试通过
  - 确保所有测试通过，如有问题请向用户提问。

- [x] 9. WPF 子窗口 UI（RawPacketWindow + ViewModel）
  - [x] 9.1 创建 `src/SimulatorApp/Views/RawPacketWindow.xaml` 和 `RawPacketWindow.xaml.cs`
    - 布局分五区：顶部（网卡下拉 + Npcap 状态）、左侧（内置报文复选框 + 目的IP/MAC 输入）、中部（Stream DataGrid + 导入/编辑/删除按钮）、右侧（速率配置 + 开始/停止按钮）、底部（统计面板）
    - `Window.Closing` 事件：调用 `Service.Stop()` 并保存配置
    - _需求: 10.1, 10.3, 10.4_
  - [x] 9.2 创建 `src/SimulatorApp/ViewModels/RawPacketViewModel.cs`
    - 实现 `INotifyPropertyChanged`
    - 属性：`Adapters`、`SelectedAdapter`、`Streams`（`ObservableCollection<StreamConfig>`）、`BuiltinPackets`（三条复选框绑定）、`DestIp`、`DestMac`、`Rate`、`Stats`、`Status`、`NpcapStatusText`
    - 命令：`StartCommand`、`StopCommand`、`ImportCommand`、`EditStreamCommand`、`DeleteStreamCommand`
    - `StartCommand` 执行前调用 `InputValidator` 验证，失败时设置对应字段的验证错误消息
    - 定时器（1 秒）：调用 `Service.RefreshStats()` 并更新 `Stats` 属性
    - _需求: 6.3, 9.3, 10.1_
  - [x] 9.3 实现导入报文功能（`ImportCommand`）
    - 打开文件对话框（过滤 `.etc;*.pcap`）
    - `.etc` 文件：调用 `EtcFileParser.Parse`，追加 Stream 到列表
    - `.pcap` 文件：读取全局头和数据包，每包生成一条 Stream（名称 `pcap#序号`）
    - 解析失败时显示包含文件名和错误原因的提示，不影响已有列表
    - _需求: 2c.1, 2c.2, 2c.4, 2c.7_
  - [x] 9.4 实现字段编辑对话框（`EditStreamCommand`）
    - 弹出简单对话框，允许修改目的MAC、源MAC、目的IP、源IP、目的端口、源端口
    - 确认后调用 `PacketTemplateBuilder` 对应 setter 并重算校验和
    - _需求: 2c.5, 2c.6_

- [x] 10. 主界面入口按钮（MainWindow 修改）
  - [x] 10.1 在 `src/SimulatorApp/MainWindow.xaml` 适当位置新增"攻击报文发送"按钮
    - 按钮 Click 事件：实例化 `RawPacketWindow`，调用 `Show()`（非模态，不阻塞主窗口）
    - _需求: 10.2_

- [x] 11. 配置持久化（AppConfig 扩展 + RawPacketSenderConfig）
  - [x] 11.1 修改 `src/SimulatorLib/Persistence/AppConfig.cs`，新增 `RawPacketSender` 属性
    - 添加 `public RawPacketSenderConfig RawPacketSender { get; set; } = new();`
    - 确保现有 `LoadAsync`/`SaveAsync` 序列化逻辑自动包含新字段（`System.Text.Json` 默认行为）
    - _需求: 8.1, 8.2, 8.3_
  - [ ]* 11.2 为配置往返编写属性测试（Property 5：配置 JSON 往返）
    - **Property 5: 配置 JSON 往返**
    - **Validates: Requirements 8.4, 1.4**
    - 测试文件：`src/SimulatorLib.Tests/AppConfigTests.cs`
    - 使用 FsCheck 生成随机 `RawPacketSenderConfig`，验证 JSON 序列化→反序列化后所有字段等价，`FrameData` 字节数组内容相同

- [x] 12. 单文件打包集成
  - [x] 12.1 修改 `src/SimulatorApp/SimulatorApp.csproj`，新增 `RawPacketEngine.dll` 的 `<None>` 条目
    - 参照现有 `NativeEngine.dll` / `NativeSender.dll` 条目，添加：
      ```xml
      <None Include="..\..\src\RawPacketEngine\build\Release\RawPacketEngine.dll"
            Condition="Exists('..\..\src\RawPacketEngine\build\Release\RawPacketEngine.dll')">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
        <CopyToPublishDirectory>Always</CopyToPublishDirectory>
        <ExcludeFromSingleFile>false</ExcludeFromSingleFile>
        <Link>RawPacketEngine.dll</Link>
      </None>
      ```
    - _需求: 7.1（单文件打包约束）_
  - [x] 12.2 修改 `scripts/publish_simulatorapp.ps1`，在步骤 2b（NativeSender 构建）之后新增步骤 2c（RawPacketEngine 构建）
    - 参照 NativeSender 构建块，添加：cmake configure（`-G "Visual Studio 17 2022" -A x64`）+ cmake build（`--config Release`）
    - 构建目录：`$PSScriptRoot\..\src\RawPacketEngine\build`
    - 验证 `RawPacketEngine.dll` 存在，否则 `Write-Error` 并 `exit 1`
    - 在发布后验证块中同样检查 `RawPacketEngine.dll` 不出现在输出目录（已内嵌进 EXE）
    - _需求: 7.1（单文件打包约束）_

- [x] 13. 属性测试项目搭建与剩余属性测试
  - [x] 13.1 创建 `src/SimulatorLib.Tests/SimulatorLib.Tests.csproj`
    - `TargetFramework net8.0`，引用 `SimulatorLib`，添加 `FsCheck` 和 `FsCheck.Xunit`（或 `FsCheck.NUnit`）NuGet 包，添加测试框架（xUnit 或 NUnit）
    - _需求: 测试策略_
  - [ ]* 13.2 为 FieldRule 循环递增编写属性测试（Property 6）
    - **Property 6: FieldRule 字段值循环递增**
    - **Validates: Requirements 3.2**
    - 测试文件：`src/SimulatorLib.Tests/FieldRuleTests.cs`
    - 使用 FsCheck 生成有效 `FieldRuleConfig`（baseValue <= maxValue，stepSize >= 1，width 为 1/2/4），连续应用 N 次后验证字段值 = `baseValue + (N * stepSize) mod (maxValue - baseValue + 1)`
  - [ ]* 13.3 为发送统计单调递增编写属性测试（Property 7）
    - **Property 7: 发送统计单调递增**
    - **Validates: Requirements 6.3, 6.4**
    - 测试文件：`src/SimulatorLib.Tests/SendStatsTests.cs`
    - 使用 FsCheck 生成统计快照序列，验证 `sendTotal`、`sendBytes`、`sendFail` 均单调不减
  - [ ]* 13.4 为 Round-Robin 覆盖编写属性测试（Property 8）
    - **Property 8: Round-Robin 发送覆盖所有启用 Stream**
    - **Validates: Requirements 5.2**
    - 测试文件：`src/SimulatorLib.Tests/RoundRobinTests.cs`
    - 使用 FsCheck 生成 N >= 1 条已启用 Stream，模拟 Round-Robin 调度 N 次，验证每条 Stream 至少被选中一次

- [x] 14. 最终检查点 — 确保所有测试通过
  - 确保所有测试通过，如有问题请向用户提问。

## 备注

- 标有 `*` 的子任务为可选测试任务，可跳过以加快 MVP 交付
- 每个任务均引用具体需求条款以保证可追溯性
- 属性测试标签格式：`// Feature: raw-packet-sender, Property {N}: {property_text}`
- DLL 集成测试（需要 Npcap 环境）标记 `[Trait("Category", "Integration")]` 可选跳过
- 运行测试：`dotnet test src/SimulatorLib.Tests --no-watch`（单次执行）
