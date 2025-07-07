#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <thread>
#include <chrono>
#include <regex>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include "library_system.h"
#include "http_server.h"

int main() {
    try {
        // 初始化图书管理系统
        LibrarySystem library;
        
        // 加载测试数据
        library.loadTestData();
        
        // 创建HTTP服务器
        HttpServer server(8080, &library);
        
        std::cout << "图书管理系统启动中..." << std::endl;
        std::cout << "服务器运行在: http://localhost:8080" << std::endl;
        std::cout << "请在浏览器中打开上述地址访问系统" << std::endl;
        std::cout << "按 Ctrl+C 退出系统" << std::endl;
        
        // 启动服务器
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "系统启动失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}