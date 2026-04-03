// raw_packet_engine.cpp — RawPacketEngine.dll 实现
// 对齐 xb-ether-tester 的发包循环、FieldRule 和校验和更新逻辑。
// Windows 平台，使用 QueryPerformanceCounter 实现微秒级延时。

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

// Npcap SDK 头文件（需在 winsock2.h 之后）
#include <pcap.h>

#include "raw_packet_engine.h"

#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

// ═══════════════════════════════════════════════════════════════════
// 内部数据结构
// ═══════════════════════════════════════════════════════════════════

struct Stream {
    std::vector<uint8_t> data;
    std::string          name;
    std::vector<RPE_Rule> rules;
    uint32_t             checksumFlags;
    bool                 enabled;
    int                  id;
};

// ═══════════════════════════════════════════════════════════════════
// 全局状态
// ═══════════════════════════════════════════════════════════════════

static pcap_if_t*          g_alldevs       = nullptr;
static int                 g_adapter_count = 0;
static char                g_cur_dev_name[512] = {0};
static pcap_t*             g_pcap_handle   = nullptr;
static std::mutex          g_mutex;

static std::vector<Stream> g_streams;
static int                 g_next_stream_id = 0;

static std::atomic<bool>   g_running{false};
static std::thread         g_send_thread;

// 速率配置
static int32_t  g_speed_type  = RPE_SPEED_PPS;
static int64_t  g_interval_us = 1000;   // 微秒间隔（PPS 模式换算后存这里）
static int32_t  g_snd_mode    = RPE_MODE_CONTINUOUS;
static int64_t  g_snd_count   = 0;

// 统计（原子变量，线程安全）
static std::atomic<uint64_t> g_send_total{0};
static std::atomic<uint64_t> g_send_bytes{0};
static std::atomic<uint64_t> g_send_fail{0};

// ═══════════════════════════════════════════════════════════════════
// 任务 1.3 — 适配器枚举与选择
// ═══════════════════════════════════════════════════════════════════

extern "C" __declspec(dllexport)
int32_t RPE_Init()
{
    try {
        char errbuf[PCAP_ERRBUF_SIZE] = {0};

        // 释放上次枚举结果
        if (g_alldevs) {
            pcap_freealldevs(g_alldevs);
            g_alldevs = nullptr;
        }
        g_adapter_count = 0;

        // 使用 future + timeout 防止 pcap_findalldevs_ex 无响应导致程序挂起
        auto future = std::async(std::launch::async, [&errbuf]() -> int {
            return pcap_findalldevs_ex(
                const_cast<char*>(PCAP_SRC_IF_STRING),
                nullptr,
                &g_alldevs,
                errbuf);
        });

        // 等待最多 3 秒
        if (future.wait_for(std::chrono::seconds(3)) == std::future_status::timeout)
        {
            // 超时：Npcap 可能未安装或驱动未加载
            return -2;  // -2 表示超时（区别于 -1 的一般错误）
        }

        int result = future.get();
        if (result == -1)
        {
            // pcap_findalldevs_ex 失败（Npcap 未安装或权限不足）
            return -1;
        }

        // 统计适配器数量
        for (pcap_if_t* d = g_alldevs; d != nullptr; d = d->next)
            g_adapter_count++;

        return 0;
    }
    catch (...) { return -1; }
}

extern "C" __declspec(dllexport)
void RPE_Cleanup()
{
    try {
        RPE_Stop();

        std::lock_guard<std::mutex> lk(g_mutex);

        if (g_pcap_handle) {
            pcap_close(g_pcap_handle);
            g_pcap_handle = nullptr;
        }
        if (g_alldevs) {
            pcap_freealldevs(g_alldevs);
            g_alldevs = nullptr;
        }
        g_adapter_count = 0;
        g_streams.clear();
        g_next_stream_id = 0;
    }
    catch (...) {}
}

extern "C" __declspec(dllexport)
int32_t RPE_GetAdapterCount()
{
    return g_adapter_count;
}

