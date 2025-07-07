#include "library_system.h"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

// User类实现
std::string User::toString() const {
    std::ostringstream oss;
    oss << "User[ID:" << id << ", Name:" << name << ", Email:" << email 
        << ", Phone:" << phone << ", Borrowed:" << borrowHistory.size() << "]";
    return oss.str();
}

Json::Value User::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["name"] = name;
    json["email"] = email;
    json["phone"] = phone;
    json["maxBorrowCount"] = maxBorrowCount;
    json["createTime"] = static_cast<int64_t>(createTime);
    
    Json::Value historyArray(Json::arrayValue);
    for (int bookId : borrowHistory) {
        historyArray.append(bookId);
    }
    json["borrowHistory"] = historyArray;
    
    return json;
}

void User::fromJson(const Json::Value& json) {
    id = json["id"].asInt();
    name = json["name"].asString();
    email = json["email"].asString();
    phone = json["phone"].asString();
    maxBorrowCount = json["maxBorrowCount"].asInt();
    createTime = static_cast<std::time_t>(json["createTime"].asInt64());
    
    borrowHistory.clear();
    const Json::Value& historyArray = json["borrowHistory"];
    for (const auto& item : historyArray) {
        borrowHistory.push_back(item.asInt());
    }
}

void User::display() const {
    std::cout << "用户信息:" << std::endl;
    std::cout << "  ID: " << id << std::endl;
    std::cout << "  姓名: " << name << std::endl;
    std::cout << "  邮箱: " << email << std::endl;
    std::cout << "  电话: " << phone << std::endl;
    std::cout << "  当前借阅: " << borrowHistory.size() << "/" << maxBorrowCount << std::endl;
}

void User::addBorrowRecord(int bookId) {
    auto it = std::find(borrowHistory.begin(), borrowHistory.end(), bookId);
    if (it == borrowHistory.end()) {
        borrowHistory.push_back(bookId);
    }
}

void User::removeBorrowRecord(int bookId) {
    auto it = std::find(borrowHistory.begin(), borrowHistory.end(), bookId);
    if (it != borrowHistory.end()) {
        borrowHistory.erase(it);
    }
}

bool User::canBorrow() const {
    return borrowHistory.size() < static_cast<size_t>(maxBorrowCount);
}

int User::getCurrentBorrowCount() const {
    return static_cast<int>(borrowHistory.size());
}

// Book类实现
std::string Book::toString() const {
    std::ostringstream oss;
    oss << "Book[ID:" << id << ", Title:" << name << ", Author:" << author 
        << ", Category:" << category << ", Available:" << (isAvailable ? "Yes" : "No") << "]";
    return oss.str();
}

Json::Value Book::toJson() const {
    Json::Value json;
    json["id"] = id;
    json["title"] = name;
    json["author"] = author;
    json["category"] = category;
    json["keywords"] = keywords;
    json["description"] = description;
    json["isAvailable"] = isAvailable;
    json["borrowerId"] = borrowerId;
    json["createTime"] = static_cast<int64_t>(createTime);
    
    Json::Value historyArray(Json::arrayValue);
    for (int userId : borrowHistory) {
        historyArray.append(userId);
    }
    json["borrowHistory"] = historyArray;
    
    return json;
}

void Book::fromJson(const Json::Value& json) {
    id = json["id"].asInt();
    name = json["title"].asString();
    author = json["author"].asString();
    category = json["category"].asString();
    keywords = json["keywords"].asString();
    description = json["description"].asString();
    isAvailable = json["isAvailable"].asBool();
    borrowerId = json["borrowerId"].asInt();
    createTime = static_cast<std::time_t>(json["createTime"].asInt64());
    
    borrowHistory.clear();
    const Json::Value& historyArray = json["borrowHistory"];
    for (const auto& item : historyArray) {
        borrowHistory.push_back(item.asInt());
    }
}

void Book::display() const {
    std::cout << "图书信息:" << std::endl;
    std::cout << "  ID: " << id << std::endl;
    std::cout << "  书名: " << name << std::endl;
    std::cout << "  作者: " << author << std::endl;
    std::cout << "  类别: " << category << std::endl;
    std::cout << "  关键字: " << keywords << std::endl;
    std::cout << "  简介: " << description << std::endl;
    std::cout << "  状态: " << (isAvailable ? "可借阅" : "已借出") << std::endl;
}

