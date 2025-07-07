#include "http_server.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <filesystem>

HttpServer::HttpServer(int port, LibrarySystem* library) 
    : port(port), serverSocket(INVALID_SOCKET), running(false), librarySystem(library) {
    initializeWinsock();
    setupRoutes();
}

HttpServer::~HttpServer() {
    stop();
    cleanupWinsock();
}

void HttpServer::initializeWinsock() {
#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        throw std::runtime_error("WSAStartup failed: " + std::to_string(result));
    }
#endif
}

void HttpServer::cleanupWinsock() {
#ifdef _WIN32
    WSACleanup();
#endif
}

void HttpServer::setupRoutes() {
    routes["/"] = [this](const HttpRequest& req) { return handleIndex(req); };
    routes["/index.html"] = [this](const HttpRequest& req) { return handleIndex(req); };
    routes["/users"] = [this](const HttpRequest& req) { return handleApiUsers(req); };
    routes["/books"] = [this](const HttpRequest& req) { return handleApiBooks(req); };
    routes["/borrow"] = [this](const HttpRequest& req) { return handleApiBorrow(req); };
    routes["/return"] = [this](const HttpRequest& req) { return handleApiReturn(req); };
    routes["/statistics"] = [this](const HttpRequest& req) { return handleApiStatistics(req); };
    routes["/api/users"] = [this](const HttpRequest& req) { return handleApiUsers(req); };
    routes["/api/books"] = [this](const HttpRequest& req) { return handleApiBooks(req); };
    routes["/api/borrow"] = [this](const HttpRequest& req) { return handleApiBorrow(req); };
    routes["/api/return"] = [this](const HttpRequest& req) { return handleApiReturn(req); };
    routes["/api/statistics"] = [this](const HttpRequest& req) { return handleApiStatistics(req); };
}

void HttpServer::start() {
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // 设置socket选项
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(serverSocket);
        throw std::runtime_error("Failed to bind socket");
    }
    
    if (listen(serverSocket, 10) == SOCKET_ERROR) {
        closesocket(serverSocket);
        throw std::runtime_error("Failed to listen on socket");
    }
    
    running = true;
    std::cout << "HTTP服务器启动成功，监听端口: " << port << std::endl;
    
    // 自动打开浏览器
#ifdef _WIN32
    std::string command = "start http://localhost:" + std::to_string(port);
    system(command.c_str());
#endif
    
    while (running) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket != INVALID_SOCKET) {
            std::thread clientThread(&HttpServer::handleClient, this, clientSocket);
            clientThread.detach();
        }
    }
}

void HttpServer::stop() {
    running = false;
    if (serverSocket != INVALID_SOCKET) {
        closesocket(serverSocket);
        serverSocket = INVALID_SOCKET;
    }
}

void HttpServer::handleClient(SOCKET clientSocket) {
    try {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string requestData(buffer);
            
            HttpRequest request = parseRequest(requestData);
            HttpResponse response;
            
            // 查找路由处理函数
            auto it = routes.find(request.path);
            if (it != routes.end()) {
                response = it->second(request);
            } else {
                response = errorResponse(404, "Page Not Found");
            }
            
            std::string responseStr = buildResponse(response);
            send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
        }
    } catch (const std::exception& e) {
        std::cerr << "处理客户端请求时出错: " << e.what() << std::endl;
    }
    
    closesocket(clientSocket);
}

HttpRequest HttpServer::parseRequest(const std::string& requestData) {
    HttpRequest request;
    std::istringstream iss(requestData);
    std::string line;
    
    // 解析请求行
    if (std::getline(iss, line)) {
        std::istringstream requestLine(line);
        requestLine >> request.method >> request.path >> request.version;
        
        // 解析查询参数
        size_t queryPos = request.path.find('?');
        if (queryPos != std::string::npos) {
            std::string queryString = request.path.substr(queryPos + 1);
            request.path = request.path.substr(0, queryPos);
            request.queryParams = parseQueryString(queryString);
        }
    }
    
    // 解析请求头
    while (std::getline(iss, line) && line != "\r") {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t\r") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            request.headers[key] = value;
        }
    }
    
    // 解析请求体
    std::string body;
    while (std::getline(iss, line)) {
        body += line + "\n";
    }
    request.body = body;
    
    // 解析POST数据
    if (request.method == "POST" && !request.body.empty()) {
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end() && 
            contentType->second.find("application/x-www-form-urlencoded") != std::string::npos) {
            request.postParams = parseQueryString(request.body);
        }
    }
    
    return request;
}

