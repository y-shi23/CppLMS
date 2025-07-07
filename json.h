#ifndef JSON_H
#define JSON_H

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iostream>
#include <memory>
#include <variant>

namespace Json {
    
class Value;

// JSON值类型枚举
enum ValueType {
    nullValue = 0,
    intValue,
    realValue,
    stringValue,
    booleanValue,
    arrayValue,
    objectValue
};

// JSON值类
class Value {
public:
    using Object = std::map<std::string, Value>;
    using Array = std::vector<Value>;
    
private:
    ValueType type_;
    std::variant<std::nullptr_t, int, double, std::string, bool, Array, Object> value_;
    
public:
    // 构造函数
    Value() : type_(nullValue), value_(nullptr) {}
    Value(int val) : type_(intValue), value_(val) {}
    Value(int64_t val) : type_(intValue), value_(static_cast<int>(val)) {}
    Value(double val) : type_(realValue), value_(val) {}
    Value(const std::string& val) : type_(stringValue), value_(val) {}
    Value(const char* val) : type_(stringValue), value_(std::string(val)) {}
    Value(bool val) : type_(booleanValue), value_(val) {}
    Value(const Array& val) : type_(arrayValue), value_(val) {}
    Value(const Object& val) : type_(objectValue), value_(val) {}
    Value(ValueType type) : type_(type) {
        switch (type) {
            case nullValue: value_ = nullptr; break;
            case intValue: value_ = 0; break;
            case realValue: value_ = 0.0; break;
            case stringValue: value_ = std::string(); break;
            case booleanValue: value_ = false; break;
            case arrayValue: value_ = Array(); break;
            case objectValue: value_ = Object(); break;
        }
    }
    
    // 类型检查
    ValueType type() const { return type_; }
    bool isNull() const { return type_ == nullValue; }
    bool isInt() const { return type_ == intValue; }
    bool isDouble() const { return type_ == realValue; }
    bool isString() const { return type_ == stringValue; }
    bool isBool() const { return type_ == booleanValue; }
    bool isArray() const { return type_ == arrayValue; }
    bool isObject() const { return type_ == objectValue; }
    
    // 值获取
    int asInt() const {
        if (type_ == intValue) return std::get<int>(value_);
        if (type_ == realValue) return static_cast<int>(std::get<double>(value_));
        if (type_ == stringValue) return std::stoi(std::get<std::string>(value_));
        return 0;
    }
    
    double asDouble() const {
        if (type_ == realValue) return std::get<double>(value_);
        if (type_ == intValue) return static_cast<double>(std::get<int>(value_));
        if (type_ == stringValue) return std::stod(std::get<std::string>(value_));
        return 0.0;
    }
    
    std::string asString() const {
        if (type_ == stringValue) return std::get<std::string>(value_);
        if (type_ == intValue) return std::to_string(std::get<int>(value_));
        if (type_ == realValue) return std::to_string(std::get<double>(value_));
        if (type_ == booleanValue) return std::get<bool>(value_) ? "true" : "false";
        return "";
    }
    
    bool asBool() const {
        if (type_ == booleanValue) return std::get<bool>(value_);
        if (type_ == intValue) return std::get<int>(value_) != 0;
        if (type_ == stringValue) {
            const std::string& str = std::get<std::string>(value_);
            return str == "true" || str == "1";
        }
        return false;
    }
    
    int64_t asInt64() const {
        return static_cast<int64_t>(asInt());
    }
    
    // 数组操作
    Value& operator[](int index) {
        if (type_ != arrayValue) {
            type_ = arrayValue;
            value_ = Array();
        }
        Array& arr = std::get<Array>(value_);
        while (arr.size() <= static_cast<size_t>(index)) {
            arr.emplace_back();
        }
        return arr[index];
    }
    
    const Value& operator[](int index) const {
        static Value nullVal;
        if (type_ != arrayValue) return nullVal;
        const Array& arr = std::get<Array>(value_);
        if (index < 0 || static_cast<size_t>(index) >= arr.size()) return nullVal;
        return arr[index];
    }
    
    void append(const Value& val) {
        if (type_ != arrayValue) {
            type_ = arrayValue;
            value_ = Array();
        }
        std::get<Array>(value_).push_back(val);
    }
    
    size_t size() const {
        if (type_ == arrayValue) return std::get<Array>(value_).size();
        if (type_ == objectValue) return std::get<Object>(value_).size();
        return 0;
    }
    
    // 对象操作
    Value& operator[](const std::string& key) {
        if (type_ != objectValue) {
            type_ = objectValue;
            value_ = Object();
        }
        return std::get<Object>(value_)[key];
    }
    
    const Value& operator[](const std::string& key) const {
        static Value nullVal;
        if (type_ != objectValue) return nullVal;
        const Object& obj = std::get<Object>(value_);
        auto it = obj.find(key);
        return (it != obj.end()) ? it->second : nullVal;
    }
    
