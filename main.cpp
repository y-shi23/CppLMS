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
        
        std::cout << "Book Management System is activating..." << std::endl;
        std::cout << "Running on: http://localhost:8080" << std::endl;
        std::cout << "Please open browser and vistit 8080" << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;
        
        // 启动服务器
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Failed: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}