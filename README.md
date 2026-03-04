# IEGPerTest Simulator

IEG/EDR 客户端模拟器：用于模拟客户端注册、心跳、日志发送与白名单上传的 WPF 桌面工具（.NET 8）。

## 📚 核心文档

- **项目看板**：[project_dashboard.html](project_dashboard.html) - 可视化进度看板（启动：`.\scripts\start_dashboard.ps1`）
- **实施文档**：[docs/项目实施文档.md](docs/项目实施文档.md)
- **经验教训**：[docs/经验教训-日志类型实现.md](docs/经验教训-日志类型实现.md)

## 🚀 快速开始

### 开发环境运行

```powershell
# 构建并运行（需要 .NET 8 SDK）
dotnet build
dotnet run --project src/SimulatorApp
```

### 发布独立可执行文件

```powershell
# 发布所有组件
.\scripts\publish_all.ps1

# 或单独发布
.\scripts\publish_simulatorapp.ps1      # 主应用
.\scripts\publish_simulatorrunner.ps1   # 运行器
.\scripts\publish_testreceiver.ps1      # 测试接收器
.\scripts\publish_devrunner.ps1         # 开发工具
```

发布产物位于 `artifacts/*Publish/` 目录。

### 部署到无 .NET 环境的机器

发布的应用是**完全自包含单文件**，只需拷贝一个文件：

```
SimulatorApp.exe    # 主程序（约 162 MB，含 .NET 运行时 + WPF 原生库）
```

在任何 Windows x64 机器上双击即可运行，无需安装 .NET 或额外 DLL。

> 注意：程序首次运行时会在同目录生成 `config.json` 和 `Clients.log`。

## ✅ 当前状态（v3.4.0）

**已完成功能：**
- ✅ 客户端注册、心跳、白名单上传（PT/HTTP/HTTPS）
- ✅ **TCP 心跳 1000 客户端全部稳定在线**（独立 Task + bool alive 重连机制）
- ✅ 心跳断线原因监控：自动写 `heartbeat_monitor_*.log`（5 种原因：服务端关闭/写失败/连接失败/超时/正常）
- ✅ **11 种日志类型**完整实现并验证（详见下方列表），JSON 字段名对齐平台 `WLJsonParse.cpp`
- ✅ UI 布局优化（左列注册+日志分类加宽，右列心跳+白名单+日志发送，2×2 统计面板）
- ✅ **外设控制子分类**：9 种（USB接口/手机平板/光驱/无线网卡/USB网卡/软盘/蓝牙/串口/并口）
- ✅ **日志发送 EPS 精确控制**（deadline 模式：HTTP 耗时从间隔中扣除，超期重置基准而非累积欠债）
- ✅ **防 TLS 握手风暴**：>100 客户端时错峰窗口扩大到 10s（≤100 TLS/s 均匀分散）
- ✅ **对齐原始客户端 HTTP 行为**（HTTP/1.1、每次请求独立连接 `PooledConnectionLifetime=Zero`、连接超时 6s/请求超时 30s）
- ✅ **日志发送每客户端独立并发**，失败单独写分类 debug 日志
- ✅ HTTPS 失败响应写独立 log（记录状态码和响应体，便于诊断平台下发策略）
- ✅ 启动时默认全选 IEG 分类、默认不勾选日志服务器
- ✅ **压测模式**：连接复用+信号量门控，单机支持 3000 客户端 / 6000+ EPS（并发连接数可配置，默认 50）
- ✅ 单文件发布（154.6 MB，含 WPF 原生依赖，无需目标机安装 .NET）
- ✅ 持久化存储（`Clients.log`、`config.json`）
- ✅ **多IP模式运行时连接计数**：每IP实际建连次数原子统计，任务结束输出分布（如"多IP连接分布 → 192.168.1.10: 1234次  192.168.1.11: 1198次"）
- ✅ **连接参数诊断-参数优化区**：一键推荐设置（端口1025–65535，TIME_WAIT=30s）、单独应用端口范围/超时；端口范围通过 `netsh show` 正则解析（中英文系统兼容），应用后自动延迟刷新同步显示
- ✅ UI 细节：日志发送区添加"default短连接"/"连接池复用"说明，多IP输入框自适应宽度
- ✅ **威胁检测拆分为 5 种事件子类**（TCP 长连接 + PtProtocol，CMDID=21）：进程启动/注册表访问/文件访问/系统日志/DLL加载，支持 6000 EPS
- ✅ **日志发送设置分通道独立配置**：HTTPS 与威胁检测各自独立配置客户端数和 EPS
- ✅ **失败日志改为文件大小限制（50 MB）**，移除硬编码条数上限
- ✅ **任务面板（TaskPanel）**：所有后台任务（注册/心跳/白名单/日志发送）统一写入 DataGrid，实时显示状态、开始时间、进度、成功/失败计数
- ✅ **策略接收（PolicyReceiveWorker）**：心跳 TCP 流上接收平台下发策略包，解析 PT 包体 JSON 的真实 CMDTYPE/CMDID，自动回 ACK，UI 展示收/回包计数
- ✅ **白名单轮换（WhitelistUploadWorker 重写）**：随机轮换，15 分钟周期，每客户端去重，与 TaskRecord 集成
- ✅ **注册版本选择**：注册时可选客户端版本（ComboBox，默认 V300R011C01B030）
- ✅ **日志发送复用心跳 TCP 流**：避免同 ComputerID 建立双 TCP 连接被平台踢掉
- ✅ **全部 17+ 种日志类型实现完毕**（含原剩余 6 种：USB设备认证、USB访问告警、U盘插拔、软件安装、防火墙、客户端操作）
- ✅ **NetworkStream 写锁（v3.2.0）**：`HeartbeatStreamRegistry` 为每个 clientId 维护独立 `SemaphoreSlim`，心跳写与日志写串行化，彻底解决包体互相穿插导致的 3000 客户端全部离线 + 策略"待发送"问题
- ✅ **HeartbeatWorker 锁超时修复（v3.3.0）**：`lockAcq=false` 时跳过本次心跳（`continue`），而非 `Dispose` 仍然有效的 TCP，杜绝雪崩式重连耗尽平台连接槽；新增 `Reason.LockBusy = 6`，UI 显示 `⚡锁竞争跳过:N`
- ✅ **客户端 OS 类型切换（v3.4.0）**：注册/心跳时可选 Windows / Linux，影响注册包 OS 字段
- ✅ **SSH 平台日志一键收集（v3.4.0，TcpDiagWindow 增强）**：SSH 连接平台 → `su root` 切换权限 → RTT 时间偏差自动检测与补偿 → `find` 递归收集 `/root/logs/**`（保留子目录结构）→ 大文件服务端 `awk` 时间戳过滤 → 同步收集 `/var/log/messages`（系统级 socket/fd 错误）；单个文件失败自动跳过，不影响整体收集
- ✅ **SshLogWindow 独立浮窗（v3.4.0）**：SSH 收集进度在独立子窗口展示，X 键隐藏不关闭，完成后显示输出目录快捷按钮

