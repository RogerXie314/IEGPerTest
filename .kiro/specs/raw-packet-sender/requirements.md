# 需求文档：原始报文发送模块（raw-packet-sender）

## 简介

本模块为 IEGPerTest Simulator（.NET 8 WPF）新增"原始报文发送"功能，移植 xb-ether-tester（xiaobing）工具的核心发包能力。用户可在独立子窗口（RawPacketWindow）中配置源/目的 IP 范围、目的 MAC、报文类型、发包速率，并通过 Npcap/WinPcap 直接向网络发送原始以太网帧，支持同时发送多种报文类型。

模块内置三种漏洞利用攻击报文（MS08-067/MS17-010/MS20-796），用户可勾选后直接发送；同时支持从外部 `.etc`（xb-ether-tester 格式）或 `.pcap` 文件导入自定义报文，并在发送前编辑目的 IP/MAC 等关键字段。

底层发包能力由新增的 C++ DLL（RawPacketSender.dll）提供，C# 层负责配置管理、UI 交互和任务调度，与现有 NativeSender/NativeEngine 机制保持一致。主界面仅保留一个"攻击报文发送"入口按钮，点击后打开独立子窗口。

---

## 词汇表

- **RawPacketSender**：本模块整体，包含 C++ DLL 和 C# 封装层。
- **RawPacketWindow**：独立 WPF 子窗口，承载本模块全部 UI；主界面仅保留入口按钮。
- **PacketEngine**：RawPacketSender.dll 中的 C++ 核心，负责构造以太网帧并通过 Npcap 发送。
- **Stream**：一条待发送的报文流，包含报文模板字节数组、字段变化规则（t_rule）和发送统计。
- **BuiltinPacket**：内置攻击报文，程序集资源中预置的三种漏洞利用报文（MS08-067/MS17-010/MS20-796）。
- **SendTask**：一次发包任务，包含一组 Stream、速率配置和运行状态。
- **RateConfig**：发包速率配置，包含速率类型（包率/间隔/最大速率）和对应数值。
- **FieldRule**：字段自动变化规则，描述某字段的起始值、最大值和步长（对应 xb-ether-tester 的 t_rule）。
- **NicAdapter**：本机网络适配器，通过 Npcap 枚举和使用。
- **PacketType**：报文协议类型，支持内置攻击报文、ICMP、TCP SYN、UDP、ARP、ICMPv6、自定义。
- **IpRange**：IP 地址范围，由起始 IP 和结束 IP 定义（如 192.168.1.1 ~ 192.168.1.100）。
- **EtcFile**：xb-ether-tester 的二进制配置文件格式（`.etc`），包含速率配置和一组 Stream。

---

## 需求

### 需求 1：网络适配器枚举与选择

**用户故事：** 作为测试工程师，我希望能选择本机网络适配器，以便通过指定网卡发送原始报文。

#### 验收标准

1. WHEN 模块初始化时，THE RawPacketSender SHALL 通过 Npcap `pcap_findalldevs_ex` 枚举本机所有网络适配器，并在下拉列表中显示适配器名称和 IPv4 地址。
2. WHEN 用户选择一个网络适配器时，THE RawPacketSender SHALL 将该适配器设为当前发包网卡，并在后续所有发包操作中使用该网卡。
3. IF Npcap 未安装或枚举失败，THEN THE RawPacketSender SHALL 在界面显示错误信息"Npcap 未安装或初始化失败，请安装 Npcap"，并禁用发包功能。
4. THE RawPacketSender SHALL 在配置文件中持久化最后一次选择的适配器名称，下次启动时自动恢复选择。

---

### 需求 2：报文模板构造

**用户故事：** 作为测试工程师，我希望能选择报文类型并配置源/目的地址，以便构造符合测试需求的以太网帧模板。

#### 验收标准

