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
    routes["/login"] = [this](const HttpRequest& req) { return handleLogin(req); };
    routes["/api/login"] = [this](const HttpRequest& req) { return handleApiLogin(req); };
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
        std::string requestData;
        char buffer[4096];
        int bytesReceived;

        // 先读取一部分数据，期望能包含完整的请求头
        bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            closesocket(clientSocket);
            return;
        }
        buffer[bytesReceived] = '\0';
        requestData.append(buffer, bytesReceived);

        // 检查是否收到了完整的请求头
        size_t header_end_pos = requestData.find("\r\n\r\n");
        if (header_end_pos == std::string::npos) {
            // 如果4KB还不够一个请求头，那这个请求也太大了，直接放弃
            errorResponse(413, "Request-Header Fields Too Large");
            closesocket(clientSocket);
            return;
        }

        // 解析请求头以获取Content-Length
        std::string headers_part = requestData.substr(0, header_end_pos);
        std::istringstream iss(headers_part);
        std::string line;
        int content_length = 0;
        while (std::getline(iss, line) && line != "\r") {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                if (key == "Content-Length") {
                    std::string value = line.substr(colon_pos + 1);
                    value.erase(0, value.find_first_not_of(" \t"));
                    value.erase(value.find_last_not_of(" \t\r") + 1);
                    try {
                        content_length = std::stoi(value);
                    } catch (...) {
                        content_length = 0;
                    }
                    break;
                }
            }
        }

        // 如果有请求体，确保接收完整
        if (content_length > 0) {
            size_t body_start_pos = header_end_pos + 4;
            size_t body_received = requestData.length() - body_start_pos;
            if (body_received < content_length) {
                requestData.resize(body_start_pos + content_length);
                int remaining = content_length - body_received;
                int current_pos = body_start_pos + body_received;
                while (remaining > 0) {
                    bytesReceived = recv(clientSocket, &requestData[current_pos], remaining, 0);
                    if (bytesReceived <= 0) {
                        // 连接断开或出错
                        closesocket(clientSocket);
                        return;
                    }
                    remaining -= bytesReceived;
                    current_pos += bytesReceived;
                }
            }
        }

        HttpRequest request = parseRequest(requestData);
        HttpResponse response;
        
        // 查找路由处理函数
        auto it = routes.find(request.path);
        if (it != routes.end()) {
            response = it->second(request);
        } else {
            // 检查是否是带参数的API路由
            if (request.path.find("/api/users/") == 0 && request.path.length() > 11) {
                // 处理 /api/users/{id} 路由
                response = handleApiUsers(request);
            } else if (request.path.find("/api/books/") == 0 && request.path.length() > 11) {
                // 处理 /api/books/{id} 路由
                response = handleApiBooks(request);
            } else {
                response = errorResponse(404, "Page Not Found");
            }
        }
        
        std::string responseStr = buildResponse(response);
        send(clientSocket, responseStr.c_str(), responseStr.length(), 0);

    } catch (const std::exception& e) {
        std::cerr << "处理客户端请求时出错: " << e.what() << std::endl;
    }
    
    closesocket(clientSocket);
}

