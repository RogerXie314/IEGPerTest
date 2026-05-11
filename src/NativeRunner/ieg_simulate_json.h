#pragma once
#include <string>

// Option B: 直接从老工具 SimulateJson.cpp / WLJsonParse.cpp 提取，零手写 JSON 字段

// 对应 WLSimulateJson::ThreatLog_SimulateJson_File
std::string IEG_SimulateJson_File(const std::wstring& computerID, bool bHit);

// 对应 WLSimulateJson::ThreatLog_SimulateJson_ProcStart
std::string IEG_SimulateJson_ProcStart(const std::wstring& computerID, bool bHit);

// 对应 WLSimulateJson::ThreatLog_SimulateJson_Reg
std::string IEG_SimulateJson_Reg(const std::wstring& computerID, bool bHit);