1. THE RawPacketSender SHALL 支持以下内置报文类型：ICMP Echo Request（IPv4）、TCP SYN（IPv4）、UDP（IPv4）、ARP Request、ICMPv6 Echo Request。
2. WHEN 用户选择报文类型并填写源 IP、目的 IP、目的 MAC 后，THE PacketEngine SHALL 构造对应的完整以太网帧字节数组，包含正确的以太网头、IP 头（含校验和）和协议头（含校验和）。
3. WHEN 用户配置源 IP 范围（起始 IP 至结束 IP）时，THE RawPacketSender SHALL 为范围内每个 IP 地址生成一条独立的 Stream，每条 Stream 的源 IP 字段设为对应 IP 值。
4. IF 用户输入的 IP 地址格式不合法（不符合 IPv4 点分十进制格式），THEN THE RawPacketSender SHALL 在对应输入框下方显示"IP 地址格式无效"提示，并阻止任务启动。
5. IF 用户输入的起始 IP 大于结束 IP，THEN THE RawPacketSender SHALL 显示"起始 IP 不能大于结束 IP"提示，并阻止任务启动。
6. IF 用户输入的 MAC 地址格式不合法（不符合 XX:XX:XX:XX:XX:XX 或 XX-XX-XX-XX-XX-XX 格式），THEN THE RawPacketSender SHALL 显示"MAC 地址格式无效"提示，并阻止任务启动。
7. WHEN 用户选择"自定义"报文类型时，THE RawPacketSender SHALL 允许用户以十六进制字节串形式直接输入报文内容，并将其作为 Stream 的字节数组。

---

### 需求 2b：内置攻击报文

**用户故事：** 作为测试工程师，我希望工具内置常见漏洞利用报文，勾选后即可直接向目标发送，无需手动导入文件。

#### 验收标准

1. THE RawPacketSender SHALL 内置以下三种攻击报文，以程序集嵌入资源（Embedded Resource）形式打包，无需外部文件：
   - **MS08-067**：针对 Windows XP 的 SMB 漏洞利用报文（源文件：`ms08067-winXP.etc`）
   - **MS17-010**：针对 Windows 7 的 EternalBlue SMB 漏洞利用报文（源文件：`ms17010-win7.etc`）
   - **MS20-796**：针对 Windows 10 的 SMB 漏洞利用报文（源文件：`ms20796-win10.etc`）
2. WHEN RawPacketWindow 打开时，THE RawPacketSender SHALL 在"内置攻击报文"区域以复选框列表展示上述三种报文，每项显示：漏洞编号、目标系统、报文描述。
3. WHEN 用户勾选一种或多种内置攻击报文并填写目的 IP/MAC 后，THE RawPacketSender SHALL 将每种勾选的报文解析为一条 Stream，并将目的 IP 和目的 MAC 字段替换为用户输入值，重新计算受影响的校验和。
4. WHEN 用户同时勾选多种内置攻击报文时，THE PacketEngine SHALL 按 Round-Robin 顺序依次发送各勾选报文的 Stream，与普通 Stream 的发送逻辑保持一致。
5. THE RawPacketSender SHALL 允许内置攻击报文与用户自定义/导入 Stream 混合发送，统一纳入同一 SendTask 的 Stream 列表。
6. WHEN 用户修改目的 IP/MAC 时，THE RawPacketSender SHALL 实时更新 Stream 列表中对应内置报文条目的显示，无需重新勾选。

---

### 需求 2c：外部报文导入与编辑

**用户故事：** 作为测试工程师，我希望能导入第三方收集的报文文件并编辑关键字段，以便灵活扩展攻击报文库。

#### 验收标准

1. THE RawPacketSender SHALL 支持导入以下两种格式的外部报文文件：
   - **`.etc` 文件**：xb-ether-tester 二进制配置格式，包含速率配置和一组 Stream（含 t_rule 字段变化规则）。
   - **`.pcap` 文件**：标准 Wireshark/tcpdump 抓包格式，每个数据包解析为一条 Stream。