**已实现的全部日志类型（17 种主类 + 9 种外设子类）：**

*通用/IEG 类（HTTPS 通道）：*
1. 非法程序启动（非白名单）
2. 白名单防篡改
3. 进程审计事件
4. 病毒告警事件
5. 漏洞预警
6. 系统资源异常告警
7. 违规外联
8. USB 设备认证
9. USB 访问告警（IEG）
10. U 盘插拔事件（IEG）

*威胁检测（TCP 长连接，CMDID=21）：*
11. 威胁数据采集-进程启动
12. 威胁数据采集-注册表访问
13. 威胁数据采集-文件访问
14. 威胁数据采集-系统日志
15. 威胁数据采集-DLL 加载

*EDR 专属（HTTPS，DP/SysGuard 通道）：*
16. 文件保护（DP/HostDefence）
17. 注册表保护（RegProtect/HostDefence）
18. 强制访问控制（HostDefence）
19. 系统防护（SysGuard）
20. 软件安装异常（SafetyStore）
21. 防火墙事件（FireWall）
22. 客户端操作（Admin）

*外设控制子类（9 种，共享 CMDID=204，区别在 UsbType 字段）：*
23\~31. 禁用 USB接口 / 手机平板 / CDROM / 无线网卡 / USB网卡 / 软盘 / 蓝牙 / 串口 / 并口

## � 压力测试场景：3000 客户端在线 + 6000 EPS

### 目标

| 指标 | 目标值 |
|---|---|
| 模拟在线客户端数 | 3000 |
| 日志发送速率 | 6000 条/秒（每客户端 2/s）|

### 推荐方案：3 台主机分工

每台主机负责 1000 个客户端，三台合计达到目标。

| 主机 | 起始序号 | 数量 | IP 段 | EPS |
|---|---|---|---|---|
| 主机 A | 1 | 1000 | 192.168.0.1 | 2000 |
| 主机 B | 1001 | 1000 | 192.168.1.1 | 2000 |
| 主机 C | 2001 | 1000 | 192.168.2.1 | 2000 |

