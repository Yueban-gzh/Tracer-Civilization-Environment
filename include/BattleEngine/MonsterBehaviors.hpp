/**
 * 怪物行为：根据怪物 ID 与当前回合数决定意图并执行行动（含上 debuff 等，需写代码实现）
 * 维护点：本目录下 MonsterBehaviors.cpp
 */
#pragma once

#include "../BattleCoreRefactor/Types.hpp"
#include "../BattleCoreRefactor/BattleEngine.hpp"
#include <string>
#include <vector>

namespace tce {

/** 本回合该怪物显示的意图（用于 UI）；monster_statuses 可选，用于需历史的状态（如 red_louse） */
MonsterIntent get_monster_intent(MonsterId id, int turn_number, const std::vector<StatusInstance>* monster_statuses = nullptr);

/** 执行该怪物本回合行动（伤害/格挡/给玩家上负面等，通过 ctx 调用） */
void execute_monster_behavior(MonsterId id, int turn_number, EffectContext& ctx, int monster_index);

} // namespace tce
