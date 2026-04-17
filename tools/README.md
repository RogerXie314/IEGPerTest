# 白名单文件解析工具

## 功能说明

这是一个用于解析IEG白名单文件(.wl)的工具集，可以读取白名单文件并显示其中的内容。

## 文件说明

- `wl_reader.py` - 命令行版本的解析工具
- `wl_reader_gui.py` - 图形界面版本的解析工具
- `run_gui.bat` - Windows批处理文件，用于快速启动GUI工具

## 白名单文件格式

白名单文件(.wl)包含以下信息：
- 文件版本标识
- 白名单条目列表，每条包含：
  - 文件路径
  - 操作类型（添加/删除）
  - 是否系统文件
  - 来源信息
  - 判定方式
  - Hash类型（SHA1/MD5）
  - Hash值

## 使用方法

### 命令行版本

```bash
# 显示摘要信息
python wl_reader.py file.wl

# 显示所有条目
python wl_reader.py file.wl -l

# 显示前10条
python wl_reader.py file.wl -l -n 10

# 导出到文本文件
python wl_reader.py file.wl -e output.txt

# 查看帮助
python wl_reader.py -h
```

### GUI版本

```bash
# 启动GUI工具
python wl_reader_gui.py

# 或者直接打开指定文件
python wl_reader_gui.py file.wl

# Windows用户可以双击 run_gui.bat
```

### GUI功能

1. **打开文件** - 选择并加载.wl文件
2. **搜索** - 在文件路径和Hash值中搜索
3. **导出文本** - 将白名单导出为文本文件
4. **刷新** - 重新加载当前文件

## 系统要求

- Python 3.6 或更高版本
- tkinter（GUI版本需要，通常Python自带）

## 示例

假设你有一个白名单文件 `IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl`：

```bash
# 查看文件信息和白名单数量
python wl_reader.py IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl

# 查看所有白名单条目
python wl_reader.py IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl -l

# 导出为文本文件
python wl_reader.py IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl -e whitelist.txt
```

## 注意事项

1. 工具目前支持V2、V3、V4版本的白名单文件格式
2. 文件采用UTF-16LE编码存储路径和Hash值
3. 如果文件无法解析，请检查文件是否损坏或格式不正确

## 技术细节

### 文件结构

**V2格式 (0xFEFE) - 最常见：**
```
[2字节] 版本标识: 0xFEFE
[重复] 白名单条目:
  [4字节] dwAddOrDel - 操作类型 (1=添加, 2=删除)
  [4字节] dwFullPathLength - 路径长度（字节）
  [变长] szFullPath - 文件路径 (UTF-16LE)
  [4字节] dwHashType - Hash类型 (1=SHA1, 2=MD5)
  [4字节] dwFileHashLength - Hash长度（字节）
  [变长] szFileHash - Hash值 (UTF-16LE)
```

**V3/V4格式 (0xFEFC/0xFEFB) - 扩展版：**
```
[2字节] 版本标识: 0xFEFC 或 0xFEFB
[重复] 白名单条目:
  [4字节] dwAddOrDel - 操作类型 (1=添加, 2=删除)
  [4字节] dwIsSyetemFile - 是否系统文件
  [4字节] dwItemFrom - 来源
  [4字节] dwJudgeMethod - 判定方式
  [4字节] dwFullPathLength - 路径长度（字节）
  [变长] szFullPath - 文件路径 (UTF-16LE)
  [4字节] dwHashType - Hash类型 (1=SHA1, 2=MD5)
  [4字节] dwFileHashLength - Hash长度（字节）
  [变长] szFileHash - Hash值 (UTF-16LE)
```

**注意：** V2格式是最简化的格式，不包含系统文件标识、来源和判定方式字段。

## 故障排除

### 问题：无法启动GUI工具
- 确保已安装Python 3.6+
- 确保tkinter已安装（`python -m tkinter` 测试）

### 问题：文件解析失败
- 检查文件是否为有效的.wl文件
- 检查文件是否损坏
- 尝试使用命令行版本查看详细错误信息

### 问题：中文显示乱码
- 确保终端支持UTF-8编码
- Windows用户可以使用 `chcp 65001` 切换到UTF-8

## 更新日志

### v1.0.0 (2026-04-16)
- 初始版本
- 支持V2/V3/V4格式白名单文件
- 提供命令行和GUI两种界面
- 支持搜索和导出功能
