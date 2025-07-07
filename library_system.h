#ifndef LIBRARY_SYSTEM_H
#define LIBRARY_SYSTEM_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <memory>
#include "json.h"

// 抽象基类 - 实体基类
class Entity {
protected:
    int id;
    std::string name;
    std::time_t createTime;
    
public:
    Entity(int id = 0, const std::string& name = "") 
        : id(id), name(name), createTime(std::time(nullptr)) {}
    
    virtual ~Entity() = default;
    
    // 纯虚函数
    virtual std::string toString() const = 0;
    virtual Json::Value toJson() const = 0;
    virtual void fromJson(const Json::Value& json) = 0;
    
    // 虚函数
    virtual void display() const {
        std::cout << "ID: " << id << ", Name: " << name << std::endl;
    }
    
    // 访问器
    int getId() const { return id; }
    std::string getName() const { return name; }
    std::time_t getCreateTime() const { return createTime; }
    
    void setId(int newId) { id = newId; }
    void setName(const std::string& newName) { name = newName; }
};

// 用户类
class User : public Entity {
private:
    std::string email;
    std::string phone;
    std::vector<int> borrowHistory;
    int maxBorrowCount;
    
public:
    User(int id = 0, const std::string& name = "", const std::string& email = "", 
         const std::string& phone = "", int maxBorrow = 5)
        : Entity(id, name), email(email), phone(phone), maxBorrowCount(maxBorrow) {}
    
    // 重写虚函数
    std::string toString() const override;
    Json::Value toJson() const override;
    void fromJson(const Json::Value& json) override;
    void display() const override;
    
    // 用户特有方法
    void addBorrowRecord(int bookId);
    void removeBorrowRecord(int bookId);
    bool canBorrow() const;
    int getCurrentBorrowCount() const;
    
    // 访问器
    std::string getEmail() const { return email; }
    std::string getPhone() const { return phone; }
    const std::vector<int>& getBorrowHistory() const { return borrowHistory; }
    int getMaxBorrowCount() const { return maxBorrowCount; }
    
    void setEmail(const std::string& newEmail) { email = newEmail; }
    void setPhone(const std::string& newPhone) { phone = newPhone; }
    void setMaxBorrowCount(int count) { maxBorrowCount = count; }
};

// 图书类
class Book : public Entity {
private:
    std::string author;
    std::string category;
    std::string keywords;
    std::string description;
    bool isAvailable;
    int borrowerId;
    std::vector<int> borrowHistory;
    
public:
    Book(int id = 0, const std::string& title = "", const std::string& author = "",
         const std::string& category = "", const std::string& keywords = "",
         const std::string& description = "")
        : Entity(id, title), author(author), category(category), 
          keywords(keywords), description(description), 
          isAvailable(true), borrowerId(0) {}
    
    // 重写虚函数
    std::string toString() const override;
    Json::Value toJson() const override;
    void fromJson(const Json::Value& json) override;
    void display() const override;
    
    // 图书特有方法
    void borrowBook(int userId);
    void returnBook();
    bool matchesKeyword(const std::string& keyword) const;
    void addBorrowHistory(int userId);
    
    // 访问器
    std::string getAuthor() const { return author; }
    std::string getCategory() const { return category; }
    std::string getKeywords() const { return keywords; }
    std::string getDescription() const { return description; }
    bool getIsAvailable() const { return isAvailable; }
    int getBorrowerId() const { return borrowerId; }
    const std::vector<int>& getBorrowHistory() const { return borrowHistory; }
    
    void setAuthor(const std::string& newAuthor) { author = newAuthor; }
    void setCategory(const std::string& newCategory) { category = newCategory; }
    void setKeywords(const std::string& newKeywords) { keywords = newKeywords; }
    void setDescription(const std::string& newDesc) { description = newDesc; }
};

