/**
 * DataLayer 数据类型定义
 * 与 docs/设计与接口.md 第二节数据表格式一致
 */

#pragma once

#include <string>
#include <vector>

namespace DataLayer {

// ---------- 卡牌 ----------
struct Card {
    std::string id;
    std::string name;
    std::string cardType;   // "attack" | "defend" | "skill" | "support"
    int         cost       = 0;
    std::string rarity;    // "common" | "uncommon" | "rare"
    std::string description;
    std::string effectType;
    int         effectValue = 0;
};

// ---------- 怪物 ----------
struct Monster {
    std::string id;
    std::string name;
    bool        isBoss     = false;
    int         maxHp      = 0;
    std::string intentPattern;  // 如 "attack,block"
    int         attackDamage = 0;
    int         blockAmount  = 0;
};

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

// ---------- 事件 ----------
struct Event {
    std::string id;
    std::string title;
    std::string description;
    std::vector<EventOption> options;
};

// 公共 id 类型（与文档一致，统一 string）
using CardId    = std::string;
using MonsterId = std::string;
using EventId   = std::string;

} // namespace DataLayer