    Value& operator[](const char* key) {
        return operator[](std::string(key));
    }
    
    const Value& operator[](const char* key) const {
        return operator[](std::string(key));
    }
    
    // 迭代器支持
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Value;
        using difference_type = std::ptrdiff_t;
        using pointer = Value*;
        using reference = Value&;
        
    private:
        Array::iterator array_it_;
        Object::iterator object_it_;
        ValueType type_;
        
    public:
        iterator(Array::iterator it) : array_it_(it), type_(arrayValue) {}
        iterator(Object::iterator it) : object_it_(it), type_(objectValue) {}
        
        Value& operator*() {
            if (type_ == arrayValue) return *array_it_;
            return object_it_->second;
        }
        
        iterator& operator++() {
            if (type_ == arrayValue) ++array_it_;
            else ++object_it_;
            return *this;
        }
        
        bool operator!=(const iterator& other) const {
            if (type_ != other.type_) return true;
            if (type_ == arrayValue) return array_it_ != other.array_it_;
            return object_it_ != other.object_it_;
        }
        
        bool operator==(const iterator& other) const {
            return !(*this != other);
        }
    };
    
    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const Value;
        using difference_type = std::ptrdiff_t;
        using pointer = const Value*;
        using reference = const Value&;
        
    private:
        Array::const_iterator array_it_;
        Object::const_iterator object_it_;
        ValueType type_;
        
    public:
        const_iterator(Array::const_iterator it) : array_it_(it), type_(arrayValue) {}
        const_iterator(Object::const_iterator it) : object_it_(it), type_(objectValue) {}
        
        const Value& operator*() const {
            if (type_ == arrayValue) return *array_it_;
            return object_it_->second;
        }
        
        const_iterator& operator++() {
            if (type_ == arrayValue) ++array_it_;
            else ++object_it_;
            return *this;
        }
        
        bool operator!=(const const_iterator& other) const {
            if (type_ != other.type_) return true;
            if (type_ == arrayValue) return array_it_ != other.array_it_;
            return object_it_ != other.object_it_;
        }
        
        bool operator==(const const_iterator& other) const {
            return !(*this != other);
        }
    };
    
    iterator begin() {
        if (type_ == arrayValue) return iterator(std::get<Array>(value_).begin());
        if (type_ == objectValue) return iterator(std::get<Object>(value_).begin());
        return iterator(Array().begin());
    }
    
    iterator end() {
        if (type_ == arrayValue) return iterator(std::get<Array>(value_).end());
        if (type_ == objectValue) return iterator(std::get<Object>(value_).end());
        return iterator(Array().end());
    }
    
    const_iterator begin() const {
        if (type_ == arrayValue) return const_iterator(std::get<Array>(value_).begin());
        if (type_ == objectValue) return const_iterator(std::get<Object>(value_).begin());
        return const_iterator(Array().begin());
    }
    
    const_iterator end() const {
        if (type_ == arrayValue) return const_iterator(std::get<Array>(value_).end());
        if (type_ == objectValue) return const_iterator(std::get<Object>(value_).end());
        return const_iterator(Array().end());
    }
    
    // 序列化为字符串
    std::string toString() const {
        std::ostringstream oss;
        serialize(oss);
        return oss.str();
    }
    
