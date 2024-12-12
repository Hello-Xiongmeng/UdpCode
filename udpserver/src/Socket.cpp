#include "Socket.hpp"
#include <netinet/in.h>
#include <string.h>
#include "ChangePrint.hpp"


Socket::Socket(int domain, ProtocolType protocolType, SocketMode socketMode,
               int protocol)
    : 
     _coreRecvBufSize(CORE_BUF_SIZE),
      _coreSendBufSize(CORE_BUF_SIZE),
      _protocolType(protocolType),
      _socketMode(socketMode),
      _ifName("eth0")
      {
  int type = (_protocolType == TCP) ? SOCK_STREAM : SOCK_DGRAM;
  _appBufSize = (_protocolType == TCP) ? TCP_APP_BUF_SIZE : UDP_APP_BUF_SIZE;

  _sockfd = socket(domain, type, protocol);
  if (_sockfd < 0) {
    throw SocketException("Failed to create socket: " +
                          std::string(strerror(errno)));
  }
  std::cout << "Socket created with fd: " << _sockfd << std::endl;
}

/**
   * @brief  Destroy the Socket object
   */
Socket::~Socket() {
  // 判断是否有打开的套接字
  if (_sockfd != -1) {
    // 针对TCP协议，可能有更多特定的清理工作
    if (_protocolType == TCP) {
      std::cout << "Closing TCP socket with fd: " << _sockfd << std::endl;

      // 如果是接收模式，先关闭接收端
      if (_socketMode == RECV) {
        // 对接收端进行 shutdown
        if (shutdown(_sockfd, SHUT_RD) < 0) {
          std::cerr << "Failed to shutdown the receive side of the socket: "
                    << strerror(errno) << std::endl;
        } else {
          std::cout << "TCP socket receive side shutdown successfully."
                    << std::endl;
        }
      }

      // 如果是发送模式，先关闭发送端
      if (_socketMode == SEND) {
        // 对发送端进行 shutdown
        if (shutdown(_sockfd, SHUT_WR) < 0) {
          std::cerr << "Failed to shutdown the send side of the socket: "
                    << strerror(errno) << std::endl;
        } else {
          std::cout << "TCP socket send side shutdown successfully."
                    << std::endl;
        }
      }

      // 完全关闭连接（发送和接收）
      if (shutdown(_sockfd, SHUT_RDWR) < 0) {
        std::cerr << "Failed to shutdown the socket completely: "
                  << strerror(errno) << std::endl;
      } else {
        std::cout << "TCP socket completely shutdown successfully."
                  << std::endl;
      }
    }

    else if (_protocolType == UDP) {
      // 对于UDP协议，可以直接关闭套接字
      std::cout << "Closing UDP socket with fd: " << _sockfd << std::endl;
    }

    // 关闭套接字
    close(_sockfd);
    std::cout << "Socket closed with fd: " << _sockfd << std::endl;
  }
}
/**
   * @brief  Get the Fd object
   * @return int: 
   */
int Socket::getFd() const {
  return _sockfd;
}  //外部代码只能读取 sockfd 的值，而不能直接修改它

int Socket::getAppBufSize() const {
  return _appBufSize;
}  
/**
   * @brief  设置内核缓冲区
   * @param [入参] bufferType: 
   * @return int: 
   */
void Socket::setMaxSocketBufferSize() {
  int bufferSizeBefore = 0, bufferSizeAfter = 0;  // 使用正确的数据类型
  socklen_t len = sizeof(bufferSizeBefore);

  // 根据缓冲区类型选择选项
  int option = (_socketMode == SEND) ? SO_SNDBUF : SO_RCVBUF;
  int maxBufferSize =
      (_socketMode == SEND) ? _coreSendBufSize : _coreRecvBufSize;

  // 获取当前的缓冲区大小
  if (getsockopt(_sockfd, SOL_SOCKET, option, &bufferSizeBefore, &len) < 0) {
    throw SocketException("Failed to get socket buffer size: " +
                          std::string(strerror(errno)));
  }

  // 输出系统默认的缓冲区大小
  std::cout << ((_socketMode == SEND) ? "Default send buffer size: "
                                      : "Default receive buffer size: ")
            << bufferSizeBefore << " bytes" << std::endl;

  // 尝试设置最大缓冲区大小
  if (setsockopt(_sockfd, SOL_SOCKET, option, &maxBufferSize,
                 sizeof(maxBufferSize)) < 0) {
    throw SocketException("Failed to set socket buffer size: " +
                          std::string(strerror(errno)));
  }

  // 再次获取设置后的缓冲区大小
  if (getsockopt(_sockfd, SOL_SOCKET, option, &bufferSizeAfter, &len) < 0) {
    throw SocketException("Failed to get socket buffer size after setting: " +
                          std::string(strerror(errno)));
  }

  // 输出设置后的缓冲区大小
  std::cout << ((_socketMode == SEND) ? "Updated send buffer size: "
                                      : "Updated receive buffer size: ")
            << bufferSizeAfter << " bytes" << std::endl;

  // 检查是否设置成功
  if (bufferSizeAfter < maxBufferSize * 2) {

    printWithColor("red", "Warning: Unable to set buffer size to maximum. ",
                   "Current size: ", bufferSizeAfter,
                   " bytes, requested: ", maxBufferSize, " bytes.");
  } else {
    std::cout << "Buffer size successfully set to: " << bufferSizeAfter
              << " bytes." << std::endl;
  }
}
/**
   * @brief  
   * @param [入参] interfaceName: 
   * @return int: 
   */
