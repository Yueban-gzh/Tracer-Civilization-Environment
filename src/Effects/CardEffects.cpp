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

// 御血术：先失去 2 点生命，再造成 15/20 伤害
void effect_hemokinesis(EffectContext& ctx, bool is_upgraded) {
    // 先对玩家自身造成 2 点无视格挡伤害
    ctx.deal_damage_to_player_ignoring_block(2);
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 20 : 15;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 重刃：基础 14 伤，力量在本牌上 3/5 倍加成
void effect_heavy_blade(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        const int base = 14;
        // EffectContext 内部的公式本身会 +1×strength，这里额外叠加 (multiplier-1)×strength
        const int str = ctx.get_status_stacks_on_player("strength");
        const int extra_mult = is_upgraded ? 5 - 1 : 3 - 1;
        int effective_base = base + str * extra_mult;
        int dmg = ctx.get_effective_damage_dealt_by_player(effective_base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 闪电霹雳：对所有敌人造成 4/7 伤害，并给予 1 层易伤
void effect_thunderclap(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 7 : 4;
    // 先结算范围伤害（每个怪物单独算易伤/虚弱等）
    ctx.deal_damage_to_all_monsters(base);
    // 再对所有存活怪物施加 1 层易伤，持续 1 回合
    ctx.apply_status_to_all_monsters("vulnerable", 1, 1);
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

// 上勾拳：造成 13 伤害，并施加 1/2 层虚弱与易伤（层数即持续回合）
void effect_uppercut(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = 13;
        int debuff = is_upgraded ? 2 : 1;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "weak", debuff, debuff);
        ctx.apply_status_to_monster(ctx.target_monster_index, "vulnerable", debuff, debuff);
    }
}

// 耸肩无视：未升级 8 格挡并抽 1；升级 11 格挡并抽 1
void effect_shrug_it_off(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 11 : 8;
    int block = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
    ctx.draw_cards(1);
}

// 快斩：未升级 8 伤并抽 1；升级 12 伤并抽 1
void effect_quick_slash(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 12 : 8;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.draw_cards(1);
    }
}

// 切割：未升级 6 伤；升级 9 伤（0 费）
void effect_slice(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 9 : 6;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 后空翻：未升级 5 格挡并抽 2；升级 8 格挡并抽 2
void effect_backflip(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 8 : 5;
    int block = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
    ctx.draw_cards(2);
}

// 中和：0 费；未升级 3 伤并施加 1 层虚弱，升级 4 伤并施加 2 层虚弱
void effect_neutralize(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 4 : 3;
        int weak = is_upgraded ? 2 : 1;
        int dmg  = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "weak", weak, weak);
    }
}

// 突然一拳：未升级 7 伤并施加 1 层虚弱，升级 9 伤并施加 2 层虚弱
void effect_sucker_punch(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 9 : 7;
        int weak = is_upgraded ? 2 : 1;
        int dmg  = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "weak", weak, weak);
    }
}

// 带毒刺击：未升级 6 伤并施加 3 层中毒，升级 8 伤并施加 4 层中毒
void effect_poisoned_stab(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base   = is_upgraded ? 8 : 6;
        int poison = is_upgraded ? 4 : 3;
        int dmg    = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        ctx.apply_status_to_monster(ctx.target_monster_index, "poison", poison, poison);
    }
}

