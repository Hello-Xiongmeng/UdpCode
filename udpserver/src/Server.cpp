#include "Server.hpp"
#include "Socket.hpp"
#include "ThreadManager.hpp"
#include "ChangePrint.hpp"
#include <chrono>
#include <random>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <thread>
#include <tuple>

Server::Server(std::vector<Server::CommunicationInfo> infos)
    : _clientInfos(std::move(infos)),
      _clientNum(_clientInfos.size()),
      _sendPriority(80),
      _recvPriority(90),        //接收的优先级高
      _stackSize(1024 * 1024),  //1M
      _coreIds({0, 1, 2, 3, 4, 5}),  //核心
      _printLimit(10000)   // 
{
  // 设置示例：将接收和发送缓冲区大小设置为 1048576 字节
  //TODO具体设置大小要进行实测，1M的缓冲区可能过大
  //updateSysctlConfig("net.core.rmem_max", std::to_string(CORE_BUF_SIZE));
  //updateSysctlConfig("net.core.wmem_max", std::to_string(CORE_BUF_SIZE));
  // 注册信号处理函数
  signal(SIGINT, &Server::signalHandlerWrapper);
  signal(SIGTERM, &Server::signalHandlerWrapper);
  signal(SIGSEGV, &Server::signalHandlerWrapper);
  signal(SIGUSR1, &Server::signalHandlerWrapper);
  // 初始化 _isPrinting 向量为 _clientNum 个 false
  _isRunning = std::vector<std::atomic<bool>>(_clientNum);

  _isPrinting = std::vector<std::atomic<bool>>(_clientNum);
  for (auto& printFlag : _isPrinting) {
    printFlag.store(false);  // 初始化为 false
  }
  for (auto& runFlag : _isRunning) {
    runFlag.store(true);  // 初始化为 true
  }

}

Server::~Server() {
  pthread_attr_destroy(&_sendAttr);
  pthread_attr_destroy(&_recvAttr);
}

uint16_t Server::getClientNum() {
  return _clientNum;
}

bool Server::getRunnFlag(uint16_t clientId) {
  return _isRunning[clientId];
}

void Server::setRunnFlag(uint16_t clientId, bool flag) {
  _isRunning[clientId].store(flag);
}
// 通过客户端 ID 获取对应的 CommunicationInfo
Server::CommunicationInfo* Server::getClientInfoById(uint16_t clientId) {
  for (auto& info : _clientInfos) {
    if (info.clientId == clientId) {
      return &info;
    }
  }
  return nullptr;  // 如果找不到，返回 nullptr
}

void Server::setPrintFlag(uint16_t clientId, bool flag) {
  _isPrinting[clientId].store(flag);
}

bool Server::getPrintFlag(uint16_t clientId) {
  return _isPrinting[clientId];
}

Server::CommunicationInfo Server::getClientInfos(uint16_t clientId) {
  return _clientInfos[clientId];
}

unsigned long Server::getExecutionTime(struct timeval& startTime,
                                       struct timeval& stopTime) {
  unsigned long timeVal;
  struct timeval executionTime;
  if (stopTime.tv_usec < startTime.tv_usec) {
    executionTime.tv_sec = stopTime.tv_sec - startTime.tv_sec - 1;
    executionTime.tv_usec = stopTime.tv_usec + 1000000 - startTime.tv_usec;
  } else {
    executionTime.tv_sec = stopTime.tv_sec - startTime.tv_sec;
    executionTime.tv_usec = stopTime.tv_usec - startTime.tv_usec;
  }
  timeVal = executionTime.tv_sec * 1000000 + executionTime.tv_usec;
  return timeVal;
}

void Server::printClientInfo(const uint16_t& clientId) {
  CommunicationInfo* client = getClientInfoById(clientId);
  if (client) {
    std::cout << "Client ID: " << client->clientId << std::endl;
    std::cout << "Client IP: " << client->clientIp << std::endl;
    std::cout << "Client Receive Port: " << client->clientRecvPort << std::endl;
    std::cout << "Local Send Port: " << client->localSendPort << std::endl;
    std::cout << "Local Receive Port: " << client->localRecvPort << std::endl;
  } else {
    std::cout << "Client ID \"" << clientId << "\" not found!" << std::endl;
  }
}
void Server::printRealTimeInfo(uint16_t clientId, ThreadType type) {}

