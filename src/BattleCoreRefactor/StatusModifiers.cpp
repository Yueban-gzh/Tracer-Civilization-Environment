/**
 * 状态效果实现（按杀戮尖塔 wiki 规则）
 */
 #include <algorithm>                                               // std::remove_if
 #include "../../include/BattleCoreRefactor/StatusModifiers.hpp"   // 工厂函数声明
 #include "../../include/BattleCoreRefactor/BattleEngine.hpp"       // BattleEngine、BattleState
 
namespace tce {

// 叠加或新建力量状态（Ritual、StrengthBoost 等共用）
static void add_strength_stacks(std::vector<StatusInstance>& list, int stacks) {
    for (auto& s : list) {
        if (s.id == "strength") { s.stacks += stacks; return; }
    }
    list.push_back(StatusInstance{"strength", stacks, -1});
}

// ========== 负面 - 玩家 ==========
 class PlayerPoisonModifier : public IBattleModifier {              // 玩家中毒：回合开始扣血
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx || !ctx->deal_damage_to_player_ignoring_block) return;  // 无回调则跳过
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "poison");  // 中毒层数
         if (stacks <= 0) return;
         ctx->deal_damage_to_player_ignoring_block(stacks);         // 造成无视格挡伤害
         BattleEngine::reduce_status_stacks(state.player.statuses, "poison", 1);  // 中毒层数 -1
     }
 };

 class PlayerConfusionModifier : public IBattleModifier {};         // 混乱：抽牌费用随机（桩）
 class PlayerDexterityDownModifier : public IBattleModifier {};     // 敏捷降低（桩）

 class PlayerFrailModifier : public IBattleModifier {               // 脆弱：在 X 回合内，从卡牌获得的格挡减少 25%
 public:
     void on_player_gain_block(int& block, const BattleState& state) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "frail");
         if (stacks <= 0) return;
         block = block * 3 / 4;                                       // 减少 25%
     }
 };
 
class PlayerCannotDrawModifier : public IBattleModifier {};       // 无法抽牌：由 BattleEngine::draw_cards_impl 统一拦截“额外抽牌”
 class PlayerFlexModifier : public IBattleModifier {};              // 灵活（桩）
 class PlayerDeviationModifier : public IBattleModifier {};         // 偏差（桩）

 class PlayerEntangleModifier : public IBattleModifier {           // 缠身：本回合不能打出攻击牌
 public:
     void on_can_play_card(CanPlayCardContext& ctx) override {
         int stacks = BattleEngine::get_status_stacks(ctx.state.player.statuses, "entangle");
         if (stacks <= 0 || !ctx.is_attack) return;
         ctx.blocked = true;                                         // 禁止打出攻击牌
     }
 };
 
 class PlayerDrawReductionModifier : public IBattleModifier {       // 抽牌减少：回合开始少抽 1 张
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx) return;
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "draw_reduction");
         if (stacks <= 0) return;
         ctx->draw_count -= 1;                                       // 抽牌数 -1
         BattleEngine::reduce_status_stacks(state.player.statuses, "draw_reduction", 1);  // 层数 -1
     }
 };

 class PlayerHauntedModifier : public IBattleModifier {};           // 闹鬼（桩）
 class PlayerFastingModifier : public IBattleModifier {             // 禁食：回合开始减能量
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx) return;
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "fasting");
         if (stacks <= 0) return;
         ctx->energy -= stacks;                                      // 能量减少
         if (ctx->energy < 0) ctx->energy = 0;                      // 不低于 0
     }
 };
 
 class PlayerCurseModifier : public IBattleModifier {};             // 诅咒（桩）
 class PlayerCannotBlockModifier : public IBattleModifier {};        // 无法格挡（桩）
class PlayerWraithFormModifier : public IBattleModifier {           // 幽魂形态：回合结束失去 1 点敏捷
 public:
   void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "wraith_form");
         if (stacks <= 0) return;
        BattleEngine::reduce_status_stacks(state.player.statuses, "dexterity", 1);
     }
 };
 
 // ========== 负面 - 敌方 ==========
 class MonsterRepelWithHandModifier : public IBattleModifier {};     // 推手（桩）
 class MonsterPoisonModifier : public IBattleModifier {              // 怪物中毒：敌方回合开始扣血
 public:
     void on_turn_start_monsters(BattleState& state, EnemyTurnContext* ctx) override {
         if (!ctx || !ctx->deal_damage_to_monster_ignoring_block) return;
         for (size_t i = 0; i < state.monsters.size(); ++i) {
             if (state.monsters[i].currentHp <= 0) continue;        // 已死跳过
             int stacks = BattleEngine::get_status_stacks(state.monsters[i].statuses, "poison");
             if (stacks <= 0) continue;
             ctx->deal_damage_to_monster_ignoring_block(static_cast<int>(i), stacks);
             BattleEngine::reduce_status_stacks(state.monsters[i].statuses, "poison", 1);
         }
     }
 };
 class MonsterSlowModifier : public IBattleModifier {                // 缓慢：玩家对目标伤害 +10%/层
 public:
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         int idx = dmg.target_monster_index;
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
         int stacks = BattleEngine::get_status_stacks(state.monsters[idx].statuses, "slow");
         if (stacks <= 0) return;
         int bonus = dmg.modified_amount * stacks / 10;              // 每层 +10% 伤害
         dmg.modified_amount += bonus;
     }
 };
 class MonsterShacklesModifier : public IBattleModifier {            // 镣铐：敌方回合结束减力量
 public:
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
         for (size_t i = 0; i < state.monsters.size(); ++i) {
             int shackles = BattleEngine::get_status_stacks(state.monsters[i].statuses, "shackles");
             if (shackles <= 0) continue;
             BattleEngine::reduce_status_stacks(state.monsters[i].statuses, "strength", shackles);  // 力量减镣铐层数
         }
     }
 };
 class MonsterChokeModifier : public IBattleModifier {               // 勒脖：打出牌时对目标造成层数伤害
 public:
     void on_card_played(BattleState& state, CardId /*card_id*/, int target_monster_index, CardPlayContext* ctx) override {
         if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state.monsters.size()) return;
         if (!ctx || !ctx->deal_damage_to_monster_ignoring_block) return;
         int stacks = BattleEngine::get_status_stacks(state.monsters[target_monster_index].statuses, "choke");
         if (stacks <= 0) return;
         ctx->deal_damage_to_monster_ignoring_block(target_monster_index, stacks);
     }
 };