// 冲刺：获得 10/13 点格挡并造成 10/13 点伤害
void effect_dash(EffectContext& ctx, bool is_upgraded) {
    int base_block = is_upgraded ? 13 : 10;
    int block      = ctx.get_effective_block_for_player(base_block);
    ctx.add_block_to_player(block);
    if (ctx.target_monster_index >= 0) {
        int base_dmg = is_upgraded ? 13 : 10;
        int dmg      = ctx.get_effective_damage_dealt_by_player(base_dmg, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 猎杀者：造成 15/20 点伤害，下回合多抽 2 张牌（用 draw_up 状态实现）
void effect_predator(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 20 : 15;
        int dmg  = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
    ctx.apply_status_to_player("draw_up", 2, 1);
}

// 飞膝：造成 8/11 点伤害，下一回合获得 1 点能量（用 energy_up 状态实现）
void effect_flying_knee(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 11 : 8;
        int dmg  = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
    ctx.apply_status_to_player("energy_up", 1, 1);
}

// 足跟勾：未升级 5 伤；升级 8 伤；若目标已有虚弱，则获得 1 能量并抽 1
void effect_heel_hook(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 8 : 5;
        int dmg  = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
        if (ctx.get_status_stacks_on_monster(ctx.target_monster_index, "weak") > 0) {
            ctx.add_energy_to_player(1);
            ctx.draw_cards(1);
        }
    }
}

// 致命毒药：未升级给予 5 层中毒，升级给予 7 层中毒
void effect_deadly_poison(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int poison = is_upgraded ? 7 : 5;
        ctx.apply_status_to_monster(ctx.target_monster_index, "poison", poison, poison);
    }
}

// 偏折：未升级获得 4 点格挡，升级获得 7 点格挡（0 费）
void effect_deflect(EffectContext& ctx, bool is_upgraded) {
    int base   = is_upgraded ? 7 : 4;
    int block  = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
}

// 灼热攻击：未升级 12 伤，升级 16 伤（设计上可多级升级，这里实现成普通 1 次升级版）
void effect_searing_blow(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int base = is_upgraded ? 16 : 12;
        int dmg  = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 匕首雨：对所有敌人造成 4x2 / 6x2 伤害
void effect_dagger_spray(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 6 : 4;
    ctx.deal_damage_to_all_monsters(base);
    ctx.deal_damage_to_all_monsters(base);
}

// 岿然不动：获得 30/40 点格挡，消耗
void effect_impervious(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 40 : 30;
    int block = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
}

// 威吓：给予所有敌人 1/2 层虚弱，消耗
void effect_intimidate(EffectContext& ctx, bool is_upgraded) {
    int weak = is_upgraded ? 2 : 1;
    ctx.apply_status_to_all_monsters("weak", weak, weak);
}

// 盛怒：获得 2 点能量，消耗
void effect_seeing_red(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.add_energy_to_player(2);
}

// 放血：失去 3 点生命，获得 2/3 点能量
void effect_bloodletting(EffectContext& ctx, bool is_upgraded) {
    ctx.deal_damage_to_player_ignoring_block(3);
    ctx.add_energy_to_player(is_upgraded ? 3 : 2);
}

// 扫腿：对目标给予 2/3 层虚弱，获得 11/14 点格挡
void effect_leg_sweep(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int weak = is_upgraded ? 3 : 2;
        ctx.apply_status_to_monster(ctx.target_monster_index, "weak", weak, weak);
    }
    int block = ctx.get_effective_block_for_player(is_upgraded ? 14 : 11);
    ctx.add_block_to_player(block);
}

// 恐怖：对目标给予 99 层易伤，消耗
void effect_terror(EffectContext& ctx, bool /*is_upgraded*/) {
    if (ctx.target_monster_index >= 0)
        ctx.apply_status_to_monster(ctx.target_monster_index, "vulnerable", 99, 99);
}

// 震荡波：给予所有敌人 3/5 层虚弱和易伤，消耗
void effect_shockwave(EffectContext& ctx, bool is_upgraded) {
    int n = is_upgraded ? 5 : 3;
    ctx.apply_status_to_all_monsters("weak", n, n);
    ctx.apply_status_to_all_monsters("vulnerable", n, n);
}

// 幽灵铠甲：虚无，获得 10/13 点格挡
void effect_ghostly_armor(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 13 : 10);
    ctx.add_block_to_player(block);
}

// 巩固：将当前格挡翻倍
void effect_entrench(EffectContext& ctx, bool /*is_upgraded*/) {
    int block = ctx.get_player_block();
    if (block > 0) ctx.add_block_to_player(block);
}

