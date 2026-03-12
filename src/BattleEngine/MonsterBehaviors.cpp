/**
 * 怪物行为实现：按怪物 ID + 当前回合数返回意图并执行行动（可在此写伤害、格挡、上 debuff 等）
 */
#include "../../include/BattleEngine/MonsterBehaviors.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

namespace {

MonsterIntent intent_unknown(MonsterId, int) { return MonsterIntent{MonsterIntentKind::Unknown, 0}; }
void behavior_unknown(MonsterId, int, EffectContext&, int) {}

// ---------- cultist：奇数回合攻击 5，偶数回合给自己施加 2 层“仪式” ----------
MonsterIntent intent_cultist(MonsterId, int turn_number) {
    if (turn_number % 2 == 1)
        return MonsterIntent{MonsterIntentKind::Attack, 5};
    return MonsterIntent{MonsterIntentKind::Buff, 0};
}
void behavior_cultist(MonsterId, int turn_number, EffectContext& ctx, int monster_index) {
    if (turn_number % 2 == 1) {
        int dmg = ctx.get_effective_damage_dealt_to_player(5, monster_index);
        ctx.deal_damage_to_player(dmg);
    } else {
        // 仪式本身是一个状态：在敌人回合结束时，每层为自身提供 1 点力量
        ctx.apply_status_to_monster(monster_index, "ritual", 2, -1);
    }
}

// ---------- 示例：某怪物给玩家上虚弱（仅作演示，意图 id 需在 DataLayer 登记显示名）----------
// std::string intent_debuffer(MonsterId, int turn_number) {
//     return (turn_number % 2 == 1) ? "ATK3" : "WEAK1";
// }
// void behavior_debuffer(MonsterId, int turn_number, EffectContext& ctx, int monster_index) {
//     if (turn_number % 2 == 1) {
//         int dmg = ctx.get_effective_damage_dealt_to_player(3, monster_index);
//         ctx.deal_damage_to_player(dmg);
//     } else {
//         ctx.apply_status_to_player("weak", 1, 2);
//     }
// }

} // namespace

MonsterIntent get_monster_intent(MonsterId id, int turn_number) {
    if (id == "cultist") return intent_cultist(id, turn_number);
    return intent_unknown(id, turn_number);
}

void execute_monster_behavior(MonsterId id, int turn_number, EffectContext& ctx, int monster_index) {
    if (id == "cultist") { behavior_cultist(id, turn_number, ctx, monster_index); return; }
    behavior_unknown(id, turn_number, ctx, monster_index);
}

} // namespace tce
