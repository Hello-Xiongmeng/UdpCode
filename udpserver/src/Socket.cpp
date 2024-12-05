#include "Socket.hpp"
#include "ChangePrint.hpp"
#include <iostream>
#include <string.h>

Socket::Socket(int domain, int type, int protocol)
    : 
      _coreRecvBufSize(CORE_BUF_SIZE),
      _coreSendBufSize(CORE_BUF_SIZE),
      _ifName("eth0") {
  _sockfd = socket(domain, type, protocol);
  if (_sockfd < 0) {
    throw SocketException("Failed create socket: " +
                          std::string(strerror(errno)));
  }
  std::cout << "Socket created with fd: " << _sockfd << std::endl;

         //#define IFNAME "enp12s0f1"		//socket绑定网卡

  }

/**
   * @brief  Destroy the Socket object
   */
Socket::~Socket() {
  if (_sockfd != -1) {
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

/**
   * @brief  Set the Max Socket Buffer Size object
   * @param [入参] bufferType: 
   * @return int: 
   */
void Socket::setMaxSocketBufferSize(SocketType bufferType) {
  int bufferSizeBefore = 0, bufferSizeAfter = 0;  // 使用正确的数据类型
  socklen_t len = sizeof(bufferSizeBefore);

  // 根据缓冲区类型选择选项
  int option = (bufferType == SEND) ? SO_SNDBUF : SO_RCVBUF;
  int maxBufferSize =
      (bufferType == SEND) ? _coreSendBufSize : _coreRecvBufSize;

  // 获取当前的缓冲区大小
  if (getsockopt(_sockfd, SOL_SOCKET, option, &bufferSizeBefore, &len) < 0) {
    throw SocketException("Failed to get socket buffer size: " +
                          std::string(strerror(errno)));
  }

  // 输出系统默认的缓冲区大小
  std::cout << ((bufferType == SEND) ? "Default send buffer size: "
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
  std::cout << ((bufferType == SEND) ? "Updated send buffer size: "
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
void Socket::configureSocket(const uint16_t& port, SocketType type) {

  // 将套接字绑定到eth0接口
  bindSocketToInterface(_ifName.c_str());
  // 设置套接字为非阻塞模式,读取缓冲区无数据，返回一个错误，写入缓冲区已满返回错误，写入时如果已满会丢失数据，要尝试重新写入
  int flags = fcntl(_sockfd, F_GETFL, 0);  //get函数
  if (flags < 0) {
    throw SocketException("Failed fcntl()  " + std::string(strerror(errno)));
  }
  fcntl(_sockfd, F_SETFL, flags | O_NONBLOCK);  //set函数
  struct sockaddr_in localServerAddr = createSockAddr("0.0.0.0", port);
  if (bind(_sockfd, (struct sockaddr*)&localServerAddr,
           sizeof(localServerAddr)) < 0) {
    throw SocketException("Failed bind on port " +
                          std::to_string(ntohs(localServerAddr.sin_port)) +
                          ": " + strerror(errno));
  }
  std::cout << "Sockfd successfully bound to local port :" << port << std::endl;
  setMaxSocketBufferSize(type);
}