
#ifndef READATA_HPP
#define READATA_HPP
#include <string>
#include <vector>
#include <Eigen/Dense>

class ReadData
{
private:
    Eigen::VectorXcf convertToVectorXcf(std::vector<std::vector<float>> data);   // Note: ��֪��ά�ȵ�����£��Ѷ�ά��vector ֱ��ת��Ϊ Eigen ��Matrix
    Eigen::MatrixXcf convertToMatrixXcf(std::vector<std::vector<float>> data1, std::vector<std::vector<float>> data2);   // Note: ��֪��ά�ȵ�����£��Ѷ�ά��vector ֱ��ת��Ϊ Eigen ��Matrix
    Eigen::MatrixXf convertToMatrixXf(std::vector<std::vector<float>> data);   // Note: ��֪��ά�ȵ�����£��Ѷ�ά��vector ֱ��ת��Ϊ Eigen ��Matrix

public:
    ReadData(Eigen::VectorXcf* data_VectorXcf,std:: string str);   // ���캯��
    ReadData();
    ReadData(Eigen::MatrixXcf* data_Matrix, std::string str1, std::string str2);   // ���캯��
    Eigen::VectorXcf readData(std::string str_real, std::string str_imag);
    Eigen::VectorXf readData(std::string str);


};
#endif
