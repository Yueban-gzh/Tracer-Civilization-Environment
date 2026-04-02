#include "../../include/BattleCoreRefactor/RelicModifiers.hpp"
#include "../../include/BattleCoreRefactor/BattleState.hpp"
#include "../../include/BattleCoreRefactor/Damage.hpp"
#include <algorithm>

namespace tce {

namespace {
    static void add_or_merge_status(std::vector<StatusInstance>& list, StatusId id, int stacks, int duration) {
        for (auto& s : list) {
            if (s.id == id) {
                s.stacks += stacks;
                if (duration >= 0) s.duration = duration;
                return;
            }
        }
        list.push_back(StatusInstance{std::move(id), stacks, duration});
    }
}

class BurningBloodRelic : public IBattleModifier {  // 燃烧之血：战斗胜利时回复 6 点生命（杀戮尖塔规则）
public:
    void on_monster_died(int /*monsterIndex*/, BattleState& state, MonsterDiedContext* /*ctx*/) override {
        for (const auto& m : state.monsters) {
            if (m.currentHp > 0) return;  // 仍有怪物存活则跳过
        }
        if (state.player.currentHp <= 0) return;  // 玩家已死则不回复
        state.player.currentHp += 6;
        if (state.player.currentHp > state.player.maxHp)
            state.player.currentHp = state.player.maxHp;
    }
};

class StrengthRelic : public IBattleModifier {
public:
    void on_player_deal_damage(DamagePacket& dmg, const BattleState& /*state*/) override {
        dmg.modified_amount += 2;
    }
};

class RedSkullRelic : public IBattleModifier {  // 红头骨：生命值 ≤50% 时，获得额外 3 点力量
public:
    void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
        if (!dmg.from_attack) return;                                // 仅攻击伤害生效（力量规则）
        if (state.player.maxHp <= 0) return;                        // 最大生命无效则跳过
        if (state.player.currentHp > state.player.maxHp / 2) return;  // 生命 >50% 则不触发
        dmg.modified_amount += 3;                                    // 额外 +3 点伤害（等效 3 点力量）
    }
};

class SnakeSkullRelic : public IBattleModifier {  // 异蛇头骨：给予敌人中毒时，额外给予 1 层中毒
public:
    void on_before_status_applied(StatusApplyContext& ctx) override {
        if (ctx.target != StatusApplyContext::Target::Monster) return;  // 仅对怪物施加时生效
        if (ctx.id != "poison") return;                                // 仅中毒状态
        if (ctx.blocked) return;                                      // 已被阻止则跳过
        ctx.stacks += 1;                                              // 额外 +1 层中毒
    }
};

class MarbleBagRelic : public IBattleModifier {  // 弹珠袋：战斗开始时，给予所有敌人 1 层易伤
public:
    void on_battle_start(BattleState& state) override {
        for (auto& m : state.monsters) {
            if (m.currentHp > 0)
                add_or_merge_status(m.statuses, "vulnerable", 1, 1);
        }
    }
};

class SmallBloodVialRelic : public IBattleModifier {  // 小血瓶：战斗开始时，回复 2 点生命
public:
    void on_battle_start(BattleState& state) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则不回复
        state.player.currentHp += 2;               // 回复 2 点生命
        if (state.player.currentHp > state.player.maxHp)
            state.player.currentHp = state.player.maxHp;  // 不超过最大生命
    }
};

class AnchorRelic : public IBattleModifier {  // 锚：每场战斗开始时，获得 10 点格挡
public:
    void on_battle_start(BattleState& state) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        state.player.block += 10;                   // 获得 10 点格挡
    }
};

class CopperScalesRelic : public IBattleModifier {};  // 铜制鳞片：荆棘效果已自代码中移除，遗物保留为无效果占位

class SmoothStoneRelic : public IBattleModifier {  // 意外光滑的石头：每场战斗开始时，获得 1 点敏捷
public:
    void on_battle_start(BattleState& state) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        add_or_merge_status(state.player.statuses, "dexterity", 1, -1);  // 获得 1 层敏捷，本场战斗内有效
    }
};

class VajraRelic : public IBattleModifier {  // 金刚杵：每场战斗开始时，获得 1 点力量
public:
    void on_battle_start(BattleState& state) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        add_or_merge_status(state.player.statuses, "strength", 1, -1);  // 获得 1 层力量，本场战斗内有效
    }
};

