/**
 * B - 战斗引擎实现（BattleCoreRefactor）
 */
#include "../../include/BattleCoreRefactor/BattleEngine.hpp"           // 战斗引擎头文件
#include "../../include/BattleCoreRefactor/PotionEffects.hpp"          // 药水效果
#include "../../include/BattleCoreRefactor/RelicModifiers.hpp"         // 遗物 modifier
#include "../../include/BattleCoreRefactor/StatusModifiers.hpp"        // 状态 modifier
#include "../../include/CardSystem/CardSystem.hpp"                      // 卡牌系统
#include "../../include/Common/RunRng.hpp"
#include "../../include/DataLayer/DataLayer.hpp"                        // 数据层
#include "../../include/BattleEngine/MonsterBehaviors.hpp"             // 怪物行为
#include <algorithm>                                                  // std::remove_if
#include <cassert>
 
 namespace tce {
 
 BattleEngine::BattleEngine(CardSystem& card_system,                    // 构造函数
                            GetMonsterByIdFn get_monster,
                            GetCardByIdFn get_card,
                            ExecuteMonsterActionFn execute_monster,
                            RunRng* run_rng)
     : card_system_(&card_system)                                       // 卡牌系统指针
     , run_rng_(run_rng)                                                // 与 CardSystem/主流程同源
     , get_monster_by_id_(std::move(get_monster))                       // 获取怪物数据回调
     , get_card_by_id_(std::move(get_card))                             // 获取卡牌数据回调
     , execute_monster_action_(std::move(execute_monster)) {             // 怪物行动执行回调
     assert(run_rng_);
 }
 
void BattleEngine::start_battle(const std::vector<MonsterId>& monster_ids,  // 开始战斗
                                 const PlayerBattleState&      player_state,
                                 const std::vector<CardId>&    deck_card_ids,
                                 const std::vector<RelicId>&   relic_ids) {
    state_.player = player_state;                                      // 初始化玩家状态
    state_.player.relics = relic_ids;                                  // 设置遗物列表
    state_.monsters.clear();                                           // 清空怪物列表
    state_.pendingDamageDisplays.clear();                              // 清除上一场伤害数字，避免跨战斗残留
     state_.turnNumber = 1;                                             // 回合数从 1 开始
     state_.phase      = BattleState::TurnPhase::PlayerTurnStart;      // 阶段：玩家回合开始
     for (const auto& mid : monster_ids) {                              // 遍历怪物 ID 列表
         MonsterInBattle m;                                             // 创建怪物实例
         m.id = mid;                                                    // 怪物 ID
         const MonsterData* md = get_monster_by_id_ ? get_monster_by_id_(mid) : nullptr;  // 获取怪物静态数据
         m.maxHp     = md ? md->maxHp : 10;                             // 最大生命（默认 10）
         m.currentHp = m.maxHp;                                         // 当前生命 = 最大生命
         m.currentIntent = get_monster_intent(mid, state_.turnNumber, &m.statuses);   // 当前意图
         state_.monsters.push_back(m);                                  // 加入怪物列表
     }
     if (card_system_) card_system_->init_deck(deck_card_ids);           // 初始化牌组
     build_modifiers_from_state();                                      // 根据状态构建 modifier 列表
     modifiers_.on_battle_start(state_);                               // 广播：战斗开始（弹珠袋给敌人易伤等）
 }
 
 BattleState BattleEngine::get_battle_state() const {                   // 获取战斗状态快照
     return state_;                                                     // 返回当前状态
 }

void BattleEngine::clear_pending_damage_displays() {                   // 清除本帧伤害显示（绘制后调用）
    state_.pendingDamageDisplays.clear();
}

void BattleEngine::tick_damage_displays() {                            // 每帧调用：递减显示时长，移除过期项
    auto& list = state_.pendingDamageDisplays;
    list.erase(std::remove_if(list.begin(), list.end(), [](DamageDisplayEvent& ev) {
        return --ev.frames_remaining <= 0;
    }), list.end());
}
 
namespace {
bool hand_contains_normality_card(const std::vector<CardInstance>& hand) {
    for (const auto& c : hand) {
        if (c.id == "normality" || c.id == "normality+") return true;
    }
    return false;
}
int count_pain_in_hand_excluding_index(const std::vector<CardInstance>& hand, int exclude_index) {
    int n = 0;
    for (int i = 0; i < static_cast<int>(hand.size()); ++i) {
        if (i == exclude_index) continue;
        const CardId& hid = hand[static_cast<size_t>(i)].id;
        if (hid == "pain" || hid == "pain+") ++n;
    }
    return n;
}
bool is_decay_id(const CardId& id) { return id == "decay" || id == "decay+"; }
bool is_doubt_id(const CardId& id) { return id == "doubt" || id == "doubt+"; }
bool is_regret_id(const CardId& id) { return id == "regret" || id == "regret+"; }
bool is_shame_id(const CardId& id) { return id == "shame" || id == "shame+"; }
bool is_pride_id(const CardId& id) { return id == "pride" || id == "pride+"; }
/** 灼伤：回合结束时仍在手牌则受到对应伤害（与腐朽等同一时机） */
int burn_end_turn_damage_for_id(const CardId& id) {
    if (id == "card_026") return 2;
    if (id == "card_026+") return 4;
    return 0;
}
} // namespace

 bool BattleEngine::play_card(int hand_index, int target_monster_index) {  // 打出卡牌
    if (!card_system_) return false;                                   // 无卡牌系统则失败
    const auto& hand = card_system_->get_hand();                        // 获取手牌
    if (hand_index < 0 || static_cast<size_t>(hand_index) >= hand.size()) return false;  // 下标越界
    const CardInstance& inst_in_hand = hand[static_cast<size_t>(hand_index)];
    const CardData* cd = get_card_by_id_ ? get_card_by_id_(inst_in_hand.id) : nullptr;  // 卡牌数据
    if (cd && cd->unplayable) return false;                            // 不可打出则失败

    const CardId played_id = inst_in_hand.id;
    const bool combat_zero_skill = inst_in_hand.combat_cost_zero;
    if (played_id == "grand_finale" || played_id == "grand_finale+") {
        if (get_draw_pile_size_impl() > 0) return false;               // 华丽收场：仅当抽牌堆为空
    }

    const bool is_attack = cd && cd->cardType == CardType::Attack;
    const bool use_free_attack = is_attack && get_status_stacks(state_.player.statuses, "free_attack") > 0;

    const bool corruption_active =
        cd && cd->cardType == CardType::Skill && get_status_stacks(state_.player.statuses, "corruption") > 0;

    const int raw_cost = cd ? cd->cost : 0;
    const bool is_x_cost = (raw_cost == -1);
    int adjusted_non_x = raw_cost;
    if (raw_cost >= 0) {
        adjusted_non_x = raw_cost - inst_in_hand.combatCostDiscount;
        if (adjusted_non_x < 0) adjusted_non_x = 0;
    }

    int x_spent = 0;
    if (use_free_attack) {
        x_spent = 0;
    } else if (is_x_cost) {
        if (corruption_active)
            x_spent = 0;
        else
            x_spent = state_.player.energy;
    } else {
        int eff_cost = adjusted_non_x;
        if (corruption_active && cd && cd->cardType == CardType::Skill)
            eff_cost = 0;
        else if (combat_zero_skill && cd && cd->cardType == CardType::Skill)
            eff_cost = 0;
        else if (played_id == "eviscerate" || played_id == "eviscerate+") {
            int disc = get_status_stacks(state_.player.statuses, "discarded_this_turn");
            eff_cost = std::max(0, raw_cost - disc - inst_in_hand.combatCostDiscount);
        }
        if (state_.player.energy < eff_cost) return false;               // 能量不足
    }

    // 仅当 requiresTarget 为 true 时需选目标（群伤牌为 false，不需选目标）
    const bool needs_target = (cd && cd->requiresTarget);
    if (needs_target) {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state_.monsters.size())
            return false;
        if (state_.monsters[static_cast<size_t>(target_monster_index)].currentHp <= 0)
            return false;  // 目标已死
    }

    // 凡庸：手牌中有凡庸时，本回合最多打出 3 张牌
    if (hand_contains_normality_card(hand) && state_.player.cardsPlayedThisTurn >= 3)
        return false;

    // 打出前：缠身等 modifier 可禁止打出（如禁止攻击牌）
    CanPlayCardContext can_ctx{state_, played_id, cd && cd->cardType == CardType::Attack, target_monster_index, false};
    modifiers_.on_can_play_card(can_ctx);
    if (can_ctx.blocked) return false;  // 缠身等禁止打出
    if (!card_system_->has_effect_registered(played_id)) return false;  // 未注册效果则不打出（避免消耗能量无效果）

    const int pain_triggers = count_pain_in_hand_excluding_index(hand, hand_index);  // 疼痛：打出前统计其余手牌中的疼痛张数
    CardInstance played = card_system_->remove_from_hand(hand_index);    // 从手牌移除（保留实例上的 combatCostDiscount）

    if (pain_triggers > 0) {                                             // 疼痛：每有一张，失去 1 生命（无视格挡）
        DamagePacket pain_dmg;
        pain_dmg.modified_amount = pain_triggers;
        pain_dmg.ignore_block    = true;
        pain_dmg.can_be_reduced  = true;
        pain_dmg.source_type     = DamagePacket::SourceType::Status;
        apply_damage_to_player(pain_dmg);
    }

    if (use_free_attack) {
        reduce_status_stacks(state_.player.statuses, "free_attack", 1);  // 消耗 1 次免费攻击
    } else if (is_x_cost) {
        if (!corruption_active)
            state_.player.energy = 0;                                  // X 费：消耗全部当前能量（腐化下技能 X 不扣）
    } else {
        int eff_cost = adjusted_non_x;
        if (corruption_active && cd && cd->cardType == CardType::Skill)
            eff_cost = 0;
        else if (combat_zero_skill && cd && cd->cardType == CardType::Skill)
            eff_cost = 0;
        else if (played_id == "eviscerate" || played_id == "eviscerate+") {
            int disc = get_status_stacks(state_.player.statuses, "discarded_this_turn");
            eff_cost = std::max(0, raw_cost - disc - inst_in_hand.combatCostDiscount);
        }
        state_.player.energy -= eff_cost;                              // 扣除能量（腐化下技能为 0）
    }
 
     EffectContext ctx;                                                 // 效果上下文
     fill_effect_context(ctx);                                          // 填充引擎指针
     ctx.target_monster_index = target_monster_index;                   // 目标怪物下标
     ctx.from_attack = (cd && cd->cardType == CardType::Attack);          // 是否为攻击牌
    ctx.x_cost_spent = x_spent;                                        // 记录本次 X 费值（非 X 费为 0）
 
     CardPlayContext card_ctx{                                          // 打出卡牌上下文（勒脖、点穴等）
         [this](int monster_index, int amount) {                         // 对怪物造成无视格挡伤害的回调
             if (amount <= 0 || monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;  // 参数校验
             DamagePacket dmg;                                          // 伤害包
             dmg.modified_amount = amount;                               // 伤害值
             dmg.ignore_block    = true;                                 // 无视格挡
             dmg.can_be_reduced  = true;                                 // 可被减伤
             dmg.source_type     = DamagePacket::SourceType::Status;     // 来源：状态
             dmg.target_monster_index = monster_index;                  // 目标怪物
             apply_damage_to_monster(dmg);                               // 施加伤害
         },
         (cd && cd->cardType == CardType::Attack),                       // 是否为攻击牌（双截棍等用）
         [this](int n) { draw_cards_impl(n); },  // 散热等：抽牌（尊重不能抽牌）
         (cd && cd->cardType == CardType::Power),                        // 是否为能力牌
         [this](int amount) {                                           // 死亡律动等：对玩家造成伤害（考虑格挡）
             if (amount <= 0) return;                                    // 非正伤害跳过
             DamagePacket dmg;                                           // 伤害包
             dmg.raw_amount      = amount;                               // 原始数值
             dmg.modified_amount = amount;                               // 结算用数值
             dmg.ignore_block    = false;                                // 尊重玩家格挡
             dmg.can_be_reduced  = true;                                 // 可无实体等减伤
             dmg.source_type     = DamagePacket::SourceType::Status;     // 来源记为状态效果
             dmg.from_attack     = false;                                // 非攻击牌伤害（不吃易伤/虚弱攻击修正）
             apply_damage_to_player(dmg);                                // 走统一玩家受伤管线
         },
         [this](int b) { add_block_to_player_impl(b); }                  // 残像等：经统一入口叠格挡以触发势不可挡
     };
     modifiers_.on_card_played(state_, played_id, target_monster_index, &card_ctx);  // 广播：打出卡牌（勒脖等）

    if (cd && cd->cardType == CardType::Attack)
        apply_status_to_player_impl("attacks_played_this_turn", 1, -1);  // 终结技等：本回合打出攻击牌次数
 
    int execute_times = 1;
    if (cd && cd->cardType == CardType::Skill) {
        int burst_stacks = get_status_stacks(state_.player.statuses, "burst");
        if (burst_stacks > 0) {
            execute_times = 2;                                           // 爆发：下一张技能额外执行 1 次
            reduce_status_stacks(state_.player.statuses, "burst", 1);   // 消耗 1 层爆发
        }
    }
    if (cd && cd->cardType == CardType::Attack) {
        int dt = get_status_stacks(state_.player.statuses, "double_tap");
        if (dt > 0) {                                                    // 双发：本回合下 X 张攻击牌各执行 2 次
            execute_times *= 2;
            reduce_status_stacks(state_.player.statuses, "double_tap", 1);
        }
    }
    {                                                                    // 复制：层数 X = 接下来 X 张牌各打出 2 次（与爆发叠乘）
        int dup = get_status_stacks(state_.player.statuses, "duplicate");
        if (dup > 0) {
            execute_times *= 2;
            reduce_status_stacks(state_.player.statuses, "duplicate", 1);
        }
    }
    for (int i = 0; i < execute_times; ++i) {
        card_system_->execute_effect(played_id, ctx);                    // 执行卡牌效果
    }
 
    const bool force_exhaust = corruption_active;                       // 腐化：打出的技能进入消耗堆

    // 能力牌（Power）：按你的设计，打出后只生效一次，不再进入弃牌或消耗堆
    if (cd && cd->cardType == CardType::Power) {
        // 不调用 add_to_discard / add_to_exhaust，直接“从本场战斗中移除”
    } else if ((cd && cd->exhaust) || force_exhaust) {
        card_system_->add_to_exhaust(played);                          // 消耗 / 腐化技能
        apply_exhaust_passives_from_hand(1);
    } else if (cd && cd->retain) {
        const int est = get_status_stacks(state_.player.statuses, "establishment");  // 确立基础层数
        if (est > 0)
            played.combatCostDiscount += est;                          // 累加到该实例
        card_system_->add_to_hand(played);                              // 保留：回到手牌
    } else {
        card_system_->add_to_discard(played);                          // 加入弃牌堆
    }
    panache_on_any_card_played();                                        // 神气制胜：每 5 张牌触发一次
    ++state_.player.cardsPlayedThisTurn;                                 // 本回合成功打出计数（凡庸）
     return true;                                                       // 打出成功
 }

void BattleEngine::apply_exhaust_passives_from_hand(int count) {
    if (count <= 0) return;
    const int de = get_status_stacks(state_.player.statuses, "dark_embrace");
    if (de > 0) draw_cards_impl(de * count);
    const int fnp = get_status_stacks(state_.player.statuses, "feel_no_pain");
    if (fnp > 0) add_block_to_player_impl(get_effective_block_for_player_impl(fnp * count));
}
 
 void BattleEngine::end_turn() {                                        // 结束回合
     if (state_.phase != BattleState::TurnPhase::Idle) return;          // 非空闲阶段则忽略
     handle_player_turn_end();                                          // 处理玩家回合结束
     state_.phase = BattleState::TurnPhase::EnemyTurnStart;               // 切换到敌方回合开始
 }
 
 bool BattleEngine::is_battle_over() const {                            // 战斗是否结束
     if (state_.player.currentHp <= 0) return true;                     // 玩家死亡则结束
     bool all_dead = true;                                              // 假设怪物全灭
     for (const auto& m : state_.monsters) {                            // 遍历怪物
         if (m.currentHp > 0) {                                         // 有存活怪物
             all_dead = false;                                           // 未结束
             break;
         }
     }
     return all_dead;                                                   // 返回是否全灭
 }

 bool BattleEngine::is_victory() const {                               // 是否胜利（玩家存活且怪物全灭）
     if (state_.player.currentHp <= 0) return false;                    // 玩家死亡为失败
     for (const auto& m : state_.monsters) {
         if (m.currentHp > 0) return false;                            // 有存活怪物未胜利
     }
     return true;
 }

 namespace {
     void apply_relic_pickup_effect(PlayerBattleState& p, const RelicId& id) {  // 遗物拾起时的效果（草莓、药水腰带等）
         if (id == "strawberry") {                                     // 草莓：拾起时，最大生命值 +7
             p.maxHp += 7;                                             // 提升 7 点最大生命
             p.currentHp += 7;                                         // 同时回复 7 点当前生命（杀戮尖塔规则）
             if (p.currentHp > p.maxHp) p.currentHp = p.maxHp;          // 不超过最大生命
         } else if (id == "potion_belt") {                             // 药水腰带：拾起时，获得 2 个药水栏位
             p.potionSlotCount += 2;                                   // 药水槽位 +2
         }
     }
     void revert_relic_discard_effect(PlayerBattleState& p, const RelicId& id) {  // 遗物丢弃时回滚效果
         if (id == "strawberry") {                                     // 草莓：丢弃时，最大生命 -7
             p.maxHp -= 7;                                             // 回滚最大生命
             if (p.maxHp < 1) p.maxHp = 1;                             // 不低于 1
             if (p.currentHp > p.maxHp) p.currentHp = p.maxHp;         // 当前生命不超过新上限
         } else if (id == "potion_belt") {                             // 药水腰带：丢弃时，药水槽位 -2
             p.potionSlotCount -= 2;                                    // 回滚药水槽位
             if (p.potionSlotCount < 1) p.potionSlotCount = 1;         // 不低于 1
             while (static_cast<int>(p.potions.size()) > p.potionSlotCount)  // 超出槽位的药水需丢弃
                 p.potions.pop_back();                                  // 从末尾移除（可改为让玩家选择）
         }
     }
 }

 RelicId BattleEngine::grant_reward_relic() {
     static const std::vector<RelicId> relic_pool = {
        "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle", "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull", "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum", "pen_nib", "toy_ornithopter", "preparation_pack", "anchor", "art_of_war", "relic_strength_plus"
     };
     const auto& owned = state_.player.relics;
     std::vector<RelicId> available;
     for (const auto& id : relic_pool) {
         if (std::find(owned.begin(), owned.end(), id) == owned.end())
             available.push_back(id);
     }
     if (available.empty()) return {};
     RelicId id = available[run_rng_->uniform_size(0, available.size() - 1)];
     state_.player.relics.push_back(id);
     apply_relic_pickup_effect(state_.player, id);                     // 统一处理拾起效果（草莓、药水腰带等）
     return id;
 }

 bool BattleEngine::remove_relic(RelicId id) {                         // 丢弃遗物并回滚其拾起效果
     auto& relics = state_.player.relics;
     auto it = std::find(relics.begin(), relics.end(), id);
     if (it == relics.end()) return false;                             // 未找到则返回失败
     relics.erase(it);                                                 // 移除第一个匹配项
     revert_relic_discard_effect(state_.player, id);                   // 回滚拾起时的效果
     return true;
 }

 void BattleEngine::add_card_to_master_deck(CardId id) {               // 往牌组加入一张牌（奖励/商店等），触发陶瓷小鱼等遗物
     if (card_system_) card_system_->add_to_master_deck(std::move(id));  // 加入永久牌组（非抽牌）
     if (std::find(state_.player.relics.begin(), state_.player.relics.end(), "ceramic_fish") != state_.player.relics.end())
         state_.player.gold += 9;                                       // 陶瓷小鱼：每次加入牌组获得 9 金币
 }

 PotionId BattleEngine::grant_reward_potion() {
     static const std::vector<PotionId> potion_pool = {
         "strength_potion", "block_potion", "energy_potion", "poison_potion", "weak_potion", "fear_potion"
     };
     if (potion_pool.empty()) return {};
     if (static_cast<int>(state_.player.potions.size()) >= state_.player.potionSlotCount) return {};
     const auto& owned = state_.player.potions;
     std::vector<PotionId> available;
     for (const auto& id : potion_pool) {
         if (std::find(owned.begin(), owned.end(), id) == owned.end())
             available.push_back(id);
     }
     if (available.empty()) return {};
     PotionId id = available[run_rng_->uniform_size(0, available.size() - 1)];
     state_.player.potions.push_back(id);
     return id;
 }

 std::vector<CardId> BattleEngine::get_reward_cards(int count) {        // 获取奖励卡牌（随机从牌池取）
     static const std::vector<CardId> reward_pool = {                    // 奖励卡池：所有已注册效果的卡牌
         "strike", "strike+", "defend", "defend+", "bash", "bash+",
         "anger", "anger+", "iron_wave", "iron_wave+", "clothesline", "clothesline+",
         "cleave", "cleave+", "dropkick", "dropkick+", "carnage", "carnage+",
         "twin_strike", "twin_strike+", "body_slam", "body_slam+",
         "pommel_strike", "pommel_strike+", "pummel", "pummel+",
         "heavy_blade", "heavy_blade+", "hemokinesis", "hemokinesis+",
         "thunderclap", "thunderclap+", "bludgeon", "bludgeon+",
         "card_001", "card_001+", "card_002", "card_002+", "card_003", "card_003+",
         "card_004", "card_004+", "card_005", "card_005+", "card_006", "card_006+",
         "card_007", "card_007+", "card_008", "card_008+", "card_009", "card_009+",
         "card_010", "card_010+", "card_011", "card_011+", "card_012", "card_012+",
         "card_013", "card_013+", "card_014", "card_014+", "card_016", "card_016+",
         "card_017", "card_017+", "card_018", "card_018+",
        "card_015", "card_015+", "card_019", "card_019+", "card_020", "card_020+",
        "card_021", "card_021+", "card_022", "card_022+", "card_023", "card_023+",
        "card_024", "card_024+",
        "feed", "feed+", "fiend_fire", "fiend_fire+", "whirlwind", "whirlwind+",
        "reaper", "reaper+",
        "burst", "burst+", "corpse_explosion", "corpse_explosion+", "after_image", "after_image+",
       "wraith_form", "wraith_form+", "apotheosis", "apotheosis+", "master_of_strategy", "master_of_strategy+",
       "armaments", "armaments+", "burning_pact", "burning_pact+", "exhume", "exhume+",
       "true_grit", "true_grit+", "survivor", "survivor+", "acrobatics", "acrobatics+",
       "flame_barrier", "flame_barrier+", "second_wind", "second_wind+",
       "spot_weakness", "spot_weakness+", "prepared", "prepared+",
       "all_out_attack", "all_out_attack+", "calculated_gamble", "calculated_gamble+",
       "concentrate", "concentrate+", "piercing_wail", "piercing_wail+",
        "sneaky_strike", "sneaky_strike+",         "escape_plan", "escape_plan+",
        "eviscerate", "eviscerate+", "finisher", "finisher+",
        "bouncing_flask", "bouncing_flask+",
        "grand_finale", "grand_finale+", "venomology", "venomology+",
       "noxious_fumes", "noxious_fumes+", "reflex", "reflex+", "tactician", "tactician+",
       "tools_of_the_trade", "tools_of_the_trade+", "well_laid_plans", "well_laid_plans+",
       "sweeping_beam", "sweeping_beam+", "leap", "leap+",
       "charge_battery", "charge_battery+", "boot_sequence", "boot_sequence+",
       "skim", "skim+", "steam_power", "steam_power+",
       "beam_cell", "beam_cell+", "core_surge", "core_surge+",
       "turbo", "turbo+", "auto_shields", "auto_shields+",
       "rebound", "rebound+", "double_energy", "double_energy+",
        "stack", "stack+", "aggregate", "aggregate+",
        "footwork", "footwork+", "accuracy", "accuracy+",
        "bite", "bite+", "flash_of_steel", "flash_of_steel+", "finesse", "finesse+",
        "hand_of_greed", "hand_of_greed+", "panache", "panache+",
        "deep_breath", "deep_breath+",
        "disarm", "disarm+", "dark_shackles", "dark_shackles+",
        "panacea", "panacea+",
        "the_bomb", "the_bomb+",
        "barricade", "barricade+", "corruption", "corruption+",
        "dark_embrace", "dark_embrace+", "evolve", "evolve+",
        "feel_no_pain", "feel_no_pain+",
        "expunger", "expunger+", "purity", "purity+", "violence", "violence+",
        "jax", "jax+", "insight", "insight+", "chrysalis", "chrysalis+",
        "ritual_dagger", "ritual_dagger+", "secret_technique", "secret_technique+", "ghostly", "ghostly+"
     };
     std::vector<CardId> result;
     if (count <= 0 || reward_pool.empty()) return result;
     // 按类型分层后轮流取牌（攻击牌每轮多取 1 张以提高出现率），再补满张数。
     std::vector<CardId> atk, skl, pwr, oth;
     atk.reserve(reward_pool.size());
     skl.reserve(reward_pool.size());
     pwr.reserve(reward_pool.size());
     oth.reserve(reward_pool.size());
     for (const auto& id : reward_pool) {
         const CardData* cd = get_card_by_id_ ? get_card_by_id_(id) : nullptr;
         if (!cd) cd = get_card_by_id(id);
         if (!cd) continue;
         switch (cd->cardType) {
         case CardType::Attack: atk.push_back(id); break;
         case CardType::Skill: skl.push_back(id); break;
         case CardType::Power: pwr.push_back(id); break;
         default: oth.push_back(id); break;
         }
     }
     std::shuffle(atk.begin(), atk.end(), *run_rng_);
     std::shuffle(skl.begin(), skl.end(), *run_rng_);
     std::shuffle(pwr.begin(), pwr.end(), *run_rng_);
     std::shuffle(oth.begin(), oth.end(), *run_rng_);
     size_t ia = 0, is = 0, ip = 0, io = 0;
     result.reserve(static_cast<size_t>(count));
     while (static_cast<int>(result.size()) < count &&
            (ia < atk.size() || is < skl.size() || ip < pwr.size() || io < oth.size())) {
         if (ia < atk.size()) result.push_back(atk[ia++]);
         if (static_cast<int>(result.size()) >= count) break;
         if (ia < atk.size()) result.push_back(atk[ia++]);
         if (static_cast<int>(result.size()) >= count) break;
         if (is < skl.size()) result.push_back(skl[is++]);
         if (static_cast<int>(result.size()) >= count) break;
         if (ip < pwr.size()) result.push_back(pwr[ip++]);
         if (static_cast<int>(result.size()) >= count) break;
         if (io < oth.size()) result.push_back(oth[io++]);
     }
     if (static_cast<int>(result.size()) < count) {
         std::vector<CardId> rest;
         for (; ia < atk.size(); ++ia) rest.push_back(atk[ia]);
         for (; is < skl.size(); ++is) rest.push_back(skl[is]);
         for (; ip < pwr.size(); ++ip) rest.push_back(pwr[ip]);
         for (; io < oth.size(); ++io) rest.push_back(oth[io]);
         std::shuffle(rest.begin(), rest.end(), *run_rng_);
         for (size_t i = 0; i < rest.size() && static_cast<int>(result.size()) < count; ++i)
             result.push_back(rest[i]);
     }
     while (static_cast<int>(result.size()) < count)
         result.push_back(reward_pool[run_rng_->uniform_size(0, reward_pool.size() - 1)]);
     return result;
 }

 void BattleEngine::grant_victory_gold() {                              // 根据击败怪物类型发放金币
     int gold = get_victory_gold();
     if (gold > 0) state_.player.gold += gold;
 }

 int BattleEngine::get_victory_gold() const {                           // 计算本场胜利金币（不修改状态）
     int gold = 0;
     for (const auto& m : state_.monsters) {
         const MonsterData* md = get_monster_by_id_ ? get_monster_by_id_(m.id) : nullptr;
         MonsterType t = md ? md->type : MonsterType::Normal;
         if (t == MonsterType::Normal) gold += 15;
         else if (t == MonsterType::Elite) gold += 35;
         else if (t == MonsterType::Boss) gold += 100;
     }
     return gold;
 }
 
bool BattleEngine::use_potion(int slot_index, int target_monster_index) {  // 使用药水
    if (slot_index < 0 || slot_index >= state_.player.potionSlotCount) return false;  // 槽位无效
    if (static_cast<size_t>(slot_index) >= state_.player.potions.size()) return false;  // 该槽无药水

    PotionId id = state_.player.potions[static_cast<size_t>(slot_index)];  // 药水 ID
    // 需目标的药水：目标无效则不消耗、返回 false
    if (potion_requires_monster_target(id)) {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state_.monsters.size())
            return false;
        if (state_.monsters[static_cast<size_t>(target_monster_index)].currentHp <= 0)
            return false;
    }
    apply_potion_effect(id, state_, target_monster_index, card_system_);  // 执行药水效果
    state_.player.potions.erase(state_.player.potions.begin() + slot_index);  // 移除药水
    modifiers_.on_potion_used(state_, id);                             // 广播：使用药水后（玩具扑翼飞机回复生命等）
    build_modifiers_from_state();                                      // 重建 modifier（状态可能变化）
    return true;                                                       // 使用成功
}
 
 void BattleEngine::step_turn_phase() {                                 // 推进回合阶段
     switch (state_.phase) {                                            // 根据当前阶段
     case BattleState::TurnPhase::PlayerTurnStart:                      // 玩家回合开始
         handle_player_turn_start();                                    // 抽牌、重置能量、中毒等
         state_.phase = BattleState::TurnPhase::Idle;                   // 进入空闲（可出牌）
         break;
     case BattleState::TurnPhase::EnemyTurnStart:                        // 敌方回合开始
         handle_enemy_turn_start();                                     // 怪物中毒等
         state_.phase = BattleState::TurnPhase::EnemyTurnActions;        // 进入怪物行动
         break;
     case BattleState::TurnPhase::EnemyTurnActions:                     // 怪物行动
         handle_enemy_turn_actions();                                   // 执行怪物行动
         state_.phase = BattleState::TurnPhase::EnemyTurnEnd;            // 进入敌方回合结束
         break;
     case BattleState::TurnPhase::EnemyTurnEnd:                         // 敌方回合结束
         handle_enemy_turn_end();                                       // 格挡清零、duration 递减等
         state_.phase = BattleState::TurnPhase::PlayerTurnStart;         // 下一回合开始
         ++state_.turnNumber;                                            // 回合数 +1
         break;
     case BattleState::TurnPhase::PlayerTurnEnd:                        // 玩家回合结束（未使用）
     case BattleState::TurnPhase::Idle:                                 // 空闲
     default:
         break;
     }
 }
 
