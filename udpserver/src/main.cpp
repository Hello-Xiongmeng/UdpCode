/**
 * @File Name: main.cpp
 * @brief  实现服务端和客户端在收发数据时的多线程通讯
 * @Author: 曾操老师硕士团队 email: 2023届负责人熊猛邮箱 2498941940@qq.com
 * @Version: V1.1.1.20241024
 * @Creat Date: 2024-11-4
 * 
 * @copyright Copyright (c) 2024 雷达信号处理全国重点实验室
 * 
 * modification history:修改了代码结构，增加了线程管理类
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
//#include <boost/format.hpp>
#include "ChangePrint.hpp"
#include "Socket.hpp"
#include "ThreadManager.hpp"
#include "Server.hpp"
enum ProcessingBoard {
  Board_1,  // 默认为 0
  Board_2,  // 默认为 1
  Board_3,  // 默认为 2
  Board_4,  // 默认为 3
  Board_5   // 默认为 4
};
// 初始化静态成员
Server* Server::instance = nullptr;

int main() {

  try {

    pthread_attr_t mainAttr;

    ThreadManager::bindThreadToCore(pthread_self(),
                                    0);  //主线程绑定核心,设置线程属性
    ThreadManager::setThreadAttributes(mainAttr, 90, 1024 * 1024);

    Server server({{0, "172.29.183.243 ",Socket::TCP, 8001, 8002, 8011}});
    Server::instance = &server;  // 将实例指针传递给静态变量
    server.setPrintFlag(0, true);
    server.start();
    printWithColor("green", "Program exited normally.");

    // 获取当前时间戳作为组序号
    // uint64_t timestamp =
    //     std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    //  std::cout << timestamp<< std::endl;

    //  server.convertTimestamp(timestamp);


// std::atomic<bool> test = true  ;

//   // 创建发送socket，根据协议类型选择TCP或UDP
//     Socket sendSocket(AF_INET, Socket::TCP, Socket::SEND, 0);
//     sendSocket.configureSocket(8002, test );
    
//     struct sockaddr_in clientAddr = sendSocket.createSockAddr("172.29.183.243 ",  8001);


    
    getchar();

  } catch (const ServerException& e) {
    std::cerr << e.what() << '\n';
  } catch (const std::system_error& e) {  //系统级别的错误
    std::cerr << "System error occurred: " << e.what() << std::endl;
  } catch (const std::exception& e) {  //其他标准异常
    std::cerr << "Exception occurred: " << e.what() << std::endl;
  } catch (...) {  //所有其他未知类型的异常
    std::cerr << "An unknown error occurred during sending data." << std::endl;
  }
  return 0;
}
