#pragma once
#include <arpa/inet.h>
#include <fcntl.h>
#include <net/if.h>  // for ifreq and interface names
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>


#define CORE_BUF_SIZE 67108864  //8M

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
  enum SocketType {
    SEND,  // 发送
    RECV   // 接收
  };

 public:
  Socket(int domain, int type, int protocol);

  ~Socket();

  int getFd() const;

  void setMaxSocketBufferSize(SocketType bufferType);

  void bindSocketToInterface(const char* interfaceName);

  struct sockaddr_in createSockAddr(const std::string& ip,
                                    const uint16_t& port);
  void configureSocket(const uint16_t& port, SocketType type);

 private:
  int _sockfd;
  //通过实际测试调整应用层缓存区和系统缓冲区大小到达最高效率，将缓冲区大小设置为应用数据流量的数倍来提升效率。
  unsigned long _coreRecvBufSize;
  unsigned long _coreSendBufSize;
  std::string _ifName;  //socket绑定网卡
                        //#define IFNAME "enp12s0f1"		//socket绑定网卡
};
