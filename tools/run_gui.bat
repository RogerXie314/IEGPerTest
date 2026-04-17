@echo off
chcp 65001 >nul
echo 启动白名单文件解析工具...
python wl_reader_gui.py
if errorlevel 1 (
    echo.
    echo 启动失败！请确保已安装Python 3.6或更高版本。
    pause
)
