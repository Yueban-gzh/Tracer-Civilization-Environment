/**
 * 卡牌效果实现与注册：strike、defend、bash 等
 */
#include "../../include/Effects/CardEffects.hpp"
#include "../../include/CardSystem/CardSystem.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

namespace {

void effect_strike(EffectContext& ctx) {
    if (ctx.target_monster_index >= 0) {
        int base = 6;  // 升级版可改为 9
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

void effect_defend(EffectContext& ctx) {
    int base = 5;
    int block = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
}

void effect_bash(EffectContext& ctx) {
    if (ctx.target_monster_index >= 0) {
        int base = 8;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "vulnerable", 2, 2);  // 2 层易伤，持续 2 回合
    }
}

} // namespace

void register_all_card_effects(CardSystem& card_system) {
    card_system.register_card_effect("strike", effect_strike);
    card_system.register_card_effect("strike+", effect_strike);  // 升级版可后续改为 9 伤
    card_system.register_card_effect("defend", effect_defend);
    card_system.register_card_effect("defend+", effect_defend);
    card_system.register_card_effect("bash", effect_bash);
    card_system.register_card_effect("bash+", effect_bash);
    // 后续在此追加：card_system.register_card_effect("poison_stab", effect_poison_stab); 等
}

} // namespace tce
