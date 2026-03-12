// Types.hpp
// 这个文件定义了所有模块共用的类型
// 使用 #pragma once 防止重复包含（现代C++推荐方式）

#pragma once
#include <string>

// 节点类型枚举：地图上有哪几种类型的节点
enum class NodeType {
    Battle,     // 战斗节点（打怪）
    Event,      // 事件节点（随机事件）
    Rest,       // 休息节点（回血）
    Shop,       // 商店节点（买东西）
    Boss        // Boss节点（最终战斗）
};

// 节点ID：用字符串表示，比如 "1-2" 表示第1层第2个节点
using NodeId = std::string;

// 内容ID：指向具体的战斗配置、事件配置等
using ContentId = std::string;