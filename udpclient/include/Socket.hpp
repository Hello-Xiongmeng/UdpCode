#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>  // for ifreq and interface names
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include <atomic>

// 假设的包头结构
//结构体会填充
/*
+----------------+------------+------------+------------+
|   timeStamp    | sequence   | total      | Padding    |
|   8 bytes      | 2 bytes    | 2 bytes    | 4 bytes    |
+----------------+------------+------------+------------+
|      8         |     2      |     2      |     4      |
+----------------+------------+------------+------------+
*/
struct Header {
  uint64_t timeStamp = 0;  // 时间戳 (组序号)
  uint16_t sequence = 0;   // 分片序号
  uint16_t total = 0;      // 总分片数
};

constexpr int MTU = 9000;
constexpr int IP_HEADER_SIZE = 20;
constexpr int UDP_HEADER_SIZE = 8;
constexpr int TCP_HEADER_SIZE = 20;

constexpr int PACKET_HEADER_SIZE = sizeof(Header);
constexpr int UDP_PAYLOAD_SIZE = MTU - IP_HEADER_SIZE - UDP_HEADER_SIZE -
                                 PACKET_HEADER_SIZE;  //1468，真实数据的大小
constexpr int TCP_PAYLOAD_SIZE = MTU - IP_HEADER_SIZE - TCP_HEADER_SIZE -
                                 PACKET_HEADER_SIZE;  //1468，真实数据的大小

constexpr int UDP_APP_BUF_SIZE = MTU - IP_HEADER_SIZE - UDP_HEADER_SIZE;
constexpr int TCP_APP_BUF_SIZE = MTU - IP_HEADER_SIZE - TCP_HEADER_SIZE;

#define CORE_BUF_SIZE 67108864

/**
 * @brief  定义一个类继承runtime_error，理论上不可以通过读取代码来检测到的异常
 * explicit修饰构造函数避免隐式调用，试抛出异常和捕获异常类型一直
 * 在 Server 类中处理可恢复的异常，在 main 函数中处理不可恢复的和未捕获的异常
 */
class SocketException : public std::runtime_error {
 public:
  explicit SocketException(const std::string& msg) : std::runtime_error(msg) {}
  //调用基类 std::runtime_error 的构造函数，msg传给基类，把msg存储，使用what（）打印
};
//作用域解析运算符（::）
// 套接字创建失败异常
class SocketCreationException : public SocketException {
 public:
  explicit SocketCreationException(const std::string& details)
      : SocketException("Socket creation failed: " + details) {}
};

class Socket {

 public:
  enum ProtocolType {
    TCP,  // TCP 连接
    UDP   // UDP 连接
  };
  enum SocketMode {
    SEND,  // 发送
    RECV   // 接收
  };

 public:
  Socket(int domain, ProtocolType protocolType, SocketMode socketMode,
         int protocol);

  ~Socket();

  int getFd() const;
  int getAppBufSize() const;
  void setMaxSocketBufferSize();

  void bindSocketToInterface(const char* interfaceName);

  struct sockaddr_in createSockAddr(const std::string& ip,
                                    const uint16_t& port);
  int configureSocket(const uint16_t& port, std::atomic<bool>& runFlag);

 private:
  int _sockfd;
  int _appBufSize;
  //通过实际测试调整应用层缓存区和系统缓冲区大小到达最高效率，将缓冲区大小设置为应用数据流量的数倍来提升效率。
  unsigned long _coreRecvBufSize;
  unsigned long _coreSendBufSize;
  std::string _ifName;  //socket绑定网卡

  ProtocolType _protocolType;
  SocketMode _socketMode;
};
