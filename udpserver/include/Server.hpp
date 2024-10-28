
#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <strings.h>
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
const int size = 1024;

class Server {
 public:
  /**
  * @brief  成员clientRecvPort是客户端接收数据的端口，是指服务端要发送到客户端的端口
  * 没有指定接收发送，服务器的发送端口由操作系统分配
  * FIXME:后续尝试指定端口发送数据，测试是否可以提升效率
  */
  struct CommunicationInfo {
    std::string clientIp;
    uint16_t clientRecvPort;
    uint16_t localRecvPort;
  };

  Server() = default;

  /**
 * @brief  输出当前线程id，核心信息
 */
  void printfWorkInfo() {
    auto totalCores = std::thread::hardware_concurrency();
    auto nowCpuCore = sched_getcpu();  // 获取当前 CPU 核心id
    std::cout << "The total number of cores is " << totalCores << std::endl;
    std::cout << "Main thread ID is " << std::this_thread::get_id()
              << " and it is running on core " << nowCpuCore << std::endl;
  }
  /**
 * @brief  发送数据到客户端
 * @param [入参] data: 发送的数据，注意使用const，函数内部就不会修改外部值
 * @param [入参] CommunicationInfo: 客户端的ip地址和客户端的接收端口，const同上
 * @param [入参] running: 多线程bool值默认使用原子类型即可，避免竞争出现
 * @note  服务器的发送端口由操作系统分配，想要指定需要手动绑定
 */
  void sendDataToClient(const std::string& data,
                        const CommunicationInfo& clientInfo,
                        std::atomic<bool>& running) {
    int sendSockfd = createSocketForPort(clientInfo.clientRecvPort);
    struct sockaddr_in clientAddr =
        createSockAddr(clientInfo.clientIp, clientInfo.clientRecvPort);
    while (running) {
      ssize_t sent_bytes =
          sendto(sendSockfd, data.c_str(), data.size(), 0,
                 (struct sockaddr*)&clientAddr, sizeof(clientAddr));
      if (sent_bytes < 0) {
        std::cerr << "Failed to send data to client at " << clientInfo.clientIp
                  << ":" << clientInfo.clientRecvPort << std::endl;
      } else {
        std::cout << "Sent data " << data << " to client at "
                  << clientInfo.clientIp << ":" << clientInfo.clientRecvPort
                  << std::endl;
      }
      std::this_thread::sleep_for(std::chrono::seconds(1));  // 控制发送频率
    }
    close(sendSockfd);
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
  void recvDataFromClient(const uint16_t& localRecvPort,
                          std::atomic<bool>& running) {
    int recvSockfd = createSocketForPort(localRecvPort);
    struct sockaddr_in localServerAddr =
        createSockAddr("0.0.0.0", localRecvPort);
    if (bind(recvSockfd, (struct sockaddr*)&localServerAddr,
             sizeof(localServerAddr)) < 0) {
      std::cerr << "Bind failed on port " << ntohs(localServerAddr.sin_port)
                << std::endl;
      close(recvSockfd);
      return;
    }
    char buffer[size];
    //FIXME这里是否不需要创建clientAddr，因为没有包含有用信息
    while (running) {
      ssize_t n = recvfrom(recvSockfd, buffer, size - 1, 0, nullptr, nullptr);
      if (n > 0) {
        buffer[n] = '\0';
        std::cout << "Received result from client: " << buffer << std::endl;
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
 * @param [入参] CommunicationInfos: 客户端的信息
 * @param [入参] coreIds: 核心id
 * @note  多线程编程中，在线程函数中传入外部参数时，直接使用引用类型（包括 const 引用和普通引用）是有风险的。
 * 这是因为线程的执行时间不确定，如果主线程退出或作用域结束，
 * 引用的变量也可能被销毁，导致子线程访问到无效的内存，
 * 最终引发未定义行为，如崩溃或 Segmentation Fault。
 *可以使用值传递，这样线程有自己的数据拷贝，避免引用外部变量。
 *也可以使用共享数据结构，如智能指针（例如 std::shared_ptr），保证对象的生命周期与线程一致。
 */
  void start(const std::vector<CommunicationInfo>& infos,
             const std::vector<uint16_t>& coreIds) {
    if (coreIds.size() < infos.size() * 2) {
      std::cerr << "Error:The total number of input core IDs does not meet the "
                   "requirement"
                << std::endl;
      return;
    } else {
      std::cout << "The total number of input core IDs has met the needs for "
                   "sending and receiving data"
                << std::endl;
    }
    std::vector<std::thread> sendThreads;
    std::vector<std::thread> recvThreads;
    std::atomic<bool> running(true);
    uint16_t coreIndex = 0;

    for (const auto& clientInfo : infos) {
      //HINT:lambda表达式中变量的值默认是被追加了const的，不允许被修改。
      //HINT:如果在lambda表达式中如果想对捕获列表中的变量进行修改，建议采用“引用 "指针"的方式。
      sendThreads.emplace_back([this, clientInfo, &running]() {
        std::string data = "hello";
        sendDataToClient(data, clientInfo, running);
      });  //NOTE:用emplace_back创建对象，lambda捕获this使用成员函数SendDataToClient
      bindThreadToCore(
          sendThreads.back(),
          coreIds[coreIndex]);  //NOTE:back()获取容器最后一个元素的引用
      recvThreads.emplace_back([this, clientInfo, &running]() {
        recvDataFromClient(clientInfo.localRecvPort, running);
      });
      bindThreadToCore(recvThreads.back(), coreIds[coreIndex]);
      coreIndex++;
    }

    //FIXME:首先这里的线程不需要绑定核心，不建议单独开辟一个线程实现控制功能，可能影响代码性能
    std::thread controlThread(&Server::controlInCommandline, this,
                              std::ref(running));  // 创建命令行控制线程

    for (auto& thread : sendThreads) {
      thread.join();
      for (auto& thread : recvThreads) {
        thread.join();
      }
    }  // 等待所有线程结束
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
};