void BattleEngine::apply_damage_to_player(DamagePacket& dmg) {         // 对玩家施加伤害
     if (dmg.modified_amount <= 0) return;                              // 无伤害则跳过

    // 怪物攻击统一伤害入口：先叠怪物 strength（一次），再交给 modifier 链处理 weak/vulnerable/strength_down 等。
    if (dmg.source_type == DamagePacket::SourceType::Monster && dmg.from_attack) {
        const int idx = dmg.source_monster_index;
        if (idx >= 0 && static_cast<size_t>(idx) < state_.monsters.size()) {
            dmg.modified_amount += get_status_stacks(state_.monsters[static_cast<size_t>(idx)].statuses, "strength");
            if (dmg.modified_amount < 0) dmg.modified_amount = 0;
        }
    }
    modifiers_.on_monster_deal_damage(dmg, state_);                    // 广播：怪物造成伤害前（虚弱、易伤等）

    int totalDamage = dmg.modified_amount;                              // 总伤害（含被格挡部分）
    if (get_status_stacks(state_.player.statuses, "intangible") > 0 && totalDamage > 1) {
        totalDamage = 1;                                                // 无实体：伤害最多为 1
    }
     if (totalDamage < 0) totalDamage = 0;                               // 确保非负

     int hpDamage = totalDamage;                                         // 实际扣血量
     if (!dmg.ignore_block) {                                           // 若不无视格挡
         int absorbed = (hpDamage < state_.player.block) ? hpDamage : state_.player.block;  // 格挡吸收量
         state_.player.block -= absorbed;                               // 减少格挡
         hpDamage -= absorbed;                                          // 剩余伤害
     }

    modifiers_.on_player_before_hp_loss(hpDamage, state_, dmg);          // 缓冲等：可令本次实际掉血为 0（任意来源）

    state_.player.currentHp -= hpDamage;                               // 扣血
    if (state_.player.currentHp < 0) state_.player.currentHp = 0;       // 不低于 0
    if (hpDamage > 0) state_.pendingDamageDisplays.push_back(DamageDisplayEvent{true, -1, hpDamage, 180});

     DamageAppliedContext dmg_ctx;                                      // 伤害结算后上下文（百年积木抽牌等）
     dmg_ctx.damage_to_player = true;                                   // 标记：伤害施加给玩家
     dmg_ctx.hp_damage_to_player = hpDamage;                            // 玩家实际扣血量（格挡后）
     dmg_ctx.draw_cards = [this](int n) { draw_cards_impl(n); };  // 抽牌回调（尊重不能抽牌）
     modifiers_.on_damage_applied(dmg, state_, &dmg_ctx);                 // 广播：伤害结算后
     if (state_.player.currentHp <= 0) {                                // 若玩家死亡
         on_player_just_died();                                          // 触发玩家死亡
     }
 }
 