class MonsterCorpseExplosionModifier : public IBattleModifier {     // 尸体爆炸：怪物死亡时对全体造成 maxHp×层数伤害
public:
    void on_monster_died(int monsterIndex, BattleState& state, MonsterDiedContext* ctx) override {
        if (!ctx || !ctx->deal_damage_to_monster_ignoring_block) return;
        if (ctx->from_corpse_explosion) return;                     // 防止递归
        if (monsterIndex < 0 || static_cast<size_t>(monsterIndex) >= state.monsters.size()) return;
        const auto& dead = state.monsters[monsterIndex];
         int stacks = BattleEngine::get_status_stacks(dead.statuses, "corpse_explosion");
         if (stacks <= 0) return;
         int damage = dead.maxHp * stacks;                           // 伤害 = 最大生命 × 层数
         if (damage <= 0) return;
         ctx->from_corpse_explosion = true;                           // 标记防止递归
         for (size_t i = 0; i < state.monsters.size(); ++i) {
             if (state.monsters[i].currentHp <= 0) continue;
             ctx->deal_damage_to_monster_ignoring_block(static_cast<int>(i), damage);
         }
         ctx->from_corpse_explosion = false;
     }
 };
 // ========== 负面 - 通用 ==========
 class StrengthDownModifier : public IBattleModifier {               // 力量降低：伤害 - 层数
 public:
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "strength_down");
         if (stacks <= 0) return;
         dmg.modified_amount -= stacks;
         if (dmg.modified_amount < 0) dmg.modified_amount = 0;
     }
     void on_monster_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         int idx = dmg.source_monster_index;
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
         int stacks = BattleEngine::get_status_stacks(state.monsters[idx].statuses, "strength_down");
         if (stacks <= 0) return;
         dmg.modified_amount -= stacks;
         if (dmg.modified_amount < 0) dmg.modified_amount = 0;
     }
 };
 class WeakModifier : public IBattleModifier {                       // 虚弱：攻击伤害 ×0.75
 public:
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         if (!dmg.from_attack) return;                               // 仅攻击牌
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "weak");
         if (stacks <= 0) return;
         dmg.modified_amount = dmg.modified_amount * 3 / 4;
     }
     void on_monster_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         if (!dmg.from_attack) return;
         int idx = dmg.source_monster_index;
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
         int stacks = BattleEngine::get_status_stacks(state.monsters[idx].statuses, "weak");
         if (stacks <= 0) return;
         dmg.modified_amount = dmg.modified_amount * 3 / 4;
     }
 };
 class VulnerableModifier : public IBattleModifier {                 // 易伤：受到攻击伤害 ×1.5
 public:
     void on_monster_deal_damage(DamagePacket& dmg, const BattleState& state) override {  // 怪物打玩家
         if (!dmg.from_attack) return;
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "vulnerable");  // 玩家易伤
         if (stacks <= 0) return;
         dmg.modified_amount = dmg.modified_amount * 3 / 2;
     }
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {  // 玩家打怪物
         if (!dmg.from_attack) return;
         int idx = dmg.target_monster_index;
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
         int stacks = BattleEngine::get_status_stacks(state.monsters[idx].statuses, "vulnerable");  // 怪物易伤
         if (stacks <= 0) return;
         dmg.modified_amount = dmg.modified_amount * 3 / 2;
     }
 };
 
 // ========== 正面 - 敌方（桩） ==========
 class MonsterDeathRhythmModifier : public IBattleModifier {};       // 死亡节奏（桩）
 class MonsterCuriosityModifier : public IBattleModifier {};         // 好奇（桩）
 class MonsterAngerModifier : public IBattleModifier {};            // 愤怒（桩）
 class MonsterDelicateBodyModifier : public IBattleModifier {};      // 脆弱之躯（桩）
 class MonsterRearAttackModifier : public IBattleModifier {};        // 后击（桩）
 class MonsterFlightModifier : public IBattleModifier {};            // 飞行（桩）
 class MonsterVanishModifier : public IBattleModifier {};           // 消失（桩）
 class MonsterEnrageModifier : public IBattleModifier {};            // 激怒（桩）
 class MonsterSelfDestructModifier : public IBattleModifier {};      // 自爆（桩）
 class MonsterLifeLinkModifier : public IBattleModifier {};          // 生命链接（桩）
 class MonsterMinionModifier : public IBattleModifier {};            // 仆从（桩）
 class MonsterResilienceModifier : public IBattleModifier {};        // 韧性（桩）

 class RegenerateModifier : public IBattleModifier {                   // 再生（玩家）：其回合结束时回复 X 点生命，层数每回合 -1
    public:
       void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
            int stacks = BattleEngine::get_status_stacks(state.player.statuses, "regeneration");
            if (stacks <= 0) return;                                      // 无再生层数则跳过
            state.player.currentHp += stacks;                             // 回复生命（层数 = 回复量）
            if (state.player.currentHp > state.player.maxHp)
                state.player.currentHp = state.player.maxHp;             // 不超过最大生命
            BattleEngine::reduce_status_stacks(state.player.statuses, "regeneration", 1);  // 玩家层数正常减少
        }
    };

