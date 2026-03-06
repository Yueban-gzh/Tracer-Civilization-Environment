/**
 * 数据层（E）桩实现
 */
#include "../../include/DataLayer/DataLayer.hpp"
#include <unordered_map>

namespace tce {

namespace {

std::unordered_map<CardId, CardData> s_cards;
std::unordered_map<MonsterId, MonsterData> s_monsters;

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
