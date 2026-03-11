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

// 铁斩波：未升级 5 格挡 + 5 伤，升级 7 格挡 + 7 伤
void effect_iron_wave(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 7 : 5;
        int block = ctx.get_effective_block_for_player(base);
        ctx.add_block_to_player(block);
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 金刚臂：未升级 12 伤 + 2 层虚弱（持续 2 回合），升级 14 伤 + 3 层虚弱（持续 3 回合）
void effect_clothesline(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 14 : 12;
        int weak = is_upgraded ? 3 : 2; // 层数 = 持续回合
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "weak", weak, weak);
    }
}

// 顺劈斩：未升级对所有敌人 8 伤，升级 11 伤
void effect_cleave(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 11 : 8;
    ctx.deal_damage_to_all_monsters(base);
}

// 飞踢：未升级 5 伤，升级 8 伤；若目标处于易伤，则获得 1 能量并抽 1 张牌
void effect_dropkick(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 8 : 5;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        if (ctx.get_status_stacks_on_monster(ctx.target_monster_index, "vulnerable") > 0) {
            ctx.add_energy_to_player(1);
            ctx.draw_cards(1);
        }
    }
}

// 残杀：虚无；未升级 20 伤，升级 28 伤
void effect_carnage(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 28 : 20;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 双重打击：未升级对同一目标造成 5×2 伤害，升级 7×2
void effect_twin_strike(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 7 : 5;
        // 为避免多次消耗活力等“一次性增伤”，只计算一次有效伤害值，然后打两次
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        if (dmg <= 0) return;
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 全身撞击：未升级/升级都造成当前格挡值的伤害（升级仅降费）
void effect_body_slam(EffectContext& ctx, bool /*is_upgraded*/) {
    if (ctx.target_monster_index >= 0) {
        int block = ctx.get_player_block();
        if (block <= 0) return;
        int dmg = ctx.get_effective_damage_dealt_by_player(block, ctx.target_monster_index);
        if (dmg <= 0) return;
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 剑柄打击：未升级 9 伤 + 抽 1 张牌，升级 10 伤 + 抽 2 张牌
void effect_pommel_strike(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 10 : 9;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.draw_cards(is_upgraded ? 2 : 1);
    }
}

// 连续拳：未升级 2×4，升级 2×5，均为消耗
void effect_pummel(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int hits = is_upgraded ? 5 : 4;
        int base = 2;
        for (int i = 0; i < hits; ++i) {
            int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
            if (dmg <= 0) continue;
            ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        }
    }
}

// 重锤：未升级 32 伤，升级 42 伤
void effect_bludgeon(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 42 : 32;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
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
    card_system.register_card_effect("iron_wave", [](EffectContext& c) { effect_iron_wave(c, false); });
    card_system.register_card_effect("iron_wave+", [](EffectContext& c) { effect_iron_wave(c, true); });
    card_system.register_card_effect("clothesline", [](EffectContext& c) { effect_clothesline(c, false); });
    card_system.register_card_effect("clothesline+", [](EffectContext& c) { effect_clothesline(c, true); });
    card_system.register_card_effect("cleave", [](EffectContext& c) { effect_cleave(c, false); });
    card_system.register_card_effect("cleave+", [](EffectContext& c) { effect_cleave(c, true); });
    card_system.register_card_effect("dropkick", [](EffectContext& c) { effect_dropkick(c, false); });
    card_system.register_card_effect("dropkick+", [](EffectContext& c) { effect_dropkick(c, true); });
    card_system.register_card_effect("carnage", [](EffectContext& c) { effect_carnage(c, false); });
    card_system.register_card_effect("carnage+", [](EffectContext& c) { effect_carnage(c, true); });
    card_system.register_card_effect("twin_strike", [](EffectContext& c) { effect_twin_strike(c, false); });
    card_system.register_card_effect("twin_strike+", [](EffectContext& c) { effect_twin_strike(c, true); });
    card_system.register_card_effect("body_slam", [](EffectContext& c) { effect_body_slam(c, false); });
    card_system.register_card_effect("body_slam+", [](EffectContext& c) { effect_body_slam(c, true); });
    card_system.register_card_effect("pommel_strike", [](EffectContext& c) { effect_pommel_strike(c, false); });
    card_system.register_card_effect("pommel_strike+", [](EffectContext& c) { effect_pommel_strike(c, true); });
    card_system.register_card_effect("pummel", [](EffectContext& c) { effect_pummel(c, false); });
    card_system.register_card_effect("pummel+", [](EffectContext& c) { effect_pummel(c, true); });
    card_system.register_card_effect("bludgeon", [](EffectContext& c) { effect_bludgeon(c, false); });
    card_system.register_card_effect("bludgeon+", [](EffectContext& c) { effect_bludgeon(c, true); });
    card_system.register_card_effect("strike", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("strike+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("defend", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("defend+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("bash", [](EffectContext& c) { effect_bash(c, false); });
    card_system.register_card_effect("bash+", [](EffectContext& c) { effect_bash(c, true); });
    // 后续在此追加：card_system.register_card_effect("poison_stab", effect_poison_stab); 等
}

} // namespace tce