std::string HttpServer::buildResponse(const HttpResponse& response) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << response.statusCode << " " << response.statusText << "\r\n";
    
    for (const auto& header : response.headers) {
        oss << header.first << ": " << header.second << "\r\n";
    }
    
    oss << "Content-Length: " << response.body.length() << "\r\n";
    oss << "\r\n";
    oss << response.body;
    
    return oss.str();
}

std::map<std::string, std::string> HttpServer::parseQueryString(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos) {
            std::string key = urlDecode(pair.substr(0, equalPos));
            std::string value = urlDecode(pair.substr(equalPos + 1));
            params[key] = value;
        }
    }
    
    return params;
}

std::map<std::string, std::string> HttpServer::parsePostData(const std::string& data) {
    return parseQueryString(data);
}

std::string HttpServer::urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            if (iss >> std::hex >> value) {
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += str[i];
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

HttpResponse HttpServer::handleIndex(const HttpRequest& request) {
    HttpResponse response;
    response.body = generateIndexPage();
    return response;
}

HttpResponse HttpServer::handleApiUsers(const HttpRequest& request) {
    if (request.method == "GET") {
        // 获取用户列表
        Json::Value usersJson(Json::arrayValue);
        auto users = librarySystem->getAllUsers();
        for (User* user : users) {
            usersJson.append(user->toJson());
        }
        return jsonResponse(usersJson);
    } else if (request.method == "POST") {
        // 添加用户
        auto name = request.postParams.find("name");
        auto email = request.postParams.find("email");
        auto phone = request.postParams.find("phone");
        
        if (name != request.postParams.end() && email != request.postParams.end() && 
            phone != request.postParams.end()) {
            int userId = librarySystem->addUser(name->second, email->second, phone->second);
            if (userId > 0) {
                Json::Value result;
                result["success"] = true;
                result["userId"] = userId;
                result["message"] = "用户添加成功";
                return jsonResponse(result);
            }
        }
        return errorResponse(400, "添加用户失败");
    }
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleApiBooks(const HttpRequest& request) {
    if (request.method == "GET") {
        // 获取图书列表
        auto search = request.queryParams.find("search");
        std::vector<Book*> books;
        
        if (search != request.queryParams.end() && !search->second.empty()) {
            books = librarySystem->searchBooks(search->second);
        } else {
            books = librarySystem->getAllBooks();
        }
        
        Json::Value booksJson(Json::arrayValue);
        for (Book* book : books) {
            booksJson.append(book->toJson());
        }
        return jsonResponse(booksJson);
    } else if (request.method == "POST") {
        // 添加图书
        auto title = request.postParams.find("title");
        auto author = request.postParams.find("author");
        auto category = request.postParams.find("category");
        auto keywords = request.postParams.find("keywords");
        auto description = request.postParams.find("description");
        
        if (title != request.postParams.end() && author != request.postParams.end()) {
            std::string cat = (category != request.postParams.end()) ? category->second : "";
            std::string key = (keywords != request.postParams.end()) ? keywords->second : "";
            std::string desc = (description != request.postParams.end()) ? description->second : "";
            
            int bookId = librarySystem->addBook(title->second, author->second, cat, key, desc);
            if (bookId > 0) {
                Json::Value result;
                result["success"] = true;
                result["bookId"] = bookId;
                result["message"] = "图书添加成功";
                return jsonResponse(result);
            }
        }
        return errorResponse(400, "添加图书失败");
    }
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleApiBorrow(const HttpRequest& request) {
    if (request.method == "POST") {
        auto userIdStr = request.postParams.find("userId");
        auto bookIdStr = request.postParams.find("bookId");
        
        if (userIdStr != request.postParams.end() && bookIdStr != request.postParams.end()) {
            try {
                int userId = std::stoi(userIdStr->second);
                int bookId = std::stoi(bookIdStr->second);
                
                if (librarySystem->borrowBook(userId, bookId)) {
                    Json::Value result;
                    result["success"] = true;
                    result["message"] = "借阅成功";
                    return jsonResponse(result);
                } else {
                    Json::Value result;
                    result["success"] = false;
                    result["message"] = "借阅失败：用户不存在、图书不可借或已达借阅上限";
                    return jsonResponse(result, 400);
                }
            } catch (const std::exception& e) {
                return errorResponse(400, "无效的用户ID或图书ID");
            }
        }
        return errorResponse(400, "缺少必要参数");
    }
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleApiReturn(const HttpRequest& request) {
    if (request.method == "POST") {
        auto userIdStr = request.postParams.find("userId");
        auto bookIdStr = request.postParams.find("bookId");
        
        if (userIdStr != request.postParams.end() && bookIdStr != request.postParams.end()) {
            try {
                int userId = std::stoi(userIdStr->second);
                int bookId = std::stoi(bookIdStr->second);
                
                if (librarySystem->returnBook(userId, bookId)) {
                    Json::Value result;
                    result["success"] = true;
                    result["message"] = "归还成功";
                    return jsonResponse(result);
                } else {
                    Json::Value result;
                    result["success"] = false;
                    result["message"] = "归还失败：用户不存在、图书不存在或该用户未借阅此书";
                    return jsonResponse(result, 400);
                }
            } catch (const std::exception& e) {
                return errorResponse(400, "无效的用户ID或图书ID");
            }
        }
        return errorResponse(400, "缺少必要参数");
    }
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleApiStatistics(const HttpRequest& request) {
    if (request.method == "GET") {
        Json::Value stats = librarySystem->getStatisticsJson();
        
        // 添加额外的统计信息
        Json::Value result;
        result["statistics"] = stats;
        result["totalUsers"] = static_cast<int>(librarySystem->getAllUsers().size());
        result["totalBooks"] = static_cast<int>(librarySystem->getAllBooks().size());
        result["totalRecords"] = static_cast<int>(librarySystem->getAllBorrowRecords().size());
        
        return jsonResponse(result);
    }
    return errorResponse(405, "Method Not Allowed");
}

std::string HttpServer::getContentType(const std::string& filename) {
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".html") return "text/html; charset=utf-8";
    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".css") return "text/css";
    if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".js") return "application/javascript";
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".json") return "application/json";
    return "text/plain";
}

std::string HttpServer::readFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

Json::Value HttpServer::parseJsonBody(const std::string& body) {
    Json::Value json;
    Json::Reader reader;
    reader.parse(body, json);
    return json;
}

HttpResponse HttpServer::jsonResponse(const Json::Value& json, int statusCode) {
    HttpResponse response(statusCode);
    response.headers["Content-Type"] = "application/json; charset=utf-8";
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    response.body = Json::writeString(builder, json);
    
    return response;
}

HttpResponse HttpServer::errorResponse(int statusCode, const std::string& message) {
    Json::Value error;
    error["error"] = true;
    error["message"] = message;
    return jsonResponse(error, statusCode);
}

std::string HttpServer::generateIndexPage() {
    return R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>图书管理系统</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            color: #333;
        }
        
        .container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .header {
            text-align: center;
            color: white;
            margin-bottom: 40px;
        }
        
        .header h1 {
            font-size: 3rem;
            margin-bottom: 10px;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.3);
        }
        
        .header p {
            font-size: 1.2rem;
            opacity: 0.9;
        }
        
        .main-content {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 30px;
            margin-bottom: 40px;
        }
        
        .section {
            background: white;
            border-radius: 15px;
            padding: 30px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
            transition: transform 0.3s ease;
        }
        
        .section:hover {
            transform: translateY(-5px);
        }
        
        .section h2 {
            color: #667eea;
            margin-bottom: 20px;
            font-size: 1.8rem;
            border-bottom: 3px solid #667eea;
            padding-bottom: 10px;
        }
        
        .form-group {
            margin-bottom: 20px;
        }
        
        .form-group label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #555;
        }
        
        .form-group input, .form-group textarea, .form-group select {
            width: 100%;
            padding: 12px;
            border: 2px solid #e1e5e9;
            border-radius: 8px;
            font-size: 16px;
            transition: border-color 0.3s ease;
        }
        
        .form-group input:focus, .form-group textarea:focus, .form-group select:focus {
            outline: none;
            border-color: #667eea;
        }
        
        .btn {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 12px 30px;
            border-radius: 8px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s ease;
            width: 100%;
        }
        
        .btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        
        .btn-secondary {
            background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
        }
        
        .btn-success {
            background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
        }
        
        .data-section {
            grid-column: 1 / -1;
            background: white;
            border-radius: 15px;
            padding: 30px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
        }
        
        .tabs {
            display: flex;
            margin-bottom: 30px;
            border-bottom: 2px solid #e1e5e9;
        }
        
        .tab {
            padding: 15px 30px;
            background: none;
            border: none;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            color: #666;
            transition: all 0.3s ease;
            border-bottom: 3px solid transparent;
        }
        
        .tab.active {
            color: #667eea;
            border-bottom-color: #667eea;
        }
        
        .tab-content {
            display: none;
        }
        
        .tab-content.active {
            display: block;
        }
        
        .data-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }
        
        .data-table th, .data-table td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid #e1e5e9;
        }
        
        .data-table th {
            background: #f8f9fa;
            font-weight: 600;
            color: #555;
        }
        
        .data-table tr:hover {
            background: #f8f9fa;
        }
        
        .status-available {
            color: #28a745;
            font-weight: 600;
        }
        
        .status-borrowed {
            color: #dc3545;
            font-weight: 600;
        }
        
        .search-box {
            margin-bottom: 20px;
        }
        
        .search-box input {
            width: 100%;
            padding: 12px;
            border: 2px solid #e1e5e9;
            border-radius: 8px;
            font-size: 16px;
        }
        
        .message {
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 20px;
            display: none;
        }
        
        .message.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        
        .message.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 30px;
        }
        
        .stat-card {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 20px;
            border-radius: 10px;
            text-align: center;
        }
        
        .stat-card h3 {
            font-size: 2rem;
            margin-bottom: 5px;
        }
        
        .stat-card p {
            opacity: 0.9;
        }
        
        @media (max-width: 768px) {
            .main-content {
                grid-template-columns: 1fr;
            }
            
            .header h1 {
                font-size: 2rem;
            }
            
            .tabs {
                flex-wrap: wrap;
            }
            
            .tab {
                flex: 1;
                min-width: 120px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>📚 图书管理系统</h1>
            <p>现代化的图书馆管理解决方案</p>
        </div>
        
        <div class="main-content">
            <div class="section">
                <h2>👤 用户管理</h2>
                <div id="userMessage" class="message"></div>
                <form id="userForm">
                    <div class="form-group">
                        <label for="userName">姓名</label>
                        <input type="text" id="userName" name="name" required>
                    </div>
                    <div class="form-group">
                        <label for="userEmail">邮箱</label>
                        <input type="email" id="userEmail" name="email" required>
                    </div>
                    <div class="form-group">
                        <label for="userPhone">电话</label>
                        <input type="tel" id="userPhone" name="phone" required>
                    </div>
                    <button type="submit" class="btn">添加用户</button>
                </form>
            </div>
            
            <div class="section">
                <h2>📖 图书管理</h2>
                <div id="bookMessage" class="message"></div>
                <form id="bookForm">
                    <div class="form-group">
                        <label for="bookTitle">书名</label>
                        <input type="text" id="bookTitle" name="title" required>
                    </div>
                    <div class="form-group">
                        <label for="bookAuthor">作者</label>
                        <input type="text" id="bookAuthor" name="author" required>
                    </div>
                    <div class="form-group">
                        <label for="bookCategory">类别</label>
                        <input type="text" id="bookCategory" name="category">
                    </div>
                    <div class="form-group">
                        <label for="bookKeywords">关键字</label>
                        <input type="text" id="bookKeywords" name="keywords">
                    </div>
                    <div class="form-group">
                        <label for="bookDescription">简介</label>
                        <textarea id="bookDescription" name="description" rows="3"></textarea>
                    </div>
                    <button type="submit" class="btn">添加图书</button>
                </form>
            </div>
        </div>
        
        <div class="main-content">
            <div class="section">
                <h2>📚 借阅管理</h2>
                <div id="borrowMessage" class="message"></div>
                <form id="borrowForm">
                    <div class="form-group">
                        <label for="borrowUserId">用户ID</label>
                        <input type="number" id="borrowUserId" name="userId" required>
                    </div>
                    <div class="form-group">
                        <label for="borrowBookId">图书ID</label>
                        <input type="number" id="borrowBookId" name="bookId" required>
                    </div>
                    <button type="submit" class="btn btn-secondary">借阅图书</button>
                </form>
            </div>
            
            <div class="section">
                <h2>📤 归还管理</h2>
                <div id="returnMessage" class="message"></div>
                <form id="returnForm">
                    <div class="form-group">
                        <label for="returnUserId">用户ID</label>
                        <input type="number" id="returnUserId" name="userId" required>
                    </div>
                    <div class="form-group">
                        <label for="returnBookId">图书ID</label>
                        <input type="number" id="returnBookId" name="bookId" required>
                    </div>
                    <button type="submit" class="btn btn-success">归还图书</button>
                </form>
            </div>
        </div>
        
        <div class="data-section">
            <div class="tabs">
                <button class="tab active" onclick="showTab('users')">用户列表</button>
                <button class="tab" onclick="showTab('books')">图书列表</button>
                <button class="tab" onclick="showTab('statistics')">统计分析</button>
            </div>
            
            <div id="users" class="tab-content active">
                <div class="search-box">
                    <input type="text" id="userSearch" placeholder="搜索用户..." onkeyup="searchUsers()">
                </div>
                <table class="data-table" id="usersTable">
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>姓名</th>
                            <th>邮箱</th>
                            <th>电话</th>
                            <th>当前借阅</th>
                        </tr>
                    </thead>
                    <tbody></tbody>
                </table>
            </div>
            
            <div id="books" class="tab-content">
                <div class="search-box">
                    <input type="text" id="bookSearch" placeholder="搜索图书..." onkeyup="searchBooks()">
                </div>
                <table class="data-table" id="booksTable">
                    <thead>
                        <tr>
                            <th>ID</th>
                            <th>书名</th>
                            <th>作者</th>
                            <th>类别</th>
                            <th>状态</th>
                        </tr>
                    </thead>
                    <tbody></tbody>
                </table>
            </div>
            
            <div id="statistics" class="tab-content">
                <div class="stats-grid">
                    <div class="stat-card">
                        <h3 id="totalUsers">0</h3>
                        <p>总用户数</p>
                    </div>
                    <div class="stat-card">
                        <h3 id="totalBooks">0</h3>
                        <p>总图书数</p>
                    </div>
                    <div class="stat-card">
                        <h3 id="totalRecords">0</h3>
                        <p>借阅记录</p>
                    </div>
                </div>
                <div id="statisticsContent"></div>
            </div>
        </div>
    </div>
    
    <script>
        // 全局变量
        let allUsers = [];
        let allBooks = [];
        
        // 页面加载完成后初始化
        document.addEventListener('DOMContentLoaded', function() {
            loadUsers();
            loadBooks();
            loadStatistics();
            
            // 绑定表单提交事件
            document.getElementById('userForm').addEventListener('submit', handleUserSubmit);
            document.getElementById('bookForm').addEventListener('submit', handleBookSubmit);
            document.getElementById('borrowForm').addEventListener('submit', handleBorrowSubmit);
            document.getElementById('returnForm').addEventListener('submit', handleReturnSubmit);
        });
        
        // 显示消息
        function showMessage(elementId, message, type) {
            const messageEl = document.getElementById(elementId);
            messageEl.textContent = message;
            messageEl.className = `message ${type}`;
            messageEl.style.display = 'block';
            setTimeout(() => {
                messageEl.style.display = 'none';
            }, 3000);
        }
        
        // 切换标签页
        function showTab(tabName) {
            // 隐藏所有标签内容
            const tabContents = document.querySelectorAll('.tab-content');
            tabContents.forEach(content => content.classList.remove('active'));
            
            // 移除所有标签的活动状态
            const tabs = document.querySelectorAll('.tab');
            tabs.forEach(tab => tab.classList.remove('active'));
            
            // 显示选中的标签内容
            document.getElementById(tabName).classList.add('active');
            event.target.classList.add('active');
            
            // 如果是统计页面，重新加载数据
            if (tabName === 'statistics') {
                loadStatistics();
            }
        }
        
        // 用户管理
        async function handleUserSubmit(e) {
            e.preventDefault();
            const formData = new FormData(e.target);
            
            try {
                const response = await fetch('/api/users', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('userMessage', result.message, 'success');
                    e.target.reset();
                    loadUsers();
                } else {
                    showMessage('userMessage', result.message || '添加用户失败', 'error');
                }
            } catch (error) {
                showMessage('userMessage', '网络错误', 'error');
            }
        }
        
        async function loadUsers() {
            try {
                const response = await fetch('/api/users');
                allUsers = await response.json();
                displayUsers(allUsers);
            } catch (error) {
                console.error('加载用户失败:', error);
            }
        }
        
        function displayUsers(users) {
            const tbody = document.querySelector('#usersTable tbody');
            tbody.innerHTML = '';
            
            users.forEach(user => {
                const row = tbody.insertRow();
                row.innerHTML = `
                    <td>${user.id}</td>
                    <td>${user.name}</td>
                    <td>${user.email}</td>
                    <td>${user.phone}</td>
                    <td>${user.borrowHistory ? user.borrowHistory.length : 0}</td>
                `;
            });
        }
        
        function searchUsers() {
            const searchTerm = document.getElementById('userSearch').value.toLowerCase();
            const filteredUsers = allUsers.filter(user => 
                user.name.toLowerCase().includes(searchTerm) ||
                user.email.toLowerCase().includes(searchTerm)
            );
            displayUsers(filteredUsers);
        }
        
        // 图书管理
        async function handleBookSubmit(e) {
            e.preventDefault();
            const formData = new FormData(e.target);
            
            try {
                const response = await fetch('/api/books', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('bookMessage', result.message, 'success');
                    e.target.reset();
                    loadBooks();
                } else {
                    showMessage('bookMessage', result.message || '添加图书失败', 'error');
                }
            } catch (error) {
                showMessage('bookMessage', '网络错误', 'error');
            }
        }
        
        async function loadBooks() {
            try {
                const response = await fetch('/api/books');
                allBooks = await response.json();
                displayBooks(allBooks);
            } catch (error) {
                console.error('加载图书失败:', error);
            }
        }
        
        function displayBooks(books) {
            const tbody = document.querySelector('#booksTable tbody');
            tbody.innerHTML = '';
            
            books.forEach(book => {
                const row = tbody.insertRow();
                const statusClass = book.isAvailable ? 'status-available' : 'status-borrowed';
                const statusText = book.isAvailable ? '可借阅' : '已借出';
                
                row.innerHTML = `
                    <td>${book.id}</td>
                    <td>${book.title}</td>
                    <td>${book.author}</td>
                    <td>${book.category || '-'}</td>
                    <td><span class="${statusClass}">${statusText}</span></td>
                `;
            });
        }
        
        function searchBooks() {
            const searchTerm = document.getElementById('bookSearch').value.toLowerCase();
            const filteredBooks = allBooks.filter(book => 
                book.title.toLowerCase().includes(searchTerm) ||
                book.author.toLowerCase().includes(searchTerm) ||
                (book.category && book.category.toLowerCase().includes(searchTerm))
            );
            displayBooks(filteredBooks);
        }
        
        // 借阅管理
        async function handleBorrowSubmit(e) {
            e.preventDefault();
            const formData = new FormData(e.target);
            
            try {
                const response = await fetch('/api/borrow', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('borrowMessage', result.message, 'success');
                    e.target.reset();
                    loadUsers();
                    loadBooks();
                } else {
                    showMessage('borrowMessage', result.message || '借阅失败', 'error');
                }
            } catch (error) {
                showMessage('borrowMessage', '网络错误', 'error');
            }
        }
        
        // 归还管理
        async function handleReturnSubmit(e) {
            e.preventDefault();
            const formData = new FormData(e.target);
            
            try {
                const response = await fetch('/api/return', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('returnMessage', result.message, 'success');
                    e.target.reset();
                    loadUsers();
                    loadBooks();
                } else {
                    showMessage('returnMessage', result.message || '归还失败', 'error');
                }
            } catch (error) {
                showMessage('returnMessage', '网络错误', 'error');
            }
        }
        
        // 统计分析
        async function loadStatistics() {
            try {
                const response = await fetch('/api/statistics');
                const data = await response.json();
                
                // 更新统计卡片
                document.getElementById('totalUsers').textContent = data.totalUsers;
                document.getElementById('totalBooks').textContent = data.totalBooks;
                document.getElementById('totalRecords').textContent = data.totalRecords;
                
                // 显示详细统计信息
                displayDetailedStatistics(data.statistics);
                
            } catch (error) {
                console.error('加载统计信息失败:', error);
            }
        }
        
        function displayDetailedStatistics(stats) {
            const container = document.getElementById('statisticsContent');
            let html = '';
            
            // 最受欢迎的图书
            if (stats.bookPopularity) {
                html += '<h3>📈 最受欢迎的图书</h3>';
                html += '<table class="data-table">';
                html += '<thead><tr><th>图书ID</th><th>借阅次数</th></tr></thead><tbody>';
                
                const sortedBooks = Object.entries(stats.bookPopularity)
                    .sort(([,a], [,b]) => b - a)
                    .slice(0, 10);
                
                sortedBooks.forEach(([bookId, count]) => {
                    html += `<tr><td>${bookId}</td><td>${count}</td></tr>`;
                });
                
                html += '</tbody></table>';
            }
            
            // 最活跃的用户
            if (stats.userActivity) {
                html += '<h3>👥 最活跃的用户</h3>';
                html += '<table class="data-table">';
                html += '<thead><tr><th>用户ID</th><th>借阅次数</th></tr></thead><tbody>';
                
                const sortedUsers = Object.entries(stats.userActivity)
                    .sort(([,a], [,b]) => b - a)
                    .slice(0, 10);
                
                sortedUsers.forEach(([userId, count]) => {
                    html += `<tr><td>${userId}</td><td>${count}</td></tr>`;
                });
                
                html += '</tbody></table>';
            }
            
            // 月度趋势
            if (stats.monthlyStats) {
                html += '<h3>📊 月度借阅趋势</h3>';
                html += '<table class="data-table">';
                html += '<thead><tr><th>月份</th><th>借阅次数</th></tr></thead><tbody>';
                
                const sortedMonths = Object.entries(stats.monthlyStats)
                    .sort(([a], [b]) => a.localeCompare(b));
                
                sortedMonths.forEach(([month, count]) => {
                    html += `<tr><td>${month}</td><td>${count}</td></tr>`;
                });
                
                html += '</tbody></table>';
            }
            
            container.innerHTML = html;
        }
    </script>
</body>
</html>
)HTML";
}