/**
 * @brief  创建一个网络通信通用结构体
 * @param [入参] ip: 输入通信ip
 * @param [入参] port: 输入通信端口
 * @return struct sockaddr_in: 返回结构体变量
 * @note  创建客户端的网络结构体信息时，string& ip输入客户端的ip地址，
 * 创建服务端网络结构体信息时，输入0.0.0.0监听所有可用的网卡IP
 */
struct sockaddr_in Server::createSockAddr(const std::string& ip,
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

// 外部可以调用的函数，用于手动触发清理工作
void Server::cleanup() {
  for (auto& printFlag : _isPrinting) {
    printFlag.store(false);  // 初始化为 false
  }
  for (auto& runFlag : _isRunning) {
    runFlag.store(false);  // 初始化为 true
  }
  std::cout << "Cleanup over." << std::endl;
  // 这里可以做任何其他清理操作
}

// 处理信号的函数
void Server::signalHandler(int signum) {
  std::cout << "Received signal (" << signum << "), shutting down."
            << std::endl;
  switch (signum) {
    case SIGTERM:
      // SIGTERM 是终止信号，调用清理函数
      cleanup();
      break;
    case SIGINT:
      // 是中断信号（通常是 Ctrl+C），调用清理函数
      cleanup();
      break;
    case SIGSEGV:
      // SIGSEGV 是段错误信号，调用清理函数
      cleanup();
      break;
    case SIGUSR1:
      // 自定义信号反应函数
      std::cout << "Received SIGUSR1 signal." << std::endl;
      break;
  }
}

// 静态信号处理器（包装器），将静态方法转换为非静态方法
void Server::signalHandlerWrapper(int signum) {
  // 使用 static_cast 访问到 Server 对象，并调用实例方法
  if (instance) {
    instance->signalHandler(signum);
  }
}

/**
 * @brief  发送数据到客户端
 * @param [入参] data: 发送的数据，注意使用const，函数内部就不会修改外部值
 * @param [入参] CommunicationInfo: 客户端的ip地址和客户端的接收端口，const同上
 * @param [入参] running: 多线程bool值默认使用原子类型即可，避免竞争出现
 * @note  服务器的发送端口由操作系统分配，想要指定需要手动绑定
 * 形参中第一个const指向的内容不能修改，第二个const指针本身的地址不能修改
 */
void* Server::sendDataToClient(void* arg) {
  auto* args = static_cast<std::tuple<CommunicationInfo&>*>(arg);
  CommunicationInfo& info = std::get<0>(*args);
  try {
    unsigned long sendCount = 0;
    Socket sendSocket(AF_INET, SOCK_DGRAM, 0);
    sendSocket.configureSocket(info.localSendPort, Socket::SEND);
    struct sockaddr_in clientAddr =
        createSockAddr(info.clientIp, info.clientRecvPort);
    while (getRunnFlag(info.clientId)) {


      std::unique_lock<std::mutex> lock(_sendMutex);
      _sendCondition.wait(lock, [this, info] {
        return !_sendConsumer.empty() || !getRunnFlag(info.clientId);
      });

      if (_sendConsumer.empty() && !getRunnFlag(info.clientId)) {

        break;
      }
      ///NOTE:每次至少发送一个PRT

      printWithColor("yellow", "Sendqueue's size is ", _sendConsumer.size());

      while (!_sendConsumer.empty()) {

        ssize_t sendBytes =
            sendto(sendSocket.getFd(), _sendConsumer.front().data(),
                   _sendConsumer.front().size(), 0,
                   (struct sockaddr*)&clientAddr, sizeof(clientAddr));

        _sendConsumer.pop();  //释放数据所有权,调用析构函数

        // std::this_thread::sleep_for(std::chrono::seconds(1));
        if (sendBytes < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 缓冲区已满，等待一段时间重试,不用抛出异常
            std::cout << "Send buffer is full, retrying..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
          } else {
            throw ServerException("Failed to send data: " + info.clientIp +
                                  " " + std::string(strerror(errno)));
          }
        } else if (getPrintFlag(info.clientId) == false) {
          sendCount = 0;
          info.sendLen = 0;
        } else if (getPrintFlag(info.clientId) == true) {

          if (sendBytes != APP_BUF_SIZE) {
          
            printWithColor("blue","This is send thread last packet size: ",sendBytes," bytes");
          }

          sendCount++;
          info.sendLen += sendBytes;

          if (sendCount == 1) {
            gettimeofday(&info.sendApiStart, NULL);
          } else if (sendCount >= _printLimit) {
            gettimeofday(&info.sendApiEnd, NULL);

            auto consumeTime =
                getExecutionTime(info.sendApiStart, info.sendApiEnd);

            std::cout << "Thread " << std::this_thread::get_id()
                      << " bandwidth is :"
                      << (double)(info.sendLen) * 8 * 1000000 / 1024 / 1024 /
                             1024 / consumeTime
                      << "Gb / s" << std::endl;
            sendCount = 0;
            info.sendLen = 0;
            consumeTime = 0;
            // 通知打印线程
          }
          // std::cout << "Sent data to client at " << info.clientIp
          //           << ":" << info.clientRecvPort << std::endl;
        }
      }

      //解锁
    }
  } catch (const SocketException& e) {
    std::cerr << "Socket exception occurred: " << e.what() << std::endl;
  } catch (const ServerException& e) {
    std::cerr << "Server exception occurred: " << e.what() << std::endl;
  } catch (const std::system_error& e) {  //系统级别的错误
    std::cerr << "System error occurred: " << e.what() << std::endl;
  } catch (const std::exception& e) {  //其他标准异常
    std::cerr << "Exception occurred: " << e.what() << std::endl;
  } catch (...) {  //所有其他未知类型的异常
    std::cerr << "An unknown error occurred during sending data." << std::endl;
  }

  if (getRunnFlag(info.clientId) == false) {
    std::cout << "Sending has been stopped by setting isRunning" << std::endl;
  } else {
    std::cout << "Sending has been stopped by catching exception" << std::endl;
  }
  return nullptr;
}

