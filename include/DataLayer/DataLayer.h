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
#include "DataLayer/DataLayer.hpp"
#include "../Common/NodeTypes.hpp"
#include "../Common/RunRng.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace DataLayer {

struct RootEventCandidate {
    EventId id;
    int     weight = 1;
};

/** 一页地图（与 MapConfig 索引 0..2 对应）：普通/精英各 9 组遭遇，Boss 一组。 */
struct EncounterPage {
    std::vector<std::vector<tce::MonsterId>> enemy_groups;
    std::vector<std::vector<tce::MonsterId>> elite_groups;
    std::vector<tce::MonsterId>              boss;
};

class DataLayerImpl {
public:
    DataLayerImpl() = default;

    // ---------- 加载（主流程启动时各调一次，或首次 get 时懒加载）----------
    bool load_cards(const std::string& path_or_base_dir);
    bool load_monsters(const std::string& path_or_base_dir);
    bool load_events(const std::string& path_or_base_dir);
    /** 加载 data/encounters.json：三页地图各 9 组普通怪、9 组精英、1 组 Boss（怪物 id 与怪物表一致）。 */
    bool load_encounters(const std::string& path_or_base_dir);

    // ---------- 查找（哈希表）：按 id 作为 key 查找，平均 O(1)；卡牌/怪物统一用 tce 类型，与 B/C 一致 ----------
    const tce::CardData*    get_card_by_id(const CardId& id) const;
    const tce::MonsterData* get_monster_by_id(const MonsterId& id) const;
    const Event*            get_event_by_id(const EventId& id) const;
    /** 问号房用：返回“根事件 id”集合（如 event_001），用于从事件树的起点随机抽取。 */
    std::vector<EventId>   get_root_event_ids() const;

    /** 问号房加权抽取：返回满足 layer 范围的“根事件候选 + 权重”；若无任何事件覆盖该层则退化为全部根事件。 */
    std::vector<RootEventCandidate> get_root_event_candidates_for_layer(int layer) const;

    // ---------- 排序：对卡牌 id 序列按稀有度关键字排序（common < uncommon < rare），用于战斗奖励展示 ----------
    std::vector<CardId> sort_cards_by_rarity(const std::vector<CardId>& card_ids) const;

    // ---------- 排序：对排行榜按 score 关键字降序排序（可选）----------
    struct LeaderboardEntry {
        std::string playerId;
        int         score = 0;
        int         time  = 0;
    };
    std::vector<LeaderboardEntry> sort_leaderboard(std::vector<LeaderboardEntry> entries) const;

    /** 按当前地图页（0=第一张…）与节点类型，从遭遇表随机一组怪物 id（Boss 不随机）。 */
    std::vector<tce::MonsterId> roll_monsters_for_battle(int map_page_index, NodeType node_type, tce::RunRng& rng) const;

private:
    std::unordered_map<EventId, Event> events_;
    std::vector<EncounterPage>         encounter_pages_;

    static int rarity_order(tce::Rarity r);
    std::string resolve_data_path(const std::string& base, const std::string& filename) const;
};

} // namespace DataLayer
