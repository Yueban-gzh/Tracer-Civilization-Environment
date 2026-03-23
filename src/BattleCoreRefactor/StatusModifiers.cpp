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

/** 与 on_player_gain_block 中「敏捷→脆弱」一致；敏捷可为负。 */
static int apply_dexterity_and_frail_to_card_block(int base, const BattleState& state) {
    int b = base;
    b += BattleEngine::get_status_stacks(state.player.statuses, "dexterity");
    if (BattleEngine::get_status_stacks(state.player.statuses, "frail") > 0)
        b = b * 3 / 4;
    return b > 0 ? b : 0;
}

/** 从玩家「敏捷」层数中减去 loss；可减至负数；仅当结果恰为 0 时移除该状态条目（不用 reduce_status_stacks，因其会把 ≤0 一律删掉）。 */
static void subtract_player_dexterity_allow_negative(std::vector<StatusInstance>& list, int loss) {
    if (loss <= 0) return;                                             // 无效减量直接返回
    for (auto it = list.begin(); it != list.end(); ++it) {             // 扫描玩家状态列表
        if (it->id != "dexterity") continue;                           // 非敏捷条目跳过
        it->stacks -= loss;                                            // 扣层数，允许变成负数
        if (it->stacks == 0)                                           // 恰好为 0 时视为无敏捷
            list.erase(it);                                            // 移除空条目
        return;                                                        // 已处理完已有敏捷
    }
    list.push_back(StatusInstance{"dexterity", -loss, -1});            // 原本没有敏捷则新建负敏捷
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

 class PlayerDexterityDownModifier : public IBattleModifier {      // 敏捷下降：回合结束时从「敏捷」扣 X，可负
 public:                                                              // 对外接口
     void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {  // 玩家回合结束
         const int x = BattleEngine::get_status_stacks(state.player.statuses, "dexterity_down");  // X=敏捷下降层数=本次扣除量
         if (x <= 0) return;                                          // 无敏捷下降则不扣敏捷
         subtract_player_dexterity_allow_negative(state.player.statuses, x);  // 从 dexterity 扣 X，可低于 0
     }                                                                // on_turn_end_player 结束
 };                                                                   // PlayerDexterityDownModifier 结束

 class PlayerFrailModifier : public IBattleModifier {               // 脆弱：在 X 回合内，从卡牌获得的格挡减少 25%
 public:
     void on_player_gain_block(int& block, const BattleState& state) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "frail");
         if (stacks <= 0) return;
         block = block * 3 / 4;                                       // 减少 25%
     }
 };
 
 class PlayerCannotDrawModifier : public IBattleModifier {        // 不能抽牌：本回合内无法从抽牌堆抽牌（回合初清零抽牌数；见 draw_cards_impl）
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx) return;
         if (BattleEngine::get_status_stacks(state.player.statuses, "cannot_draw") <= 0) return;
         ctx->draw_count = 0;
     }
 };
 class PlayerFlexModifier : public IBattleModifier {};              // 灵活（桩）

 class PlayerEntangleModifier : public IBattleModifier {           // 缠绕：本回合不能打出攻击牌；回合结束时受到等同于层数的伤害（走格挡）
 public:
     void on_can_play_card(CanPlayCardContext& ctx) override {
         int stacks = BattleEngine::get_status_stacks(ctx.state.player.statuses, "entangle");
         if (stacks <= 0 || !ctx.is_attack) return;
         ctx.blocked = true;                                         // 禁止打出攻击牌
     }
     void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) override {
         const int x = BattleEngine::get_status_stacks(state.player.statuses, "entangle");
         if (x <= 0) return;
         if (!ctx || !ctx->deal_damage_to_player) return;
         ctx->deal_damage_to_player(x);
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
 class PlayerFastingModifier : public IBattleModifier {             // 斋戒：回合开始减能量
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
 class PlayerCannotBlockModifier : public IBattleModifier {         // 无法格挡 debuff（状态 id：cannot_block）
 public:
     void on_player_gain_block(int& block, const BattleState& state) override {
         if (BattleEngine::get_status_stacks(state.player.statuses, "cannot_block") <= 0) return;  // 未携带则走正常敏捷/脆弱链
         block = 0;                                                                              // 从卡牌结算的格挡清零（先于 add_block_to_player）
     }
 };
class PlayerWraithFormModifier : public IBattleModifier {           // 幽魂形态：回合结束失去 1 点敏捷
 public:
   void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "wraith_form");
         if (stacks <= 0) return;
        BattleEngine::reduce_status_stacks(state.player.statuses, "dexterity", 1);
     }
 };
 
 // ========== 负面 - 敌方 ==========
 class MonsterRepelWithHandModifier : public IBattleModifier {};     // 以手拒之（桩）
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
 class MonsterDeathRhythmModifier : public IBattleModifier {          // 死亡律动：怪物携带，玩家每次打牌触发结算伤害
 public:                                                               // 对外仅实现出牌事件
     void on_card_played(BattleState& state, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {
         if (!ctx || !ctx->deal_damage_to_player) return;              // 上下文或回调缺失则无法对玩家扣血
         int sum = 0;                                                  // 本张牌触发的总伤害（多怪叠加）
         for (size_t i = 0; i < state.monsters.size(); ++i) {          // 枚举场上每一个怪物槽位
             if (state.monsters[i].currentHp <= 0) continue;           // 已死亡怪物不参与死亡律动
             sum += BattleEngine::get_status_stacks(state.monsters[i].statuses, "death_rhythm");  // 累加该怪 death_rhythm 层数
         }
         if (sum <= 0) return;                                         // 没有任何层数则无事发生
         ctx->deal_damage_to_player(sum);                              // 经引擎对玩家结算 sum 点（格挡/缓冲等照常）
     }                                                                 // 函数结束
 };                                                                    // MonsterDeathRhythmModifier 结束
 
 class MonsterCuriosityModifier : public IBattleModifier {            // 好奇：玩家每打出一张能力牌，带该效果的怪物获得 X 点力量
 public:                                                               // 对外只响应出牌事件
     void on_card_played(BattleState& state, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {  // 打牌时由引擎广播
         if (!ctx || !ctx->is_power) return;                           // 非能力牌不触发好奇
         for (auto& m : state.monsters) {                              // 遍历场上每一个怪物实例
             if (m.currentHp <= 0) continue;                           // 已死亡的不获得力量
             int x = BattleEngine::get_status_stacks(m.statuses, "curiosity");  // 该怪好奇层数 = 每次触发的力量值
             if (x <= 0) continue;                                     // 未带好奇则跳过
             add_strength_stacks(m.statuses, x);                       // 叠加到 strength 状态（持久，duration=-1 由力量条目决定）
         }                                                             // 结束遍历
     }                                                                 // on_card_played 结束
 };                                                                    // MonsterCuriosityModifier 结束

 class MonsterAngerModifier : public IBattleModifier {             // 生气：每次受到玩家攻击伤害结算后获得 X 点力量（层数 X）
 public:
     void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) override {
         if (ctx) return;                                             // 仅怪物受伤管线传入 nullptr
         if (!dmg.from_attack || dmg.source_type != DamagePacket::SourceType::Player) return;  // 仅玩家攻击牌造成的伤害
         const int idx = dmg.target_monster_index;                    // 受伤怪物下标
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;  // 下标非法
         auto& m = state.monsters[static_cast<size_t>(idx)];          // 目标怪物
         const int x = BattleEngine::get_status_stacks(m.statuses, "anger");  // 生气层数 = 每次获得的力量
         if (x <= 0) return;                                          // 无此状态不触发
         add_strength_stacks(m.statuses, x);                          // 累加到 strength（与行为里读的力量一致）
     }
 };

 class MonsterFlightModifier : public IBattleModifier {             // 飞行：玩家攻击伤害减半；层数 X = 本玩家回合内第 X 次此类伤害结算后移除
 public:
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         if (!dmg.from_attack) return;
         if (dmg.source_type != DamagePacket::SourceType::Player) return;
         int idx = dmg.target_monster_index;
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
         int f = BattleEngine::get_status_stacks(state.monsters[static_cast<size_t>(idx)].statuses, "flight");
         if (f <= 0) return;
         dmg.modified_amount /= 2;
     }
     void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) override {
         if (ctx) return;                                             // 仅怪物受伤路径传入 nullptr
         if (!dmg.from_attack || dmg.source_type != DamagePacket::SourceType::Player) return;
         int idx = dmg.target_monster_index;
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;
         auto& m = state.monsters[static_cast<size_t>(idx)];
         int need = BattleEngine::get_status_stacks(m.statuses, "flight");
         if (need <= 0) return;
         ++m.flightAttackHitsThisTurn;
         if (m.flightAttackHitsThisTurn >= need)
             BattleEngine::reduce_status_stacks(m.statuses, "flight", need);
     }
 };

 class MonsterCurlUpModifier : public IBattleModifier {            // 蜷身：首次受玩家攻击伤害结算时获 X 格挡（每场战斗一次）
 public:                                                               // 仅监听伤害结算后事件
     void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) override {  // 伤害结算后广播
         if (ctx) return;                                             // 玩家受伤路径带上下文，此处只处理怪物受伤
         if (!dmg.from_attack || dmg.source_type != DamagePacket::SourceType::Player) return;  // 仅统计玩家攻击牌来源
         const int idx = dmg.target_monster_index;                    // 挨打怪物下标
         if (idx < 0 || static_cast<size_t>(idx) >= state.monsters.size()) return;  // 下标越界则忽略
         auto& m = state.monsters[static_cast<size_t>(idx)];          // 引用该怪物实例
         if (m.curlUpUsedThisBattle) return;                          // 本场已蜷身过则不再触发
         const int x = BattleEngine::get_status_stacks(m.statuses, "curl_up");  // 层数 X = 获得的格挡值
         if (x <= 0) return;                                          // 未携带蜷身状态则跳过
         m.block += x;                                                 // 蜷起身子：增加格挡
         m.curlUpUsedThisBattle = true;                                 // 标记本场已用掉唯一一次机会
     }                                                                 // on_damage_applied 结束
 };                                                                    // MonsterCurlUpModifier 结束

 class MonsterVanishModifier : public IBattleModifier {              // 消逝：层数=剩余回合，每回合末-1，归零即死亡
 public:                                                               // 对外暴露回合结束逻辑
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* ctx) override {  // 敌方回合结束时递减
         for (size_t i = 0; i < state.monsters.size(); ++i) {          // 遍历每个怪物槽位
             if (state.monsters[i].currentHp <= 0) continue;           // 已死亡不处理消逝
             const int before = BattleEngine::get_status_stacks(state.monsters[i].statuses, "vanish");  // 递减前层数
             if (before <= 0) continue;                                  // 无消逝状态则跳过
             BattleEngine::reduce_status_stacks(state.monsters[i].statuses, "vanish", 1);  // 每回合层数 -1
             const int after = BattleEngine::get_status_stacks(state.monsters[i].statuses, "vanish");  // 递减后层数
             if (after > 0) continue;                                   // 仍有剩余回合则暂不死亡
             if (!ctx || !ctx->kill_monster) continue;                   // 无引擎回调则无法执行击杀
             ctx->kill_monster(static_cast<int>(i));                    // 层数耗尽：触发死亡管线
         }                                                             // 遍历结束
     }                                                                 // on_turn_end_monsters 结束
 };                                                                    // MonsterVanishModifier 结束
 
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