void Server::convertTimestamp(uint64_t timestamp) {
  // 将毫秒时间戳转为秒
  auto seconds = std::chrono::seconds(timestamp / 1000);
  auto milliseconds = std::chrono::milliseconds(timestamp % 1000);

  // 创建 time_point 对象
  std::chrono::time_point<std::chrono::system_clock> timePoint(seconds);

  // 转换为 time_t
  std::time_t timeT = std::chrono::system_clock::to_time_t(timePoint);

  // 转换为本地时间
  std::tm* localTime = std::localtime(&timeT);

  // 格式化输出时间
  std::cout << "Readable time: "
            << std::put_time(localTime, "%Y-%m-%d %H:%M:%S")  // 格式化日期时间
            << "." << std::setfill('0') << std::setw(3)
            << milliseconds.count()  // 添加毫秒
            << std::endl;
}

// 模拟生成大小不确定的数据
std::vector<float> Server::generateData(size_t minSize, size_t maxSize) {

  // minSize = 64 * 3260 * 2;
  // maxSize = 64 * 3260 * 4;
  //定义了一个大小是minSize, maxSize之间的数组
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<size_t> dist(minSize, maxSize);
  size_t dataSize = dist(gen);
  std::vector<float> data(dataSize);
  char* rawMemory = new char[PACKET_HEADER_SIZE + UDP_PAYLOAD_SIZE];
  std::unique_ptr<char[]> sendPacket(rawMemory);
  // 填充数据（可以是随机数）
  for (size_t i = 0; i < dataSize; ++i) {
    data[i] = static_cast<float>(i % 1000) * 0.1f;  // 模拟一些浮点数
  }
  return data;
}

