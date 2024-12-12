
#include "Client.hpp"
#include <unistd.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <tuple>
#include "ChangePrint.hpp"
#include "Socket.hpp"
#include "ThreadManager.hpp"

Client::Client(std::vector<CommunicationInfo> infos)
    : _serverInfos(std::move(infos)),
      _serverNum(_serverInfos.size()),
      _sendPriority(80),
      _recvPriority(90),             //接收的优先级高
      _stackSize(512 * 1024),        //512KB
      _coreIds({0, 1, 2, 3, 4, 5}),  //核心
      _timeOut(50),                  //50
      _printLimit(10000),
      signalProcess(_consumerQueue, _queueMutex, _queueCondition) {
  // 设置示例：将接收和发送缓冲区大小设置为 1048576 字节
  //TODO具体设置大小要进行实测，1M的缓冲区可能过大
  //updateSysctlConfig("net.core.rmem_max", std::to_string(CORE_BUF_SIZE));
  //updateSysctlConfig("net.core.wmem_max", std::to_string(CORE_BUF_SIZE));
  // 注册信号处理函数
  signal(SIGINT, signalHandlerWrapper);
  signal(SIGTERM, signalHandlerWrapper);
  signal(SIGSEGV, signalHandlerWrapper);
  signal(SIGUSR1, signalHandlerWrapper);
  // 初始化 _isPrinting 向量为 _clientNum 个 false
  _isRunning = std::vector<std::atomic<bool>>(_serverNum);
  _isPrinting = std::vector<std::atomic<bool>>(_serverNum);

  for (auto& runFlag : _isRunning) {
    runFlag.store(true);  // 初始化为 true
  }
  for (auto& printFlag : _isPrinting) {
    printFlag.store(false);  // 初始化为 false
  }
}

Client::~Client() {
  pthread_attr_destroy(&_sendAttr);
  pthread_attr_destroy(&_recvAttr);
  cleanup();
  std::cout << "Client destruction." << std::endl;
}

uint16_t Client::getClientNum() {
  return _serverNum;
}

Client::CommunicationInfo* Client::getServerInfoById(uint16_t serverId) {
  for (auto& info : _serverInfos) {
    if (info.serverId == serverId) {
      return &info;
    }
  }
  return nullptr;
}

Client::CommunicationInfo Client::getServerInfos(uint16_t serverId) {
  return _serverInfos[serverId];
}

