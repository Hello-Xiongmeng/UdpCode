#include "ReadData.hpp"
#include <fstream>
#include <iostream>


Eigen::VectorXcf ReadData::convertToVectorXcf(
   std:: vector<std::vector<float>> data) {
  Eigen::VectorXcf eVector = Eigen::VectorXcf::Zero(data.size());
  for (int i = 0; i < data.size(); ++i) {
    eVector(i).real(data[i][0]);
    eVector(i).imag(data[i][1]);
    // = +data[i][1]i;   // ʹ��mapӳ��
    // eVector(i).imag() = data[i][1];   // ʹ��mapӳ��
  }
  //MatrixXf eMatrix(data.size(), data[0].size());
  //for (int i = 0; i < data.size(); ++i)
  //    eMatrix.row(i) = VectorXf::Map(&data[i][0], data[0].size());   // ʹ��mapӳ��
  return eVector;
}

ReadData::ReadData(Eigen::VectorXcf* data_VectorXcf,std:: string str) {
  std::ifstream ifs;  // ����һ���ļ�������
 std:: string buffer;
  float Line_Per_Data;
  std::vector<float> Data_buf;
std::vector<std::vector<float>> Data_demo;

  ifs.open(str, std::ios::in);  // �Զ��ļ�����ʽ���ı�

  if (!ifs.is_open()) {
    std::cout << "fail to open!!!!" << std::endl;
  } else {
    while (getline(ifs, buffer))  // ��ȡһ��
    {
      std::istringstream stream(buffer);  // ���ո�ָ�
      while (stream >> Line_Per_Data) {
        Data_buf.push_back(Line_Per_Data);
      }
      Data_demo.push_back(Data_buf);
      Data_buf.clear();
    }
    ifs.close();
    *data_VectorXcf =
        convertToVectorXcf(Data_demo);  //*Data_Matrix  �ǽ�����
  }
}

ReadData::ReadData()
{
}

Eigen::MatrixXf ReadData::convertToMatrixXf(
    std::vector<std::vector<float>> data) {
  Eigen::MatrixXf eMatrix = Eigen::MatrixXf::Zero(data.size(), data[0].size());
  for (int i = 0; i < data.size(); ++i) {

    for (int j = 0; j < data[0].size(); j++) {
      eMatrix(i, j) = data[i][j];
    }

    // = +data[i][1]i;   // ʹ��mapӳ��
    // eVector(i).imag() = data[i][1];   // ʹ��mapӳ��
  }
  //MatrixXf eMatrix(data.size(), data[0].size());
  //for (int i = 0; i < data.size(); ++i)
  //    eMatrix.row(i) = VectorXf::Map(&data[i][0], data[0].size());   // ʹ��mapӳ��
  return eMatrix;
}
Eigen::VectorXcf  ReadData::readData( std::string str_real, std::string str_imag)
{
    std::ifstream file_real(str_real, std::ios::binary | std::ios::in);
    std::ifstream file_imag(str_imag, std::ios::binary | std::ios::in);
    if ((!file_real) && (!file_imag))
    {
       // cout << "fail to open!!!!" << endl;
       std::cerr << "fail to open!!!!" << std::endl;
       return Eigen::VectorXcf();
    }
    else
    {
        file_real.seekg(0, std::ios::end);
        std::streamsize size1 = file_real.tellg();//size �Ǽ�����ֽ���
        file_real.seekg(0, std::ios::beg);
        file_imag.seekg(0, std::ios::end);
        std::streamsize size2 = file_imag.tellg();//size �Ǽ�����ֽ���
        file_imag.seekg(0, std::ios::beg);
        // �����ڴ棬׼����ȡȫ������
        std::vector<char> buffer1(size1);//�������ݻ��壬�洢��λ���ֽ�
        std::vector<char> buffer2(size2);//�������ݻ��壬�洢��λ���ֽ�
        Eigen::VectorXcf data(size1/ sizeof(float));
        if ((file_real.read(buffer1.data(), size1))&&(file_imag.read(buffer2.data(), size2)))
        {
            Eigen::VectorXf data_real(size1 / sizeof(float));
            Eigen::VectorXf data_imag(size2 / sizeof(float));
            std::memcpy(data_real.data(), buffer1.data(), size1);//ֱ�ӿ���}
            std::memcpy(data_imag.data(), buffer2.data(), size2);//ֱ�ӿ���}
            // ȷ���ļ���С
            data.real() = data_real;     
            data.imag() = data_imag;
        }
        file_imag.close();
        file_real.close();
        return data;
    }
}
Eigen::VectorXf ReadData::readData(std::string str)
{

    std::ifstream file_data(str, std::ios::binary | std::ios::in);
    if (!file_data)
    {
      std::cerr << "fail to open!!!!" << std::endl;
     return Eigen::VectorXf();

    }
    else
    {
        file_data.seekg(0, std::ios::end);
        std::streamsize size = file_data.tellg();//size �Ǽ�����ֽ���
        file_data.seekg(0, std::ios::beg);
        // �����ڴ棬׼����ȡȫ������
        std::vector<char> buffer(size);//�������ݻ��壬
        Eigen::VectorXf data(size / sizeof(float));//����float���͵����ݴ洢
        if (file_data.read(buffer.data(), size))
        {//��ȡbuffer.data() ��ʼ��size���ֽڵ�����
            // ���ݶ�ȡ�ɹ���ת��Ϊfloat
            std::memcpy(data.data(), buffer.data(), size);//ֱ�ӿ���}
            // ȷ���ļ���С
        }
        file_data.close();
        return data;
    }

    return Eigen::VectorXf();
}

