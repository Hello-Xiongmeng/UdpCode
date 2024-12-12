#pragma once
#include <fftw3.h>
#include <Eigen/Dense>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>
#include <vector>
#
#include "ThreadPool.hpp"
#include "Socket.hpp"

#define BWin 1
#define SWin 1
#define C 3e8
#define RF 1
#define Lambda C / RF
#define Fs 1
#define Ts 1 / Fs
#define pi 3.1415926535
#define Pfa 1
#define th 1
#define th_db 1
#define CPI_Num 1

struct MyStruct {
  Eigen::VectorXf result_d_n;
  Eigen::MatrixXf result_Distance;
  Eigen::MatrixXf result_Vel;
};

// 模拟接收到的分片数据包
struct ReceivedPacket {
  Header header;
  std::vector<char> payload;  // 数据负载
};

// 假设的包头结构
struct prtHeader {
  uint64_t timeStamp = 0;  // 时间戳 (来自组序号)
  uint16_t pluseNum = 0;   // 总分片数
  uint16_t sampleNum = 0;  // 总分片数
};
// 模拟接收到的分片数据包
struct prtPacket {
  prtHeader header;
  Eigen::VectorXcf payload;  // 数据负载
};

class SignalProcess {
 private:
  /* data */
 public:
  SignalProcess(std::queue<std::vector<char>>& ptrQueue,
                std::mutex& queueMutex,
                std::condition_variable& queueCondition);

  ~SignalProcess();

  void start(std::atomic<bool>& processFlag);

  void processData(prtPacket&& prt);

  std::queue<std::vector<char>>& _consumerQueue;

  std::queue<prtPacket> checkMissingPackets(
      std::vector<ReceivedPacket>& recvPackets,
      std::vector<uint64_t>& missTimeFlag,
      std::queue<prtPacket>& validPackets);

  void convertTimestamp(uint64_t timestamp);

      std::mutex& _queueMutex;  // 使用 shared_ptr 管理 mutex
  std::condition_variable& _queueCondition;
  static SignalProcess* instance;

 private:
  ThreadPool pool;

 public:
  unsigned int sampleNumber_;
  unsigned int pulseNumber_;

  Eigen::VectorXcf chirpComplexout;
  Eigen::MatrixXcf pcResult_;
  Eigen::MatrixXcf wpcResult_;
  Eigen::MatrixXcf ypcResult_;
  Eigen::MatrixXcf slbpcResult_;
  Eigen::MatrixXcf mtdMatrix_;

  Eigen::MatrixXcf portectMtdMatrix_;

  Eigen::MatrixXcf wmtdMatrix;
  Eigen::MatrixXcf ymtdMatrix;

  Eigen::VectorXf ratiofw_Phase_s;
  Eigen::VectorXf ratiofy_Phase_s;

  float ratiofwPhase;
  float ratiofyPhase;

  unsigned int time_;

 public:
  fftwf_plan FFT;
  fftwf_plan FFTtest;
  fftwf_plan IFFT;
  fftwf_plan mtdFFT;
 

  //Eigen::VectorXcf pulseCompressionOfTime_;        // IFFT��ĸ�����ʽ
  void dataProcessReady(Eigen::VectorXcf chirpComplex);
  Eigen::VectorXcf pulseCompression(Eigen::VectorXcf echoComplex);
  Eigen::MatrixXcf dop(float PRF);  //����
  void w_y_slb_MtiAndMtd(unsigned int Cpi_Id);

  Eigen::VectorXcf mtiMultithreading(Eigen::VectorXcf row1,
                                     Eigen::VectorXcf row2,
                                     Eigen::VectorXcf row3);

  Eigen::VectorXcf mtd(Eigen::VectorXcf mtiMatrixcol);
  Eigen::MatrixXcf mtdWhole(Eigen::MatrixXcf mtiMatrix);
  std::vector<std::vector<float>> cfar2DWhole(
      Eigen::MatrixXcf mtdMatrix);  //����59*3260,����ֵ��������

  void cfar(int j);

  std::vector<std::vector<float>> cfarEnd();  ///j ��Χ��0-��pulseNumber_ -2��
  std::vector<std::vector<float>> cfarResult;
  std::vector<Eigen::MatrixXcf> wmtd;
  std::vector<Eigen::MatrixXcf> ymtd;
  std::vector<Eigen::MatrixXcf> slbResult;
  std::vector<Eigen::MatrixXcf> mtdMatrixvector;

  std::vector<int> potFilter(
      std::vector<std::vector<float>>
          cafrMatrix);  ///TODO �޸ı����� ̫�Ѷ�����
  MyStruct solveAmbiguity(std::vector<std::vector<float>> targ_result_dis,
                          std::vector<std::vector<float>> targ_result_vel);
  void SumDiffDOA(MyStruct Result, unsigned int Cpi_Id);

  void setSampleNumber(unsigned int sampleNumber);
  void setPulseNumber(unsigned int pulseNumber);
  void setPcFftLength(unsigned int fftLength);
  void setChirpNumber(unsigned int chirpNumber);
  void timeConsume(unsigned int time);

 private:
  fftwf_complex* in;
  fftwf_complex* out;
  //unsigned int echoSize_;
  unsigned int chirpNumber_;
  unsigned int fftLength_;
  std::vector<int> potFilterResult_;
  int n;
  Eigen::MatrixXcf Result_vec_;
  Eigen::MatrixXf Result_vec_moduls;
  Eigen::MatrixXcf data_select_;
  Eigen::MatrixXf Result_vec_db_;
  Eigen::MatrixXf targ_result_dis_;
  Eigen::MatrixXf targ_result_vel_;

  float num_cankao = (BWin * 2 + 1) * (BWin * 2 + 1) -
                     (SWin * 2 + 1) * (SWin * 2 + 1);  //9*9-5*5 = 56  ��set����
  float alpha = num_cankao * (pow(Pfa, (-1 / num_cankao)) - 1) * 3;

 private:
  std::vector<float> unique(std::vector<float> vefData);
  std::vector<float> unique(Eigen::VectorXf vefData);
  Eigen::VectorXf sort_vec(Eigen::VectorXf vec);
  Eigen::MatrixXf generateMatrix(Eigen::VectorXf vectorRow,
                                 Eigen::VectorXf vectorCol);
  Eigen::VectorXf sort_vec2(std::vector<float> vec,
                            Eigen::VectorXi& vdelete_id);
  Eigen::VectorXf sort_vec2(std::vector<float> vec);
  Eigen::VectorXf matrixfConvertVectorxf(Eigen::MatrixXf vec);
  Eigen::VectorXf Variance(Eigen::VectorXf vec, float n);
};
