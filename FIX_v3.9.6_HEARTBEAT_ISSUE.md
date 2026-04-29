# v3.9.6 心跳上线问题修复说明

## 问题诊断

### 根本原因
C++子进程从`Clients.log`读取客户端数据时，**只提取了ClientId和IP，完全丢失了DeviceId和TcpPort信息**。

平台识别客户端需要DeviceId，导致所有心跳都被拒绝（返回cmdId=18 NOREGISTER）。

### 数据流对比

#### ❌ 修复前（v3.9.6）
```
C#注册 → DeviceId存储在Clients.log
  ↓
C++读取Clients.log → 只读取ClientId/IP（❌ 丢失DeviceId）
  ↓
C++构建心跳包 → DeviceId=0或缺失
  ↓
平台收到心跳 → 无法识别客户端 → 返回NOREGISTER(18)
  ↓
❌ 一个都上不去
```

#### ✅ 修复后（v3.9.6-fixed）
```
C#注册 → DeviceId存储在Clients.log
  ↓
C++读取Clients.log → 完整读取ClientId/IP/DeviceId/TcpPort
  ↓
C++构建心跳包 → 包含正确的DeviceId
  ↓
平台收到心跳 → 识别客户端 → 返回正常回包(1)
  ↓
✅ 全部上线
```

### Clients.log 数据格式
```json
{"ClientId":"WLClient_200","IP":"6.6.6.6","RegisteredAt":"2026-04-26T10:30:00Z","Status":"Registered","DeviceId":123456,"TcpPort":4575}
{"ClientId":"WLClient_201","IP":"6.6.6.7","RegisteredAt":"2026-04-26T10:30:01Z","Status":"Registered","DeviceId":123457,"TcpPort":4575}
```

---

## 修复内容

### 1. client.h - 添加DeviceId和TcpPort字段
```cpp
class client {
private:
    // 原有字段...
    
    // v3.9.6: 子进程架构需要的字段（从Clients.log读取）
    UINT m_deviceId;    // 平台分配的设备ID（注册时获得）
    int m_tcpPort;      // TCP心跳端口（注册时获得）

public:
    // v3.9.6: 子进程架构新增方法
    void    Client_SetComputerIDDirect(const CString& computerID);
    UINT    GetDeviceId() const;
    void    SetDeviceId(UINT deviceId);
    int     GetTcpPort() const;
    void    SetTcpPort(int port);
};
```

### 2. client.cpp - 实现新增方法
```cpp
// 构造函数初始化
client::client(__in CString inClientID, __in CString inClientIP) {
    // ... 原有初始化
    m_deviceId = 0;
    m_tcpPort = 0;
}

// 新增方法实现
void client::Client_SetComputerIDDirect(const CString& computerID) {
    m_csComputerID_PreClientIDSuf = computerID;
}

UINT client::GetDeviceId() const { return m_deviceId; }
void client::SetDeviceId(UINT deviceId) { m_deviceId = deviceId; }
int client::GetTcpPort() const { return m_tcpPort; }
void client::SetTcpPort(int port) { m_tcpPort = port; }
```

### 3. ConsoleMode.cpp - 完整解析Clients.log
```cpp
// 新增：解析数值字段的lambda
auto extractUInt = [&](const std::string& key) -> UINT {
    std::string search = "\"" + key + "\":";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return 0;
    pos += search.size();
    size_t end = line.find_first_of(",}", pos);
    if (end == std::string::npos) return 0;
    std::string numStr = line.substr(pos, end - pos);
    try {
        return std::stoul(numStr);
    } catch (...) {
        return 0;
    }
};

// 读取完整信息
std::string clientId = extractStr("ClientId");
std::string ip       = extractStr("IP");
UINT deviceId        = extractUInt("DeviceId");  // ✅ 新增
int tcpPort          = (int)extractUInt("TcpPort");  // ✅ 新增

// 设置到client对象
client newClient(csClientId, csIp);
newClient.Client_SetComputerIDDirect(csClientId);
newClient.SetDeviceId(deviceId);  // ✅ 新增
newClient.SetTcpPort(tcpPort);    // ✅ 新增
newClient.Client_SetRegistered(TRUE);

// 调试输出（前5个客户端）
if (g_vecAllClientObjects.size() <= 5) {
    std::cout << "INFO|Client[" << g_vecAllClientObjects.size() << "]: "
              << "ClientId=" << clientId 
              << " IP=" << ip 
              << " DeviceId=" << deviceId 
              << " TcpPort=" << tcpPort << std::endl;
}
```

---

## 编译和测试

### 1. 重新编译WLServerTest.exe
```bash
# 使用Visual Studio 2019
cd external/WLServerTest
msbuild WLServerTest.vcxproj /p:Configuration=Release /p:Platform=x64

# 或者在VS中打开WLServerTest.sln，选择Release配置，编译
```