extern "C" __declspec(dllexport)
int32_t RPE_GetAdapterInfo(
    int32_t index,
    char*   name,    int32_t nameLen,
    char*   ipv4,    int32_t ipv4Len)
{
    try {
        if (!g_alldevs || index < 0 || index >= g_adapter_count)
            return -1;

        pcap_if_t* d = g_alldevs;
        for (int i = 0; i < index; i++) {
            d = d->next;
            if (!d) return -1;
        }

        // 复制 pcap 设备名
        if (name && nameLen > 0) {
            strncpy_s(name, nameLen, d->name ? d->name : "", _TRUNCATE);
        }

        // 查找 IPv4 地址
        if (ipv4 && ipv4Len > 0) {
            ipv4[0] = '\0';
            for (pcap_addr_t* a = d->addresses; a != nullptr; a = a->next) {
                if (a->addr && a->addr->sa_family == AF_INET) {
                    struct sockaddr_in* sin =
                        reinterpret_cast<struct sockaddr_in*>(a->addr);
                    // inet_ntop 转换为点分十进制
                    inet_ntop(AF_INET, &sin->sin_addr, ipv4, ipv4Len);
                    break;
                }
            }
        }

        return 0;
    }
    catch (...) { return -1; }
}

extern "C" __declspec(dllexport)
int32_t RPE_SelectAdapter(int32_t index)
{
    try {
        if (!g_alldevs || index < 0 || index >= g_adapter_count)
            return -1;

        pcap_if_t* d = g_alldevs;
        for (int i = 0; i < index; i++) {
            d = d->next;
            if (!d) return -1;
        }

        std::lock_guard<std::mutex> lk(g_mutex);
        strncpy_s(g_cur_dev_name, sizeof(g_cur_dev_name),
                  d->name ? d->name : "", _TRUNCATE);
        return 0;
    }
    catch (...) { return -1; }
}

// ═══════════════════════════════════════════════════════════════════
// 任务 1.5 — FieldRule 引擎 & 校验和更新（内部实现，先声明供发包循环使用）
// ═══════════════════════════════════════════════════════════════════

// ── 网络协议头结构（紧凑布局）────────────────────────────────────────
#pragma pack(push, 1)

struct IpHdr {
    uint8_t  ihl_ver;   // version(4) | ihl(4)
    uint8_t  tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
};

struct IcmpHdr {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint32_t rest;
};

struct TcpHdr {
    uint16_t source;
    uint16_t dest;
    uint32_t seq;
    uint32_t ack_seq;
    uint16_t flags_doff;  // doff(4) | res(4) | flags(8)
    uint16_t window;
    uint16_t check;
    uint16_t urg_ptr;
};

struct UdpHdr {
    uint16_t source;
    uint16_t dest;
    uint16_t len;
    uint16_t check;
};

struct TcpUdpPseudoHdr {
    uint32_t saddr;
    uint32_t daddr;
    uint8_t  bz;
    uint8_t  protocol;
    uint16_t len;
};

#pragma pack(pop)

#define IPPROTO_ICMP_  1
#define IPPROTO_IGMP_  2
#define IPPROTO_TCP_   6
#define IPPROTO_UDP_  17
#define ETH_P_IP_   0x0800
#define ETH_P_VLAN_ 0x8100
#define ETH_HDR_LEN 14

// ── 以太网头之后的 IP 头偏移（跳过 VLAN tag）────────────────────────
static int eth_hdr_len(const uint8_t* pkt, int pkt_len)
{
    if (pkt_len < ETH_HDR_LEN) return ETH_HDR_LEN;
    uint16_t etype = (uint16_t)((pkt[12] << 8) | pkt[13]);
    int off = ETH_HDR_LEN;
    while (etype == ETH_P_VLAN_ && off + 4 <= pkt_len) {
        off += 4;
        etype = (uint16_t)((pkt[off - 2] << 8) | pkt[off - 1]);
    }
    return off;
}

// ── Internet 校验和（对齐 xb-ether-tester csum）──────────────────────
static uint16_t csum(const uint8_t* addr, int count)
{
    uint32_t sum = 0;
    while (count > 1) {
        sum += (uint16_t)((addr[0] << 8) | addr[1]);
        count -= 2;
        addr  += 2;
    }
    if (count > 0)
        sum += (uint16_t)(addr[0] << 8);

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return htons((uint16_t)(~sum));
}

// ── IP 校验和更新（对齐 ip_update_check）────────────────────────────
static void ip_update_check(IpHdr* iph)
{
    int ihl = (iph->ihl_ver & 0x0f) * 4;
    iph->check = 0;
    iph->check = csum(reinterpret_cast<const uint8_t*>(iph), ihl);
}

