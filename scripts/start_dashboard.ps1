# 启动本地HTTP服务器以访问项目看板
# 访问地址: http://127.0.0.1:5500/project_dashboard.html

Write-Host "🚀 启动本地HTTP服务器..." -ForegroundColor Green
Write-Host "访问地址: " -NoNewline
Write-Host "http://127.0.0.1:5500/project_dashboard.html" -ForegroundColor Cyan
Write-Host ""
Write-Host "按 Ctrl+C 停止服务器" -ForegroundColor Yellow
Write-Host ""

try {
    # 使用Python启动简单的HTTP服务器
    python -m http.server 5500
}
catch {
    Write-Host "❌ 启动失败，请确保已安装Python" -ForegroundColor Red
    Write-Host ""
    Write-Host "替代方案：" -ForegroundColor Yellow
    Write-Host "1. 在VS Code中右键 project_dashboard.html -> Open with Live Server" -ForegroundColor Gray
    Write-Host "2. 或直接双击 project_dashboard.html 在浏览器中打开" -ForegroundColor Gray
    pause
}
