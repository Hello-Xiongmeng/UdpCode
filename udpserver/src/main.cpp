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
#include "Server.hpp"

int main() {

  std::vector<uint16_t> newCoreIds = {0, 1, 2, 3, 4, 5};
  Server server;
  server.printfWorkInfo();
  std::vector<Server::CommunicationInfo> infos = {
      {"172.24.228.100", 8001, 8011}};
  server.start(infos, newCoreIds);
  return 0;
}