void BattleEngine::apply_damage_to_monster(DamagePacket& dmg) {        // 对怪物施加伤害
     if (dmg.modified_amount <= 0) return;                              // 无伤害则跳过

     //modifiers_.on_player_deal_damage(dmg, state_);                      // 广播：玩家造成伤害前（力量、易伤等）

     int idx = dmg.target_monster_index;                                // 目标怪物下标
     if (idx < 0 || idx >= static_cast<int>(state_.monsters.size())) return;  // 下标无效

     auto& m = state_.monsters[static_cast<size_t>(idx)];               // 目标怪物引用

    int totalDamage = dmg.modified_amount;                             // 总伤害（含被格挡部分）
    if (get_status_stacks(m.statuses, "intangible") > 0 && totalDamage > 1) {
        totalDamage = 1;                                               // 无实体：伤害最多为 1
    }
     if (totalDamage < 0) totalDamage = 0;                             // 确保非负

     int hpDamage = totalDamage;                                        // 格挡后拟扣生命
     if (!dmg.ignore_block) {                                           // 若不无视格挡
         int absorbed = (hpDamage < m.block) ? hpDamage : m.block;      // 格挡吸收量
         m.block -= absorbed;                                           // 减少格挡
         hpDamage -= absorbed;                                          // 剩余伤害
     }

     const int hpAfterBlock = hpDamage;                                 // 坚不可摧等：在扣血前再修正
     modifiers_.on_monster_before_hp_loss(hpDamage, state_, idx, dmg);
     if (get_status_stacks(m.statuses, "indestructible") > 0)
         m.indestructibleDamageTakenThisTurn += hpDamage;              // 统计本回合已实际失去的生命

    m.currentHp -= hpDamage;                                          // 扣血
    if (m.currentHp < 0) m.currentHp = 0;                              // 不低于 0
     const int damageDisplayAmount = (totalDamage - hpAfterBlock) + hpDamage;  // 格挡吸收 + 实际掉血（与飘字一致）
     if (damageDisplayAmount > 0) state_.pendingDamageDisplays.push_back(DamageDisplayEvent{false, idx, damageDisplayAmount, 180});

     modifiers_.on_damage_applied(dmg, state_, nullptr);                 // 广播：伤害施加给怪物
     if (m.currentHp <= 0) {                                            // 若怪物死亡
         on_monster_just_died(idx);                                      // 触发怪物死亡（尸体爆炸等）
     }
 }
 
 void BattleEngine::handle_player_turn_start() {                        // 玩家回合开始
    reduce_status_stacks(state_.player.statuses, "discarded_this_turn", 9999);  // 新回合重置“本回合已弃牌”计数
    reduce_status_stacks(state_.player.statuses, "attacks_played_this_turn", 9999);
    reduce_status_stacks(state_.player.statuses, "panache_counter", 9999);
    for (auto& m : state_.monsters) {                                 // 每玩家回合开始重置与「本回合」相关的怪物计数
        m.flightAttackHitsThisTurn = 0;
        m.indestructibleDamageTakenThisTurn = 0;
    }
     int draw_count = state_.player.cardsToDrawPerTurn;                 // 本回合抽牌数
     int energy      = state_.player.maxEnergy;                         // 本回合能量
    if (state_.turnNumber == 1 && card_system_) {
        // 首回合：固有牌已在 init_deck 预放入手牌，首回合抽牌需要扣除这些已在手的牌。
        int already_in_hand = static_cast<int>(card_system_->get_hand().size());
        draw_count -= already_in_hand;
        if (draw_count < 0) draw_count = 0;
    }
 
     TurnStartContext ctx{                                              // 回合开始上下文
         draw_count,                                                    // 抽牌数（可被 modifier 修改）
         energy,                                                        // 能量（可被 modifier 修改）
         [this](int amount) {                                            // 对玩家造成无视格挡伤害（中毒等）
             if (amount <= 0) return;
             DamagePacket dmg;
             dmg.modified_amount = amount;
             dmg.ignore_block    = true;
             dmg.can_be_reduced  = true;
             dmg.source_type     = DamagePacket::SourceType::Status;
             apply_damage_to_player(dmg);
         },
         [this](CardId id) {                                              // 生成牌入手牌（其它效果用）
             if (card_system_) card_system_->generate_to_hand(std::move(id));
         },
         [this]() {                                                       // 渎神者：直接致死并广播死亡（不经缓冲）
             state_.player.currentHp = 0;                                // 当前生命置零
             on_player_just_died();                                       // 触发玩家死亡 modifier 链
         },
         [this](int n) {                                                  // 你好：随机普通稀有度牌入手（临时牌）
             if (n <= 0 || !card_system_) return;                         // 无效数量或无牌堆则跳过
             std::vector<CardId> pool;                                   // 候选：全部已加载且稀有度为普通的卡
             for (const CardId& cid : get_all_card_ids()) {               // 遍历数据层中的卡牌 id
                 const CardData* cd = get_card_by_id_ ? get_card_by_id_(cid) : get_card_by_id(cid);  // 解析静态数据
                 if (!cd || cd->rarity != Rarity::Common) continue;      // 仅保留普通稀有度
                 pool.push_back(cid);                                     // 加入候选池
             }
             if (pool.empty()) return;                                    // 无可用卡则不做任何事
             std::random_device rd;                                       // 非确定性种子（与战斗内洗牌相互独立）
             std::mt19937 gen(rd());                                      // Mersenne Twister 生成器
             std::uniform_int_distribution<std::size_t> dist(0, pool.size() - 1u);  // 闭区间下标
             for (int i = 0; i < n; ++i)                                  // 独立随机 n 次（可重复抽到同 id）
                 card_system_->generate_to_hand(pool[dist(gen)]);         // 临时生成并尝试置入手牌
         },
         [this](int b) { add_block_to_player_impl(b); }                   // 统一加格挡入口（下回合格挡+、势不可挡等）
     };
 
     modifiers_.on_turn_start_player(state_, &ctx);                      // 广播：玩家回合开始（中毒扣血、抽牌修改等）
     if (state_.player.currentHp <= 0) return;                           // 渎神等已在回合初致死，不再抽牌/叠力量
     build_modifiers_from_state();                                       // 回合开始可能改变力量等（恶魔形态等），重建以同步 StrengthModifier
     state_.player.energy = energy;                                    // 应用能量（modifier 可能已修改，如 4/3）
     if (draw_count < 0) draw_count = 0;                                // 抽牌数不低于 0
    draw_cards_impl(draw_count);                                         // 抽牌（经引擎：虚空抽到时扣能量等）
    // 必备工具：回合开始额外抽 1 并弃 1（按层数重复）
    int tools = get_status_stacks(state_.player.statuses, "essential_tools");
    if (tools > 0) {
        draw_cards_impl(tools);
        discard_random_hand_cards_impl(tools);
    }
 }
 