void Book::borrowBook(int userId) {
    if (isAvailable) {
        isAvailable = false;
        borrowerId = userId;
        addBorrowHistory(userId);
    }
}

void Book::returnBook() {
    isAvailable = true;
    borrowerId = 0;
}

bool Book::matchesKeyword(const std::string& keyword) const {
    std::string lowerKeyword = keyword;
    std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
    
    std::string lowerTitle = name;
    std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(), ::tolower);
    
    std::string lowerAuthor = author;
    std::transform(lowerAuthor.begin(), lowerAuthor.end(), lowerAuthor.begin(), ::tolower);
    
    std::string lowerCategory = category;
    std::transform(lowerCategory.begin(), lowerCategory.end(), lowerCategory.begin(), ::tolower);
    
    std::string lowerKeywords = keywords;
    std::transform(lowerKeywords.begin(), lowerKeywords.end(), lowerKeywords.begin(), ::tolower);
    
    return lowerTitle.find(lowerKeyword) != std::string::npos ||
           lowerAuthor.find(lowerKeyword) != std::string::npos ||
           lowerCategory.find(lowerKeyword) != std::string::npos ||
           lowerKeywords.find(lowerKeyword) != std::string::npos;
}

void Book::addBorrowHistory(int userId) {
    borrowHistory.push_back(userId);
}

// BorrowRecord类实现
void BorrowRecord::returnBook() {
    if (!isReturned) {
        returnTime = std::time(nullptr);
        isReturned = true;
    }
}

Json::Value BorrowRecord::toJson() const {
    Json::Value json;
    json["recordId"] = recordId;
    json["userId"] = userId;
    json["bookId"] = bookId;
    json["borrowTime"] = static_cast<int64_t>(borrowTime);
    json["returnTime"] = static_cast<int64_t>(returnTime);
    json["isReturned"] = isReturned;
    return json;
}

void BorrowRecord::fromJson(const Json::Value& json) {
    recordId = json["recordId"].asInt();
    userId = json["userId"].asInt();
    bookId = json["bookId"].asInt();
    borrowTime = static_cast<std::time_t>(json["borrowTime"].asInt64());
    returnTime = static_cast<std::time_t>(json["returnTime"].asInt64());
    isReturned = json["isReturned"].asBool();
}

std::string BorrowRecord::toString() const {
    std::ostringstream oss;
    oss << "BorrowRecord[ID:" << recordId << ", User:" << userId 
        << ", Book:" << bookId << ", Status:" << (isReturned ? "Returned" : "Borrowed") << "]";
    return oss.str();
}

// Statistics类实现
void Statistics::updateBookPopularity(int bookId) {
    bookPopularity[bookId]++;
}

void Statistics::updateUserActivity(int userId) {
    userActivity[userId]++;
}

void Statistics::updateMonthlyStats(std::time_t borrowTime) {
    std::tm* timeinfo = std::localtime(&borrowTime);
    std::ostringstream oss;
    oss << std::put_time(timeinfo, "%Y-%m");
    monthlyStats[oss.str()]++;
}

