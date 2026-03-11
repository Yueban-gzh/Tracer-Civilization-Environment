/**
 * 卡牌效果实现与注册：strike、defend、bash 等
 */
#include "../../include/Effects/CardEffects.hpp"
#include "../../include/CardSystem/CardSystem.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

namespace {

// 愤怒：未升级 6 伤并在弃牌堆加入一张愤怒；升级 8 伤并加入一张愤怒
void effect_anger(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 8 : 6;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.generate_to_discard_pile("anger");
    }
}

// 打击：未升级 6 伤，升级 9 伤
void effect_strike(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 9 : 6;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 防御：未升级 5 格挡，升级 8 格挡
void effect_defend(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 8 : 5;
    int block = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
}

// 痛击：未升级 8 伤 + 2 层易伤（持续 2 回合），升级 10 伤 + 3 层易伤（持续 3 回合）；易伤层数即持续时间
void effect_bash(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 10 : 8;
        int vuln = is_upgraded ? 3 : 2;  // 层数 = 持续回合
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "vulnerable", vuln, vuln);
    }
}

} // namespace

void register_all_card_effects(CardSystem& card_system) {
    card_system.register_card_effect("anger", [](EffectContext& c) { effect_anger(c, false); });
    card_system.register_card_effect("anger+", [](EffectContext& c) { effect_anger(c, true); });
    card_system.register_card_effect("strike", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("strike+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("defend", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("defend+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("bash", [](EffectContext& c) { effect_bash(c, false); });
    card_system.register_card_effect("bash+", [](EffectContext& c) { effect_bash(c, true); });
    // 后续在此追加：card_system.register_card_effect("poison_stab", effect_poison_stab); 等
}

} // namespace tce