class MonsterRegenerationModifier : public IBattleModifier {           // 再生（怪物）：其回合结束时回复 X 点生命，层数不减少
public:
    void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
        for (size_t i = 0; i < state.monsters.size(); ++i) {          // 遍历所有怪物
            if (state.monsters[i].currentHp <= 0) continue;           // 已死跳过
            int stacks = BattleEngine::get_status_stacks(state.monsters[i].statuses, "regeneration");
            if (stacks <= 0) continue;                                 // 无再生层数则跳过
            state.monsters[i].currentHp += stacks;                    // 回复生命
            if (state.monsters[i].currentHp > state.monsters[i].maxHp)
                state.monsters[i].currentHp = state.monsters[i].maxHp; // 不超过最大生命
            // 怪物层数不减少，duration 由 DurationTickModifier 正常递减
        }
    }
};

 class MonsterIndestructibleModifier : public IBattleModifier {};   // 不可摧毁（桩）
 class MonsterSharpCarapaceModifier : public IBattleModifier {};     // 尖刺甲壳（桩）
 class MonsterFormShiftModifier : public IBattleModifier {};         // 形态转换（桩）
 class MonsterTransformModifier : public IBattleModifier {};         // 变形（桩）
 class MonsterSplitModifier : public IBattleModifier {};             // 分裂（桩）
 class MonsterStasisModifier : public IBattleModifier {};            // 静滞（桩）

class StrengthBoostModifier : public IBattleModifier {                 // 力量增益：每回合结束时获得 X 点力量
public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
        int stacks = BattleEngine::get_status_stacks(state.player.statuses, "strength_boost");
        if (stacks <= 0) return;
        add_strength_stacks(state.player.statuses, stacks);
    }
    void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
        for (auto& m : state.monsters) {
            int stacks = BattleEngine::get_status_stacks(m.statuses, "strength_boost");
            if (stacks <= 0) continue;
            add_strength_stacks(m.statuses, stacks);
        }
    }
};
 
 class MonsterSporeCloudModifier : public IBattleModifier {          // 孢子云：死亡时给予玩家 X 回合易伤
 public:
     void on_monster_died(int monsterIndex, BattleState& state, MonsterDiedContext* /*ctx*/) override {
         if (monsterIndex < 0 || static_cast<size_t>(monsterIndex) >= state.monsters.size()) return;  // 下标越界则跳过
         const auto& dead = state.monsters[static_cast<size_t>(monsterIndex)];                        // 死亡怪物
         int x = BattleEngine::get_status_stacks(dead.statuses, "spore_cloud");                      // 孢子云层数 = 易伤回合数
         if (x <= 0) return;                                                                        // 无孢子云则跳过
         auto& list = state.player.statuses;                                                         // 玩家状态列表
         for (auto& s : list) {
             if (s.id == "vulnerable") {
                 s.stacks += 1;                                                                     // 易伤层数 +1
                 if (x > 0 && (s.duration < 0 || x > s.duration)) s.duration = x;                    // 取较长持续回合
                 return;
             }
         }
         list.push_back(StatusInstance{"vulnerable", 1, x});                                         // 新建易伤：1 层，持续 X 回合
     }
 };
 
 class MonsterTwistModifier : public IBattleModifier {};             // 扭曲（桩）
 class MonsterStealModifier : public IBattleModifier {};              // 窃取（桩）
 class MonsterTimeWarpModifier : public IBattleModifier {};          // 时间扭曲（桩）
 class MonsterUnawakenedModifier : public IBattleModifier {};        // 未觉醒（桩）
 class MonsterPainfulJabModifier : public IBattleModifier {};        // 痛刺（桩）
 
 // ========== 正面 - 通用 ==========
 class ArtifactModifier : public IBattleModifier {                  // 人工制品：免疫 X 次负面效果（按杀戮尖塔规则）
 public:
     void on_before_status_applied(StatusApplyContext& ctx) override {
         if (ctx.blocked) return;                                  // 已被其他 modifier 阻止则跳过
         if (!is_negative_status(ctx.id)) return;                  // 仅对负面效果生效
         int artifact = get_artifact_stacks(ctx);                   // 获取目标的人工制品层数
         if (artifact <= 0) return;                                 // 无人工制品则无法免疫
         ctx.blocked = true;                                        // 阻止本次负面效果施加
         consume_artifact(ctx, 1);                                 // 消耗 1 层人工制品
     }
 private:
     static bool is_negative_status(const StatusId& id) {            // 判断是否为负面效果（可被人工制品免疫）
         static const char* const negative[] = {                     // 杀戮尖塔 wiki：人工制品可阻挡的负面效果
             "weak", "vulnerable", "strength_down", "dexterity_down", "poison", "frail",
             "draw_reduction", "fasting", "confusion", "entangle", "choke", "slow",
            "shackles", "corpse_explosion", "wraith_form"
         };
         for (const auto* n : negative) if (id == n) return true;
         return false;
     }
     static int get_artifact_stacks(const StatusApplyContext& ctx) {
         if (ctx.target == StatusApplyContext::Target::Player)
             return BattleEngine::get_status_stacks(ctx.state.player.statuses, "artifact");
         if (ctx.monster_index >= 0 && ctx.monster_index < static_cast<int>(ctx.state.monsters.size()))
             return BattleEngine::get_status_stacks(ctx.state.monsters[static_cast<size_t>(ctx.monster_index)].statuses, "artifact");
         return 0;
     }
     static void consume_artifact(StatusApplyContext& ctx, int amount) {
         if (ctx.target == StatusApplyContext::Target::Player)
             BattleEngine::reduce_status_stacks(ctx.state.player.statuses, "artifact", amount);
         else if (ctx.monster_index >= 0 && ctx.monster_index < static_cast<int>(ctx.state.monsters.size()))
             BattleEngine::reduce_status_stacks(ctx.state.monsters[static_cast<size_t>(ctx.monster_index)].statuses, "artifact", amount);
     }
 };
 class BarricadeModifier : public IBattleModifier {};                // 壁垒（桩）