unsigned long Client::getExecutionTime(timeval& startTime, timeval& stopTime) {
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

void Client::setPrintFlag(uint16_t serverId, bool flag) {
  _isPrinting[serverId].store(flag);
}

bool Client::getPrintFlag(uint16_t serverId) {
  return _isPrinting[serverId];
}

void Client::setRunnFlag(uint16_t serverId, bool flag) {
  _isRunning[serverId].store(flag);
}

bool Client::getRunnFlag(uint16_t serverId) {
  return _isRunning[serverId];
}

void Client::printServerInfo(const uint16_t& serverId) {

  CommunicationInfo* server = getServerInfoById(serverId);
  if (server) {
    std::cout << "Server ID: " << server->serverId << std::endl;
    std::cout << "Server IP: " << server->serverIp << std::endl;
    std::cout << "Server Receive Port: " << server->serverRecvPort << std::endl;
    std::cout << "Local Send Port: " << server->localSendPort << std::endl;
    std::cout << "Local Receive Port: " << server->localRecvPort << std::endl;
  } else {
    std::cout << "Server ID \"" << serverId << "\" not found!" << std::endl;
  }
}

sockaddr_in Client::createSockAddr(const std::string& ip,
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

void Client::signalHandler(int signum) {
  // std::cout << "Received signal (" << signum << "), shutting down."
  //           << std::endl;
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

void Client::cleanup() {

  for (auto& printFlag : _isPrinting) {
    printFlag.store(false);  // 初始化为 false
  }
  for (auto& runFlag : _isRunning) {
    runFlag.store(false);  // 初始化为 true
  }
}

void Client::signalHandlerWrapper(int signum) {
  if (instance) {
    instance->signalHandler(signum);
  }
}

void* Client::sendDataToServer(void* arg) {

  auto* args = static_cast<std::tuple<CommunicationInfo&>*>(arg);
  auto& info = std::get<0>(*args);
  try {

    unsigned long sendCount = 0;
    unsigned char sendBuf[TCP_APP_BUF_SIZE];

    Socket sendSocket(AF_INET, Socket::UDP, Socket::SEND, 0);
    sendSocket.configureSocket(info.localSendPort, _isRunning[0]);
    auto serverAddr = createSockAddr(info.serverIp, info.serverRecvPort);
    while (getRunnFlag(info.serverId)) {
      ssize_t sendBytes =
          sendto(sendSocket.getFd(), sendBuf, TCP_APP_BUF_SIZE, 0,
                 (struct sockaddr*)&serverAddr, sizeof(serverAddr));
      if (sendBytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // 缓冲区已满，等待一段时间重试,不用抛出异常
          std::cout << "Send buffer is full, retrying..." << std::endl;
          std::this_thread::sleep_for(std::chrono::milliseconds(_timeOut));
          continue;
        } else {
          throw ClientException("Failed to send data: " + info.serverIp + " " +
                                std::string(strerror(errno)));
        }
      } else if (getPrintFlag(info.serverId) == false) {
        sendCount = 0;
        info.sendLen = 0;
      } else if (getPrintFlag(info.serverId) == true) {
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
        // std::cout << "Sent data to server at " << info.serverIp
        //           << ":" << info.serverRecvPort << std::endl;
      }
    }
  } catch (const SocketException& e) {
    std::cerr << "Socket exception occurred: " << e.what() << std::endl;
  } catch (const ClientException& e) {
    std::cerr << "Server exception occurred: " << e.what() << std::endl;
  } catch (const std::system_error& e) {  //系统级别的错误
    std::cerr << "System error occurred: " << e.what() << std::endl;
  } catch (const std::exception& e) {  //其他标准异常
    std::cerr << "Exception occurred: " << e.what() << std::endl;
  } catch (...) {  //所有其他未知类型的异常
    std::cerr << "An unknown error occurred during sending data." << std::endl;
  }

  if (getRunnFlag(info.serverId) == false) {
    std::cout << "Sending has been stopped by setting isRunning" << std::endl;
  } else {
    std::cout << "Sending has been stopped by catching exception" << std::endl;
  }
  return nullptr;
}

void* Client::recvDataFromServer(void* arg) {

  auto* args = static_cast<std::tuple<CommunicationInfo&>*>(arg);
  auto& info = std::get<0>(*args);
  try {
    Socket recvSocket(AF_INET, info.protocolType, Socket::RECV, 0);

    std::unique_ptr<int> newRecvSocket;

    if (info.protocolType == Socket::TCP) {

      newRecvSocket = std::make_unique<int>(
          recvSocket.configureSocket(info.localRecvPort, _isRunning[0]));

    } else if (info.protocolType == Socket::UDP) {

      recvSocket.configureSocket(info.localRecvPort, _isRunning[0]);
      newRecvSocket = std::make_unique<int>(recvSocket.getFd());
    }

    fd_set readfds;
    struct timeval timeout;
    unsigned long recvCount = 0;
    uint16_t packets;
    uint16_t oldPackets;
    int64_t timeStamp;
    int64_t oldTimeStamp;
    std::vector<char> recvBuf;
    ssize_t recvBytes = 0;
    ssize_t oldRecvBytes = 0;

    //NOTE:在循环之外定义循环内使用的对象和变量提高性能，避免重复定义
    //
    // 标志位，确保只处理一次 "READY" 和 "OK"
    bool handshakeDone = false;

    while (getRunnFlag(info.serverId)) {
      //fflush(stdout);  //刷新流 stream 的输出缓冲区。
      //设置超时为 1 秒
      timeout.tv_sec = 0;
      timeout.tv_usec =
          _timeOut * 1000;  //微秒数 tv_usec 必须在 0 到 999,999 之间
      FD_ZERO(&readfds);
      FD_SET(*newRecvSocket, &readfds);
      //使用 select 来轮询接收套接字
      //如果没有数据会在select阻塞一定的时间，超时进入下一次循环,继续检查
      int ret =
          select(*newRecvSocket + 1, &readfds, nullptr, nullptr, &timeout);
      if (ret == -1) {
        throw ClientException(
            "the select function in recvDataFromserver function " +
            std::string(strerror(errno)));
        break;
      } else if (ret == 0) {
        //如果 select 超时，检查是否需要停止接收
        //std::cout << "Timeout, no data received." << std::endl;
        continue;  // 超时不处理，继续检查 isRunning
      }
      if (FD_ISSET(*newRecvSocket, &readfds)) {
        if (!handshakeDone && info.protocolType == Socket::TCP) {
          char buffer[256];  // 收到数据
          ssize_t received = recv(*newRecvSocket, buffer, sizeof(buffer), 0);

          if (received < 0) {
            if (errno == EWOULDBLOCK) {
              // 非阻塞模式下没有数据
              std::cout << "No data received, continuing..." << std::endl;
              continue;
            } else {
              // 其他错误
              std::cerr << "Error receiving data: " << strerror(errno)
                        << std::endl;
              break;
            }
          }

          std::string receivedMsg(buffer, received);
          if (!handshakeDone && receivedMsg == "READY") {
            // 收到 "READY" 消息，表示发送端准备好了
            std::cout << "Received 'READY' message from sender. Sending 'OK'..."
                      << std::endl;

            // 发送 "OK" 确认消息
            std::string okMessage = "OK";
            ssize_t sentBytes =
                send(*newRecvSocket, okMessage.c_str(), okMessage.length(), 0);
            if (sentBytes < 0) {
              std::cerr << "Failed to send 'OK' message to sender."
                        << std::endl;
              break;
            }

            std::cout << "Sent 'OK' message to sender. Now ready to receive "
                         "actual data..."
                      << std::endl;
            handshakeDone = true;  // 设置标志，确保只执行一次握手逻辑
          }
        } else {

          ///fixme:需要每次开辟吗，直接开辟足够使用的内存
          recvBuf = std::vector<char>(recvSocket.getAppBufSize());
          //std::vector 内部的数据是动态分配的，通常是在堆上进行分配

          if (info.protocolType == Socket::UDP) {
            oldRecvBytes = recvBytes;
            recvBytes =
                recvfrom(*newRecvSocket, recvBuf.data(),
                         recvSocket.getAppBufSize(), 0, nullptr, nullptr);
          } else if (info.protocolType == Socket::TCP) {

            // 接受连接后，使用新的套接字进行数据接收
            oldRecvBytes = recvBytes;

            recvBytes = recv(*newRecvSocket, recvBuf.data(),
                             recvSocket.getAppBufSize(), 0);
            if (recvBytes < 0) {
              throw ClientException("Failed to receive data from server: " +
                                    std::string(strerror(errno)));
            }
          }

          if (recvBytes > 0) {

            if (getPrintFlag(info.serverId) == false) {

              recvCount = 0;
              info.recvLen = 0;

            } else if (getPrintFlag(info.serverId) == true) {

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
                            << " bandwidth is :"
                            << (double)(info.recvLen) * 8 * 1000000 / 1024 /
                                   1024 / 1024 / consumeTime
                            << "Gb / s" << std::endl;
                  recvCount = 0;
                  info.recvLen = 0;
                  consumeTime = 0;
                }
              }
            }

   
            //FIXME:这里的需要考虑结构体的内存对齐问题吗
            memcpy(&timeStamp, recvBuf.data(), sizeof(uint64_t));

            //第一次接收时，或者已经清空之后第一次接收时，时间戳给相同的值
            if (_producerQueue.empty()) {
              oldTimeStamp = timeStamp;
              convertTimestamp(timeStamp);
            }

            if (timeStamp != oldTimeStamp) {
              convertTimestamp(timeStamp);

              //std::unique_lock<std::mutex> lock(_queueMutex);
              std::unique_lock<std::mutex> lock(_queueMutex, std::try_to_lock);
              // 在两个队列交换数据时尝试加锁
              if (lock.owns_lock()) {

                printWithColor("yellow",
                               "ProducerQueue:", _producerQueue.size(),
                               "packets");
                while (!_producerQueue.empty()) {
                 

                  _consumerQueue.push(std::move(_producerQueue.front()));
                  _producerQueue.pop();

                }

                _queueCondition.notify_one();

              }

              oldTimeStamp = timeStamp ;
            }

            //如果时间戳变了，锁住_consumerQueue，交换数据，时间戳不变一直往队列里放数据
            _producerQueue.push(std::move(recvBuf));

            ///fixme： _batchSize 要合理设置大小，才能提高接受效率
            ///NOTE:保证每次接收一个prt合并之后交给处理线程,如果丢包这个逻辑还成立吗

          } else {

            throw ClientException("Failed to receive data from server" +
                                  std::string(strerror(errno)));
          }
        }
      }
    }

    // 确保在循环结束后，将剩余的数据也存入队列
    if (!_producerQueue.empty()) {
      std::lock_guard<std::mutex> lock(_queueMutex);
      while (!_producerQueue.empty()) {
        _consumerQueue.push(std::move(_producerQueue.front()));
        _producerQueue.pop();
      }
      // lock.unlock();  //显式释放锁
      _queueCondition.notify_one();
      //batchData.clear();
    }

  } catch (const SocketException& e) {
    std::cerr << "Socket exception occurred: " << e.what() << std::endl;
  } catch (const ClientException& e) {
    //无论被异常终止还是手动终止都表示接收完成
    std::cerr << "Server exception occurred: " << e.what() << std::endl;
  } catch (const std::system_error& e) {  //系统级别的错误
    std::cerr << "System error occurred: " << e.what() << std::endl;
  } catch (const std::exception& e) {  //其他标准异常
    std::cerr << "Exception occurred: " << e.what() << std::endl;
  } catch (...) {  //所有其他未知类型的异常
    std::cerr << "An unknown error occurred during sending data." << std::endl;
  }

  if (getRunnFlag(info.serverId) == false) {
    std::cout << "Recving has been stopped by setting isRunning" << std::endl;
  } else {
    std::cout << "Recving has been stopped by catching exception" << std::endl;
  }
  cleanup();
  _queueCondition.notify_all();  // 接收循环已经退出，通知等待线程，唤醒

  return nullptr;
}

