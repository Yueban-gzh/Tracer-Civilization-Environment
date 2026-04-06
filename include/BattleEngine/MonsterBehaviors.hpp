/**
 * 怪物行为：根据怪物 ID 与当前回合数决定意图并执行行动（含上 debuff 等，需写代码实现）
 * 维护点：本目录下 MonsterBehaviors.cpp
 */
#pragma once

#include "../BattleCoreRefactor/Types.hpp"
#include "../BattleCoreRefactor/BattleEngine.hpp"
#include <functional>
#include <string>
#include <vector>

namespace tce {

/**
 * 规划阶段产物：上一回合显示，下一回合执行。
 * action_code 用于描述本回合选中的具体动作（如 1=噬咬/2=生长）。
 */
struct MonsterTurnPlan {
    MonsterIntent intent{};
    int action_code = 0;
};

/** 可覆写的怪物行为钩子（代码实现可注册，后续也可接 JSON 提供器）。 */
struct MonsterBehaviorHooks {
    std::function<MonsterTurnPlan(MonsterId, int, const std::vector<StatusInstance>*)> plan_turn;
    std::function<void(MonsterId, int, const MonsterTurnPlan&, EffectContext&, int)> execute_turn;
};

/** 注册/反注册某个怪物的行为实现（注册后可覆盖默认行为）。 */
void register_monster_behavior(const MonsterId& id, MonsterBehaviorHooks hooks);
void unregister_monster_behavior(const MonsterId& id);

/** 本回合该怪物显示的意图（用于 UI）；monster_statuses 可选，用于需读历史的状态机 */
MonsterIntent get_monster_intent(MonsterId id, int turn_number, const std::vector<StatusInstance>* monster_statuses = nullptr);

/** 执行该怪物本回合行动（伤害/格挡/给玩家上负面等，通过 ctx 调用） */
void execute_monster_behavior(MonsterId id, int turn_number, EffectContext& ctx, int monster_index);

} // namespace tce