HttpRequest HttpServer::parseRequest(const std::string& requestData) {
    HttpRequest request;
    std::string headersPart;
    
    size_t bodyStartPos = requestData.find("\r\n\r\n");
    if (bodyStartPos != std::string::npos) {
        headersPart = requestData.substr(0, bodyStartPos);
        request.body = requestData.substr(bodyStartPos + 4);
    } else {
        headersPart = requestData;
    }

    std::istringstream iss(headersPart);
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
    
    // 解析POST数据
    if (request.method == "POST" && !request.body.empty()) {
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end()) {
            if (contentType->second.find("application/x-www-form-urlencoded") != std::string::npos) {
                request.postParams = parseQueryString(request.body);
            } else if (contentType->second.find("multipart/form-data") != std::string::npos) {
                request.postParams = parseMultipartData(request.body, contentType->second);
            }
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

std::map<std::string, std::string> HttpServer::parseMultipartData(const std::string& body, const std::string& contentType) {
    std::map<std::string, std::string> params;
    
    // 提取boundary
    size_t boundaryPos = contentType.find("boundary=");
    if (boundaryPos == std::string::npos) {
        return params;
    }
    
    std::string boundary = "--" + contentType.substr(boundaryPos + 9);
    
    // 分割multipart数据
    size_t pos = 0;
    while (pos < body.length()) {
        size_t boundaryStart = body.find(boundary, pos);
        if (boundaryStart == std::string::npos) break;
        
        size_t nextBoundaryStart = body.find(boundary, boundaryStart + boundary.length());
        if (nextBoundaryStart == std::string::npos) break;
        
        // 提取这一部分的数据
        std::string part = body.substr(boundaryStart + boundary.length(), 
                                      nextBoundaryStart - boundaryStart - boundary.length());
        
        // 查找Content-Disposition头
        size_t headerEnd = part.find("\r\n\r\n");
        if (headerEnd != std::string::npos) {
            std::string headers = part.substr(0, headerEnd);
            std::string value = part.substr(headerEnd + 4);
            
            // 移除末尾的\r\n
            if (value.length() >= 2 && value.substr(value.length() - 2) == "\r\n") {
                value = value.substr(0, value.length() - 2);
            }
            
            // 提取name属性
            size_t namePos = headers.find("name=\"");
            if (namePos != std::string::npos) {
                namePos += 6; // 跳过name="
                size_t nameEnd = headers.find("\"", namePos);
                if (nameEnd != std::string::npos) {
                    std::string name = headers.substr(namePos, nameEnd - namePos);
                    params[name] = value;
                }
            }
        }
        
        pos = nextBoundaryStart;
    }
    
    return params;
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

HttpResponse HttpServer::handleLogin(const HttpRequest& request) {
    HttpResponse response;
    response.body = generateLoginPage();
    return response;
}

HttpResponse HttpServer::handleApiLogin(const HttpRequest& request) {
    if (request.method == "POST") {
        std::string username, password;
        
        // 检查Content-Type来决定如何解析数据
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end() && 
            contentType->second.find("application/json") != std::string::npos) {
            // 解析JSON数据
            Json::Value jsonData = parseJsonBody(request.body);
            if (jsonData.isObject() && !jsonData["username"].isNull() && !jsonData["password"].isNull()) {
                username = jsonData["username"].asString();
                password = jsonData["password"].asString();
            } else {
                return errorResponse(400, "缺少用户名或密码");
            }
        } else {
            // 解析form-urlencoded数据
            auto usernameIt = request.postParams.find("username");
            auto passwordIt = request.postParams.find("password");
            
            if (usernameIt != request.postParams.end() && passwordIt != request.postParams.end()) {
                username = usernameIt->second;
                password = passwordIt->second;
            } else {
                return errorResponse(400, "缺少用户名或密码");
            }
        }
        
        // 根据用户名判断用户类型并验证
        bool loginSuccess = false;
        std::string message = "";
        std::string userType = ""        std::string actualUsername = "";
        int userId = -1;
        
        // 管理员账户验证
        if (username == "admin" && password == "1234") {
            loginSuccess = true;
            userType = "admin";
            actualUsername = "admin";
            userId = 0;
            message = "管理员登录成功";
        }
        else {
            // 从users.json验证用户
            try {
                std::ifstream file("data/users.json");
                if (file.is_open()) {
                    Json::Value usersData;
                    file >> usersData;
                    file.close();
                    
                    if (usersData.isArray()) {
                        for (const auto& user : usersData) {
                            std::string userName = user["name"].asString();
                            std::string userEmail = user["email"].asString();
                            
                            // 支持用户名或邮箱登录，密码暂时使用用户ID
                            std::string userPassword = std::to_string(user["id"].asInt());
                            
                            if ((username == userName || username == userEmail) && password == userPassword) {
                                loginSuccess = true;
                                userType = "reader";
                                actualUsername = userName;
                                userId = user["id"].asInt();
                                message = "用户登录成功";
                                break;
                            }
                        }
                    }
                }
            } catch (const std::exception& e) {
                message = "系统错误，请稍后重试";
            }
            
            if (!loginSuccess) {
                message = "用户名或密码错误";
            }
        }
        
        Json::Value result;
        result["success"] = loginSuccess;
        result["message"] = message;
        if (loginSuccess) {
            result["userType"] = userType;
            result["username"] = actualUsername;
            result["userId"] = userId;
        }
        
        return jsonResponse(result);
    }
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleStaticFile(const HttpRequest& request, const std::string& filePath) {
    HttpResponse response;
    std::string content = readFile(filePath);
    
    if (content.empty()) {
        return errorResponse(404, "File not found");
    }
    
    response.body = content;
    response.headers["Content-Type"] = getContentType(filePath);
    response.headers["Cache-Control"] = "public, max-age=3600";
    
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
        std::string name, email, phone;
        
        // 检查Content-Type来决定如何解析数据
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end() && 
            contentType->second.find("application/json") != std::string::npos) {
            // 解析JSON数据
            Json::Value jsonData = parseJsonBody(request.body);
            if (jsonData.isObject() && !jsonData["name"].isNull() && 
                !jsonData["email"].isNull() && !jsonData["phone"].isNull()) {
                name = jsonData["name"].asString();
                email = jsonData["email"].asString();
                phone = jsonData["phone"].asString();
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        } else {
            // 解析form-urlencoded数据
            auto nameIt = request.postParams.find("name");
            auto emailIt = request.postParams.find("email");
            auto phoneIt = request.postParams.find("phone");
            
            if (nameIt != request.postParams.end() && emailIt != request.postParams.end() && 
                phoneIt != request.postParams.end()) {
                name = nameIt->second;
                email = emailIt->second;
                phone = phoneIt->second;
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        }
        
        int userId = librarySystem->addUser(name, email, phone);
        if (userId > 0) {
            Json::Value result;
            result["success"] = true;
            result["userId"] = userId;
            result["message"] = "用户添加成功";
            return jsonResponse(result);
        }
        return errorResponse(400, "添加用户失败");
    } else if (request.method == "PUT") {
        // 修改用户
        // 从URL路径中提取用户ID
        std::string path = request.path;
        size_t pos = path.find("/api/users/");
        if (pos == std::string::npos) {
            return errorResponse(400, "无效的URL路径");
        }
        
        std::string userIdStr = path.substr(pos + 11); // "/api/users/"的长度是11
        int userId;
        try {
            userId = std::stoi(userIdStr);
        } catch (const std::exception& e) {
            return errorResponse(400, "无效的用户ID");
        }
        
        std::string name, email, phone;
        
        // 解析JSON数据
        Json::Value jsonData = parseJsonBody(request.body);
        if (jsonData.isObject() && !jsonData["name"].isNull() && 
            !jsonData["email"].isNull() && !jsonData["phone"].isNull()) {
            name = jsonData["name"].asString();
            email = jsonData["email"].asString();
            phone = jsonData["phone"].asString();
        } else {
            return errorResponse(400, "缺少必要参数");
        }
        
        if (librarySystem->updateUser(userId, name, email, phone)) {
            Json::Value result;
            result["success"] = true;
            result["message"] = "用户修改成功";
            return jsonResponse(result);
        }
        return errorResponse(400, "修改用户失败");
    } else if (request.method == "DELETE") {
        // 删除用户
        // 从URL路径中提取用户ID
        std::string path = request.path;
        size_t pos = path.find("/api/users/");
        if (pos == std::string::npos) {
            return errorResponse(400, "无效的URL路径");
        }
        
        std::string userIdStr = path.substr(pos + 11); // "/api/users/"的长度是11
        int userId;
        try {
            userId = std::stoi(userIdStr);
        } catch (const std::exception& e) {
            return errorResponse(400, "无效的用户ID");
        }
        
        if (librarySystem->deleteUser(userId)) {
            Json::Value result;
            result["success"] = true;
            result["message"] = "用户删除成功";
            return jsonResponse(result);
        }
        return errorResponse(400, "删除用户失败");
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
        std::string title, author, category, keywords, description;
        
        // 检查Content-Type来决定如何解析数据
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end() && 
            contentType->second.find("application/json") != std::string::npos) {
            // 解析JSON数据
            Json::Value jsonData = parseJsonBody(request.body);
            if (jsonData.isObject() && !jsonData["title"].isNull() && !jsonData["author"].isNull()) {
                title = jsonData["title"].asString();
                author = jsonData["author"].asString();
                category = jsonData["category"].isNull() ? "" : jsonData["category"].asString();
                keywords = jsonData["keywords"].isNull() ? "" : jsonData["keywords"].asString();
                description = jsonData["description"].isNull() ? "" : jsonData["description"].asString();
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        } else {
            // 解析form-urlencoded数据
            auto titleIt = request.postParams.find("title");
            auto authorIt = request.postParams.find("author");
            auto categoryIt = request.postParams.find("category");
            auto keywordsIt = request.postParams.find("keywords");
            auto descriptionIt = request.postParams.find("description");
            
            if (titleIt != request.postParams.end() && authorIt != request.postParams.end()) {
                title = titleIt->second;
                author = authorIt->second;
                category = (categoryIt != request.postParams.end()) ? categoryIt->second : "";
                keywords = (keywordsIt != request.postParams.end()) ? keywordsIt->second : "";
                description = (descriptionIt != request.postParams.end()) ? descriptionIt->second : "";
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        }
        
        int bookId = librarySystem->addBook(title, author, category, keywords, description);
        if (bookId > 0) {
            Json::Value result;
            result["success"] = true;
            result["bookId"] = bookId;
            result["message"] = "图书添加成功";
            return jsonResponse(result);
        }
        return errorResponse(400, "添加图书失败");
    } else if (request.method == "PUT") {
        // 修改图书
        // 从URL路径中提取图书ID
        std::string path = request.path;
        size_t pos = path.find("/api/books/");
        if (pos == std::string::npos) {
            return errorResponse(400, "无效的URL路径");
        }
        
        std::string bookIdStr = path.substr(pos + 11); // "/api/books/"的长度是11
        int bookId;
        try {
            bookId = std::stoi(bookIdStr);
        } catch (const std::exception& e) {
            return errorResponse(400, "无效的图书ID");
        }
        
        std::string title, author, category, keywords, description;
        
        // 解析JSON数据
        Json::Value jsonData = parseJsonBody(request.body);
        if (jsonData.isObject() && !jsonData["title"].isNull() && !jsonData["author"].isNull()) {
            title = jsonData["title"].asString();
            author = jsonData["author"].asString();
            category = jsonData["category"].isNull() ? "" : jsonData["category"].asString();
            keywords = jsonData["keywords"].isNull() ? "" : jsonData["keywords"].asString();
            description = jsonData["description"].isNull() ? "" : jsonData["description"].asString();
        } else {
            return errorResponse(400, "缺少必要参数");
        }
        
        if (librarySystem->updateBook(bookId, title, author, category, keywords, description)) {
            Json::Value result;
            result["success"] = true;
            result["message"] = "图书修改成功";
            return jsonResponse(result);
        }
        return errorResponse(400, "修改图书失败");
    } else if (request.method == "DELETE") {
        // 删除图书
        // 从URL路径中提取图书ID
        std::string path = request.path;
        size_t pos = path.find("/api/books/");
        if (pos == std::string::npos) {
            return errorResponse(400, "无效的URL路径");
        }
        
        std::string bookIdStr = path.substr(pos + 11); // "/api/books/"的长度是11
        int bookId;
        try {
            bookId = std::stoi(bookIdStr);
        } catch (const std::exception& e) {
            return errorResponse(400, "无效的图书ID");
        }
        
        if (librarySystem->deleteBook(bookId)) {
            Json::Value result;
            result["success"] = true;
            result["message"] = "图书删除成功";
            return jsonResponse(result);
        }
        return errorResponse(400, "删除图书失败");
    }
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleApiBorrow(const HttpRequest& request) {
    if (request.method == "POST") {
        std::string userIdStr, bookIdStr;
        
        // 检查Content-Type来决定如何解析数据
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end() && 
            contentType->second.find("application/json") != std::string::npos) {
            // 解析JSON数据
            Json::Value jsonData = parseJsonBody(request.body);
            if (jsonData.isObject() && !jsonData["userId"].isNull() && !jsonData["bookId"].isNull()) {
                userIdStr = jsonData["userId"].asString();
                bookIdStr = jsonData["bookId"].asString();
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        } else {
            // 解析form-urlencoded数据
            auto userIdIt = request.postParams.find("userId");
            auto bookIdIt = request.postParams.find("bookId");
            
            if (userIdIt != request.postParams.end() && bookIdIt != request.postParams.end()) {
                userIdStr = userIdIt->second;
                bookIdStr = bookIdIt->second;
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        }
        
        try {
            int userId = std::stoi(userIdStr);
            int bookId = std::stoi(bookIdStr);
            
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
    return errorResponse(405, "Method Not Allowed");
}

HttpResponse HttpServer::handleApiReturn(const HttpRequest& request) {
    if (request.method == "POST") {
        std::string userIdStr, bookIdStr;
        
        // 检查Content-Type来决定如何解析数据
        auto contentType = request.headers.find("Content-Type");
        if (contentType != request.headers.end() && 
            contentType->second.find("application/json") != std::string::npos) {
            // 解析JSON数据
            Json::Value jsonData = parseJsonBody(request.body);
            if (jsonData.isObject() && !jsonData["userId"].isNull() && !jsonData["bookId"].isNull()) {
                userIdStr = jsonData["userId"].asString();
                bookIdStr = jsonData["bookId"].asString();
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        } else {
            // 解析form-urlencoded数据
            auto userIdIt = request.postParams.find("userId");
            auto bookIdIt = request.postParams.find("bookId");
            
            if (userIdIt != request.postParams.end() && bookIdIt != request.postParams.end()) {
                userIdStr = userIdIt->second;
                bookIdStr = bookIdIt->second;
            } else {
                return errorResponse(400, "缺少必要参数");
            }
        }
        
        try {
            int userId = std::stoi(userIdStr);
            int bookId = std::stoi(bookIdStr);
            
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
    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".png") return "image/png";
    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".jpg") return "image/jpeg";
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".jpeg") return "image/jpeg";
    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".svg") return "image/svg+xml";
    return "text/plain";
}

std::string HttpServer::readFile(const std::string& filename) {
    // 检查文件扩展名，决定是否以二进制模式打开
    bool isBinary = false;
    if (filename.size() >= 4) {
        std::string ext = filename.substr(filename.size() - 4);
        if (ext == ".png" || ext == ".jpg" || ext == ".gif" || ext == ".ico") {
            isBinary = true;
        }
    }
    if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".jpeg") {
        isBinary = true;
    }
    
    std::ifstream file(filename, isBinary ? std::ios::binary : std::ios::in);
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

std::string HttpServer::generateLoginPage() {
    return R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>图书管理系统 - 登录</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <style>
        :root {
            --bg-color: #ffffff;
            --text-color: #37352f;
            --border-color: #e9e9e7;
            --hover-color: #f7f6f3;
            --primary-color: #2383e2;
        }
        
        [data-theme="dark"] {
            --bg-color: #191919;
            --text-color: #e9e9e7;
            --border-color: #373737;
            --hover-color: #2f2f2f;
            --primary-color: #529cca;
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, 'Apple Color Emoji', Arial, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-color);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: all 0.3s ease;
        }
        
        .login-container {
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 48px;
            width: 100%;
            max-width: 400px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
        }
        
        .logo-container {
            text-align: center;
            margin-bottom: 32px;
        }
        
        .title {
            font-size: 24px;
            font-weight: 600;
            margin-bottom: 8px;
            color: var(--text-color);
        }
        
        .subtitle {
            font-size: 14px;
            color: var(--text-color);
            opacity: 0.7;
        }
        
        .form-group {
            margin-bottom: 20px;
            position: relative;
        }
        
        .form-group label {
            display: block;
            margin-bottom: 6px;
            font-size: 14px;
            font-weight: 500;
            color: var(--text-color);
        }
        
        .input-wrapper {
            position: relative;
        }
        
        .input-icon {
            position: absolute;
            left: 12px;
            top: 50%;
            transform: translateY(-50%);
            color: var(--text-color);
            opacity: 0.5;
        }
        
        .form-group input {
            width: 100%;
            padding: 12px 12px 12px 40px;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            font-size: 16px;
            background-color: var(--bg-color);
            color: var(--text-color);
            transition: border-color 0.2s ease;
        }
        
        .form-group input:focus {
            outline: none;
            border-color: var(--primary-color);
            box-shadow: 0 0 0 3px rgba(35, 131, 226, 0.1);
        }
        
        .btn {
            width: 100%;
            padding: 12px;
            background-color: var(--primary-color);
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 16px;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }
        
        .btn:hover {
            background-color: #1a73d1;
            transform: translateY(-1px);
        }
        
        .theme-toggle {
            position: absolute;
            top: 20px;
            right: 20px;
            background: none;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 8px 12px;
            cursor: pointer;
            color: var(--text-color);
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            gap: 6px;
        }
        
        .theme-toggle:hover {
            background-color: var(--hover-color);
        }
        
        .error-message {
            color: #dc2626;
            font-size: 14px;
            margin-top: 8px;
            display: none;
        }
    </style>
</head>
<body>
    <button class="theme-toggle" onclick="toggleTheme()">
        <i id="theme-icon" class="fas fa-moon"></i>
        <span id="theme-text">深色模式</span>
    </button>
    
    <div class="login-container">
        <div class="logo-container">
            <h1 class="title">图书管理系统</h1>
            <p class="subtitle">请输入您的账户信息登录</p>
        </div>
        
        <form id="loginForm">
            <div class="form-group">
                <label for="username">
                    <i class="fas fa-user"></i> 用户名
                </label>
                <div class="input-wrapper">
                    <i class="fas fa-user input-icon"></i>
                    <input type="text" id="username" name="username" required placeholder="请输入用户名">
                </div>
            </div>
            
            <div class="form-group">
                <label for="password">
                    <i class="fas fa-lock"></i> 密码
                </label>
                <div class="input-wrapper">
                    <i class="fas fa-lock input-icon"></i>
                    <input type="password" id="password" name="password" required placeholder="请输入密码">
                </div>
            </div>
            
            <div class="error-message" id="errorMessage"></div>
            
            <button type="submit" class="btn">
                <i class="fas fa-sign-in-alt"></i>
                登录
            </button>
        </form>
    </div>
    
    <script>
        function toggleTheme() {
            const body = document.body;
            const themeIcon = document.getElementById('theme-icon');
            const themeText = document.getElementById('theme-text');
            
            if (body.getAttribute('data-theme') === 'dark') {
                body.removeAttribute('data-theme');
                themeIcon.className = 'fas fa-moon';
                themeText.textContent = '深色模式';
                localStorage.setItem('theme', 'light');
            } else {
                body.setAttribute('data-theme', 'dark');
                themeIcon.className = 'fas fa-sun';
                themeText.textContent = '浅色模式';
                localStorage.setItem('theme', 'dark');
            }
        }
        
        // 初始化主题
        function initTheme() {
            const savedTheme = localStorage.getItem('theme');
            const themeIcon = document.getElementById('theme-icon');
            const themeText = document.getElementById('theme-text');
            
            if (savedTheme === 'dark') {
                document.body.setAttribute('data-theme', 'dark');
                themeIcon.className = 'fas fa-sun';
                themeText.textContent = '浅色模式';
            }
        }
        
        // 显示错误信息
        function showError(message) {
            const errorElement = document.getElementById('errorMessage');
            errorElement.textContent = message;
            errorElement.style.display = 'block';
        }
        
        // 隐藏错误信息
        function hideError() {
            const errorElement = document.getElementById('errorMessage');
            errorElement.style.display = 'none';
        }
        
        // 登录处理
        document.getElementById('loginForm').addEventListener('submit', function(e) {
            e.preventDefault();
            hideError();
            
            const username = document.getElementById('username').value;
            const password = document.getElementById('password').value;
            
            // 发送登录请求到后端
            fetch('/api/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/x-www-form-urlencoded',
                },
                body: `username=${encodeURIComponent(username)}&password=${encodeURIComponent(password)}`
            })
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    localStorage.setItem('userType', data.userType);
                    localStorage.setItem('username', data.username);
                    localStorage.setItem('userId', data.userId);
                    window.location.href = '/';
                } else {
                    showError(data.message || '登录失败，请检查用户名和密码');
                }
            })
            .catch(error => {
                console.error('登录错误:', error);
                showError('网络错误，请稍后重试');
            });
        });
        
        // 页面加载时初始化主题
        initTheme();
    </script>
</body>
</html>
)HTML";
}