### 2. 复制到发布目录
```bash
copy external\WLServerTest\x64\Release\WLServerTest.exe artifacts\SimulatorAppPublish\
```

### 3. 测试流程
```
1. 启动SimulatorApp.exe
2. 配置服务器地址（如：192.168.7.254）
3. 注册客户端（如：起始序号200，数量100）
   → 生成Clients.log，包含DeviceId
4. 关闭程序
5. 重新启动SimulatorApp.exe
6. 直接点击"开始心跳"（不重新注册）
   → C++子进程读取Clients.log
   → 应该看到：INFO|Client[1]: ClientId=WLClient_200 IP=6.6.6.6 DeviceId=123456 TcpPort=4575
   → 心跳应该全部上线（HBServerAck=100）
```

### 4. 验证输出
查看控制台输出，应该看到：
```
INFO|Loaded 100 clients from Clients.log (with DeviceId and TcpPort)
INFO|Client[1]: ClientId=WLClient_200 IP=6.6.6.6 DeviceId=123456 TcpPort=4575
INFO|Client[2]: ClientId=WLClient_201 IP=6.6.6.7 DeviceId=123457 TcpPort=4575
...
INFO|Starting heartbeat threads...
STATS|RegisteredTotal=100|HBSending=100|HBNotSending=0|HBServerAck=100|LogNotSending=50
```

---

## 预期效果

### 修复前
- 注册100个客户端 → Clients.log包含DeviceId
- 重启程序，直接开始心跳
- **结果**：HBSending=100，但HBServerAck=0（一个都上不去）
- **原因**：C++子进程没有读取DeviceId，心跳包中DeviceId=0

### 修复后
- 注册100个客户端 → Clients.log包含DeviceId
- 重启程序，直接开始心跳
- **结果**：HBSending=100，HBServerAck=100（全部上线）
- **原因**：C++子进程正确读取DeviceId，心跳包包含正确的DeviceId

---

## 与v3.9.5的对比

### v3.9.5（DLL方式）
- C#和C++共享内存
- 注册后DeviceId直接传递给C++ DLL
- 心跳包自动包含DeviceId
- ✅ 重启后直接心跳可以上线

### v3.9.6修复前（子进程方式）
- C#和C++进程隔离
- 通过Clients.log传递数据
- ❌ C++只读取ClientId/IP，丢失DeviceId
- ❌ 重启后直接心跳无法上线

### v3.9.6修复后（子进程方式）
- C#和C++进程隔离
- 通过Clients.log传递数据
- ✅ C++完整读取ClientId/IP/DeviceId/TcpPort
- ✅ 重启后直接心跳可以上线
- ✅ 保持进程隔离的稳定性优势

---

## 相关文件

### 修改的文件
1. `external/WLServerTest/client.h` - 添加DeviceId/TcpPort字段和方法声明
2. `external/WLServerTest/client.cpp` - 实现新增方法
3. `external/WLServerTest/ConsoleMode.cpp` - 完整解析Clients.log

### 需要重新编译
- `WLServerTest.exe` - C++子进程可执行文件

### 不需要修改
- `SimulatorApp.exe` - C#主程序（已经正确写入DeviceId到Clients.log）
- `Clients.log` - 数据格式正确，无需修改

---

## 故障排查

### 如果心跳还是上不去

1. **检查Clients.log格式**
   ```bash
   type Clients.log
   # 应该看到 "DeviceId":123456 这样的字段
   ```

2. **检查C++输出**
   ```
   # 应该看到：
   INFO|Client[1]: ClientId=WLClient_200 IP=6.6.6.6 DeviceId=123456 TcpPort=4575
   
   # 如果DeviceId=0，说明读取失败
   ```

3. **检查平台回包**
   - cmdId=1：正常心跳回包 → 上线成功
   - cmdId=17：需要拉取策略 → 上线成功
   - cmdId=18：未注册 → DeviceId错误或失效

4. **重新注册**
   如果DeviceId在平台上已失效：
   - 删除Clients.log
   - 重新注册
   - 再次测试心跳

---

## 总结

这次修复解决了v3.9.6子进程架构的核心问题：**数据传递不完整**。

通过在C++端添加DeviceId和TcpPort字段，并完整解析Clients.log，现在C++子进程可以像v3.9.5一样，使用已注册的持久化数据直接上线，同时保持进程隔离带来的稳定性优势。

修复后，v3.9.6应该能够：
- ✅ 注册后持久化DeviceId
- ✅ 重启后直接心跳上线
- ✅ 300客户端稳定运行（进程隔离，无GC干扰）
- ✅ 保留所有现有功能（白名单、HTTPS日志等）