// 接收线程，模拟接收数据并分包,每次接收一组
void Server::shardData() {

  int j = 0;
  while (getRunnFlag(0)) {
    for (; j < 100; j++) {

      // if (j%10 == 0) {

      //   std::this_thread::sleep_for(std::chrono::seconds(1));
      // }
                                         // 模拟生成数据64 * 3260 * 4180 * 3260 * 4)
      auto data = generateData(64 * 3260 * 4,180 * 3260 * 4 );  //随机生成
    

      size_t totalBytes = data.size() * sizeof(float);         //字节数
      //向上取整的数学公式 ⌈a/b⌉=(a+b−1)/b  避免丢失包
      int totalPackets = (totalBytes + UDP_PAYLOAD_SIZE - 1) / UDP_PAYLOAD_SIZE;
      // 获取当前时间戳作为组序号
      uint64_t timestamp =
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::system_clock::now().time_since_epoch())
              .count();

      //convertTimestamp(timestamp);
      //以复数形式输出10个数据
      
      // for (int i = 0; i < 10; i++) {
      //   std::cout << "(" << data[i * 2] << " , ";
      //   std::cout << data[i * 2 + 1] << ")";

      // }
      // 分包并存入队列
      for (int i = 0; i < totalPackets; ++i) {

        if (i == 80 && (j % 10 == 0)) {
          convertTimestamp(timestamp);
          continue;
        }

        // 填充负载
        size_t offset = i * UDP_PAYLOAD_SIZE;  //原始分片起始位置
        size_t payloadSize = std::min(static_cast<size_t>(UDP_PAYLOAD_SIZE),
                                      totalBytes - offset);

        ///FIXME 最后一片数据不会填满，要怎么处理，在发送时处理还是接收时处理
        ///NOTE:使用负载数去开辟内存，保证不会多发数据
        //  char* rawMemory = new char[PACKET_HEADER_SIZE + payloadSize];

        // std::unique_ptr<char[]> sendPacket(rawMemory);  //在堆上给智能指针管理内存

        std::vector<char> sendPacket2(PACKET_HEADER_SIZE + payloadSize);

        UdpHeader header = {timestamp, static_cast<uint16_t>(i),
                            static_cast<uint16_t>(totalPackets)};
        //  memcpy(a, b, c);把在b位置的c个数据拷贝到a位置

        memcpy(sendPacket2.data(), &header, PACKET_HEADER_SIZE);  //放在头部

        memcpy(sendPacket2.data() + PACKET_HEADER_SIZE,
               reinterpret_cast<const char*>(data.data()) + offset,
               payloadSize);

        _sendProducer.push(std::move(sendPacket2));

        if (i == totalPackets - 1) {
          // printWithColor("blue", "This is main thread last packet size: ",
          //                payloadSize + PACKET_HEADER_SIZE, " bytes");

          //  std::cout << "This is main thread last packet size: "
          //            << payloadSize + PACKET_HEADER_SIZE << " bytes" << std::endl;
        }
      }
      //   for (int i = 0; i < 30; i++) {
      //   std::cout  <<  static_cast<int>(_sendProducer.front()[i+16]) << " , ";
      //  //std::cout << static_cast<int>(_sendProducer.back()[i * 2 + 1]) << ")";

      // }
      // printWithColor("blue",
      //                "This is main thread total packets number: ", totalPackets,
      //                "  packets");
      // std::cout << "This is main thread total packets number: " << totalPackets
      //           << "  packets" << std::endl;

      // 加入队列
      //NOTE:在wsl已测试每次传输一个prt不会发生两个prt以上发送情况，如果严格要求一次发一个prt则，直接加锁而不是尝试加锁
      {
        std::unique_lock<std::mutex> lock(
            _sendMutex,
            std::try_to_lock);  // 在两个队列交换数据时尝试加锁
        if (lock.owns_lock()) {

        //int j =0;
          while (!_sendProducer.empty()) {
            _sendConsumer.push(
                std::move(_sendProducer.front()));  //数据所有权归消费者队列
            _sendProducer.pop();

              // for (; j < 1; j++){
              //     for (int i = 0; i < 10; i++) {
              //       std::cout << "("
              //                 << static_cast<int>(_sendProducer.front()[i * 2])
              //                 << " , ";
              //       std::cout
              //           << static_cast<int>(_sendProducer.front()[i * 2 + 1])
              //           << ")";
              //     }
              //   }
          }
          _sendCondition.notify_one();
        }
      }
    }

    //std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  _sendCondition.notify_all();
}