void BattleEngine::apply_curse_hand_effects_before_turn_end_discard() {
    if (!card_system_) return;
    const auto& hand = card_system_->get_hand();
    const int hand_n = static_cast<int>(hand.size());
    int decay_n = 0, doubt_n = 0, regret_n = 0, shame_n = 0;
    for (const auto& c : hand) {
        if (is_decay_id(c.id)) ++decay_n;
        else if (is_doubt_id(c.id)) ++doubt_n;
        else if (is_regret_id(c.id)) ++regret_n;
        else if (is_shame_id(c.id)) ++shame_n;
    }
    if (decay_n > 0) {
        DamagePacket d;
        d.modified_amount = decay_n * 2;
        d.ignore_block    = true;
        d.can_be_reduced  = true;
        d.source_type     = DamagePacket::SourceType::Status;
        apply_damage_to_player(d);
    }
    for (int i = 0; i < doubt_n; ++i)
        apply_status_to_player_impl("weak", 1, 1);
    if (regret_n > 0 && hand_n > 0) {
        DamagePacket d;
        d.modified_amount = regret_n * hand_n;
        d.ignore_block    = true;
        d.can_be_reduced  = true;
        d.source_type     = DamagePacket::SourceType::Status;
        apply_damage_to_player(d);
    }
    for (int i = 0; i < shame_n; ++i)
        apply_status_to_player_impl("frail", 1, 1);
    for (const auto& c : hand) {
        if (is_pride_id(c.id))
            generate_to_draw_pile_impl(c.id);                            // 傲慢：复制洗入抽牌堆顶（与抽牌从顶取一致）
    }
    int burn_total = 0;
    for (const auto& c : hand) {
        burn_total += burn_end_turn_damage_for_id(c.id);
    }
    if (burn_total > 0) {
        DamagePacket bd;
        bd.modified_amount = burn_total;
        bd.ignore_block    = true;
        bd.can_be_reduced  = true;
        bd.source_type     = DamagePacket::SourceType::Status;
        apply_damage_to_player(bd);
    }
}

 void BattleEngine::handle_player_turn_end() {                          // 玩家回合结束
    apply_curse_hand_effects_before_turn_end_discard();                 // 诅咒：回合结束时仍在手牌则结算（在弃牌前）
    bool skip_full_hand_discard = false;                               // 均衡等：本回合是否跳过「非保留牌入弃牌堆」
    modifiers_.on_before_player_hand_discard(state_, skip_full_hand_discard);  // 询问 modifier（均衡会置 true 并扣层数）
     if (card_system_) {                                                // 若有卡牌系统
        int keep_quota = get_status_stacks(state_.player.statuses, "well_planned");  // 周密计划：额外保留张数
         const auto& hand = card_system_->get_hand();                    // 获取手牌
         for (int i = static_cast<int>(hand.size()) - 1; i >= 0; --i) {  // 从后往前遍历（避免下标错位）
             const CardData* cd = get_card_by_id_ ? get_card_by_id_(hand[static_cast<size_t>(i)].id) : nullptr;  // 卡牌数据
             if (cd && cd->retain) continue;                            // 保留牌不移除
             if (skip_full_hand_discard) {                              // 均衡激活：整手视同保留，但虚无仍须消耗
                 if (!cd || !cd->ethereal) continue;                    // 非虚无牌留在手上
                 auto c = card_system_->remove_from_hand(i);             // 虚无牌从手牌移除
                 card_system_->add_to_exhaust(c);                        // 送入消耗堆
                 continue;                                               // 处理下一索引
             }
            if (keep_quota > 0) {                                      // 周密计划：额外保留非保留牌
                --keep_quota;
                continue;
            }
             auto c = card_system_->remove_from_hand(i);                 // 从手牌移除
             if (cd && cd->ethereal)                                     // 若为虚无牌
                 card_system_->add_to_exhaust(c);                        // 加入消耗堆
             else
                 card_system_->add_to_discard(c);                        // 否则加入弃牌堆
         }
     }
    PlayerTurnEndContext end_ctx{
        [this](int monster_index, int amount) {
            if (amount <= 0 || monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;
            DamagePacket dmg;
            dmg.modified_amount = amount;
            dmg.ignore_block = true;
            dmg.can_be_reduced = true;
            dmg.source_type = DamagePacket::SourceType::Status;
            dmg.target_monster_index = monster_index;
            apply_damage_to_monster(dmg);
        },
        [this](int amount) {                                              // 缠绕等：对玩家结算伤害（尊重格挡）
            if (amount <= 0 || state_.player.currentHp <= 0) return;
            DamagePacket dmg;
            dmg.raw_amount      = amount;
            dmg.modified_amount = amount;
            dmg.ignore_block    = false;
            dmg.can_be_reduced  = true;
            dmg.source_type     = DamagePacket::SourceType::Status;
            dmg.from_attack     = false;
            apply_damage_to_player(dmg);
        },
        [this](int discount_delta) {                                     // 确立基础：给仍留在手上的保留牌叠减费
            if (!card_system_ || discount_delta <= 0) return;          // 无牌堆或无效增量
            const auto& h = card_system_->get_hand();                    // 弃牌后的当前手牌
            for (int i = 0; i < static_cast<int>(h.size()); ++i) {     // 按索引遍历
                const CardData* cd = get_card_by_id_ ? get_card_by_id_(h[static_cast<size_t>(i)].id) : nullptr;
                if (cd && cd->retain)                                    // 仅处理带保留词条的牌
                    card_system_->add_combat_cost_discount_to_hand_index(i, discount_delta);  // 叠加减费
            }
        },
        [this](int b) { add_block_to_player_impl(b); }                   // 金属化、奥利哈钢等：统一加格挡
    };
    modifiers_.on_turn_end_player(state_, &end_ctx);                    // 广播：玩家回合结束（金属化、炸弹等）
 }
 
 void BattleEngine::handle_enemy_turn_start() {                         // 敌方回合开始
     EnemyTurnContext ctx{                                              // 敌方回合上下文
         [this](int monster_index, int amount) {                         // 对怪物造成无视格挡伤害（中毒等）
             if (amount <= 0 || monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;
             DamagePacket dmg;
             dmg.modified_amount = amount;
             dmg.ignore_block    = true;
             dmg.can_be_reduced  = true;
             dmg.source_type     = DamagePacket::SourceType::Status;
             dmg.target_monster_index = monster_index;
             apply_damage_to_monster(dmg);
         }
     };
     modifiers_.on_turn_start_monsters(state_, &ctx);                    // 广播：敌方回合开始（怪物中毒扣血等）
 }
 
 void BattleEngine::handle_enemy_turn_actions() {                       // 怪物行动
     if (!execute_monster_action_) return;                              // 无回调则跳过
     EffectContext ctx;                                                 // 效果上下文
     fill_effect_context(ctx);                                          // 填充引擎指针
     for (size_t i = 0; i < state_.monsters.size(); ++i) {             // 遍历怪物
         if (state_.monsters[i].currentHp <= 0) continue;               // 已死跳过
         ctx.source_monster_index = static_cast<int>(i);                // 当前行动怪物下标
         ctx.target_monster_index = -1;                                 // 目标由怪物行为设定
         ctx.from_attack = false;                                       // 由怪物行为设定
         execute_monster_action_(state_.monsters[i].id, state_.turnNumber, ctx, static_cast<int>(i));  // 执行怪物行动
     }
 }
 
 void BattleEngine::handle_enemy_turn_end() {                           // 敌方回合结束
     EnemyTurnContext ctx{};                                            // 敌方回合结束上下文
     ctx.kill_monster = [this](int idx) {                                // 消逝归零时由 modifier 调用
         if (idx < 0 || static_cast<size_t>(idx) >= state_.monsters.size()) return;  // 下标无效
         auto& m = state_.monsters[static_cast<size_t>(idx)];           // 目标怪物
         if (m.currentHp <= 0) return;                                  // 已死不再处理
         m.currentHp = 0;                                               // 生命清零
         on_monster_just_died(idx);                                      // 触发尸体爆炸等死亡逻辑
     };
     modifiers_.on_turn_end_monsters(state_, &ctx);                     // 广播：镣铐减力量、duration 递减、消逝等
 }
 
 void BattleEngine::on_monster_just_died(int monsterIndex) {             // 怪物刚死亡时
     MonsterDiedContext ctx{                                            // 怪物死亡上下文
         [this](int idx, int amount) {                                  // 对怪物造成伤害（尸体爆炸等）
             if (amount <= 0 || idx < 0 || idx >= static_cast<int>(state_.monsters.size())) return;
             DamagePacket dmg;
             dmg.modified_amount = amount;
             dmg.ignore_block    = true;
             dmg.can_be_reduced  = true;
             dmg.source_type     = DamagePacket::SourceType::Status;
             dmg.target_monster_index = idx;
             apply_damage_to_monster(dmg);
         },
         false                                                          // 非尸体爆炸触发
     };
     modifiers_.on_monster_died(monsterIndex, state_, &ctx);             // 广播：怪物死亡（尸体爆炸等）
 }
 
 void BattleEngine::on_player_just_died() {                             // 玩家刚死亡时
     modifiers_.on_player_died(state_);                                  // 广播：玩家死亡
 }
 
 void BattleEngine::build_modifiers_from_state() {                      // 根据战斗状态构建 modifier 列表
     modifiers_.clear();                                                // 清空现有 modifier
     modifiers_.add_modifier(create_buffer_modifier(), MOD_PRIORITY_BUFFER_PRE);  // 缓冲：先于遗物，扣血前判定
     int idx = 0;                                                      // 遗物索引
     for (auto& m : create_relic_modifiers(state_.player.relics))      // 遗物 modifier
         modifiers_.add_modifier(std::move(m), MOD_PRIORITY_RELIC + (idx++));  // 按获得顺序
     for (auto& m : create_player_status_modifiers(state_.player))      // 玩家状态 modifier
         modifiers_.add_modifier(std::move(m), MOD_PRIORITY_PLAYER_ST);
     modifiers_.add_modifier(create_blasphemy_modifier(), MOD_PRIORITY_PLAYER_ST + 1);  // 渎神须最后判定回合初处决
     for (auto& m : create_monster_status_modifiers(state_.monsters))   // 怪物状态 modifier
         modifiers_.add_modifier(std::move(m), MOD_PRIORITY_MONSTER_ST);
 }
 
 void BattleEngine::fill_effect_context(EffectContext& ctx) {            // 填充效果上下文
     ctx.engine_ = this;                                                // 设置引擎指针
 }
 
 int BattleEngine::get_status_stacks(const std::vector<StatusInstance>& list, const StatusId& id) {  // 获取状态层数
     for (const auto& s : list) {                                        // 遍历状态列表
         if (s.id == id) return s.stacks;                               // 找到则返回层数
     }
     return 0;                                                          // 未找到返回 0
 }
 
 void BattleEngine::reduce_status_stacks(std::vector<StatusInstance>& list,  // 减少状态层数
                                         const StatusId& id,
                                         int amount) {
     if (amount <= 0) return;                                          // 无效数量跳过
     for (auto it = list.begin(); it != list.end(); ++it) {             // 遍历状态列表
         if (it->id == id) {                                            // 找到目标状态
             it->stacks -= amount;                                      // 减少层数
             if (it->stacks <= 0) list.erase(it);                       // 层数归零则移除
             return;
         }
     }
 }

 void BattleEngine::merge_status_into_list(std::vector<StatusInstance>& list, StatusId id, int stacks, int duration) {
     for (auto& s : list) {
         if (s.id == id) {
             s.stacks += stacks;
             if (duration >= 0) s.duration = duration;
             return;
         }
     }
     list.push_back(StatusInstance{std::move(id), stacks, duration});
 }
 
 void EffectContext::deal_damage_to_player(int amount) {                // 对玩家造成伤害（考虑格挡）
     if (!engine_) return;                                              // 无引擎跳过
     DamagePacket dmg;                                                  // 伤害包
     dmg.raw_amount      = amount;                                      // 原始伤害
     dmg.modified_amount = amount;                                       // 修正后伤害
     dmg.source_type     = DamagePacket::SourceType::Monster;            // 来源：怪物
     dmg.ignore_block    = false;                                       // 考虑格挡
     dmg.can_be_reduced   = true;                                       // 可被减伤
     dmg.source_monster_index = source_monster_index;                    // 攻击者下标
     dmg.from_attack      = from_attack;                                // 是否来自攻击
     engine_->apply_damage_to_player(dmg);                              // 施加伤害
 }
 
 void EffectContext::deal_damage_to_monster(int monster_index, int amount) {  // 对怪物造成伤害（考虑格挡）
     if (!engine_) return;
     DamagePacket dmg;
     dmg.raw_amount        = amount;
     dmg.modified_amount   = amount;
     dmg.source_type       = DamagePacket::SourceType::Player;           // 来源：玩家
     dmg.target_monster_index = monster_index;
     dmg.ignore_block      = false;
     dmg.can_be_reduced     = true;
     dmg.from_attack       = from_attack;
     engine_->apply_damage_to_monster(dmg);
 }
 
 void EffectContext::deal_damage_to_player_ignoring_block(int amount) {  // 对玩家造成无视格挡伤害
     if (!engine_) return;
     DamagePacket dmg;
     dmg.raw_amount      = amount;
     dmg.modified_amount = amount;
     dmg.source_type     = DamagePacket::SourceType::Monster;
     dmg.ignore_block    = true;                                        // 无视格挡
     dmg.can_be_reduced   = true;
     dmg.source_monster_index = source_monster_index;
     dmg.from_attack      = from_attack;
     engine_->apply_damage_to_player(dmg);
 }
 
 void EffectContext::deal_damage_to_monster_ignoring_block(int monster_index, int amount) {  // 对怪物造成无视格挡伤害
     if (!engine_) return;
     DamagePacket dmg;
     dmg.raw_amount        = amount;
     dmg.modified_amount   = amount;
     dmg.source_type       = DamagePacket::SourceType::Player;
     dmg.target_monster_index = monster_index;
     dmg.ignore_block      = true;
     dmg.can_be_reduced     = true;
     dmg.from_attack       = from_attack;
     engine_->apply_damage_to_monster(dmg);
 }
 
 void EffectContext::apply_status_to_player(StatusId id, int stacks, int duration) {  // 对玩家施加状态
     if (!engine_) return;
     engine_->apply_status_to_player_impl(std::move(id), stacks, duration);  // 经 modifier 检查后施加
 }
 
 void EffectContext::apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration) {  // 对怪物施加状态
     if (!engine_) return;
     engine_->apply_status_to_monster_impl(monster_index, std::move(id), stacks, duration);
 }
 void EffectContext::set_monster_status_stacks(int monster_index, StatusId id, int stacks) {  // 设置怪物状态层数（替换）
     if (engine_) engine_->set_monster_status_stacks_impl(monster_index, std::move(id), stacks);
 }
 
 // --- CardEffects 兼容接口实现 ---
 void EffectContext::add_block_to_player(int amount) {                  // 给玩家加格挡
     if (engine_) engine_->add_block_to_player_impl(amount);
 }
 void EffectContext::add_block_to_monster(int monster_index, int amount) {  // 给怪物加格挡
     if (engine_) engine_->add_block_to_monster_impl(monster_index, amount);
 }
 int EffectContext::get_effective_damage_dealt_by_player(int base_damage, int target_monster_index) const {  // 玩家对怪物的有效伤害
     return engine_ ? engine_->get_effective_damage_dealt_by_player_impl(base_damage, target_monster_index, from_attack) : base_damage;
 }
 int EffectContext::get_effective_block_for_player(int base_block) const {  // 玩家有效格挡
     return engine_ ? engine_->get_effective_block_for_player_impl(base_block) : base_block;
 }
 void EffectContext::generate_to_discard_pile(CardId id) {              // 生成卡牌到弃牌堆
     if (engine_) engine_->generate_to_discard_pile_impl(std::move(id));
 }
 void EffectContext::generate_to_draw_pile(CardId id) {                 // 生成卡牌到抽牌堆
     if (engine_) engine_->generate_to_draw_pile_impl(std::move(id));
 }
 void EffectContext::generate_to_draw_pile_combat_zero_skill(CardId id) {
     if (engine_) engine_->generate_to_draw_pile_impl(std::move(id), true);
 }
 int EffectContext::draw_random_attack_cards_from_draw_pile(int max_count) {
     return engine_ ? engine_->draw_random_attack_cards_from_draw_pile_impl(max_count) : 0;
 }
 int EffectContext::draw_random_skill_cards_from_draw_pile(int max_count) {
     return engine_ ? engine_->draw_random_skill_cards_from_draw_pile_impl(max_count) : 0;
 }
 int EffectContext::get_ritual_dagger_run_bonus() const {
     return engine_ ? engine_->get_ritual_dagger_run_bonus_impl() : 0;
 }
 void EffectContext::add_ritual_dagger_run_bonus(int amount) {
     if (engine_) engine_->add_ritual_dagger_run_bonus_impl(amount);
 }
 void EffectContext::generate_to_hand(CardId id) {                      // 生成卡牌到手牌（手牌满则入弃牌堆）
     if (engine_) engine_->generate_to_hand_impl(std::move(id));
 }
 void EffectContext::draw_cards(int n) {                                // 抽牌
     if (engine_) engine_->draw_cards_impl(n);
 }
 void EffectContext::add_energy_to_player(int amount) {                  // 给玩家加能量
     if (engine_) engine_->add_energy_to_player_impl(amount);
 }
 int EffectContext::get_status_stacks_on_monster(int monster_index, const StatusId& id) const {  // 怪物某状态层数
     return engine_ ? engine_->get_status_stacks_on_monster_impl(monster_index, id) : 0;
 }
 int EffectContext::get_status_stacks_on_player(const StatusId& id) const {  // 玩家某状态层数
     return engine_ ? engine_->get_status_stacks_on_player_impl(id) : 0;
 }
int EffectContext::get_monster_current_hp(int monster_index) const {
    return engine_ ? engine_->get_monster_current_hp_impl(monster_index) : 0;
}
int EffectContext::get_monster_count() const {
    return engine_ ? engine_->get_monster_count_impl() : 0;
}
 void EffectContext::apply_status_to_all_monsters(StatusId id, int stacks, int duration) {  // 对全体怪物施加状态
     if (engine_) engine_->apply_status_to_all_monsters_impl(std::move(id), stacks, duration);
 }
 void EffectContext::deal_damage_to_all_monsters(int base_damage) {     // 对全体怪物造成伤害
     if (engine_) engine_->deal_damage_to_all_monsters_impl(base_damage);
 }
void EffectContext::add_player_max_hp(int amount) {
    if (engine_) engine_->add_player_max_hp_impl(amount);
}
void EffectContext::heal_player(int amount) {
    if (engine_) engine_->heal_player_impl(amount);
}
 int EffectContext::get_player_block() const {                          // 获取玩家格挡
     return engine_ ? engine_->get_player_block_impl() : 0;
 }
 int EffectContext::get_player_energy() const {                          // 当前能量
     return engine_ ? engine_->get_player_energy_impl() : 0;
 }
 int EffectContext::get_discard_pile_size() const {                    // 弃牌堆张数
     return engine_ ? engine_->get_discard_pile_size_impl() : 0;
 }
 int EffectContext::get_draw_pile_size() const {                       // 抽牌堆张数
     return engine_ ? engine_->get_draw_pile_size_impl() : 0;
 }
 int EffectContext::get_hand_card_count() const {
     return engine_ ? engine_->get_hand_card_count_impl() : 0;
 }
 void EffectContext::shuffle_discard_into_draw() {
     if (engine_) engine_->shuffle_discard_into_draw_impl();
 }
 bool EffectContext::was_last_hand_card_skill() const {
     return engine_ && engine_->was_last_hand_card_skill_impl();
 }
int EffectContext::exhaust_all_hand_cards() {                         // 消耗全部手牌
    return engine_ ? engine_->exhaust_all_hand_cards_impl() : 0;
}
int EffectContext::discard_random_hand_cards(int n) {                  // 随机弃牌
    return engine_ ? engine_->discard_random_hand_cards_impl(n) : 0;
}
int EffectContext::discard_all_hand_cards() {
    return engine_ ? engine_->discard_all_hand_cards_impl() : 0;
}
int EffectContext::discard_selected_hand_cards(int n) {
    return engine_ ? engine_->discard_selected_hand_cards_impl(n) : 0;
}
int EffectContext::exhaust_random_hand_cards(int n) {                  // 随机消耗手牌
    return engine_ ? engine_->exhaust_random_hand_cards_impl(n) : 0;
}
int EffectContext::exhaust_selected_hand_cards(int n) {
    return engine_ ? engine_->exhaust_selected_hand_cards_impl(n) : 0;
}
int EffectContext::exhaust_non_attack_hand_cards() {                    // 消耗手牌中所有非攻击牌
    return engine_ ? engine_->exhaust_non_attack_hand_cards_impl() : 0;
}
int EffectContext::upgrade_random_cards_in_hand(int n) {               // 随机升级手牌
    return engine_ ? engine_->upgrade_random_cards_in_hand_impl(n) : 0;
}
int EffectContext::upgrade_selected_cards_in_hand(int n) {
    return engine_ ? engine_->upgrade_selected_cards_in_hand_impl(n) : 0;
}
int EffectContext::add_exhaust_selected_to_hand(int n) {
    return engine_ ? engine_->add_exhaust_selected_to_hand_impl(n) : 0;
}
int EffectContext::upgrade_all_cards_in_hand() {                       // 全部升级手牌
    return engine_ ? engine_->upgrade_all_cards_in_hand_impl() : 0;
}
int EffectContext::upgrade_all_cards_in_combat() {                    // 升级战斗内全部牌
    return engine_ ? engine_->upgrade_all_cards_in_combat_impl() : 0;
}
bool EffectContext::any_monster_intends_attack() const {
    return engine_ ? engine_->any_monster_intends_attack_impl() : false;
}
bool EffectContext::target_monster_intends_attack() const {
    return engine_ ? engine_->monster_intends_attack_impl(target_monster_index) : false;
}
PotionId EffectContext::grant_random_potion() {
    return engine_ ? engine_->grant_reward_potion() : PotionId{};
}
void EffectContext::add_gold_to_player(int amount) {
    if (engine_ && amount > 0) engine_->add_gold_to_player_impl(amount);
}

int EffectContext::uniform_int(int lo, int hi) {
    return engine_ ? engine_->run_rng_uniform_int(lo, hi) : lo;
}

size_t EffectContext::uniform_size(size_t lo, size_t hi) {
    return engine_ ? engine_->run_rng_uniform_size(lo, hi) : lo;
}

int BattleEngine::run_rng_uniform_int(int lo, int hi) {
    if (!run_rng_) return lo;
    return run_rng_->uniform_int(lo, hi);
}

size_t BattleEngine::run_rng_uniform_size(size_t lo, size_t hi) {
    if (!run_rng_) return lo;
    return run_rng_->uniform_size(lo, hi);
}

 // --- BattleEngine 内部实现 ---
 void BattleEngine::add_block_to_player_impl(int amount) {               // 给玩家加格挡
     if (amount <= 0) return;                                            // 非正数不加
     state_.player.block += amount;                                      // 先写入格挡
     PlayerBlockGainedContext block_ctx;                                 // 势不可挡等用的上下文
     block_ctx.deal_damage_to_random_living_monster = [this](int dmg) {  // 对随机存活敌人造成伤害
         if (dmg <= 0) return;                                          // 无效伤害跳过
         std::vector<int> alive;                                         // 存活敌人下标列表
         for (int i = 0; i < static_cast<int>(state_.monsters.size()); ++i) {  // 遍历怪物
             if (state_.monsters[static_cast<size_t>(i)].currentHp > 0)  // 仍存活
                 alive.push_back(i);                                    // 记入候选
         }
         if (alive.empty()) return;                                      // 无人可打则返回
         std::random_device rd;                                          // 随机源
         std::mt19937 gen(rd());                                         // 随机引擎
         std::uniform_int_distribution<std::size_t> pick(0, alive.size() - 1u);  // 均匀选下标
         const int idx = alive[pick(gen)];                               // 随机敌人
         DamagePacket packet;                                             // 伤害包
         packet.raw_amount        = dmg;                                 // 原始伤害
         packet.modified_amount   = dmg;                                 // 初始结算值
         packet.source_type       = DamagePacket::SourceType::Player;     // 来源：玩家
         packet.target_monster_index = idx;                              // 目标
         packet.ignore_block      = false;                                // 尊重怪物格挡
         packet.can_be_reduced    = true;                                 // 可无实体等
         packet.from_attack       = false;                                // 非攻击类（不吃攻击向易伤/虚弱）
         apply_damage_to_monster(packet);                                // 走统一对怪伤害
     };
     modifiers_.on_player_block_gained(state_, amount, &block_ctx);     // 广播：已获得格挡（势不可挡）
 }
 void BattleEngine::add_block_to_monster_impl(int monster_index, int amount) {  // 给怪物加格挡
     if (amount <= 0 || monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;
     state_.monsters[static_cast<size_t>(monster_index)].block += amount;
 }
 int BattleEngine::get_effective_damage_dealt_by_player_impl(int base_damage, int target_monster_index, bool from_attack) const {  // 玩家对怪物的有效伤害（经 modifier）
     DamagePacket dmg;                                                  // 构造伤害包
     dmg.raw_amount = base_damage;
     dmg.modified_amount = base_damage;
     dmg.source_type = DamagePacket::SourceType::Player;
     dmg.target_monster_index = target_monster_index;
     dmg.from_attack = from_attack;
     const_cast<ModifierSystem&>(modifiers_).on_player_deal_damage(dmg, state_);  // 经力量、易伤等修正
     return dmg.modified_amount > 0 ? dmg.modified_amount : 0;           // 返回修正后伤害
 }
 int BattleEngine::get_effective_block_for_player_impl(int base_block) const {  // 玩家有效格挡（经 modifier：敏捷、脆弱等）
     int block = base_block;
     const_cast<ModifierSystem&>(modifiers_).on_player_gain_block(block, state_);
     return block > 0 ? block : 0;
 }
 void BattleEngine::generate_to_discard_pile_impl(CardId id) {          // 生成卡牌到弃牌堆
     if (card_system_) card_system_->generate_to_discard_pile(std::move(id));
 }
 void BattleEngine::generate_to_draw_pile_impl(CardId id) {              // 生成卡牌到抽牌堆
     if (card_system_) card_system_->generate_to_draw_pile(std::move(id));
 }
 void BattleEngine::generate_to_draw_pile_impl(CardId id, bool combat_cost_zero_skill) {
     if (card_system_) card_system_->generate_to_draw_pile(std::move(id), combat_cost_zero_skill);
 }
 int BattleEngine::draw_random_attack_cards_from_draw_pile_impl(int max_count) {
     return card_system_ ? card_system_->draw_random_attack_cards_from_draw_pile(max_count) : 0;
 }
 int BattleEngine::draw_random_skill_cards_from_draw_pile_impl(int max_count) {
     return card_system_ ? card_system_->draw_random_skill_cards_from_draw_pile(max_count) : 0;
 }
 int BattleEngine::get_ritual_dagger_run_bonus_impl() const {
     return state_.player.ritualDaggerRunBonus;
 }
 void BattleEngine::add_ritual_dagger_run_bonus_impl(int amount) {
     if (amount == 0) return;
     state_.player.ritualDaggerRunBonus += amount;
     if (state_.player.ritualDaggerRunBonus < 0) state_.player.ritualDaggerRunBonus = 0;
 }
 void BattleEngine::generate_to_hand_impl(CardId id) {                   // 生成卡牌到手牌
     if (card_system_) card_system_->generate_to_hand(std::move(id));
 }
 void BattleEngine::draw_cards_impl(int n) {                            // 抽牌
    if (!card_system_ || n <= 0) return;
    // 战斗专注等效果：本回合内禁止再抽牌
    if (get_status_stacks(state_.player.statuses, "cannot_draw") > 0) return;
    const int evolve_n = get_status_stacks(state_.player.statuses, "evolve");
    int guard = 0;
    while (n > 0 && ++guard < 600) {
        const size_t hand_before = card_system_->get_hand().size();
        card_system_->draw_cards(1);
        --n;
        if (card_system_->get_hand().size() <= hand_before) break;
        const auto& drawn = card_system_->get_hand().back();
        if (drawn.id == "void" || drawn.id == "void+") {                 // 虚空：抽到时失去 1 点能量
            if (state_.player.energy > 0) --state_.player.energy;
        }
        if (evolve_n <= 0) continue;
        const CardData* dcd = get_card_by_id_ ? get_card_by_id_(drawn.id) : nullptr;
        if (dcd && dcd->cardType == CardType::Status)
            n += evolve_n;                                             // 进化：抽到状态牌时再抽 evolve_n 张
    }
 }
 void BattleEngine::add_energy_to_player_impl(int amount) {              // 给玩家加能量（如 3/3+2 → 5/3，当前可超过最大，UI 显示当前/最大）
     if (amount > 0)
         state_.player.energy += amount;
 }
 int BattleEngine::get_status_stacks_on_monster_impl(int monster_index, const StatusId& id) const {  // 怪物某状态层数
     if (monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return 0;
     return get_status_stacks(state_.monsters[static_cast<size_t>(monster_index)].statuses, id);
 }
 int BattleEngine::get_status_stacks_on_player_impl(const StatusId& id) const {  // 玩家某状态层数
     return get_status_stacks(state_.player.statuses, id);
 }
int BattleEngine::get_monster_current_hp_impl(int monster_index) const {
    if (monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return 0;
    return state_.monsters[static_cast<size_t>(monster_index)].currentHp;
}
int BattleEngine::get_monster_count_impl() const {
    return static_cast<int>(state_.monsters.size());
}
static void add_or_merge_status(std::vector<StatusInstance>& list, StatusId id, int stacks, int duration) {
    BattleEngine::merge_status_into_list(list, std::move(id), stacks, duration);
}

void BattleEngine::apply_status_to_player_impl(StatusId id, int stacks, int duration) {  // 对玩家施加状态（经 modifier 检查）
    StatusApplyContext ctx{StatusApplyContext::Target::Player, -1, id, stacks, duration, false, state_};  // 构造上下文
    modifiers_.on_before_status_applied(ctx);                           // 广播：施加前（人工制品可阻止）
    if (!ctx.blocked)
        add_or_merge_status(state_.player.statuses, std::move(ctx.id), ctx.stacks, ctx.duration);
}

void BattleEngine::apply_status_to_monster_impl(int monster_index, StatusId id, int stacks, int duration) {  // 对怪物施加状态
    if (monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;
    if (state_.monsters[static_cast<size_t>(monster_index)].currentHp <= 0) return;  // 已死跳过
    StatusApplyContext ctx{StatusApplyContext::Target::Monster, monster_index, id, stacks, duration, false, state_};
    modifiers_.on_before_status_applied(ctx);
    if (!ctx.blocked)
        add_or_merge_status(state_.monsters[static_cast<size_t>(monster_index)].statuses, std::move(ctx.id), ctx.stacks, ctx.duration);
}

void BattleEngine::set_monster_status_stacks_impl(int monster_index, StatusId id, int stacks) {
    if (monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;
    auto& list = state_.monsters[static_cast<size_t>(monster_index)].statuses;
    list.erase(std::remove_if(list.begin(), list.end(), [&id](const StatusInstance& s) { return s.id == id; }), list.end());
    if (stacks != 0)
        list.push_back(StatusInstance{std::move(id), stacks, -1});
}
 
 void BattleEngine::apply_status_to_all_monsters_impl(StatusId id, int stacks, int duration) {  // 对全体怪物施加状态
     for (size_t i = 0; i < state_.monsters.size(); ++i) {
         if (state_.monsters[i].currentHp > 0) {                        // 存活怪物
             apply_status_to_monster_impl(static_cast<int>(i), id, stacks, duration);
         }
     }
 }
 void BattleEngine::deal_damage_to_all_monsters_impl(int base_damage) {  // 对全体怪物造成伤害
     EffectContext ctx;
     fill_effect_context(ctx);
     ctx.from_attack = true;
     for (size_t i = 0; i < state_.monsters.size(); ++i) {
         if (state_.monsters[i].currentHp > 0) {
             int dmg = get_effective_damage_dealt_by_player_impl(base_damage, static_cast<int>(i), true);  // 经 modifier 修正（群体攻击）
             if (dmg > 0) ctx.deal_damage_to_monster(static_cast<int>(i), dmg);
         }
     }
 }
 int BattleEngine::get_player_block_impl() const {                      // 获取玩家格挡
     return state_.player.block;
 }
 int BattleEngine::get_player_energy_impl() const {                      // 当前能量
     return state_.player.energy;
 }
 int BattleEngine::get_discard_pile_size_impl() const {                 // 弃牌堆张数
     return card_system_ ? card_system_->get_discard_size() : 0;
 }
 int BattleEngine::get_draw_pile_size_impl() const {                    // 抽牌堆张数
     return card_system_ ? card_system_->get_deck_size() : 0;
 }
 int BattleEngine::get_hand_card_count_impl() const {
     return card_system_ ? static_cast<int>(card_system_->get_hand().size()) : 0;
 }
 void BattleEngine::shuffle_discard_into_draw_impl() {
     if (card_system_) card_system_->shuffle_discard_into_draw();
 }
 bool BattleEngine::was_last_hand_card_skill_impl() const {
     if (!card_system_ || card_system_->get_hand().empty()) return false;
     const CardInstance& c = card_system_->get_hand().back();
     const CardData* cd = get_card_by_id_ ? get_card_by_id_(c.id) : nullptr;
     return cd && cd->cardType == CardType::Skill;
 }
int BattleEngine::exhaust_all_hand_cards_impl() {
    if (!card_system_) return 0;
    const int r = card_system_->exhaust_all_hand_cards();
    if (r > 0) apply_exhaust_passives_from_hand(r);
    return r;
}
int BattleEngine::discard_random_hand_cards_impl(int n) {
   if (!card_system_ || n <= 0) return 0;
   int discarded = 0;
   auto trigger_on_discard = [this](const CardId& id) {
       if (id == "reflex") draw_cards_impl(2);
       else if (id == "reflex+") draw_cards_impl(3);
       else if (id == "tactician") add_energy_to_player_impl(1);
       else if (id == "tactician+") add_energy_to_player_impl(2);
   };
   while (n > 0 && !card_system_->get_hand().empty()) {
       int idx = run_rng_->uniform_int(0, static_cast<int>(card_system_->get_hand().size()) - 1);
       CardInstance c = card_system_->remove_from_hand(idx);
       CardId id = c.id;
       card_system_->add_to_discard(std::move(c));
       trigger_on_discard(id);
       --n;
       ++discarded;
   }
   if (discarded > 0) apply_status_to_player_impl("discarded_this_turn", discarded, -1);
   return discarded;
}
int BattleEngine::discard_all_hand_cards_impl() {
   if (!card_system_) return 0;
   int discarded = 0;
   auto trigger_on_discard = [this](const CardId& id) {
       if (id == "reflex") draw_cards_impl(2);
       else if (id == "reflex+") draw_cards_impl(3);
       else if (id == "tactician") add_energy_to_player_impl(1);
       else if (id == "tactician+") add_energy_to_player_impl(2);
   };
   for (int i = static_cast<int>(card_system_->get_hand().size()) - 1; i >= 0; --i) {
       CardInstance c = card_system_->remove_from_hand(i);
       CardId id = c.id;
       card_system_->add_to_discard(std::move(c));
       trigger_on_discard(id);
       ++discarded;
   }
   if (discarded > 0) apply_status_to_player_impl("discarded_this_turn", discarded, -1);
   return discarded;
}
int BattleEngine::discard_selected_hand_cards_impl(int n) {
   if (!card_system_ || n <= 0) return 0;
   int discarded = 0;
   while (n > 0 && !effect_selected_instance_ids_.empty()) {
       InstanceId iid = effect_selected_instance_ids_.front();
       effect_selected_instance_ids_.pop_front();
       int ok = card_system_->discard_hand_card_by_instance_id(iid);
       if (ok > 0) {
           ++discarded;
           --n;
       }
   }
   if (discarded > 0) apply_status_to_player_impl("discarded_this_turn", discarded, -1);
   return discarded;
}
int BattleEngine::exhaust_random_hand_cards_impl(int n) {
   if (!card_system_ || n <= 0) return 0;
   const int r = card_system_->exhaust_random_hand_cards(n);
   if (r > 0) apply_exhaust_passives_from_hand(r);
   return r;
}
int BattleEngine::exhaust_selected_hand_cards_impl(int n) {
   if (!card_system_ || n <= 0) return 0;
   int exhausted = 0;
   while (n > 0 && !effect_selected_instance_ids_.empty()) {
       InstanceId iid = effect_selected_instance_ids_.front();
       effect_selected_instance_ids_.pop_front();
       int ok = card_system_->exhaust_hand_card_by_instance_id(iid);
       if (ok > 0) {
           ++exhausted;
           --n;
       }
   }
   if (exhausted > 0) apply_exhaust_passives_from_hand(exhausted);
   return exhausted;
}
int BattleEngine::exhaust_non_attack_hand_cards_impl() {
   if (!card_system_) return 0;
   const int r = card_system_->exhaust_non_attack_hand_cards();
   if (r > 0) apply_exhaust_passives_from_hand(r);
   return r;
}
int BattleEngine::upgrade_random_cards_in_hand_impl(int n) {
   return card_system_ ? card_system_->upgrade_random_cards_in_hand(n) : 0;
}
int BattleEngine::upgrade_selected_cards_in_hand_impl(int n) {
    if (!card_system_ || n <= 0) return 0;
    int upgraded = 0;
    while (n > 0 && !effect_selected_instance_ids_.empty()) {
        InstanceId iid = effect_selected_instance_ids_.front();
        effect_selected_instance_ids_.pop_front();
        if (card_system_->upgrade_card_in_deck(iid)) {
            ++upgraded;
            --n;
        }
    }
    return upgraded;
}
int BattleEngine::upgrade_all_cards_in_hand_impl() {
   return card_system_ ? card_system_->upgrade_all_cards_in_hand() : 0;
}
int BattleEngine::add_exhaust_selected_to_hand_impl(int n) {
    if (!card_system_ || n <= 0) return 0;
    int moved = 0;
    while (n > 0 && !effect_selected_instance_ids_.empty()) {
        InstanceId iid = effect_selected_instance_ids_.front();
        effect_selected_instance_ids_.pop_front();
        if (card_system_->move_exhaust_card_to_hand(iid)) {
            ++moved;
            --n;
        }
    }
    return moved;
}
int BattleEngine::upgrade_all_cards_in_combat_impl() {
    return card_system_ ? card_system_->upgrade_all_cards_in_combat() : 0;
}
bool BattleEngine::any_monster_intends_attack_impl() const {
    for (const auto& m : state_.monsters) {
        if (m.currentHp <= 0) continue;
        if (m.currentIntent.kind == MonsterIntentKind::Attack || m.currentIntent.kind == MonsterIntentKind::Mul_Attack
            || m.currentIntent.kind == MonsterIntentKind::Attack_And_Block
            || m.currentIntent.kind == MonsterIntentKind::Attack_And_Weak
            || m.currentIntent.kind == MonsterIntentKind::Attack_And_Vulnerable)
            return true;
    }
    return false;
}
bool BattleEngine::monster_intends_attack_impl(int monster_index) const {
    if (monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return false;
    const auto& m = state_.monsters[static_cast<size_t>(monster_index)];
    if (m.currentHp <= 0) return false;
    return m.currentIntent.kind == MonsterIntentKind::Attack || m.currentIntent.kind == MonsterIntentKind::Mul_Attack
        || m.currentIntent.kind == MonsterIntentKind::Attack_And_Block
        || m.currentIntent.kind == MonsterIntentKind::Attack_And_Weak
        || m.currentIntent.kind == MonsterIntentKind::Attack_And_Vulnerable;
}
void BattleEngine::set_effect_selected_instance_ids(const std::vector<InstanceId>& ids) {
    effect_selected_instance_ids_.clear();
    for (InstanceId id : ids) effect_selected_instance_ids_.push_back(id);
}
void BattleEngine::add_player_max_hp_impl(int amount) {
    if (amount <= 0) return;
    state_.player.maxHp += amount;
    state_.player.currentHp += amount;
    if (state_.player.currentHp > state_.player.maxHp) state_.player.currentHp = state_.player.maxHp;
}

void BattleEngine::heal_player_impl(int amount) {
    if (amount <= 0) return;
    state_.player.currentHp += amount;
    if (state_.player.currentHp > state_.player.maxHp) state_.player.currentHp = state_.player.maxHp;
}

void BattleEngine::add_gold_to_player_impl(int amount) {
    if (amount > 0) state_.player.gold += amount;
}

void BattleEngine::panache_on_any_card_played() {
    int dmg = get_status_stacks(state_.player.statuses, "panache");
    if (dmg <= 0) return;
    int c = get_status_stacks(state_.player.statuses, "panache_counter") + 1;
    if (c >= 5) {
        deal_damage_to_all_monsters_impl(dmg);
        c = 0;
    }
    auto& list = state_.player.statuses;
    list.erase(std::remove_if(list.begin(), list.end(), [](const StatusInstance& s) { return s.id == "panache_counter"; }), list.end());
    if (c > 0)
        list.push_back(StatusInstance{StatusId{"panache_counter"}, c, -1});
}

 // --- 金手指接口 ---
 void BattleEngine::cheat_set_player_hp(int hp) {
     state_.player.currentHp = (hp < 0) ? 0 : hp;
     if (state_.player.currentHp > state_.player.maxHp) state_.player.currentHp = state_.player.maxHp;
 }
 void BattleEngine::cheat_set_player_max_hp(int hp) {
     if (hp < 1) hp = 1;
     state_.player.maxHp = hp;
     if (state_.player.currentHp > hp) state_.player.currentHp = hp;
 }
 void BattleEngine::cheat_set_player_block(int b) {
     state_.player.block = (b < 0) ? 0 : b;
 }
 void BattleEngine::cheat_set_player_energy(int e) {
     state_.player.energy = (e < 0) ? 0 : e;
     // 作弊允许当前能量超过最大（如 10/3），便于测试，与 add_energy_to_player_impl 行为一致
 }
 void BattleEngine::cheat_set_player_gold(int g) {
     state_.player.gold = (g < 0) ? 0 : g;
 }
 void BattleEngine::cheat_set_player_status(StatusId id, int stacks, int duration) {
     auto it = std::find_if(state_.player.statuses.begin(), state_.player.statuses.end(),
         [&id](const StatusInstance& s) { return s.id == id; });
     if (it != state_.player.statuses.end()) {
         it->stacks = stacks;
         it->duration = duration;
     } else {
         state_.player.statuses.push_back(StatusInstance{std::move(id), stacks, duration});
     }
 }
 void BattleEngine::cheat_remove_player_status(StatusId id) {
     state_.player.statuses.erase(
         std::remove_if(state_.player.statuses.begin(), state_.player.statuses.end(),
             [&id](const StatusInstance& s) { return s.id == id; }),
         state_.player.statuses.end());
 }
 void BattleEngine::cheat_set_monster_hp(int idx, int hp) {
     if (idx < 0 || static_cast<size_t>(idx) >= state_.monsters.size()) return;
     state_.monsters[static_cast<size_t>(idx)].currentHp = (hp < 0) ? 0 : hp;
 }
 void BattleEngine::cheat_set_monster_max_hp(int idx, int hp) {
     if (idx < 0 || static_cast<size_t>(idx) >= state_.monsters.size()) return;
     if (hp < 1) hp = 1;
     state_.monsters[static_cast<size_t>(idx)].maxHp = hp;
     if (state_.monsters[static_cast<size_t>(idx)].currentHp > hp)
         state_.monsters[static_cast<size_t>(idx)].currentHp = hp;
 }
 void BattleEngine::cheat_set_monster_status(int idx, StatusId id, int stacks, int duration) {
     if (idx < 0 || static_cast<size_t>(idx) >= state_.monsters.size()) return;
     auto& list = state_.monsters[static_cast<size_t>(idx)].statuses;
     auto it = std::find_if(list.begin(), list.end(), [&id](const StatusInstance& s) { return s.id == id; });
     if (it != list.end()) {
         it->stacks = stacks;
         it->duration = duration;
     } else {
         list.push_back(StatusInstance{std::move(id), stacks, duration});
     }
 }
 void BattleEngine::cheat_remove_monster_status(int idx, StatusId id) {
     if (idx < 0 || static_cast<size_t>(idx) >= state_.monsters.size()) return;
     auto& list = state_.monsters[static_cast<size_t>(idx)].statuses;
     list.erase(std::remove_if(list.begin(), list.end(), [&id](const StatusInstance& s) { return s.id == id; }), list.end());
 }
 void BattleEngine::cheat_kill_monster(int idx) {
     if (idx < 0 || static_cast<size_t>(idx) >= state_.monsters.size()) return;
     state_.monsters[static_cast<size_t>(idx)].currentHp = 0;
     on_monster_just_died(idx);
 }
 void BattleEngine::cheat_add_relic(RelicId id) {
     if (std::find(state_.player.relics.begin(), state_.player.relics.end(), id) != state_.player.relics.end()) return;
     state_.player.relics.push_back(std::move(id));
     build_modifiers_from_state();
 }
 void BattleEngine::cheat_remove_relic(RelicId id) {
     auto it = std::find(state_.player.relics.begin(), state_.player.relics.end(), id);
     if (it != state_.player.relics.end()) {
         state_.player.relics.erase(it);
         build_modifiers_from_state();
     }
 }
 void BattleEngine::cheat_add_potion(PotionId id) {
     if (static_cast<int>(state_.player.potions.size()) >= state_.player.potionSlotCount)
         state_.player.potionSlotCount = static_cast<int>(state_.player.potions.size()) + 1;
     state_.player.potions.push_back(std::move(id));
 }
 void BattleEngine::cheat_remove_potion(int slot) {
     if (slot < 0 || static_cast<size_t>(slot) >= state_.player.potions.size()) return;
     state_.player.potions.erase(state_.player.potions.begin() + slot);
 }
 void BattleEngine::cheat_add_hand(CardId id) {
     if (card_system_) card_system_->generate_to_hand(std::move(id));
 }
 void BattleEngine::cheat_remove_hand(CardId id) {
     if (!card_system_) return;
     const auto& hand = card_system_->get_hand();
     for (size_t i = 0; i < hand.size(); ++i) {
         if (hand[i].id == id) {
             auto c = card_system_->remove_from_hand(static_cast<int>(i));
             card_system_->add_to_discard(std::move(c));
             return;
         }
     }
 }

 } // namespace tce