2. WHEN 用户点击"导入报文"按钮时，THE RawPacketSender SHALL 打开文件选择对话框，过滤显示 `.etc` 和 `.pcap` 文件，用户选择后解析并将 Stream 追加到当前列表。
3. WHEN 解析 `.etc` 文件时，THE RawPacketSender SHALL 按照 xb-ether-tester 的 `load_stream` 格式读取：版本头（4 字节）、速率配置（`t_fc_cfg`）、抓包配置（`t_pkt_cap_cfg`）、Stream 数量及各 Stream 数据（`STREAM_HDR_LEN + len` 字节）。
4. WHEN 解析 `.pcap` 文件时，THE RawPacketSender SHALL 读取全局头和各数据包头，将每个数据包的字节数组作为一条 Stream 的报文模板，Stream 名称默认为"pcap#序号"。
5. WHEN 用户在 Stream 列表中选中一条 Stream 并点击"编辑"时，THE RawPacketSender SHALL 弹出字段编辑对话框，允许修改以下字段（若报文类型支持）：
   - 目的 MAC（以太网头 dst[6]）
   - 源 MAC（以太网头 src[6]）
   - 目的 IP（IPv4 daddr 或 IPv6 daddr）
   - 源 IP（IPv4 saddr 或 IPv6 saddr）
   - 目的端口（TCP/UDP dest）
   - 源端口（TCP/UDP source）
6. WHEN 用户在编辑对话框中修改字段并确认时，THE RawPacketSender SHALL 将修改写入 Stream 字节数组，并重新计算所有受影响的校验和（IP/TCP/UDP/ICMP）。
7. IF 导入的文件格式不合法或解析失败，THEN THE RawPacketSender SHALL 显示包含文件名和错误原因的提示，不影响已有 Stream 列表。
8. THE RawPacketSender SHALL 支持将当前 Stream 列表（含导入和编辑后的报文）导出为 `.etc` 格式，供后续复用。

---

### 需求 3：字段变化规则（FieldRule）

**用户故事：** 作为测试工程师，我希望能为报文字段配置自动递增/范围变化规则，以便模拟多样化的流量。

#### 验收标准

1. THE RawPacketSender SHALL 支持对以下字段配置 FieldRule：源 IP、目的 IP、源端口（TCP/UDP）、目的端口（TCP/UDP）。
2. WHEN 用户为某字段配置 FieldRule（起始值、最大值、步长）时，THE PacketEngine SHALL 在每次发送该 Stream 后，按步长递增该字段值；WHEN 字段值超过最大值时，THE PacketEngine SHALL 将字段值重置为起始值。
3. WHEN FieldRule 修改了 IP 字段时，THE PacketEngine SHALL 重新计算并更新 IP 校验和；WHEN FieldRule 修改了 TCP/UDP/ICMP 字段时，THE PacketEngine SHALL 重新计算并更新对应协议校验和。
4. THE RawPacketSender SHALL 允许每条 Stream 最多配置 10 条 FieldRule，与 xb-ether-tester 的 MAX_FIELD_RULE_NUM 对齐。

---

### 需求 4：发包速率配置

**用户故事：** 作为测试工程师，我希望能配置发包速率，以便控制网络负载和测试强度。

#### 验收标准

1. THE RawPacketSender SHALL 支持三种速率模式：
   - **包率模式（PPS）**：用户指定每秒发包数（1 ~ 1,000,000 pps），PacketEngine 按 `间隔 = 1,000,000 / pps` 微秒控制发包间隔。
   - **间隔模式（Interval）**：用户指定发包间隔（微秒，1 ~ 1,000,000,000），PacketEngine 按指定间隔发包。
   - **最大速率模式（Fastest）**：PacketEngine 不做任何延时，以 Npcap 和系统允许的最大速率发包。
2. WHEN 用户选择包率模式并输入 pps 值时，THE RawPacketSender SHALL 将 pps 值换算为微秒间隔并存入 RateConfig，换算公式为 `interval_us = 1,000,000 / pps`（整数除法，最小值为 1）。
3. IF 用户输入的 pps 值不在 1 ~ 1,000,000 范围内，THEN THE RawPacketSender SHALL 显示"包率须在 1 ~ 1,000,000 pps 范围内"提示，并阻止任务启动。
4. THE RawPacketSender SHALL 支持两种发包模式：
   - **持续模式（Continuous）**：任务持续发包直到用户手动停止。
   - **定量模式（Burst）**：用户指定总发包次数，PacketEngine 达到该次数后自动停止任务。