void* Client::sendThreadFunction(void* arg) {
  //instance->sendDataToServer(arg);
  return nullptr;
}

void* Client::recvThreadFunction(void* arg) {
  instance->recvDataFromServer(arg);
  return nullptr;
}

void Client::start() {

  ThreadManager::printfCpuInfo();

  ThreadManager::setThreadAttributes(_sendAttr, _sendPriority, _stackSize);
  ThreadManager::setThreadAttributes(_recvAttr, _recvPriority, _stackSize);

  std::vector<pthread_t> sendThreads(_serverNum);
  std::vector<pthread_t> recvThreads(_serverNum);

  for (uint16_t i = 0; i < _serverNum; ++i) {
    printServerInfo(i);

    // auto sendArgs = std::make_tuple(std::ref(_serverInfos[i]));
    // if (pthread_create(&sendThreads[i], &_sendAttr, sendThreadFunction,
    //                    &sendArgs) != 0) {
    //   throw ClientException("Falied to create send thread for server " +
    //                         std::to_string(i));
    // }
    // ThreadManager::bindThreadToCore(sendThreads[i], _coreIds[i]);
    // ThreadManager::printfThreadInfo(sendThreads[i]);
    auto recvArgs = std::make_tuple(std::ref(_serverInfos[i]));
    // std::cout << server._serverInfos[i].localRecvPort << std::endl;

    if (pthread_create(&recvThreads[i], &_recvAttr, recvThreadFunction,
                       &recvArgs) != 0) {
      std::cerr << "Error creating thread: " << std::strerror(errno)
                << std::endl;
      throw ClientException("Falied to create recv thread for server  " +
                            std::to_string(i));
    }
    ThreadManager::bindThreadToCore(recvThreads[i], _coreIds[i]);
    ThreadManager::printfThreadInfo(recvThreads[i]);

    signalProcess.start(_isRunning[0]);
  }
}

void Client::updateSysctlConfig(const std::string& parameter,
                                const std::string& value) {
  std::ofstream configFile("/etc/sysctl.conf", std::ios::app);
  if (!configFile) {
    throw ClientException("Failed to open /etc/sysctl.conf for writing" +
                          std::string(strerror(errno)));
    return;
  }
  // 写入参数到文件末尾
  configFile << parameter << "=" << value << std::endl;
  configFile.close();

  // 应用更改，使其立即生效
  int result = system("sysctl -p");
  if (result != 0) {
    throw ClientException("Failed to reload sysctl configuration " +
                          std::string(strerror(errno)));
  } else {
    std::cout << "Sysctl configuration updated and reloaded successfully."
              << std::endl;
  }
}

void Client::display(unsigned char* buf, int start, int end) {}
void Client::convertTimestamp(uint64_t timestamp) {
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