void Socket::bindSocketToInterface(const char* interfaceName) {
  struct ifreq ifr;
  strncpy(ifr.ifr_ifrn.ifrn_name, interfaceName, IFNAMSIZ);

  // 使用SO_BINDTODEVICE将套接字绑定到指定的网络接口
  if (setsockopt(_sockfd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0) {
    throw SocketException("Failed to bind socket to interface " +
                          std::string(interfaceName) + ": " + strerror(errno));
  }
  std::cout << "Socket successfully bound to interface: " << interfaceName
            << std::endl;
}
/**
 * @brief  创建一个网络通信通用结构体
 * @param [入参] ip: 输入通信ip
 * @param [入参] port: 输入通信端口
 * @return struct sockaddr_in: 返回结构体变量
 * @note  创建客户端的网络结构体信息时，string& ip输入客户端的ip地址，
 * 创建服务端网络结构体信息时，输入0.0.0.0监听所有可用的网卡IP
 */
struct sockaddr_in Socket::createSockAddr(const std::string& ip,
                                          const uint16_t& port) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));  // 将结构体置零
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);  // 转为网络字节序
  if (ip == "0.0.0.0") {
    addr.sin_addr.s_addr = INADDR_ANY;
  } else {
    addr.sin_addr.s_addr = inet_addr(ip.c_str());  // 将IP地址转换为网络字节序
  }
  return addr;
}

/**
 * @brief  创建网络通信udp套接字
 * @param [入参] recvPort: 只是用作输出提示信息，recvPort并没有实际应用
 * @return int: 返回文件描述符fd是file descriptor缩写
 */
int Socket::configureSocket(const uint16_t& port, std::atomic<bool>& runFlag) {
  // 将套接字绑定到eth0接口
  bindSocketToInterface(_ifName.c_str());
  setMaxSocketBufferSize();
  int newRecvSocket;
  // 设置套接字为非阻塞模式,读取缓冲区无数据，返回一个错误，写入缓冲区已满返回错误，写入时如果已满会丢失数据，要尝试重新写入
  int flags = fcntl(_sockfd, F_GETFL, 0);  //get函数

  if (flags < 0) {
    throw SocketException("Failed fcntl()  " + std::string(strerror(errno)));
  }

  fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);  //set函数
  auto localAddr = createSockAddr("0.0.0.0", port);

  //允许重复使用端口,否则操作系统会不允许你在 TIME_WAIT 时间之内使用该端口
  int reuse = 1;
  if (setsockopt(_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) <
      0) {
    std::cerr << "Failed to set SO_REUSEADDR: " << strerror(errno) << std::endl;
    // 处理错误
  }

  // 处理不同的协议和操作模式
  if (_protocolType == TCP) {
    // 如果是TCP协议
    if (_socketMode == SEND) {

      if (bind(_sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        throw SocketException("Failed to bind on port " +
                              std::to_string(ntohs(localAddr.sin_port)) + ": " +
                              strerror(errno));
      }
      std::cout << "Socket successfully bound to port " << ntohs(localAddr.sin_port)<< std::endl;
      // 发送端不需要监听，直接配置
      // if (connect(_sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) <
      //     0) {
      //   throw SocketException("Failed to connect to the server: " +
      //                         std::string(strerror(errno)));
      // }
    } else if (_socketMode == RECV) {
      // 监听模式，绑定端口并开始监听

      if (bind(_sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
        throw SocketException("Failed to bind on port " +
                              std::to_string(ntohs(localAddr.sin_port)) + ": " +
                              strerror(errno));
      }
      std::cout << "Socket successfully bound to port "
                << ntohs(localAddr.sin_port) << std::endl;

      // 开始监听
      if (listen(_sockfd, SOMAXCONN) < 0) {
        throw SocketException("Failed to listen on port " +
                              std::to_string(ntohs(localAddr.sin_port)) + ": " +
                              strerror(errno));
      }

      fd_set readfds;
      struct timeval timeout;
      while (true && runFlag) {    // 循环等待连接
        timeout.tv_sec = 0;        // 设置秒为0，不阻塞
        timeout.tv_usec = 500000;  // 设置微秒为500000（0.5秒）

        FD_ZERO(&readfds);
        FD_SET(_sockfd, &readfds);

        // 使用 select 来检测是否有连接请求
        int ret = select(_sockfd + 1, &readfds, nullptr, nullptr, &timeout);
        if (ret == -1) {
          throw SocketException("The select function failed: " +
                                std::string(strerror(errno)));
        } else if (ret == 0) {
          // 如果 select 超时，表示没有新的连接请求
          std::cout << "TCP  connect timeout." << std::endl;
          continue;  // 超时后继续等待新连接
        } else {
          // 如果 select 检测到有事件，处理新的连接请求
          if (FD_ISSET(_sockfd, &readfds)) {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);

            newRecvSocket =
                accept(_sockfd, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (newRecvSocket < 0) {
              throw SocketException("Failed to accept connection: " +
                                    std::string(strerror(errno)));
            }

            std::cout << "New connection accepted from "
                      << inet_ntoa(clientAddr.sin_addr) << ":"
                      << ntohs(clientAddr.sin_port) << std::endl;
            return newRecvSocket;  // 跳出循环并返回新连接的套接字

            // 返回新的连接套接字
          }
        }
      }
    }

  } else {
    // 如果是UDP协议
  

    if (bind(_sockfd, (struct sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
      throw SocketException("Failed to bind on port " +
                            std::to_string(ntohs(localAddr.sin_port)) + ": " +
                            strerror(errno));
    }
    std::cout << "Socket successfully bound to UDP port "
              << ntohs(localAddr.sin_port) << std::endl;
    return _sockfd;
  }
  return 0;
}