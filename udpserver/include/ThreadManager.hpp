#pragma once
#include <pthread.h>
#include <atomic>
#include <cstring>


class ThreadManager {
 public:
  static void setThreadAttributes(pthread_attr_t& attr, int priority,
                                  int stackSize);
  static void setSendThread(pthread_attr_t& attr, int priority, int stackSize);

  static void printfCpuInfo();

  static void printfThreadInfo(pthread_t thread);

  static void bindThreadToCore(pthread_t thread, const uint16_t& coreId);

  static void controlInCommandline(std::atomic<bool>& running);
};
