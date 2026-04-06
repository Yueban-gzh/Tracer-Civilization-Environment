#pragma once

#include "Common/RunRng.hpp"
#include "Common/Types.hpp"

#include <string>
#include <vector>

namespace tce {

/** 地图宝箱房：小 / 中 / 大（出现权重 3 : 2 : 1）。 */
enum class TreasureChestKind { Small, Medium, Large };

/** 用于从遗物池按稀有度抽取（与卡牌 common/uncommon/rare 概念对齐，数据为静态表）。 */
enum class TreasureRelicTier { Common, Uncommon, Rare };

/**
 * 一次宝箱结算结果（不含 UI）。与策划表一致：
 * - 遗物：每箱必出 1 个（池空时为空串）；稀有度按箱体概率 roll。
 * - 小型：遗物 75% 普通 / 25% 罕见 / 0% 稀有；金币 50% × 基数 25（±10%）。
 * - 中型：遗物 35% / 50% / 15%；金币 35% × 基数 50（±10%）。
 * - 大型：遗物 0% / 75% / 25%；金币 50% × 基数 75（±10%）。
 */
struct TreasureRoomOutcome {
    TreasureChestKind chest_kind = TreasureChestKind::Small;
    TreasureRelicTier relic_tier = TreasureRelicTier::Common;
    bool grants_gold = false;
    int  gold_amount = 0;
    RelicId relic_id;       // 池已空时为空串
};

TreasureRoomOutcome roll_and_resolve_treasure_room(RunRng& rng, const std::vector<RelicId>& owned_relics);

const char* treasure_chest_kind_label_cn(TreasureChestKind k);

} // namespace tce