// 借阅记录类
class BorrowRecord {
private:
    int recordId;
    int userId;
    int bookId;
    std::time_t borrowTime;
    std::time_t returnTime;
    bool isReturned;
    
public:
    BorrowRecord(int id, int userId, int bookId)
        : recordId(id), userId(userId), bookId(bookId), 
          borrowTime(std::time(nullptr)), returnTime(0), isReturned(false) {}
    
    void returnBook();
    Json::Value toJson() const;
    void fromJson(const Json::Value& json);
    std::string toString() const;
    
    // 访问器
    int getRecordId() const { return recordId; }
    int getUserId() const { return userId; }
    int getBookId() const { return bookId; }
    std::time_t getBorrowTime() const { return borrowTime; }
    std::time_t getReturnTime() const { return returnTime; }
    bool getIsReturned() const { return isReturned; }
};

// 统计分析类 - 多重继承示例
class Displayable {
public:
    virtual void showStatistics() const = 0;
    virtual ~Displayable() = default;
};

class Serializable {
public:
    virtual Json::Value serialize() const = 0;
    virtual ~Serializable() = default;
};

class Statistics : public Displayable, public Serializable {
private:
    std::map<int, int> bookPopularity;  // 图书ID -> 借阅次数
    std::map<int, int> userActivity;    // 用户ID -> 借阅次数
    std::map<std::string, int> monthlyStats; // 月份 -> 借阅次数
    
public:
    void updateBookPopularity(int bookId);
    void updateUserActivity(int userId);
    void updateMonthlyStats(std::time_t borrowTime);
    
    std::vector<std::pair<int, int>> getMostPopularBooks(int count = 10) const;
    std::vector<std::pair<int, int>> getMostActiveUsers(int count = 10) const;
    std::map<std::string, int> getMonthlyTrends() const;
    
    // 实现抽象方法
    void showStatistics() const override;
    Json::Value serialize() const override;
    
    void clear();
};

// 主要的图书管理系统类
class LibrarySystem {
private:
    std::vector<std::unique_ptr<User>> users;
    std::vector<std::unique_ptr<Book>> books;
    std::vector<std::unique_ptr<BorrowRecord>> borrowRecords;
    Statistics statistics;
    
    int nextUserId;
    int nextBookId;
    int nextRecordId;
    
    // 数据文件路径
    const std::string USERS_FILE = "data/users.json";
    const std::string BOOKS_FILE = "data/books.json";
    const std::string RECORDS_FILE = "data/records.json";
    
public:
    LibrarySystem();
    ~LibrarySystem();
    
    // 用户管理
    int addUser(const std::string& name, const std::string& email, const std::string& phone);
    bool deleteUser(int userId);
    bool updateUser(int userId, const std::string& name, const std::string& email, const std::string& phone);
    User* findUser(int userId);
    std::vector<User*> searchUsers(const std::string& keyword);
    std::vector<User*> getAllUsers();
    
    // 图书管理
    int addBook(const std::string& title, const std::string& author, 
                const std::string& category, const std::string& keywords, 
                const std::string& description);
    bool deleteBook(int bookId);
    bool updateBook(int bookId, const std::string& title, const std::string& author,
                   const std::string& category, const std::string& keywords,
                   const std::string& description);
    Book* findBook(int bookId);
    std::vector<Book*> searchBooks(const std::string& keyword);
    std::vector<Book*> getAllBooks();
    
    // 借还书管理
    bool borrowBook(int userId, int bookId);
    bool returnBook(int userId, int bookId);
    std::vector<BorrowRecord*> getUserBorrowHistory(int userId);
    std::vector<BorrowRecord*> getBookBorrowHistory(int bookId);
    std::vector<BorrowRecord*> getAllBorrowRecords();
    
    // 统计分析
    Statistics& getStatistics() { return statistics; }
    Json::Value getStatisticsJson();
    
    // 数据持久化
    void saveData();
    void loadData();
    void loadTestData();
    
    // 工具方法
    bool validateInput(const std::string& input);
    std::string getCurrentTimeString();
    
private:
    void createDataDirectory();
    void updateStatistics();
};

#endif // LIBRARY_SYSTEM_H