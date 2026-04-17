# V2格式解析问题修复说明

## 问题描述

初次运行工具时出现错误：
```
SyntaxError: Non-UTF-8 code starting with '\xfe' in file ... on line 1
```

## 问题原因

**错误假设：** 基于代码分析，假设V2格式包含以下字段：
```
dwAddOrDel
dwIsSyetemFile      <- 错误！V2没有这个字段
dwFullPathLength
szFullPath
...
```

**实际情况：** 通过十六进制分析发现，V2格式更简化：
```
文件头: FE FE (版本标识)
记录1:  01 00 00 00 (dwAddOrDel = 1)
        92 00 00 00 (dwFullPathLength = 146) <- 直接是路径长度！
        43 00 3A 00 5C 00... (路径数据)
```

## 格式对比

### V2格式 (0xFEFE) - 实际结构
```
[2字节] 版本标识
[重复] 每条记录:
  [4字节] dwAddOrDel
  [4字节] dwFullPathLength
  [变长] szFullPath
  [4字节] dwHashType
  [4字节] dwFileHashLength
  [变长] szFileHash
```

### V3/V4格式 (0xFEFC/0xFEFB) - 扩展结构
```
[2字节] 版本标识
[重复] 每条记录:
  [4字节] dwAddOrDel
  [4字节] dwIsSyetemFile      <- V3/V4新增
  [4字节] dwItemFrom          <- V3/V4新增
  [4字节] dwJudgeMethod       <- V3/V4新增
  [4字节] dwFullPathLength
  [变长] szFullPath
  [4字节] dwHashType
  [4字节] dwFileHashLength
  [变长] szFileHash
```

## 修复方法

修改 `wl_reader.py` 中的 `_read_v2()` 函数：

**修复前：**
```python
def _read_v2(self, f) -> bool:
    # 读取 dwAddOrDel
    entry.add_or_del = struct.unpack('<I', f.read(4))[0]
    
    # 读取 dwIsSyetemFile  <- 错误！
    entry.is_system_file = struct.unpack('<I', f.read(4))[0]
    
    # 读取 dwFullPathLength
    path_length = struct.unpack('<I', f.read(4))[0]
    ...
```

**修复后：**
```python
def _read_v2(self, f) -> bool:
    # 读取 dwAddOrDel
    entry.add_or_del = struct.unpack('<I', f.read(4))[0]
    
    # V2格式直接是路径长度，没有 dwIsSyetemFile
    # 读取 dwFullPathLength
    path_length = struct.unpack('<I', f.read(4))[0]
    ...
```

## 测试结果

**测试文件：** `IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl`

**修复前：**
```
SyntaxError: Non-UTF-8 code starting with '\xfe'
```

**修复后：**
```
文件版本: 0xFEFE
成功读取 63939 条白名单记录
============================================================
白名单文件: IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl
文件版本: 0xFEFE
白名单数量: 63939
============================================================
```

## 经验教训

1. **不要完全依赖代码分析**
   - 代码中可能有多个版本的实现
   - 实际使用的格式可能与代码不一致

2. **使用十六进制分析验证**
   - 创建 `debug_wl.py` 工具查看原始字节
   - 对比实际数据和预期结构

3. **版本演进的复杂性**
   - V2 → V3 → V4 逐步增加字段
   - 不能假设所有版本都有相同的基础字段

4. **测试驱动开发**
   - 先用实际文件测试
   - 根据错误调整实现
   - 再次测试验证

## 调试工具

创建了 `debug_wl.py` 用于分析文件格式：
```bash
python debug_wl.py file.wl
```

输出：
- 文件大小
- 前100字节的十六进制dump
- 版本标识
- 第一条记录的字段解析

这个工具对于分析未知格式非常有用。
