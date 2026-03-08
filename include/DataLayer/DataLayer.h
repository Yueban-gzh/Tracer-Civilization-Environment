/**
 * DataLayer 对外接口
 * 提供卡牌/怪物/事件表加载与按 id 查找（哈希）、奖励排序
 * 见 docs/设计与接口.md 第五节 E - DataLayer
 *
 * 课设数据结构对应：
 * - 查找：哈希表（unordered_map），key=id，按 key 查找平均 O(1)；
 * - 排序：对线性表（vector）做排序，用于奖励展示与排行榜。
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

    // ---------- 查找（哈希表）：按 id 作为 key 查找，平均时间复杂度 O(1)；id 不存在返回 nullptr ----------
    const Card*    get_card_by_id(const CardId& id) const;
    const Monster* get_monster_by_id(const MonsterId& id) const;
    const Event*   get_event_by_id(const EventId& id) const;

    // ---------- 排序：对卡牌 id 序列按稀有度关键字排序（common < uncommon < rare），用于战斗奖励展示 ----------
    std::vector<CardId> sort_cards_by_rarity(const std::vector<CardId>& card_ids) const;

    // ---------- 排序：对排行榜按 score 关键字降序排序（可选）----------
    struct LeaderboardEntry {
        std::string playerId;
        int         score = 0;
        int         time  = 0;
    };
    std::vector<LeaderboardEntry> sort_leaderboard(std::vector<LeaderboardEntry> entries) const;

private:
    // 哈希表存储：key 为 id（CardId/MonsterId/EventId），value 为整条记录；查找 O(1)
    std::unordered_map<CardId, Card>       cards_;
    std::unordered_map<MonsterId, Monster> monsters_;
    std::unordered_map<EventId, Event>     events_;

    int rarity_order(const std::string& rarity) const;
    std::string resolve_data_path(const std::string& base, const std::string& filename) const;
};

} // namespace DataLayer
