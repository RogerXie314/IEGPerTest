# 更新日志

## v1.0.1 (2026-04-16)

### 修复
- **修复V2格式解析错误**：V2格式（0xFEFE）的实际结构与预期不同
  - V2格式只包含：dwAddOrDel + dwFullPathLength + szFullPath + dwHashType + dwFileHashLength + szFileHash
  - 不包含 dwIsSyetemFile, dwItemFrom, dwJudgeMethod 字段
  - 修复后可以正确解析V2格式文件

### 测试
- 测试文件：`IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl`
- 文件大小：20,588,636 字节
- 白名单数量：63,939 条
- 格式版本：V2 (0xFEFE)
- 解析结果：✅ 成功

## v1.0.0 (2026-04-16)

### 新功能
- 初始版本
- 支持V2/V3/V4格式白名单文件
- 提供命令行和GUI两种界面
- 支持搜索和导出功能