// ── TCP/UDP 校验和（对齐 tcp_udp_checksum）──────────────────────────
static uint16_t tcp_udp_checksum(IpHdr* iph)
{
    int ihl      = (iph->ihl_ver & 0x0f) * 4;
    int tot_len  = ntohs(iph->tot_len);
    int data_len = tot_len - ihl;

    uint8_t* ip_data = reinterpret_cast<uint8_t*>(iph) + ihl;

    TcpUdpPseudoHdr pseudo;
    pseudo.saddr    = iph->saddr;
    pseudo.daddr    = iph->daddr;
    pseudo.bz       = 0;
    pseudo.protocol = iph->protocol;
    pseudo.len      = htons((uint16_t)data_len);

    // 暂存并清零校验和字段
    uint16_t ori_sum = 0;
    if (iph->protocol == IPPROTO_TCP_) {
        TcpHdr* tcp = reinterpret_cast<TcpHdr*>(ip_data);
        ori_sum = tcp->check;
        tcp->check = 0;
    } else {
        UdpHdr* udp = reinterpret_cast<UdpHdr*>(ip_data);
        ori_sum = udp->check;
        udp->check = 0;
    }

    // 计算：伪头部 + 数据
    uint32_t sum = 0;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&pseudo);
    int cnt = (int)sizeof(pseudo);
    while (cnt > 1) {
        sum += (uint16_t)((p[0] << 8) | p[1]);
        cnt -= 2; p += 2;
    }
    p = ip_data; cnt = data_len;
    while (cnt > 1) {
        sum += (uint16_t)((p[0] << 8) | p[1]);
        cnt -= 2; p += 2;
    }
    if (cnt > 0)
        sum += (uint16_t)(p[0] << 8);

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    uint16_t result = htons((uint16_t)(~sum));

    // 恢复原始校验和（csum 函数不修改数据，这里是计算函数）
    if (iph->protocol == IPPROTO_TCP_) {
        TcpHdr* tcp = reinterpret_cast<TcpHdr*>(ip_data);
        tcp->check = ori_sum;
    } else {
        UdpHdr* udp = reinterpret_cast<UdpHdr*>(ip_data);
        udp->check = ori_sum;
    }

    return result;
}

static void tcp_update_check(IpHdr* iph)
{
    int ihl = (iph->ihl_ver & 0x0f) * 4;
    TcpHdr* tcp = reinterpret_cast<TcpHdr*>(
        reinterpret_cast<uint8_t*>(iph) + ihl);
    tcp->check = tcp_udp_checksum(iph);
}

static void udp_update_check(IpHdr* iph)
{
    int ihl = (iph->ihl_ver & 0x0f) * 4;
    UdpHdr* udp = reinterpret_cast<UdpHdr*>(
        reinterpret_cast<uint8_t*>(iph) + ihl);
    udp->check = tcp_udp_checksum(iph);
}

// ── ICMP/IGMP 校验和（对齐 icmp_igmp_update_check）──────────────────
static void icmp_igmp_update_check(IpHdr* iph)
{
    int ihl      = (iph->ihl_ver & 0x0f) * 4;
    int data_len = ntohs(iph->tot_len) - ihl;
    IcmpHdr* icmp = reinterpret_cast<IcmpHdr*>(
        reinterpret_cast<uint8_t*>(iph) + ihl);
    icmp->checksum = 0;
    icmp->checksum = csum(reinterpret_cast<const uint8_t*>(icmp), data_len);
}

// ── 校验和更新入口（对齐 update_check_sum_v4）────────────────────────
static void update_checksums(Stream& s)
{
    if (s.data.size() < (size_t)(ETH_HDR_LEN + 20))
        return;

    int eth_len = eth_hdr_len(s.data.data(), (int)s.data.size());
    uint16_t etype = (uint16_t)((s.data[eth_len - 2] << 8) | s.data[eth_len - 1]);
    if (etype != ETH_P_IP_)
        return;  // 仅处理 IPv4（IPv6 校验和暂不支持）

    IpHdr* iph = reinterpret_cast<IpHdr*>(s.data.data() + eth_len);

    if (s.checksumFlags & RPE_CKSUM_IP)
        ip_update_check(iph);

    // 分片报文不更新传输层校验和
    if (ntohs(iph->frag_off) & 0x1fff)
        return;

    if (iph->protocol == IPPROTO_ICMP_ && (s.checksumFlags & RPE_CKSUM_ICMP))
        icmp_igmp_update_check(iph);
    else if (iph->protocol == IPPROTO_IGMP_ && (s.checksumFlags & RPE_CKSUM_IGMP))
        icmp_igmp_update_check(iph);
    else if (iph->protocol == IPPROTO_TCP_ && (s.checksumFlags & RPE_CKSUM_TCP))
        tcp_update_check(iph);
    else if (iph->protocol == IPPROTO_UDP_ && (s.checksumFlags & RPE_CKSUM_UDP))
        udp_update_check(iph);
}

