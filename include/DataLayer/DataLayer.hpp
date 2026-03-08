/**
 * 数据层（E）：卡牌/怪物静态数据，供 B、C 只读使用
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

} // namespace tce