std::string HttpServer::generateIndexPage() {
    return R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>图书管理系统</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
    <style>
        :root {
            --bg-color: #ffffff;
            --text-color: #37352f;
            --border-color: #e9e9e7;
            --hover-color: #f7f6f3;
            --primary-color: #2383e2;
            --secondary-color: #6b7280;
            --success-color: #059669;
            --warning-color: #d97706;
            --error-color: #dc2626;
            --sidebar-bg: #f8f9fa;
            --sidebar-width: 250px;
        }
        
        [data-theme="dark"] {
            --bg-color: #191919;
            --text-color: #e9e9e7;
            --border-color: #373737;
            --hover-color: #2f2f2f;
            --primary-color: #529cca;
            --secondary-color: #9ca3af;
            --success-color: #10b981;
            --warning-color: #f59e0b;
            --error-color: #ef4444;
            --sidebar-bg: #2d2d2d;
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Helvetica, 'Apple Color Emoji', Arial, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-color);
            line-height: 1.5;
            transition: all 0.3s ease;
            display: flex;
            min-height: 100vh;
        }
        
        .top-bar {
            position: fixed;
            top: 0;
            right: 0;
            left: var(--sidebar-width);
            height: 60px;
            background-color: var(--bg-color);
            border-bottom: 1px solid var(--border-color);
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 0 20px;
            gap: 12px;
            z-index: 1000;
            transition: left 0.3s ease;
        }
        
        .top-search {
            flex: 1;
            max-width: 400px;
            position: relative;
        }
        
        .top-search input {
            width: 100%;
            padding: 8px 12px 8px 36px;
            border: 1px solid var(--border-color);
            border-radius: 20px;
            font-size: 14px;
            background-color: var(--hover-color);
            color: var(--text-color);
            transition: all 0.2s ease;
        }
        
        .top-search input:focus {
            outline: none;
            border-color: var(--primary-color);
            box-shadow: 0 0 0 3px rgba(35, 131, 226, 0.1);
        }
        
        .top-search i {
            position: absolute;
            left: 12px;
            top: 50%;
            transform: translateY(-50%);
            color: var(--secondary-color);
        }
        
        .search-results {
            position: absolute;
            top: 100%;
            left: 0;
            right: 0;
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            border-top: none;
            border-radius: 0 0 6px 6px;
            max-height: 300px;
            overflow-y: auto;
            z-index: 1000;
            display: none;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }
        
        .search-result-item {
            padding: 10px 12px;
            border-bottom: 1px solid var(--border-color);
            cursor: pointer;
            transition: background-color 0.2s;
        }
        
        .search-result-item:hover {
            background: var(--hover-color);
        }
        
        .search-result-item:last-child {
            border-bottom: none;
        }
        
        .search-result-type {
            font-size: 12px;
            color: var(--secondary-color);
            margin-bottom: 2px;
            display: flex;
            align-items: center;
            gap: 4px;
        }
        
        .search-result-title {
            font-weight: 500;
            margin-bottom: 2px;
        }
        
        .search-result-subtitle {
            font-size: 12px;
            color: var(--secondary-color);
        }
        
        .no-results {
            padding: 15px 12px;
            text-align: center;
            color: var(--secondary-color);
            font-size: 14px;
        }
        
        .search-dropdown {
            position: absolute;
            top: 100%;
            left: 0;
            right: 0;
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            border-top: none;
            border-radius: 0 0 6px 6px;
            max-height: 200px;
            overflow-y: auto;
            z-index: 1000;
            display: none;
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }
        
        .search-dropdown-item {
            padding: 8px 12px;
            border-bottom: 1px solid var(--border-color);
            cursor: pointer;
            transition: background-color 0.2s;
            font-size: 14px;
        }
        
        .search-dropdown-item:hover {
            background: var(--hover-color);
        }
        
        .search-dropdown-item:last-child {
            border-bottom: none;
        }
        
        .search-dropdown-item .item-title {
            font-weight: 500;
            margin-bottom: 2px;
        }
        
        .search-dropdown-item .item-subtitle {
            font-size: 12px;
            color: var(--secondary-color);
        }
        
        .data-table tbody tr.highlight {
            background-color: rgba(35, 131, 226, 0.1);
            border-left: 3px solid var(--primary-color);
            animation: highlightFade 3s ease-out;
        }
        
        @keyframes highlightFade {
            0% {
                background-color: rgba(35, 131, 226, 0.3);
            }
            100% {
                background-color: rgba(35, 131, 226, 0.1);
            }
        }
        
        .modal-overlay {
            position: fixed;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: rgba(0, 0, 0, 0.5);
            display: none;
            justify-content: center;
            align-items: center;
            z-index: 2000;
        }
        
        .modal-card {
            background: var(--bg-color);
            border-radius: 8px;
            padding: 24px;
            max-width: 500px;
            width: 90%;
            max-height: 80vh;
            overflow-y: auto;
            box-shadow: 0 10px 25px rgba(0, 0, 0, 0.2);
        }
        
        .modal-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 20px;
            padding-bottom: 12px;
            border-bottom: 1px solid var(--border-color);
        }
        
        .modal-title {
            font-size: 18px;
            font-weight: 600;
            color: var(--text-color);
        }
        
        .modal-close {
            background: none;
            border: none;
            font-size: 20px;
            cursor: pointer;
            color: var(--secondary-color);
            padding: 4px;
            border-radius: 4px;
            transition: all 0.2s ease;
        }
        
        .modal-close:hover {
            background: var(--hover-color);
            color: var(--text-color);
        }
        
        .add-button {
            background: var(--primary-color);
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px 12px;
            cursor: pointer;
            font-size: 14px;
            display: flex;
            align-items: center;
            gap: 6px;
            margin-bottom: 16px;
            transition: all 0.2s ease;
        }
        
        .add-button:hover {
            background: #1d72c7;
        }
        
        .action-buttons {
            display: flex;
            gap: 4px;
            padding-right: 8px;
        }
        
        .action-btn {
            background: none;
            border: none;
            padding: 6px;
            border-radius: 4px;
            cursor: pointer;
            font-size: 14px;
            transition: all 0.2s ease;
            color: var(--secondary-color);
        }
        
        .action-btn:hover {
            background: var(--hover-color);
            color: var(--text-color);
        }
        
        .action-btn.edit {
            color: var(--primary-color);
        }
        
        .action-btn.delete {
            color: var(--error-color);
        }
        
        .action-btn.edit:hover {
            background: rgba(35, 131, 226, 0.1);
        }
        
        .action-btn.delete:hover {
            background: rgba(220, 38, 38, 0.1);
        }
        
        .top-bar-right {
            display: flex;
            align-items: center;
            gap: 12px;
        }
        
        .top-bar.sidebar-collapsed {
            left: 60px;
        }
        
        .user-dropdown {
            position: relative;
        }
        
        .user-button {
            display: flex;
            align-items: center;
            gap: 8px;
            background: none;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 8px 12px;
            cursor: pointer;
            color: var(--text-color);
            transition: all 0.2s ease;
            font-size: 14px;
        }
        
        .user-button:hover {
            background-color: var(--hover-color);
        }
        
        .dropdown-menu {
            position: absolute;
            top: calc(100% + 8px);
            left: 0;
            background-color: var(--bg-color);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1);
            min-width: 150px;
            display: none;
            z-index: 1001;
        }
        
        .dropdown-menu.show {
            display: block;
        }
        
        .dropdown-item {
            display: flex;
            align-items: center;
            gap: 8px;
            padding: 12px 16px;
            color: var(--text-color);
            text-decoration: none;
            transition: all 0.2s ease;
            cursor: pointer;
            border: none;
            background: none;
            width: 100%;
            text-align: left;
            font-size: 14px;
        }
        
        .dropdown-item:hover {
            background-color: var(--hover-color);
        }
        
        .sidebar {
            width: var(--sidebar-width);
            background-color: var(--sidebar-bg);
            border-right: 1px solid var(--border-color);
            padding: 20px 0;
            position: fixed;
            height: 100vh;
            overflow-y: auto;
            transition: width 0.3s ease, transform 0.3s ease;
        }
        
        .sidebar.collapsed {
            width: 60px;
        }
        
        .sidebar-toggle {
            position: absolute;
            top: 15px;
            right: 15px;
            width: 30px;
            height: 30px;
            background-color: var(--bg-color);
            border: 1px solid var(--border-color);
            border-radius: 50%;
            display: flex;
            align-items: center;
            justify-content: center;
            cursor: pointer;
            z-index: 1001;
            transition: all 0.2s ease;
        }
        
        .sidebar-toggle:hover {
            background-color: var(--hover-color);
        }
        
        .sidebar-header {
            padding: 0 20px 20px;
            border-bottom: 1px solid var(--border-color);
            margin-bottom: 20px;
            transition: all 0.3s ease;
        }
        
        .sidebar.collapsed .sidebar-header {
            padding: 0 10px 20px;
            text-align: center;
        }
        
        .sidebar-header h1 {
            font-size: 18px;
            font-weight: 600;
            color: var(--text-color);
            margin-bottom: 8px;
            transition: all 0.3s ease;
        }
        
        .sidebar.collapsed .sidebar-header h1 {
            font-size: 0;
        }
        
        .sidebar.collapsed .sidebar-header h1 i {
            font-size: 20px;
        }
        
        .nav-menu {
            list-style: none;
        }
        
        .nav-item {
            margin-bottom: 4px;
        }
        
        .nav-link {
            display: flex;
            align-items: center;
            gap: 12px;
            padding: 0 20px;
            color: var(--text-color);
            text-decoration: none;
            transition: all 0.2s ease;
            cursor: pointer;
            border: none;
            background: none;
            width: 100%;
            text-align: left;
            font-size: 14px;
            position: relative;
            height: 48px;
        }
        
        .sidebar.collapsed .nav-link {
            padding: 0 12px;
            justify-content: center;
        }
        
        .nav-link:hover {
            background-color: var(--hover-color);
        }
        
        .nav-link.active {
            background-color: var(--primary-color);
            color: white;
        }
        
        .nav-link i {
            width: 16px;
            text-align: center;
            flex-shrink: 0;
        }
        
        .nav-link span {
            transition: all 0.3s ease;
        }
        
        .sidebar.collapsed .nav-link span {
            opacity: 0;
            width: 0;
            overflow: hidden;
        }
        
        .sidebar.collapsed .nav-link {
            position: relative;
        }
        
        .sidebar.collapsed .nav-link:hover::after {
            content: attr(data-tooltip);
            position: absolute;
            left: 100%;
            top: 50%;
            transform: translateY(-50%);
            background-color: var(--text-color);
            color: var(--bg-color);
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            white-space: nowrap;
            margin-left: 8px;
            z-index: 1002;
        }
        
        .main-container {
            flex: 1;
            margin-left: var(--sidebar-width);
            margin-top: 60px;
            padding: 20px;
            transition: margin-left 0.3s ease;
        }
        
        .main-container.sidebar-collapsed {
            margin-left: 60px;
        }
        
        .content-area {
            max-width: 1000px;
            margin: 0 auto;
        }
        
        .content-section {
            display: none;
        }
        
        .content-section.active {
            display: block;
        }
        
        .section {
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 24px;
            transition: all 0.2s ease;
        }
        
        .section:hover {
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
        }
        
        .section h2 {
            color: var(--text-color);
            margin-bottom: 16px;
            font-size: 18px;
            font-weight: 600;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 8px;
        }
        
        .form-group {
            margin-bottom: 16px;
        }
        
        .form-group label {
            display: block;
            margin-bottom: 6px;
            font-size: 14px;
            font-weight: 500;
            color: var(--text-color);
        }
        
        .form-group input, .form-group textarea, .form-group select {
            width: 100%;
            padding: 12px;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            font-size: 14px;
            background-color: var(--bg-color);
            color: var(--text-color);
            transition: border-color 0.2s ease;
        }
        
        .form-group input:focus, .form-group textarea:focus, .form-group select:focus {
            outline: none;
            border-color: var(--primary-color);
            box-shadow: 0 0 0 3px rgba(35, 131, 226, 0.1);
        }
        
        .btn {
            background-color: var(--text-color);
            color: var(--bg-color);
            border: 1px solid var(--text-color);
            padding: 12px 16px;
            border-radius: 6px;
            font-size: 14px;
            font-weight: 500;
            cursor: pointer;
            transition: all 0.2s ease;
            width: 100%;
        }
        
        .btn:hover {
            background-color: var(--bg-color);
            color: var(--text-color);
            transform: translateY(-1px);
        }
        
        .btn-primary {
            background-color: var(--primary-color);
            color: white;
            border: 1px solid var(--primary-color);
        }
        
        .btn-primary:hover {
            background-color: transparent;
            color: var(--primary-color);
        }
        
        .btn-success {
            background-color: var(--success-color);
            color: white;
            border: 1px solid var(--success-color);
        }
        
        .btn-success:hover {
            background-color: transparent;
            color: var(--success-color);
        }
        
        .btn-danger {
            background-color: var(--error-color);
            color: white;
            border: 1px solid var(--error-color);
        }
        
        .btn-danger:hover {
            background-color: transparent;
            color: var(--error-color);
        }
        
        .data-section {
            grid-column: 1 / -1;
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 24px;
        }
        
        .tabs {
            display: flex;
            margin-bottom: 24px;
            border-bottom: 1px solid var(--border-color);
        }
        
        .tab {
            padding: 12px 16px;
            background: none;
            border: none;
            font-size: 14px;
            font-weight: 500;
            cursor: pointer;
            color: var(--secondary-color);
            transition: all 0.2s ease;
            border-bottom: 2px solid transparent;
        }
        
        .tab.active {
            color: var(--primary-color);
            border-bottom-color: var(--primary-color);
        }
        
        .tab:hover {
            color: var(--text-color);
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
            margin-top: 16px;
        }
        
        .data-table th, .data-table td {
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid var(--border-color);
        }
        
        .data-table th {
            background-color: var(--hover-color);
            font-weight: 500;
            color: var(--text-color);
            font-size: 14px;
        }
        
        .data-table tr:hover {
            background-color: var(--hover-color);
        }
        
        .status-available {
            color: var(--success-color);
            font-weight: 500;
        }
        
        .status-borrowed {
            color: var(--error-color);
            font-weight: 500;
        }
        
        .search-box {
            margin-bottom: 16px;
        }
        
        .search-box input {
            width: 100%;
            padding: 12px;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            font-size: 14px;
            background-color: var(--bg-color);
            color: var(--text-color);
        }
        
        .message {
            padding: 12px;
            border-radius: 6px;
            margin-bottom: 16px;
            display: none;
            font-size: 14px;
        }
        
        .message.success {
            background-color: #dcfce7;
            color: var(--success-color);
            border: 1px solid #bbf7d0;
        }
        
        .message.error {
            background-color: #fef2f2;
            color: var(--error-color);
            border: 1px solid #fecaca;
        }
        
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 16px;
            margin-bottom: 24px;
        }
        
        .stat-card {
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            color: var(--text-color);
            padding: 20px;
            border-radius: 8px;
            text-align: center;
            transition: all 0.2s ease;
        }
        
        .stat-card:hover {
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06);
        }
        
        .stat-card h3 {
            font-size: 2rem;
            margin-bottom: 8px;
            color: var(--primary-color);
            font-weight: 600;
        }
        
        .stat-card p {
            color: var(--secondary-color);
            font-size: 14px;
        }
        
        /* 用户界面样式 */
        .user-dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
            gap: 20px;
            padding: 20px 0;
        }
        
        .dashboard-card {
            background: var(--bg-color);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            overflow: hidden;
            transition: all 0.3s ease;
            box-shadow: 0 2px 4px rgba(0, 0, 0, 0.05);
        }
        
        .dashboard-card:hover {
            box-shadow: 0 8px 25px rgba(0, 0, 0, 0.1);
            transform: translateY(-2px);
        }
        
        .card-header {
            background: linear-gradient(135deg, var(--primary-color), #4f46e5);
            color: white;
            padding: 16px 20px;
            font-weight: 600;
        }
        
        .card-header h3 {
            margin: 0;
            font-size: 16px;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        
        .card-content {
            padding: 20px;
            min-height: 200px;
        }
        
        .loading {
            text-align: center;
            color: var(--secondary-color);
            padding: 40px 0;
        }
        
        .search-container {
            display: flex;
            gap: 8px;
            margin-bottom: 16px;
        }
        
        .search-input {
            flex: 1;
            padding: 12px;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            font-size: 14px;
            background-color: var(--bg-color);
            color: var(--text-color);
        }
        
        .search-btn {
            padding: 12px 16px;
            background-color: var(--primary-color);
            color: white;
            border: none;
            border-radius: 6px;
            cursor: pointer;
            transition: background-color 0.2s ease;
        }
        
        .search-btn:hover {
            background-color: #1d4ed8;
        }
        
        .book-results {
            max-height: 300px;
            overflow-y: auto;
        }
        
        .book-item {
            padding: 12px;
            border: 1px solid var(--border-color);
            border-radius: 6px;
            margin-bottom: 8px;
            background: var(--hover-color);
        }
        
        .book-item h4 {
            margin: 0 0 4px 0;
            color: var(--text-color);
            font-size: 14px;
        }
        
        .book-item p {
            margin: 0;
            color: var(--secondary-color);
            font-size: 12px;
        }
        
        .borrow-item {
            padding: 12px;
            border-left: 4px solid var(--primary-color);
            background: var(--hover-color);
            margin-bottom: 8px;
            border-radius: 0 6px 6px 0;
        }
        
        .borrow-item h4 {
            margin: 0 0 4px 0;
            color: var(--text-color);
            font-size: 14px;
        }
        
        .borrow-item p {
            margin: 0;
            color: var(--secondary-color);
            font-size: 12px;
        }
        
        .stats-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 12px 0;
            border-bottom: 1px solid var(--border-color);
        }
        
        .stats-item:last-child {
            border-bottom: none;
        }
        
        .stats-label {
            color: var(--text-color);
            font-weight: 500;
        }
        
        .stats-value {
            color: var(--primary-color);
            font-weight: 600;
        }
        
        .heatmap-card {
            grid-column: 1 / -1;
        }
        
        .heatmap-container {
            min-height: 300px;
            background: var(--hover-color);
            border-radius: 8px;
            padding: 20px;
            text-align: center;
        }
        
        .heatmap-grid {
            display: grid;
            grid-template-columns: repeat(7, 1fr);
            gap: 2px;
            max-width: 800px;
            margin: 0 auto;
        }
        
        .heatmap-day {
            aspect-ratio: 1;
            border-radius: 2px;
            cursor: pointer;
            transition: all 0.2s ease;
            position: relative;
            min-height: 12px;
        }
        
        .heatmap-day:hover {
            transform: scale(1.1);
            z-index: 10;
        }
        
        .heatmap-tooltip {
            position: absolute;
            bottom: 100%;
            left: 50%;
            transform: translateX(-50%);
            background: var(--text-color);
            color: var(--bg-color);
            padding: 4px 8px;
            border-radius: 4px;
            font-size: 12px;
            white-space: nowrap;
            opacity: 0;
            pointer-events: none;
            transition: opacity 0.2s ease;
            z-index: 1000;
        }
        
        .heatmap-day:hover .heatmap-tooltip {
            opacity: 1;
        }
        
        @media (max-width: 768px) {
            .sidebar {
                width: 100%;
                height: auto;
                position: relative;
                border-right: none;
                border-bottom: 1px solid var(--border-color);
            }
            
            .main-container {
                margin-left: 0;
                padding: 10px;
            }
            
            .nav-menu {
                display: flex;
                overflow-x: auto;
                padding: 0 10px;
            }
            
            .nav-item {
                margin-bottom: 0;
                margin-right: 4px;
                flex-shrink: 0;
            }
            
            .nav-link {
                padding: 8px 12px;
                font-size: 12px;
                white-space: nowrap;
            }
            
            .sidebar-header h1 {
                font-size: 16px;
            }
            
            .theme-toggle {
                font-size: 10px;
                padding: 4px 8px;
            }
            
            .user-dashboard {
                grid-template-columns: 1fr;
                gap: 16px;
            }
            
            .dashboard-card {
                min-width: unset;
            }
        }
    </style>
</head>
<body data-theme="light">
    <!-- 顶部栏 -->
    <div class="top-bar">
        <div class="top-search">
            <i class="fas fa-search"></i>
            <input type="text" id="globalSearch" placeholder="搜索用户或图书..." onkeyup="performGlobalSearch()" onfocus="showSearchResults()" onblur="hideSearchResults()">
            <div class="search-results" id="searchResults"></div>
        </div>
        <div class="top-bar-right">
            <div class="user-dropdown">
                <button class="user-button" onclick="toggleUserDropdown()">
                    <i class="fas fa-user-circle"></i>
                    <span id="currentUser">管理员</span>
                </button>
                <div class="dropdown-menu" id="userDropdown">
                    <button class="dropdown-item" onclick="toggleTheme()">
                        <i class="fas fa-moon" id="themeIcon"></i>
                        <span id="themeText">夜间模式</span>
                    </button>
                    <button class="dropdown-item" onclick="logout()">
                        <i class="fas fa-sign-out-alt"></i>
                        退出登录
                    </button>
        </div>
    </div>
    
    <!-- 左侧导航栏 -->
    <div class="sidebar" id="sidebar">
        <button class="sidebar-toggle" onclick="toggleSidebar()">
            <i class="fas fa-bars"></i>
        </button>
        <div class="sidebar-header">
            <h1><i class="fas fa-book"></i> <span>图书管理系统</span></h1>
        </div>
        
        <nav class="nav-menu">
            <div class="nav-item">
                <button class="nav-link active" onclick="showSection('users')" data-tooltip="用户管理">
                    <i class="fas fa-users"></i>
                    <span>用户管理</span>
                </button>
            </div>
            <div class="nav-item">
                <button class="nav-link" onclick="showSection('books')" data-tooltip="图书管理">
                    <i class="fas fa-book"></i>
                    <span>图书管理</span>
                </button>
            </div>
            <div class="nav-item">
                <button class="nav-link" onclick="showSection('borrow-return')" data-tooltip="借阅归还管理">
                    <i class="fas fa-exchange-alt"></i>
                    <span>借阅归还管理</span>
                </button>
            </div>
            <div class="nav-item">
                <button class="nav-link" onclick="showSection('statistics')" data-tooltip="统计分析">
                    <i class="fas fa-chart-bar"></i>
                    <span>统计分析</span>
                </button>
            </div>
        </nav>
    </div>
    
    <!-- 管理员界面 -->
    <div id="admin-interface" style="display: block;">
        <!-- 主内容区域 -->
        <div class="main-container">
            <div class="content-area">
            <!-- 用户管理 -->
            <div id="users-section" class="content-section active">
                <div class="section">
                    <h2><i class="fas fa-users"></i> 用户管理</h2>
                    <div id="userMessage" class="message"></div>
                    <button class="add-button" onclick="openUserModal()">
                        <i class="fas fa-plus"></i>
                        添加用户
                    </button>
                    <table class="data-table" id="usersTable">
                        <thead>
                            <tr>
                                <th width="80">操作</th>
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
            </div>
            
            <!-- 图书管理 -->
            <div id="books-section" class="content-section">
                <div class="section">
                    <h2><i class="fas fa-book"></i> 图书管理</h2>
                    <div id="bookMessage" class="message"></div>
                    <button class="add-button" onclick="openBookModal()">
                        <i class="fas fa-plus"></i>
                        添加图书
                    </button>
                    <table class="data-table" id="booksTable">
                        <thead>
                            <tr>
                                <th width="80">操作</th>
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
            </div>
            
            <!-- 借阅归还管理 -->
            <div id="borrow-return-section" class="content-section">
                <div class="section">
                    <h2><i class="fas fa-exchange-alt"></i> 借阅归还管理</h2>
                    <div id="borrowReturnMessage" class="message"></div>
                    
                    <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 24px; margin-top: 20px;">
                        <!-- 借阅管理 -->
                        <div style="border: 1px solid var(--border-color); border-radius: 8px; padding: 20px;">
                            <h3 style="margin-bottom: 16px; color: var(--primary-color); display: flex; align-items: center; gap: 8px;">
                                <i class="fas fa-hand-holding"></i> 借阅图书
                            </h3>
                            <form id="borrowForm">
                                <div class="form-group">
                                    <label for="borrowUserInput">用户ID或姓名</label>
                                    <div style="position: relative;">
                                        <input type="text" id="borrowUserInput" placeholder="输入用户ID或姓名搜索..." required autocomplete="off">
                                        <div class="search-dropdown" id="borrowUserDropdown"></div>
                                    </div>
                                    <input type="hidden" id="borrowUserId" name="userId">
                                </div>
                                <div class="form-group">
                                    <label for="borrowBookInput">图书ID或书名</label>
                                    <div style="position: relative;">
                                        <input type="text" id="borrowBookInput" placeholder="输入图书ID或书名搜索..." required autocomplete="off">
                                        <div class="search-dropdown" id="borrowBookDropdown"></div>
                                    </div>
                                    <input type="hidden" id="borrowBookId" name="bookId">
                                </div>
                                <button type="submit" class="btn btn-primary">借阅图书</button>
                            </form>
                        </div>
                        
                        <!-- 归还管理 -->
                        <div style="border: 1px solid var(--border-color); border-radius: 8px; padding: 20px;">
                            <h3 style="margin-bottom: 16px; color: var(--success-color); display: flex; align-items: center; gap: 8px;">
                                <i class="fas fa-undo"></i> 归还图书
                            </h3>
                            <form id="returnForm">
                                <div class="form-group">
                                    <label for="returnUserInput">用户ID或姓名</label>
                                    <div style="position: relative;">
                                        <input type="text" id="returnUserInput" placeholder="输入用户ID或姓名搜索..." required autocomplete="off">
                                        <div class="search-dropdown" id="returnUserDropdown"></div>
                                    </div>
                                    <input type="hidden" id="returnUserId" name="userId">
                                </div>
                                <div class="form-group">
                                    <label for="returnBookInput">图书ID或书名</label>
                                    <div style="position: relative;">
                                        <input type="text" id="returnBookInput" placeholder="输入图书ID或书名搜索..." required autocomplete="off">
                                        <div class="search-dropdown" id="returnBookDropdown"></div>
                                    </div>
                                    <input type="hidden" id="returnBookId" name="bookId">
                                </div>
                                <button type="submit" class="btn btn-success">归还图书</button>
                            </form>
                        </div>
                    </div>
                </div>
            </div>
            
            <!-- 统计分析 -->
            <div id="statistics-section" class="content-section">
                <div class="section">
                    <h2><i class="fas fa-chart-bar"></i> 统计分析</h2>
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
    </div>
    
    <!-- 弹出卡片 -->
    <div class="modal-overlay" id="modalOverlay">
        <div class="modal-card">
            <div class="modal-header">
                <h3 class="modal-title" id="modalTitle">添加用户</h3>
                <button class="modal-close" onclick="closeModal()">
                    <i class="fas fa-times"></i>
                </button>
            </div>
            <div class="modal-body">
                <form id="modalForm">
                    <div id="modalFormContent"></div>
                    <div class="form-group" style="margin-top: 20px; text-align: right;">
                        <button type="button" class="btn" onclick="closeModal()" style="margin-right: 10px; background: #6c757d;">取消</button>
                        <button type="submit" class="btn" id="modalSubmitBtn">确定</button>
                    </div>
                </form>
            </div>
        </div>
    </div>
    </div> <!-- 关闭管理员界面 -->
    
    <!-- 用户界面 -->
    <div id="user-interface" style="display: none; margin-left: 0; width: 100%; padding: 20px;">
        <!-- 用户面板 -->
        <div class="user-dashboard">
                    <!-- 当前借阅 -->
                    <div class="dashboard-card">
                        <div class="card-header">
                            <h3><i class="fas fa-book-open"></i> 当前借阅</h3>
                        </div>
                        <div class="card-content" id="currentBorrowings">
                            <div class="loading">加载中...</div>
                        </div>
                    </div>
                    
                    <!-- 历史借阅 -->
                    <div class="dashboard-card">
                        <div class="card-header">
                            <h3><i class="fas fa-history"></i> 历史借阅</h3>
                        </div>
                        <div class="card-content" id="borrowHistory">
                            <div class="loading">加载中...</div>
                        </div>
                    </div>
                    
                    <!-- 借阅时长统计 -->
                    <div class="dashboard-card">
                        <div class="card-header">
                            <h3><i class="fas fa-chart-bar"></i> 借阅时长统计</h3>
                        </div>
                        <div class="card-content" id="borrowStats">
                            <div class="loading">加载中...</div>
                        </div>
                    </div>
                    
                    <!-- 图书检索 -->
                    <div class="dashboard-card">
                        <div class="card-header">
                            <h3><i class="fas fa-search"></i> 图书检索</h3>
                        </div>
                        <div class="card-content">
                            <div class="search-container">
                                <input type="text" id="userBookSearch" placeholder="搜索图书标题、作者或分类..." class="search-input">
                                <button onclick="searchBooksForUser()" class="search-btn">
                                    <i class="fas fa-search"></i>
                                </button>
                            </div>
                            <div id="userBookResults" class="book-results"></div>
                        </div>
                    </div>
                    
                    <!-- 图书热力图 -->
                    <div class="dashboard-card heatmap-card">
                        <div class="card-header">
                            <h3><i class="fas fa-fire"></i> 图书热力图</h3>
                        </div>
                        <div class="card-content">
                            <div id="heatmap" class="heatmap-container"></div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>
    
    <script>
        // 全局变量
        let allUsers = [];
        let allBooks = [];
        

        
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
        
        // 切换导航栏
        function showSection(sectionName) {
            // 隐藏所有内容区域
            const sections = document.querySelectorAll('.content-section');
            sections.forEach(section => section.classList.remove('active'));
            
            // 移除所有导航链接的活动状态
            const navLinks = document.querySelectorAll('.nav-link');
            navLinks.forEach(link => link.classList.remove('active'));
            
            // 显示选中的内容区域
            document.getElementById(sectionName + '-section').classList.add('active');
            
            // 激活对应的导航链接
            const targetLink = document.querySelector(`[onclick="showSection('${sectionName}')"]`);
            if (targetLink) {
                targetLink.classList.add('active');
            }
            
            // 清空全局搜索框
            clearGlobalSearch();
            
            // 如果是统计页面，重新加载数据
            if (sectionName === 'statistics') {
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
                    <td class="action-buttons">
                        <button class="action-btn edit" onclick="editUser(${user.id})" title="编辑">
                            <i class="fas fa-edit"></i>
                        </button>
                        <button class="action-btn delete" onclick="deleteUser(${user.id})" title="删除">
                            <i class="fas fa-trash"></i>
                        </button>
                    </td>
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
                    <td class="action-buttons">
                        <button class="action-btn edit" onclick="editBook(${book.id})" title="编辑">
                            <i class="fas fa-edit"></i>
                        </button>
                        <button class="action-btn delete" onclick="deleteBook(${book.id})" title="删除">
                            <i class="fas fa-trash"></i>
                        </button>
                    </td>
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
            const userId = document.getElementById('borrowUserId').value;
            const bookId = document.getElementById('borrowBookId').value;
            
            if (!userId || !bookId) {
                showMessage('borrowReturnMessage', '请选择用户和图书', 'error');
                return;
            }
            
            const formData = new FormData();
            formData.append('userId', userId);
            formData.append('bookId', bookId);
            
            try {
                const response = await fetch('/api/borrow', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('borrowReturnMessage', result.message, 'success');
                    e.target.reset();
                    document.getElementById('borrowUserId').value = '';
                    document.getElementById('borrowBookId').value = '';
                    loadUsers();
                    loadBooks();
                } else {
                    showMessage('borrowReturnMessage', result.message || '借阅失败', 'error');
                }
            } catch (error) {
                showMessage('borrowReturnMessage', '网络错误', 'error');
            }
        }
        
        // 归还管理
        async function handleReturnSubmit(e) {
            e.preventDefault();
            const userId = document.getElementById('returnUserId').value;
            const bookId = document.getElementById('returnBookId').value;
            
            if (!userId || !bookId) {
                showMessage('borrowReturnMessage', '请选择用户和图书', 'error');
                return;
            }
            
            const formData = new FormData();
            formData.append('userId', userId);
            formData.append('bookId', bookId);
            
            try {
                const response = await fetch('/api/return', {
                    method: 'POST',
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('borrowReturnMessage', result.message, 'success');
                    e.target.reset();
                    document.getElementById('returnUserId').value = '';
                    document.getElementById('returnBookId').value = '';
                    loadUsers();
                    loadBooks();
                } else {
                    showMessage('borrowReturnMessage', result.message || '归还失败', 'error');
                }
            } catch (error) {
                showMessage('borrowReturnMessage', '网络错误', 'error');
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
        
        // 侧边栏折叠功能
        function toggleSidebar() {
            const sidebar = document.getElementById('sidebar');
            const mainContainer = document.querySelector('.main-container');
            const topBar = document.querySelector('.top-bar');
            
            sidebar.classList.toggle('collapsed');
            const isCollapsed = sidebar.classList.contains('collapsed');
            
            if (isCollapsed) {
                mainContainer.classList.add('sidebar-collapsed');
                topBar.classList.add('sidebar-collapsed');
            } else {
                mainContainer.classList.remove('sidebar-collapsed');
                topBar.classList.remove('sidebar-collapsed');
            }
            
            localStorage.setItem('sidebarCollapsed', isCollapsed);
        }
        
        // 全局搜索功能
        function performGlobalSearch() {
            const searchTerm = document.getElementById('globalSearch').value.trim();
            const searchResults = document.getElementById('searchResults');
            
            if (searchTerm.length === 0) {
                searchResults.style.display = 'none';
                return;
            }
            
            if (searchTerm.length < 2) {
                return; // 至少输入2个字符才开始搜索
            }
            
            const results = [];
            const searchTermLower = searchTerm.toLowerCase();
            
            // 搜索用户
            if (allUsers && allUsers.length > 0) {
                allUsers.forEach(user => {
                    if (user.name.toLowerCase().includes(searchTermLower) ||
                        user.email.toLowerCase().includes(searchTermLower) ||
                        user.phone.toLowerCase().includes(searchTermLower)) {
                        results.push({
                            type: 'user',
                            id: user.id,
                            title: user.name,
                            subtitle: user.email,
                            section: 'users'
                        });
                    }
                });
            }
            
            // 搜索图书
            if (allBooks && allBooks.length > 0) {
                allBooks.forEach(book => {
                    if (book.title.toLowerCase().includes(searchTermLower) ||
                        book.author.toLowerCase().includes(searchTermLower) ||
                        (book.category && book.category.toLowerCase().includes(searchTermLower)) ||
                        (book.keywords && book.keywords.toLowerCase().includes(searchTermLower))) {
                        results.push({
                            type: 'book',
                            id: book.id,
                            title: book.title,
                            subtitle: book.author,
                            section: 'books'
                        });
                    }
                });
            }
            
            displaySearchResults(results);
        }
        
        // 显示搜索结果
        function displaySearchResults(results) {
            const searchResults = document.getElementById('searchResults');
            
            if (results.length === 0) {
                searchResults.innerHTML = '<div class="no-results">未找到相关结果</div>';
            } else {
                let html = '';
                results.slice(0, 8).forEach(result => { // 最多显示8个结果
                    const typeText = result.type === 'user' ? '用户' : '图书';
                    const icon = result.type === 'user' ? 'fas fa-user' : 'fas fa-book';
                    
                    html += `
                        <div class="search-result-item" onclick="selectSearchResult('${result.section}', ${result.id})">
                            <div class="search-result-type"><i class="${icon}"></i> ${typeText}</div>
                            <div class="search-result-title">${result.title}</div>
                            <div class="search-result-subtitle">${result.subtitle}</div>
                        </div>
                    `;
                });
                
                if (results.length > 8) {
                    html += `<div class="search-result-item" style="text-align: center; color: var(--secondary-color);">还有 ${results.length - 8} 个结果...</div>`;
                }
                
                searchResults.innerHTML = html;
            }
            
            searchResults.style.display = 'block';
        }
        
        // 选择搜索结果
        function selectSearchResult(section, id) {
            // 切换到对应页面
            showSection(section);
            
            // 高亮对应的行
            setTimeout(() => {
                const table = section === 'users' ? document.getElementById('usersTable') : document.getElementById('booksTable');
                const rows = table.querySelectorAll('tbody tr');
                
                rows.forEach(row => {
                    row.classList.remove('highlight');
                    if (parseInt(row.cells[1].textContent) === id) { // ID现在在第二列
                        row.classList.add('highlight');
                        row.scrollIntoView({ behavior: 'smooth', block: 'center' });
                        
                        // 3秒后移除高亮
                        setTimeout(() => {
                            row.classList.remove('highlight');
                        }, 3000);
                    }
                });
            }, 100);
            
            // 隐藏搜索结果
            document.getElementById('searchResults').style.display = 'none';
            document.getElementById('globalSearch').blur();
        }
        
        // 显示搜索结果
        function showSearchResults() {
            const searchTerm = document.getElementById('globalSearch').value.trim();
            if (searchTerm.length >= 2) {
                performGlobalSearch();
            }
        }
        
        // 隐藏搜索结果（延迟执行以允许点击）
        function hideSearchResults() {
            setTimeout(() => {
                document.getElementById('searchResults').style.display = 'none';
            }, 200);
        }
        
        // 清空搜索框当切换页面时
        function clearGlobalSearch() {
            document.getElementById('globalSearch').value = '';
            document.getElementById('searchResults').style.display = 'none';
        }
        
        // 用户下拉菜单切换
        function toggleUserDropdown() {
            const dropdown = document.getElementById('userDropdown');
            dropdown.style.display = dropdown.style.display === 'block' ? 'none' : 'block';
        }
        
        // 点击其他地方关闭下拉菜单
        document.addEventListener('click', function(event) {
            const dropdown = document.getElementById('userDropdown');
            const userButton = document.querySelector('.user-button');
            
            if (!userButton.contains(event.target) && !dropdown.contains(event.target)) {
                dropdown.style.display = 'none';
            }
        });
        
        // 主题切换功能
        function toggleTheme() {
            const body = document.body;
            const themeIcon = document.getElementById('themeIcon');
            const themeText = document.getElementById('themeText');
            
            if (body.getAttribute('data-theme') === 'dark') {
                body.setAttribute('data-theme', 'light');
                themeIcon.className = 'fas fa-moon';
                themeText.textContent = '夜间模式';
                localStorage.setItem('theme', 'light');
            } else {
                body.setAttribute('data-theme', 'dark');
                themeIcon.className = 'fas fa-sun';
                themeText.textContent = '日间模式';
                localStorage.setItem('theme', 'dark');
            }
        }
        
        // 初始化主题
        function initTheme() {
            const savedTheme = localStorage.getItem('theme');
            const themeIcon = document.getElementById('themeIcon');
            const themeText = document.getElementById('themeText');
            
            if (savedTheme === 'dark') {
                document.body.setAttribute('data-theme', 'dark');
                themeIcon.className = 'fas fa-sun';
                themeText.textContent = '日间模式';
            } else {
                document.body.setAttribute('data-theme', 'light');
                themeIcon.className = 'fas fa-moon';
                themeText.textContent = '夜间模式';
            }
        }
        
        // 初始化侧边栏状态
        function initSidebar() {
            const sidebarCollapsed = localStorage.getItem('sidebarCollapsed') === 'true';
            const sidebar = document.getElementById('sidebar');
            const mainContainer = document.querySelector('.main-container');
            const topBar = document.querySelector('.top-bar');
            
            if (sidebarCollapsed) {
                sidebar.classList.add('collapsed');
                mainContainer.classList.add('sidebar-collapsed');
                topBar.classList.add('sidebar-collapsed');
            }
        }
        
        // 登录功能
        function logout() {
            if (confirm('确定要退出登录吗？')) {
                localStorage.removeItem('userType');
                localStorage.removeItem('username');
                window.location.href = '/login';
            }
        }
        
        // 检查登录状态
        function checkLoginStatus() {
            const userType = localStorage.getItem('userType');
            const username = localStorage.getItem('username');
            const currentUserSpan = document.getElementById('currentUser');
            
            if (userType && username) {
                const userTypeText = userType === 'admin' ? '管理员' : '读者';
                currentUserSpan.textContent = `${userTypeText}: ${username}`;
                
                // 根据用户类型显示不同界面
                if (userType === 'reader') {
                    showUserInterface();
                } else {
                    showAdminInterface();
                }
            } else {
                currentUserSpan.textContent = '游客模式';
            }
        }
        
        // 显示管理员界面
        function showAdminInterface() {
            document.getElementById('admin-interface').style.display = 'block';
            document.getElementById('user-interface').style.display = 'none';
        }
        
        // 显示用户界面
        function showUserInterface() {
            document.getElementById('admin-interface').style.display = 'none';
            document.getElementById('user-interface').style.display = 'block';
            loadUserData();
        }
        
        // 加载用户数据
        async function loadUserData() {
            const userId = localStorage.getItem('userId');
            if (!userId) return;
            
            try {
                // 加载当前借阅
                await loadCurrentBorrowings(parseInt(userId));
                // 加载历史借阅
                await loadBorrowHistory(parseInt(userId));
                // 加载借阅统计
                await loadBorrowStats(parseInt(userId));
                // 加载热力图
                await loadHeatmap();
            } catch (error) {
                console.error('加载用户数据失败:', error);
            }
        }
        
        // 加载当前借阅
        async function loadCurrentBorrowings(userId) {
            const container = document.getElementById('currentBorrowings');
            try {
                // 获取用户信息
                const usersResponse = await fetch('/data/users.json');
                const users = await usersResponse.json();
                const currentUser = users.find(user => user.id === userId);
                
                if (!currentUser) {
                    container.innerHTML = '<div style="text-align: center; color: var(--error-color); padding: 20px;">用户不存在</div>';
                    return;
                }
                
                // 获取借阅记录
                const recordsResponse = await fetch('/data/records.json');
                const records = await recordsResponse.json();
                
                // 获取图书信息
                const booksResponse = await fetch('/data/books.json');
                const books = await booksResponse.json();
                
                // 筛选当前用户的未归还借阅记录
                const currentBorrowings = records.filter(record => 
                    record.userId === currentUser.id && !record.isReturned
                );
                
                if (currentBorrowings.length === 0) {
                    container.innerHTML = '<div style="text-align: center; color: var(--secondary-color); padding: 20px;">暂无借阅记录</div>';
                } else {
                    let html = '';
                    currentBorrowings.forEach(record => {
                        const book = books.find(b => b.id === record.bookId);
                        const borrowDate = new Date(record.borrowTime * 1000);
                        const dueDate = new Date(borrowDate.getTime() + 30 * 24 * 60 * 60 * 1000); // 假设借期30天
                        const daysLeft = Math.ceil((dueDate - new Date()) / (1000 * 60 * 60 * 24));
                        const statusClass = daysLeft < 0 ? 'overdue' : daysLeft <= 3 ? 'due-soon' : 'normal';
                        
                        html += `
                            <div class="borrow-item ${statusClass}">
                                <h4>${book ? book.title : '未知图书'}</h4>
                                <p>借阅日期: ${borrowDate.toLocaleDateString()}</p>
                                <p>应还日期: ${dueDate.toLocaleDateString()}</p>
                                <p class="status">${daysLeft < 0 ? '已逾期' + Math.abs(daysLeft) + '天' : daysLeft <= 3 ? '即将到期' : '还有' + daysLeft + '天'}</p>
                            </div>
                        `;
                    });
                    container.innerHTML = html;
                }
            } catch (error) {
                console.error('加载当前借阅失败:', error);
                container.innerHTML = '<div style="text-align: center; color: var(--error-color);">加载失败</div>';
            }
        }
        
        // 加载历史借阅
        async function loadBorrowHistory(userId) {
            const container = document.getElementById('borrowHistory');
            try {
                // 获取用户信息
                const usersResponse = await fetch('/data/users.json');
                const users = await usersResponse.json();
                const currentUser = users.find(user => user.id === userId);
                
                if (!currentUser) {
                    container.innerHTML = '<div style="text-align: center; color: var(--error-color); padding: 20px;">用户不存在</div>';
                    return;
                }
                
                // 获取借阅记录
                const recordsResponse = await fetch('/data/records.json');
                const records = await recordsResponse.json();
                
                // 获取图书信息
                const booksResponse = await fetch('/data/books.json');
                const books = await booksResponse.json();
                
                // 筛选当前用户的已归还借阅记录
                const historyRecords = records.filter(record => 
                    record.userId === currentUser.id && record.isReturned
                );
                
                if (historyRecords.length === 0) {
                    container.innerHTML = '<div style="text-align: center; color: var(--secondary-color); padding: 20px;">暂无历史记录</div>';
                } else {
                    let html = '';
                    historyRecords.slice(0, 5).forEach(record => {
                        const book = books.find(b => b.id === record.bookId);
                        const borrowDate = new Date(record.borrowTime * 1000);
                        const returnDate = record.returnTime ? new Date(record.returnTime * 1000) : null;
                        
                        html += `
                            <div class="borrow-item">
                                <h4>${book ? book.title : '未知图书'}</h4>
                                <p>借阅日期: ${borrowDate.toLocaleDateString()}</p>
                                <p>归还日期: ${returnDate ? returnDate.toLocaleDateString() : '未归还'}</p>
                            </div>
                        `;
                    });
                    if (historyRecords.length > 5) {
                        html += `<div style="text-align: center; color: var(--secondary-color); padding: 10px;">还有 ${historyRecords.length - 5} 条记录...</div>`;
                    }
                    container.innerHTML = html;
                }
            } catch (error) {
                console.error('加载历史借阅失败:', error);
                container.innerHTML = '<div style="text-align: center; color: var(--error-color);">加载失败</div>';
            }
        }
        
        // 加载借阅统计
        async function loadBorrowStats(userId) {
            const container = document.getElementById('borrowStats');
            try {
                // 获取用户信息
                const usersResponse = await fetch('/data/users.json');
                const users = await usersResponse.json();
                const currentUser = users.find(user => user.id === userId);
                
                if (!currentUser) {
                    container.innerHTML = '<div style="text-align: center; color: var(--error-color); padding: 20px;">用户不存在</div>';
                    return;
                }
                
                // 获取借阅记录
                const recordsResponse = await fetch('/data/records.json');
                const records = await recordsResponse.json();
                
                // 筛选当前用户的借阅记录
                const userRecords = records.filter(record => record.userId === currentUser.id);
                
                // 计算统计数据
                const totalBorrows = userRecords.length;
                const currentBorrows = userRecords.filter(record => !record.isReturned).length;
                
                // 计算平均借阅天数（仅计算已归还的记录）
                const returnedRecords = userRecords.filter(record => record.isReturned && record.returnTime);
                let avgBorrowDays = 0;
                if (returnedRecords.length > 0) {
                    const totalDays = returnedRecords.reduce((sum, record) => {
                        const borrowTime = record.borrowTime * 1000;
                        const returnTime = record.returnTime * 1000;
                        const days = Math.ceil((returnTime - borrowTime) / (1000 * 60 * 60 * 24));
                        return sum + days;
                    }, 0);
                    avgBorrowDays = Math.round(totalDays / returnedRecords.length);
                }
                
                // 计算逾期次数（假设借期30天）
                let overdueCount = 0;
                userRecords.forEach(record => {
                    const borrowTime = record.borrowTime * 1000;
                    const dueTime = borrowTime + 30 * 24 * 60 * 60 * 1000; // 30天后
                    
                    if (record.isReturned && record.returnTime) {
                        // 已归还，检查是否逾期归还
                        if (record.returnTime * 1000 > dueTime) {
                            overdueCount++;
                        }
                    } else {
                        // 未归还，检查是否已逾期
                        if (Date.now() > dueTime) {
                            overdueCount++;
                        }
                    }
                });
                
                let html = `
                    <div class="stats-item">
                        <span class="stats-label">总借阅次数</span>
                        <span class="stats-value">${totalBorrows}</span>
                    </div>
                    <div class="stats-item">
                        <span class="stats-label">当前借阅</span>
                        <span class="stats-value">${currentBorrows}</span>
                    </div>
                    <div class="stats-item">
                        <span class="stats-label">平均借阅天数</span>
                        <span class="stats-value">${avgBorrowDays} 天</span>
                    </div>
                    <div class="stats-item">
                        <span class="stats-label">逾期次数</span>
                        <span class="stats-value">${overdueCount}</span>
                    </div>
                `;
                container.innerHTML = html;
            } catch (error) {
                console.error('加载借阅统计失败:', error);
                container.innerHTML = '<div style="text-align: center; color: var(--error-color);">加载失败</div>';
            }
        }
        
        // 用户图书搜索
        async function searchBooksForUser() {
            const searchTerm = document.getElementById('userBookSearch').value.trim();
            const container = document.getElementById('userBookResults');
            
            if (!searchTerm) {
                container.innerHTML = '';
                return;
            }
            
            try {
                // 获取图书信息
                const booksResponse = await fetch('/data/books.json');
                const books = await booksResponse.json();
                
                // 搜索图书
                const filteredBooks = books.filter(book => 
                    book.title.toLowerCase().includes(searchTerm.toLowerCase()) ||
                    book.author.toLowerCase().includes(searchTerm.toLowerCase()) ||
                    (book.category && book.category.toLowerCase().includes(searchTerm.toLowerCase())) ||
                    (book.keywords && book.keywords.toLowerCase().includes(searchTerm.toLowerCase()))
                );
                
                if (filteredBooks.length === 0) {
                    container.innerHTML = '<div style="text-align: center; color: var(--secondary-color); padding: 20px;">未找到相关图书</div>';
                } else {
                    let html = '';
                    filteredBooks.forEach(book => {
                        const statusClass = book.isAvailable ? 'available' : 'borrowed';
                        const statusText = book.isAvailable ? '可借阅' : '已借出';
                        
                        html += `
                            <div class="book-item ${statusClass}">
                                <h4>${book.title}</h4>
                                <p>作者: ${book.author}</p>
                                <p>分类: ${book.category || '未分类'}</p>
                                <p>描述: ${book.description || '暂无描述'}</p>
                                <p class="status">状态: ${statusText}</p>
                            </div>
                        `;
                    });
                    container.innerHTML = html;
                }
            } catch (error) {
                console.error('搜索图书失败:', error);
                container.innerHTML = '<div style="text-align: center; color: var(--error-color);">搜索失败</div>';
            }
        }
        
        // 加载热力图
        async function loadHeatmap() {
            const container = document.getElementById('heatmap');
            try {
                const response = await fetch('/data/records.json');
                if (response.ok) {
                    const records = await response.json();
                    
                    // 处理借阅记录数据，生成热力图数据
                    const heatmapData = processRecordsForHeatmap(records);
                    
                    // 生成热力图
                    generateHeatmap(container, heatmapData);
                } else {
                    // 如果没有数据文件，生成示例热力图
                    const sampleData = generateSampleHeatmapData();
                    generateHeatmap(container, sampleData);
                }
            } catch (error) {
                // 生成示例热力图
                const sampleData = generateSampleHeatmapData();
                generateHeatmap(container, sampleData);
            }
        }
        
        // 生成示例热力图数据
        function generateSampleHeatmapData() {
            const data = {};
            const currentDate = new Date();
            const startDate = new Date(currentDate.getFullYear(), 0, 1);
            
            // 随机生成一些借阅数据
            const books = ['JavaScript高级程序设计', '算法导论', '深入理解计算机系统', '设计模式', '数据结构与算法'];
            
            for (let i = 0; i < 50; i++) {
                const randomDate = new Date(startDate.getTime() + Math.random() * (currentDate.getTime() - startDate.getTime()));
                const dateStr = randomDate.toISOString().split('T')[0];
                
                if (!data[dateStr]) {
                    data[dateStr] = [];
                }
                
                const randomBook = books[Math.floor(Math.random() * books.length)];
                data[dateStr].push(randomBook);
            }
            
            return data;
        }
        
        // 处理借阅记录为热力图数据
        function processRecordsForHeatmap(records) {
            const heatmapData = {};
            const currentDate = new Date();
            
            records.forEach(record => {
                if (record.borrowDate && !record.returnDate) {
                    // 只处理当前正在借阅的记录
                    const borrowDate = new Date(record.borrowDate);
                    const endDate = record.dueDate ? new Date(record.dueDate) : currentDate;
                    
                    // 为借阅期间的每一天添加图书记录
                    for (let date = new Date(borrowDate); date <= endDate && date <= currentDate; date.setDate(date.getDate() + 1)) {
                        const dateStr = date.toISOString().split('T')[0];
                        if (!heatmapData[dateStr]) {
                            heatmapData[dateStr] = [];
                        }
                        heatmapData[dateStr].push(record.bookTitle || '未知图书');
                    }
                }
            });
            
            return heatmapData;
        }
        
        // 生成热力图
        function generateHeatmap(container, data) {
            const currentYear = new Date().getFullYear();
            
            let html = `
                <div class="heatmap-header" style="margin-bottom: 20px; text-align: center;">
                    <h4 style="margin: 0 0 10px 0; color: var(--text-color);">图书借阅热力图 - ${currentYear}</h4>
                    <div class="heatmap-legend" style="display: flex; align-items: center; justify-content: center; gap: 10px; font-size: 12px; color: var(--secondary-color);">
                        <span>少</span>
                        <div class="legend-colors" style="display: flex; gap: 2px;">
                            <div class="legend-item level-0" style="width: 10px; height: 10px; background-color: #ebedf0; border-radius: 2px;" title="无借阅"></div>
                            <div class="legend-item level-1" style="width: 10px; height: 10px; background-color: #9be9a8; border-radius: 2px;" title="1本书"></div>
                            <div class="legend-item level-2" style="width: 10px; height: 10px; background-color: #40c463; border-radius: 2px;" title="2本书"></div>
                            <div class="legend-item level-3" style="width: 10px; height: 10px; background-color: #30a14e; border-radius: 2px;" title="3本书"></div>
                            <div class="legend-item level-4" style="width: 10px; height: 10px; background-color: #216e39; border-radius: 2px;" title="4+本书"></div>
                        </div>
                        <span>多</span>
                    </div>
                </div>
                <div class="heatmap-grid" style="display: flex; gap: 2px; overflow-x: auto; padding: 10px;">
            `;
            
            // 生成一年的日历格子，按周排列
            const startDate = new Date(currentYear, 0, 1);
            const endDate = new Date(currentYear, 11, 31);
            
            // 计算第一周的开始日期（周日开始）
            const firstWeekStart = new Date(startDate);
            firstWeekStart.setDate(startDate.getDate() - startDate.getDay());
            
            // 计算最后一周的结束日期
            const lastWeekEnd = new Date(endDate);
            lastWeekEnd.setDate(endDate.getDate() + (6 - endDate.getDay()));
            
            // 按周生成日历
            for (let weekStart = new Date(firstWeekStart); weekStart <= lastWeekEnd; weekStart.setDate(weekStart.getDate() + 7)) {
                html += '<div class="week-column" style="display: flex; flex-direction: column; gap: 2px;">';
                
                // 为每周的7天创建单元格
                for (let dayOffset = 0; dayOffset < 7; dayOffset++) {
                    const currentDate = new Date(weekStart);
                    currentDate.setDate(weekStart.getDate() + dayOffset);
                    
                    const dateStr = currentDate.toISOString().split('T')[0];
                    const isCurrentYear = currentDate.getFullYear() === currentYear;
                    
                    const books = data[dateStr] || [];
                    const level = Math.min(books.length, 4);
                    
                    let tooltip = `${dateStr}\n`;
                    if (books.length === 0) {
                        tooltip += '无借阅记录';
                    } else {
                        tooltip += `${books.length} 本书:\n${books.join('\n')}`;
                    }
                    
                    const levelColors = {
                        0: '#ebedf0',
                        1: '#9be9a8',
                        2: '#40c463',
                        3: '#30a14e',
                        4: '#216e39'
                    };
                    
                    const opacity = isCurrentYear ? '1' : '0.3';
                    
                    html += `
                        <div class="heatmap-day" 
                             style="width: 12px; height: 12px; background-color: ${levelColors[level]}; border-radius: 2px; cursor: pointer; opacity: ${opacity};" 
                             title="${tooltip}">
                        </div>
                    `;
                }
                
                html += '</div>';
            }
            
            html += '</div>';
            container.innerHTML = html;
        }
        
        // 页面加载完成后初始化
        // 设置搜索下拉框功能
        
        // 为用户界面的搜索框添加事件监听器
        const userBookSearchInput = document.getElementById('userBookSearch');
        if (userBookSearchInput) {
            userBookSearchInput.addEventListener('input', searchBooksForUser);
            userBookSearchInput.addEventListener('keypress', function(e) {
                if (e.key === 'Enter') {
                    searchBooksForUser();
                }
            });
        }
        function setupSearchDropdown(inputId, dropdownId, hiddenInputId, type) {
            const input = document.getElementById(inputId);
            const dropdown = document.getElementById(dropdownId);
            const hiddenInput = document.getElementById(hiddenInputId);
            
            let searchTimeout;
            
            input.addEventListener('input', function() {
                const searchTerm = this.value.trim();
                
                clearTimeout(searchTimeout);
                
                if (searchTerm.length === 0) {
                    dropdown.style.display = 'none';
                    hiddenInput.value = '';
                    return;
                }
                
                if (searchTerm.length < 2) {
                    return;
                }
                
                searchTimeout = setTimeout(() => {
                    performDropdownSearch(searchTerm, dropdown, hiddenInput, type, input);
                }, 300);
            });
            
            input.addEventListener('blur', function() {
                // 延迟隐藏下拉框，以便点击事件能够触发
                setTimeout(() => {
                    dropdown.style.display = 'none';
                }, 200);
            });
            
            input.addEventListener('focus', function() {
                if (this.value.trim().length >= 2) {
                    performDropdownSearch(this.value.trim(), dropdown, hiddenInput, type, input);
                }
            });
        }
        
        // 执行下拉框搜索
        function performDropdownSearch(searchTerm, dropdown, hiddenInput, type, input) {
            const searchTermLower = searchTerm.toLowerCase();
            let results = [];
            
            if (type === 'user' && allUsers) {
                results = allUsers.filter(user => 
                    user.id.toString().includes(searchTerm) ||
                    user.name.toLowerCase().includes(searchTermLower) ||
                    user.email.toLowerCase().includes(searchTermLower)
                ).slice(0, 10);
            } else if (type === 'book' && allBooks) {
                results = allBooks.filter(book => 
                    book.id.toString().includes(searchTerm) ||
                    book.title.toLowerCase().includes(searchTermLower) ||
                    book.author.toLowerCase().includes(searchTermLower)
                ).slice(0, 10);
            }
            
            displayDropdownResults(results, dropdown, hiddenInput, type, input);
        }
        
        // 显示下拉框搜索结果
        function displayDropdownResults(results, dropdown, hiddenInput, type, input) {
            dropdown.innerHTML = '';
            
            if (results.length === 0) {
                dropdown.innerHTML = '<div class="search-dropdown-item">未找到匹配结果</div>';
                dropdown.style.display = 'block';
                return;
            }
            
            results.forEach(item => {
                const div = document.createElement('div');
                div.className = 'search-dropdown-item';
                
                if (type === 'user') {
                    div.innerHTML = `
                        <div class="item-title">${item.name}</div>
                        <div class="item-subtitle">ID: ${item.id} | ${item.email}</div>
                    `;
                } else if (type === 'book') {
                    div.innerHTML = `
                        <div class="item-title">${item.title}</div>
                        <div class="item-subtitle">ID: ${item.id} | 作者: ${item.author}</div>
                    `;
                }
                
                div.addEventListener('click', function() {
                    if (type === 'user') {
                        input.value = `${item.name} (ID: ${item.id})`;
                        hiddenInput.value = item.id;
                    } else if (type === 'book') {
                        input.value = `${item.title} (ID: ${item.id})`;
                        hiddenInput.value = item.id;
                    }
                    dropdown.style.display = 'none';
                });
                
                dropdown.appendChild(div);
            });
            
            dropdown.style.display = 'block';
        }
        
        document.addEventListener('DOMContentLoaded', function() {
            initTheme();
            initSidebar();
            checkLoginStatus();
            
            // 检查是否已登录，如果未登录则重定向到登录页面
            const userType = localStorage.getItem('userType');
            const username = localStorage.getItem('username');
            
            if (!userType || !username) {
                window.location.href = '/login';
                return;
            }
            
            // 如果已登录，则加载数据
            loadUsers();
            loadBooks();
            loadStatistics();
            
            // 绑定表单提交事件
            document.getElementById('borrowForm').addEventListener('submit', handleBorrowSubmit);
            document.getElementById('returnForm').addEventListener('submit', handleReturnSubmit);
            document.getElementById('modalForm').addEventListener('submit', handleModalSubmit);
            
            // 添加搜索下拉框事件监听器
            setupSearchDropdown('borrowUserInput', 'borrowUserDropdown', 'borrowUserId', 'user');
            setupSearchDropdown('borrowBookInput', 'borrowBookDropdown', 'borrowBookId', 'book');
            setupSearchDropdown('returnUserInput', 'returnUserDropdown', 'returnUserId', 'user');
            setupSearchDropdown('returnBookInput', 'returnBookDropdown', 'returnBookId', 'book');
        });
        
        // 弹出卡片相关函数
        let currentModalType = '';
        let currentEditId = null;
        
        // 打开用户模态框
        function openUserModal(user = null) {
            currentModalType = 'user';
            currentEditId = user ? user.id : null;
            
            document.getElementById('modalTitle').textContent = user ? '编辑用户' : '添加用户';
            document.getElementById('modalSubmitBtn').textContent = user ? '保存' : '添加';
            
            const formContent = `
                <div class="form-group">
                    <label for="modalUserName">姓名</label>
                    <input type="text" id="modalUserName" name="name" value="${user ? user.name : ''}" required>
                </div>
                <div class="form-group">
                    <label for="modalUserEmail">邮箱</label>
                    <input type="email" id="modalUserEmail" name="email" value="${user ? user.email : ''}" required>
                </div>
                <div class="form-group">
                    <label for="modalUserPhone">电话</label>
                    <input type="tel" id="modalUserPhone" name="phone" value="${user ? user.phone : ''}" required>
                </div>
            `;
            
            document.getElementById('modalFormContent').innerHTML = formContent;
            document.getElementById('modalOverlay').style.display = 'flex';
        }
        
        // 打开图书模态框
        function openBookModal(book = null) {
            currentModalType = 'book';
            currentEditId = book ? book.id : null;
            
            document.getElementById('modalTitle').textContent = book ? '编辑图书' : '添加图书';
            document.getElementById('modalSubmitBtn').textContent = book ? '保存' : '添加';
            
            const formContent = `
                <div class="form-group">
                    <label for="modalBookTitle">书名</label>
                    <input type="text" id="modalBookTitle" name="title" value="${book ? book.title : ''}" required>
                </div>
                <div class="form-group">
                    <label for="modalBookAuthor">作者</label>
                    <input type="text" id="modalBookAuthor" name="author" value="${book ? book.author : ''}" required>
                </div>
                <div class="form-group">
                    <label for="modalBookCategory">类别</label>
                    <input type="text" id="modalBookCategory" name="category" value="${book ? book.category || '' : ''}">
                </div>
                <div class="form-group">
                    <label for="modalBookKeywords">关键字</label>
                    <input type="text" id="modalBookKeywords" name="keywords" value="${book ? book.keywords || '' : ''}">
                </div>
                <div class="form-group">
                    <label for="modalBookDescription">简介</label>
                    <textarea id="modalBookDescription" name="description" rows="3">${book ? book.description || '' : ''}</textarea>
                </div>
            `;
            
            document.getElementById('modalFormContent').innerHTML = formContent;
            document.getElementById('modalOverlay').style.display = 'flex';
        }
        
        // 关闭模态框
        function closeModal() {
            document.getElementById('modalOverlay').style.display = 'none';
            document.getElementById('modalForm').reset();
            currentModalType = '';
            currentEditId = null;
        }
        
        // 处理模态框表单提交
        async function handleModalSubmit(e) {
            e.preventDefault();
            const formData = new FormData(e.target);
            
            try {
                let url, method, messageElementId;
                
                if (currentModalType === 'user') {
                    url = currentEditId ? `/api/users/${currentEditId}` : '/api/users';
                    method = currentEditId ? 'PUT' : 'POST';
                    messageElementId = 'userMessage';
                } else if (currentModalType === 'book') {
                    url = currentEditId ? `/api/books/${currentEditId}` : '/api/books';
                    method = currentEditId ? 'PUT' : 'POST';
                    messageElementId = 'bookMessage';
                }
                
                const response = await fetch(url, {
                    method: method,
                    body: formData
                });
                
                const result = await response.json();
                
                if (result.success) {
                    const action = currentEditId ? '更新' : '添加';
                    const type = currentModalType === 'user' ? '用户' : '图书';
                    showMessage(messageElementId, `${action}${type}成功`, 'success');
                    closeModal();
                    
                    if (currentModalType === 'user') {
                        loadUsers();
                    } else {
                        loadBooks();
                    }
                } else {
                    const action = currentEditId ? '更新' : '添加';
                    const type = currentModalType === 'user' ? '用户' : '图书';
                    showMessage(messageElementId, result.message || `${action}${type}失败`, 'error');
                }
            } catch (error) {
                const messageElementId = currentModalType === 'user' ? 'userMessage' : 'bookMessage';
                showMessage(messageElementId, '网络错误', 'error');
            }
        }
        
        // 编辑用户
        function editUser(userId) {
            const user = allUsers.find(u => u.id === userId);
            if (user) {
                openUserModal(user);
            }
        }
        
        // 编辑图书
        function editBook(bookId) {
            const book = allBooks.find(b => b.id === bookId);
            if (book) {
                openBookModal(book);
            }
        }
        
        // 删除用户
        async function deleteUser(userId) {
            if (!confirm('确定要删除这个用户吗？')) {
                return;
            }
            
            try {
                const response = await fetch(`/api/users/${userId}`, {
                    method: 'DELETE'
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('userMessage', '删除用户成功', 'success');
                    loadUsers();
                } else {
                    showMessage('userMessage', result.message || '删除用户失败', 'error');
                }
            } catch (error) {
                showMessage('userMessage', '网络错误', 'error');
            }
        }
        
        // 删除图书
        async function deleteBook(bookId) {
            if (!confirm('确定要删除这本图书吗？')) {
                return;
            }
            
            try {
                const response = await fetch(`/api/books/${bookId}`, {
                    method: 'DELETE'
                });
                
                const result = await response.json();
                
                if (result.success) {
                    showMessage('bookMessage', '删除图书成功', 'success');
                    loadBooks();
                } else {
                    showMessage('bookMessage', result.message || '删除图书失败', 'error');
                }
            } catch (error) {
                showMessage('bookMessage', '网络错误', 'error');
            }
        }
        
        // 点击模态框外部关闭
        document.getElementById('modalOverlay').addEventListener('click', function(e) {
            if (e.target === this) {
                closeModal();
            }
        });
    </script>
</body>
</html>
)HTML";
}