// 祭品：失去 6 点生命，获得 2 能量，抽 3/5 张牌，消耗
void effect_offering(EffectContext& ctx, bool is_upgraded) {
    ctx.deal_damage_to_player_ignoring_block(6);
    ctx.add_energy_to_player(2);
    ctx.draw_cards(is_upgraded ? 5 : 3);
}

// 千穿百刺：对目标造成 3/4 点伤害 5 次
void effect_riddle_with_holes(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int base = is_upgraded ? 4 : 3;
    for (int i = 0; i < 5; ++i) {
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        if (dmg > 0) ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 狂野打击：造成 12/17 点伤害，将一张伤口放入抽牌堆
void effect_wild_strike(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 17 : 12, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
    ctx.generate_to_draw_pile("card_025");
}

// 无谋冲锋：造成 7/10 点伤害，将一张晕眩放入抽牌堆
void effect_reckless_charge(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 10 : 7, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
    ctx.generate_to_draw_pile("daze");
}

// 燔祭：对所有敌人造成 21/28 点伤害，将一张灼伤放入弃牌堆
void effect_immolate(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 28 : 21;
    ctx.deal_damage_to_all_monsters(base);
    ctx.generate_to_discard_pile("card_026");
}

// 背刺：造成 11/15 点伤害，固有，消耗
void effect_backstab(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index >= 0) {
        int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 15 : 11, ctx.target_monster_index);
        ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 死吧死吧死吧：对所有敌人造成 13/17 点伤害，消耗
void effect_die_die_die(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 17 : 13;
    ctx.deal_damage_to_all_monsters(base);
}

// 灾祸：造成 7/10 点伤害，若目标有中毒则再造成 7/10
void effect_bane(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int base = is_upgraded ? 10 : 7;
    int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    if (ctx.get_status_stacks_on_monster(ctx.target_monster_index, "poison") > 0) {
        int dmg2 = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        if (dmg2 > 0) ctx.deal_damage_to_monster(ctx.target_monster_index, dmg2);
    }
}

// 肾上腺素：获得 1/2 点能量，抽 2 张牌，消耗
void effect_adrenaline(EffectContext& ctx, bool is_upgraded) {
    ctx.add_energy_to_player(is_upgraded ? 2 : 1);
    ctx.draw_cards(2);
}

// 抢占先机：下一回合获得 2/3 点能量（energy_up）
void effect_outmaneuver(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("energy_up", is_upgraded ? 3 : 2, 1);
}

// 催化剂：将目标中毒层数翻倍/三倍，消耗
void effect_catalyst(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int p = ctx.get_status_stacks_on_monster(ctx.target_monster_index, "poison");
    if (p <= 0) return;
    int add = is_upgraded ? p * 2 : p;  // 翻倍：+p；三倍：+2p
    ctx.apply_status_to_monster(ctx.target_monster_index, "poison", add, add);
}

// 致残毒云：给予所有敌人 4/7 层中毒和 2 层虚弱，消耗
void effect_crippling_poison(EffectContext& ctx, bool is_upgraded) {
    int poison = is_upgraded ? 7 : 4;
    ctx.apply_status_to_all_monsters("poison", poison, poison);
    ctx.apply_status_to_all_monsters("weak", 2, 2);
}

// 燃烧：获得 2/3 点力量（永久，duration -1）
void effect_inflame(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("strength", is_upgraded ? 3 : 2, -1);
}

// 战斗专注：抽 3/4 张牌（本回合禁抽用状态实现时可在此加 draw_reduction）
void effect_battle_trance(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 4 : 3);
}

// 金属化：回合结束时获得 3/4 点格挡（metallicize 状态）
void effect_metallicize(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("metallicize", is_upgraded ? 4 : 3, -1);
}

// 灵动步法：获得 2/3 点敏捷
void effect_footwork(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("dexterity", is_upgraded ? 3 : 2, -1);
}

// 闪躲翻滚：获得 4/6 格挡，下回合获得等量格挡（block_up 在回合开始时加格挡并消耗）
void effect_dodge_and_roll(EffectContext& ctx, bool is_upgraded) {
    int n = is_upgraded ? 6 : 4;
    int block = ctx.get_effective_block_for_player(n);
    ctx.add_block_to_player(block);
    ctx.apply_status_to_player("block_up", n, -1);
}

// 硬撑：获得 15/20 格挡，将 2 张伤口加入手牌（手牌满则入弃牌堆）
void effect_power_through(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 20 : 15);
    ctx.add_block_to_player(block);
    ctx.generate_to_hand("card_025");
    ctx.generate_to_hand("card_025");
}

