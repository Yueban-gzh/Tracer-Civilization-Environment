/**
 * 数据层（E）：卡牌/怪物静态数据，供 B、C 只读使用
 *
 * - CardData / MonsterData 与 docs/BC模块设计与接口.md 一致，为唯一数据源（无冗余副本）。
 * - 效果不由 E 提供（C 用卡牌 id→效果函数）；意图/伤害/格挡不由 E 提供（B 用怪物 id→行为函数）。
 * - 实现与 load 在 DataLayer.cpp：主流程调用 DataLayerImpl::load_cards/load_monsters 直接填充本命名空间存储，
 *   get_card_by_id / get_monster_by_id 供 B/C 与 DataLayer 共用。
 */
#pragma once

#include "../Common/Types.hpp"
#include <string>

namespace tce {

enum class CardType { Attack, Skill, Power, Status, Curse };
enum class Rarity { Common, Uncommon, Rare };
enum class MonsterType { Normal, Elite, Boss };

struct CardData {
    CardId     id;
    std::string name;
    CardType   cardType   = CardType::Attack;
    int        cost       = 0;
    Rarity     rarity     = Rarity::Common;
    std::string description;
    bool       exhaust    = false;
    bool       ethereal   = false;
    bool       innate     = false;
    bool       retain     = false;
    bool       unplayable = false;
    // 是否需要确认目标：true 时需要外部提供具体目标（如敌人）；
    // false 时使用默认目标（通常是玩家自身或全体）
    bool       requiresTarget = false;
};

struct MonsterData {
    MonsterId  id;
    std::string name;
    MonsterType type  = MonsterType::Normal;
    int         maxHp = 0;
};

// E 对外接口：按 id 获取数据，未就绪时可由 Mock 实现
const CardData*    get_card_by_id(CardId id);
const MonsterData* get_monster_by_id(MonsterId id);

inline const char* to_string(Rarity r) {
    switch (r) { case Rarity::Common: return "common"; case Rarity::Uncommon: return "uncommon"; case Rarity::Rare: return "rare"; default: return "common"; }
}
inline const char* to_string(MonsterType t) {
    switch (t) { case MonsterType::Normal: return "normal"; case MonsterType::Elite: return "elite"; case MonsterType::Boss: return "boss"; default: return "normal"; }
}

} // namespace tce
