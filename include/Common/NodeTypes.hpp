// include/Common/NodeTypes.hpp
#pragma once
#include <string>

// 节点类型枚举 - 现在有6种类型
enum class NodeType {
    Enemy,      // 普通战斗节点 (原来的Battle)
    Elite,      // 精英战斗节点 (新增)
    Event,      // 事件节点
    Rest,       // 休息节点
    Merchant,   // 商店节点 (原来的Shop)
    Treasure,   // 宝藏节点 (新增)
    Boss        // Boss节点
};

// 节点ID
using NodeId = std::string;

// 内容ID
using ContentId = std::string;

// 2D坐标（用于UI）
struct Vector2 {
    float x = 0.0f;
    float y = 0.0f;
};