class MonsterIndestructibleModifier : public IBattleModifier {      // 坚不可摧：层数 X = 本段「回合」内最多还能再掉多少生命
public:
    void on_monster_before_hp_loss(int& hpDamage, BattleState& state, int monster_index, const DamagePacket& /*dmg*/) override {
        if (hpDamage <= 0) return;                                  // 格挡后已无生命伤害，跳过
        if (monster_index < 0 || static_cast<size_t>(monster_index) >= state.monsters.size()) return;  // 目标下标非法，跳过
        auto& m = state.monsters[static_cast<size_t>(monster_index)]; // 当前受伤的怪物引用
        int cap = BattleEngine::get_status_stacks(m.statuses, "indestructible");  // 读取坚不可摧层数作为本回合生命损失上限 X
        if (cap <= 0) return;                                         // 未持有该状态，不限制
        const int room = cap - m.indestructibleDamageTakenThisTurn;   // 本回合剩余可再扣的生命额度
        if (room <= 0) {                                              // 额度已用尽
            hpDamage = 0;                                             // 本次不再扣血
            return;
        }
        if (hpDamage > room) hpDamage = room;                         // 超出剩余额度则压到上限内
    }
};

 class MonsterSharpCarapaceModifier : public IBattleModifier {};     // 尖刺甲壳（桩）
 class MonsterFormShiftModifier : public IBattleModifier {};         // 形态转换（桩）
 class MonsterTransformModifier : public IBattleModifier {};         // 变形（桩）
 class MonsterSplitModifier : public IBattleModifier {};             // 分裂（桩）
 class MonsterStasisModifier : public IBattleModifier {};            // 静滞（桩）