class IntangibleModifier : public IBattleModifier {                // 无实体：受到伤害时最多为 1
public:
    void on_monster_deal_damage(DamagePacket& dmg, const BattleState& state) override {  // 怪物打玩家
        int stacks = BattleEngine::get_status_stacks(state.player.statuses, "intangible");
        if (stacks <= 0) return;
        if (dmg.modified_amount > 1) dmg.modified_amount = 1;
    }
    void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {   // 玩家打怪物
        int idx = dmg.target_monster_index;
        if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
        int stacks = BattleEngine::get_status_stacks(state.monsters[static_cast<size_t>(idx)].statuses, "intangible");
        if (stacks <= 0) return;
        if (dmg.modified_amount > 1) dmg.modified_amount = 1;
    }
};
 class MetallicizeModifier : public IBattleModifier {                // 金属化：玩家回合结束加格挡
 public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "metallicize");
         if (stacks > 0) state.player.block += stacks;
     }
 };
 class MultiArmorModifier : public IBattleModifier {                 // 多层护甲：回合结束加格挡（玩家+怪物）
 public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "multi_armor");
         if (stacks > 0) state.player.block += stacks;
     }
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
         for (size_t i = 0; i < state.monsters.size(); ++i) {
             int stacks = BattleEngine::get_status_stacks(state.monsters[i].statuses, "multi_armor");
             if (stacks > 0) state.monsters[i].block += stacks;
         }
     }
 };
 class RitualModifier : public IBattleModifier {                     // 仪式：回合结束加力量（玩家+怪物）
 public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "ritual");
         if (stacks <= 0) return;
         add_strength_stacks(state.player.statuses, stacks);
     }
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
         for (auto& m : state.monsters) {
             int stacks = BattleEngine::get_status_stacks(m.statuses, "ritual");
             if (stacks <= 0) continue;
             add_strength_stacks(m.statuses, stacks);
         }
     }
 };
 
 class StrengthModifier : public IBattleModifier {                   // 力量：玩家伤害 + 层数（按玩家 strength 动态创建）
 public:
     explicit StrengthModifier(int stacks) : stacks_(stacks) {}
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& /*state*/) override {
         if (stacks_ <= 0) return;
         dmg.modified_amount += stacks_;
     }
 private:
     int stacks_ = 0;
 };
 class ThornsModifier : public IBattleModifier {                   // 荆棘：每受到一次攻击，对攻击者造成 X 点反伤
 public:
     void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) override {
         if (!ctx || !ctx->damage_to_player) return;               // 仅当玩家受到伤害时触发
         if (!dmg.from_attack) return;                             // 仅对攻击类伤害生效（排除中毒、勒脖等）
         if (dmg.source_type != DamagePacket::SourceType::Monster) return;  // 仅怪物攻击
         if (dmg.source_monster_index < 0 || dmg.source_monster_index >= static_cast<int>(state.monsters.size())) return;  // 攻击者下标无效
         if (state.monsters[static_cast<size_t>(dmg.source_monster_index)].currentHp <= 0) return;  // 攻击者已死则跳过
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "thorns");
         if (stacks <= 0) return;                                  // 无荆棘层数则跳过
         if (!ctx->deal_damage_to_monster_ignoring_block) return;   // 无回调则无法反伤
         ctx->deal_damage_to_monster_ignoring_block(dmg.source_monster_index, stacks);  // 对攻击者造成 X 点无视格挡伤害
     }
 };
 
 // ========== 正面 - 玩家 ==========
 class PlayerBufferModifier : public IBattleModifier {};             // 缓冲（桩）
 class PlayerDexterityModifier : public IBattleModifier {};           // 敏捷（桩）
 class PlayerDrawCardModifier : public IBattleModifier {              // 抽牌+：回合开始多抽 X 张
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx) return;
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "draw_up");
         if (stacks <= 0) return;
         ctx->draw_count += stacks;                                  // 抽牌数 + 层数
         BattleEngine::reduce_status_stacks(state.player.statuses, "draw_up", stacks);  // 一次性消耗
     }
 };
 class PlayerEnergyUpModifier : public IBattleModifier {              // 能量+：回合开始多 X 能量
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx) return;
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "energy_up");
         if (stacks <= 0) return;
         ctx->energy += stacks;
         BattleEngine::reduce_status_stacks(state.player.statuses, "energy_up", stacks);
     }
 };
 class PlayerVigorModifier : public IBattleModifier {};               // 活力（桩）

 class PlayerBlockUpModifier : public IBattleModifier {               // 格挡+：回合开始加格挡
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* /*ctx*/) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "block_up");
         if (stacks <= 0) return;
         state.player.block += stacks;
         BattleEngine::reduce_status_stacks(state.player.statuses, "block_up", stacks);
     }
 };
 class PlayerMantraModifier : public IBattleModifier {};              // 真言（桩）
 class PlayerAccuracyModifier : public IBattleModifier {};           // 精准（桩）
 class PlayerAmplifyModifier : public IBattleModifier {};             // 增幅（桩）
