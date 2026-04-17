# 白名单解析工具集成指南

## 概述

本文档说明如何将白名单解析工具集成到现有的IEGPerTest项目中。

## 集成方案

### 方案1：作为独立工具使用

最简单的方式是将工具作为独立程序使用，不需要修改现有项目代码。

**优点：**
- 无需修改现有代码
- 可以独立运行和测试
- 易于维护和更新

**使用方式：**
```bash
# 命令行方式
python tools/wl_reader.py path/to/file.wl -l

# GUI方式
python tools/wl_reader_gui.py
```

### 方案2：集成到C#项目中

如果需要在SimulatorApp中集成白名单预览功能，可以通过以下方式：

#### 2.1 使用Process调用Python脚本

```csharp
using System.Diagnostics;
using System.IO;

public class WhitelistPreview
{
    public static int GetWhitelistCount(string wlFilePath)
    {
        // 调用Python脚本获取白名单数量
        var startInfo = new ProcessStartInfo
        {
            FileName = "python",
            Arguments = $"tools/wl_reader.py \"{wlFilePath}\"",
            RedirectStandardOutput = true,
            UseShellExecute = false,
            CreateNoWindow = true
        };
        
        using (var process = Process.Start(startInfo))
        {
            string output = process.StandardOutput.ReadToEnd();
            process.WaitForExit();
            
            // 解析输出获取数量
            // 输出格式: "白名单数量: 90000"
            var match = System.Text.RegularExpressions.Regex.Match(
                output, @"白名单数量:\s*(\d+)");
            
            if (match.Success)
            {
                return int.Parse(match.Groups[1].Value);
            }
        }
        
        return -1;
    }
    
    public static void ShowWhitelistGUI(string wlFilePath)
    {
        // 启动GUI工具显示白名单
        Process.Start(new ProcessStartInfo
        {
            FileName = "python",
            Arguments = $"tools/wl_reader_gui.py \"{wlFilePath}\"",
            UseShellExecute = true
        });
    }
}
```

#### 2.2 在XAML中添加按钮

```xml
<!-- 在上传白名单的界面添加预览按钮 -->
<Button Content="预览白名单" 
        Command="{Binding PreviewWhitelistCommand}"
        Margin="5"/>
```

#### 2.3 在ViewModel中实现命令

```csharp
public class UploadViewModel : ViewModelBase
{
    private string _selectedWlFile;
    
    public ICommand PreviewWhitelistCommand { get; }
    
    public UploadViewModel()
    {
        PreviewWhitelistCommand = new RelayCommand(
            () => PreviewWhitelist(),
            () => !string.IsNullOrEmpty(_selectedWlFile) && File.Exists(_selectedWlFile)
        );
    }
    
    private void PreviewWhitelist()
    {
        try
        {
            // 显示白名单数量
            int count = WhitelistPreview.GetWhitelistCount(_selectedWlFile);
            if (count > 0)
            {
                MessageBox.Show($"白名单文件包含 {count:N0} 条记录", "白名单信息");
            }
            
            // 打开GUI工具
            WhitelistPreview.ShowWhitelistGUI(_selectedWlFile);
        }
        catch (Exception ex)
        {
            MessageBox.Show($"预览失败: {ex.Message}", "错误");
        }
    }
}
```

### 方案3：使用C#重写解析逻辑

如果不想依赖Python，可以用C#重写解析逻辑：

