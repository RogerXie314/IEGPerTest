# 快速开始指南

## 5分钟上手白名单解析工具

### 第一步：检查Python环境

打开命令行，输入：
```bash
python --version
```

如果显示 `Python 3.x.x`（x >= 6），说明环境正常。

如果没有Python，请从 [python.org](https://www.python.org/downloads/) 下载安装。

### 第二步：测试工具

#### 方法1：使用GUI工具（推荐）

**Windows用户：**
1. 双击 `tools/run_gui.bat`
2. 点击"打开文件"按钮
3. 选择你的.wl文件
4. 查看白名单列表

**其他系统：**
```bash
cd tools
python wl_reader_gui.py
```

#### 方法2：使用命令行工具

```bash
cd tools
python wl_reader.py "你的文件路径.wl"
```

### 第三步：查看示例

如果你有测试文件：
```bash
cd tools
python wl_reader.py "G:\Pictures\stress\wl\IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl" -l -n 10
```

这会显示文件信息和前10条白名单记录。

### 常用命令

```bash
# 查看文件信息和数量
python wl_reader.py file.wl

# 查看所有白名单条目
python wl_reader.py file.wl -l

# 查看前20条
python wl_reader.py file.wl -l -n 20

# 导出到文本文件
python wl_reader.py file.wl -e output.txt

# 查看帮助
python wl_reader.py -h
```

### GUI工具功能

1. **打开文件** - 选择.wl文件
2. **搜索** - 在搜索框输入关键词（文件名或Hash）
3. **导出** - 点击"导出文本"保存为.txt文件
4. **刷新** - 重新加载当前文件

### 预期输出示例

```
============================================================
白名单文件: IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl
文件路径: G:\Pictures\stress\wl\IEG-WorkstationDefender-res-wlFull_2_20231205172338.wl
文件版本: 0xFEFB
白名单数量: 90123
============================================================

[1] 路径: C:\Program Files\Common Files\tipresx.dll.mui
  操作: 添加
  系统文件: 是
  来源: 0
  判定方式: 0
  Hash类型: SHA1
  Hash值: 2A34EF9409BCD7...

[2] 路径: C:\Program Files\Common Files\tipresx.dll.mui
  操作: 添加
  系统文件: 是
  来源: 0
  判定方式: 0
  Hash类型: SHA1
  Hash值: A3AF42334110C4...

...
```

### 故障排除

**问题：找不到python命令**
- 解决：安装Python 3.6+，安装时勾选"Add Python to PATH"

**问题：GUI无法启动**
- 解决：确保安装了tkinter（Python通常自带）
- 测试：`python -m tkinter`

**问题：文件解析失败**
- 检查文件是否为有效的.wl文件
- 尝试用命令行工具查看详细错误

**问题：中文显示乱码**
- Windows: 在命令行执行 `chcp 65001`
- 或使用GUI工具（自动处理编码）

### 下一步

- 阅读 `README.md` 了解详细功能
- 阅读 `INTEGRATION_GUIDE.md` 了解如何集成到项目
- 阅读 `PROJECT_SUMMARY.md` 了解技术细节

### 需要帮助？

如果遇到问题：
1. 查看 `README.md` 的故障排除部分
2. 运行测试脚本：`python test_reader.py`
3. 检查Python版本是否 >= 3.6
