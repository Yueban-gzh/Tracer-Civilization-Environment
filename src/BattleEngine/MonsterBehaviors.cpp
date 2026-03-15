/**
 * 怪物行为实现：按怪物 ID + 当前回合数返回意图并执行行动（可在此写伤害、格挡、上 debuff 等）
 */
#include "../../include/BattleEngine/MonsterBehaviors.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

namespace {

MonsterIntent intent_unknown(MonsterId, int) { return MonsterIntent{MonsterIntentKind::Unknown, 0}; }  // 默认意图：未知
void behavior_unknown(MonsterId, int, EffectContext&, int) {}  // 默认行为：无操作

// ---------- red_louse（红色虱虫）：生长 25% / 噬咬 75%，无法连续三回合相同行动 ----------
// 行动：1=噬咬，2=生长；历史编码 red_louse_history.stacks：0 无，1/2 上一动，11/22 上两动相同，12/21 不同
static int get_red_louse_history(const std::vector<StatusInstance>* statuses) {
    if (!statuses) return 0;
    return BattleEngine::get_status_stacks(*statuses, "red_louse_history");
}
static int choose_red_louse_action(int history, int turn_number) {  // 1=噬咬，2=生长
    if (history == 11) return 2;   // 连两噬咬，必须生长
    if (history == 22) return 1;   // 连两生长，必须噬咬
    return ((turn_number * 31 + 7) % 100 < 25) ? 2 : 1;  // 25% 生长，75% 噬咬
}
static int red_louse_bite_damage(int turn_number) {  // 5-7 点，确定性
    return 5 + ((turn_number * 31 + 13) % 3);
}

MonsterIntent intent_red_louse(MonsterId, int turn_number, const std::vector<StatusInstance>* statuses) {
    int action = choose_red_louse_action(get_red_louse_history(statuses), turn_number);
    if (action == 2)
        return MonsterIntent{MonsterIntentKind::Buff, 3};   // 生长：+3 力量
    return MonsterIntent{MonsterIntentKind::Attack, red_louse_bite_damage(turn_number)};  // 噬咬：5-7
}
void behavior_red_louse(MonsterId, int turn_number, EffectContext& ctx, int monster_index) {
    int history = ctx.get_status_stacks_on_monster(monster_index, "red_louse_history");
    int action = choose_red_louse_action(history, turn_number);
    if (action == 2) {
        ctx.apply_status_to_monster(monster_index, "strength", 3, -1);  // 生长：+3 力量
    } else {
        ctx.from_attack = true;
        int base = red_louse_bite_damage(turn_number);
        int str = ctx.get_status_stacks_on_monster(monster_index, "strength");
        ctx.deal_damage_to_player(base + str);  // 噬咬：5-7 + 力量
    }
    int new_history = (history % 10) * 10 + action;  // 更新：上一动*10 + 本动
    ctx.set_monster_status_stacks(monster_index, "red_louse_history", new_history);
}

// ---------- green_louse（绿色虱虫）：吐网 25% / 噬咬 75%，无法连续三回合相同行动 ----------
// 行动：1=噬咬，2=吐网；历史编码 green_louse_history.stacks：0 无，1/2 上一动，11/22 上两动相同，12/21 不同
static int get_green_louse_history(const std::vector<StatusInstance>* statuses) {
    if (!statuses) return 0;
    return BattleEngine::get_status_stacks(*statuses, "green_louse_history");
}
static int choose_green_louse_action(int history, int turn_number) {  // 1=噬咬，2=吐网
    if (history == 11) return 2;   // 连两噬咬，必须吐网
    if (history == 22) return 1;   // 连两吐网，必须噬咬
    return ((turn_number * 37 + 11) % 100 < 25) ? 2 : 1;  // 25% 吐网，75% 噬咬（与 red_louse 不同种子）
}
static int green_louse_bite_damage(int turn_number) {  // 5-7 点，确定性
    return 5 + ((turn_number * 37 + 17) % 3);
}

MonsterIntent intent_green_louse(MonsterId, int turn_number, const std::vector<StatusInstance>* statuses) {
    int action = choose_green_louse_action(get_green_louse_history(statuses), turn_number);
    if (action == 2)
        return MonsterIntent{MonsterIntentKind::Debuff, 2};  // 吐网：2 层易伤
    return MonsterIntent{MonsterIntentKind::Attack, green_louse_bite_damage(turn_number)};  // 噬咬：5-7
}
void behavior_green_louse(MonsterId, int turn_number, EffectContext& ctx, int monster_index) {
    int history = ctx.get_status_stacks_on_monster(monster_index, "green_louse_history");
    int action = choose_green_louse_action(history, turn_number);
    if (action == 2) {
        ctx.apply_status_to_player("frail", 2, 2);  // 吐网：2 层脆弱，持续 2 回合
    } else {
        ctx.from_attack = true;
        int base = green_louse_bite_damage(turn_number);
        int str = ctx.get_status_stacks_on_monster(monster_index, "strength");
        ctx.deal_damage_to_player(base + str);  // 噬咬：5-7 + 力量
    }
    int new_history = (history % 10) * 10 + action;
    ctx.set_monster_status_stacks(monster_index, "green_louse_history", new_history);
}

// ---------- cultist（邪教徒）：奇数回合仪式，偶数回合攻击（参考杀戮尖塔：先仪式再打）----------
MonsterIntent intent_cultist(MonsterId, int turn_number, const std::vector<StatusInstance>* statuses) {
    if (turn_number % 2 == 1)
        return MonsterIntent{MonsterIntentKind::Buff, 0};         // 第 1、3、5… 回合：仪式（强化）
    int str = statuses ? BattleEngine::get_status_stacks(*statuses, "strength") : 0;
    return MonsterIntent{MonsterIntentKind::Attack, 5 + str};    // 第 2、4、6… 回合：5 + 力量
}
void behavior_cultist(MonsterId, int turn_number, EffectContext& ctx, int monster_index) {
    if (turn_number % 2 == 1) {
        ctx.apply_status_to_monster(monster_index, "ritual", 2, -1);  // 奇数回合：加 2 层仪式（-1=永久）
    } else {
        ctx.from_attack = true;  // 标记为攻击，易伤/虚弱等 modifier 会生效
        int base = 5;
        int str = ctx.get_status_stacks_on_monster(monster_index, "strength");
        ctx.deal_damage_to_player(base + str);  // 偶数回合：5 + 仪式累积的力量
    }
}

// ---------- fat_gremlin（胖地精）：每回合攻击 4 点伤害 + 给玩家挂 1 层虚弱（持续 2 回合）----------
MonsterIntent intent_fat_gremlin(MonsterId, int) {
    return MonsterIntent{MonsterIntentKind::Attack, 4};  // 唯一意图：攻击 4
}
void behavior_fat_gremlin(MonsterId, int, EffectContext& ctx, int) {
    ctx.from_attack = true;
    ctx.deal_damage_to_player(4);           // 对玩家造成 4 点伤害
    ctx.apply_status_to_player("weak", 1, 2);  // 给玩家挂 1 层虚弱，持续 2 回合
}

// ---------- red_slaver（红色奴隶主）：刺伤/刮伤/丢网 ----------
// 行动：1=刺伤(Stab)，2=刮伤(Scratch)第一下，3=刮伤第二下，4=丢网(Throw Net)
// 规则：第一回合必刺伤；之后每回合 25% 丢网，否则按 刮伤→刮伤→刺伤 循环
static int get_red_slaver_history(const std::vector<StatusInstance>* statuses) {
    if (!statuses) return 0;
    return BattleEngine::get_status_stacks(*statuses, "red_slaver_history");
}
static int choose_red_slaver_action(int history, int turn_number) {
    if (turn_number == 1) return 1;  // 第一回合必定刺伤
    int roll = (turn_number * 41 + 19) % 100;
    if (history == 4) return (roll < 25) ? 4 : 2;  // 丢网后：25% 再丢网，否则刮伤
    if (history == 1) return 2;   // 刺伤后→刮伤
    if (history == 2) return 3;   // 刮伤1后→刮伤2
    if (history == 3) return 1;   // 刮伤2后→刺伤
    return 2;  // 无历史：从刮伤开始
}
static int red_slaver_stab_damage() { return 13; }   // 刺伤：13
static int red_slaver_scratch_damage() { return 8; }  // 刮伤：8 + 1 层易伤
static int red_slaver_scratch_vuln() { return 1; }    // 刮伤：1 层易伤

MonsterIntent intent_red_slaver(MonsterId, int turn_number, const std::vector<StatusInstance>* statuses) {
    int action = choose_red_slaver_action(get_red_slaver_history(statuses), turn_number);
    if (action == 4)
        return MonsterIntent{MonsterIntentKind::Debuff, 1};  // 丢网：1 层缠身
    if (action == 2 || action == 3)
        return MonsterIntent{MonsterIntentKind::Attack, red_slaver_scratch_damage()};  // 刮伤：8 + 易伤
    return MonsterIntent{MonsterIntentKind::Attack, red_slaver_stab_damage()};  // 刺伤：13
}
void behavior_red_slaver(MonsterId, int turn_number, EffectContext& ctx, int monster_index) {
    int history = ctx.get_status_stacks_on_monster(monster_index, "red_slaver_history");
    int action = choose_red_slaver_action(history, turn_number);
    if (action == 4) {
        ctx.apply_status_to_player("entangle", 1, 1);  // 丢网：1 层缠身，持续 1 回合
    } else if (action == 2 || action == 3) {
        ctx.from_attack = true;
        int base = red_slaver_scratch_damage();
        int str = ctx.get_status_stacks_on_monster(monster_index, "strength");
        ctx.deal_damage_to_player(base + str);
        ctx.apply_status_to_player("vulnerable", red_slaver_scratch_vuln(), 2);  // 刮伤：易伤
    } else {
        ctx.from_attack = true;
        int base = red_slaver_stab_damage();
        int str = ctx.get_status_stacks_on_monster(monster_index, "strength");
        ctx.deal_damage_to_player(base + str);  // 刺伤
    }
    ctx.set_monster_status_stacks(monster_index, "red_slaver_history", action);
}

} // namespace