class PlayerAfterimageModifier : public IBattleModifier {           // 残像：每打出一张牌获得 1 点格挡/层
public:
    void on_card_played(BattleState& state, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* /*ctx*/) override {
        int stacks = BattleEngine::get_status_stacks(state.player.statuses, "after_image");
        if (stacks <= 0) return;
        state.player.block += stacks;
    }
};
 class PlayerBattleHymnModifier : public IBattleModifier {};          // 战歌（桩）
 class PlayerBlasphemyModifier : public IBattleModifier {};          // 渎神（桩）
 class PlayerShadowModifier : public IBattleModifier {};              // 暗影（桩）
 class PlayerCollectModifier : public IBattleModifier {};            // 收集（桩）
 class PlayerBurstModifier : public IBattleModifier {};               // 爆发（桩）
 class PlayerBerserkModifier : public IBattleModifier {};             // 狂暴（桩）
 class PlayerBrutalityModifier : public IBattleModifier {};           // 残暴（桩）
 class PlayerCombustModifier : public IBattleModifier {};             // 自燃（桩）
 class PlayerCorruptionModifier : public IBattleModifier {};          // 腐化（桩）
 class PlayerCreativeAIModifier : public IBattleModifier {};          // 创意 AI（桩）
 class PlayerDarkEmbraceModifier : public IBattleModifier {};        // 黑暗之拥（桩）
 
 // ========== 正面 - 唯一（桩） ==========
 class ForesightModifier : public IBattleModifier {};                // 预见（桩）
 class DuplicateModifier : public IBattleModifier {};                // 复制（桩）
 class DoubleDamageModifier : public IBattleModifier {};             // 双倍伤害（桩）
 class DoubleTapModifier : public IBattleModifier {};                // 双发（桩）
 class DevotionModifier : public IBattleModifier {};                  // 虔诚（桩）
 class DemonFormModifier : public IBattleModifier {};                // 恶魔形态（桩）
 class CelestialModifier : public IBattleModifier {};                // 天界（桩）
 class EstablishmentModifier : public IBattleModifier {};            // 建立（桩）
 class PoisonModifier : public IBattleModifier {};                   // 毒（桩）
 class EquilibriumModifier : public IBattleModifier {};               // 均衡（桩）
 class EchoFormModifier : public IBattleModifier {};                 // 回响形态（桩）
 class ElectrodynamicsModifier : public IBattleModifier {};          // 电动力学（桩）
 class FeelNoPainModifier : public IBattleModifier {};               // 无痛（桩）
 class EvolveModifier : public IBattleModifier {};                   // 进化（桩）
class FlameBarrierModifier : public IBattleModifier {                // 火焰屏障：玩家被攻击时对攻击者造成层数反伤
public:
    void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) override {
        if (!ctx || !ctx->damage_to_player || !ctx->deal_damage_to_monster_ignoring_block) return;
        if (!dmg.from_attack) return;
        if (dmg.source_type != DamagePacket::SourceType::Monster) return;
        if (dmg.source_monster_index < 0 || dmg.source_monster_index >= static_cast<int>(state.monsters.size())) return;
        if (state.monsters[static_cast<size_t>(dmg.source_monster_index)].currentHp <= 0) return;
        int stacks = BattleEngine::get_status_stacks(state.player.statuses, "flame_barrier");
        if (stacks <= 0) return;
        ctx->deal_damage_to_monster_ignoring_block(dmg.source_monster_index, stacks);
    }
};
 class HeatSinkModifier : public IBattleModifier {};                 // 散热片（桩）
 class FreeAttackModifier : public IBattleModifier {};               // 免费攻击（桩）
 class FireBreathModifier : public IBattleModifier {};                // 火焰吐息（桩）
 class LikeWaterModifier : public IBattleModifier {};                // 似水（桩）
 class HelloModifier : public IBattleModifier {};                    // 你好（桩）
 class InfiniteBladesModifier : public IBattleModifier {};           // 无限之刃（桩）
 class LoopModifier : public IBattleModifier {};                      // 循环（桩）
 class UnstoppableModifier : public IBattleModifier {};               // 势不可挡（桩）
 class NightmareModifier : public IBattleModifier {};                 // 梦魇（桩）
 class MagnetismModifier : public IBattleModifier {};                // 磁力（桩）
 class RealityManipulationModifier : public IBattleModifier {};     // 现实操纵（桩）
 class MindFortressModifier : public IBattleModifier {};             // 心灵堡垒（桩）
 class MachineLearningModifier : public IBattleModifier {};         // 机器学习（桩）
 class ConfidentVictoryModifier : public IBattleModifier {};         // 自信胜利（桩）
 class NirvanaModifier : public IBattleModifier {};                  // 涅槃（桩）
