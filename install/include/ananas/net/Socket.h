
#ifndef BERT_SOCKET_H
#define BERT_SOCKET_H

#include <arpa/inet.h>
#include <sys/resource.h>
#include <string.h>
#include <string>
#include <memory>
#include <functional>

namespace ananas {

std::string ConvertIp(const char* ip);

struct SocketAddr { // SocketAddr地址, 内部维护sockaddr_in
    static const uint16_t kInvalidPort = -1;

    SocketAddr() {
        Clear();
    }

    SocketAddr(const sockaddr_in& addr) {   // 构造函数不同参数重载
        Init(addr);
    }
    SocketAddr(uint32_t netip, uint16_t netport) {
        Init(netip, netport);
    }

    SocketAddr(const char* ip, uint16_t hostport) {
        Init(ip, hostport);
    }
    SocketAddr(const std::string& ip, uint16_t hostport) {
        Init(ip.data(), hostport);
    }

    // ipv4 address format like "127.0.0.1:8000"
    SocketAddr(const std::string& ipport) {
        Init(ipport);
    }

    void Init(const sockaddr_in& addr) {
        memcpy(&addr_, &addr, sizeof(addr));    // sockaddr_in赋值
    }

    void Init(uint32_t netip, uint16_t netport) {
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = netip;
        addr_.sin_port   = netport;
    }

    // The htons function takes a 16-bit number in host byte order and returns a 16-bit number in network byte order used in TCP/IP networks
    void Init(const char* ip, uint16_t hostport) {
        std::string sip = ConvertIp(ip);
        addr_.sin_family = AF_INET;
        addr_.sin_addr.s_addr = ::inet_addr(sip.data());
        addr_.sin_port = htons(hostport);   // 将整数端口转为addr_.sin_port格式
    }

    // ip port format:  127.0.0.1:6379
    void Init(const std::string& ipport) {
        std::string::size_type p = ipport.find_first_of(':');
        std::string ip = ipport.substr(0, p);
        std::string port = ipport.substr(p + 1);

        Init(ip.c_str(), static_cast<uint16_t>(std::stoi(port)));
    }

    const sockaddr_in& GetAddr() const {
        return addr_;
    }

    // inet_ntop将&addr_.sin_addr转为ip地址
    std::string GetIP() const {
        char tmp[32];
        const char* res = inet_ntop(AF_INET, &addr_.sin_addr,
                                    tmp, (socklen_t)(sizeof tmp));
        return std::string(res);
    }
    // 转为port整数
    uint16_t GetPort() const {
        return ntohs(addr_.sin_port);
    }

    // 转为Address string format like 127.0.0.1:6379
    std::string ToString() const {
        char tmp[32];
        const char* res = inet_ntop(AF_INET, &addr_.sin_addr, tmp, (socklen_t)(sizeof tmp));

        return std::string(res) + ":" + std::to_string(ntohs(addr_.sin_port));
    }

    ///@brief IsValid
    bool IsValid() const {
        return addr_.sin_family != 0;
    }

    ///@brief addr Reset to zeros
    void Clear() {
        memset(&addr_, 0, sizeof addr_);
    }

    // operator==操作符
    inline friend bool operator== (const SocketAddr& a, const SocketAddr& b) {
        return a.addr_.sin_family      ==  b.addr_.sin_family &&
               a.addr_.sin_addr.s_addr ==  b.addr_.sin_addr.s_addr &&
               a.addr_.sin_port        ==  b.addr_.sin_port ;
    }

    inline friend bool operator!= (const SocketAddr& a, const SocketAddr& b) {
        return !(a == b);
    }

private:
/*
struct sockaddr_in
  {
    __SOCKADDR_COMMON (sin_);
    in_port_t sin_port;			
    struct in_addr sin_addr;		
    unsigned char sin_zero[sizeof (struct sockaddr) -
			   __SOCKADDR_COMMON_SIZE -
			   sizeof (in_port_t) -
			   sizeof (struct in_addr)];
  };
*/
    sockaddr_in  addr_;
};

extern const int kInvalid;  // 引用其他文件的全局变量

extern const int kTimeout;
extern const int kError;
extern const int kEof;

int CreateTCPSocket();  // 创建TCP socket
int CreateUDPSocket();  // 创建UDP socket
bool CreateSocketPair(int& readSock, int& writeSock);
void CloseSocket(int &sock);    // 关闭sockfd

void SetNonBlock(int sock, bool nonBlock = true);   // 设置sockfd为非阻塞
///@brief Set no delay for socket
void SetNodelay(int sock, bool enable = true);
void SetSndBuf(int sock, socklen_t size = 64 * 1024);
void SetRcvBuf(int sock, socklen_t size = 64 * 1024);
void SetReuseAddr(int sock);

bool GetLocalAddr(int sock, SocketAddr& );  // 获得本地ip地址
bool GetPeerAddr(int sock, SocketAddr& );   // 获得对方(socket连接方)的ip地址

///@brief Return the local ip, not 127.0.0.1
///
/// PAY ATTENTION: Only for linux, NOT work on mac os
in_addr_t GetLocalAddrInfo();

rlim_t GetMaxOpenFd();
bool SetMaxOpenFd(rlim_t maxfdPlus1);

} // namespace ananas


namespace std { // std namespace
template<>
struct hash<ananas::SocketAddr> {   // ananas::SocketAddr类型的hash函数
    typedef ananas::SocketAddr argument_type;
    typedef std::size_t result_type;

    result_type operator() (const argument_type& s) const noexcept {    // 三个变量sin_family, sin_port, sin_addr的哈希
        result_type h1 = std::hash<short>{} (s.GetAddr().sin_family);
        result_type h2 = std::hash<unsigned short> {}(s.GetAddr().sin_port);
        result_type h3 = std::hash<unsigned int> {}(s.GetAddr().sin_addr.s_addr);
        result_type tmp = h1 ^ (h2 << 1);
        return h3 ^ (tmp << 1);
    }
};
}

#endif

