/**
 * @File Name: main.cpp
 * @brief  实现服务端和客户端在收发数据时的多线程通讯
 * @Author: 曾操老师硕士团队 email: 2023届负责人熊猛邮箱 2498941940@qq.com
 * @Version: V1.0.1.20241024
 * @Creat Date: 2024-10-24
 *
 * @copyright Copyright (c) 2024 雷达信号处理全国重点实验室
 *
 * modification history:
 * Date:       Version:      Author:
 * Changes:
 * 代码tag作用说明
 * TODO-未完成代码
 * FIXME-代码需要修正
 * HINT-提示
 * NOTE-记录代码作用
 * HACK-可能出现问题
 * BUG-这里有问题
 */
//sudo ifconfig eth0 mtu 9000

#include <iostream>
#include "Client.hpp"
#include "SignalProcess.hpp"
#include "Socket.hpp"
#include "ThreadManager.hpp"

Client* Client::instance = nullptr;
SignalProcess* SignalProcess ::instance = nullptr;
int Thread::generateId_ = 0;
/// bug:等待在条件上的线程怎么回收
int main() {
  try {
     ThreadManager::bindThreadToCore(pthread_self(), 0);  //主线程绑定核心
     Client client({{0, "172.29.183.243",Socket::TCP, 8011, 8003, 8001}});
     Client::instance = &client;  // 将实例指针传递给静态变量
     client.setPrintFlag(0, true);
     //std::cout << sizeof(UdpHeader) << std::endl;

     //std::cout << sizeof(uint64_t)+sizeof(uint16_t)*2 << std::endl;

       client.start(

       );
     getchar();
  } catch (const ClientException& e) {
    std::cerr << e.what() << '\n';
  } catch (const std::system_error& e) {  //系统级别的错误
    std::cerr << "System error occurred: " << e.what() << std::endl;
  } catch (const std::exception& e) {  //其他标准异常
    std::cerr << "Exception occurred: " << e.what() << std::endl;
  } catch (...) {  //所有其他未知类型的异常
    std::cerr << "An unknown error occurred during sending data." << std::endl;
  }
  //   std::vector<std::shared_ptr<unsigned char[]>> batchData;  //指针容器

  //
  //   std::this_thread::sleep_for(std::chrono::seconds(1));
  //   //std::cout << "Program exited normally." << std::endl;
  //   // 先用 new 动态分配内存
  //  unsigned char* rawMemory = new unsigned char[40];
  //   std::shared_ptr<unsigned char[]> managedMemory(rawMemory);

  //  // auto managedMemory = std::make_shared<unsigned char[]>(40);
  //   // 用智能指针 std::shared_ptr 管理这块内存
  //   // 初始化和使用
  //   for (size_t i = 0; i < 40; ++i) {
  //     managedMemory[i] = static_cast<unsigned char>(i % 256);
  //   }

  //   for (size_t i = 0; i < 40; ++i) {
  //     std::cout << static_cast<int>(managedMemory[i]) << " ";
  //   }
  //   batchData.push_back(std::move(managedMemory));

  //   std::cout << std::endl;

  // getchar();
  return 0;
}

