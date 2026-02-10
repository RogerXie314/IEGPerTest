样例字段与原始客户端字段对照

说明：下面把你提供的样例（docs/JSON/log.log）中常见键映射到原始客户端 JSON（由 WarningLog_GetJsonByVector 生成）的期望字段及来源，并列出差异与注意点。

通用外层（原始客户端期望的外层信封）
- ComputerID : 客户端机器标识（样例中使用 `machinecode` / `machineCode`）
- CMDTYPE / CMDID : 固定命令类型/ID，由发送方设置
- CMDContent : 告警条目数组（内部使用下列字段）

告警条目字段映射（原始字段 -> 样例常见键 / 说明）
- Time : `logtime` 或 `receiptdate`（样例格式 ISO8601），原始来自 audit llTime，格式为 "YYYY-MM-DD HH:MM:SS"。
- HoldBack : 样例无通用字段，原始由 `bHoldback` 布尔位决定（是否为阻断/延后）。
- IntegrityCheck : 原始由 `bIntegrityCheckFailed`（布尔），样例可能通过 `threat`/其他标志间接反映。
- CertCheck : 原始由 `bCertCheckFailed`（布尔），样例不统一提供。
- Type : 原始为 numeric（调用 `WarningLog_Type_2_DB` 映射后填入），样例用中文 `usmeventtype` / `eventname` / `usmalarmtype` 表示类型；需保留样例字段用于 UI 文本，但 JSON 必须包含 numeric `Type`。
- SubType : 原始为 numeric（映射后的细分类），样例通常通过 `usmalarmtype` / `eventcategory` 表示文本层次。
- FullPath : 样例多用 `curl` / `virusPath` / `cprogram` 等表示路径或程序名。
- ParentProcess : `parentprocess`（样例已有）
- CompanyName : `companyname` 或 `companyName` / `cproduct`（样例有 `companyname` / `cproduct`） → 映射到 `CompanyName`。
- ProductName : `cproduct` / `cproduct`（样例） → 映射到 `ProductName`。
- Version : `cproductver` / `cproductver`（样例） → 映射到 `Version`。
- UserName : `user_name` 或 `user_name`（样例）→ 映射到 `UserName`。
- Hash : 样例字段 `hash` 或 `hashvalue`（有时为内核完整性）→ 对应原始 `Hash`（来自内核审计 `szIntegrity`，通常 hex）。
- IEGHash / DefIntegrity : 样例字段 `ieghashvalue` 有时出现 → 对应原始计算后的文件 SHA1（IEGHash/DefIntegrity）。保证为大写 hex 格式。

样例中额外常见但原始 JSON 可能没有或另作展示的字段（建议在模拟器中同时保留）
- ceventdesc / ceventdesc_en / desc : 告警描述文本（UI 常用），保留
- clientip / assetIp / caffectedip : 客户端/目标 IP，保留
- clientname / aliasName : 主机名或别名，保留
- safedeviceid / devId : 设备 ID，保留
- machinecode / machineCode : 映射到 `ComputerID`（外层）
- eventlevel / eventlevel_en / eventcategory / eventcategory_en : 严重性/分类文本，保留
- threat / result / riskLevel : 风险属性，保留
- virusName / virusType / virusPath : 病毒字段（病毒告警特定结构，保持现有样式）

差异与注意事项
- 字段名称与大小写必须精确：原始 JSON 使用如 `Time`,`FullPath`,`ParentProcess`,`CompanyName`,`ProductName`,`Version`,`UserName`,`Hash`,`IEGHash`（注意驼峰与大小写）。
- Type/SubType 必须是数值（整数），不能仅发送文本字段；映射规则应参考 `WLUtils::WarningLog_Type_2_DB`。
- Hash 与 IEGHash 的含义：`Hash` 通常来自内核提供的完整性字段；`IEGHash`（或 DefIntegrity）是用户空间计算的 SHA1（需大写 hex）。样例中有时二者相同或交换命名，模拟器应按优先级填充：若有 `hash` 和 `ieghashvalue`，分别赋给 `Hash` 与 `IEGHash`；若只有一个哈希，优先当作 `Hash` 并同时计算/复制到 `IEGHash`（以兼容平台显示）。
- 时间格式：样例中为 ISO8601（带时区），原始 builder 输出为可读时间戳。建议模拟器输出两者：`Time`（原始格式）并在外层或额外字段保留 `receiptdate`（ISO8601）以匹配样例。
- 外层信封（`ComputerID`/`CMDTYPE`/`CMDID`/`CMDContent`）必须保留，平台后端可能依赖该结构解析。

接下来的具体可执行项（按优先级）
1. 在 `LogJsonBuilder` 中：确保 `Process/Whitelist/DP/MAC` builder 输出上述原始字段名（严格大小写），并同时保留样例的文本字段（如 `ceventdesc`、`clientip` 等）。
2. 在类型映射处：实现 `WarningLog_Type_2_DB` 的等价映射（或直接包含映射表）以产生正确的 `Type`/`SubType` 数值。
3. 哈希处理：保留 `ComputeFakeSha1` 产物用于 `IEGHash`，并确保 `Hash` 用大写 hex；若样例同时带 `hashvalue`/`ieghashvalue`，按上文规则映射。
4. 时间输出：在 `Time` 字段使用原始可读格式，并同时保留 `receiptdate`（ISO8601）以兼容样例。

文件位置建议（将要修改）
- `src/SimulatorLib/Protocol/LogJsonBuilder.cs` — 主构建器
- `src/SimulatorLib/Workers/LogWorker.cs` — 发送逻辑（确保外层信封与 endpoint 匹配）
- `src/SimulatorLib/Models/LogCategory.cs` — 可加入数值映射表或将映射逻辑内联至 `LogJsonBuilder`

---
已完成样例解析并生成对照表。下一步我将对比原项目逻辑与当前模拟器实现的具体差异，并列出需要修改的代码位置与补丁。