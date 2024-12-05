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
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <queue>
#include <string>
#include <vector>


///HINT - 建议根据应用程序的数据接收速率和处理速度来设置,还有硬件的带宽，要看程序单位时间能处理多少数据，缓冲区大小不能少于每次处理的数据量
///TODO在命令行中设置系统参数： 使用 sysctl 命令增大接收和发送缓冲区的最大值。sudo sysctl -w net.core.rmem_max=1048576
///TODOsudo sysctl - w net.core.wmem_max = 1048576
///TODO使用 巨型帧（MTU > 1, 500）在支持的网络中可提高数据吞吐量。
///TODO多线程发送读取，套接字支持多个线程使用
 struct UdpHeader {
    uint64_t timeStamp= 0;  // 时间戳 (组序号)
    uint16_t sequence= 0;  // 分片序号
    uint16_t total= 0;     // 总分片数
    
  };
constexpr int MTU = 9000;//HINT巨型帧需要每次修改吗，使用巨型帧要确定双方配置相同，使用大数据包ping通，保证不丢失
constexpr int IP_HEADER_SIZE = 20;
constexpr int UDP_HEADER_SIZE = 8;
constexpr int PACKET_HEADER_SIZE = sizeof(UdpHeader) ;
constexpr int UDP_PAYLOAD_SIZE = MTU - IP_HEADER_SIZE - UDP_HEADER_SIZE -
                                 PACKET_HEADER_SIZE;  //1468，真实数据的大小

constexpr int APP_BUF_SIZE = MTU - IP_HEADER_SIZE - UDP_HEADER_SIZE;

/**
 * @brief  定义一个类继承runtime_error，理论上不可以通过读取代码来检测到的异常
 * explicit修饰构造函数避免隐式调用，试抛出异常和捕获异常类型一直
 * 在 Server 类中处理可恢复的异常，在 main 函数中处理不可恢复的和未捕获的异常
 */
class ServerException : public std::runtime_error {
 public:
  explicit ServerException(const std::string& msg) : std::runtime_error(msg) {}
  //调用基类 std::runtime_error 的构造函数，msg传给基类，把msg存储，使用what（）打印
};

class Server {

 public:
  /**
  * @brief  成员clientRecvPort是客户端接收数据的端口，是指服务端要发送到客户端的端口
  * 没有指定接收发送，服务器的发送端口由操作系统分配
  * FIXME:后续尝试指定端口发送数据，测试是否可以提升效率
  */
  struct CommunicationInfo {
    const uint16_t clientId;
    const std::string clientIp;
    const uint16_t clientRecvPort;
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
  Server(std::vector<CommunicationInfo> infos);

  ~Server();

  uint16_t getClientNum();

  CommunicationInfo* getClientInfoById(uint16_t clientId);


  CommunicationInfo getClientInfos(uint16_t clientId);

  static unsigned long getExecutionTime(struct timeval& startTime,
                                        struct timeval& stopTime);
  void setPrintFlag(uint16_t clientId, bool flag);

  bool getPrintFlag(uint16_t clientId);

  void setRunnFlag(uint16_t clientId, bool flag);

  bool getRunnFlag(uint16_t clientId);

  void printClientInfo(const uint16_t& clientId);

  void printRealTimeInfo(uint16_t clientId, ThreadType type);

  struct sockaddr_in createSockAddr(const std::string& ip,
                                    const uint16_t& port);
  // 信号处理函数
  void signalHandler(int signum);

  void cleanup();

  static void signalHandlerWrapper(int signum);

  void* sendDataToClient(void* arg);

  void convertTimestamp(uint64_t timestamp);

  std::vector<float> generateData(size_t minSize, size_t);

  void shardData();

  void* recvDataFromClient(void* arg);

  static void* sendThreadFunction(void* arg);

  static void* recvThreadFunction(void* arg);

  void recvDataFromFPGA(int recvSockfd, Server& server);

  void start();

 private:
  void updateSysctlConfig(const std::string& parameter,
                          const std::string& value);
  void display(unsigned char* buf, int start, int end);

 private:
  // 存储单例实例的指针
  static Server* _instance;
  std::vector<CommunicationInfo> _clientInfos;
  uint16_t _clientNum;
  uint16_t _sendPriority;
  uint16_t _recvPriority;
  pthread_attr_t _sendAttr;
  pthread_attr_t _recvAttr;
  unsigned long _stackSize;
  std::queue<std::vector<char>> _sendProducer;
  std::queue < std::vector < char >> _sendConsumer;

  std::mutex _sendMutex;
  std::condition_variable _sendCondition;

  std::vector<uint16_t> _coreIds;
  std::vector<std::atomic<bool>> _isRunning;
  // g_isRunning = false;  // 停止所有线程的循环

  std::vector<std::atomic<bool>> _isPrinting;

  unsigned long _printLimit;

 public:
  // 静态指针，指向 Server 实例
  static Server* instance;

  // std::condition_variable cv;
  // std::mutex mtx;
  // bool readyToPrint = false;  // 标志是否可以打印信息
};