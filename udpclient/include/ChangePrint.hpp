#ifndef CHANGEPRINT_HPP
#define CHANGEPRINT_HPP

#include <iostream>
#include <string>
#include <unordered_map>

// 定义颜色代码
const std::unordered_map<std::string, std::string> COLORS = {
    {"reset", "\033[0m"},   {"red", "\033[31m"},  {"green", "\033[32m"},
    {"yellow", "\033[33m"}, {"blue", "\033[34m"}, {"magenta", "\033[35m"},
    {"cyan", "\033[36m"},   {"white", "\033[37m"}};

// 变参模板封装打印函数
template <typename... Args>
void printWithColor(const std::string& color, Args&&... args) {
  // 查找颜色代码
  auto it = COLORS.find(color);
  if (it == COLORS.end()) {
    std::cerr << "Invalid color specified: " << color << std::endl;
    return;
  }

  // 输出颜色前缀
  std::cout << it->second;

  // 使用 `std::initializer_list` 输出参数
  (void)std::initializer_list<int>{(std::cout << args, 0)...};

  // 重置颜色并换行
  std::cout << COLORS.at("reset") << std::endl;
}

#endif  