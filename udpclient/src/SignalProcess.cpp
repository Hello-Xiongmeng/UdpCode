
#include "SignalProcess.hpp"
#include <iomanip>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include "ChangePrint.hpp"

const int THREAD_POOL_NUM = 12;  // 单位：秒

SignalProcess::SignalProcess(std::queue<std::vector<char>>& consumerQueue,
                             std::mutex& queueMutex,
                             std::condition_variable& queueCondition)
    : _consumerQueue(consumerQueue),
      _queueMutex(queueMutex),
      _queueCondition(queueCondition) {

  pool.setMode(PoolMode::MODE_FIXED);

  pool.start(THREAD_POOL_NUM);

  FFT = fftwf_plan_dft_1d(fftLength_, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  IFFT = fftwf_plan_dft_1d(fftLength_, in, out, FFTW_BACKWARD, FFTW_ESTIMATE);

}

SignalProcess::~SignalProcess() {}

void SignalProcess::start(std::atomic<bool>& isRunningFlag) {



  std::vector<ReceivedPacket> recvPackets;
  std::queue<prtPacket> prtPackets;

  std::vector<uint64_t> missTimeFlag;

  while (isRunningFlag) {

    {
      std::unique_lock<std::mutex> lock(_queueMutex);

      _queueCondition.wait(lock, [this, &isRunningFlag] {
        return !_consumerQueue.empty() || !isRunningFlag;
      });

      // 等待直到队列有数据
      // 如果队列为空且退出标志为 false，跳出循环
      if (_consumerQueue.empty() && !isRunningFlag) {

        break;
      }
      while (!_consumerQueue.empty()) {
        // 检查数据是否足够
        // if (rawData.size() < sizeof(UdpHeader)) {
        //   std::cerr << "Error: Received data is too small to contain a header."
        //             << std::endl;
        //   return;
        // }
        // 解析数据
        // 解析包头到 header

        ReceivedPacket packet;
        packet.payload = std::move(_consumerQueue.front());
        _consumerQueue.pop();
        recvPackets.push_back(std::move(packet));
      }
    }

    checkMissingPackets(recvPackets, missTimeFlag,
                        prtPackets);  //记录丢包得时间戳直接丢掉

    //函数执行结束
    recvPackets.clear();

    if (prtPackets.size() >= THREAD_POOL_NUM) {
      while (!prtPackets.empty()) {

        pool.submitTask([this, data = std::move(prtPackets.front())]() mutable {
          this->processData(std::move(data));
        });
        prtPackets.pop();
      }
    }
  }
}

void SignalProcess::processData(prtPacket&& prt) {

 // convertTimestamp(prt.header.timeStamp);
  // for (int i = 0; i < 10; i++) {
  //   std::cout << prt.payload[i] << " , ";
  //     }
  //     std::cout << "\n";

}

// +------------------+--------------------+
// |   UdpHeader      |     数据部分       |
// +------------------+--------------------+

std::queue<prtPacket> SignalProcess::checkMissingPackets(
    std::vector<ReceivedPacket>& recvPackets,
    std::vector<uint64_t>& missTimeFlag, std::queue<prtPacket>& prtPackets) {

  // 按时间戳（组序号）将数据包分类
  std::unordered_map<uint64_t, std::unordered_set<uint16_t>>
      groupPackets;  // 时间戳--分片集合
  std::unordered_map<uint64_t, uint16_t> totalFragmentsMap;  // 时间戳--总包数

  for (auto& packet : recvPackets) {
    // 提取包头
    memcpy(&packet.header, packet.payload.data(), sizeof(UdpHeader));
    packet.payload.erase(packet.payload.begin(),
                         packet.payload.begin() + sizeof(UdpHeader));
    groupPackets[packet.header.timeStamp].insert(packet.header.sequence);
    //每个分片序号插入到对应的时间戳对应的集合,如果有多组数据（多组时间戳），会插入新的键值对
    // 只有当总包数发生变化时，才更新 totalFragmentsMap，//只有当时间戳变化时插入新的键，在总包数变化时插入新的值
    if (totalFragmentsMap[packet.header.timeStamp] != packet.header.total) {
      totalFragmentsMap[packet.header.timeStamp] = packet.header.total;  //
    }
  }

  // 检查每个时间戳的数据包是否丢失
  for (const auto& [timestamp, sequences] : groupPackets) {
    uint16_t totalFragments = totalFragmentsMap[timestamp];

    bool missing = false;

    // 检查是否丢失了任何数据包
    for (uint16_t i = 0; i < totalFragments; ++i) {
      if (sequences.find(i) == sequences.end()) {
        // 丢包
        missing = true;
        missTimeFlag.push_back(timestamp);
        break;  // 一旦发现丢包，就跳出循环，忽略该时间戳的数据
      }
    }

    if (missing) {
      convertTimestamp(timestamp);

      printWithColor("red", "Missing packets for : ");
          //输出丢失的包的序号
          for (uint16_t i = 0; i < totalFragments; ++i) {
        if (sequences.find(i) == sequences.end()) {
          printWithColor("red", i, " ");
        }
      }
    } else {
      // 没有丢包，开始拼接有效数据
      Eigen::VectorXcf fullPayloadEigen;
      int length = 0;

      // 计算有效数据的总复数个数
      for (auto& packet : recvPackets) {
        //确定是没丢包的时间戳对应的数据包
        if (packet.header.timeStamp == timestamp) {
          length += packet.payload.size() /
                    (2 * sizeof(float));  // 每两个浮动数对应一个复数
        }
      }

      // 预分配空间
      fullPayloadEigen.resize(length);

      size_t idx = 0;  // 复数数据的索引
                       // 再次遍历数据包并填充复数数据
      int j = 0;
      for (auto& packet : recvPackets) {
        if (packet.header.timeStamp == timestamp) {
    
          float* payloadPtr = reinterpret_cast<float*>(packet.payload.data());

          // 填充复数数据
          for (size_t i = 0; i < packet.payload.size() / (2 * sizeof(float));
               ++i) {
            // 存入 Eigen 容器
            fullPayloadEigen[idx++] =
                std::complex<float>(payloadPtr[i * 2], payloadPtr[i * 2 + 1]);
        
          }
        }
      }

      // 将最终数据保存到有效包中
      prtPacket validPacket;
      validPacket.header.timeStamp = timestamp;
      validPacket.payload = std::move(fullPayloadEigen);
      prtPackets.push(std::move(validPacket));
    }
  }

  return prtPackets;  //丢包时这里可能为空
}
void SignalProcess::convertTimestamp(uint64_t timestamp) {
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

// +------------------+--------------------+------------------+--------------------+
void SignalProcess::setSampleNumber(unsigned int sampleNumber) {
  sampleNumber_ = sampleNumber;
}

void SignalProcess::setPulseNumber(unsigned int pulseNumber) {
  pulseNumber_ = pulseNumber;
}

void SignalProcess::setPcFftLength(unsigned int fftLength) {
  fftLength_ = fftLength;
}

void SignalProcess::setChirpNumber(unsigned int chirpNumber) {
  chirpNumber_ = chirpNumber;
}

void SignalProcess::timeConsume(unsigned int time) {
  time_ += time;
}

/**对向量进行排序，从大到小
 * vec: 待排序的向量
 * sorted_vec: 排序的结果
 * ind: 排序结果中各个元素在原始向量的位置
 */
Eigen::VectorXf SignalProcess::sort_vec(Eigen::VectorXf vec) {
  Eigen::VectorXf sorted_vec;
  if (vec.size() != 0) {
    Eigen::VectorXi ind;
    ind = Eigen::VectorXi::LinSpaced(vec.size(), 0,
                                     vec.size() - 1);  //[0 1 2 3 ... N-1]
    auto rule = [vec](int i, int j) -> bool {
      return vec(i) < vec(j);
    };  //正则表达式，作为sort的谓词
    std::sort(ind.data(), ind.data() + ind.size(), rule);
    //data成员函数返回VectorXd的第一个元素的指针，类似于begin()
    sorted_vec.resize(vec.size());
    for (int i = 0; i < vec.size(); i++) {
      sorted_vec(i) = vec(ind(i));
    }
  }

  return sorted_vec;
}

Eigen::MatrixXf SignalProcess::generateMatrix(Eigen::VectorXf vectorRow,
                                            Eigen::VectorXf vectorCol) {
  int colNum = vectorRow.size();  //行向量的尺寸作为生成矩阵的列数 60
  int rowNum = vectorCol.size();  //列向量的尺寸作为生成矩阵的行数 4

  Eigen::MatrixXf resultMatrix(rowNum, colNum);

  for (size_t i = 0; i < rowNum; i++) {
    for (size_t j = 0; j < colNum; j++) {
      resultMatrix(i, j) = vectorRow(j) * vectorCol(i);
    }
  }
  return resultMatrix;
}

Eigen::VectorXf SignalProcess::sort_vec2(
    std::vector<float> vec,
    Eigen::VectorXi& vdelete_id)  //对vector排序返回VectorXf和索引
{
  Eigen::VectorXf sorted_vec;
  if (vec.size() != 0) {
    vdelete_id = Eigen::VectorXi::LinSpaced(
        vec.size(), 0, vec.size() - 1);  //[0 1 2 3 ... N-1]
    auto rule = [vec](int i, int j) -> bool {
      return vec[i] < vec[j];
    };  //正则表达式，作为sort的谓词
    std::sort(vdelete_id.data(), vdelete_id.data() + vdelete_id.size(), rule);
    //data成员函数返回VectorXd的第一个元素的指针，类似于begin()
    sorted_vec.resize(vec.size());
    for (int i = 0; i < vec.size(); i++) {
      sorted_vec(i) = vec[vdelete_id(i)];
    }
  }

  return sorted_vec;
}

Eigen::VectorXf SignalProcess::sort_vec2(
    std::vector<float> vec)  //对vector排序返回VectorXf和索引
{
  Eigen::VectorXf sorted_vec;
  if (vec.size() != 0) {
    Eigen::VectorXi vdelete_id = Eigen::VectorXi::LinSpaced(
        vec.size(), 0, vec.size() - 1);  //[0 1 2 3 ... N-1]
    auto rule = [vec](int i, int j) -> bool {
      return vec[i] < vec[j];
    };  //正则表达式，作为sort的谓词
    std::sort(vdelete_id.data(), vdelete_id.data() + vdelete_id.size(), rule);
    //data成员函数返回VectorXd的第一个元素的指针，类似于begin()
    sorted_vec.resize(vec.size());
    for (int i = 0; i < vec.size(); i++) {
      sorted_vec(i) = vec[vdelete_id(i)];
    }
  }

  return sorted_vec;
}
//Map 也可以切片和重塑，用法很多
Eigen::VectorXf SignalProcess::matrixfConvertVectorxf(
    Eigen::MatrixXf vec)  //把输入的矩阵转换成向量，可以列向量也可以行向量
{
  Eigen::Map<Eigen::VectorXf> v1(
      vec.data(),
      vec.size());  //将已有的vec矩阵从首地址到结束，直接映射成VectorXf（列）的v1向量
  //std::cout << "v1:" << std:: endl << v1 << std::endl;

  //Eigen::Matrix<float, Dynamic, Dynamic, RowMajor> M2(M1);
  //Map<RowVectorXf> v2(M2.data(), M2.size());
  //cout << "v2:" << endl << v2 << endl;
  return v1;
}

Eigen::VectorXf SignalProcess::Variance(
    Eigen::VectorXf vec,
    float n)  //对vec的元素每n个值做方差，直接返回方差后的向量
{

  if (n > 0) {
    int length = vec.size() - n + 1;
    Eigen::VectorXf varianceResult(length);
    float meanResult = 0.0;
    float varianceBuf = 0.0;
    for (size_t i = 0; i < length; i++) {
      for (size_t j = 0; j < n; j++) {
        meanResult += vec(i + j) / n;
      }
      for (size_t j = 0; j < n; j++) {
        varianceBuf +=
            (vec(i + j) - meanResult) * (vec(i + j) - meanResult) / n;
      }

      varianceResult(i) = varianceBuf;
      meanResult = 0;
      varianceBuf = 0;
    }
    return varianceResult;
  } else {
    Eigen::VectorXf errorresult =
        Eigen::VectorXf::Zero(1);  // 定义固定尺寸的零矩阵后面就不用补零了

    std::cerr << "Variance size error!" << std::endl;
    return errorresult;
  }

}  //220

std::vector<float> SignalProcess::unique(std::vector<float> vefData) {
  std::vector<float> uniqueValue;
  if (vefData.size() != 0) {
    int i = 0, j = 0;
    int length = vefData.size();

    //	std::cout << cfarResult.row(posResult[i]) << std::endl;
    uniqueValue.push_back(
        vefData
            [0]);  //把第一列的第一个的值存起来作为初始化的值，因为使用++j上去判断的是第2个元素，想要比较第一个元素要提前存起来
    while (i < length && j < (length - 1))  //因为下面是++j所以j不能越界
    {
      if (vefData[i] != vefData[++j]) {
        if (vefData[j] != 0) {
          i = j;  //因为矩阵已经按顺序排列了使用++j向下索引，只要值不相等就记录j的位置
          uniqueValue.push_back(vefData[i]);  //把第一列对应位置的元素存起来
        }
      }
    }
  }

  return uniqueValue;
}

std::vector<float> SignalProcess::unique(Eigen::VectorXf vefData) {
  std::vector<float> uniqueValue;
  if (vefData.size() != 0) {
    int i = 0, j = 0;
    int length = vefData.size();
    //	std::cout << cafrMatrix.row(posResult[i]) << std::endl;
    uniqueValue.push_back(vefData(
        0));  //把第一列的第一个的值存起来作为初始化的值，因为使用++j上去判断的是第2个元素，想要比较第一个元素要提前存起来
    while (i < length && j < (length - 1))  //因为下面是++j所以j不能越界
    {
      if (vefData(i) != vefData(++j)) {
        if (vefData(j) != 0) {
          i = j;  //因为矩阵已经按顺序排列了使用++j向下索引，只要值不相等就记录j的位置
          uniqueValue.push_back(vefData(i));  //把第一列对应位置的元素存起来
        }
      }
    }
  }

  return uniqueValue;
}

void RemoveRow(Eigen::MatrixXcf& matrix, unsigned int rowToRemove) {
  unsigned int numRows = matrix.rows() - 1;
  unsigned int numCols = matrix.cols();

  if (rowToRemove < numRows) {
    matrix.block(rowToRemove, 0, numRows - rowToRemove, numCols) =
        matrix.block(rowToRemove + 1, 0, numRows - rowToRemove, numCols);
  }
  matrix.conservativeResize(numRows, numCols);
}

void RemoveColumn(Eigen::MatrixXcf& matrix, unsigned int colToRemove) {
  unsigned int numRows = matrix.rows();
  unsigned int numCols = matrix.cols() - 1;

  if (colToRemove < numCols) {
    matrix.block(0, colToRemove, numRows, numCols - colToRemove) =
        matrix.block(0, colToRemove + 1, numRows, numCols - colToRemove);
  }

  matrix.conservativeResize(numRows, numCols);
}

void SignalProcess::dataProcessReady(
    Eigen::VectorXcf chirpComplex)  //传入59*3260,门限值，检测概率
{
  /// TODO:那些矩阵需要赋值全零那些矩阵不需要赋值全零
  pcResult_ = Eigen::MatrixXcf::Zero(pulseNumber_, sampleNumber_);
  mtdMatrix_ = Eigen::MatrixXcf::Zero(pulseNumber_ - 2, sampleNumber_);

  portectMtdMatrix_ =
      Eigen::MatrixXcf::Zero(pulseNumber_ + BWin * 2, sampleNumber_ + BWin * 2);
  Result_vec_ =
      Eigen::MatrixXcf::Zero(pulseNumber_ + BWin * 2, sampleNumber_ + BWin * 2);
  Result_vec_db_ =
      Eigen::MatrixXf::Zero(pulseNumber_ + BWin * 2, sampleNumber_ + BWin * 2);
  data_select_ = Eigen::MatrixXcf::Zero(pulseNumber_, sampleNumber_);

  mtdFFT =
      fftwf_plan_dft_1d(pulseNumber_ - 2, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

  potFilterResult_.clear();
  cfarResult.clear();
  //slbResult.clear();

  chirpComplexout = Eigen::VectorXcf::Zero(fftLength_);  // 频域相乘后的复数形式

  Eigen::VectorXcf chirpComplex_ = Eigen::VectorXcf::Zero(fftLength_);
  chirpComplex_.head(chirpNumber_) = chirpComplex;
  chirpComplex_.reverseInPlace();  // 原地倒序函数
  chirpComplex_
      .adjointInPlace();  // 这个是原地共轭转置(伴随矩阵)，一列数据共轭转置相当于只有共轭
  fftwf_execute_dft(FFT, (fftwf_complex*)chirpComplex_.data(),
                    (fftwf_complex*)chirpComplexout.data());

  n = ceil((pulseNumber_ - 2) / 2.0);
}

Eigen::VectorXcf SignalProcess::pulseCompression(Eigen::VectorXcf echoComplex) {
  Eigen::VectorXcf echoComplex_ = Eigen::VectorXcf::Zero(
      fftLength_);  // 定义固定尺寸的零矩阵后面就不用补零了
  echoComplex_.head(sampleNumber_) = echoComplex;
  //memcpy(echoComplex_.data(), echoComplex.data(), sizeof(echoComplex));

  Eigen::VectorXcf pulseCompressionOfTime(fftLength_);  // 频域相乘后的复数形式
  Eigen::VectorXcf echoComplexout(fftLength_);  // 频域相乘后的复数形式

  fftwf_execute_dft(
      FFT, (fftwf_complex*)echoComplex_.data(),
      (fftwf_complex*)echoComplexout
          .data());  //多线程中不能直接用echoComplex_变换输出直接覆盖echoComplex_，会出问题，单线程可以

  Eigen::VectorXcf pulseCompressionIfft = echoComplexout.cwiseProduct(
      chirpComplexout);  /// Note:这两条函数都是对应点相乘

  fftwf_execute_dft(IFFT, (fftwf_complex*)pulseCompressionIfft.data(),
                    (fftwf_complex*)pulseCompressionOfTime.data());
  return (pulseCompressionOfTime / fftLength_)
      .head(sampleNumber_);  ///能不能用指针直接拷贝到目标位置
}

Eigen::MatrixXcf SignalProcess::dop(float PRF)  //估计
{
  //auto start1 = std::chrono::system_clock::now();
  Eigen::MatrixXcf mtiMatrix(pulseNumber_, sampleNumber_);
  Eigen::VectorXcf fdcomplex(sampleNumber_);
  Eigen::VectorXf equalNum = Eigen::VectorXf::LinSpaced(
      sampleNumber_ * pulseNumber_, 0, sampleNumber_ * pulseNumber_ - 1);
  std::complex<float> mean;
  for (int i = 0; i < sampleNumber_; i++) {
    mean = (pcResult_.col(i).tail(pulseNumber_ - 1).array() *
            pcResult_.col(i).head(pulseNumber_ - 1).conjugate().array())
               .mean();
    //fd(i) = atan(mean.imag() / mean.real()) * PRF / (2 * pi);
    fdcomplex(i) = mean;
  }
  Eigen::VectorXf fd = fdcomplex.cwiseArg() * PRF / (2 * pi);
  float fdcenter = sort_vec(fd)(sampleNumber_ / 2);
  Eigen::MatrixXf Modufactor =
      Eigen::Map<Eigen::MatrixXf>(equalNum.data(), sampleNumber_, pulseNumber_)
          .transpose() *
      -2 * pi * Ts * fdcenter;
  //Eigen::MatrixXf asd = Eigen::Map<Eigen::Matrix<float, 61, 3260, Eigen::StorageOptions::RowMajor>>(equalNum.data()) * -2 * pi * Ts * 0.0421556;
  mtiMatrix.real() = Modufactor.array().cos();
  mtiMatrix.imag() = Modufactor.array().sin();

  mtiMatrix = pcResult_.cwiseProduct(mtiMatrix);
  //auto end1 = std::chrono::system_clock::now(); auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1); timeConsume(elapsed1.count());
  //std::cout << "Dop执行完成,耗时：" << elapsed1.count() << "ms" << '\n';
  //return  mtiMatrix.topRows(pulseNumber_ - 2);//因为直接是用mtiMatrix直接覆盖原来的值，有两行没有用
  return mtiMatrix;
}
/// 不用每次传入Eigen::VectorXcf  row1, Eigen::VectorXcf  row2, Eigen::VectorXcf  row3)传入索引值即可告诉行数
Eigen::VectorXcf SignalProcess::mtiMultithreading(Eigen::VectorXcf row1,
                                                Eigen::VectorXcf row2,
                                                Eigen::VectorXcf row3) {
  return row3 + row1 - row2 * 2;
}

Eigen::MatrixXcf SignalProcess::mtdWhole(
    Eigen::MatrixXcf
        mtiMatrix)  ///如果行数较多可能多线程更快，也可以使用fftw many  使用fftw many 结合多线程应该更更快
///mtd 分块处理 把3260分成多少份取多线程
{
  //auto start1 = std::chrono::system_clock::now();
  Eigen::MatrixXcf mtdMatrixbuf(pulseNumber_ - 2, sampleNumber_);
  int fft_lenth = pulseNumber_ - 2;  //单信号长度
  mtdFFT = fftwf_plan_many_dft(
      1, &fft_lenth, sampleNumber_, (fftwf_complex*)mtiMatrix.data(), NULL, 1,
      fft_lenth, (fftwf_complex*)mtdMatrixbuf.data(), NULL, 1, fft_lenth,
      FFTW_FORWARD, FFTW_ESTIMATE);  //FFTW_MEASURE
  fftwf_execute(mtdFFT);

  mtdMatrix_ << mtdMatrixbuf.bottomRows(pulseNumber_ - 2 - n),
      mtdMatrixbuf.topRows(n);
  portectMtdMatrix_.block(BWin, BWin, mtdMatrix_.rows(), mtdMatrix_.cols()) =
      mtdMatrix_.array().square();
  //portectMtdMatrix_.block<mtdMatrix.rows(), mtdMatrix.cols()>(BWin, BWin);
  //auto end1 = std::chrono::system_clock::now(); auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1); timeConsume(elapsed1.count());
  //std::cout << "MTD执行完成,耗时：" << elapsed1.count() << "ms" << '\n';
  return mtdMatrix_;
  /*@fftwf_plan_many_dft 说明
fftw_plan fftw_plan_many_dft_r2c(int rank,           // rank=m，指m维fft
							 const int *n,           // 每一组fft的数据个数。
							 int howmany,            // 同时进行几组fft
							 double* in,             // 保存所有需要进行fft的数据，eigen默认是列优先
							 const int *inembed,	//给1
							 int istride,            // 同组fft中两个元素之间的距离。如果istride=1，则这组fft的数组的内存是连续的，下面数据假设是5行10列
							 int idist,              // 相邻两组fft之间的距离。**eigen默认是列优先，按列处理istride和idist配合使用 istride=1 （每组数据之间连续） idist=5（这里给每组数据的长度，按列处理直接给行数，因为一列就是一组数据）  表示内存中每连续5个数据是待处理的fft数据
													 // 相邻两组fft之间的距离。**eigen默认是列优先，按行处理istride和idist配合使用 istride=5（这里给行数，每隔5个内存取一个）  idist=1（这里给每组数据的长度）  按列存储时，取每行数据
							 fftw_complex *out,      // 保存fft结果的数组
							 const int *onembed,
							 int ostride,			 //和istride idist作用相同，按行按列同上，如果主动设置行优先和列优先数据处理更快
							 int odist,
							 unsigned sign
							 unsigned flags);*/
}

Eigen::VectorXcf SignalProcess::mtd(Eigen::VectorXcf mtiMatrixcol) {
  Eigen::VectorXcf mtdAdjust(pulseNumber_ - 2);
  fftwf_execute_dft(mtdFFT, (fftwf_complex*)mtiMatrixcol.data(),
                    (fftwf_complex*)mtiMatrixcol.data());
  mtdAdjust << mtiMatrixcol.segment(n, pulseNumber_ - 2 - n),
      mtiMatrixcol.segment(0, n);
  return mtdAdjust;
}

std::vector<std::vector<float>> SignalProcess::cfar2DWhole(
    Eigen::MatrixXcf mtdMatrix)  //传入59*3260,门限值，检测概率
{
  Eigen::MatrixXcf portectMtdMatrix =
      Eigen::MatrixXcf::Zero(pulseNumber_ + BWin * 2, sampleNumber_ + BWin * 2);
  Eigen::MatrixXcf Result_vec =
      Eigen::MatrixXcf::Zero(pulseNumber_ + BWin * 2, sampleNumber_ + BWin * 2);
  Eigen::MatrixXcf data_select =
      Eigen::MatrixXcf::Zero(pulseNumber_, sampleNumber_);
  Eigen::MatrixXf Result_vec_db =
      Eigen::MatrixXf::Zero(pulseNumber_ + BWin * 2, sampleNumber_ + BWin * 2);
  std::vector<std::vector<float>> cfarResult(4);
  portectMtdMatrix.block(BWin, BWin, mtdMatrix.rows(), mtdMatrix.cols()) =
      mtdMatrix.array().square();  //

  //auto start2 = std::chrono::system_clock::now();

  for (size_t j = 0; j < pulseNumber_ - 2; j++) {
    for (size_t i = 0; i < sampleNumber_; i++) {
      data_select(j, i) =
          (portectMtdMatrix.block<2 * BWin + 1, 2 * BWin + 1>(j, i).sum() -
           portectMtdMatrix.block<2 * SWin + 1, 2 * SWin + 1>(2 + j, SWin + i)
               .sum()) /
          num_cankao;  //取出9*9的块的和减去5*5的块的和
      data_select(j, i) = data_select(j, i) * alpha;
      if (portectMtdMatrix(j + BWin, i + BWin).real() >=
          data_select(j, i).real()) {
        Result_vec(j, i) = portectMtdMatrix(j + BWin, i + BWin);
      }
    }
  }

  float maxResult = Result_vec.cwiseAbs().maxCoeff();

  Result_vec_db = 20 * (Result_vec.cwiseAbs() / maxResult).array().log10();

  for (size_t i = 0; i < pulseNumber_ + BWin * 2; i++) {
    for (size_t j = 0; j < sampleNumber_ + BWin * 2; j++) {
      if (Result_vec_db(i, j) > th_db) {
        cfarResult[0].push_back(i);
        cfarResult[1].push_back(j);
        cfarResult[2].push_back(Result_vec(i, j).real());
        cfarResult[3].push_back(Result_vec(i, j).imag());
      }
    }
  }
  //  auto end2 = std::chrono::system_clock::now(); auto elapsed2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2); std::cout << elapsed2.count() << "ms" << '\n';
  return cfarResult;
}

void SignalProcess::cfar(int j)  ///j 范围是0-（pulseNumber_ -2）
{
  //auto start2 = std::chrono::system_clock::now();
  for (size_t i = 0; i < sampleNumber_; i++) {
    data_select_(j, i) =
        alpha *
        (portectMtdMatrix_.block<2 * BWin + 1, 2 * BWin + 1>(j, i).sum() -
         portectMtdMatrix_.block<2 * SWin + 1, 2 * SWin + 1>(2 + j, SWin + i)
             .sum()) /
        num_cankao;  //取出9*9的块的和减去5*5的块的和
    if (portectMtdMatrix_(j + BWin, i + BWin).real() >=
        data_select_(j, i).real()) {
      Result_vec_(j, i) = portectMtdMatrix_(
          j + BWin,
          i + BWin);  ///多线程可以对单独元素赋值但是不能对一整块赋值比如  Result_vec_.col(j)
      //Result_vec_db_(j, i) =sqrt( pow(portectMtdMatrix_(j + BWin, i + BWin).real(), 2) + pow(portectMtdMatrix_(j + BWin, i + BWin).imag(), 2));
    }
  }
  //auto end2 = std::chrono::system_clock::now(); auto elapsed2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
  //std::cout << "cfar执行完成，耗时：" << elapsed2.count() << "ms" << '\n';
}

std::vector<std::vector<float>>
SignalProcess::cfarEnd()  ///j 范围是0-（pulseNumber_ -2）
{
  std::vector<std::vector<float>> cfarResult(4);
  Result_vec_db_ = Result_vec_.cwiseAbs();
  float thre = pow(10, -1.5) * Result_vec_db_.maxCoeff();
  for (size_t i = 0; i < pulseNumber_ + BWin * 2; i++) {
    for (size_t j = 0; j < sampleNumber_ + BWin * 2; j++) {
      if (Result_vec_db_(i, j) > thre) {
        cfarResult[0].push_back(
            i);  ///这里可以多线程吗不用双重for循环使用select函数
        cfarResult[1].push_back(j);
        cfarResult[2].push_back(Result_vec_(i, j).real());
        cfarResult[3].push_back(Result_vec_(i, j).imag());
      }
    }
  }
  return cfarResult;
}

std::vector<int> SignalProcess::potFilter(
    std::vector<std::vector<float>>
        cfarResult)  ///TODO 修改变量名 太难读懂了//能不能减少循环 一次算完，这个是每循环一次找到一个值能不能用一次循环
{
  //auto start1 = std::chrono::system_clock::now();
  if (cfarResult[0].size() != 0) {
    std::vector<int> posOfSameVal;  //posOfSameVal
    std::vector<int> posOfMaxOfSameVal;
    int posOfMaxModelusOfSameVal = 0;   // 记录t
    int length = cfarResult[0].size();  //传入的矩阵行数
    float modelusBuf;
    float modelusOfSameVal;
    std::vector<float> uniqueCfar;

    uniqueCfar = unique(cfarResult[0]);  //筛选出来了单值，存放在uniqueCfar容器

    for (size_t j = 0; j < uniqueCfar.size();
         j++)  //对uniqueCfar容器中的独立元素索引
    {
      posOfMaxOfSameVal.push_back(0);
      for (
          size_t i = 0; i < length;
          i++)  //对原来的Cfar矩阵索引找到重复的速度维中，第三列模值最大的元素对应位置
      {
        if (cfarResult[0][i] == uniqueCfar[j]) {
          posOfSameVal.push_back(i);  //找到相同的值把位置先存起来

          if (posOfSameVal.size() > 1) {
            if ((cfarResult[1][posOfSameVal[posOfSameVal.size() - 1]] -
                 cfarResult[1][posOfSameVal[posOfSameVal.size() - 2]]) >
                50)  //第二行值大于50 存在两个目标 倒数第一个元素减去倒数第二个元素
            {
              posOfMaxOfSameVal.push_back(
                  posOfSameVal.size() -
                  1);  //拿到差值过大的位置索引也就是倒数第一个元素的位置索引
            }
          }
        }
      }
      posOfMaxOfSameVal.push_back(posOfSameVal.size());
      if (posOfMaxOfSameVal.size() == 2)  //第一行相同值中没有第二行差值大于50的
      {
        modelusBuf = sqrt(
            cfarResult[2][posOfSameVal[0]] * cfarResult[2][posOfSameVal[0]] +
            cfarResult[3][posOfSameVal[0]] *
                cfarResult[3]
                          [posOfSameVal[0]]);  //把相同的距离维的第一个模值拿到
        posOfMaxModelusOfSameVal =
            0;  //要把他刷新，不然下次循环它的值还是上一次，相同的函数都要注意这个易错点
        for (size_t i = 1; i < posOfSameVal.size();
             i++)  //在第二个元素开始比较大小
        {
          modelusOfSameVal = sqrt(
              cfarResult[2][posOfSameVal[i]] * cfarResult[2][posOfSameVal[i]] +
              cfarResult[3][posOfSameVal[i]] * cfarResult[3][posOfSameVal[i]]);
          if (modelusOfSameVal >
              modelusBuf)  ///TODO  如果if函数一直未进入，应该获得什么值
          {
            posOfMaxModelusOfSameVal =
                i;  //循环比较，如果后一个比前一个大，记录较大的值在posOfSameVal中的位置////////////////////////////	counterMax 初值在哪里给，如果一直没有进入循环
            modelusBuf = modelusOfSameVal;  //较大的值替换初值
          }
        }
        potFilterResult_.push_back(posOfSameVal[posOfMaxModelusOfSameVal]);
        posOfMaxModelusOfSameVal = 0;  //清零，
        posOfSameVal
            .clear();  //用来记录相同距离维的位置，每次清零记录一个新的距离维位置
        posOfMaxOfSameVal.clear();
      } else {
        for (size_t j = 0; j < posOfMaxOfSameVal.size() - 1;
             j++)  //分段计算每一段最大值
        {
          modelusBuf = sqrt(
              cfarResult[2][posOfSameVal[posOfMaxOfSameVal[j]]] *
                  cfarResult[2][posOfSameVal[posOfMaxOfSameVal[j]]] +
              cfarResult[3][posOfSameVal[posOfMaxOfSameVal[j]]] *
                  cfarResult[3]
                            [posOfSameVal
                                 [posOfMaxOfSameVal
                                      [j]]]);  //把相同的距离维的第一个模值拿到
          posOfMaxModelusOfSameVal =
              0;  //要把他刷新，不然下次循环它的值还是上一次，相同的函数都要注意这个易错点
          for (size_t i = posOfMaxOfSameVal[j]; i < posOfMaxOfSameVal[j + 1];
               i++)  //在第二个元素开始比较大小
          {
            modelusOfSameVal = sqrt(cfarResult[2][posOfSameVal[i]] *
                                        cfarResult[2][posOfSameVal[i]] +
                                    cfarResult[3][posOfSameVal[i]] *
                                        cfarResult[3][posOfSameVal[i]]);
            if (modelusOfSameVal >
                modelusBuf)  ///TODO  如果if函数一直未进入，应该获得什么值
            {
              posOfMaxModelusOfSameVal =
                  i;  //循环比较，如果后一个比前一个大，记录较大的值在posOfSameVal中的位置////////////////////////////	counterMax 初值在哪里给，如果一直没有进入循环
              modelusBuf = modelusOfSameVal;  //较大的值替换初值
            }
          }
          potFilterResult_.push_back(posOfSameVal[posOfMaxModelusOfSameVal]);
          posOfMaxModelusOfSameVal = 0;  //清零，
        }
        posOfSameVal
            .clear();  //用来记录相同距离维的位置，每次清零记录一个新的距离维位置
        posOfMaxOfSameVal.clear();
      }
    }
  }
  //auto end1 = std::chrono::system_clock::now(); auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1); timeConsume(elapsed1.count());
  //std::cout << "点迹滤除执行完成,耗时：" << elapsed1.count() << "ms" << '\n';
  return potFilterResult_;
}

MyStruct SignalProcess::solveAmbiguity(
    std::vector<std::vector<float>> targ_result_dis,
    std::vector<std::vector<float>> targ_result_vel) {

  MyStruct Result;
  //auto start1 = std::chrono::system_clock::now();

  Eigen::Vector4f PulseNumber = {61, 65, 73, 81};
  Eigen::VectorXf equalNum = Eigen::VectorXf::LinSpaced(31, 0, 30);
  Eigen::Vector4f SampleNumber = {3260, 3019, 2700, 2540};

  Eigen::MatrixXf DRealBuf = generateMatrix(
      equalNum, SampleNumber);  //局部变量，每次都生成一个新的，优化一下
  Eigen::MatrixXf DReal(SampleNumber.size(), equalNum.size());

  Eigen::VectorXi ddelete_id;
  Eigen::VectorXf TargetDistance1Sort;

  Eigen::VectorXf Darray;
  Eigen::VectorXf varianceResult;
  Eigen::VectorXf::Index varianceResultMinPos = 0;
  std::vector<std::vector<float>> uniqueVal;
  std::vector<float> id(4);
  std::vector<std::vector<float>> TargetDistanceId;
  std::vector<std::vector<float>> TargetDistanceIdSort;

  std::vector<std::vector<float>> DistanceId;

  std::vector<float> TargetDistance1;
  std::vector<float> TargetDistance2;
  std::vector<float> delatdistance;
  std::vector<int> ID5;

  std::vector<std::vector<float>> targetresultid(4);
  ID5.push_back(0);
  float dreal1 = 0;
  float dreal2 = 0;
  float rMeas = 0;
  float minVarianceResult = 0;
  float delatdistanceMinbuf = 0;
  int delatdistanceMinbufid = 0;
  int cir_num = 1;
  int n = 0;

  for (size_t i = 0; i < targ_result_dis.size(); i++) {
    uniqueVal.push_back(unique(sort_vec2(
        targ_result_dis[i])));  //unique函数只能对排好序的数据，所以先排序
    if (uniqueVal[i].size() == 0) {
      uniqueVal[i].push_back(0);  //如果某一个cpi检测结果为空，直接赋一个0值
    }
    cir_num *= uniqueVal[i].size();
  }

  Eigen::MatrixXf Dmeas(4, cir_num);

  for (size_t i = 0; i < uniqueVal[0].size(); i++) {
    for (size_t j = 0; j < uniqueVal[1].size(); j++) {
      for (size_t k = 0; k < uniqueVal[2].size(); k++) {
        for (size_t l = 0; l < uniqueVal[3].size(); l++)  //一定是4行吗
        {
          if (n < cir_num) {
            Dmeas(0, n) = uniqueVal[0][i];
            Dmeas(1, n) = uniqueVal[1][j];
            Dmeas(2, n) = uniqueVal[2][k];
            Dmeas(3, n) = uniqueVal[3][l];
            id[0] = i, id[1] = j, id[2] = k, id[3] = l;
            DistanceId.push_back(id);
            n++;
          }
        }
      }
    }
  }
  //std::cout << Dmeas << std::endl;

  for (size_t i = 0; i < cir_num; i++) {
    for (size_t j = 0; j < DReal.cols(); j++) {
      DReal.col(j) = DRealBuf.col(j) + Dmeas.col(i);
    }

    Darray = sort_vec(matrixfConvertVectorxf(DReal));

    varianceResult = Variance(Darray, 3);  //求方差
    minVarianceResult = varianceResult.minCoeff(&varianceResultMinPos);

    if (minVarianceResult < 2) {
      rMeas = (Darray(varianceResultMinPos) + Darray(varianceResultMinPos + 1) +
               Darray(varianceResultMinPos + 2)) /
              3 * C / Fs / 2;
      dreal2 =
          (Darray(varianceResultMinPos) + Darray(varianceResultMinPos + 1) +
           Darray(varianceResultMinPos + 2) +
           Darray(varianceResultMinPos + 3)) /
          4;
      TargetDistance1.push_back(rMeas);
      //TargetDistance2.push_back(dreal2 * C / Fs / 2);
      delatdistance.push_back(dreal2 * C / Fs / 2 - rMeas);
      TargetDistanceId.push_back(DistanceId[i]);
    }

    //std::cout << Darray << std::endl;
  }
  Eigen::VectorXf delatdistanceSort(TargetDistance1.size());
  if (TargetDistance1.size() > 0) {
    TargetDistance1Sort = sort_vec2(TargetDistance1, ddelete_id);
    for (size_t i = 0; i < ddelete_id.size(); i++) {
      delatdistanceSort(i) = delatdistance[ddelete_id(i)];  //使用排序之后的数据
      TargetDistanceIdSort.push_back(TargetDistanceId[ddelete_id(i)]);
    }
    for (size_t i = 0; i < TargetDistance1Sort.size() - 1; i++) {
      if (abs(TargetDistance1Sort[i + 1] - TargetDistance1Sort[i]) >
          C / Fs / 2) {
        ID5.push_back(i + 1);  //ID5的作用是给数据分段，每段找出目标值
      }
    }

    ID5.push_back(TargetDistance1Sort
                      .size());  //这里也是matlab中放 size（）+1 我这里不用+1
    //std::cout << TargetDistance1Sort.size() << std::endl;

    Eigen::VectorXf result_d_n(ID5.size() - 1);
    Eigen::MatrixXf result_Distance(4, ID5.size() - 1);
    Eigen::MatrixXf result_Vel(4, ID5.size() - 1);

    //std::cout << delatdistanceSort << std::endl;

    for (size_t i = 0; i < ID5.size() - 1; i++) {
      delatdistanceMinbuf = delatdistanceSort(ID5[i]);
      delatdistanceMinbufid = ID5[i];
      for (size_t j = ID5[i] + 1; j < ID5[i + 1]; j++) {
        if (delatdistanceSort(j) <
            delatdistanceMinbuf)  ///TODO  如果if函数一直未进入，应该获得什么值
        {
          delatdistanceMinbuf = delatdistanceSort(j);
          delatdistanceMinbufid = j;
        }
      }
      //	std::cout << delatdistanceMinbufid << std::endl;

      result_d_n(i) = TargetDistance1Sort(delatdistanceMinbufid);
      Result.result_d_n = result_d_n;
      result_Distance(0, i) = uniqueVal
          [0]
          [TargetDistanceIdSort
               [delatdistanceMinbufid]
               [0]];  //TargetDistanceIdSort[delatdistanceMinbufid][0]是delatdistanceMinbufid这个位置的对应第一个值
      result_Distance(1, i) =
          uniqueVal[1][TargetDistanceIdSort[delatdistanceMinbufid][1]];
      result_Distance(2, i) =
          uniqueVal[2][TargetDistanceIdSort[delatdistanceMinbufid][2]];
      result_Distance(3, i) =
          uniqueVal[3][TargetDistanceIdSort[delatdistanceMinbufid][3]];
      Result.result_Distance = result_Distance;

      result_Vel(0, i) = targ_result_vel
          [0]
          [TargetDistanceIdSort
               [delatdistanceMinbufid]
               [0]];  //TargetDistanceIdSort[delatdistanceMinbufid][0]是delatdistanceMinbufid这个位置的对应第一个值
      result_Vel(1, i) =
          targ_result_vel[1][TargetDistanceIdSort[delatdistanceMinbufid][1]];
      result_Vel(2, i) =
          targ_result_vel[2][TargetDistanceIdSort[delatdistanceMinbufid][2]];
      result_Vel(3, i) =
          targ_result_vel[3][TargetDistanceIdSort[delatdistanceMinbufid][3]];
      Result.result_Vel = result_Vel;
      /*		Eigen::VectorXf velocity_result(4);
			for (size_t j = 0; j < 4; j++)
			{
				velocity_result(j) = targ_result_vel[j][TargetDistanceIdSort[delatdistanceMinbufid][0]];
			}*/
    }
    //std::cout << Result.result_Vel << std::endl;
  }
  //auto end1 = std::chrono::system_clock::now(); auto elapsed1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1); timeConsume(elapsed1.count());
  //std::cout << "距离解模糊执行完成,耗时：" << elapsed1.count() << "ms" << '\n';
  return Result;
}

void SignalProcess::w_y_slb_MtiAndMtd(unsigned int Cpi_Id) {
  Eigen::MatrixXcf wmtiMatrix(pulseNumber_ - 2, sampleNumber_);
  Eigen::MatrixXcf ymtiMatrix(pulseNumber_ - 2, sampleNumber_);
  Eigen::MatrixXcf slbmtiMatrix(pulseNumber_ - 2, sampleNumber_);
  Eigen::MatrixXcf wmtdMatrix(pulseNumber_ - 2, sampleNumber_);
  Eigen::MatrixXcf ymtdMatrix(pulseNumber_ - 2, sampleNumber_);
  Eigen::MatrixXcf slbmtdMatrix(pulseNumber_ - 2, sampleNumber_);

  mtdFFT =
      fftwf_plan_dft_1d(pulseNumber_ - 2, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  Eigen::VectorXcf mtdBufw(pulseNumber_ - 2), mtdBufy(pulseNumber_ - 2),
      mtdBufslb(
          pulseNumber_ -
          2);  // 可以直接不用定义中间缓冲，对内存数据进行fft，Matrix在内存排序是列优先
  Eigen::VectorXcf mtdBufwout(pulseNumber_ - 2), mtdBufyout(pulseNumber_ - 2),
      mtdBufslbout(
          pulseNumber_ -
          2);  // 可以直接不用定义中间缓冲，对内存数据进行fft，Matrix在内存排序是列优先

  int n = ceil((pulseNumber_ - 2) / 2.0);

  for (size_t i = 0; i < pulseNumber_ - 2; i++)  ///这里可以使用多线程
  {
    wmtiMatrix.row(i) =
        wpcResult_.row(i + 2) + wpcResult_.row(i) -
        wpcResult_.row(i + 1) * 2;  // 从start开始数那个元素 取出来
    ymtiMatrix.row(i) =
        ypcResult_.row(i + 2) + ypcResult_.row(i) -
        ypcResult_.row(i + 1) * 2;  // 从start开始数那个元素 取出来
    slbmtiMatrix.row(i) =
        slbpcResult_.row(i + 2) + slbpcResult_.row(i) -
        slbpcResult_.row(i + 1) * 2;  // 从start开始数那个元素 取出来
  }
  for (size_t i = 0; i < sampleNumber_; i++) {
    mtdBufw = wmtiMatrix.col(i);
    mtdBufy = ymtiMatrix.col(i);
    mtdBufslb = slbmtiMatrix.col(i);

    fftwf_execute_dft(mtdFFT, (fftwf_complex*)mtdBufw.data(),
                      (fftwf_complex*)mtdBufwout.data());
    fftwf_execute_dft(mtdFFT, (fftwf_complex*)mtdBufy.data(),
                      (fftwf_complex*)mtdBufyout.data());
    fftwf_execute_dft(mtdFFT, (fftwf_complex*)mtdBufslb.data(),
                      (fftwf_complex*)mtdBufslbout.data());

    wmtdMatrix.col(i) << mtdBufwout.segment(n, pulseNumber_ - 2 - n),
        mtdBufwout.segment(0, n);
    ymtdMatrix.col(i) << mtdBufyout.segment(n, pulseNumber_ - 2 - n),
        mtdBufyout.segment(0, n);
    slbmtdMatrix.col(i) << mtdBufslbout.segment(n, pulseNumber_ - 2 - n),
        mtdBufslbout.segment(0, n);
  }
  Eigen::MatrixXf mtdMatrixabs = mtdMatrixvector[Cpi_Id].cwiseAbs();
  Eigen::MatrixXf slbmtdMatrixabs = slbmtdMatrix.cwiseAbs();

  Eigen::MatrixXcf slb_result =
      (mtdMatrixabs.array() >= slbmtdMatrixabs.array())
          .select(mtdMatrixvector[Cpi_Id], 0);

  wmtd.push_back(wmtdMatrix);
  ymtd.push_back(ymtdMatrix);
  slbResult.push_back(slb_result);
}

void SignalProcess::SumDiffDOA(
    MyStruct Result,
    unsigned int Cpi_Id)  //每一个目标值进行一次运算，先找目标数
{
  unsigned int targetresultnum = Result.result_d_n.size();

  ratiofw_Phase_s =
      Eigen::MatrixXf::Zero(Result.result_Distance.rows(), targetresultnum);
  ratiofy_Phase_s =
      Eigen::MatrixXf::Zero(Result.result_Distance.rows(), targetresultnum);

  if (targetresultnum > 0) {
    Eigen::VectorXcf ys = Eigen::VectorXcf::Zero(targetresultnum);
    Eigen::VectorXcf yfw = Eigen::VectorXcf::Zero(targetresultnum);
    Eigen::VectorXcf yfy = Eigen::VectorXcf::Zero(targetresultnum);

    for (size_t i = 0; i < targetresultnum; i++) {
      ys(i) = slbResult[Cpi_Id](
          (int)Result.result_Vel(Cpi_Id, i),
          (int)Result.result_Distance(
              Cpi_Id, i));  ///坐标值在一出现的时候就应该定义成int
      yfw(i) = wmtd[Cpi_Id]((int)Result.result_Vel(Cpi_Id, i),
                            (int)Result.result_Distance(Cpi_Id, i));
      yfy(i) = ymtd[Cpi_Id]((int)Result.result_Vel(Cpi_Id, i),
                            (int)Result.result_Distance(Cpi_Id, i));
    }
    ratiofw_Phase_s = (yfw.array() / ys.array()).imag();
    ratiofy_Phase_s = (yfy.array() / ys.array()).imag();
  }

  //std::cout << ratiofy_Phase_s << std::endl;
  //std::cout << ratiofy_Phase_s << std::endl;
}