---

### 需求 5：多 Stream 并发发送

**用户故事：** 作为测试工程师，我希望能同时发送多种类型的报文，以便模拟真实的混合流量场景。

#### 验收标准

1. THE RawPacketSender SHALL 支持在一个 SendTask 中配置最多 100 条 Stream（与 xb-ether-tester 的 MAX_STREAM_NUM 对齐），每条 Stream 可以是不同的报文类型。
2. WHEN SendTask 启动时，THE PacketEngine SHALL 按轮询（Round-Robin）顺序依次发送各 Stream，每轮发送一次所有已启用的 Stream。
3. WHEN 用户在 Stream 列表中勾选/取消勾选某条 Stream 时，THE RawPacketSender SHALL 在下次发包轮次起生效，已运行的任务无需重启。
4. THE RawPacketSender SHALL 在界面中以列表形式展示所有 Stream，每条 Stream 显示：序号、报文类型、源 IP（范围）、目的 IP、目的 MAC、是否启用。

---

### 需求 6：发包任务控制

**用户故事：** 作为测试工程师，我希望能启动、停止发包任务并实时查看发包统计，以便监控测试进度。

#### 验收标准

1. WHEN 用户点击"开始发送"按钮时，THE RawPacketSender SHALL 在后台线程启动 PacketEngine 发包循环，UI 线程保持响应。
2. WHEN 用户点击"停止"按钮时，THE RawPacketSender SHALL 在 500ms 内停止 PacketEngine 发包循环，并将任务状态更新为"已停止"。
3. WHILE 发包任务运行中，THE RawPacketSender SHALL 每秒刷新一次以下统计数据并显示在界面上：已发包总数、已发字节总数、发送失败数、当前实际 PPS。
4. IF PacketEngine 调用 `pcap_sendpacket` 失败，THEN THE RawPacketSender SHALL 累计失败计数，并在统计面板中显示失败数，任务继续运行不中断。
5. WHEN 发包任务因定量模式达到目标次数而自动结束时，THE RawPacketSender SHALL 将任务状态更新为"已完成"，并在界面显示最终统计数据。
6. THE RawPacketSender SHALL 将每次 SendTask 的配置（Stream 列表、速率配置、适配器名称）持久化到 `config.json` 的 `RawPacketSender` 节点，下次启动时自动恢复。

---

### 需求 7：C++ DLL 接口（PacketEngine）

**用户故事：** 作为开发者，我希望 C++ DLL 提供清晰的 C 接口，以便 C# 通过 P/Invoke 调用。

#### 验收标准

1. THE PacketEngine SHALL 导出以下 C 接口函数（`extern "C" __declspec(dllexport)`）：
   - `RPE_Init()`：初始化 Npcap，枚举适配器，返回 0 成功 / -1 失败。
   - `RPE_Cleanup()`：释放所有资源。
   - `RPE_GetAdapterCount()`：返回枚举到的适配器数量。
   - `RPE_GetAdapterInfo(int index, char* name, int nameLen, char* ipv4, int ipv4Len)`：获取指定适配器的名称和 IPv4 地址。
   - `RPE_SelectAdapter(int index)`：选择发包适配器，返回 0 成功 / -1 失败。
   - `RPE_AddStream(const uint8_t* data, int len, const char* name)`：添加一条 Stream，返回 stream_id（≥0）或 -1 失败。
   - `RPE_ClearStreams()`：清空所有 Stream。
   - `RPE_SetStreamEnabled(int streamId, int enabled)`：启用/禁用某条 Stream。
   - `RPE_SetRateConfig(int speedType, int64_t speedValue, int sndMode, int64_t sndCount)`：设置速率配置。
   - `RPE_Start()`：启动发包循环（在 DLL 内部线程运行），返回 0 成功 / -1 失败。
   - `RPE_Stop()`：停止发包循环，阻塞直到线程退出（最多等待 1000ms）。
   - `RPE_GetStats(uint64_t* sendTotal, uint64_t* sendBytes, uint64_t* sendFail)`：获取当前统计数据。
