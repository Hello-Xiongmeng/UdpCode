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
#include <Eigen/Dense>
#include "Socket.hpp"
#include "ThreadManager.hpp"
#include "Server.hpp"
#include "ReadData.hpp"  // Include the header file for Data_read

#define fftLength 4096

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
//192.168.6.2
    Server server({{0, "172.24.175.169 ", Socket::TCP, 8001, 8002, 8011}});
    Server::instance = &server;  // 将实例指针传递给静态变量
    server.setPrintFlag(0, true);
    server.start();

    ReadData a;
    Eigen::VectorXf TotalNumber = a.readData("TotalNumber.dat");
    Eigen::VectorXf SamepleNumber = a.readData("SampleNumber.dat");
    Eigen::VectorXf phi2 = a.readData("phi2.dat");
    Eigen::VectorXf K_Phasefw = a.readData("K_Phasefw.dat");
    Eigen::VectorXf K_Phasefy = a.readData("K_Phasefy.dat");
    Eigen::VectorXcf Chirp = a.readData("Chirp_real.dat", "Chirp_imag.dat");
    Eigen::VectorXcf data_Xs = a.readData("Xs_real.dat", "Xs_imag.dat");
    Eigen::VectorXcf data_Xs_w = a.readData("Xfw_real.dat", "Xfw_imag.dat");
    Eigen::VectorXcf data_Xs_y = a.readData("Xfy_real.dat", "Xfy_imag.dat");
    Eigen::VectorXcf data_Xs_slb =
        a.readData("Xs_sub_real.dat", "Xs_sub_imag.dat");
    std::vector<Eigen::MatrixXcf> cpiVector, cpiVector_w, cpiVector_y,
        cpiVector_slb;

    unsigned int TotalNumberCount = 0;
    for (size_t i = 0; i < TotalNumber.size(); i++) {

      Eigen::MatrixXcf echoMatrix((int)(TotalNumber[i] / SamepleNumber[i]),
                                  (int)(SamepleNumber[i]));

      Eigen::MatrixXcf echoMatrix_w((int)(TotalNumber[i] / SamepleNumber[i]),
                                    (int)(SamepleNumber[i]));

      Eigen::MatrixXcf echoMatrix_y((int)(TotalNumber[i] / SamepleNumber[i]),
                                    (int)(SamepleNumber[i]));
      
      Eigen::MatrixXcf echoMatrix_slb((int)(TotalNumber[i] / SamepleNumber[i]),
                                      (int)(SamepleNumber[i]));
      
      for (int k = 0; k < echoMatrix.rows(); k++) {
        echoMatrix.row(k) = data_Xs.segment(
            SamepleNumber[i] * k + TotalNumberCount, SamepleNumber[i]);
        echoMatrix_w.row(k) = data_Xs_w.segment(
            SamepleNumber[i] * k + TotalNumberCount, SamepleNumber[i]);
        echoMatrix_y.row(k) = data_Xs_y.segment(
            SamepleNumber[i] * k + TotalNumberCount, SamepleNumber[i]);
        echoMatrix_slb.row(k) = data_Xs_slb.segment(
            SamepleNumber[i] * k + TotalNumberCount, SamepleNumber[i]);
      }
      TotalNumberCount += TotalNumber[i];
      cpiVector.push_back(echoMatrix);
      cpiVector_w.push_back(echoMatrix_w);
      cpiVector_y.push_back(echoMatrix_y);
      cpiVector_slb.push_back(echoMatrix_slb);
    }

    std::vector<std::vector<float>> targ_result_dis(4);
    std::vector<std::vector<float>> targ_result_vel(4);

    Eigen::Vector4f PRT = {163 * 1e-6, 151 * 1e-6, 135 * 1e-6, 127 * 1e-6};
    Eigen::Vector4f PRF = 1 / PRT.array();

    // DataProcess operationCpi(fftLength);

    
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