// ReadData::ReadData(MatrixXf* data_Matrixxf, string str)
// {
//    ifstream              ifs;   // ����һ���ļ�������
//    string                buffer;
//    float                 Line_Per_Data;
//    vector<float>         Data_buf;
//    vector<vector<float>> Data_demo;

//    ifs.open(str, ios::in);   // �Զ��ļ�����ʽ���ı�

//    if (!ifs.is_open())
//    {
//        cout << "fail to open!!!!" << endl;
//    }
//    else
//    {
//        while (getline(ifs, buffer))   // ��ȡһ��
//        {
//            istringstream stream(buffer);   // ���ո�ָ�
//            while (stream >> Line_Per_Data)
//            {
//                Data_buf.push_back(Line_Per_Data);
//            }
//            Data_demo.push_back(Data_buf);
//            Data_buf.clear();
//        }
//        ifs.close();
//        *data_Matrixxf = ConvertToEigenMatrixXf(Data_demo);//*Data_Matrix  �ǽ�����
//    }
// }

Eigen::MatrixXcf ReadData::convertToMatrixXcf(std::vector<std::vector<float>> data1, std::vector<std::vector<float>> data2)
{

  Eigen::MatrixXcf eMatrix =
      Eigen::MatrixXcf::Zero(data1.size(), data1[0].size());
  for (int i = 0; i < data1.size(); ++i) {

    for (int j = 0; j < data1[0].size(); j++) {
      eMatrix(i, j).real(data1[i][j]);
      eMatrix(i, j).imag(data2[i][j]);
    }

    // = +data[i][1]i;   // ʹ��mapӳ��
    // eVector(i).imag() = data[i][1];   // ʹ��mapӳ��
    }
    //MatrixXf eMatrix(data.size(), data[0].size());
    //for (int i = 0; i < data.size(); ++i)
    //    eMatrix.row(i) = VectorXf::Map(&data[i][0], data[0].size());   // ʹ��mapӳ��

    return eMatrix;
}

ReadData::ReadData(Eigen::MatrixXcf* data_Matrix, std::string str1, std::string str2)
{
    std::ifstream              ifs;   // ����һ���ļ�������
    std::string                buffer;
    float                 Line_Per_Data;
    std::vector<float> Data_buf;
    std::vector<std::vector<float>> Data_demo1;
    std::vector<std::vector<float>> Data_demo2;

    ifs.open(str1, std::ios::in);  // �Զ��ļ�����ʽ���ı����ȶ�ʵ��

    if (!ifs.is_open())
    {
      std::cout << "fail to open!!!!" << std::endl;
    }
    else
    {
        while (getline(ifs, buffer))   // ��ȡһ��
        {
          std::istringstream stream(buffer);  // ���ո�ָ�
          while (stream >> Line_Per_Data) {
            Data_buf.push_back(Line_Per_Data);
            }
            Data_demo1.push_back(Data_buf);
            Data_buf.clear();
        }
       ifs.close();

        //*data_Matrix = ConvertToEigenMatrixXcf(Data_demo1, Data_demo2);//*Data_Matrix  �ǽ�����
    }

    ifs.open(str2, std::ios::in);  // �Զ��ļ�����ʽ���ı����ȶ�ʵ��

    if (!ifs.is_open())//�ٶ��鲿
    {
      std::cout << "fail to open!!!!" << std::endl;
    }
    else
    {
        while (getline(ifs, buffer))   // ��ȡһ��
        {
          std::istringstream stream(buffer);  // ���ո�ָ�
          while (stream >> Line_Per_Data) {
            Data_buf.push_back(Line_Per_Data);
            }
            Data_demo2.push_back(Data_buf);
            Data_buf.clear();
        }
        ifs.close();

    }

    *data_Matrix = convertToMatrixXcf(Data_demo1, Data_demo2);//*Data_Matrix  �ǽ�����

}