2. THE PacketEngine SHALL 保证所有导出函数线程安全，C# 端可从任意线程调用统计查询接口。
3. IF `RPE_Start` 在未选择适配器的情况下被调用，THEN THE PacketEngine SHALL 返回 -1 并设置内部错误状态，不启动发包线程。

---

### 需求 8：配置持久化与恢复

**用户故事：** 作为测试工程师，我希望发包配置能自动保存和恢复，以便重启工具后无需重新配置。

#### 验收标准

1. THE RawPacketSender SHALL 将发包配置序列化为 JSON 并存储在 `config.json` 的 `RawPacketSender` 字段中，与现有 `AppConfig` 序列化机制保持一致。
2. WHEN 应用启动时，THE RawPacketSender SHALL 从 `config.json` 反序列化发包配置，并恢复 Stream 列表、速率配置和上次选择的适配器。
3. IF `config.json` 中不存在 `RawPacketSender` 字段，THEN THE RawPacketSender SHALL 使用默认配置（空 Stream 列表、包率模式 1000 pps、持续模式）。
4. FOR ALL 合法的发包配置对象，序列化后再反序列化 SHALL 产生与原对象等价的配置对象（往返属性）。

---

### 需求 9：错误处理与用户反馈

**用户故事：** 作为测试工程师，我希望在操作出错时能看到明确的错误提示，以便快速定位和解决问题。

#### 验收标准

1. IF Npcap 适配器打开失败（`pcap_open_live` 返回 NULL），THEN THE RawPacketSender SHALL 在界面显示包含适配器名称的错误信息，并将任务状态设为"错误"。
2. IF 发包过程中连续 100 次 `pcap_sendpacket` 失败，THEN THE RawPacketSender SHALL 自动停止任务，将状态设为"错误"，并在界面显示"连续发包失败，任务已终止"。
3. WHEN 任务状态变为"错误"或"已停止"时，THE RawPacketSender SHALL 启用"开始发送"按钮，允许用户重新启动任务。
4. THE RawPacketSender SHALL 将所有错误事件（含时间戳、错误类型、详细信息）追加写入应用日志，日志格式与现有模块保持一致。

---

### 需求 10：独立子窗口 UI 与主界面入口

**用户故事：** 作为测试工程师，我希望报文发送功能在独立窗口中操作，不干扰主界面的其他功能。

#### 验收标准

1. THE RawPacketSender SHALL 以独立 WPF 子窗口（`RawPacketWindow`）承载全部 UI，包括：网卡选择、内置攻击报文勾选区、Stream 列表、字段编辑、速率配置、发包控制按钮、实时统计面板。
2. THE RawPacketSender SHALL 在主窗口（`MainWindow`）的适当位置新增一个"攻击报文发送"入口按钮，点击后打开 `RawPacketWindow`；`RawPacketWindow` 可独立最小化/最大化，不阻塞主窗口操作。
3. `RawPacketWindow` 的布局 SHALL 分为以下区域：
   - **顶部**：网卡选择下拉框 + Npcap 状态指示
   - **左侧**：内置攻击报文勾选列表（MS08-067 / MS17-010 / MS20-796）+ 目的 IP/MAC 输入框
   - **中部**：Stream 列表（DataGrid，含序号/名称/类型/源IP/目的IP/目的MAC/启用复选框）+ 导入/编辑/删除按钮
   - **右侧**：速率配置（模式选择 + 数值输入）+ 发包模式（持续/定量）+ 开始/停止按钮
   - **底部**：实时统计（已发包数 / 已发字节 / 失败数 / 当前 PPS）
4. WHEN `RawPacketWindow` 关闭时，THE RawPacketSender SHALL 自动停止正在运行的发包任务，并保存当前配置到 `config.json`。
5. THE RawPacketSender SHALL 支持 `RawPacketWindow` 在发包任务运行期间被关闭后重新打开，重新打开时恢复上次的 Stream 列表和统计数据显示。
