# CHANGELOG V6.0 (2026-05-16)

## 修复

- **注册 doPost 弹窗移除**：高并发注册（如 500 客户端）时，`RegisterClientToServer` 中 `pdoPost` 失败不再弹出 `AfxMessageBox` 阻塞线程，改为静默返回 FALSE 交由上层重试循环处理。对齐老工具行为。

## 新增

- **RawPacketEngine.dll 入库**：攻击报文模块所需 DLL 纳入项目源码目录，编译后自动拷贝至发布目录。

## 版本

- 主版本号 5.9 → 6.0
