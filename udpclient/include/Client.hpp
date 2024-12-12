#pragma once
#include <arpa/inet.h>
#include <net/if.h>  // for ifreq and interface names
#include <netinet/in.h>
#include <sys/ioctl.h>  // for ioctl() and ifreq
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>
#include <Eigen/Dense>
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <queue>
#include <string>
#include <vector>
#include "SignalProcess.hpp"
#include "Socket.hpp"

///HINT - 建议根据应用程序的数据接收速率和处理速度来设置,还有硬件的带宽，要看程序单位时间能处理多少数据，缓冲区大小不能少于每次处理的数据量
///TODO在命令行中设置系统参数： 使用 sysctl 命令增大接收和发送缓冲区的最大值。sudo sysctl -w net.core.rmem_max=1048576
///TODOsudo sysctl - w net.core.wmem_max = 1048576
///TODO使用 巨型帧（MTU > 1, 500）在支持的网络中可提高数据吞吐量。
///TODO多线程发送读取，套接字支持多个线程使用
// 假设的包头结构

/**
 * @brief  定义一个类继承runtime_error，理论上不可以通过读取代码来检测到的异常
 * explicit修饰构造函数避免隐式调用，试抛出异常和捕获异常类型一直
 * 在 Client 类中处理可恢复的异常，在 main 函数中处理不可恢复的和未捕获的异常
 */
class ClientException : public std::runtime_error {
 public:
  explicit ClientException(const std::string& msg) : std::runtime_error(msg) {}
  //调用基类 std::runtime_error 的构造函数，msg传给基类，把msg存储，使用what（）打印
};

class Client {

 public:
  /**
  * @brief  成员clientRecvPort是客户端接收数据的端口，是指服务端要发送到客户端的端口
  * 没有指定接收发送，服务器的发送端口由操作系统分配
  * FIXME:后续尝试指定端口发送数据，测试是否可以提升效率
  */
  struct CommunicationInfo {
    const uint16_t serverId;
    const std::string serverIp;
    Socket::ProtocolType protocolType;
    const uint16_t serverRecvPort;
    const uint16_t localSendPort;
    const uint16_t localRecvPort;

    struct timeval sendApiStart = {0};
    struct timeval recvApiStart = {0};
    struct timeval sendApiEnd = {0};
    struct timeval recvApiEnd = {0};
    struct timeval oldSendApiEnd = {0};
    struct timeval oldRecvApiEnd = {0};
    unsigned long long sendLen = 0;
    unsigned long long recvLen = 0;
    //std::atomic<bool> isPrinting;
    // std::atomic<bool> recvFlag = 0;
  };

  enum ThreadType {
    SEND,  // 发送
    RECV   // 接收
  };

 public:
  Client(std::vector<CommunicationInfo> infos);

  ~Client();

  uint16_t getClientNum();

  CommunicationInfo* getServerInfoById(uint16_t cserverId);

  CommunicationInfo getServerInfos(uint16_t serverId);

  static unsigned long getExecutionTime(struct timeval& startTime,
                                        struct timeval& stopTime);
  void setPrintFlag(uint16_t serverId, bool flag);

  bool getPrintFlag(uint16_t serverId);

  void setRunnFlag(uint16_t serverId, bool flag);

  bool getRunnFlag(uint16_t serverId);

  void printServerInfo(const uint16_t& serverId);

  struct sockaddr_in createSockAddr(const std::string& ip,
                                    const uint16_t& port);
  // 信号处理函数
  void signalHandler(int signum);

  void cleanup();

  static void signalHandlerWrapper(int signum);

  void* sendDataToServer(void* arg);

  void* recvDataFromServer(void* arg);

  static void* sendThreadFunction(void* arg);

  static void* recvThreadFunction(void* arg);

  void convertTimestamp(uint64_t timestamp);
      void start();

 private:
  void updateSysctlConfig(const std::string& parameter,
                          const std::string& value);
  void display(unsigned char* buf, int start, int end);

 private:
  // 存储单例实例的指针
  std::vector<CommunicationInfo> _serverInfos;
  uint16_t _serverNum;
  uint16_t _sendPriority;
  uint16_t _recvPriority;
  pthread_attr_t _sendAttr;
  pthread_attr_t _recvAttr;
  unsigned long _stackSize;
  unsigned long _printLimit;

  std::vector<uint16_t> _coreIds;
  std::vector<std::atomic<bool>> _isRunning;  //手动控制启动或停止
  std::vector<std::atomic<bool>> _isPrinting;  //手动控制打印

  std::queue<std::vector<char>> _consumerQueue;
  std::queue<std::vector<char>> _producerQueue;
  std::mutex _queueMutex;  // 使用 shared_ptr 管理 mutex
  std::condition_variable _queueCondition;

  uint16_t _timeOut;

 public:
  // 静态指针，指向 实例
  static Client* instance;
 private:
  SignalProcess signalProcess;
  };