/**
  * @brief
  * @param [入参] arg: 使用 void* arg 作为参数可以接收任意类型和任意数量的参数
  * @return void*:
  */
void* Server::sendThreadFunction(void* arg) {
  instance->sendDataToClient(arg);
  return nullptr;
}

/**
 * @brief  接收来自客户端的数据
 * @param [入参] recvPort:
 * @param [入参] running:
 * @note  使用createSockAddr("0.0.0.0", CommunicationInfo.recvPort)时，
 * 0.0.0.0监听所有可用的网卡IP，CommunicationInfo.recvPort指定本地服务端就收数据的端口
 * 使用bind指定接收服务端数据的端口，因为客户端需要指定出服务端接收数据的端口，两个端口要统一
 *
 */
void* Server::recvDataFromClient(void* arg) {

  auto* args = static_cast<std::tuple<CommunicationInfo&>*>(arg);
  CommunicationInfo& info = std::get<0>(*args);
  try {
    Socket recvSocket(AF_INET, SOCK_DGRAM, 0);
    recvSocket.configureSocket(info.localRecvPort, Socket::RECV);
    fd_set readfds;
    struct timeval timeout;
    std::unique_ptr<char[]> _recvbuf = std::make_unique<char[]>(APP_BUF_SIZE);
    if (!_recvbuf) {
      throw ServerException("Alloc recvbuf failed" +
                            std::string(strerror(errno)));
    }
    unsigned long recvCount = 0;

    // int cnt = 0;
    while (getRunnFlag(info.clientId)) {
      //fflush(stdout);  //刷新流 stream 的输出缓冲区。
      //设置超时为 1 秒
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      FD_ZERO(&readfds);
      FD_SET(recvSocket.getFd(), &readfds);
      //使用 select 来轮询接收套接字
      //如果没有数据会在select阻塞一定的时间，超时进入下一次循环,继续检查
      int ret =
          select(recvSocket.getFd() + 1, &readfds, nullptr, nullptr, &timeout);
      if (ret == -1) {
        throw ServerException(
            "the select function in recvDataFromClient function " +
            std::string(strerror(errno)));
        break;
      } else if (ret == 0) {
        //如果 select 超时，检查是否需要停止接收
        std::cout << "Receiving timeout" << std::endl;
        continue;  // 超时不处理，继续检查 isRunning
      }
      if (FD_ISSET(recvSocket.getFd(), &readfds)) {
        ssize_t recvBytes = recvfrom(recvSocket.getFd(), _recvbuf.get(),
                                     APP_BUF_SIZE - 1, 0, nullptr, nullptr);
        if (recvBytes > 0) {
          _recvbuf[recvBytes] = '\0';
          std::cout << "Received result from client: " << _recvbuf.get()
                    << std::endl;
          if (getPrintFlag(info.clientId) == false) {
            recvCount = 0;
            info.recvLen = 0;
          } else if (getPrintFlag(info.clientId) == true) {
            recvCount++;
            info.recvLen += recvBytes;
            if (recvCount == 1) {
              gettimeofday(&info.recvApiStart, NULL);
            } else {
              if (recvCount >= _printLimit) {
                gettimeofday(&info.recvApiEnd, NULL);
                auto consumeTime =
                    getExecutionTime(info.recvApiStart, info.recvApiEnd);
                std::cout << "Thread " << std::this_thread::get_id()
                          << " bandwith is :"
                          << (double)(info.recvLen) * 8 * 1000000 / 1024 /
                                 1024 / 1024 / consumeTime
                          << "Gb / s" << std::endl;
                recvCount = 0;
                info.recvLen = 0;
                consumeTime = 0;
              }
            }
          }

        } else {
          throw ServerException("Failed to receive data from server" +
                                std::string(strerror(errno)));
        }
      }
    }
  } catch (const SocketException& e) {
    std::cerr << "Socket exception occurred: " << e.what() << std::endl;
  } catch (const ServerException& e) {
    std::cerr << "Server exception occurred: " << e.what() << std::endl;
  } catch (const std::system_error& e) {  //系统级别的错误
    std::cerr << "System error occurred: " << e.what() << std::endl;
  } catch (const std::exception& e) {  //其他标准异常
    std::cerr << "Exception occurred: " << e.what() << std::endl;
  } catch (...) {  //所有其他未知类型的异常
    std::cerr << "An unknown error occurred during sending data." << std::endl;
  }
  if (getRunnFlag(info.clientId) == false) {
    std::cout << "Recving has been stopped by setting isRunning" << std::endl;
  } else {
    std::cout << "Recving has been stopped by catching exception" << std::endl;
  }
  return nullptr;
}