// 【路由】根据怪物 id 返回本回合意图（供 UI 显示）
MonsterIntent get_monster_intent(MonsterId id, int turn_number, const std::vector<StatusInstance>* monster_statuses) {
    if (id == "red_louse") return intent_red_louse(id, turn_number, monster_statuses);
    if (id == "green_louse") return intent_green_louse(id, turn_number, monster_statuses);
    if (id == "cultist") return intent_cultist(id, turn_number, monster_statuses);
    if (id == "fat_gremlin") return intent_fat_gremlin(id, turn_number);
    if (id == "red_slaver") return intent_red_slaver(id, turn_number, monster_statuses);
    return intent_unknown(id, turn_number);
}

// 【路由】根据怪物 id 执行本回合行动（伤害/格挡/上 debuff 等）
void execute_monster_behavior(MonsterId id, int turn_number, EffectContext& ctx, int monster_index) {
    if (id == "red_louse") { behavior_red_louse(id, turn_number, ctx, monster_index); return; }
    if (id == "green_louse") { behavior_green_louse(id, turn_number, ctx, monster_index); return; }
    if (id == "cultist") { behavior_cultist(id, turn_number, ctx, monster_index); return; }
    if (id == "fat_gremlin") { behavior_fat_gremlin(id, turn_number, ctx, monster_index); return; }
    if (id == "red_slaver") { behavior_red_slaver(id, turn_number, ctx, monster_index); return; }
    behavior_unknown(id, turn_number, ctx, monster_index);
}

} // namespace tce
