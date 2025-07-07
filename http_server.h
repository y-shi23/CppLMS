#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <map>
#include <functional>
#include <thread>
#include <atomic>
#include <sstream>
#include <iostream>
#include <fstream>
#include <regex>
#include "json.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket close
#endif

#include "library_system.h"

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    std::map<std::string, std::string> queryParams;
    std::map<std::string, std::string> postParams;
};

struct HttpResponse {
    int statusCode;
    std::string statusText;
    std::map<std::string, std::string> headers;
    std::string body;
    
    HttpResponse(int code = 200, const std::string& text = "OK") 
        : statusCode(code), statusText(text) {
        headers["Content-Type"] = "text/html; charset=utf-8";
        headers["Connection"] = "close";
    }
};

class HttpServer {
private:
    int port;
    SOCKET serverSocket;
    std::atomic<bool> running;
    LibrarySystem* librarySystem;
    
    // 路由处理函数类型
    using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;
    std::map<std::string, RouteHandler> routes;
    
public:
    HttpServer(int port, LibrarySystem* library);
    ~HttpServer();
    
    void start();
    void stop();
    
private:
    void initializeWinsock();
    void cleanupWinsock();
    void setupRoutes();
    void handleClient(SOCKET clientSocket);
    
    HttpRequest parseRequest(const std::string& requestData);
    std::string buildResponse(const HttpResponse& response);
    std::map<std::string, std::string> parseQueryString(const std::string& query);
    std::map<std::string, std::string> parsePostData(const std::string& data);
    std::map<std::string, std::string> parseMultipartData(const std::string& body, const std::string& contentType);
    std::string urlDecode(const std::string& str);
    
    // 路由处理函数
    HttpResponse handleIndex(const HttpRequest& request);
    HttpResponse handleLogin(const HttpRequest& request);
    HttpResponse handleApiLogin(const HttpRequest& request);
    HttpResponse handleStatic(const HttpRequest& request);
    HttpResponse handleStaticFile(const HttpRequest& request, const std::string& filePath);
    HttpResponse handleApiUsers(const HttpRequest& request);
    HttpResponse handleApiBooks(const HttpRequest& request);
    HttpResponse handleApiBorrow(const HttpRequest& request);
    HttpResponse handleApiReturn(const HttpRequest& request);
    HttpResponse handleApiStatistics(const HttpRequest& request);
    
    // 工具函数
    std::string getContentType(const std::string& filename);
    std::string readFile(const std::string& filename);
    Json::Value parseJsonBody(const std::string& body);
    HttpResponse jsonResponse(const Json::Value& json, int statusCode = 200);
    HttpResponse errorResponse(int statusCode, const std::string& message);
    
    // HTML页面生成
    std::string generateIndexPage();
    std::string generateLoginPage();
    std::string generateUsersPage();
    std::string generateBooksPage();
    std::string generateBorrowPage();
    std::string generateStatisticsPage();
};

#endif // HTTP_SERVER_H