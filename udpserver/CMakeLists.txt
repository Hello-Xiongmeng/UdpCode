
#写几个set直接生成文件夹就不用手动创建了
#运行程序后文件读取函数默认读取当前文件夹下的文件，所以在bin目录下执行命令./YJJtest，如果在其他文件夹下执行,例如../bin/YJJtest，显示打开文件失败
cmake_minimum_required(VERSION 3.15)#指定最小cmake版本

project(UdpServer)#指定项目名称

set(CMAKE_CXX_STANDARD 14)

#set(CMAKE_BUILD_TYPE "Release")#这个模式设置之后程序无法执行

set (EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)#告知生成exe文件的目录，这里使用主动生成的目录

include_directories(${PROJECT_SOURCE_DIR}/include)#告知头文件目录

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_SUB)#告知源文件目录

add_executable(${PROJECT_NAME} ${SRC_SUB})#告知生成exe文件的名字

find_package(Threads REQUIRED)         # 查找线程库
# 链接 pthread 库到可执行文件
target_link_libraries(${PROJECT_NAME} Threads::Threads)