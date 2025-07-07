# C++ 图书管理系统

一个基于C++开发的现代化图书管理系统，具有Web界面和完整的图书馆管理功能。

## 功能特性

### 核心功能
- **用户管理**：用户注册、信息修改、用户查询
- **图书管理**：图书添加、信息更新、模糊查询、分类管理
- **借还书管理**：图书借阅、归还、续借、逾期处理
- **统计分析**：借阅统计、图书流行度、用户活跃度分析
- **数据持久化**：JSON格式数据存储，支持数据导入导出

### 技术特性
- **面向对象设计**：采用C++面向对象编程思想
- **设计模式**：包含抽象类、继承、多重继承、虚函数多态性
- **运算符重载**：自定义比较和输出操作
- **Web界面**：现代化响应式Web UI
- **HTTP服务器**：内置轻量级HTTP服务器
- **跨平台**：支持Windows平台编译运行

## 系统架构

### 类设计结构

```
Entity (抽象基类)
├── User (用户类)
├── Book (图书类)
└── BorrowRecord (借阅记录类)

Displayable (显示接口)
Serializable (序列化接口)

Statistics (统计类) - 多重继承自 Displayable 和 Serializable

LibrarySystem (主管理类)
HttpServer (HTTP服务器类)
```

### 主要类说明

- **Entity**: 抽象基类，定义了所有实体的基本接口
- **User**: 用户类，管理用户信息和操作
- **Book**: 图书类，管理图书信息和库存
- **BorrowRecord**: 借阅记录类，跟踪借还书操作
- **Statistics**: 统计类，提供各种统计分析功能
- **LibrarySystem**: 系统主类，协调各个模块
- **HttpServer**: Web服务器类，提供HTTP接口和Web界面

## 编译和运行

### 系统要求
- Windows 10/11
- Visual Studio 2019/2022 或 MinGW-w64
- CMake 3.15+
- C++20 支持

### 编译步骤

1. **克隆或下载项目**
   ```bash
   cd d:/dev/C++library
   ```

2. **创建构建目录**
   ```bash
   mkdir build
   cd build
   ```

3. **生成项目文件**
   ```bash
   cmake ..
   ```

4. **编译项目**
   ```bash
   cmake --build . --config Release
   ```

5. **运行程序**
   ```bash
   cd Release
   ./LibrarySystem.exe
   ```

### 快速启动

程序启动后会自动：
1. 加载测试数据（`test_data.json`）
2. 启动HTTP服务器（默认端口8080）
3. 在默认浏览器中打开Web界面

访问地址：`http://localhost:8080`

## 使用说明

### Web界面功能

1. **用户管理**
   - 添加新用户
   - 查看用户列表
   - 编辑用户信息
   - 删除用户

2. **图书管理**
   - 添加新图书
   - 图书信息查询（支持模糊搜索）
   - 编辑图书信息
   - 管理图书库存

3. **借还书操作**
   - 图书借阅
   - 图书归还
   - 查看借阅记录
   - 逾期提醒

4. **统计分析**
   - 借阅统计图表
   - 热门图书排行
   - 用户活跃度分析
   - 库存状态概览

### 数据文件

- **test_data.json**: 包含初始测试数据
  - 5个测试用户
  - 10本测试图书
  - 8条借阅记录
  - 统计信息

- **library_data.json**: 运行时数据文件（自动生成）

## 项目结构

```
C++library/
├── main.cpp              # 程序入口
├── library_system.h      # 核心类定义
├── library_system.cpp    # 核心类实现
├── http_server.h         # HTTP服务器定义
├── http_server.cpp       # HTTP服务器实现
├── json.h                # 自定义JSON库
├── test_data.json        # 测试数据
├── CMakeLists.txt        # CMake配置
├── README.md             # 项目说明
└── 要求.txt              # 项目需求文档
```

## 设计特点

### 面向对象特性

1. **抽象类和继承**
   ```cpp
   class Entity {  // 抽象基类
   public:
       virtual ~Entity() = default;
       virtual void display() const = 0;
       virtual Json::Value toJson() const = 0;
   };
   ```

2. **多重继承**
   ```cpp
   class Statistics : public Displayable, public Serializable {
       // 继承多个接口
   };
   ```

3. **虚函数和多态**
   ```cpp
   virtual bool operator==(const Entity& other) const = 0;
   virtual bool operator<(const Entity& other) const = 0;
   ```

4. **运算符重载**
   ```cpp
   friend std::ostream& operator<<(std::ostream& os, const User& user);
   bool operator==(const User& other) const override;
   ```

### 错误处理

- 输入验证和数据校验
- 异常处理机制
- 用户友好的错误提示
- 数据完整性检查

### 性能优化

- 智能指针管理内存
- 高效的数据结构
- 缓存机制
- 异步处理

## 扩展功能

系统设计具有良好的可扩展性，可以轻松添加：

- 图书预约功能
- 罚金管理
- 图书推荐系统
- 多语言支持
- 数据库集成
- 用户权限管理
- 移动端适配

## 技术栈

- **后端**: C++20, STL, Winsock2
- **前端**: HTML5, CSS3, JavaScript (ES6+)
- **数据**: 自定义JSON库
- **构建**: CMake
- **平台**: Windows

## 许可证

本项目仅用于学习和教育目的。

## 联系方式

如有问题或建议，请通过以下方式联系：
- 项目地址：`d:/dev/C++library`
- 文档：参见 `要求.txt` 文件

---

**注意**: 首次运行前请确保系统已安装必要的编译工具和运行时库。