class NunchakuRelic : public IBattleModifier {  // 双截棍：每打出 10 张攻击牌，获得 1 点能量
private:
    int attack_played_this_combat_ = 0;             // 本场战斗已打出攻击牌数
public:
    void on_battle_start(BattleState& /*state*/) override {
        attack_played_this_combat_ = 0;            // 新战斗重置计数
    }
    void on_card_played(BattleState& state, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {
        if (!ctx || !ctx->is_attack) return;        // 仅攻击牌计入
        ++attack_played_this_combat_;               // 计数 +1
        if (attack_played_this_combat_ % 10 != 0) return;  // 非第 10、20、30… 张则不触发
        state.player.energy += 1;                   // 获得 1 点能量（本回合可用）
    }
};

class PenNibRelic : public IBattleModifier {  // 钢笔尖：每打出的第 10 张攻击牌造成双倍伤害
private:
    int attack_played_this_combat_ = 0;             // 本场战斗已打出攻击牌数
    bool next_attack_double_ = false;                // 本张攻击牌是否双倍（第 10、20… 张时置位，下张牌打出时清除）
public:
    void on_battle_start(BattleState& /*state*/) override {
        attack_played_this_combat_ = 0;            // 新战斗重置计数
        next_attack_double_ = false;                // 重置双倍标记
    }
    void on_card_played(BattleState& /*state*/, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {
        next_attack_double_ = false;                // 打出新牌时清除（上一张攻击的双倍标记已消耗完毕）
        if (!ctx || !ctx->is_attack) return;        // 仅攻击牌计入
        ++attack_played_this_combat_;               // 计数 +1
        if (attack_played_this_combat_ % 10 == 0)
            next_attack_double_ = true;             // 第 10、20、30… 张：本张攻击双倍伤害（含多段）
    }
    void on_player_deal_damage(DamagePacket& dmg, const BattleState& /*state*/) override {
        if (!dmg.from_attack || !next_attack_double_) return;  // 仅攻击且已标记双倍时生效
        dmg.modified_amount *= 2;                   // 双倍伤害（多段攻击每段都双倍）
    }
};

class HandDrumRelic : public IBattleModifier {  // 手摇鼓：回合开始时，获得 1 层真言
public:
    void on_turn_start_player(BattleState& state, TurnStartContext* /*ctx*/) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        add_or_merge_status(state.player.statuses, "mantra", 1, -1);  // 获得 1 层真言，本场战斗内有效
    }
};

class ToyOrnithopterRelic : public IBattleModifier {  // 玩具扑翼飞机：每使用一瓶药水，回复 5 点生命
public:
    void on_potion_used(BattleState& state, PotionId /*id*/) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        state.player.currentHp += 5;               // 回复 5 点生命
        if (state.player.currentHp > state.player.maxHp)
            state.player.currentHp = state.player.maxHp;  // 不超过最大生命
    }
};

class PreparationPackRelic : public IBattleModifier {  // 准备背包：每场战斗开始时，额外抽 2 张牌
public:
    void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
        if (!ctx) return;                           // 无上下文则跳过
        if (state.turnNumber != 1) return;          // 仅第一回合（战斗开始）触发
        ctx->draw_count += 2;                       // 本回合额外抽 2 张牌
    }
};

class ArtOfWarRelic : public IBattleModifier {  // 孙子兵法：回合中未打出攻击牌，下一回合获得 1 点额外能量
private:
    bool attacked_this_turn_ = false;                // 本回合是否打出过攻击牌
    bool next_turn_extra_energy_ = false;            // 下一回合是否获得额外能量（上回合未打攻击）
public:
    void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
        if (!ctx) return;                           // 无上下文则跳过
        if (next_turn_extra_energy_) {               // 上回合未打攻击，本回合奖励
            ctx->energy += 1;                       // 获得 1 点额外能量
            next_turn_extra_energy_ = false;         // 消耗奖励
        }
        attacked_this_turn_ = false;                 // 重置本回合攻击标记
    }
    void on_card_played(BattleState& /*state*/, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {
        if (ctx && ctx->is_attack)
            attacked_this_turn_ = true;             // 打出攻击牌，标记本回合已攻击
    }
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        if (!attacked_this_turn_)
            next_turn_extra_energy_ = true;          // 本回合未打攻击，下回合获得 1 能量
    }
};

class LanternRelic : public IBattleModifier {  // 灯笼：每场战斗的第一回合获得 1 点能量
public:
    void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
        if (!ctx) return;                           // 无上下文则跳过
        if (state.turnNumber != 1) return;          // 仅第一回合触发
        ctx->energy += 1;                           // 本回合能量 +1
    }
};

class HappyFlowerRelic : public IBattleModifier {  // 开心小花：每 3 个回合，获得 1 点能量
public:
    void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
        if (!ctx) return;                           // 无上下文则跳过
        if (state.turnNumber <= 0) return;          // 回合数无效则跳过
        if (state.turnNumber % 3 != 0) return;      // 非第 3、6、9… 回合则跳过
        ctx->energy += 1;                           // 本回合能量 +1
    }
};

