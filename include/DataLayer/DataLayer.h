/**
 * DataLayer 对外接口
 * 提供卡牌/怪物/事件表加载与按 id 查找（哈希）、奖励排序
 * 见 docs/设计与接口.md 第五节 E - DataLayer
 */

#pragma once

#include "DataTypes.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace DataLayer {

class DataLayerImpl {
public:
    DataLayerImpl() = default;

    // ---------- 加载（主流程启动时各调一次，或首次 get 时懒加载）----------
    bool load_cards(const std::string& path_or_base_dir);
    bool load_monsters(const std::string& path_or_base_dir);
    bool load_events(const std::string& path_or_base_dir);

    // ---------- 按 id 查找（哈希表 O(1)）；id 不存在返回 nullptr ----------
    const Card*    get_card_by_id(const CardId& id) const;
    const Monster* get_monster_by_id(const MonsterId& id) const;
    const Event*   get_event_by_id(const EventId& id) const;

    // ---------- 排序：按稀有度 common < uncommon < rare ----------
    std::vector<CardId> sort_cards_by_rarity(const std::vector<CardId>& card_ids) const;

    // ---------- 可选：排行榜按 score 降序 ----------
    struct LeaderboardEntry {
        std::string playerId;
        int         score = 0;
        int         time  = 0;
    };
    std::vector<LeaderboardEntry> sort_leaderboard(std::vector<LeaderboardEntry> entries) const;

private:
    std::unordered_map<CardId, Card>    cards_;
    std::unordered_map<MonsterId, Monster> monsters_;
    std::unordered_map<EventId, Event>   events_;

    int rarity_order(const std::string& rarity) const;
    std::string resolve_data_path(const std::string& base, const std::string& filename) const;
};

} // namespace DataLayer