std::vector<std::pair<int, int>> Statistics::getMostPopularBooks(int count) const {
    std::vector<std::pair<int, int>> result(bookPopularity.begin(), bookPopularity.end());
    std::sort(result.begin(), result.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    if (result.size() > static_cast<size_t>(count)) {
        result.resize(count);
    }
    return result;
}

std::vector<std::pair<int, int>> Statistics::getMostActiveUsers(int count) const {
    std::vector<std::pair<int, int>> result(userActivity.begin(), userActivity.end());
    std::sort(result.begin(), result.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    if (result.size() > static_cast<size_t>(count)) {
        result.resize(count);
    }
    return result;
}

std::map<std::string, int> Statistics::getMonthlyTrends() const {
    return monthlyStats;
}

void Statistics::showStatistics() const {
    std::cout << "=== 图书馆统计信息 ===" << std::endl;
    
    auto popularBooks = getMostPopularBooks(5);
    std::cout << "最受欢迎的图书:" << std::endl;
    for (const auto& book : popularBooks) {
        std::cout << "  图书ID " << book.first << ": " << book.second << " 次借阅" << std::endl;
    }
    
    auto activeUsers = getMostActiveUsers(5);
    std::cout << "最活跃的用户:" << std::endl;
    for (const auto& user : activeUsers) {
        std::cout << "  用户ID " << user.first << ": " << user.second << " 次借阅" << std::endl;
    }
    
    std::cout << "月度借阅趋势:" << std::endl;
    for (const auto& month : monthlyStats) {
        std::cout << "  " << month.first << ": " << month.second << " 次借阅" << std::endl;
    }
}

Json::Value Statistics::serialize() const {
    Json::Value json;
    
    Json::Value bookPop(Json::objectValue);
    for (const auto& book : bookPopularity) {
        bookPop[std::to_string(book.first)] = book.second;
    }
    json["bookPopularity"] = bookPop;
    
    Json::Value userAct(Json::objectValue);
    for (const auto& user : userActivity) {
        userAct[std::to_string(user.first)] = user.second;
    }
    json["userActivity"] = userAct;
    
    Json::Value monthly(Json::objectValue);
    for (const auto& month : monthlyStats) {
        monthly[month.first] = month.second;
    }
    json["monthlyStats"] = monthly;
    
    return json;
}

void Statistics::clear() {
    bookPopularity.clear();
    userActivity.clear();
    monthlyStats.clear();
}

// LibrarySystem类实现
LibrarySystem::LibrarySystem() : nextUserId(1), nextBookId(1), nextRecordId(1) {
    createDataDirectory();
    loadData();
}

LibrarySystem::~LibrarySystem() {
    saveData();
}

void LibrarySystem::createDataDirectory() {
    try {
        std::filesystem::create_directories("data");
    } catch (const std::exception& e) {
        std::cerr << "创建数据目录失败: " << e.what() << std::endl;
    }
}

int LibrarySystem::addUser(const std::string& name, const std::string& email, const std::string& phone) {
    if (!validateInput(name) || !validateInput(email)) {
        return -1;
    }
    
    auto user = std::make_unique<User>(nextUserId++, name, email, phone);
    int userId = user->getId();
    users.push_back(std::move(user));
    saveData();
    return userId;
}

bool LibrarySystem::deleteUser(int userId) {
    auto it = std::find_if(users.begin(), users.end(),
                          [userId](const auto& user) { return user->getId() == userId; });
    
    if (it != users.end()) {
        // 检查用户是否有未归还的图书
        User* user = it->get();
        if (user->getCurrentBorrowCount() > 0) {
            return false; // 不能删除有借阅记录的用户
        }
        
        users.erase(it);
        saveData();
        return true;
    }
    return false;
}

bool LibrarySystem::updateUser(int userId, const std::string& name, 
                              const std::string& email, const std::string& phone) {
    User* user = findUser(userId);
    if (user && validateInput(name) && validateInput(email)) {
        user->setName(name);
        user->setEmail(email);
        user->setPhone(phone);
        saveData();
        return true;
    }
    return false;
}

User* LibrarySystem::findUser(int userId) {
    auto it = std::find_if(users.begin(), users.end(),
                          [userId](const auto& user) { return user->getId() == userId; });
    return (it != users.end()) ? it->get() : nullptr;
}

std::vector<User*> LibrarySystem::searchUsers(const std::string& keyword) {
    std::vector<User*> result;
    std::string lowerKeyword = keyword;
    std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(), ::tolower);
    
    for (const auto& user : users) {
        std::string lowerName = user->getName();
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (lowerName.find(lowerKeyword) != std::string::npos ||
            user->getEmail().find(keyword) != std::string::npos) {
            result.push_back(user.get());
        }
    }
    return result;
}

std::vector<User*> LibrarySystem::getAllUsers() {
    std::vector<User*> result;
    for (const auto& user : users) {
        result.push_back(user.get());
    }
    return result;
}

int LibrarySystem::addBook(const std::string& title, const std::string& author,
                          const std::string& category, const std::string& keywords,
                          const std::string& description) {
    if (!validateInput(title) || !validateInput(author)) {
        return -1;
    }
    
    auto book = std::make_unique<Book>(nextBookId++, title, author, category, keywords, description);
    int bookId = book->getId();
    books.push_back(std::move(book));
    saveData();
    return bookId;
}

bool LibrarySystem::deleteBook(int bookId) {
    auto it = std::find_if(books.begin(), books.end(),
                          [bookId](const auto& book) { return book->getId() == bookId; });
    
    if (it != books.end()) {
        // 检查图书是否已被借出
        if (!(*it)->getIsAvailable()) {
            return false; // 不能删除已借出的图书
        }
        
        books.erase(it);
        saveData();
        return true;
    }
    return false;
}

bool LibrarySystem::updateBook(int bookId, const std::string& title, const std::string& author,
                              const std::string& category, const std::string& keywords,
                              const std::string& description) {
    Book* book = findBook(bookId);
    if (book && validateInput(title) && validateInput(author)) {
        book->setName(title);
        book->setAuthor(author);
        book->setCategory(category);
        book->setKeywords(keywords);
        book->setDescription(description);
        saveData();
        return true;
    }
    return false;
}

Book* LibrarySystem::findBook(int bookId) {
    auto it = std::find_if(books.begin(), books.end(),
                          [bookId](const auto& book) { return book->getId() == bookId; });
    return (it != books.end()) ? it->get() : nullptr;
}

std::vector<Book*> LibrarySystem::searchBooks(const std::string& keyword) {
    std::vector<Book*> result;
    for (const auto& book : books) {
        if (book->matchesKeyword(keyword)) {
            result.push_back(book.get());
        }
    }
    return result;
}

std::vector<Book*> LibrarySystem::getAllBooks() {
    std::vector<Book*> result;
    for (const auto& book : books) {
        result.push_back(book.get());
    }
    return result;
}

bool LibrarySystem::borrowBook(int userId, int bookId) {
    User* user = findUser(userId);
    Book* book = findBook(bookId);
    
    if (!user || !book) {
        return false;
    }
    
    if (!user->canBorrow() || !book->getIsAvailable()) {
        return false;
    }
    
    // 执行借阅操作
    book->borrowBook(userId);
    user->addBorrowRecord(bookId);
    
    // 创建借阅记录
    auto record = std::make_unique<BorrowRecord>(nextRecordId++, userId, bookId);
    borrowRecords.push_back(std::move(record));
    
    // 更新统计信息
    statistics.updateBookPopularity(bookId);
    statistics.updateUserActivity(userId);
    statistics.updateMonthlyStats(std::time(nullptr));
    
    saveData();
    return true;
}

bool LibrarySystem::returnBook(int userId, int bookId) {
    User* user = findUser(userId);
    Book* book = findBook(bookId);
    
    if (!user || !book) {
        return false;
    }
    
    if (book->getIsAvailable() || book->getBorrowerId() != userId) {
        return false;
    }
    
    // 执行归还操作
    book->returnBook();
    user->removeBorrowRecord(bookId);
    
    // 更新借阅记录
    for (const auto& record : borrowRecords) {
        if (record->getUserId() == userId && record->getBookId() == bookId && !record->getIsReturned()) {
            record->returnBook();
            break;
        }
    }
    
    saveData();
    return true;
}

std::vector<BorrowRecord*> LibrarySystem::getUserBorrowHistory(int userId) {
    std::vector<BorrowRecord*> result;
    for (const auto& record : borrowRecords) {
        if (record->getUserId() == userId) {
            result.push_back(record.get());
        }
    }
    return result;
}

std::vector<BorrowRecord*> LibrarySystem::getBookBorrowHistory(int bookId) {
    std::vector<BorrowRecord*> result;
    for (const auto& record : borrowRecords) {
        if (record->getBookId() == bookId) {
            result.push_back(record.get());
        }
    }
    return result;
}

std::vector<BorrowRecord*> LibrarySystem::getAllBorrowRecords() {
    std::vector<BorrowRecord*> result;
    for (const auto& record : borrowRecords) {
        result.push_back(record.get());
    }
    return result;
}

Json::Value LibrarySystem::getStatisticsJson() {
    return statistics.serialize();
}

void LibrarySystem::saveData() {
    try {
        // 保存用户数据
        Json::Value usersJson(Json::arrayValue);
        for (const auto& user : users) {
            usersJson.append(user->toJson());
        }
        
        std::ofstream usersFile(USERS_FILE);
        usersFile << usersJson;
        usersFile.close();
        
        // 保存图书数据
        Json::Value booksJson(Json::arrayValue);
        for (const auto& book : books) {
            booksJson.append(book->toJson());
        }
        
        std::ofstream booksFile(BOOKS_FILE);
        booksFile << booksJson;
        booksFile.close();
        
        // 保存借阅记录
        Json::Value recordsJson(Json::arrayValue);
        for (const auto& record : borrowRecords) {
            recordsJson.append(record->toJson());
        }
        
        std::ofstream recordsFile(RECORDS_FILE);
        recordsFile << recordsJson;
        recordsFile.close();
        
    } catch (const std::exception& e) {
        std::cerr << "保存数据失败: " << e.what() << std::endl;
    }
}

void LibrarySystem::loadData() {
    try {
        // 加载用户数据
        std::ifstream usersFile(USERS_FILE);
        if (usersFile.is_open()) {
            Json::Value usersJson;
            usersFile >> usersJson;
            
            for (const auto& userJson : usersJson) {
                auto user = std::make_unique<User>();
                user->fromJson(userJson);
                if (user->getId() >= nextUserId) {
                    nextUserId = user->getId() + 1;
                }
                users.push_back(std::move(user));
            }
            usersFile.close();
        }
        
        // 加载图书数据
        std::ifstream booksFile(BOOKS_FILE);
        if (booksFile.is_open()) {
            Json::Value booksJson;
            booksFile >> booksJson;
            
            for (const auto& bookJson : booksJson) {
                auto book = std::make_unique<Book>();
                book->fromJson(bookJson);
                if (book->getId() >= nextBookId) {
                    nextBookId = book->getId() + 1;
                }
                books.push_back(std::move(book));
            }
            booksFile.close();
        }
        
        // 加载借阅记录
        std::ifstream recordsFile(RECORDS_FILE);
        if (recordsFile.is_open()) {
            Json::Value recordsJson;
            recordsFile >> recordsJson;
            
            for (const auto& recordJson : recordsJson) {
                auto record = std::make_unique<BorrowRecord>(0, 0, 0);
                record->fromJson(recordJson);
                if (record->getRecordId() >= nextRecordId) {
                    nextRecordId = record->getRecordId() + 1;
                }
                borrowRecords.push_back(std::move(record));
            }
            recordsFile.close();
        }
        
        updateStatistics();
        
    } catch (const std::exception& e) {
        std::cerr << "加载数据失败: " << e.what() << std::endl;
    }
}

void LibrarySystem::loadTestData() {
    // 如果没有数据，加载测试数据
    if (users.empty() && books.empty()) {
        // 添加测试用户
        addUser("张三", "zhangsan@example.com", "13800138001");
        addUser("李四", "lisi@example.com", "13800138002");
        addUser("王五", "wangwu@example.com", "13800138003");
        
        // 添加测试图书
        addBook("C++程序设计", "谭浩强", "计算机", "编程,C++,程序设计", "经典的C++编程教材");
        addBook("数据结构与算法", "严蔚敏", "计算机", "数据结构,算法", "数据结构与算法分析");
        addBook("操作系统概念", "Abraham Silberschatz", "计算机", "操作系统,系统编程", "操作系统原理与实现");
        addBook("计算机网络", "谢希仁", "计算机", "网络,通信", "计算机网络基础教程");
        addBook("软件工程", "Ian Sommerville", "计算机", "软件工程,项目管理", "软件工程理论与实践");
        
        std::cout << "测试数据加载完成" << std::endl;
    }
}

bool LibrarySystem::validateInput(const std::string& input) {
    return !input.empty() && input.length() <= 255;
}

std::string LibrarySystem::getCurrentTimeString() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

void LibrarySystem::updateStatistics() {
    statistics.clear();
    for (const auto& record : borrowRecords) {
        statistics.updateBookPopularity(record->getBookId());
        statistics.updateUserActivity(record->getUserId());
        statistics.updateMonthlyStats(record->getBorrowTime());
    }
}