/**
 * @brief  接收数据
 * @param [入参] arg:
 * @return void*:
 */

void* Server::recvThreadFunction(void* arg) {
  instance->recvDataFromClient(arg);
  return nullptr;
}

/**
 *
 * @brief  待开发
 * @param [入参] recvSockfd:
 * @param [入参] server:
 */
void Server::recvDataFromFPGA(int recvSockfd, Server& server) {
  char buffer[400];
  while (true) {
    ssize_t n = recvfrom(recvSockfd, buffer, 400 - 1, 0, nullptr, nullptr);
    if (n > 0) {
      buffer[n] = '\0';  // null-terminate the string
      // 这里可以将数据存入一个队列或直接发送给客户端
      //  server.sendDataToClient(, /* client info */);
    } else {
      std::cerr << "Failed to receive data from FPGA"
                << " , error: " << strerror(errno) << std::endl;
    }
  }
}

void Server::start() {

  ThreadManager::printfCpuInfo();

  ThreadManager::setThreadAttributes(_sendAttr, _sendPriority, _stackSize);
  ThreadManager::setThreadAttributes(_recvAttr, _recvPriority, _stackSize);

  std::vector<pthread_t> sendThreads(_clientNum);
  std::vector<pthread_t> recvThreads(_clientNum);
  std::vector<pthread_t> shardDataThreads(_clientNum);

  for (uint16_t i = 0; i < _clientNum; ++i) {
    printClientInfo(i);

    auto sendArgs = std::make_tuple(std::ref(_clientInfos[i]));
    if (pthread_create(&sendThreads[i], &_sendAttr, sendThreadFunction,
                       &sendArgs) != 0) {
      throw ServerException("Falied to create send thread for client " +
                            std::to_string(i));
    }
    ThreadManager::bindThreadToCore(sendThreads[i], _coreIds[i]);
    ThreadManager::printfThreadInfo(sendThreads[i]);
    auto recvArgs = std::make_tuple(std::ref(_clientInfos[i]));
    // std::cout << server._clientInfos[i].localRecvPort << std::endl;

    if (pthread_create(&recvThreads[i], &_recvAttr, recvThreadFunction,
                       &recvArgs) != 0) {
      throw ServerException("Falied to create recv thread for client  " +
                            std::to_string(i));
    }
    ThreadManager::bindThreadToCore(recvThreads[i], _coreIds[i]);
    ThreadManager::printfThreadInfo(recvThreads[i]);

    shardData();
  }

  // for (auto& thread : sendThreads) {
  //   pthread_join(thread, nullptr);
  // }
  // for (auto& thread : recvThreads) {
  //   pthread_join(thread, nullptr);
  // }
}

void Server::updateSysctlConfig(const std::string& parameter,
                                const std::string& value) {
  std::ofstream configFile("/etc/sysctl.conf", std::ios::app);
  if (!configFile) {
    throw ServerException("Failed to open /etc/sysctl.conf for writing" +
                          std::string(strerror(errno)));
    return;
  }
  // 写入参数到文件末尾
  configFile << parameter << "=" << value << std::endl;
  configFile.close();

  // 应用更改，使其立即生效
  int result = system("sysctl -p");
  if (result != 0) {
    throw ServerException("Failed to reload sysctl configuration " +
                          std::string(strerror(errno)));
  } else {
    std::cout << "Sysctl configuration updated and reloaded successfully."
              << std::endl;
  }
}

void Server::display(unsigned char* buf, int start, int end) {
  int i;
  if (start >= end)
    return;
  for (i = start; i < end / 4; i++) {
    if (i % 8 == 0)
      printf("\n");
    printf("0x%08x  ", *((unsigned int*)buf + i));
  }
  printf("\n");
}
