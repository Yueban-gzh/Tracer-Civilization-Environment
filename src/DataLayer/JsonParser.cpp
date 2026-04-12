/**
 * 简易 JSON 解析实现
 *
 * 课设数据结构：
 * - get_key：哈希表按 key 查找（find），O(1) 平均；
 * - parse_array：顺序表尾插（push_back）存储数组元素；
 * - parse_object：哈希表插入（obj[key]=value）存储键值对；
 * - parse_value 等递归调用：等价于借助系统栈实现递归下降解析。
 */

#include "DataLayer/JsonParser.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace DataLayer {

// 哈希查找：在 JSON 对象（哈希表 obj）中按 key 查找，找不到返回 nullptr
const JsonValue* JsonValue::get_key(const std::string& key) const {
    if (type != Type::Object) return nullptr;
    auto it = obj.find(key);
    if (it == obj.end()) return nullptr;
    return &it->second;
}

std::string JsonValue::as_string() const {
    if (type == Type::String) return s;
    if (type == Type::Int) return std::to_string(i);
    if (type == Type::Bool) return b ? "true" : "false";
    return "";
}

int JsonValue::as_int() const {
    if (type == Type::Int) return i;
    if (type == Type::Bool) return b ? 1 : 0;
    return 0;
}

bool JsonValue::as_bool() const {
    if (type == Type::Bool) return b;
    if (type == Type::Int) return i != 0;
    return false;
}

// ----- 递归下降解析（递归即栈：每次 parse_value/parse_array/parse_object 调用压栈，返回时出栈）-----
static std::string read_file_utf8(const std::string& path) {
    namespace fs = std::filesystem;
    std::ifstream f(fs::u8path(path), std::ios::binary);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    // 处理 UTF-8 BOM（0xEF,0xBB,0xBF），避免影响首个 token 识别
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content.erase(0, 3);
    }
    return content;
}

static size_t skip_ws(const std::string& s, size_t i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    return i;
}

static JsonValue parse_value(const std::string& s, size_t& i);

static std::string parse_string(const std::string& s, size_t& i) {
    std::string out;
    if (i >= s.size() || s[i] != '"') return "";
    ++i;
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"') break;
        if (c == '\\') {
            if (i >= s.size()) break;
            c = s[i++];
            if (c == 'n') out += '\n';
            else if (c == 't') out += '\t';
            else if (c == 'r') out += '\r';
            else if (c == '"' || c == '\\') out += c;
            else out += c;
        } else
            out += c;
    }
    return out;
}

static JsonValue parse_number(const std::string& s, size_t& i) {
    JsonValue v;
    v.type = JsonValue::Type::Int;
    int sign = 1;
    if (i < s.size() && s[i] == '-') { sign = -1; ++i; }
    int num = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
        num = num * 10 + (s[i++] - '0');
    v.i = sign * num;
    return v;
}

// 解析 JSON 数组 → 用顺序表 arr 存储，逐元素 push_back（顺序表尾插）
static JsonValue parse_array(const std::string& s, size_t& i) {
    JsonValue v;
    v.type = JsonValue::Type::Array;
    if (i >= s.size() || s[i] != '[') return v;
    ++i;
    i = skip_ws(s, i);
    if (i < s.size() && s[i] == ']') { ++i; return v; }
    while (i < s.size()) {
        v.arr.push_back(parse_value(s, i));  // 顺序表尾插，保持元素顺序
        i = skip_ws(s, i);
        if (i < s.size() && s[i] == ',') { ++i; i = skip_ws(s, i); continue; }
        if (i < s.size() && s[i] == ']') { ++i; break; }
        break;
    }
    return v;
}

// 解析 JSON 对象 → 用哈希表 obj 存储键值对，按 key 插入（便于后续 get_key 查找）
static JsonValue parse_object(const std::string& s, size_t& i) {
    JsonValue v;
    v.type = JsonValue::Type::Object;
    if (i >= s.size() || s[i] != '{') return v;
    ++i;
    i = skip_ws(s, i);
    if (i < s.size() && s[i] == '}') { ++i; return v; }
    while (i < s.size()) {
        std::string key = parse_string(s, i);
        i = skip_ws(s, i);
        if (i < s.size() && s[i] == ':') ++i;
        i = skip_ws(s, i);
        v.obj[key] = parse_value(s, i);  // 哈希表插入：key 为 JSON 键名
        i = skip_ws(s, i);
        if (i < s.size() && s[i] == ',') { ++i; i = skip_ws(s, i); continue; }
        if (i < s.size() && s[i] == '}') { ++i; break; }
        break;
    }
    return v;
}

// 递归下降入口：根据首字符分发到 parse_string/parse_array/parse_object/parse_number 等（递归调用栈）
static JsonValue parse_value(const std::string& s, size_t& i) {
    i = skip_ws(s, i);
    if (i >= s.size()) return JsonValue();
    char c = s[i];
    if (c == '"') {
        JsonValue v;
        v.type = JsonValue::Type::String;
        v.s = parse_string(s, i);
        return v;
    }
    if (c == '[') return parse_array(s, i);
    if (c == '{') return parse_object(s, i);
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number(s, i);
    if (c == 't' && s.substr(i, 4) == "true")  { i += 4; JsonValue v; v.type = JsonValue::Type::Bool; v.b = true;  return v; }
    if (c == 'f' && s.substr(i, 5) == "false") { i += 5; JsonValue v; v.type = JsonValue::Type::Bool; v.b = false; return v; }
    if (c == 'n' && s.substr(i, 4) == "null")  { i += 4; return JsonValue(); }
    return JsonValue();
}

JsonValue parse_json(const std::string& content) {
    size_t i = 0;
    i = skip_ws(content, i);
    if (i >= content.size()) return JsonValue();
    return parse_value(content, i);
}

JsonValue parse_json_file(const std::string& path) {
    std::string content = read_file_utf8(path);
    if (content.empty()) return JsonValue();
    return parse_json(content);
}

} // namespace DataLayer