```csharp
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

public class WhitelistEntry
{
    public int AddOrDel { get; set; }
    public int IsSystemFile { get; set; }
    public int ItemFrom { get; set; }
    public int JudgeMethod { get; set; }
    public string FullPath { get; set; }
    public int HashType { get; set; }
    public string FileHash { get; set; }
}

public class WhitelistReader
{
    private const ushort WL_VERSION_4 = 0xFEFB;
    private const ushort WL_VERSION_3 = 0xFEFC;
    private const ushort WL_VERSION_2 = 0xFEFE;
    
    public ushort Version { get; private set; }
    public List<WhitelistEntry> Entries { get; private set; }
    
    public WhitelistReader()
    {
        Entries = new List<WhitelistEntry>();
    }
    
    public bool Read(string filePath)
    {
        try
        {
            using (var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read))
            using (var reader = new BinaryReader(fs))
            {
                // 读取版本头
                Version = reader.ReadUInt16();
                
                // 根据版本解析
                if (Version == WL_VERSION_4 || Version == WL_VERSION_3)
                {
                    return ReadV4(reader);
                }
                else if (Version == WL_VERSION_2)
                {
                    return ReadV2(reader);
                }
                
                return false;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"读取失败: {ex.Message}");
            return false;
        }
    }
    
    private bool ReadV4(BinaryReader reader)
    {
        while (reader.BaseStream.Position < reader.BaseStream.Length)
        {
            try
            {
                var entry = new WhitelistEntry();
                
                // 读取字段
                entry.AddOrDel = reader.ReadInt32();
                entry.IsSystemFile = reader.ReadInt32();
                entry.ItemFrom = reader.ReadInt32();
                entry.JudgeMethod = reader.ReadInt32();
                
                // 读取路径
                int pathLength = reader.ReadInt32();
                if (pathLength > 0)
                {
                    byte[] pathBytes = reader.ReadBytes(pathLength);
                    entry.FullPath = Encoding.Unicode.GetString(pathBytes).TrimEnd('\0');
                }
                
                // 读取Hash
                entry.HashType = reader.ReadInt32();
                int hashLength = reader.ReadInt32();
                if (hashLength > 0)
                {
                    byte[] hashBytes = reader.ReadBytes(hashLength);
                    entry.FileHash = Encoding.Unicode.GetString(hashBytes).TrimEnd('\0');
                }
                
                Entries.Add(entry);
            }
            catch (EndOfStreamException)
            {
                break;
            }
        }
        
        return true;
    }
    
    private bool ReadV2(BinaryReader reader)
    {
        // V2版本没有 ItemFrom 和 JudgeMethod 字段
        while (reader.BaseStream.Position < reader.BaseStream.Length)
        {
            try
            {
                var entry = new WhitelistEntry();
                
                entry.AddOrDel = reader.ReadInt32();
                entry.IsSystemFile = reader.ReadInt32();
                
                int pathLength = reader.ReadInt32();
                if (pathLength > 0)
                {
                    byte[] pathBytes = reader.ReadBytes(pathLength);
                    entry.FullPath = Encoding.Unicode.GetString(pathBytes).TrimEnd('\0');
                }
                
                entry.HashType = reader.ReadInt32();
                int hashLength = reader.ReadInt32();
                if (hashLength > 0)
                {
                    byte[] hashBytes = reader.ReadBytes(hashLength);
                    entry.FileHash = Encoding.Unicode.GetString(hashBytes).TrimEnd('\0');
                }
                
                Entries.Add(entry);
            }
            catch (EndOfStreamException)
            {
                break;
            }
        }
        
        return true;
    }
}
```

## 推荐方案

**开发阶段：** 使用方案1（独立工具），快速验证功能

**集成阶段：** 
- 如果Python环境可用，使用方案2（Process调用）
- 如果需要完全独立，使用方案3（C#重写）

## 使用示例

### 示例1：上传前预览

```csharp
private void OnSelectWlFile()
{
    var dialog = new OpenFileDialog
    {
        Filter = "白名单文件 (*.wl)|*.wl|所有文件 (*.*)|*.*"
    };
    
    if (dialog.ShowDialog() == true)
    {
        SelectedWlFile = dialog.FileName;
        
        // 自动显示白名单数量
        int count = WhitelistPreview.GetWhitelistCount(SelectedWlFile);
        WhitelistCount = count;
        StatusMessage = $"已选择白名单文件，包含 {count:N0} 条记录";
    }
}
```

### 示例2：添加到主界面

在SimulatorApp的主界面添加一个"工具"菜单：

```xml
<Menu>
    <MenuItem Header="工具(_T)">
        <MenuItem Header="白名单解析器" 
                  Command="{Binding OpenWhitelistToolCommand}"/>
    </MenuItem>
</Menu>
```

```csharp
public ICommand OpenWhitelistToolCommand { get; }

private void OpenWhitelistTool()
{
    Process.Start(new ProcessStartInfo
    {
        FileName = "python",
        Arguments = "tools/wl_reader_gui.py",
        UseShellExecute = true,
        WorkingDirectory = AppDomain.CurrentDomain.BaseDirectory
    });
}
```

## 注意事项

1. **Python依赖**：如果使用方案2，确保目标机器安装了Python 3.6+
2. **路径问题**：使用相对路径时注意工作目录
3. **错误处理**：添加适当的异常处理和用户提示
4. **性能考虑**：大文件（>10万条）可能需要几秒钟加载时间

## 测试清单

- [ ] 测试小文件（<1000条）
- [ ] 测试大文件（>50000条）
- [ ] 测试不同版本格式（V2, V3, V4）
- [ ] 测试错误文件处理
- [ ] 测试中文路径
- [ ] 测试搜索功能
- [ ] 测试导出功能
