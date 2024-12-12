
#include "ThreadManager.hpp"
#include <iostream>
#include <thread>
#include "ChangePrint.hpp"

#define DEBUG

void ThreadManager::setThreadAttributes(pthread_attr_t& attr, int priority,
                                        int stackSize) {
  struct sched_param _param;
  int ret;

  // 初始化线程属性
  ret = pthread_attr_init(&attr);
  if (ret != 0) {
    std::cerr << "Error initializing thread attributes: " << std::strerror(ret)
              << std::endl;
    return;
  }

  // 设置调度策略
  _param.sched_priority = priority;
  ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  if (ret != 0) {
    std::cerr << "Error setting scheduling policy: " << std::strerror(ret)
              << std::endl;
    return;
  }

  // 设置优先级
  ret = pthread_attr_setschedparam(&attr, &_param);
  if (ret != 0) {
    std::cerr << "Error setting scheduling parameter: " << std::strerror(ret)
              << std::endl;
    return;
  }

// 设置调度继承方式（在调试环境中跳过）,编译时没有 DEBUG 宏,正常执行，有编译时有 DEBUG 宏，编译时会跳过
#ifndef DEBUG
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  if (ret != 0) {
    std::cerr << "Error setting inherit scheduler attribute: "
              << std::strerror(ret) << std::endl;
    return;
  }
#endif

#ifdef DEBUG

printWithColor("red", "DEBUG: Not set inherit scheduler attribute");
  
#endif
  // 设置堆栈大小
  ret = pthread_attr_setstacksize(&attr, stackSize);
  if (ret != 0) {
    std::cerr << "Error setting stack size: " << std::strerror(ret)
              << std::endl;
    return;
  }

  //将线程设置为分离状态（如果需要）
  ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (ret != 0) {
      std::cerr << "Error setting detach state: " << std::strerror(ret) << std::endl;
      return;
  }
}
/**
 * @brief 
 * HINT:调用 pthread_attr_setschedparam 之前，必须先调用 pthread_attr_setschedpolicy。否则会报错
 * HINT:这是因为调度策略和优先级是相互关联的，不同的策略可能允许不同的优先级范围。
 */
void ThreadManager::setSendThread(pthread_attr_t& attr, int priority,
                                  int stackSize) {
  struct sched_param _param;
  // 初始化线程属性
  pthread_attr_init(&attr);
  // 设置调度策略
  _param.__sched_priority = priority;
  pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  // 设置优先级
  pthread_attr_setschedparam(&attr, &_param);
  // 设置调度继承方式
  pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  // 设置堆栈大小
  pthread_attr_setstacksize(&attr, stackSize);
  // 将线程设置为分离状态
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
}

/**
 * @brief  打印cpu信息
 */
void ThreadManager::printfCpuInfo() {
  auto totalCores = std::thread::hardware_concurrency();
  auto nowCpuCore = sched_getcpu();  // 获取当前 CPU 核心id
  std::cout << "The total number of cores is " << totalCores << std::endl;
  std::cout << "Main thread ID is " << std::this_thread::get_id()
            << " and it is running on core " << nowCpuCore << std::endl;
}
/**
   * @brief  打印线程配置信息
   * @param [入参] thread: 
   */
void ThreadManager::printfThreadInfo(pthread_t thread) {
  int policy;
  struct sched_param param;
  // 获取线程的调度参数
  ///FIXME:这里在多次调用之后会意外报错
  if (pthread_getschedparam(thread, &policy, &param) != 0) {
    std::cerr << "Error getting scheduling parameters: " << strerror(errno)
              << std::endl;
    return;
  }
  // 打印调度策略和优先级
  std::cout << "Thread Scheduling Policy: " << policy << std::endl;
  std::cout << "Thread Scheduling Priority: " << param.sched_priority
            << std::endl;
}

/**
 * @brief  将目前所在的线程对象绑定到输入的核心id
 * @param [入参] thread: 
 * @param [入参] coreId: 
 */
void ThreadManager::bindThreadToCore(pthread_t thread, const uint16_t& coreId) {
  //pthread_t pthread = pthread_self();
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(coreId, &cpuset);
  int result = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  if (result != 0) {
    std::cerr << "Error setting thread affinity: " << strerror(result)
              << std::endl;
  } else {
    std::cout << "Thread ID: " << thread << " bound to core " << coreId
              << std::endl;
  }
}

/**
 * @brief  监听控制函数，在命令行输入stop可以停止发送和接收的while (running)
 * 循环，从而结束程序运行
 * @param [入参] running: 
 */
void ThreadManager::controlInCommandline(std::atomic<bool>& running) {
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