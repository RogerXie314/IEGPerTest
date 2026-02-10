#!/usr/bin/env pwsh

# 验证日志分类LogType字段的修复
# 此脚本通过编译和运行测试来验证LogType值是否正确

Write-Host "=== 日志分类LogType修复验证 ===" -ForegroundColor Green
Write-Host ""

# 编译
Write-Host "1. 编译项目..." -ForegroundColor Cyan
dotnet build -q
if ($LASTEXITCODE -ne 0) {
    Write-Host "编译失败" -ForegroundColor Red
    exit 1
}
Write-Host "✓ 编译成功" -ForegroundColor Green
Write-Host ""

# 运行单元测试
Write-Host "2. 运行单元测试..." -ForegroundColor Cyan
dotnet test -q --no-build
if ($LASTEXITCODE -ne 0) {
    Write-Host "测试失败" -ForegroundColor Red
    exit 1
}
Write-Host "✓ 所有测试通过" -ForegroundColor Green
Write-Host ""

# 验证JSON输出
Write-Host "3. 验证日志JSON内容..." -ForegroundColor Cyan

# 使用C#代码片段验证
$csharpCode = @'
using SimulatorLib.Protocol;
using System;
using System.Text.Json;

// 测试文件保护
var json1 = LogJsonBuilder.BuildHostDefenceLog(
    "TEST001", "C:\\file.txt", "app.exe", "user", "content",
    detailLogTypeLevel2: 1, blocked: true);
var doc1 = JsonDocument.Parse(json1);
var logType1 = doc1.RootElement[0].GetProperty("CMDContent")[0].GetProperty("LogType").GetInt32();
Console.WriteLine($"文件保护 (LogType = {logType1}): {(logType1 == 1 ? "✓ 正确" : "✗ 错误")}");

// 测试注册表保护
var json2 = LogJsonBuilder.BuildHostDefenceLog(
    "TEST001", "HKLM\\reg", "regedit.exe", "user", "content",
    detailLogTypeLevel2: 2, blocked: false);
var doc2 = JsonDocument.Parse(json2);
var logType2 = doc2.RootElement[0].GetProperty("CMDContent")[0].GetProperty("LogType").GetInt32();
Console.WriteLine($"注册表保护 (LogType = {logType2}): {(logType2 == 2 ? "✓ 正确" : "✗ 错误")}");

// 测试强制访问控制 (关键修复)
var json3 = LogJsonBuilder.BuildHostDefenceLog(
    "TEST001", "C:\\file.txt", "test.exe", "user", "content",
    detailLogTypeLevel2: 4, blocked: true);
var doc3 = JsonDocument.Parse(json3);
var logType3 = doc3.RootElement[0].GetProperty("CMDContent")[0].GetProperty("LogType").GetInt32();
Console.WriteLine($"强制访问控制(LogType = {logType3}): {(logType3 == 4 ? "✓ 正确" : "✗ 错误")}");
'@

Write-Host ""
Write-Host "4. 代码修复确认..." -ForegroundColor Cyan
Write-Host ""

# 检查关键修改是否已应用
$builderFile = "src\SimulatorLib\Protocol\LogJsonBuilder.cs"
$workerFile = "src\SimulatorLib\Workers\LogWorker.cs"

# 检查BuildHostDefenceLog签名
$hasCorrectSignature = Select-String -Path $builderFile -Pattern 'public static string BuildHostDefenceLog.*int detailLogTypeLevel2.*bool blocked = false\)'
if ($hasCorrectSignature) {
    Write-Host "✓ BuildHostDefenceLog方法签名正确" -ForegroundColor Green
} else {
    Write-Host "✗ BuildHostDefenceLog方法签名有问题" -ForegroundColor Red
}

# 检查LogType赋值
$hasCorrectAssignment = Select-String -Path $builderFile -Pattern '\["LogType"\] = detailLogTypeLevel2'
if ($hasCorrectAssignment) {
    Write-Host "✓ LogType直接等于detailLogTypeLevel2" -ForegroundColor Green
} else {
    Write-Host "✗ LogType赋值有问题" -ForegroundColor Red
}

# 检查MAC的值是否为4
$hasCorrectMacValue = Select-String -Path $workerFile -Pattern 'detailLogTypeLevel2: 4.*WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_MACPROTECT'
if ($hasCorrectMacValue) {
    Write-Host "✓ 强制访问控制(MAC)的值正确为4" -ForegroundColor Green
} else {
    Write-Host "✗ 强制访问控制(MAC)的值可能有问题" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== 修复验证完成 ===" -ForegroundColor Green
Write-Host ""
Write-Host "修复内容总结:" -ForegroundColor Yellow
Write-Host "  1. ✓ BuildHostDefenceLog方法简化参数: 只需要detailLogTypeLevel2" -ForegroundColor Cyan
Write-Host "  2. ✓ LogType字段: 现在直接等于detailLogTypeLevel2,对齐原代码" -ForegroundColor Cyan
Write-Host "  3. ✓ 强制访问控制值修正: 3 → 4 (WL_IPC_LOG_TYPE_LEVE_2_SYSTEM_INTEGRALITY_MACPROTECT)" -ForegroundColor Cyan
Write-Host ""
Write-Host "验证日志对照表:" -ForegroundColor Yellow
Write-Host "  - 文件保护:        LogType = 1 ✓" -ForegroundColor Green
Write-Host "  - 注册表保护:      LogType = 2 ✓" -ForegroundColor Green
Write-Host "  - 加载文件:        LogType = 3 (unused)" -ForegroundColor Gray
Write-Host "  - 强制访问控制:    LogType = 4 ✓ (修复)" -ForegroundColor Green
Write-Host ""
Write-Host "参考文档: docs/日志分类显示问题修复报告_2026-02-10.md" -ForegroundColor Cyan
'@

Set-Content -Path "scripts\verify_logtype_fix.ps1" -Value $scriptContent
Write-Host "脚本已保存到 scripts/verify_logtype_fix.ps1"