// ── FieldRule 应用（对齐 xb-ether-tester rule_fileds_update）─────────
// 读取字段当前值（网络字节序），加 step_size，超过 max_value 回绕到 base_value，写回。
static void rule_apply_and_update(Stream& s)
{
    for (auto& rule : s.rules) {
        if (!rule.valid) continue;

        uint16_t off   = rule.offset;
        uint8_t  width = rule.width;

        if (off + width > (uint16_t)s.data.size()) continue;
        if (width != 1 && width != 2 && width != 4) continue;

        uint8_t* field = s.data.data() + off;

        // 读取当前值（网络字节序 → 主机序）
        uint32_t cur = 0;
        if (width == 1) {
            cur = field[0];
        } else if (width == 2) {
            cur = (uint32_t)ntohs(*reinterpret_cast<uint16_t*>(field));
        } else {
            cur = ntohl(*reinterpret_cast<uint32_t*>(field));
        }

        // 读取 base_value 和 max_value（小端存储，取 width 字节）
        uint32_t base_val = 0, max_val = 0;
        for (int b = (int)width - 1; b >= 0; b--) {
            base_val = (base_val << 8) | rule.base_value[b];
            max_val  = (max_val  << 8) | rule.max_value[b];
        }

        // 递增
        cur += rule.step_size;

        // 超过 max_value 回绕到 base_value
        if (cur > max_val)
            cur = base_val;

        // 写回（主机序 → 网络字节序）
        if (width == 1) {
            field[0] = (uint8_t)cur;
        } else if (width == 2) {
            *reinterpret_cast<uint16_t*>(field) = htons((uint16_t)cur);
        } else {
            *reinterpret_cast<uint32_t*>(field) = htonl(cur);
        }
    }

    // 更新校验和
    update_checksums(s);
}

// ═══════════════════════════════════════════════════════════════════
// 任务 1.4 — 发包循环核心
// ═══════════════════════════════════════════════════════════════════

// ── 高精度微秒延时（QueryPerformanceCounter，不用 Sleep）─────────────
static void spin_wait_us(int64_t us)
{
    if (us <= 0) return;

    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    int64_t ticks_needed = us * freq.QuadPart / 1000000LL;
    do {
        QueryPerformanceCounter(&now);
    } while ((now.QuadPart - start.QuadPart) < ticks_needed);
}

// ── 发包线程主循环 ────────────────────────────────────────────────────
static void send_loop()
{
    int stream_idx = -1;
    int64_t send_times_cnt = 0;
    int consecutive_fail = 0;

    LARGE_INTEGER freq, last_send_time, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last_send_time);

    while (g_running.load(std::memory_order_relaxed)) {

        // 速率控制（非 Fastest 模式）
        if (g_speed_type != RPE_SPEED_FASTEST) {
            // 等待到下一个发包时刻
            int64_t interval_ticks = g_interval_us * freq.QuadPart / 1000000LL;
            do {
                QueryPerformanceCounter(&now);
                if (!g_running.load(std::memory_order_relaxed)) goto EXIT;
            } while ((now.QuadPart - last_send_time.QuadPart) < interval_ticks);
            last_send_time = now;
        }

        // Round-Robin 选取下一条 enabled stream
        {
            std::lock_guard<std::mutex> lk(g_mutex);

            if (g_streams.empty()) {
                // 没有 stream，短暂等待
                spin_wait_us(1000);
                continue;
            }

            // 找下一条 enabled stream
            int n = (int)g_streams.size();
            int found = -1;
            for (int i = 1; i <= n; i++) {
                int idx = (stream_idx + i) % n;
                if (g_streams[idx].enabled) {
                    found = idx;
                    break;
                }
            }

            if (found < 0) {
                // 全部禁用，等待
                spin_wait_us(1000);
                continue;
            }

            stream_idx = found;
            Stream& s = g_streams[stream_idx];

            // 发包
            int ret = pcap_sendpacket(
                g_pcap_handle,
                s.data.data(),
                (int)s.data.size());

            if (ret != 0) {
                g_send_fail.fetch_add(1, std::memory_order_relaxed);
                consecutive_fail++;
                if (consecutive_fail >= 100) {
                    // 连续 100 次失败，退出循环
                    g_running.store(false, std::memory_order_relaxed);
                    goto EXIT;
                }
            } else {
                g_send_total.fetch_add(1, std::memory_order_relaxed);
                g_send_bytes.fetch_add(s.data.size(), std::memory_order_relaxed);
                consecutive_fail = 0;

                // 应用 FieldRule 并更新校验和
                rule_apply_and_update(s);
            }

            send_times_cnt++;

            // Burst 模式：达到目标次数后退出
            if (g_snd_mode == RPE_MODE_BURST && g_snd_count > 0
                && send_times_cnt >= g_snd_count)
            {
                g_running.store(false, std::memory_order_relaxed);
                goto EXIT;
            }
        }
    }