class ClockworkBootsRelic : public IBattleModifier {  // 发条靴：攻击伤害 ≤5 且未被格挡时，提升为 5
public:
    void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
        if (!dmg.from_attack) return;                // 仅攻击伤害
        if (dmg.source_type != DamagePacket::SourceType::Player) return;  // 仅玩家造成
        int unblocked = dmg.modified_amount;         // 默认等于总伤害
        if (!dmg.ignore_block && dmg.target_monster_index >= 0 &&
            static_cast<size_t>(dmg.target_monster_index) < state.monsters.size()) {
            int block = state.monsters[static_cast<size_t>(dmg.target_monster_index)].block;  // 目标格挡
            unblocked = (dmg.modified_amount > block) ? (dmg.modified_amount - block) : 0;  // 实际扣血量
        }
        if (unblocked <= 0 || unblocked > 5) return;  // 无伤害或已 >5 则不提升
        if (dmg.ignore_block)
            dmg.modified_amount = 5;                  // 无视格挡时直接设为 5
        else
            dmg.modified_amount = state.monsters[static_cast<size_t>(dmg.target_monster_index)].block + 5;  // 提升使扣血为 5
    }
};

class OrichalcumRelic : public IBattleModifier {  // 奥利哈钢：回合结束时若没有任何格挡，获得 6 点格挡
public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) override {
        if (state.player.currentHp <= 0) return;   // 玩家已死则跳过
        if (state.player.block > 0) return;         // 已有格挡则不触发
        if (ctx && ctx->grant_player_block) ctx->grant_player_block(6);  // 经统一入口（势不可挡等）
        else state.player.block += 6;             // 无回调时直加
    }
};

class CentennialPuzzleRelic : public IBattleModifier {  // 百年积木：每场战斗中第一次损伤生命时，抽 3 张牌
private:
    bool triggered_this_battle_ = false;               // 本场是否已触发
public:
    void on_battle_start(BattleState& /*state*/) override {
        triggered_this_battle_ = false;                // 新战斗重置
    }
    void on_damage_applied(const DamagePacket& /*dmg*/, BattleState& /*state*/, DamageAppliedContext* ctx) override {
        if (!ctx || !ctx->damage_to_player) return;    // 仅玩家受伤时
        if (ctx->hp_damage_to_player <= 0) return;     // 未实际扣血（全格挡）不触发
        if (triggered_this_battle_) return;             // 本场已触发过则跳过
        if (!ctx->draw_cards) return;                  // 无抽牌回调则跳过
        triggered_this_battle_ = true;                 // 标记已触发
        ctx->draw_cards(3);                            // 抽 3 张牌
    }
};

std::vector<std::shared_ptr<IBattleModifier>>
create_relic_modifiers(const std::vector<RelicId>& relics) {
    std::vector<std::shared_ptr<IBattleModifier>> out;
    out.reserve(relics.size());

    for (const auto& id : relics) {
        if (id == "burning_blood") {
            out.push_back(std::make_shared<BurningBloodRelic>());
        } else if (id == "relic_strength_plus") {
            out.push_back(std::make_shared<StrengthRelic>());
        } else if (id == "marble_bag") {
            out.push_back(std::make_shared<MarbleBagRelic>());
        } else if (id == "small_blood_vial") {
            out.push_back(std::make_shared<SmallBloodVialRelic>());
        } else if (id == "copper_scales") {
            out.push_back(std::make_shared<CopperScalesRelic>());
        } else if (id == "centennial_puzzle") {
            out.push_back(std::make_shared<CentennialPuzzleRelic>());
        } else if (id == "clockwork_boots") {
            out.push_back(std::make_shared<ClockworkBootsRelic>());
        } else if (id == "happy_flower") {
            out.push_back(std::make_shared<HappyFlowerRelic>());
        } else if (id == "lantern") {
            out.push_back(std::make_shared<LanternRelic>());
        } else if (id == "smooth_stone") {
            out.push_back(std::make_shared<SmoothStoneRelic>());
        } else if (id == "orichalcum") {
            out.push_back(std::make_shared<OrichalcumRelic>());
        } else if (id == "red_skull") {
            out.push_back(std::make_shared<RedSkullRelic>());
        } else if (id == "snake_skull") {
            out.push_back(std::make_shared<SnakeSkullRelic>());
        } else if (id == "vajra") {
            out.push_back(std::make_shared<VajraRelic>());
        } else if (id == "nunchaku") {
            out.push_back(std::make_shared<NunchakuRelic>());
        } else if (id == "hand_drum") {
            out.push_back(std::make_shared<HandDrumRelic>());
        } else if (id == "pen_nib") {
            out.push_back(std::make_shared<PenNibRelic>());
        } else if (id == "toy_ornithopter") {
            out.push_back(std::make_shared<ToyOrnithopterRelic>());
        } else if (id == "preparation_pack") {
            out.push_back(std::make_shared<PreparationPackRelic>());
        } else if (id == "anchor") {
            out.push_back(std::make_shared<AnchorRelic>());
        } else if (id == "art_of_war") {
            out.push_back(std::make_shared<ArtOfWarRelic>());
        }
    }

    return out;
}

} // namespace tce
