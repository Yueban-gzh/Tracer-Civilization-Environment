/**
 * 卡牌效果实现与注册：strike、defend、bash 等
 */
#include "../../include/Effects/CardEffects.hpp"
#include "../../include/CardSystem/CardSystem.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"
#include <algorithm>
#include <vector>

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
    if (ctx.get_status_stacks_on_player("cannot_block") > 0) return;  // 无法格挡：不通过打牌增加格挡（不经 get_effective 链）
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

// 武装：获得 5 点格挡；未升级由玩家选手牌升级 1 张（无预选则随机 1 张），升级版升级手牌全部
void effect_armaments(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(5);
    ctx.add_block_to_player(block);
    if (is_upgraded) ctx.upgrade_all_cards_in_hand();
    else {
        int u = ctx.upgrade_selected_cards_in_hand(1);
        if (u <= 0) ctx.upgrade_random_cards_in_hand(1);
    }
}

// 发掘：将玩家所选消耗堆中的一张牌移入手牌（预选由 UI 注入）；消耗堆无匹配则无事发生
void effect_exhume(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.add_exhaust_selected_to_hand(1);
}

// 燃烧契约：随机消耗 1 张手牌；抽 2/3 张牌
void effect_burning_pact(EffectContext& ctx, bool is_upgraded) {
    int exhausted = ctx.exhaust_selected_hand_cards(1);
    if (exhausted <= 0) exhausted = ctx.exhaust_random_hand_cards(1);
    if (exhausted <= 0) return;
    ctx.draw_cards(is_upgraded ? 3 : 2);
}

// 坚毅：获得 7/9 点格挡；未升级随机消耗 1 张手牌，升级版由玩家选择 1 张消耗（UI 注入预选）
void effect_true_grit(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 9 : 7);
    ctx.add_block_to_player(block);
    if (is_upgraded) {
        int exhausted = ctx.exhaust_selected_hand_cards(1);
        if (exhausted <= 0) ctx.exhaust_random_hand_cards(1);
    } else {
        ctx.exhaust_random_hand_cards(1);
    }
}

// 生存者：获得 8/11 点格挡；随机弃掉 1 张手牌
void effect_survivor(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 11 : 8);
    ctx.add_block_to_player(block);
    int discarded = ctx.discard_selected_hand_cards(1);
    if (discarded <= 0) ctx.discard_random_hand_cards(1);
}

// 杂技：抽 3/4 张牌，然后随机弃掉 1 张手牌
void effect_acrobatics(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 4 : 3);
    int discarded = ctx.discard_selected_hand_cards(1);
    if (discarded <= 0) ctx.discard_random_hand_cards(1);
}

// 火焰屏障：获得 12/16 点格挡，并获得本回合反伤 4/6（flame_barrier）
void effect_flame_barrier(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 16 : 12);
    ctx.add_block_to_player(block);
    ctx.apply_status_to_player("flame_barrier", is_upgraded ? 6 : 4, 1);
}

// 重振精神：消耗手牌中所有非攻击牌；每张获得 5/7 点格挡
void effect_second_wind(EffectContext& ctx, bool is_upgraded) {
    int exhausted = ctx.exhaust_non_attack_hand_cards();
    if (exhausted <= 0) return;
    int base_total = exhausted * (is_upgraded ? 7 : 5);
    int block = ctx.get_effective_block_for_player(base_total);
    ctx.add_block_to_player(block);
}

// 看破弱点：选择一名敌人；若该敌人意图攻击，则获得 3/4 点力量
void effect_spot_weakness(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    if (!ctx.target_monster_intends_attack()) return;
    ctx.apply_status_to_player("strength", is_upgraded ? 4 : 3, -1);
}

// 噬咬：造成 7/8 伤害，回复 2/3 生命
void effect_bite(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int base = is_upgraded ? 8 : 7;
    int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    ctx.heal_player(is_upgraded ? 3 : 2);
}

// 亮剑：造成 3/6 伤害，抽 1 张牌
void effect_flash_of_steel(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int base = is_upgraded ? 6 : 3;
    int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    ctx.draw_cards(1);
}

// 妙计：获得 2/4 格挡，抽 1 张牌
void effect_finesse(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 4 : 2);
    ctx.add_block_to_player(block);
    ctx.draw_cards(1);
}

// 缴械：目标本场战斗永久失去 2/3 点力量
void effect_disarm(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int n = is_upgraded ? 3 : 2;
    ctx.apply_status_to_monster(ctx.target_monster_index, "strength_down", n, -1);
}

// 黑暗镣铐：目标本回合失去 9/15 点力量
void effect_dark_shackles(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int n = is_upgraded ? 15 : 9;
    ctx.apply_status_to_monster(ctx.target_monster_index, "strength_down", n, 1);
}

// 万能药：获得 1/2 层人工制品
void effect_panacea(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("artifact", is_upgraded ? 2 : 1, -1);
}

// 预谋：抽 1/2，随后随机弃 1/2（当前 UI 未支持手选弃牌）
void effect_prepared(EffectContext& ctx, bool is_upgraded) {
    int n = is_upgraded ? 2 : 1;
    ctx.draw_cards(n);
    int discarded = ctx.discard_selected_hand_cards(n);
    if (discarded < n) ctx.discard_random_hand_cards(n - discarded);
}