EXIT:
    return;
}

// ── Stream 管理 API ──────────────────────────────────────────────────

extern "C" __declspec(dllexport)
int32_t RPE_AddStream(
    const uint8_t* data,
    int32_t        len,
    const char*    name,
    const RPE_Rule* rules,
    int32_t        ruleCount,
    uint32_t       checksumFlags)
{
    try {
        if (!data || len <= 0) return -1;

        std::lock_guard<std::mutex> lk(g_mutex);

        Stream s;
        s.data.assign(data, data + len);
        s.name = name ? name : "";
        s.checksumFlags = checksumFlags;
        s.enabled = true;
        s.id = g_next_stream_id++;

        if (rules && ruleCount > 0) {
            s.rules.assign(rules, rules + ruleCount);
        }

        g_streams.push_back(std::move(s));
        return g_streams.back().id;
    }
    catch (...) { return -1; }
}

extern "C" __declspec(dllexport)
void RPE_ClearStreams()
{
    try {
        std::lock_guard<std::mutex> lk(g_mutex);
        g_streams.clear();
        g_next_stream_id = 0;
    }
    catch (...) {}
}

extern "C" __declspec(dllexport)
void RPE_SetStreamEnabled(int32_t streamId, int32_t enabled)
{
    try {
        std::lock_guard<std::mutex> lk(g_mutex);
        for (auto& s : g_streams) {
            if (s.id == streamId) {
                s.enabled = (enabled != 0);
                break;
            }
        }
    }
    catch (...) {}
}

extern "C" __declspec(dllexport)
void RPE_SetRateConfig(
    int32_t speedType,
    int64_t speedValue,
    int32_t sndMode,
    int64_t sndCount)
{
    try {
        g_speed_type = speedType;
        g_snd_mode   = sndMode;
        g_snd_count  = sndCount;

        if (speedType == RPE_SPEED_PPS) {
            // PPS 模式：换算微秒间隔，最小 1
            g_interval_us = (speedValue > 0)
                ? (1000000LL / speedValue)
                : 1000000LL;
            if (g_interval_us < 1) g_interval_us = 1;
        } else if (speedType == RPE_SPEED_INTERVAL) {
            g_interval_us = (speedValue > 0) ? speedValue : 1;
        } else {
            // Fastest：interval 无意义
            g_interval_us = 0;
        }
    }
    catch (...) {}
}

extern "C" __declspec(dllexport)
int32_t RPE_Start()
{
    try {
        if (g_cur_dev_name[0] == '\0') return -1;
        if (g_running.load()) return -1;

        // 打开 pcap 句柄
        char errbuf[PCAP_ERRBUF_SIZE] = {0};
        pcap_t* handle = pcap_open_live(
            g_cur_dev_name,
            65536,   // snaplen
            1,       // promiscuous
            1,       // read timeout (ms)
            errbuf);

        if (!handle) return -1;

        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (g_pcap_handle) {
                pcap_close(g_pcap_handle);
            }
            g_pcap_handle = handle;
        }

        // 重置统计
        g_send_total.store(0);
        g_send_bytes.store(0);
        g_send_fail.store(0);

        g_running.store(true);
        g_send_thread = std::thread(send_loop);
        return 0;
    }
    catch (...) { return -1; }
}

extern "C" __declspec(dllexport)
void RPE_Stop()
{
    try {
        g_running.store(false);

        if (g_send_thread.joinable()) {
            // 最多等待 1000ms
            auto future = std::async(std::launch::async, [&]() {
                g_send_thread.join();
            });
            if (future.wait_for(std::chrono::milliseconds(1000))
                == std::future_status::timeout)
            {
                // 超时：线程仍在运行，detach 避免析构崩溃
                g_send_thread.detach();
            }
        }

        std::lock_guard<std::mutex> lk(g_mutex);
        if (g_pcap_handle) {
            pcap_close(g_pcap_handle);
            g_pcap_handle = nullptr;
        }
    }
    catch (...) {}
}

extern "C" __declspec(dllexport)
void RPE_GetStats(
    uint64_t* sendTotal,
    uint64_t* sendBytes,
    uint64_t* sendFail)
{
    try {
        if (sendTotal) *sendTotal = g_send_total.load(std::memory_order_relaxed);
        if (sendBytes) *sendBytes = g_send_bytes.load(std::memory_order_relaxed);
        if (sendFail)  *sendFail  = g_send_fail.load(std::memory_order_relaxed);
    }
    catch (...) {}
}
