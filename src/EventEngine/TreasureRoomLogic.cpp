#include "EventEngine/TreasureRoomLogic.hpp"

#include <algorithm>
#include <cmath>

namespace tce {

namespace {

bool roll_percent(RunRng& rng, int percent) {
    if (percent <= 0) return false;
    if (percent >= 100) return true;
    return rng.uniform_int(0, 99) < percent;
}

/** 基数 ±10% 的整数金币量，至少 1。 */
int roll_gold_around(RunRng& rng, int base) {
    const int lo = std::max(1, static_cast<int>(std::lround(base * 0.9)));
    const int hi = std::max(lo, static_cast<int>(std::lround(base * 1.1)));
    return rng.uniform_int(lo, hi);
}

TreasureChestKind roll_chest_kind(RunRng& rng) {
    // 出现权重 3 : 2 : 1，共 6
    const int r = rng.uniform_int(0, 5);
    if (r < 3) return TreasureChestKind::Small;
    if (r < 5) return TreasureChestKind::Medium;
    return TreasureChestKind::Large;
}

TreasureRelicTier roll_relic_tier(RunRng& rng, TreasureChestKind kind) {
    const int r = rng.uniform_int(0, 99);
    switch (kind) {
    case TreasureChestKind::Small:
        // 75% 普通 / 25% 罕见 / 0% 稀有
        if (r < 75) return TreasureRelicTier::Common;
        return TreasureRelicTier::Uncommon;
    case TreasureChestKind::Medium:
        // 35% / 50% / 15%
        if (r < 35) return TreasureRelicTier::Common;
        if (r < 85) return TreasureRelicTier::Uncommon;
        return TreasureRelicTier::Rare;
    case TreasureChestKind::Large:
    default:
        // 0% / 75% / 25%
        if (r < 75) return TreasureRelicTier::Uncommon;
        return TreasureRelicTier::Rare;
    }
}

const std::vector<RelicId>& pool_for_tier(TreasureRelicTier t) {
    static const std::vector<RelicId> common = {
        "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle", "clockwork_boots",
        "happy_flower", "lantern", "smooth_stone", "hand_drum", "toy_ornithopter", "ceramic_fish",
    };
    static const std::vector<RelicId> uncommon = {
        "red_skull", "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "pen_nib",
        "preparation_pack", "anchor",
    };
    static const std::vector<RelicId> rare = {
        "orichalcum", "art_of_war", "relic_strength_plus",
    };
    switch (t) {
    case TreasureRelicTier::Common: return common;
    case TreasureRelicTier::Uncommon: return uncommon;
    case TreasureRelicTier::Rare:
    default: return rare;
    }
}

RelicId pick_relic(RunRng& rng, TreasureRelicTier preferred, const std::vector<RelicId>& owned) {
    const auto not_owned = [&](const RelicId& id) {
        return std::find(owned.begin(), owned.end(), id) == owned.end();
    };

    std::vector<TreasureRelicTier> try_order;
    switch (preferred) {
    case TreasureRelicTier::Rare:
        try_order = { TreasureRelicTier::Rare, TreasureRelicTier::Uncommon, TreasureRelicTier::Common };
        break;
    case TreasureRelicTier::Uncommon:
        try_order = { TreasureRelicTier::Uncommon, TreasureRelicTier::Rare, TreasureRelicTier::Common };
        break;
    case TreasureRelicTier::Common:
    default:
        try_order = { TreasureRelicTier::Common, TreasureRelicTier::Uncommon, TreasureRelicTier::Rare };
        break;
    }

    for (TreasureRelicTier tier : try_order) {
        std::vector<RelicId> avail;
        for (const RelicId& id : pool_for_tier(tier)) {
            if (not_owned(id)) avail.push_back(id);
        }
        if (!avail.empty()) {
            const size_t i = rng.uniform_size(0, avail.size() - 1);
            return avail[i];
        }
    }
    return {};
}

int curse_chance_for(TreasureChestKind k) {
    switch (k) {
    case TreasureChestKind::Small: return 12;
    case TreasureChestKind::Medium: return 28;
    case TreasureChestKind::Large:
    default: return 42;
    }
}

} // namespace

TreasureRoomOutcome roll_and_resolve_treasure_room(RunRng& rng, const std::vector<RelicId>& owned_relics) {
    TreasureRoomOutcome out;
    out.chest_kind = roll_chest_kind(rng);

    switch (out.chest_kind) {
    case TreasureChestKind::Small:
        out.grants_gold = roll_percent(rng, 50);
        if (out.grants_gold) out.gold_amount = roll_gold_around(rng, 25);
        break;
    case TreasureChestKind::Medium:
        out.grants_gold = roll_percent(rng, 35);
        if (out.grants_gold) out.gold_amount = roll_gold_around(rng, 50);
        break;
    case TreasureChestKind::Large:
    default:
        out.grants_gold = roll_percent(rng, 50);
        if (out.grants_gold) out.gold_amount = roll_gold_around(rng, 75);
        break;
    }

    out.relic_tier = roll_relic_tier(rng, out.chest_kind);
    out.relic_id = pick_relic(rng, out.relic_tier, owned_relics);
    out.grants_curse = roll_percent(rng, curse_chance_for(out.chest_kind));
    return out;
}

const char* treasure_chest_kind_label_cn(TreasureChestKind k) {
    switch (k) {
    case TreasureChestKind::Small: return "小型宝箱";
    case TreasureChestKind::Medium: return "中型宝箱";
    case TreasureChestKind::Large:
    default: return "大型宝箱";
    }
}

} // namespace tce