// 全力攻击：对所有敌人造成 10/14 伤害，随后随机弃 1 张手牌
void effect_all_out_attack(EffectContext& ctx, bool is_upgraded) {
    ctx.deal_damage_to_all_monsters(is_upgraded ? 14 : 10);
    int discarded = ctx.discard_selected_hand_cards(1);
    if (discarded <= 0) ctx.discard_random_hand_cards(1);
}

// 计算下注：弃掉全部手牌，然后抽等量张牌（消耗由 cards.json 字段控制）
void effect_calculated_gamble(EffectContext& ctx, bool /*is_upgraded*/) {
    int n = ctx.discard_all_hand_cards();
    if (n > 0) ctx.draw_cards(n);
}

// 全神贯注：随机弃 3/2 张牌，若弃牌数量满足则获得 2 点能量
void effect_concentrate(EffectContext& ctx, bool is_upgraded) {
    int need = is_upgraded ? 2 : 3;
    int discarded = ctx.discard_selected_hand_cards(need);
    if (discarded < need) discarded += ctx.discard_random_hand_cards(need - discarded);
    if (discarded >= need) ctx.add_energy_to_player(2);
}

// 尖啸：所有敌人失去 6/8 点力量，持续 1 回合（通过 strength_down 表示）
void effect_piercing_wail(EffectContext& ctx, bool is_upgraded) {
    int n = is_upgraded ? 8 : 6;
    ctx.apply_status_to_all_monsters("strength_down", n, 1);
}

// 隐秘打击：造成 12/16 伤害；若本回合弃过牌，获得 2 点能量
void effect_sneaky_strike(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 16 : 12, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    if (ctx.get_status_stacks_on_player("discarded_this_turn") > 0)
        ctx.add_energy_to_player(2);
}

// 逃脱计划：抽 1 张；若抽到技能牌，获得有效格挡 3/5
void effect_escape_plan(EffectContext& ctx, bool is_upgraded) {
    const int before = ctx.get_hand_card_count();
    ctx.draw_cards(1);
    if (ctx.get_hand_card_count() <= before) return;
    if (ctx.was_last_hand_card_skill()) {
        int block = ctx.get_effective_block_for_player(is_upgraded ? 5 : 3);
        ctx.add_block_to_player(block);
    }
}

// 华丽收场：仅当抽牌堆为空时可打出（BattleEngine::play_card）；对全体造成 50/60
void effect_grand_finale(EffectContext& ctx, bool is_upgraded) {
    ctx.deal_damage_to_all_monsters(is_upgraded ? 60 : 50);
}

// 深呼吸：弃牌堆洗入抽牌堆后抽 1/2 张
void effect_deep_breath(EffectContext& ctx, bool is_upgraded) {
    ctx.shuffle_discard_into_draw();
    ctx.draw_cards(is_upgraded ? 2 : 1);
}

// 内脏切除：对目标造成 7/9 点伤害三次（本回合弃牌减费由 BattleEngine::play_card 处理）
void effect_eviscerate(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    const int hit = is_upgraded ? 9 : 7;
    for (int k = 0; k < 3; ++k) {
        int dmg = ctx.get_effective_damage_dealt_by_player(hit, ctx.target_monster_index);
        if (dmg > 0) ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 终结技：本回合每打出过一张攻击牌，对目标造成 6/8 点伤害一次（次数含本牌，由 attacks_played_this_turn 统计）
void effect_finisher(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    const int n = ctx.get_status_stacks_on_player("attacks_played_this_turn");
    const int per = is_upgraded ? 8 : 6;
    for (int i = 0; i < n; ++i) {
        int dmg = ctx.get_effective_damage_dealt_by_player(per, ctx.target_monster_index);
        if (dmg > 0) ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 弹跳药瓶：随机对存活敌人施加 3 层中毒，共 3/4 次
void effect_bouncing_flask(EffectContext& ctx, bool is_upgraded) {
    const int times = is_upgraded ? 4 : 3;
    const int mc = ctx.get_monster_count();
    if (mc <= 0) return;
    std::vector<int> alive;
    alive.reserve(static_cast<size_t>(mc));
    for (int i = 0; i < mc; ++i)
        if (ctx.get_monster_current_hp(i) > 0) alive.push_back(i);
    if (alive.empty()) return;
    for (int t = 0; t < times; ++t) {
        const int idx = alive[static_cast<size_t>(
            ctx.uniform_int(0, static_cast<int>(alive.size()) - 1))];
        ctx.apply_status_to_monster(idx, "poison", 3, 3);
    }
}

// 贪婪之手：造成伤害；若击杀目标获得 20/25 金币
void effect_hand_of_greed(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    const int base = is_upgraded ? 25 : 20;
    const int hp_before = ctx.get_monster_current_hp(ctx.target_monster_index);
    const int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    const int hp_after = ctx.get_monster_current_hp(ctx.target_monster_index);
    if (hp_before > 0 && hp_after <= 0) ctx.add_gold_to_player(is_upgraded ? 25 : 20);
}

// 神气制胜：能力层数存伤害值；每打出 5 张牌对全体造成该伤害（BattleEngine::panache_on_any_card_played）
void effect_panache(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("panache", is_upgraded ? 14 : 10, -1);
}

// 炼制药水：获得一瓶随机药水（药水栏满则无效果）
void effect_venomology(EffectContext& ctx, bool /*is_upgraded*/) {
    (void)ctx.grant_random_potion();
}

// 毒雾：每回合开始时使所有敌人获得 2/3 层中毒（由 PoisonCloudModifier 处理）
void effect_noxious_fumes(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("poison_cloud", is_upgraded ? 3 : 2, -1);
}

// 必备工具：回合开始额外抽 1 并弃 1（由 BattleEngine 回合开始逻辑处理）
void effect_tools_of_the_trade(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.apply_status_to_player("essential_tools", 1, -1);
}

// 计划妥当：回合结束时额外保留 1/2 张非保留手牌（由 BattleEngine 回合结束逻辑处理）
void effect_well_laid_plans(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("well_planned", is_upgraded ? 2 : 1, -1);
}

// 战斗专注：抽 3/4 张牌，并在本回合内禁止再抽牌
void effect_battle_trance(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 4 : 3);
    ctx.apply_status_to_player("cannot_draw", 1, 1);
}

// 扫荡射线：对全体造成 6/9 伤害并抽 1
void effect_sweeping_beam(EffectContext& ctx, bool is_upgraded) {
    ctx.deal_damage_to_all_monsters(is_upgraded ? 9 : 6);
    ctx.draw_cards(1);
}

// 飞跃：获得 9/12 格挡
void effect_leap(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 12 : 9);
    ctx.add_block_to_player(block);
}

// 充电：获得 7/10 格挡，下回合获得 1 点能量
void effect_charge_battery(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 10 : 7);
    ctx.add_block_to_player(block);
    ctx.apply_status_to_player("energy_up", 1, 1);
}

// 启动流程：获得 10/13 格挡（固有/消耗由数据层字段处理）
void effect_boot_sequence(EffectContext& ctx, bool is_upgraded) {
    int block = ctx.get_effective_block_for_player(is_upgraded ? 13 : 10);
    ctx.add_block_to_player(block);
}

// 快速检索：抽 3/4 张牌
void effect_skim(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 4 : 3);
}

