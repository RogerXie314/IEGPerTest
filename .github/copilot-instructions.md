# IEGPerTest Copilot 工作区规则

## 聊天记录留痕（最高优先级）

每轮对话结束后，**必须**把本轮 Q&A 追加到 `docs/聊天记录.md`：

1. 修改 `temp/fix_chatlog.py` 的 `lines` 列表，在末尾追加新条目
2. 运行 `python temp/fix_chatlog.py` 写入文件
3. 格式：`**Q [YYYY-MM-DD HH:MM]: 用户问题**` + `\n\nA: 回答摘要（1-3句）`
4. 时间戳用当前本地时间，精确到分钟
5. **禁止**用 PowerShell `Get-Content` 读写含中文的 .md 文件（GBK/UTF-8 乱码风险）
6. 不得遗漏，不得批量补写

## 代码变更流程

- 提出修复方案后，**必须等用户明确批准**才能动代码
- 编译（cmake build / msbuild）、打包（publish_simulatorapp.ps1）**必须先列改动清单，等用户回复"确认"**
- 标准流程：改代码 → 列清单等确认 → 写变更记录（docs/项目实施文档.md）→ git commit → 打包

## 关键技术备忘

- NativeRunner HB 走 AES+zlib（`PackPT_HB`），Log 走 zlib-only（`PackPT_Log`）
- AES 隔离在 `ieg_aes_wrap.c`（纯C），避免 `common/stdint.h` 与 MSVC `<type_traits>` 的 `int32_t` 冲突
- CMakeLists.txt 必须 `LANGUAGES C CXX`，否则 .c 文件 name mangling 导致链接失败
- 含中文文件一律用 Python `open(..., encoding='utf-8')` 读写
