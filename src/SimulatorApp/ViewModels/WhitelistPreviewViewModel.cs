using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Text;
using System.Windows;
using System.Windows.Input;
using Microsoft.Win32;
using SimulatorApp.Utils;

namespace SimulatorApp.ViewModels
{
    public class WhitelistPreviewViewModel : INotifyPropertyChanged
    {
        private string _searchText = string.Empty;
        private List<WhitelistEntryDisplay> _allEntries = new();
        private ObservableCollection<WhitelistEntryDisplay> _filteredEntries = new();

        public string FileName { get; private set; } = string.Empty;
        public string FileVersion { get; private set; } = string.Empty;
        public string FileSize { get; private set; } = string.Empty;
        public int EntryCount { get; private set; }
        public int TotalCount => _allEntries.Count;
        public int FilteredCount => _filteredEntries.Count;

        public string SearchText
        {
            get => _searchText;
            set
            {
                if (_searchText != value)
                {
                    _searchText = value;
                    OnPropertyChanged();
                    ApplyFilter();
                }
            }
        }

        public ObservableCollection<WhitelistEntryDisplay> FilteredEntries
        {
            get => _filteredEntries;
            set
            {
                _filteredEntries = value;
                OnPropertyChanged();
                OnPropertyChanged(nameof(FilteredCount));
            }
        }

        public ICommand ClearSearchCommand { get; }
        public ICommand ExportCommand { get; }

        public WhitelistPreviewViewModel(string filePath)
        {
            ClearSearchCommand = new RelayCommand(_ => SearchText = string.Empty);
            ExportCommand = new RelayCommand(_ => ExportToFile());

            LoadWhitelistFile(filePath);
        }

        private void LoadWhitelistFile(string filePath)
        {
            try
            {
                if (!File.Exists(filePath))
                {
                    MessageBox.Show($"文件不存在：{filePath}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    return;
                }

                var fileInfo = new FileInfo(filePath);
                FileName = fileInfo.Name;
                FileSize = FormatFileSize(fileInfo.Length);

                var reader = new WhitelistReader(filePath);
                if (!reader.Read())
                {
                    MessageBox.Show("无法读取白名单文件，请检查文件格式", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
                    return;
                }

                FileVersion = $"0x{reader.Version:X4}";
                EntryCount = reader.Entries.Count;

                _allEntries = reader.Entries.Select((e, index) => new WhitelistEntryDisplay
                {
                    Index = index + 1,
                    FileName = Path.GetFileName(e.FullPath),
                    FullPath = e.FullPath,
                    Action = e.AddOrDel == 1 ? "添加" : e.AddOrDel == 2 ? "删除" : "未知",
                    HashType = e.HashType == 1 ? "SHA1" : e.HashType == 2 ? "MD5" : "未知",
                    FileHash = e.FileHash
                }).ToList();

                ApplyFilter();

                OnPropertyChanged(nameof(FileName));
                OnPropertyChanged(nameof(FileVersion));
                OnPropertyChanged(nameof(FileSize));
                OnPropertyChanged(nameof(EntryCount));
                OnPropertyChanged(nameof(TotalCount));
            }
            catch (Exception ex)
            {
                MessageBox.Show($"加载白名单文件失败：{ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ApplyFilter()
        {
            if (string.IsNullOrWhiteSpace(_searchText))
            {
                FilteredEntries = new ObservableCollection<WhitelistEntryDisplay>(_allEntries);
            }
            else
            {
                var searchLower = _searchText.ToLower();
                var filtered = _allEntries.Where(e =>
                    e.FileName.ToLower().Contains(searchLower) ||
                    e.FullPath.ToLower().Contains(searchLower) ||
                    e.FileHash.ToLower().Contains(searchLower)
                ).ToList();
                FilteredEntries = new ObservableCollection<WhitelistEntryDisplay>(filtered);
            }
        }

        private void ExportToFile()
        {
            try
            {
                var dialog = new SaveFileDialog
                {
                    Filter = "文本文件 (*.txt)|*.txt|所有文件 (*.*)|*.*",
                    FileName = $"{Path.GetFileNameWithoutExtension(FileName)}_导出.txt"
                };

                if (dialog.ShowDialog() == true)
                {
                    var sb = new StringBuilder();
                    sb.AppendLine($"白名单文件：{FileName}");
                    sb.AppendLine($"文件版本：{FileVersion}");
                    sb.AppendLine($"白名单数量：{EntryCount}");
                    sb.AppendLine($"文件大小：{FileSize}");
                    sb.AppendLine(new string('=', 80));
                    sb.AppendLine();

                    foreach (var entry in FilteredEntries)
                    {
                        sb.AppendLine($"[{entry.Index}]");
                        sb.AppendLine($"  文件名：{entry.FileName}");
                        sb.AppendLine($"  路径：{entry.FullPath}");
                        sb.AppendLine($"  操作：{entry.Action}");
                        sb.AppendLine($"  Hash类型：{entry.HashType}");
                        sb.AppendLine($"  Hash值：{entry.FileHash}");
                        sb.AppendLine();
                    }

                    File.WriteAllText(dialog.FileName, sb.ToString(), Encoding.UTF8);
                    MessageBox.Show($"已导出 {FilteredEntries.Count} 条记录到：\n{dialog.FileName}", 
                        "导出成功", MessageBoxButton.OK, MessageBoxImage.Information);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show($"导出失败：{ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private static string FormatFileSize(long bytes)
        {
            string[] sizes = { "B", "KB", "MB", "GB" };
            double len = bytes;
            int order = 0;
            while (len >= 1024 && order < sizes.Length - 1)
            {
                order++;
                len /= 1024;
            }
            return $"{len:0.##} {sizes[order]}";
        }

        public event PropertyChangedEventHandler? PropertyChanged;

        protected virtual void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        {
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
        }
    }

    public class WhitelistEntryDisplay
    {
        public int Index { get; set; }
        public string FileName { get; set; } = string.Empty;
        public string FullPath { get; set; } = string.Empty;
        public string Action { get; set; } = string.Empty;
        public string HashType { get; set; } = string.Empty;
        public string FileHash { get; set; } = string.Empty;
    }
}
