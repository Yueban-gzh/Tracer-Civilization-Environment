/**
 * DataLayer 数据类型定义
 * 与 docs/设计与接口.md 第二节数据表格式一致
 *
 * 课设数据结构对应：
 * - 各 struct 为数据元素（记录）的抽象，对应表的一行；
 * - vector 用于顺序存储（线性表），如事件的 options 列表。
 */

#pragma once

#include <string>
#include <vector>

namespace DataLayer {

// 卡牌/怪物静态数据统一使用 tce::CardData / tce::MonsterData（见 DataLayer.hpp），此处仅保留事件与 id 类型。

// ---------- 事件：选项结果 ----------
struct EventResult {
    std::string type;   // "gold" | "heal" | "card_reward" | "none" 等
    int         value = 0;
};

// ---------- 事件：单个选项 ----------
struct EventOption {
    std::string text;
    std::string next;    // 有则跳转 EventId
    EventResult result;  // 有则结束（next 与 result 二选一由数据约定）
};

// ---------- 事件（表的一条记录；options 为线性表顺序存储选项）----------
struct Event {
    std::string id;
    std::string title;
    std::string description;
    std::string image;  // 可选，插图路径，如 "assets/images/events/event_001.png"
    std::vector<EventOption> options;  // 顺序表：选项顺序与 JSON 中一致，按下标访问
};

// 公共 id 类型（与文档一致，统一 string，作为哈希表 key）
using CardId    = std::string;
using MonsterId = std::string;
using EventId   = std::string;

} // namespace DataLayer
