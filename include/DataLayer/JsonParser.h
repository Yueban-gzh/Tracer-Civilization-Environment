/**
 * 简易 JSON 解析（仅满足 data/*.json 格式）
 * 用于 DataLayer 加载，无外部依赖
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

namespace DataLayer {

struct JsonValue;
using JsonObject = std::unordered_map<std::string, JsonValue>;
using JsonArray  = std::vector<JsonValue>;

struct JsonValue {
    enum class Type { Null, Bool, Int, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    int i = 0;
    std::string s;
    std::vector<JsonValue> arr;
    std::unordered_map<std::string, JsonValue> obj;

    bool is_null()   const { return type == Type::Null; }
    bool is_bool()   const { return type == Type::Bool; }
    bool is_int()    const { return type == Type::Int; }
    bool is_string() const { return type == Type::String; }
    bool is_array()  const { return type == Type::Array; }
    bool is_object() const { return type == Type::Object; }

    const JsonValue* get_key(const std::string& key) const;
    std::string as_string() const;
    int as_int() const;
    bool as_bool() const;
};

// 解析整段 JSON，返回根节点（可为 Array 或 Object）
JsonValue parse_json(const std::string& content);

// 从文件路径读取并解析
JsonValue parse_json_file(const std::string& path);

} // namespace DataLayer