class StrengthBoostModifier : public IBattleModifier {                 // 力量提升：每回合结束时获得 X 点力量
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
             "draw_reduction", "cannot_draw", "cannot_block", "fasting", "confusion", "entangle", "choke", "slow",
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
 
 /** 壁垒：格挡不在你的回合开始时清空（由 PlayerBlockClearModifier 在回合初判定）；能力本身永久，施加时强制 duration=-1 */
 class BarricadeModifier : public IBattleModifier {
 public:
     void on_before_status_applied(StatusApplyContext& ctx) override {
         if (ctx.id != "barricade") return;
         ctx.duration = -1;
         if (ctx.stacks < 1) ctx.stacks = 1;
     }
 };
 
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
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "metallicize");
         if (stacks <= 0) return;                                     // 无层数不加格挡
         if (ctx && ctx->grant_player_block) ctx->grant_player_block(stacks);  // 走引擎统一加格挡（势不可挡等）
         else state.player.block += stacks;                           // 无回调时直接累加
     }
 };
 class MultiArmorModifier : public IBattleModifier {                 // 多层护甲：回合结束加格挡（玩家+怪物）
 public:
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "multi_armor");
         if (stacks <= 0) return;                                     // 无层数跳过
         if (ctx && ctx->grant_player_block) ctx->grant_player_block(stacks);  // 统一加格挡入口
         else state.player.block += stacks;                          // 兜底直接改状态
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
 // ========== 正面 - 玩家 ==========
 /** 缓冲：抵消「下一次」将造成的生命值损失（一次伤害包 = 一次），不论来源（怪打、中毒、自伤牌等）；层数耗尽即移除。
  *  不在回合结束时因 duration 衰减；若 duration>0 表示与层数同步展示，消耗层数时同步减小 duration。 */
 class PlayerBufferModifier : public IBattleModifier {
 public:
     void on_player_before_hp_loss(int& hpDamage, BattleState& state, const DamagePacket& /*dmg*/) override {
         if (hpDamage <= 0) return;
         for (auto it = state.player.statuses.begin(); it != state.player.statuses.end(); ++it) {
             if (it->id != "buffer") continue;
             if (it->stacks <= 0) continue;
             hpDamage = 0;
             --it->stacks;
             if (it->duration > 0)
                 it->duration = it->stacks;
             if (it->stacks <= 0)
                 state.player.statuses.erase(it);
             return;
         }
     }
 };
 class PlayerDexterityModifier : public IBattleModifier {            // 敏捷：卡牌格挡 += 敏捷层数（负敏捷则减格挡；先于脆弱）
 public:                                                              // 对外接口
     void on_player_gain_block(int& block, const BattleState& state) override {  // 结算卡牌基础格挡时
         const int dex = BattleEngine::get_status_stacks(state.player.statuses, "dexterity");  // 当前敏捷代数和（可负）
         if (dex == 0) return;                                        // 为 0 则无加减
         block += dex;                                                // 非 0 则整段加到格挡上
     }                                                                // on_player_gain_block 结束
 };                                                                   // PlayerDexterityModifier 结束
 
 class PlayerDrawCardModifier : public IBattleModifier {              // 抽牌：回合开始多抽 X 张
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         if (!ctx) return;
         if (BattleEngine::get_status_stacks(state.player.statuses, "cannot_draw") > 0) return;
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "draw_up");
         if (stacks <= 0) return;
         ctx->draw_count += stacks;                                  // 抽牌数 + 层数
         BattleEngine::reduce_status_stacks(state.player.statuses, "draw_up", stacks);  // 一次性消耗
     }
 };
 class PlayerEnergyUpModifier : public IBattleModifier {              // 能量提升：回合开始多 X 能量
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

 class PlayerBlockUpModifier : public IBattleModifier {               // 下回合格挡：下回合开始加格挡
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {
         int stacks = BattleEngine::get_status_stacks(state.player.statuses, "block_up");
         if (stacks <= 0) return;                                     // 无层数跳过
         if (ctx && ctx->grant_player_block) ctx->grant_player_block(stacks);  // 统一加格挡
         else state.player.block += stacks;                           // 无回调时直写
         BattleEngine::reduce_status_stacks(state.player.statuses, "block_up", stacks);  // 一次性消耗层数
     }
 };
 class PlayerAccuracyModifier : public IBattleModifier {};           // 精准（桩）
 class PlayerAmplifyModifier : public IBattleModifier {};             // 增幅（桩）

class PlayerAfterimageModifier : public IBattleModifier {           // 余像：每打出一张牌获得 1 点格挡/层
public:
    void on_card_played(BattleState& state, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {
        if (BattleEngine::get_status_stacks(state.player.statuses, "cannot_block") > 0) return;  // 无法格挡：打牌触发的格挡也不生效
        int stacks = BattleEngine::get_status_stacks(state.player.statuses, "after_image");
        if (stacks <= 0) return;                                      // 无余像层数
        const int b = apply_dexterity_and_frail_to_card_block(stacks, state);  // 敏捷脆弱后的格挡量
        if (b <= 0) return;                                          // 折算后为 0 则不加
        if (ctx && ctx->grant_player_block) ctx->grant_player_block(b);  // 经引擎叠格挡（势不可挡）
        else state.player.block += b;                                // 无回调时直加
    }
};

 class PlayerBattleHymnModifier : public IBattleModifier {};          // 战歌（桩）
 
 class PlayerBlasphemyModifier : public IBattleModifier {            // 渎神者：携带 blasphemy 时，在你回合开始时立即死亡
 public:                                                              // 对外接口
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {  // 玩家回合开始回调
         if (!ctx || !ctx->instant_kill_player) return;               // 无引擎注入的处决回调则无法结算
         const int stacks = BattleEngine::get_status_stacks(state.player.statuses, "blasphemy");  // 渎神标记层数（>0 即生效）
         if (stacks <= 0) return;                                     // 未处于渎神惩罚则跳过
         BattleEngine::reduce_status_stacks(state.player.statuses, "blasphemy", stacks);  // 触发前清空该状态（避免 UI 残留）
         ctx->instant_kill_player();                                  // 执行致死并广播 on_player_died
     }                                                                // on_turn_start_player 结束
 };                                                                   // PlayerBlasphemyModifier 类结束
 class PlayerShadowModifier : public IBattleModifier {};              // 暗影（桩）
 class PlayerBurstModifier : public IBattleModifier {};               // 爆发（桩）
 class PlayerBerserkModifier : public IBattleModifier {};             // 狂暴（桩）
 class PlayerBrutalityModifier : public IBattleModifier {};           // 残暴（桩）
 class PlayerCombustModifier : public IBattleModifier {};             // 自燃（桩）
 class PlayerCorruptionModifier : public IBattleModifier {};          // 腐化（桩）
 class PlayerCreativeAIModifier : public IBattleModifier {};          // 创意 AI（桩）
 class PlayerDarkEmbraceModifier : public IBattleModifier {};        // 黑暗之拥（桩）
 
 // ========== 正面 - 唯一（桩） ==========
 // 复制 duplicate：下 X 张牌各执行 2 次效果，层数在 BattleEngine::play_card 中消耗（无独立 modifier）
 class DoubleDamageModifier : public IBattleModifier {               // 双倍伤害：攻击伤害 ×2，持续 duration 回合（与 stacks 同步递减）
 public:
     void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) override {
         if (!dmg.from_attack) return;
         for (const auto& s : state.player.statuses) {
             if (s.id != "double_damage") continue;
             if (s.duration <= 0) continue;
             dmg.modified_amount *= 2;
             return;
         }
     }
 };
 class DoubleTapModifier : public IBattleModifier {                  // 双发：仅本回合有效，回合结束时清除 double_tap（层数在 play_card 中消耗）
 public:
     void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         state.player.statuses.erase(
             std::remove_if(state.player.statuses.begin(), state.player.statuses.end(),
                 [](const StatusInstance& x) { return x.id == "double_tap"; }),
             state.player.statuses.end());
     }
 };
 class DevotionModifier : public IBattleModifier {};                  // 虔诚（桩）
 class DemonFormModifier : public IBattleModifier {                  // 恶魔形态：你的回合开始时获得 X 点力量（X = demon_form 层数）
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* /*ctx*/) override {
         int x = BattleEngine::get_status_stacks(state.player.statuses, "demon_form");
         if (x <= 0) return;
         add_strength_stacks(state.player.statuses, x);
     }
 };
 class EstablishmentModifier : public IBattleModifier {               // 确立基础：保留牌被保留时本场耗能再减 X（层数 X）
 public:                                                               // 依赖 PlayerTurnEndContext 中的手牌回调
     void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) override {  // 玩家回合结束：弃牌后仍留手的保留牌
         if (!ctx || !ctx->apply_combat_discount_to_retained_hand_cards) return;  // 无引擎注入的回调则跳过
         const int x = BattleEngine::get_status_stacks(state.player.statuses, "establishment");  // 确立基础层数 = 每次减费值
         if (x <= 0) return;                                          // 未拥有该能力则不处理
         ctx->apply_combat_discount_to_retained_hand_cards(x);       // 给当前手牌中所有 Retain 牌叠加减费
     }                                                                 // on_turn_end_player 结束
 };                                                                    // EstablishmentModifier 结束
 
 class EquilibriumModifier : public IBattleModifier {                 // 均衡：接下来 X 个己方回合整手保留；层数每回合 -1；虚无仍消耗
 public:                                                               // 对外：注册到 modifier 系统
     void on_before_player_hand_discard(BattleState& state, bool& skip_full_discard_ref) override {  // 手牌弃置前钩子
         const int turns_left = BattleEngine::get_status_stacks(state.player.statuses, "equilibrium");  // 剩余生效回合数 = 层数
         if (turns_left <= 0) return;                                   // 未持有均衡则直接返回
         skip_full_discard_ref = true;                                  // 通知引擎跳过非虚无牌的弃牌/入堆
         BattleEngine::reduce_status_stacks(state.player.statuses, "equilibrium", 1);  // 本回合已触发，层数减 1
     }                                                                  // 钩子结束
 };                                                                     // EquilibriumModifier 类结束

 class EchoFormModifier : public IBattleModifier {};                 // 回响形态（桩）
 class FeelNoPainModifier : public IBattleModifier {};               // 无痛（桩）
 class EvolveModifier : public IBattleModifier {};                   // 进化（桩）
 class FlameBarrierModifier : public IBattleModifier {};              // 火焰屏障（桩）

 class HeatSinkModifier : public IBattleModifier {                  // 散热：每打出一张能力牌抽 X 张牌（X = heat_sink 层数）
 public:
     void on_card_played(BattleState& state, CardId /*card_id*/, int /*target_monster_index*/, CardPlayContext* ctx) override {
         if (!ctx || !ctx->is_power || !ctx->draw_cards) return;
         int x = BattleEngine::get_status_stacks(state.player.statuses, "heat_sink");
         if (x <= 0) return;
         ctx->draw_cards(x);
     }
 };

 // 免费攻击 free_attack：下 X 张攻击牌耗能 0，在 BattleEngine::play_card 中结算
 class FireBreathModifier : public IBattleModifier {};                // 火焰吐息（桩）
 class HelloModifier : public IBattleModifier {                     // 你好：回合开始时将 X 张随机普通稀有度牌置入手牌
 public:                                                              // 对外接口
     void on_turn_start_player(BattleState& state, TurnStartContext* ctx) override {  // 玩家回合开始
         if (!ctx || !ctx->add_random_common_cards_to_hand) return;   // 无引擎注入的加牌回调则跳过
         const int x = BattleEngine::get_status_stacks(state.player.statuses, "hello");  // 层数 X = 每次生成的张数
         if (x <= 0) return;                                          // 未拥有该能力则不处理
         ctx->add_random_common_cards_to_hand(x);                     // 加入 X 张随机普通牌（临时牌）
     }                                                                // on_turn_start_player 结束
 };                                                                   // HelloModifier 类结束
 class UnstoppableModifier : public IBattleModifier {                 // 势不可挡：每次获得格挡时对随机敌人造成 X 伤（X=层数）
 public:                                                              // 对外接口
     void on_player_block_gained(BattleState& state, int amount, PlayerBlockGainedContext* ctx) override {  // 格挡已增加后
         if (amount <= 0) return;                                     // 本次未增加格挡则跳过
         if (!ctx || !ctx->deal_damage_to_random_living_monster) return;  // 无伤害回调无法结算
         const int x = BattleEngine::get_status_stacks(state.player.statuses, "unstoppable");  // 层数即每次加格挡时造成的伤害值
         if (x <= 0) return;                                          // 未携带势不可挡
         ctx->deal_damage_to_random_living_monster(x);                 // 随机选一个存活敌人结算伤害
     }                                                                // on_player_block_gained 结束
 };                                                                   // UnstoppableModifier 类结束
 class NightmareModifier : public IBattleModifier {};                 // 梦魇（桩）
 class RealityManipulationModifier : public IBattleModifier {};     // 现实操纵（桩）
 class MachineLearningModifier : public IBattleModifier {};         // 机器学习（桩）
 class NirvanaModifier : public IBattleModifier {};                  // 涅槃（桩）
 
 class PoisonCloudModifier : public IBattleModifier {                 // 毒雾：你的回合开始时，所有敌人 +X 层中毒（X = 玩家 poison_cloud 层数）
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* /*ctx*/) override {
         int x = BattleEngine::get_status_stacks(state.player.statuses, "poison_cloud");
         if (x <= 0) return;
         for (auto& m : state.monsters) {
             if (m.currentHp <= 0) continue;
             BattleEngine::merge_status_into_list(m.statuses, "poison", x, x);
         }
     }
 };
 class PhantomModifier : public IBattleModifier {};                  // 幻影（桩）
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
 
 class RuptureModifier : public IBattleModifier {};                  // 破裂（桩）
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
 class DurationTickModifier : public IBattleModifier {               // 持续回合递减：玩家状态在「玩家回合结束」-1；怪物状态在「敌方回合结束」-1；duration=-1 永久不递减
 public:
     // 玩家身上的持续/层数：与金属化、炸弹、再生等一致，在玩家点「结束回合」时结算（弱/易伤/脆弱等层数会立刻可见减少）
     void on_turn_end_player(BattleState& state, PlayerTurnEndContext* /*ctx*/) override {
         tick_player_status_durations(state);
     }
     void on_turn_end_monsters(BattleState& state, EnemyTurnContext* /*ctx*/) override {
         for (auto& m : state.monsters) {
            for (auto& s : m.statuses) {
                if (s.duration > 0) {                             // 每回合 -1
                    --s.duration;
                   if ((s.id == "weak" || s.id == "vulnerable" || s.id == "frail" || s.id == "intangible" || s.id == "double_damage") && s.stacks > 0) {
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

 private:
     static void tick_player_status_durations(BattleState& state) {
         for (auto& s : state.player.statuses) {
             if (s.id == "draw_up" || s.id == "energy_up" || s.id == "block_up" || s.id == "buffer" || s.id == "barricade" || s.id == "demon_form" || s.id == "heat_sink" || s.id == "establishment" || s.id == "equilibrium" || s.id == "blasphemy" || s.id == "hello" || s.id == "unstoppable" || s.id == "free_attack" || s.id == "double_tap") continue;
             if (s.duration > 0) {
                 --s.duration;
                 if ((s.id == "weak" || s.id == "vulnerable" || s.id == "frail" || s.id == "intangible" || s.id == "double_damage") && s.stacks > 0)
                     --s.stacks;
             }
         }
         state.player.statuses.erase(
             std::remove_if(state.player.statuses.begin(), state.player.statuses.end(),
                 [](const StatusInstance& x) {
                     if (x.id == "draw_up" || x.id == "energy_up" || x.id == "block_up" || x.id == "buffer" || x.id == "barricade" || x.id == "demon_form" || x.id == "heat_sink" || x.id == "establishment" || x.id == "equilibrium" || x.id == "blasphemy" || x.id == "hello" || x.id == "unstoppable" || x.id == "free_attack" || x.id == "double_tap") return false;
                     return x.duration == 0;
                 }),
             state.player.statuses.end());
     }
 };
 class PlayerBlockClearModifier : public IBattleModifier {           // 玩家格挡：无壁垒时在你回合开始时清空（有壁垒则保留格挡）
 public:
     void on_turn_start_player(BattleState& state, TurnStartContext* /*ctx*/) override {
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
     out.push_back(std::make_shared<BarricadeModifier>());             // 壁垒：施加时 duration=-1；与格挡清空规则配套
     out.push_back(std::make_shared<PlayerPoisonModifier>());
     out.push_back(std::make_shared<PlayerConfusionModifier>());
     out.push_back(std::make_shared<PlayerDexterityModifier>());       // 先于脆弱：格挡 = 基础 + 敏捷（可负），再 ×0.75
     out.push_back(std::make_shared<PlayerFrailModifier>());
     out.push_back(std::make_shared<PlayerFlexModifier>());
     out.push_back(std::make_shared<PlayerEntangleModifier>());
     out.push_back(std::make_shared<PlayerDrawReductionModifier>());
     out.push_back(std::make_shared<PlayerHauntedModifier>());
     out.push_back(std::make_shared<PlayerFastingModifier>());
     out.push_back(std::make_shared<PlayerCurseModifier>());
     out.push_back(std::make_shared<PlayerCannotBlockModifier>());
     out.push_back(std::make_shared<PlayerDexterityDownModifier>());  // 回合末先按敏捷下降扣 X 点敏捷（可负）
     out.push_back(std::make_shared<PlayerWraithFormModifier>());     // 再幽魂形态扣 1 点敏捷
     out.push_back(std::make_shared<PlayerDrawCardModifier>());
     out.push_back(std::make_shared<PlayerCannotDrawModifier>());
     out.push_back(std::make_shared<PlayerEnergyUpModifier>());
     out.push_back(std::make_shared<PlayerVigorModifier>());
     out.push_back(std::make_shared<PlayerBlockUpModifier>());
     out.push_back(std::make_shared<PlayerAccuracyModifier>());
     out.push_back(std::make_shared<PlayerAmplifyModifier>());
     out.push_back(std::make_shared<PlayerAfterimageModifier>());
     out.push_back(std::make_shared<PlayerBattleHymnModifier>());
     out.push_back(std::make_shared<PlayerShadowModifier>());
     out.push_back(std::make_shared<PlayerBurstModifier>());
     out.push_back(std::make_shared<PlayerBerserkModifier>());
     out.push_back(std::make_shared<PlayerBrutalityModifier>());
     out.push_back(std::make_shared<PlayerCombustModifier>());
     out.push_back(std::make_shared<PlayerCorruptionModifier>());
     out.push_back(std::make_shared<PlayerCreativeAIModifier>());
     out.push_back(std::make_shared<PlayerDarkEmbraceModifier>());
     out.push_back(std::make_shared<ArtifactModifier>());           // 人工制品：玩家可免疫负面效果
     out.push_back(std::make_shared<DoubleDamageModifier>());
     out.push_back(std::make_shared<DoubleTapModifier>());
     out.push_back(std::make_shared<DevotionModifier>());
     out.push_back(std::make_shared<DemonFormModifier>());
     out.push_back(std::make_shared<EstablishmentModifier>());
     out.push_back(std::make_shared<EquilibriumModifier>());
     out.push_back(std::make_shared<EchoFormModifier>());
     out.push_back(std::make_shared<FeelNoPainModifier>());
     out.push_back(std::make_shared<EvolveModifier>());
     out.push_back(std::make_shared<FlameBarrierModifier>());
     out.push_back(std::make_shared<HeatSinkModifier>());
     out.push_back(std::make_shared<FireBreathModifier>());
     out.push_back(std::make_shared<HelloModifier>());
     out.push_back(std::make_shared<UnstoppableModifier>());
     out.push_back(std::make_shared<NightmareModifier>());
     out.push_back(std::make_shared<RealityManipulationModifier>());
     out.push_back(std::make_shared<MachineLearningModifier>());
     out.push_back(std::make_shared<NirvanaModifier>());
     out.push_back(std::make_shared<PoisonCloudModifier>());
     out.push_back(std::make_shared<PhantomModifier>());
     out.push_back(std::make_shared<RicochetModifier>());
     out.push_back(std::make_shared<RageModifier>());
     out.push_back(std::make_shared<RegenerateModifier>());
     out.push_back(std::make_shared<RepairModifier>());
     out.push_back(std::make_shared<RuptureModifier>());
     out.push_back(std::make_shared<CrueltyModifier>());
     out.push_back(std::make_shared<StudyModifier>());
     out.push_back(std::make_shared<BombModifier>());
     out.push_back(std::make_shared<MeleeModifier>());
     out.push_back(std::make_shared<LingchiModifier>());
     out.push_back(std::make_shared<EssentialToolsModifier>());
     out.push_back(std::make_shared<WaveHandModifier>());
     out.push_back(std::make_shared<WellPlannedModifier>());
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
     out.push_back(std::make_shared<MonsterFlightModifier>());
     out.push_back(std::make_shared<MonsterCurlUpModifier>());
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
     out.push_back(std::make_shared<IntangibleModifier>());
     out.push_back(std::make_shared<MetallicizeModifier>());
     out.push_back(std::make_shared<MultiArmorModifier>());
     out.push_back(std::make_shared<RitualModifier>());
     out.push_back(std::make_shared<MonsterBlockClearModifier>());
     out.push_back(std::make_shared<DurationTickModifier>());
     out.push_back(std::make_shared<PlayerBlockClearModifier>());
     return out;
 }
 
 std::shared_ptr<IBattleModifier> create_buffer_modifier() {
     return std::make_shared<PlayerBufferModifier>();
 }
 
 std::shared_ptr<IBattleModifier> create_blasphemy_modifier() {
     return std::make_shared<PlayerBlasphemyModifier>();
 }
 
 } // namespace tce