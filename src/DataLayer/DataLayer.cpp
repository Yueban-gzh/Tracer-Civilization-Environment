/**
 * 数据层（E）桩实现
 */
#include "../../include/DataLayer/DataLayer.hpp"
#include <unordered_map>
#include <vector>

namespace tce {

namespace {

std::unordered_map<CardId, CardData> s_cards{
    {
        "strike",
        CardData{
            "strike",
            u8"打击",
            CardType::Attack,
            1,
            Rarity::Common,
            u8"造成6点伤害。",
            false,
            false,
            false,
            false,
            false,
            true  // 需要指定敌人目标
        }
    },
    {
        "strike+",
        CardData{
            "strike+",
            u8"打击+",
            CardType::Attack,
            1,
            Rarity::Common,
            u8"造成9点伤害。",
            false,
            false,
            false,
            false,
            false,
            true
        }
    },
    {
        "defend",
        CardData{
            "defend",
            u8"防御",
            CardType::Skill,  // 防御牌为技能牌
            1,
            Rarity::Common,
            u8"获得5点格挡。",
            false,
            false,
            false,
            false,
            false,
            false
        }
    },
    {
        "defend+",
        CardData{
            "defend+",
            u8"防御+",
            CardType::Skill,
            1,
            Rarity::Common,
            u8"获得8点格挡。",
            false,
            false,
            false,
            false,
            false,
            false
        }
    },
    {
        "bash",
        CardData{
            "bash",
            u8"重击",
            CardType::Attack,
            2,
            Rarity::Uncommon,
            u8"造成8点伤害，并施加2层易伤",
            false,
            false,
            false,
            false,
            false,
            true
        }
    },
    {
        "bash+",
        CardData{
            "bash+",
            u8"重击+",
            CardType::Attack,
            2,
            Rarity::Uncommon,
            u8"造成10点伤害，并施加3层易伤。",
            false,
            false,
            false,
            false,
            false,
            true
        }
    },
};

std::unordered_map<MonsterId, MonsterData> s_monsters{
    {
        "cultist",
        MonsterData{
            "cultist",
            u8"邪教徒",
            MonsterType::Normal,
            100
        }
    },
};

} // namespace

const CardData* get_card_by_id(CardId id) {
    auto it = s_cards.find(id);
    if (it != s_cards.end()) return &it->second;
    return nullptr;
}

const MonsterData* get_monster_by_id(MonsterId id) {
    auto it = s_monsters.find(id);
    if (it != s_monsters.end()) return &it->second;
    return nullptr;
}

} // namespace tce
