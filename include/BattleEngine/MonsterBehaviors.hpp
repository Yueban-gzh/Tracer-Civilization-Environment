/**
 * 怪物行为：根据怪物 ID 与当前回合数决定意图并执行行动（含上 debuff 等，需写代码实现）
 * 维护点：本目录下 MonsterBehaviors.cpp
 */
#pragma once

#include "../Common/Types.hpp"
#include <string>

namespace tce {

class EffectContext;

enum class MonsterIntentKind {
    Attack,   // 攻击 X 点
    Block,    // 防御（不显示数值）
    Unknown,  // 不明
    Buff,     // 强化
    Debuff,   // 施加负面效果
    Sleep,    // 睡眠
    Stun,     // 晕眩
};

struct MonsterIntent {
    MonsterIntentKind kind = MonsterIntentKind::Unknown;
    int               value = 0;  // 仅 Attack 使用（显示 X），其它类型填 0 即可
};

/** 本回合该怪物显示的意图（用于 UI） */
MonsterIntent get_monster_intent(MonsterId id, int turn_number);

/** 执行该怪物本回合行动（伤害/格挡/给玩家上负面等，通过 ctx 调用） */
void execute_monster_behavior(MonsterId id, int turn_number, EffectContext& ctx, int monster_index);

} // namespace tce