class PoisonCloudModifier : public IBattleModifier {                 // 毒雾：玩家回合开始时，对所有敌人施加中毒
public:
    void on_turn_start_player(BattleState& state, TurnStartContext* /*ctx*/) override {
        int stacks = BattleEngine::get_status_stacks(state.player.statuses, "poison_cloud");
        if (stacks <= 0) return;
        for (auto& m : state.monsters) {
            if (m.currentHp <= 0) continue;
            bool merged = false;
            for (auto& s : m.statuses) {
                if (s.id == "poison") {
                    s.stacks += stacks;
                    if (s.duration < stacks) s.duration = stacks;
                    merged = true;
                    break;
                }
            }
            if (!merged) m.statuses.push_back(StatusInstance{"poison", stacks, stacks});
        }
    }
};
 class PhantomModifier : public IBattleModifier {};                  // 幻影（桩）
 class PenNibModifier : public IBattleModifier {};                   // 笔尖（桩）
 class RicochetModifier : public IBattleModifier {};                 // 弹跳（桩）
 class RageModifier : public IBattleModifier {};                     // 愤怒（桩）

 class RepairModifier : public IBattleModifier {                     // 修理：战斗结束时回复 X 点生命（如自我修复）
    public:
        void on_monster_died(int monsterIndex, BattleState& state, MonsterDiedContext* /*ctx*/) override {
            (void)monsterIndex;                                        // 不关心具体哪个怪物死亡
            if (state.player.currentHp <= 0) return;                   // 玩家已死则不回复
            bool all_dead = true;                                      // 检查是否所有怪物都已死亡
            for (const auto& m : state.monsters) {
                if (m.currentHp > 0) { all_dead = false; break; }
            }
            if (!all_dead) return;                                     // 战斗未结束则跳过
            int repair = BattleEngine::get_status_stacks(state.player.statuses, "repair");
            if (repair <= 0) return;                                  // 无修理层数则跳过
            state.player.currentHp += repair;                          // 回复生命
            if (state.player.currentHp > state.player.maxHp)
                state.player.currentHp = state.player.maxHp;           // 不超过最大生命
            BattleEngine::reduce_status_stacks(state.player.statuses, "repair", repair);  // 消耗修理层数（一次性）
        }
    };
 
 class TigerDescendsModifier : public IBattleModifier {};            // 虎落（桩）
 class RuptureModifier : public IBattleModifier {};                  // 破裂（桩）
 class ThunderstormModifier : public IBattleModifier {};             // 雷暴（桩）
 class StaticDischargeModifier : public IBattleModifier {};         // 静电释放（桩）
 class CrueltyModifier : public IBattleModifier {};                  // 残忍（桩）
 class StudyModifier : public IBattleModifier {};                    // 学习（桩）
