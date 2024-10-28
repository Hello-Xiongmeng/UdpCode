
#pragma once
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

constexpr int size = 1024;
class Client {
 public:

  /**
  * @brief  成员sendPort是服务端接收数据的端口，是指客户端要发送到客户端的端口
  */
  struct CommunicationInfo {
    std::string serverIp;
    uint16_t serverRecvPort;
    uint16_t localRecvPort;
  };

  Client() = default;

  /**
 * @brief  输出当前线程id，核心信息
 */
  void printfWorkInfo() {
    auto totalCores = std::thread::hardware_concurrency();
    auto nowCpuCore = sched_getcpu();
    std::cout << "The total number of cores is " << totalCores << std::endl;
    std::cout << "Main thread ID is " << std::this_thread::get_id()
              << " and it is running on core " << nowCpuCore << std::endl;
  }
  
  /**
 * @brief  将接收到的数据转换为大写
 * @param [入参] data: 
 * @return std::string: 
 */
  std::string processData(const char* data) {
    std::string processed(data);
    for (auto& c : processed) {
      c = toupper(c);
    }
    return processed;
  }

  /**
 * @brief  发送数据到服务端
 * @param [入参] data: 
 * @param [入参] serverInfo: 
 * @param [入参] sendSockfd: 
 * @param [入参] serverAddr: 
 * @param [入参] running: 
 */
  void sendDataToServer(const std::string& data,
                        const CommunicationInfo& serverInfo,
                        const int sendSockfd,
                        const struct sockaddr_in& serverAddr,
                        std::atomic<bool>& running) {
    ssize_t sent_bytes =
        sendto(sendSockfd, data.c_str(), data.size(), 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (sent_bytes < 0) {
      std::cerr << "Failed to send data to server at " << serverInfo.serverIp
                << ":" << serverInfo.serverRecvPort << " - " << strerror(errno)
                << std::endl;
    } else {
      std::cout << "Sent data " << data << " to client at "
                << serverInfo.serverIp << ":" << serverInfo.serverRecvPort
                << std::endl;
    }
  }

  /**
 * @brief  处理数据并发送
 * @param [入参] serverInfo: 
 * @param [入参] running: 
 */
  void processAndSendData(const CommunicationInfo& serverInfo,
                          std::atomic<bool>& running) {
    int sendSockfd = createSocketForPort(serverInfo.serverRecvPort);
    struct sockaddr_in serverAddr =
        createSockAddr(serverInfo.serverIp.c_str(), serverInfo.serverRecvPort);
    while (running) {
      std::unique_lock<std::mutex> lock(
          _queueMutex);  //创建lock对象，尝试获取 _queueMutex 的锁，获取之后才能访问
      _queueCondition.wait(
          lock, [this] { return !_dataQueue.empty(); });  // 等待直到队列有数据
      while (!_dataQueue.empty()) {
        std::string data = std::move(_dataQueue.front());
        _dataQueue.pop();
        std::string processedData = processData(data.c_str());  // 处理数据
        lock.unlock();  // 解锁，允许接收线程继续工作
        sendDataToServer(processedData, serverInfo, sendSockfd, serverAddr,
                         running);
        lock.lock();  // 重新获取锁
      }
    }
    close(sendSockfd);
  }

  /**
 * @brief  接收来自服务端的数据
 * @param [入参] localRecvPort: 
 * @param [入参] running: 
 */
  void recvDataFromServer(const uint16_t& localRecvPort,
                          std::atomic<bool>& running) {
    int recvSockfd = createSocketForPort(localRecvPort);
    struct sockaddr_in localClientAddr =
        createSockAddr("0.0.0.0", localRecvPort);

    if (bind(recvSockfd, (struct sockaddr*)&localClientAddr,
             sizeof(localClientAddr)) < 0) {
      std::cerr << "Bind failed on port " << ntohs(localClientAddr.sin_port)
                << std::endl;
      close(recvSockfd);
      return;
    }
    char buffer[size];
    while (running) {
      ssize_t n = recvfrom(recvSockfd, buffer, size - 1, 0, nullptr, nullptr);
      if (n > 0) {
        buffer[n] = '\0';  // 确保以 '\0' 结束
        std::cout << "Received from server: " << buffer << std::endl;
        std::lock_guard<std::mutex> lock(_queueMutex);
        _dataQueue.push(std::string(buffer));
        _queueCondition.notify_one();  //通知一个处理线程有新数据

      } else {
        std::cerr << "Failed to receive data from server." << std::endl;
      }
    }
    close(recvSockfd);
  }

  /**
 * @brief  监听控制函数，在命令行输入stop可以停止发送和接收的while (running)
 * 循环，从而结束程序运行
 * @param [入参] running: 
 */
  void controlInCommandline(std::atomic<bool>& running) {
    std::string command;
    while (running) {
      std::cout << "Enter 'stop' to stop the server" << std::endl;
      std::getline(std::cin, command);
      if (command == "stop") {
        running = false;
        std::cout << "Stopping server..." << std::endl;
        break;
      }
    }
  }

  /**
 * @brief  多线程收发
 * @param [入参] info: 
 * @param [入参] coreIds: 
 */
  void start(const CommunicationInfo& info,
             const std::vector<uint16_t>& coreIds) {
    if (coreIds.size() < 2) {
      std::cerr << "Error:The total number of input core IDs does not meet the "
                   "requirement"
                << std::endl;
      return;
    } else {
      std::cout << "The total number of input core IDs has met the needs for "
                   "sending and receiving data"
                << std::endl;
    }
    std::atomic<bool> running(true);
    std::thread recvThread([this, info, &running]() {
      recvDataFromServer(info.localRecvPort, running);
    });
    bindThreadToCore(recvThread, coreIds[0]);
    std::thread processThread(
        [this, info, &running]() { processAndSendData(info, running); });
    bindThreadToCore(processThread, coreIds[1]);

    std::thread controlThread(&Client::controlInCommandline, this,
                              std::ref(running));  // 创建命令行控制线程
    recvThread.join();
    processThread.join();
  }

 private:
  /**
 * @brief  创建一个网络通信通用结构体
 * @param [入参] ip: 输入通信ip
 * @param [入参] port: 输入通信端口
 * @return struct sockaddr_in: 返回结构体变量
 * @note  创建客户端的网络结构体信息时，string& ip输入客户端的ip地址，
 * 创建服务端网络结构体信息时，输入0.0.0.0监听所有可用的网卡IP
 */
  struct sockaddr_in createSockAddr(const std::string& ip,
                                    const uint16_t& port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));  // 将结构体置零
    addr.sin_family = AF_INET;       // 设置地址族为 IPv4
    addr.sin_port = htons(port);     // 转为网络字节序
    if (ip == "0.0.0.0" || ip.empty()) {
      addr.sin_addr.s_addr = INADDR_ANY;  // 监听所有接口
    } else {
      addr.sin_addr.s_addr = inet_addr(ip.c_str());  // 将IP地址转换为网络字节序
    }
    return addr;
  }

  /**
 * @brief  创建网络通信udp套接字
 * @param [入参] recvPort: 只是用作输出提示信息，recvPort并没有实际应用
 * @return int: 返回文件描述符
 */
  int createSocketForPort(const uint16_t& recvPort) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
      std::cerr << "Socket creation failed for receiving on port " << recvPort
                << std::endl;
      return -1;
    }
    return sockfd;
  }

  /**
 * @brief  将输入的线程对象绑定到输入的核心id
 * @param [入参] thread: 
 * @param [入参] coreId: 
 */
  void bindThreadToCore(std::thread& thread, const uint16_t& coreId) {
    pthread_t pthread = thread.native_handle();
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(coreId, &cpuset);
    int result = pthread_setaffinity_np(pthread, sizeof(cpu_set_t), &cpuset);
    if (result != 0) {
      std::cerr << "Error setting thread affinity: " << strerror(result)
                << std::endl;
    } else {
      std::cout << "Thread ID: " << thread.get_id() << " bound to core "
                << coreId << std::endl;
    }
  }

 private:
  std::queue<std::string> _dataQueue;
  std::mutex _queueMutex;  // 互斥量
  std::condition_variable _queueCondition;
};
