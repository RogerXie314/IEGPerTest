using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Input;
using SimulatorLib.Config;

namespace SimulatorApp.ViewModels
{
    public class VersionManagementViewModel : INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler? PropertyChanged;
        private void OnProp([CallerMemberName] string? name = null) => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

        private ClientVersionConfig _config;
        private string _selectedOsType = "Windows";
        private string? _selectedVersion;
        private string _newVersion = string.Empty;

        public ObservableCollection<string> WindowsVersions { get; }
        public ObservableCollection<string> LinuxVersions { get; }

        public string SelectedOsType
        {
            get => _selectedOsType;
            set
            {
                if (_selectedOsType != value)
                {
                    _selectedOsType = value;
                    OnProp();
                    OnProp(nameof(CurrentVersions));
                    OnProp(nameof(IsWindows));
                    OnProp(nameof(IsLinux));
                    SelectedVersion = null;
                }
            }
        }

        public bool IsWindows
        {
            get => _selectedOsType == "Windows";
            set { if (value) SelectedOsType = "Windows"; }
        }

        public bool IsLinux
        {
            get => _selectedOsType == "Linux";
            set { if (value) SelectedOsType = "Linux"; }
        }

        public ObservableCollection<string> CurrentVersions =>
            _selectedOsType == "Windows" ? WindowsVersions : LinuxVersions;

        public string? SelectedVersion
        {
            get => _selectedVersion;
            set
            {
                _selectedVersion = value;
                OnProp();
                CommandManager.InvalidateRequerySuggested();
            }
        }

        public string NewVersion
        {
            get => _newVersion;
            set
            {
                _newVersion = value;
                OnProp();
                CommandManager.InvalidateRequerySuggested();
            }
        }

        public ICommand AddCommand { get; }
        public ICommand DeleteCommand { get; }
        public ICommand SaveCommand { get; }
        public ICommand ResetCommand { get; }

        public VersionManagementViewModel()
        {
            _config = ClientVersionConfig.Load();
            WindowsVersions = new ObservableCollection<string>(_config.WindowsVersions);
            LinuxVersions = new ObservableCollection<string>(_config.LinuxVersions);

            AddCommand = new RelayCommand(_ => AddVersion(), _ => CanAddVersion());
            DeleteCommand = new RelayCommand(_ => DeleteVersion(), _ => CanDeleteVersion());
            SaveCommand = new RelayCommand(_ => SaveConfig());
            ResetCommand = new RelayCommand(_ => ResetToDefault());
        }

        private bool CanAddVersion()
        {
            return !string.IsNullOrWhiteSpace(NewVersion) && !CurrentVersions.Contains(NewVersion.Trim());
        }

        private void AddVersion()
        {
            var version = NewVersion.Trim();
            if (!string.IsNullOrWhiteSpace(version) && !CurrentVersions.Contains(version))
            {
                CurrentVersions.Add(version);
                NewVersion = string.Empty;
            }
        }

        private bool CanDeleteVersion()
        {
            return !string.IsNullOrEmpty(SelectedVersion);
        }

        private void DeleteVersion()
        {
            if (SelectedVersion != null && CurrentVersions.Contains(SelectedVersion))
            {
                var result = MessageBox.Show(
                    $"确定要删除版本 \"{SelectedVersion}\" 吗？",
                    "确认删除",
                    MessageBoxButton.YesNo,
                    MessageBoxImage.Question);

                if (result == MessageBoxResult.Yes)
                {
                    CurrentVersions.Remove(SelectedVersion);
                    SelectedVersion = null;
                }
            }
        }

        private void SaveConfig()
        {
            try
            {
                _config.WindowsVersions = WindowsVersions.ToList();
                _config.LinuxVersions = LinuxVersions.ToList();
                _config.Save();

                MessageBox.Show("版本配置已保存", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
            }
            catch (Exception ex)
            {
                MessageBox.Show($"保存失败: {ex.Message}", "错误", MessageBoxButton.OK, MessageBoxImage.Error);
            }
        }

        private void ResetToDefault()
        {
            var result = MessageBox.Show(
                "确定要恢复默认版本列表吗？当前的自定义版本将被清除。",
                "确认重置",
                MessageBoxButton.YesNo,
                MessageBoxImage.Warning);

            if (result == MessageBoxResult.Yes)
            {
                _config = ClientVersionConfig.ResetToDefault();
                WindowsVersions.Clear();
                LinuxVersions.Clear();
                foreach (var v in _config.WindowsVersions) WindowsVersions.Add(v);
                foreach (var v in _config.LinuxVersions) LinuxVersions.Add(v);
                SelectedVersion = null;

                MessageBox.Show("已恢复默认版本列表", "成功", MessageBoxButton.OK, MessageBoxImage.Information);
            }
        }
    }
}