private:
    void serialize(std::ostringstream& oss) const {
        switch (type_) {
            case nullValue:
                oss << "null";
                break;
            case intValue:
                oss << std::get<int>(value_);
                break;
            case realValue:
                oss << std::get<double>(value_);
                break;
            case stringValue:
                oss << '"' << escapeString(std::get<std::string>(value_)) << '"';
                break;
            case booleanValue:
                oss << (std::get<bool>(value_) ? "true" : "false");
                break;
            case arrayValue: {
                oss << '[';
                const Array& arr = std::get<Array>(value_);
                for (size_t i = 0; i < arr.size(); ++i) {
                    if (i > 0) oss << ',';
                    arr[i].serialize(oss);
                }
                oss << ']';
                break;
            }
            case objectValue: {
                oss << '{';
                const Object& obj = std::get<Object>(value_);
                bool first = true;
                for (const auto& pair : obj) {
                    if (!first) oss << ',';
                    first = false;
                    oss << '"' << escapeString(pair.first) << "\":";
                    pair.second.serialize(oss);
                }
                oss << '}';
                break;
            }
        }
    }
    
    std::string escapeString(const std::string& str) const {
        std::string result;
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

// 简化的JSON解析器
class Reader {
public:
    bool parse(const std::string& document, Value& root) {
        try {
            size_t pos = 0;
            root = parseValue(document, pos);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }
    
private:
    Value parseValue(const std::string& str, size_t& pos) {
        skipWhitespace(str, pos);
        
        if (pos >= str.length()) {
            throw std::runtime_error("Unexpected end of input");
        }
        
        char c = str[pos];
        
        if (c == '{') {
            return parseObject(str, pos);
        } else if (c == '[') {
            return parseArray(str, pos);
        } else if (c == '"') {
            return parseString(str, pos);
        } else if (c == 't' || c == 'f') {
            return parseBool(str, pos);
        } else if (c == 'n') {
            return parseNull(str, pos);
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber(str, pos);
        }
        
        throw std::runtime_error("Invalid JSON value");
    }
    
    Value parseObject(const std::string& str, size_t& pos) {
        Value obj(objectValue);
        ++pos; // skip '{'
        
        skipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == '}') {
            ++pos;
            return obj;
        }
        
        while (pos < str.length()) {
            skipWhitespace(str, pos);
            
            // Parse key
            if (str[pos] != '"') {
                throw std::runtime_error("Expected string key");
            }
            std::string key = parseString(str, pos).asString();
            
            skipWhitespace(str, pos);
            if (pos >= str.length() || str[pos] != ':') {
                throw std::runtime_error("Expected ':'");
            }
            ++pos;
            
            // Parse value
            obj[key] = parseValue(str, pos);
            
            skipWhitespace(str, pos);
            if (pos >= str.length()) break;
            
            if (str[pos] == '}') {
                ++pos;
                break;
            } else if (str[pos] == ',') {
                ++pos;
            } else {
                throw std::runtime_error("Expected ',' or '}'");
            }
        }
        
        return obj;
    }
    
    Value parseArray(const std::string& str, size_t& pos) {
        Value arr(arrayValue);
        ++pos; // skip '['
        
        skipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ']') {
            ++pos;
            return arr;
        }
        
        while (pos < str.length()) {
            arr.append(parseValue(str, pos));
            
            skipWhitespace(str, pos);
            if (pos >= str.length()) break;
            
            if (str[pos] == ']') {
                ++pos;
                break;
            } else if (str[pos] == ',') {
                ++pos;
            } else {
                throw std::runtime_error("Expected ',' or ']'");
            }
        }
        
        return arr;
    }
    
    Value parseString(const std::string& str, size_t& pos) {
        ++pos; // skip opening quote
        std::string result;
        
        while (pos < str.length() && str[pos] != '"') {
            if (str[pos] == '\\' && pos + 1 < str.length()) {
                ++pos;
                char escaped = str[pos];
                switch (escaped) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: result += escaped; break;
                }
            } else {
                result += str[pos];
            }
            ++pos;
        }
        
        if (pos >= str.length()) {
            throw std::runtime_error("Unterminated string");
        }
        
        ++pos; // skip closing quote
        return Value(result);
    }
    
    Value parseNumber(const std::string& str, size_t& pos) {
        size_t start = pos;
        
        if (str[pos] == '-') ++pos;
        
        while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
            ++pos;
        }
        
        bool isDouble = false;
        if (pos < str.length() && str[pos] == '.') {
            isDouble = true;
            ++pos;
            while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
                ++pos;
            }
        }
        
        if (pos < str.length() && (str[pos] == 'e' || str[pos] == 'E')) {
            isDouble = true;
            ++pos;
            if (pos < str.length() && (str[pos] == '+' || str[pos] == '-')) {
                ++pos;
            }
            while (pos < str.length() && str[pos] >= '0' && str[pos] <= '9') {
                ++pos;
            }
        }
        
        std::string numStr = str.substr(start, pos - start);
        
        if (isDouble) {
            return Value(std::stod(numStr));
        } else {
            return Value(std::stoi(numStr));
        }
    }
    
    Value parseBool(const std::string& str, size_t& pos) {
        if (str.substr(pos, 4) == "true") {
            pos += 4;
            return Value(true);
        } else if (str.substr(pos, 5) == "false") {
            pos += 5;
            return Value(false);
        }
        throw std::runtime_error("Invalid boolean value");
    }
    
    Value parseNull(const std::string& str, size_t& pos) {
        if (str.substr(pos, 4) == "null") {
            pos += 4;
            return Value();
        }
        throw std::runtime_error("Invalid null value");
    }
    
    void skipWhitespace(const std::string& str, size_t& pos) {
        while (pos < str.length() && 
               (str[pos] == ' ' || str[pos] == '\t' || str[pos] == '\n' || str[pos] == '\r')) {
            ++pos;
        }
    }
};

// 简化的JSON写入器
class StreamWriterBuilder {
public:
    std::string& operator[](const std::string& key) {
        return settings_[key];
    }
    
private:
    std::map<std::string, std::string> settings_;
};

inline std::string writeString(const StreamWriterBuilder&, const Value& value) {
    return value.toString();
}

} // namespace Json

// 流操作符重载
inline std::ostream& operator<<(std::ostream& os, const Json::Value& value) {
    os << value.toString();
    return os;
}

inline std::istream& operator>>(std::istream& is, Json::Value& value) {
    std::string content((std::istreambuf_iterator<char>(is)),
                        std::istreambuf_iterator<char>());
    Json::Reader reader;
    reader.parse(content, value);
    return is;
}

#endif // JSON_H