// 超频：抽 2/3 张牌，并加入 1 张灼伤到弃牌堆
void effect_steam_power(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 3 : 2);
    ctx.generate_to_discard_pile("card_026");
}

// 光束射线：造成 3/4 伤害，并施加 1/2 层易伤
void effect_beam_cell(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 4 : 3, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    int vuln = is_upgraded ? 2 : 1;
    ctx.apply_status_to_monster(ctx.target_monster_index, "vulnerable", vuln, vuln);
}

// 核心电涌：造成 11/15 伤害，获得 1 层人工制品
void effect_core_surge(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 15 : 11, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    ctx.apply_status_to_player("artifact", 1, -1);
}

// 内核加速：获得 2/3 点能量，加入 1 张虚空到弃牌堆
void effect_turbo(EffectContext& ctx, bool is_upgraded) {
    ctx.add_energy_to_player(is_upgraded ? 3 : 2);
    ctx.generate_to_discard_pile("void");
}

// 自动护盾：若当前没有格挡，获得 11/15 格挡
void effect_auto_shields(EffectContext& ctx, bool is_upgraded) {
    if (ctx.get_player_block() > 0) return;
    int block = ctx.get_effective_block_for_player(is_upgraded ? 15 : 11);
    ctx.add_block_to_player(block);
}

