# 启动本地HTTP服务器以访问项目看板
# 访问地址: http://127.0.0.1:5500/docs/project_dashboard.html

Write-Host "Starting local HTTP server..." -ForegroundColor Green
Write-Host "Open: " -NoNewline
Write-Host "http://127.0.0.1:5500/docs/project_dashboard.html" -ForegroundColor Cyan
Write-Host ""
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host ""

try {
    # Serve from project root so paths like /docs/... resolve correctly
    python -m http.server 5500
}
catch {
    Write-Host "Failed to start. Make sure Python is installed." -ForegroundColor Red
    Write-Host ""
    Write-Host "Alternatives:" -ForegroundColor Yellow
    Write-Host "1. Right-click docs/project_dashboard.html in VS Code -> Open with Live Server" -ForegroundColor Gray
    Write-Host "2. Or open docs/project_dashboard.html directly in browser" -ForegroundColor Gray
    pause
}