> ⚠️ 三台主机必须注册**不同序号范围**的客户端，否则同一 ClientId 的心跳会互相踢掉 session。

### 每台主机操作步骤

```
1. 配置"平台地址"为目标平台 IP（三台相同）
2. 注册区：按上表设置起始序号 / 数量，执行"并发注册"（一次性，结果保存在 Clients.log）
3. 启动 TCP 心跳，保持 1000 个客户端在线
4. 日志发送设置：
     客户端个数     = 1000
     每客户端每秒   = 2
     每客户端总条数 = 视测试时长（10 分钟填 1200，持续测试填 999999）
     并发连接数     = 50（可逐步调高，观察失败率）
     ☑ 压测模式
5. 三台同时点"添加任务"
```

### 日志发送两种模式说明

| | 模拟模式（默认） | 压测模式（勾选）|
|---|---|---|
| **连接方式** | 每条日志新建独立 TLS 连接 | 连接复用，多条日志共享连接 |
| **单机上限** | ~50 客户端（~100 EPS） | 3000+ 客户端（6000+ EPS）|
| **用途** | 验证与真实客户端行为兼容性 | 测试平台日志处理吞吐上限 |
| **平台感知** | — | 请求内容完全相同，平台无法区分 |

### 失败排查

- 失败日志：`logs/logsend-failures-*.log`（分类统计失败原因）
- HTTPS 失败详情：`logs/logsend-failures-https-*.log`（含状态码和响应体）
- 心跳断线日志：`logs/heartbeat_monitor_*.log`（6 种断线原因，含 LockBusy）
- SSH 收集平台日志：TcpDiagWindow → "SSH 日志收集" → 输出到 `debug_collect_yyyyMMdd_HHmmss/`（含 `/root/logs/**` + `var_log_messages`）

---

## �📖 下一步建议

**协议完善：**
- Win2012R2 兼容性（见 `feature/win2012r2-compat` 分支）
- 策略字段覆盖率验证（对齐平台后端期望的全部 JSON 字段）

**稳定性 & 压测：**
- 参考 `scripts/analyze_failures.py` 分析失败分布，进一步降低高并发失败率
- `scripts/estimate_client_limit.ps1` 辅助评估单机客户端上限

**UI 增强：**
- 任务面板列宽自适应 / 支持导出 CSV
- 实时折线图（EPS、心跳在线数随时间变化）

## 🛠️ 技术栈

- .NET 8.0 (WPF)
- 自包含发布（win-x64）
- 持久化：JSON 文件
- 网络协议：HTTP/HTTPS（客户端模拟）

## 📁 项目结构

```
src/
   SimulatorApp/          # WPF 主应用
   SimulatorLib/          # 核心业务逻辑
   SimulatorRunner/       # CLI 运行器
   TestReceiver/          # 测试接收服务器
scripts/                     # 构建和发布脚本
docs/                        # 文档和经验总结
artifacts/                   # 发布产物输出
```

## 📝 重要说明

- **默认开发分支：`main`**（即原 `feature/v4-whitelist-policy-taskpanel`，所有新功能在此分支开发）
- 如需在 GitHub 上设置 `main` 为默认分支：Settings → Branches → Default branch → 改为 `main`
- `Clients.log` 和 `config.json` 在程序首次运行时自动生成
- 修改代码后需重新执行 `publish` 脚本更新 exe
- Git 代理配置（如需要）：`git config --global http.https://github.com.proxy http://127.0.0.1:7897`
- 版本标签脉络：`v2.0.0`（UI完善）→ `v2.1.0`（11种日志/文档）→ `v2.0-tcp-heartbeat-stable`（TCP心跳1000客户端稳定）→ `v2.2.0`（UI布局优化+断线原因监控日志）→ `v2.3.0`（外设控制分类、EPS精确控制、防TLS风暴、压测模式）→ `v2.4.0`（多IP运行时连接计数、连接参数诊断参数优化区）→ `v3.0.0`（5种威胁检测子类、分通道EPS配置、任务面板、策略接收、白名单轮换、版本选择）→ `v3.1.0`（日志发送复用心跳TCP流、全部17+日志类型实现完毕）→ `v3.2.0`（NetworkStream写锁：修复心跳+日志并发写包体损坏，解决3000客户端全部离线+策略待发送问题）→ `v3.3.0`（HeartbeatWorker锁超时修复：lockAcq=false跳过心跳不销毁TCP，新增LockBusy断线原因）→ **`v3.4.0`**（SSH诊断增强：平台日志一键收集、SshLogWindow浮窗、OS类型切换、递归find+relPath、/var/log/messages，当前）