// 弹回：造成 9/12 点伤害（回牌堆顶效果待后续卡序系统补齐）
void effect_rebound(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int dmg = ctx.get_effective_damage_dealt_by_player(is_upgraded ? 12 : 9, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
}

// 双倍能量：将当前能量翻倍（等价于再获得当前能量值）
void effect_double_energy(EffectContext& ctx, bool /*is_upgraded*/) {
    int e = ctx.get_player_energy();
    if (e > 0) ctx.add_energy_to_player(e);
}

// 堆栈：获得弃牌堆张数/弃牌堆张数+3 的格挡
void effect_stack(EffectContext& ctx, bool is_upgraded) {
    int base = ctx.get_discard_pile_size() + (is_upgraded ? 3 : 0);
    if (base <= 0) return;
    int block = ctx.get_effective_block_for_player(base);
    ctx.add_block_to_player(block);
}

// 汇集：抽牌堆每 4/3 张牌获得 1 点能量
void effect_aggregate(EffectContext& ctx, bool is_upgraded) {
    int per = is_upgraded ? 3 : 4;
    int e = ctx.get_draw_pile_size() / per;
    if (e > 0) ctx.add_energy_to_player(e);
}

// 金属化：回合结束时获得 3/4 点格挡（metallicize 状态）
void effect_metallicize(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("metallicize", is_upgraded ? 4 : 3, -1);
}

// 灵动步法：获得 2/3 点敏捷
void effect_footwork(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("dexterity", is_upgraded ? 3 : 2, -1);
}

// 精准：小刀基础伤害增加 4/6（层数叠加，见 effect_shiv）
void effect_accuracy(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("accuracy", is_upgraded ? 6 : 4, -1);
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

// 小刀：造成 4/6 点伤害，消耗；精准：每层 accuracy 为固定加伤（可叠加）
void effect_shiv(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    const int acc = ctx.get_status_stacks_on_player("accuracy");
    int base = (is_upgraded ? 6 : 4) + acc;
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

// 狂宴：造成 10/12 点伤害；若击杀目标，永久获得 3/4 点最大生命
void effect_feed(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int hp_before = ctx.get_monster_current_hp(ctx.target_monster_index);
    if (hp_before <= 0) return;
    int base = is_upgraded ? 12 : 10;
    int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    int hp_after = ctx.get_monster_current_hp(ctx.target_monster_index);
    if (hp_after <= 0) {
        ctx.add_player_max_hp(is_upgraded ? 4 : 3);
    }
}

// 恶魔之焰：消耗所有手牌；每张被消耗的牌造成 7/10 伤害
void effect_fiend_fire(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int hits = ctx.exhaust_all_hand_cards();
    if (hits <= 0) return;
    int base = is_upgraded ? 10 : 7;
    for (int i = 0; i < hits; ++i) {
        int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        if (dmg > 0) ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 旋风斩：对所有敌人造成 5/8 点伤害 X 次（X 由 BattleEngine 在 X 费结算时写入）
void effect_whirlwind(EffectContext& ctx, bool is_upgraded) {
    int x = ctx.x_cost_spent;
    if (x <= 0) return;
    int base = is_upgraded ? 8 : 5;
    for (int i = 0; i < x; ++i) ctx.deal_damage_to_all_monsters(base);
}

// 死亡收割：对所有敌人造成 4/5 点伤害；按实际扣血之和回复生命。消耗。
void effect_reaper(EffectContext& ctx, bool is_upgraded) {
    int base = is_upgraded ? 5 : 4;
    int total_heal = 0;
    const int n = ctx.get_monster_count();
    for (int i = 0; i < n; ++i) {
        int hp_before = ctx.get_monster_current_hp(i);
        if (hp_before <= 0) continue;
        int dmg = ctx.get_effective_damage_dealt_by_player(base, i);
        if (dmg <= 0) continue;
        ctx.deal_damage_to_monster(i, dmg);
        int hp_after = ctx.get_monster_current_hp(i);
        total_heal += hp_before - hp_after;
    }
    if (total_heal > 0) ctx.heal_player(total_heal);
}

// 爆发：本回合下一张/下两张技能额外执行一次（由 BattleEngine::play_card 消费 burst 状态）
void effect_burst(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("burst", is_upgraded ? 2 : 1, 1);
}

// 尸爆术：给予目标 6/9 层中毒与 1 层尸体爆炸
void effect_corpse_explosion(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    int poison = is_upgraded ? 9 : 6;
    ctx.apply_status_to_monster(ctx.target_monster_index, "poison", poison, poison);
    ctx.apply_status_to_monster(ctx.target_monster_index, "corpse_explosion", 1, -1);
}

// 余像：每打出一张牌获得 1 点格挡（由 PlayerAfterimageModifier 处理）
void effect_after_image(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.apply_status_to_player("after_image", 1, -1);
}

// 幽魂形态：获得 2/3 层无实体，并附加 wraith_form 标记
void effect_wraith_form(EffectContext& ctx, bool is_upgraded) {
    int n = is_upgraded ? 3 : 2;
    ctx.apply_status_to_player("intangible", n, n);
    ctx.apply_status_to_player("wraith_form", 1, -1);
}

// 神化：在本场战斗中升级所有牌
void effect_apotheosis(EffectContext& ctx, bool is_upgraded) {
    (void)is_upgraded;
    ctx.upgrade_all_cards_in_combat();
}

// 策略大师：抽 3/4 张牌，消耗
void effect_master_of_strategy(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 4 : 3);
}

// 炸弹：3 回合后对所有敌人造成 40/50 伤害（由回合结束流程读取 the_bomb 状态触发）
void effect_the_bomb(EffectContext& ctx, bool is_upgraded) {
    int dmg = is_upgraded ? 50 : 40;
    ctx.apply_status_to_player("the_bomb", dmg, 3);
}

// 壁垒：格挡不在回合开始时清空（StatusModifiers 读取 barricade）
void effect_barricade(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.apply_status_to_player("barricade", 1, -1);
}

// 腐化：技能 0 费且打出时消耗（BattleEngine::play_card）
void effect_corruption(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.apply_status_to_player("corruption", 1, -1);
}

// 黑暗之拥：每消耗一张牌抽 1（层数可叠加；apply_exhaust_passives_from_hand）
void effect_dark_embrace(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.apply_status_to_player("dark_embrace", 1, -1);
}

// 进化：抽到状态牌时再抽 1/2 张（draw_cards_impl）
void effect_evolve(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("evolve", is_upgraded ? 2 : 1, -1);
}

// 无惧疼痛：每消耗一张牌获得 3/4 格挡（经有效格挡修正）
void effect_feel_no_pain(EffectContext& ctx, bool is_upgraded) {
    ctx.apply_status_to_player("feel_no_pain", is_upgraded ? 4 : 3, -1);
}

// 灭除之刃：对目标造成 9/15 点伤害 X 次（X 费）
void effect_expunger(EffectContext& ctx, bool is_upgraded) {
    const int x = ctx.x_cost_spent;
    if (x <= 0 || ctx.target_monster_index < 0) return;
    const int base = is_upgraded ? 15 : 9;
    for (int i = 0; i < x; ++i) {
        const int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
        if (dmg > 0) ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    }
}

// 净化：随机消耗手牌中最多 3/5 张（打出后结算，不含本牌）
void effect_purity(EffectContext& ctx, bool is_upgraded) {
    const int cap = is_upgraded ? 5 : 3;
    const int n = std::min(cap, ctx.get_hand_card_count());
    if (n > 0) ctx.exhaust_random_hand_cards(n);
}

// 暴力：从抽牌堆随机将 3/4 张攻击牌入手
void effect_violence(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_random_attack_cards_from_draw_pile(is_upgraded ? 4 : 3);
}

// J.A.X.：失去 3 生命，获得 2/3 力量
void effect_jax(EffectContext& ctx, bool is_upgraded) {
    ctx.deal_damage_to_player(3);
    ctx.apply_status_to_player("strength", is_upgraded ? 3 : 2, -1);
}

// 洞见：抽 2/3 张牌（保留/消耗由数据层与 BattleEngine 处理）
void effect_insight(EffectContext& ctx, bool is_upgraded) {
    ctx.draw_cards(is_upgraded ? 3 : 2);
}

// 结茧：将 3/5 张随机技能牌洗入抽牌堆，本场战斗这些实例作为技能时耗能 0
// 仪式匕首：15 + 本局累计加成；斩杀则永久增加本局加成 +3/+5（跨战斗保存在 PlayerBattleState）
void effect_ritual_dagger(EffectContext& ctx, bool is_upgraded) {
    if (ctx.target_monster_index < 0) return;
    const int bonus = ctx.get_ritual_dagger_run_bonus();
    const int base = 15 + bonus;
    const int hp_before = ctx.get_monster_current_hp(ctx.target_monster_index);
    const int dmg = ctx.get_effective_damage_dealt_by_player(base, ctx.target_monster_index);
    ctx.deal_damage_to_monster(ctx.target_monster_index, dmg);
    const int hp_after = ctx.get_monster_current_hp(ctx.target_monster_index);
    if (hp_before > 0 && hp_after <= 0) ctx.add_ritual_dagger_run_bonus(is_upgraded ? 5 : 3);
}

// 秘密技法：从抽牌堆随机 1 张技能牌入手（与尖塔「随机」一致时可改为顶牌序）
void effect_secret_technique(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.draw_random_skill_cards_from_draw_pile(1);
}

// 灵体：获得 1 层无实体（持续 1 回合），虚无/消耗由数据层处理
void effect_ghostly(EffectContext& ctx, bool /*is_upgraded*/) {
    ctx.apply_status_to_player("intangible", 1, 1);
}

void effect_chrysalis(EffectContext& ctx, bool is_upgraded) {
    static const std::vector<CardId> pool = {
        "defend", "defend+", "survivor", "survivor+", "shrug_it_off", "shrug_it_off+", "backflip", "backflip+",
        "deadly_poison", "deadly_poison+", "piercing_wail", "piercing_wail+", "prepared", "prepared+",
        "escape_plan", "escape_plan+", "finesse", "finesse+", "deep_breath", "deep_breath+",
        "true_grit", "true_grit+", "burning_pact", "burning_pact+", "flame_barrier", "flame_barrier+",
        "spot_weakness", "spot_weakness+", "acrobatics", "acrobatics+", "concentrate", "concentrate+",
        "second_wind", "second_wind+", "disarm", "disarm+", "charge_battery", "charge_battery+",
        "skim", "skim+", "leap", "leap+", "dark_shackles", "dark_shackles+", "panacea", "panacea+",
        "master_of_strategy", "master_of_strategy+", "boot_sequence", "boot_sequence+", "turbo", "turbo+",
        "steam_power", "steam_power+", "dodge_and_roll", "dodge_and_roll+", "power_through", "power_through+",
        "auto_shields", "auto_shields+", "stack", "stack+", "aggregate", "aggregate+",
        "card_007", "card_007+", "card_008", "card_008+", "card_009", "card_009+", "card_010", "card_010+",
        "card_012", "card_012+", "card_013", "card_013+", "card_014", "card_014+", "card_016", "card_016+",
        "card_018", "card_018+",
    };
    const int times = is_upgraded ? 5 : 3;
    if (pool.empty()) return;
    for (int i = 0; i < times; ++i) {
        const CardId& id = pool[ctx.uniform_size(0, pool.size() - 1)];
        ctx.generate_to_draw_pile_combat_zero_skill(id);
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
    card_system.register_card_effect("barricade", [](EffectContext& c) { effect_barricade(c, false); });
    card_system.register_card_effect("barricade+", [](EffectContext& c) { effect_barricade(c, true); });
    card_system.register_card_effect("corruption", [](EffectContext& c) { effect_corruption(c, false); });
    card_system.register_card_effect("corruption+", [](EffectContext& c) { effect_corruption(c, true); });
    card_system.register_card_effect("dark_embrace", [](EffectContext& c) { effect_dark_embrace(c, false); });
    card_system.register_card_effect("dark_embrace+", [](EffectContext& c) { effect_dark_embrace(c, true); });
    card_system.register_card_effect("evolve", [](EffectContext& c) { effect_evolve(c, false); });
    card_system.register_card_effect("evolve+", [](EffectContext& c) { effect_evolve(c, true); });
    card_system.register_card_effect("feel_no_pain", [](EffectContext& c) { effect_feel_no_pain(c, false); });
    card_system.register_card_effect("feel_no_pain+", [](EffectContext& c) { effect_feel_no_pain(c, true); });
    card_system.register_card_effect("expunger", [](EffectContext& c) { effect_expunger(c, false); });
    card_system.register_card_effect("expunger+", [](EffectContext& c) { effect_expunger(c, true); });
    card_system.register_card_effect("purity", [](EffectContext& c) { effect_purity(c, false); });
    card_system.register_card_effect("purity+", [](EffectContext& c) { effect_purity(c, true); });
    card_system.register_card_effect("violence", [](EffectContext& c) { effect_violence(c, false); });
    card_system.register_card_effect("violence+", [](EffectContext& c) { effect_violence(c, true); });
    card_system.register_card_effect("jax", [](EffectContext& c) { effect_jax(c, false); });
    card_system.register_card_effect("jax+", [](EffectContext& c) { effect_jax(c, true); });
    card_system.register_card_effect("insight", [](EffectContext& c) { effect_insight(c, false); });
    card_system.register_card_effect("insight+", [](EffectContext& c) { effect_insight(c, true); });
    card_system.register_card_effect("chrysalis", [](EffectContext& c) { effect_chrysalis(c, false); });
    card_system.register_card_effect("chrysalis+", [](EffectContext& c) { effect_chrysalis(c, true); });
    card_system.register_card_effect("ritual_dagger", [](EffectContext& c) { effect_ritual_dagger(c, false); });
    card_system.register_card_effect("ritual_dagger+", [](EffectContext& c) { effect_ritual_dagger(c, true); });
    card_system.register_card_effect("secret_technique", [](EffectContext& c) { effect_secret_technique(c, false); });
    card_system.register_card_effect("secret_technique+", [](EffectContext& c) { effect_secret_technique(c, true); });
    card_system.register_card_effect("ghostly", [](EffectContext& c) { effect_ghostly(c, false); });
    card_system.register_card_effect("ghostly+", [](EffectContext& c) { effect_ghostly(c, true); });
    card_system.register_card_effect("armaments", [](EffectContext& c) { effect_armaments(c, false); });
    card_system.register_card_effect("armaments+", [](EffectContext& c) { effect_armaments(c, true); });
    card_system.register_card_effect("exhume", [](EffectContext& c) { effect_exhume(c, false); });
    card_system.register_card_effect("exhume+", [](EffectContext& c) { effect_exhume(c, true); });
    card_system.register_card_effect("burning_pact", [](EffectContext& c) { effect_burning_pact(c, false); });
    card_system.register_card_effect("burning_pact+", [](EffectContext& c) { effect_burning_pact(c, true); });
    card_system.register_card_effect("true_grit", [](EffectContext& c) { effect_true_grit(c, false); });
    card_system.register_card_effect("true_grit+", [](EffectContext& c) { effect_true_grit(c, true); });
    card_system.register_card_effect("survivor", [](EffectContext& c) { effect_survivor(c, false); });
    card_system.register_card_effect("survivor+", [](EffectContext& c) { effect_survivor(c, true); });
    card_system.register_card_effect("acrobatics", [](EffectContext& c) { effect_acrobatics(c, false); });
    card_system.register_card_effect("acrobatics+", [](EffectContext& c) { effect_acrobatics(c, true); });
    card_system.register_card_effect("flame_barrier", [](EffectContext& c) { effect_flame_barrier(c, false); });
    card_system.register_card_effect("flame_barrier+", [](EffectContext& c) { effect_flame_barrier(c, true); });
    card_system.register_card_effect("second_wind", [](EffectContext& c) { effect_second_wind(c, false); });
    card_system.register_card_effect("second_wind+", [](EffectContext& c) { effect_second_wind(c, true); });
    card_system.register_card_effect("spot_weakness", [](EffectContext& c) { effect_spot_weakness(c, false); });
    card_system.register_card_effect("spot_weakness+", [](EffectContext& c) { effect_spot_weakness(c, true); });
    card_system.register_card_effect("bite", [](EffectContext& c) { effect_bite(c, false); });
    card_system.register_card_effect("bite+", [](EffectContext& c) { effect_bite(c, true); });
    card_system.register_card_effect("flash_of_steel", [](EffectContext& c) { effect_flash_of_steel(c, false); });
    card_system.register_card_effect("flash_of_steel+", [](EffectContext& c) { effect_flash_of_steel(c, true); });
    card_system.register_card_effect("finesse", [](EffectContext& c) { effect_finesse(c, false); });
    card_system.register_card_effect("finesse+", [](EffectContext& c) { effect_finesse(c, true); });
    card_system.register_card_effect("disarm", [](EffectContext& c) { effect_disarm(c, false); });
    card_system.register_card_effect("disarm+", [](EffectContext& c) { effect_disarm(c, true); });
    card_system.register_card_effect("dark_shackles", [](EffectContext& c) { effect_dark_shackles(c, false); });
    card_system.register_card_effect("dark_shackles+", [](EffectContext& c) { effect_dark_shackles(c, true); });
    card_system.register_card_effect("panacea", [](EffectContext& c) { effect_panacea(c, false); });
    card_system.register_card_effect("panacea+", [](EffectContext& c) { effect_panacea(c, true); });
    card_system.register_card_effect("prepared", [](EffectContext& c) { effect_prepared(c, false); });
    card_system.register_card_effect("prepared+", [](EffectContext& c) { effect_prepared(c, true); });
    card_system.register_card_effect("all_out_attack", [](EffectContext& c) { effect_all_out_attack(c, false); });
    card_system.register_card_effect("all_out_attack+", [](EffectContext& c) { effect_all_out_attack(c, true); });
    card_system.register_card_effect("calculated_gamble", [](EffectContext& c) { effect_calculated_gamble(c, false); });
    card_system.register_card_effect("calculated_gamble+", [](EffectContext& c) { effect_calculated_gamble(c, true); });
    card_system.register_card_effect("concentrate", [](EffectContext& c) { effect_concentrate(c, false); });
    card_system.register_card_effect("concentrate+", [](EffectContext& c) { effect_concentrate(c, true); });
    card_system.register_card_effect("piercing_wail", [](EffectContext& c) { effect_piercing_wail(c, false); });
    card_system.register_card_effect("piercing_wail+", [](EffectContext& c) { effect_piercing_wail(c, true); });
    card_system.register_card_effect("sneaky_strike", [](EffectContext& c) { effect_sneaky_strike(c, false); });
    card_system.register_card_effect("sneaky_strike+", [](EffectContext& c) { effect_sneaky_strike(c, true); });
    card_system.register_card_effect("escape_plan", [](EffectContext& c) { effect_escape_plan(c, false); });
    card_system.register_card_effect("escape_plan+", [](EffectContext& c) { effect_escape_plan(c, true); });
    card_system.register_card_effect("grand_finale", [](EffectContext& c) { effect_grand_finale(c, false); });
    card_system.register_card_effect("grand_finale+", [](EffectContext& c) { effect_grand_finale(c, true); });
    card_system.register_card_effect("deep_breath", [](EffectContext& c) { effect_deep_breath(c, false); });
    card_system.register_card_effect("deep_breath+", [](EffectContext& c) { effect_deep_breath(c, true); });
    card_system.register_card_effect("eviscerate", [](EffectContext& c) { effect_eviscerate(c, false); });
    card_system.register_card_effect("eviscerate+", [](EffectContext& c) { effect_eviscerate(c, true); });
    card_system.register_card_effect("finisher", [](EffectContext& c) { effect_finisher(c, false); });
    card_system.register_card_effect("finisher+", [](EffectContext& c) { effect_finisher(c, true); });
    card_system.register_card_effect("bouncing_flask", [](EffectContext& c) { effect_bouncing_flask(c, false); });
    card_system.register_card_effect("bouncing_flask+", [](EffectContext& c) { effect_bouncing_flask(c, true); });
    card_system.register_card_effect("hand_of_greed", [](EffectContext& c) { effect_hand_of_greed(c, false); });
    card_system.register_card_effect("hand_of_greed+", [](EffectContext& c) { effect_hand_of_greed(c, true); });
    card_system.register_card_effect("panache", [](EffectContext& c) { effect_panache(c, false); });
    card_system.register_card_effect("panache+", [](EffectContext& c) { effect_panache(c, true); });
    card_system.register_card_effect("venomology", [](EffectContext& c) { effect_venomology(c, false); });
    card_system.register_card_effect("venomology+", [](EffectContext& c) { effect_venomology(c, true); });
    card_system.register_card_effect("noxious_fumes", [](EffectContext& c) { effect_noxious_fumes(c, false); });
    card_system.register_card_effect("noxious_fumes+", [](EffectContext& c) { effect_noxious_fumes(c, true); });
    card_system.register_card_effect("tools_of_the_trade", [](EffectContext& c) { effect_tools_of_the_trade(c, false); });
    card_system.register_card_effect("tools_of_the_trade+", [](EffectContext& c) { effect_tools_of_the_trade(c, true); });
    card_system.register_card_effect("well_laid_plans", [](EffectContext& c) { effect_well_laid_plans(c, false); });
    card_system.register_card_effect("well_laid_plans+", [](EffectContext& c) { effect_well_laid_plans(c, true); });
    card_system.register_card_effect("battle_trance", [](EffectContext& c) { effect_battle_trance(c, false); });
    card_system.register_card_effect("battle_trance+", [](EffectContext& c) { effect_battle_trance(c, true); });
    card_system.register_card_effect("sweeping_beam", [](EffectContext& c) { effect_sweeping_beam(c, false); });
    card_system.register_card_effect("sweeping_beam+", [](EffectContext& c) { effect_sweeping_beam(c, true); });
    card_system.register_card_effect("leap", [](EffectContext& c) { effect_leap(c, false); });
    card_system.register_card_effect("leap+", [](EffectContext& c) { effect_leap(c, true); });
    card_system.register_card_effect("charge_battery", [](EffectContext& c) { effect_charge_battery(c, false); });
    card_system.register_card_effect("charge_battery+", [](EffectContext& c) { effect_charge_battery(c, true); });
    card_system.register_card_effect("boot_sequence", [](EffectContext& c) { effect_boot_sequence(c, false); });
    card_system.register_card_effect("boot_sequence+", [](EffectContext& c) { effect_boot_sequence(c, true); });
    card_system.register_card_effect("skim", [](EffectContext& c) { effect_skim(c, false); });
    card_system.register_card_effect("skim+", [](EffectContext& c) { effect_skim(c, true); });
    card_system.register_card_effect("steam_power", [](EffectContext& c) { effect_steam_power(c, false); });
    card_system.register_card_effect("steam_power+", [](EffectContext& c) { effect_steam_power(c, true); });
    card_system.register_card_effect("beam_cell", [](EffectContext& c) { effect_beam_cell(c, false); });
    card_system.register_card_effect("beam_cell+", [](EffectContext& c) { effect_beam_cell(c, true); });
    card_system.register_card_effect("core_surge", [](EffectContext& c) { effect_core_surge(c, false); });
    card_system.register_card_effect("core_surge+", [](EffectContext& c) { effect_core_surge(c, true); });
    card_system.register_card_effect("turbo", [](EffectContext& c) { effect_turbo(c, false); });
    card_system.register_card_effect("turbo+", [](EffectContext& c) { effect_turbo(c, true); });
    card_system.register_card_effect("auto_shields", [](EffectContext& c) { effect_auto_shields(c, false); });
    card_system.register_card_effect("auto_shields+", [](EffectContext& c) { effect_auto_shields(c, true); });
    card_system.register_card_effect("rebound", [](EffectContext& c) { effect_rebound(c, false); });
    card_system.register_card_effect("rebound+", [](EffectContext& c) { effect_rebound(c, true); });
    card_system.register_card_effect("double_energy", [](EffectContext& c) { effect_double_energy(c, false); });
    card_system.register_card_effect("double_energy+", [](EffectContext& c) { effect_double_energy(c, true); });
    card_system.register_card_effect("stack", [](EffectContext& c) { effect_stack(c, false); });
    card_system.register_card_effect("stack+", [](EffectContext& c) { effect_stack(c, true); });
    card_system.register_card_effect("aggregate", [](EffectContext& c) { effect_aggregate(c, false); });
    card_system.register_card_effect("aggregate+", [](EffectContext& c) { effect_aggregate(c, true); });
    card_system.register_card_effect("metallicize", [](EffectContext& c) { effect_metallicize(c, false); });
    card_system.register_card_effect("metallicize+", [](EffectContext& c) { effect_metallicize(c, true); });
    card_system.register_card_effect("footwork", [](EffectContext& c) { effect_footwork(c, false); });
    card_system.register_card_effect("footwork+", [](EffectContext& c) { effect_footwork(c, true); });
    card_system.register_card_effect("accuracy", [](EffectContext& c) { effect_accuracy(c, false); });
    card_system.register_card_effect("accuracy+", [](EffectContext& c) { effect_accuracy(c, true); });
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
    // 新增一批可制作卡牌
    card_system.register_card_effect("feed", [](EffectContext& c) { effect_feed(c, false); });
    card_system.register_card_effect("feed+", [](EffectContext& c) { effect_feed(c, true); });
    card_system.register_card_effect("fiend_fire", [](EffectContext& c) { effect_fiend_fire(c, false); });
    card_system.register_card_effect("fiend_fire+", [](EffectContext& c) { effect_fiend_fire(c, true); });
    card_system.register_card_effect("whirlwind", [](EffectContext& c) { effect_whirlwind(c, false); });
    card_system.register_card_effect("whirlwind+", [](EffectContext& c) { effect_whirlwind(c, true); });
    card_system.register_card_effect("reaper", [](EffectContext& c) { effect_reaper(c, false); });
    card_system.register_card_effect("reaper+", [](EffectContext& c) { effect_reaper(c, true); });
    card_system.register_card_effect("burst", [](EffectContext& c) { effect_burst(c, false); });
    card_system.register_card_effect("burst+", [](EffectContext& c) { effect_burst(c, true); });
    card_system.register_card_effect("corpse_explosion", [](EffectContext& c) { effect_corpse_explosion(c, false); });
    card_system.register_card_effect("corpse_explosion+", [](EffectContext& c) { effect_corpse_explosion(c, true); });
    card_system.register_card_effect("after_image", [](EffectContext& c) { effect_after_image(c, false); });
    card_system.register_card_effect("after_image+", [](EffectContext& c) { effect_after_image(c, true); });
    card_system.register_card_effect("wraith_form", [](EffectContext& c) { effect_wraith_form(c, false); });
    card_system.register_card_effect("wraith_form+", [](EffectContext& c) { effect_wraith_form(c, true); });
    card_system.register_card_effect("apotheosis", [](EffectContext& c) { effect_apotheosis(c, false); });
    card_system.register_card_effect("apotheosis+", [](EffectContext& c) { effect_apotheosis(c, true); });
    card_system.register_card_effect("master_of_strategy", [](EffectContext& c) { effect_master_of_strategy(c, false); });
    card_system.register_card_effect("master_of_strategy+", [](EffectContext& c) { effect_master_of_strategy(c, true); });
    card_system.register_card_effect("the_bomb", [](EffectContext& c) { effect_the_bomb(c, false); });
    card_system.register_card_effect("the_bomb+", [](EffectContext& c) { effect_the_bomb(c, true); });
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
    // 诅咒：傲慢可打出，效果为空（消耗由引擎按 CardData 处理）
    card_system.register_card_effect("pride", effect_power_noop);
    card_system.register_card_effect("pride+", effect_power_noop);
    // 状态：黏液打出无额外效果，消耗由 CardData 处理
    card_system.register_card_effect("slimed", effect_power_noop);
    card_system.register_card_effect("slimed+", effect_power_noop);
    // 后续在此追加：card_system.register_card_effect("poison_stab", effect_poison_stab); 等
}

} // namespace tce
