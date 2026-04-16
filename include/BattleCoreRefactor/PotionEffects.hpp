#pragma once

#include "Types.hpp"
#include "BattleState.hpp"

namespace tce {

class CardSystem;

/** target_monster_index: 需指定目标的灵液传入怪物下标；无需目标的传 -1。card_system: 抽牌等需卡牌系统时传入。 */
void apply_potion_effect(const PotionId& id, BattleState& state, int target_monster_index = -1, CardSystem* card_system = nullptr);

/** 灵液是否需要选择怪物目标（用于 UI 决定是否进入瞄准模式） */
bool potion_requires_monster_target(const PotionId& id);

} // namespace tce