// 小刀：造成 4/6 点伤害，消耗
void effect_shiv(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int base = is_upgraded ? 6 : 4;
    int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
}

// 斗篷与匕首：获得 6 点格挡，增加 1/2 张小刀到手牌
void effect_cloak_and_dagger(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(6);
    ctx.add_block_to_player(block);
    ctx.generate_to_hand("shiv");
    if (is_upgraded) ctx.generate_to_hand("shiv");
}

// 刀刃之舞：增加 3/4 张小刀到手牌
void effect_blade_dance(EffectContext& ctx, bool is_upgraded) {
    int n = is_upgraded ? 4 : 3;
    for (int i = 0; i < n; ++i) ctx.generate_to_hand("shiv");
}

// 突破极限：将当前力量翻倍（再施加等量力量，永久）
void effect_limit_break(EffectContext& ctx, bool /*is_upgraded*/) {
    int s = ctx.get_status_stacks_on_player("strength");
    if (s > 0) ctx.apply_status_to_player("strength", s, -1);
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
    card_system.register_card_effect("heavy_blade", [](EffectContext& c) { effect_heavy_blade(c, false); });
    card_system.register_card_effect("heavy_blade+", [](EffectContext& c) { effect_heavy_blade(c, true); });
    card_system.register_card_effect("hemokinesis", [](EffectContext& c) { effect_hemokinesis(c, false); });
    card_system.register_card_effect("hemokinesis+", [](EffectContext& c) { effect_hemokinesis(c, true); });
    card_system.register_card_effect("thunderclap", [](EffectContext& c) { effect_thunderclap(c, false); });
    card_system.register_card_effect("thunderclap+", [](EffectContext& c) { effect_thunderclap(c, true); });
    card_system.register_card_effect("bludgeon", [](EffectContext& c) { effect_bludgeon(c, false); });
    card_system.register_card_effect("bludgeon+", [](EffectContext& c) { effect_bludgeon(c, true); });
    card_system.register_card_effect("strike", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("strike+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("defend", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("defend+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("bash", [](EffectContext& c) { effect_bash(c, false); });
    card_system.register_card_effect("bash+", [](EffectContext& c) { effect_bash(c, true); });
    card_system.register_card_effect("uppercut", [](EffectContext& c) { effect_uppercut(c, false); });
    card_system.register_card_effect("uppercut+", [](EffectContext& c) { effect_uppercut(c, true); });
    card_system.register_card_effect("shrug_it_off", [](EffectContext& c) { effect_shrug_it_off(c, false); });
    card_system.register_card_effect("shrug_it_off+", [](EffectContext& c) { effect_shrug_it_off(c, true); });
    card_system.register_card_effect("quick_slash", [](EffectContext& c) { effect_quick_slash(c, false); });
    card_system.register_card_effect("quick_slash+", [](EffectContext& c) { effect_quick_slash(c, true); });
    card_system.register_card_effect("slice", [](EffectContext& c) { effect_slice(c, false); });
    card_system.register_card_effect("slice+", [](EffectContext& c) { effect_slice(c, true); });
    card_system.register_card_effect("backflip", [](EffectContext& c) { effect_backflip(c, false); });
    card_system.register_card_effect("backflip+", [](EffectContext& c) { effect_backflip(c, true); });
    card_system.register_card_effect("neutralize", [](EffectContext& c) { effect_neutralize(c, false); });
    card_system.register_card_effect("neutralize+", [](EffectContext& c) { effect_neutralize(c, true); });
    card_system.register_card_effect("sucker_punch", [](EffectContext& c) { effect_sucker_punch(c, false); });
    card_system.register_card_effect("sucker_punch+", [](EffectContext& c) { effect_sucker_punch(c, true); });
    card_system.register_card_effect("poisoned_stab", [](EffectContext& c) { effect_poisoned_stab(c, false); });
    card_system.register_card_effect("poisoned_stab+", [](EffectContext& c) { effect_poisoned_stab(c, true); });
    card_system.register_card_effect("heel_hook", [](EffectContext& c) { effect_heel_hook(c, false); });
    card_system.register_card_effect("heel_hook+", [](EffectContext& c) { effect_heel_hook(c, true); });
    card_system.register_card_effect("dash", [](EffectContext& c) { effect_dash(c, false); });
    card_system.register_card_effect("dash+", [](EffectContext& c) { effect_dash(c, true); });
    card_system.register_card_effect("predator", [](EffectContext& c) { effect_predator(c, false); });
    card_system.register_card_effect("predator+", [](EffectContext& c) { effect_predator(c, true); });
    card_system.register_card_effect("flying_knee", [](EffectContext& c) { effect_flying_knee(c, false); });
    card_system.register_card_effect("flying_knee+", [](EffectContext& c) { effect_flying_knee(c, true); });
    card_system.register_card_effect("deadly_poison", [](EffectContext& c) { effect_deadly_poison(c, false); });
    card_system.register_card_effect("deadly_poison+", [](EffectContext& c) { effect_deadly_poison(c, true); });
    card_system.register_card_effect("deflect", [](EffectContext& c) { effect_deflect(c, false); });
    card_system.register_card_effect("deflect+", [](EffectContext& c) { effect_deflect(c, true); });
    // 绿色防御：复用 defend 效果
    card_system.register_card_effect("defend_green", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("defend_green+", [](EffectContext& c) { effect_defend(c, true); });
    // 绿色打击与防御：复用 strike/defend 效果
    card_system.register_card_effect("strike_green", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("strike_green+", [](EffectContext& c) { effect_strike(c, true); });
    // 灼热攻击 / 匕首雨
    card_system.register_card_effect("searing_blow", [](EffectContext& c) { effect_searing_blow(c, false); });
    card_system.register_card_effect("searing_blow+", [](EffectContext& c) { effect_searing_blow(c, true); });
    card_system.register_card_effect("dagger_spray", [](EffectContext& c) { effect_dagger_spray(c, false); });
    card_system.register_card_effect("dagger_spray+", [](EffectContext& c) { effect_dagger_spray(c, true); });
    card_system.register_card_effect("impervious", [](EffectContext& c) { effect_impervious(c, false); });
    card_system.register_card_effect("impervious+", [](EffectContext& c) { effect_impervious(c, true); });
    card_system.register_card_effect("intimidate", [](EffectContext& c) { effect_intimidate(c, false); });
    card_system.register_card_effect("intimidate+", [](EffectContext& c) { effect_intimidate(c, true); });
    card_system.register_card_effect("seeing_red", [](EffectContext& c) { effect_seeing_red(c, false); });
    card_system.register_card_effect("seeing_red+", [](EffectContext& c) { effect_seeing_red(c, true); });
    card_system.register_card_effect("bloodletting", [](EffectContext& c) { effect_bloodletting(c, false); });
    card_system.register_card_effect("bloodletting+", [](EffectContext& c) { effect_bloodletting(c, true); });
    card_system.register_card_effect("leg_sweep", [](EffectContext& c) { effect_leg_sweep(c, false); });
    card_system.register_card_effect("leg_sweep+", [](EffectContext& c) { effect_leg_sweep(c, true); });
    card_system.register_card_effect("terror", [](EffectContext& c) { effect_terror(c, false); });
    card_system.register_card_effect("terror+", [](EffectContext& c) { effect_terror(c, true); });
    card_system.register_card_effect("shockwave", [](EffectContext& c) { effect_shockwave(c, false); });
    card_system.register_card_effect("shockwave+", [](EffectContext& c) { effect_shockwave(c, true); });
    card_system.register_card_effect("ghostly_armor", [](EffectContext& c) { effect_ghostly_armor(c, false); });
    card_system.register_card_effect("ghostly_armor+", [](EffectContext& c) { effect_ghostly_armor(c, true); });
    card_system.register_card_effect("entrench", [](EffectContext& c) { effect_entrench(c, false); });
    card_system.register_card_effect("entrench+", [](EffectContext& c) { effect_entrench(c, true); });
    card_system.register_card_effect("offering", [](EffectContext& c) { effect_offering(c, false); });
    card_system.register_card_effect("offering+", [](EffectContext& c) { effect_offering(c, true); });
    card_system.register_card_effect("riddle_with_holes", [](EffectContext& c) { effect_riddle_with_holes(c, false); });
    card_system.register_card_effect("riddle_with_holes+", [](EffectContext& c) { effect_riddle_with_holes(c, true); });
    card_system.register_card_effect("wild_strike", [](EffectContext& c) { effect_wild_strike(c, false); });
    card_system.register_card_effect("wild_strike+", [](EffectContext& c) { effect_wild_strike(c, true); });
    card_system.register_card_effect("reckless_charge", [](EffectContext& c) { effect_reckless_charge(c, false); });
    card_system.register_card_effect("reckless_charge+", [](EffectContext& c) { effect_reckless_charge(c, true); });
    card_system.register_card_effect("immolate", [](EffectContext& c) { effect_immolate(c, false); });
    card_system.register_card_effect("immolate+", [](EffectContext& c) { effect_immolate(c, true); });
    card_system.register_card_effect("backstab", [](EffectContext& c) { effect_backstab(c, false); });
    card_system.register_card_effect("backstab+", [](EffectContext& c) { effect_backstab(c, true); });
    card_system.register_card_effect("die_die_die", [](EffectContext& c) { effect_die_die_die(c, false); });
    card_system.register_card_effect("die_die_die+", [](EffectContext& c) { effect_die_die_die(c, true); });
    card_system.register_card_effect("bane", [](EffectContext& c) { effect_bane(c, false); });
    card_system.register_card_effect("bane+", [](EffectContext& c) { effect_bane(c, true); });
    card_system.register_card_effect("adrenaline", [](EffectContext& c) { effect_adrenaline(c, false); });
    card_system.register_card_effect("adrenaline+", [](EffectContext& c) { effect_adrenaline(c, true); });
    card_system.register_card_effect("outmaneuver", [](EffectContext& c) { effect_outmaneuver(c, false); });
    card_system.register_card_effect("outmaneuver+", [](EffectContext& c) { effect_outmaneuver(c, true); });
    card_system.register_card_effect("catalyst", [](EffectContext& c) { effect_catalyst(c, false); });
    card_system.register_card_effect("catalyst+", [](EffectContext& c) { effect_catalyst(c, true); });
    card_system.register_card_effect("crippling_poison", [](EffectContext& c) { effect_crippling_poison(c, false); });
    card_system.register_card_effect("crippling_poison+", [](EffectContext& c) { effect_crippling_poison(c, true); });
    card_system.register_card_effect("inflame", [](EffectContext& c) { effect_inflame(c, false); });
    card_system.register_card_effect("inflame+", [](EffectContext& c) { effect_inflame(c, true); });
    card_system.register_card_effect("battle_trance", [](EffectContext& c) { effect_battle_trance(c, false); });
    card_system.register_card_effect("battle_trance+", [](EffectContext& c) { effect_battle_trance(c, true); });
    card_system.register_card_effect("metallicize", [](EffectContext& c) { effect_metallicize(c, false); });
    card_system.register_card_effect("metallicize+", [](EffectContext& c) { effect_metallicize(c, true); });
    card_system.register_card_effect("footwork", [](EffectContext& c) { effect_footwork(c, false); });
    card_system.register_card_effect("footwork+", [](EffectContext& c) { effect_footwork(c, true); });
    card_system.register_card_effect("dodge_and_roll", [](EffectContext& c) { effect_dodge_and_roll(c, false); });
    card_system.register_card_effect("dodge_and_roll+", [](EffectContext& c) { effect_dodge_and_roll(c, true); });
    card_system.register_card_effect("power_through", [](EffectContext& c) { effect_power_through(c, false); });
    card_system.register_card_effect("power_through+", [](EffectContext& c) { effect_power_through(c, true); });
    card_system.register_card_effect("shiv", [](EffectContext& c) { effect_shiv(c, false); });
    card_system.register_card_effect("shiv+", [](EffectContext& c) { effect_shiv(c, true); });
    card_system.register_card_effect("cloak_and_dagger", [](EffectContext& c) { effect_cloak_and_dagger(c, false); });
    card_system.register_card_effect("cloak_and_dagger+", [](EffectContext& c) { effect_cloak_and_dagger(c, true); });
    card_system.register_card_effect("blade_dance", [](EffectContext& c) { effect_blade_dance(c, false); });
    card_system.register_card_effect("blade_dance+", [](EffectContext& c) { effect_blade_dance(c, true); });
    card_system.register_card_effect("limit_break", [](EffectContext& c) { effect_limit_break(c, false); });
    card_system.register_card_effect("limit_break+", [](EffectContext& c) { effect_limit_break(c, true); });
    // cards.json 诗词卡：与子同袍/大风起兮等 Attack 用打击效果，雨雪霏霏等 Skill 用防御效果
    card_system.register_card_effect("card_001", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_001+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_002", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_002+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_003", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_003+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_004", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_004+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_005", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_005+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_006", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_006+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_007", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_007+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_008", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_008+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_009", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_009+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_010", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_010+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_011", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_011+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_012", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_012+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_013", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_013+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_014", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_014+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_016", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_016+", [](EffectContext& c) { effect_defend(c, true); });
    card_system.register_card_effect("card_017", [](EffectContext& c) { effect_strike(c, false); });
    card_system.register_card_effect("card_017+", [](EffectContext& c) { effect_strike(c, true); });
    card_system.register_card_effect("card_018", [](EffectContext& c) { effect_defend(c, false); });
    card_system.register_card_effect("card_018+", [](EffectContext& c) { effect_defend(c, true); });
    // Power 卡（card_015, card_019-card_024）：可打出，暂无持久效果实现
    auto effect_power_noop = [](EffectContext&) {};
    card_system.register_card_effect("card_015", effect_power_noop);
    card_system.register_card_effect("card_015+", effect_power_noop);
    card_system.register_card_effect("card_019", effect_power_noop);
    card_system.register_card_effect("card_019+", effect_power_noop);
    card_system.register_card_effect("card_020", effect_power_noop);
    card_system.register_card_effect("card_020+", effect_power_noop);
    card_system.register_card_effect("card_021", effect_power_noop);
    card_system.register_card_effect("card_021+", effect_power_noop);
    card_system.register_card_effect("card_022", effect_power_noop);
    card_system.register_card_effect("card_022+", effect_power_noop);
    card_system.register_card_effect("card_023", effect_power_noop);
    card_system.register_card_effect("card_023+", effect_power_noop);
    card_system.register_card_effect("card_024", effect_power_noop);
    card_system.register_card_effect("card_024+", effect_power_noop);
    // 后续在此追加：card_system.register_card_effect("poison_stab", effect_poison_stab); 等
}

} // namespace tce