class BombModifier : public IBattleModifier {                      // 炸弹：玩家回合结束时，倒计时到 1 则对全体造成 X 伤害
public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) override {
        if (!ctx || !ctx->deal_damage_to_monster_ignoring_block) return;
        int bomb_damage = 0;
        for (const auto& s : state.player.statuses) {
            if (s.id == "the_bomb" && s.duration == 1) {
                bomb_damage += s.stacks;
            }
        }
        if (bomb_damage <= 0) return;
        for (size_t i = 0; i < state.monsters.size(); ++i) {
            if (state.monsters[i].currentHp <= 0) continue;
            ctx->deal_damage_to_monster_ignoring_block(static_cast<int>(i), bomb_damage);
        }
    }
};
 class MeleeModifier : public IBattleModifier {};                    // 近战（桩）
 class LingchiModifier : public IBattleModifier {};                  // 凌迟（桩）
 class EssentialToolsModifier : public IBattleModifier {};           // 必备工具（桩）
 class WaveHandModifier : public IBattleModifier {};                 // 挥手（桩）
 class WellPlannedModifier : public IBattleModifier {};              // 周密计划（桩）
 class FlankAttackModifier : public IBattleModifier {};              // 侧翼攻击（桩）
 
 // ========== 系统级 ==========
 class MonsterBlockClearModifier : public IBattleModifier {          // 怪物格挡清零：无壁垒则回合开始清空
 public:
     void on_turn_start_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
         for (auto& m : state.monsters) {
             if (BattleEngine::get_status_stacks(m.statuses, "barricade") <= 0)
                 m.block = 0;
         }
     }
 };
 class DurationTickModifier : public IBattleModifier {               // 持续回合递减：敌方回合结束 duration-1，移除为 0 的；duration=-1 表示永久不递减
 public:
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
        for (auto& s : state.player.statuses) {
            if (s.id == "draw_up" || s.id == "energy_up" || s.id == "block_up") continue;  // 下回合多抽/多能量/多格挡：在玩家回合开始时被消耗，不在此按 duration 过期
            if (s.duration > 0) {                                // 每回合 -1，仅对 duration>0 的生效
                --s.duration;
                // 对于虚弱/易伤/脆弱这类「层数 = 持续时间」的减益，层数也同步递减，
               if ((s.id == "weak" || s.id == "vulnerable" || s.id == "frail" || s.id == "intangible") && s.stacks > 0) {
                    --s.stacks;
                }
            }
        }
         state.player.statuses.erase(
             std::remove_if(state.player.statuses.begin(), state.player.statuses.end(),
                 [](const StatusInstance& x) {
                     if (x.id == "draw_up" || x.id == "energy_up" || x.id == "block_up") return false;  // 仅由对应 modifier 消耗后移除
                     return x.duration == 0;
                 }),
             state.player.statuses.end());
         for (auto& m : state.monsters) {
            for (auto& s : m.statuses) {
                if (s.duration > 0) {                             // 每回合 -1
                    --s.duration;
                   if ((s.id == "weak" || s.id == "vulnerable" || s.id == "frail" || s.id == "intangible") && s.stacks > 0) {
                        --s.stacks;
                    }
                }
            }
             m.statuses.erase(
                 std::remove_if(m.statuses.begin(), m.statuses.end(),
                     [](const StatusInstance& x) { return x.duration == 0; }),
                 m.statuses.end());
         }
     }
 };
 class PlayerBlockClearModifier : public IBattleModifier {           // 玩家格挡清零：无壁垒则敌方回合结束清空
 public:
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
         if (BattleEngine::get_status_stacks(state.player.statuses, "barricade") <= 0)
             state.player.block = 0;
     }
 };
 
 // ========== 工厂函数 ==========
 std::vector<std::shared_ptr<IBattleModifier>>
 create_player_status_modifiers(const PlayerBattleState& player) {   // 创建玩家相关 modifier 列表
     std::vector<std::shared_ptr<IBattleModifier>> out;
     for (const auto& s : player.statuses) {
         if (s.id == "strength")
             out.push_back(std::make_shared<StrengthModifier>(s.stacks));  // 力量按层数创建
     }
     out.push_back(std::make_shared<PlayerPoisonModifier>());
     out.push_back(std::make_shared<PlayerConfusionModifier>());
     out.push_back(std::make_shared<PlayerDexterityDownModifier>());
     out.push_back(std::make_shared<PlayerFrailModifier>());
     out.push_back(std::make_shared<PlayerCannotDrawModifier>());
     out.push_back(std::make_shared<PlayerFlexModifier>());
     out.push_back(std::make_shared<PlayerDeviationModifier>());
     out.push_back(std::make_shared<PlayerEntangleModifier>());
     out.push_back(std::make_shared<PlayerDrawReductionModifier>());
     out.push_back(std::make_shared<PlayerHauntedModifier>());
     out.push_back(std::make_shared<PlayerFastingModifier>());
     out.push_back(std::make_shared<PlayerCurseModifier>());
     out.push_back(std::make_shared<PlayerCannotBlockModifier>());
     out.push_back(std::make_shared<PlayerWraithFormModifier>());
     out.push_back(std::make_shared<PlayerBufferModifier>());
     out.push_back(std::make_shared<PlayerDexterityModifier>());
     out.push_back(std::make_shared<PlayerDrawCardModifier>());
     out.push_back(std::make_shared<PlayerEnergyUpModifier>());
     out.push_back(std::make_shared<PlayerVigorModifier>());
     out.push_back(std::make_shared<PlayerBlockUpModifier>());
     out.push_back(std::make_shared<PlayerMantraModifier>());
     out.push_back(std::make_shared<PlayerAccuracyModifier>());
     out.push_back(std::make_shared<PlayerAmplifyModifier>());
     out.push_back(std::make_shared<PlayerAfterimageModifier>());
     out.push_back(std::make_shared<PlayerBattleHymnModifier>());
     out.push_back(std::make_shared<PlayerBlasphemyModifier>());
     out.push_back(std::make_shared<PlayerShadowModifier>());
     out.push_back(std::make_shared<PlayerCollectModifier>());
     out.push_back(std::make_shared<PlayerBurstModifier>());
     out.push_back(std::make_shared<PlayerBerserkModifier>());
     out.push_back(std::make_shared<PlayerBrutalityModifier>());
     out.push_back(std::make_shared<PlayerCombustModifier>());
     out.push_back(std::make_shared<PlayerCorruptionModifier>());
     out.push_back(std::make_shared<PlayerCreativeAIModifier>());
     out.push_back(std::make_shared<PlayerDarkEmbraceModifier>());
     out.push_back(std::make_shared<ArtifactModifier>());           // 人工制品：玩家可免疫负面效果
     out.push_back(std::make_shared<ThornsModifier>());            // 荆棘：受到攻击时对攻击者造成反伤
     out.push_back(std::make_shared<ForesightModifier>());
     out.push_back(std::make_shared<DuplicateModifier>());
     out.push_back(std::make_shared<DoubleDamageModifier>());
     out.push_back(std::make_shared<DoubleTapModifier>());
     out.push_back(std::make_shared<DevotionModifier>());
     out.push_back(std::make_shared<DemonFormModifier>());
     out.push_back(std::make_shared<CelestialModifier>());
     out.push_back(std::make_shared<EstablishmentModifier>());
     out.push_back(std::make_shared<PoisonModifier>());
     out.push_back(std::make_shared<EquilibriumModifier>());
     out.push_back(std::make_shared<EchoFormModifier>());
     out.push_back(std::make_shared<ElectrodynamicsModifier>());
     out.push_back(std::make_shared<FeelNoPainModifier>());
     out.push_back(std::make_shared<EvolveModifier>());
     out.push_back(std::make_shared<FlameBarrierModifier>());
     out.push_back(std::make_shared<HeatSinkModifier>());
     out.push_back(std::make_shared<FreeAttackModifier>());
     out.push_back(std::make_shared<FireBreathModifier>());
     out.push_back(std::make_shared<LikeWaterModifier>());
     out.push_back(std::make_shared<HelloModifier>());
     out.push_back(std::make_shared<InfiniteBladesModifier>());
     out.push_back(std::make_shared<LoopModifier>());
     out.push_back(std::make_shared<UnstoppableModifier>());
     out.push_back(std::make_shared<NightmareModifier>());
     out.push_back(std::make_shared<MagnetismModifier>());
     out.push_back(std::make_shared<RealityManipulationModifier>());
     out.push_back(std::make_shared<MindFortressModifier>());
     out.push_back(std::make_shared<MachineLearningModifier>());
     out.push_back(std::make_shared<ConfidentVictoryModifier>());
     out.push_back(std::make_shared<NirvanaModifier>());
     out.push_back(std::make_shared<PoisonCloudModifier>());
     out.push_back(std::make_shared<PhantomModifier>());
     out.push_back(std::make_shared<PenNibModifier>());
     out.push_back(std::make_shared<RicochetModifier>());
     out.push_back(std::make_shared<RageModifier>());
     out.push_back(std::make_shared<RegenerateModifier>());
     out.push_back(std::make_shared<RepairModifier>());
     out.push_back(std::make_shared<TigerDescendsModifier>());
     out.push_back(std::make_shared<RuptureModifier>());
     out.push_back(std::make_shared<ThunderstormModifier>());
     out.push_back(std::make_shared<StaticDischargeModifier>());
     out.push_back(std::make_shared<CrueltyModifier>());
     out.push_back(std::make_shared<StudyModifier>());
     out.push_back(std::make_shared<BombModifier>());
     out.push_back(std::make_shared<MeleeModifier>());
     out.push_back(std::make_shared<LingchiModifier>());
     out.push_back(std::make_shared<EssentialToolsModifier>());
     out.push_back(std::make_shared<WaveHandModifier>());
     out.push_back(std::make_shared<WellPlannedModifier>());
     out.push_back(std::make_shared<FlankAttackModifier>());
     return out;
 }
 
 std::vector<std::shared_ptr<IBattleModifier>>
 create_monster_status_modifiers(const std::vector<MonsterInBattle>& /*monsters*/) {  // 创建怪物相关 modifier 列表
     std::vector<std::shared_ptr<IBattleModifier>> out;
     out.push_back(std::make_shared<MonsterRepelWithHandModifier>());
     out.push_back(std::make_shared<MonsterPoisonModifier>());
     out.push_back(std::make_shared<MonsterSlowModifier>());
     out.push_back(std::make_shared<MonsterShacklesModifier>());
     out.push_back(std::make_shared<MonsterChokeModifier>());
     out.push_back(std::make_shared<MonsterCorpseExplosionModifier>());
     out.push_back(std::make_shared<StrengthDownModifier>());
     out.push_back(std::make_shared<WeakModifier>());
     out.push_back(std::make_shared<VulnerableModifier>());
     out.push_back(std::make_shared<MonsterDeathRhythmModifier>());
     out.push_back(std::make_shared<MonsterCuriosityModifier>());
     out.push_back(std::make_shared<MonsterAngerModifier>());
     out.push_back(std::make_shared<MonsterDelicateBodyModifier>());
     out.push_back(std::make_shared<MonsterRearAttackModifier>());
     out.push_back(std::make_shared<MonsterFlightModifier>());
     out.push_back(std::make_shared<MonsterVanishModifier>());
     out.push_back(std::make_shared<MonsterEnrageModifier>());
     out.push_back(std::make_shared<MonsterSelfDestructModifier>());
     out.push_back(std::make_shared<MonsterLifeLinkModifier>());
     out.push_back(std::make_shared<MonsterMinionModifier>());
     out.push_back(std::make_shared<MonsterResilienceModifier>());
     out.push_back(std::make_shared<MonsterRegenerationModifier>());
     out.push_back(std::make_shared<MonsterIndestructibleModifier>());
     out.push_back(std::make_shared<MonsterSharpCarapaceModifier>());
     out.push_back(std::make_shared<MonsterFormShiftModifier>());
     out.push_back(std::make_shared<MonsterTransformModifier>());
     out.push_back(std::make_shared<MonsterSplitModifier>());
     out.push_back(std::make_shared<MonsterStasisModifier>());
     out.push_back(std::make_shared<StrengthBoostModifier>());       // 力量增益（玩家+怪物共用）
     out.push_back(std::make_shared<MonsterSporeCloudModifier>());
     out.push_back(std::make_shared<MonsterTwistModifier>());
     out.push_back(std::make_shared<MonsterStealModifier>());
     out.push_back(std::make_shared<MonsterTimeWarpModifier>());
     out.push_back(std::make_shared<MonsterUnawakenedModifier>());
     out.push_back(std::make_shared<MonsterPainfulJabModifier>());
     out.push_back(std::make_shared<ArtifactModifier>());
     out.push_back(std::make_shared<BarricadeModifier>());
     out.push_back(std::make_shared<IntangibleModifier>());
     out.push_back(std::make_shared<MetallicizeModifier>());
     out.push_back(std::make_shared<MultiArmorModifier>());
     out.push_back(std::make_shared<RitualModifier>());
     out.push_back(std::make_shared<MonsterBlockClearModifier>());
     out.push_back(std::make_shared<DurationTickModifier>());
     out.push_back(std::make_shared<PlayerBlockClearModifier>());
     return out;
 }
 
 } // namespace tce