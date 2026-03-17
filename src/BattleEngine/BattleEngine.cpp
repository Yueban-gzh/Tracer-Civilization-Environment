/**
 * B - 战斗引擎实现（BattleCoreRefactor）
 */
#include "../../include/BattleCoreRefactor/BattleEngine.hpp"           // 战斗引擎头文件
#include "../../include/BattleCoreRefactor/PotionEffects.hpp"          // 药水效果
#include "../../include/BattleCoreRefactor/RelicModifiers.hpp"         // 遗物 modifier
#include "../../include/BattleCoreRefactor/StatusModifiers.hpp"        // 状态 modifier
#include "../../include/CardSystem/CardSystem.hpp"                      // 卡牌系统
#include "../../include/DataLayer/DataLayer.hpp"                        // 数据层
#include "../../include/BattleEngine/MonsterBehaviors.hpp"             // 怪物行为
#include <algorithm>                                                  // std::remove_if
#include <random>                                                      // 随机奖励卡
 
 namespace tce {
 
 BattleEngine::BattleEngine(CardSystem& card_system,                    // 构造函数
                            GetMonsterByIdFn get_monster,
                            GetCardByIdFn get_card,
                            ExecuteMonsterActionFn execute_monster)
     : card_system_(&card_system)                                       // 卡牌系统指针
     , get_monster_by_id_(std::move(get_monster))                       // 获取怪物数据回调
     , get_card_by_id_(std::move(get_card))                             // 获取卡牌数据回调
     , execute_monster_action_(std::move(execute_monster)) {             // 怪物行动执行回调
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
 
bool BattleEngine::play_card(int hand_index, int target_monster_index) {  // 打出卡牌
    if (!card_system_) return false;                                   // 无卡牌系统则失败
    const auto& hand = card_system_->get_hand();                        // 获取手牌
    if (hand_index < 0 || static_cast<size_t>(hand_index) >= hand.size()) return false;  // 下标越界
    const CardData* cd = get_card_by_id_ ? get_card_by_id_(hand[static_cast<size_t>(hand_index)].id) : nullptr;  // 卡牌数据
    if (cd && cd->unplayable) return false;                            // 不可打出则失败

    int cost = cd ? cd->cost : 0;                                      // 费用
    if (state_.player.energy < cost) return false;                     // 能量不足

    // 攻击牌或需目标的牌：必须提供有效目标
    const bool needs_target = (cd && (cd->cardType == CardType::Attack || cd->requiresTarget));
    if (needs_target) {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state_.monsters.size())
            return false;
        if (state_.monsters[static_cast<size_t>(target_monster_index)].currentHp <= 0)
            return false;  // 目标已死
    }

    // 打出前：缠身等 modifier 可禁止打出（如禁止攻击牌）
    CardId played_id = hand[static_cast<size_t>(hand_index)].id;
    CanPlayCardContext can_ctx{state_, played_id, cd && cd->cardType == CardType::Attack, target_monster_index, false};
    modifiers_.on_can_play_card(can_ctx);
    if (can_ctx.blocked) return false;  // 缠身等禁止打出
    if (!card_system_->has_effect_registered(played_id)) return false;  // 未注册效果则不打出（避免消耗能量无效果）
    CardInstance played = card_system_->remove_from_hand(hand_index);    // 从手牌移除
 
     state_.player.energy -= cost;                                      // 扣除能量
 
     EffectContext ctx;                                                 // 效果上下文
     fill_effect_context(ctx);                                          // 填充引擎指针
     ctx.target_monster_index = target_monster_index;                   // 目标怪物下标
     ctx.from_attack = (cd && cd->cardType == CardType::Attack);          // 是否为攻击牌
 
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
         (cd && cd->cardType == CardType::Attack)                        // 是否为攻击牌（双截棍等用）
     };
     modifiers_.on_card_played(state_, played_id, target_monster_index, &card_ctx);  // 广播：打出卡牌（勒脖等）
 
     card_system_->execute_effect(played_id, ctx);                       // 执行卡牌效果
 
     if (cd && cd->retain) {                                            // 若为保留牌
         card_system_->add_to_hand(played);                              // 放回手牌
     } else if (cd && cd->exhaust) {                                    // 若为消耗牌
         card_system_->add_to_exhaust(played);                           // 加入消耗堆
     } else {                                                           // 否则
         card_system_->add_to_discard(played);                           // 加入弃牌堆
     }
     return true;                                                       // 打出成功
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
         "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle", "data_disk", "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull", "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum", "pen_nib", "toy_ornithopter", "preparation_pack", "anchor", "art_of_war", "relic_strength_plus"
     };
     const auto& owned = state_.player.relics;
     std::vector<RelicId> available;
     for (const auto& id : relic_pool) {
         if (std::find(owned.begin(), owned.end(), id) == owned.end())
             available.push_back(id);
     }
     if (available.empty()) return {};
     std::random_device rd;
     std::mt19937 gen(rd());
     std::uniform_int_distribution<size_t> dist(0, available.size() - 1);
     RelicId id = available[dist(gen)];
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
     std::random_device rd;
     std::mt19937 gen(rd());
     std::uniform_int_distribution<size_t> dist(0, available.size() - 1);
     PotionId id = available[dist(gen)];
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
         "card_024", "card_024+"
     };
     std::vector<CardId> result;
     if (count <= 0 || reward_pool.empty()) return result;
     std::random_device rd;
     std::mt19937 gen(rd());
     std::uniform_int_distribution<size_t> dist(0, reward_pool.size() - 1);
     for (int i = 0; i < count; ++i) {
         result.push_back(reward_pool[dist(gen)]);
     }
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

    // modifiers_.on_monster_deal_damage(dmg, state_);                    // 广播：怪物造成伤害前（虚弱、易伤等）

     int totalDamage = dmg.modified_amount;                              // 总伤害（含被格挡部分）
     if (totalDamage < 0) totalDamage = 0;                               // 确保非负

     int hpDamage = totalDamage;                                         // 实际扣血量
     if (!dmg.ignore_block) {                                           // 若不无视格挡
         int absorbed = (hpDamage < state_.player.block) ? hpDamage : state_.player.block;  // 格挡吸收量
         state_.player.block -= absorbed;                               // 减少格挡
         hpDamage -= absorbed;                                          // 剩余伤害
     }

    state_.player.currentHp -= hpDamage;                               // 扣血
    if (state_.player.currentHp < 0) state_.player.currentHp = 0;       // 不低于 0
    if (totalDamage > 0) state_.pendingDamageDisplays.push_back(DamageDisplayEvent{true, -1, totalDamage, 180});

     DamageAppliedContext dmg_ctx;                                      // 伤害结算后上下文（荆棘反伤、百年积木抽牌用）
     dmg_ctx.damage_to_player = true;                                   // 标记：伤害施加给玩家
     dmg_ctx.hp_damage_to_player = hpDamage;                            // 玩家实际扣血量（格挡后）
     dmg_ctx.draw_cards = [this](int n) { if (card_system_) card_system_->draw_cards(n); };  // 抽牌回调
     dmg_ctx.deal_damage_to_monster_ignoring_block = [this](int idx, int amount) {  // 反伤回调
         if (amount <= 0 || idx < 0 || idx >= static_cast<int>(state_.monsters.size())) return;  // 参数校验
         DamagePacket thorns;                                           // 荆棘伤害包
         thorns.modified_amount = amount;                               // 伤害值
         thorns.ignore_block    = true;                                 // 无视格挡
         thorns.source_type     = DamagePacket::SourceType::Status;     // 来源：状态
         thorns.target_monster_index = idx;                             // 目标怪物
         apply_damage_to_monster(thorns);                                // 施加反伤
     };
     modifiers_.on_damage_applied(dmg, state_, &dmg_ctx);                 // 广播：伤害结算后（荆棘反伤等）
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
     if (totalDamage < 0) totalDamage = 0;                             // 确保非负

     int hpDamage = totalDamage;                                        // 实际扣血量
     if (!dmg.ignore_block) {                                           // 若不无视格挡
         int absorbed = (hpDamage < m.block) ? hpDamage : m.block;      // 格挡吸收量
         m.block -= absorbed;                                           // 减少格挡
         hpDamage -= absorbed;                                          // 剩余伤害
     }

    m.currentHp -= hpDamage;                                          // 扣血
    if (m.currentHp < 0) m.currentHp = 0;                              // 不低于 0
    if (totalDamage > 0) state_.pendingDamageDisplays.push_back(DamageDisplayEvent{false, idx, totalDamage, 180});

     modifiers_.on_damage_applied(dmg, state_, nullptr);                 // 广播：伤害施加给怪物，荆棘不触发
     if (m.currentHp <= 0) {                                            // 若怪物死亡
         on_monster_just_died(idx);                                      // 触发怪物死亡（尸体爆炸等）
     }
 }
 
 void BattleEngine::handle_player_turn_start() {                        // 玩家回合开始
     int draw_count = state_.player.cardsToDrawPerTurn;                 // 本回合抽牌数
     int energy      = state_.player.maxEnergy;                         // 本回合能量
 
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
         }
     };
 
     modifiers_.on_turn_start_player(state_, &ctx);                      // 广播：玩家回合开始（中毒扣血、抽牌修改等）
     state_.player.energy = energy;                                    // 应用能量（modifier 可能已修改）
     if (draw_count < 0) draw_count = 0;                                // 抽牌数不低于 0
     if (card_system_) card_system_->draw_cards(draw_count);            // 抽牌
 }
 
 void BattleEngine::handle_player_turn_end() {                          // 玩家回合结束
     if (card_system_) {                                                // 若有卡牌系统
         const auto& hand = card_system_->get_hand();                    // 获取手牌
         for (int i = static_cast<int>(hand.size()) - 1; i >= 0; --i) {  // 从后往前遍历（避免下标错位）
             const CardData* cd = get_card_by_id_ ? get_card_by_id_(hand[static_cast<size_t>(i)].id) : nullptr;  // 卡牌数据
             if (cd && cd->retain) continue;                            // 保留牌不移除
             auto c = card_system_->remove_from_hand(i);                 // 从手牌移除
             if (cd && cd->ethereal)                                     // 若为虚无牌
                 card_system_->add_to_exhaust(c);                        // 加入消耗堆
             else
                 card_system_->add_to_discard(c);                        // 否则加入弃牌堆
         }
     }
     modifiers_.on_turn_end_player(state_);                              // 广播：玩家回合结束（金属化加格挡等）
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
     EnemyTurnContext ctx{};                                            // 空上下文
     modifiers_.on_turn_end_monsters(state_, &ctx);                     // 广播：镣铐减力量、duration 递减等
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
     int idx = 0;                                                      // 遗物索引
     for (auto& m : create_relic_modifiers(state_.player.relics))      // 遗物 modifier
         modifiers_.add_modifier(std::move(m), MOD_PRIORITY_RELIC + (idx++));  // 按获得顺序
     for (auto& m : create_player_status_modifiers(state_.player))      // 玩家状态 modifier
         modifiers_.add_modifier(std::move(m), MOD_PRIORITY_PLAYER_ST);
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
     return engine_ ? engine_->get_effective_damage_dealt_by_player_impl(base_damage, target_monster_index) : base_damage;
 }
 int EffectContext::get_effective_block_for_player(int base_block) const {  // 玩家有效格挡
     return engine_ ? engine_->get_effective_block_for_player_impl(base_block) : base_block;
 }
 void EffectContext::generate_to_discard_pile(CardId id) {              // 生成卡牌到弃牌堆
     if (engine_) engine_->generate_to_discard_pile_impl(std::move(id));
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
 void EffectContext::apply_status_to_all_monsters(StatusId id, int stacks, int duration) {  // 对全体怪物施加状态
     if (engine_) engine_->apply_status_to_all_monsters_impl(std::move(id), stacks, duration);
 }
 void EffectContext::deal_damage_to_all_monsters(int base_damage) {     // 对全体怪物造成伤害
     if (engine_) engine_->deal_damage_to_all_monsters_impl(base_damage);
 }
 int EffectContext::get_player_block() const {                          // 获取玩家格挡
     return engine_ ? engine_->get_player_block_impl() : 0;
 }
 
 // --- BattleEngine 内部实现 ---
 void BattleEngine::add_block_to_player_impl(int amount) {               // 给玩家加格挡
     if (amount > 0) state_.player.block += amount;
 }
 void BattleEngine::add_block_to_monster_impl(int monster_index, int amount) {  // 给怪物加格挡
     if (amount <= 0 || monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return;
     state_.monsters[static_cast<size_t>(monster_index)].block += amount;
 }
 int BattleEngine::get_effective_damage_dealt_by_player_impl(int base_damage, int target_monster_index) const {  // 玩家对怪物的有效伤害（经 modifier）
     DamagePacket dmg;                                                  // 构造伤害包
     dmg.raw_amount = base_damage;
     dmg.modified_amount = base_damage;
     dmg.source_type = DamagePacket::SourceType::Player;
     dmg.target_monster_index = target_monster_index;
     dmg.from_attack = true;
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
 void BattleEngine::draw_cards_impl(int n) {                            // 抽牌
     if (card_system_) card_system_->draw_cards(n);
 }
 void BattleEngine::add_energy_to_player_impl(int amount) {              // 给玩家加能量
     if (amount > 0) {
         state_.player.energy += amount;
         if (state_.player.energy > state_.player.maxEnergy) state_.player.energy = state_.player.maxEnergy;  // 不超过上限
     }
 }
 int BattleEngine::get_status_stacks_on_monster_impl(int monster_index, const StatusId& id) const {  // 怪物某状态层数
     if (monster_index < 0 || monster_index >= static_cast<int>(state_.monsters.size())) return 0;
     return get_status_stacks(state_.monsters[static_cast<size_t>(monster_index)].statuses, id);
 }
 int BattleEngine::get_status_stacks_on_player_impl(const StatusId& id) const {  // 玩家某状态层数
     return get_status_stacks(state_.player.statuses, id);
 }
static void add_or_merge_status(std::vector<StatusInstance>& list, StatusId id, int stacks, int duration) {
    for (auto& s : list) {
        if (s.id == id) {
            s.stacks += stacks;                                        // 已有则叠加层数
            if (duration >= 0) s.duration = duration;                  // 有持续回合则刷新
            return;
        }
    }
    list.push_back(StatusInstance{std::move(id), stacks, duration});   // 无则新建
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
             int dmg = get_effective_damage_dealt_by_player_impl(base_damage, static_cast<int>(i));  // 经 modifier 修正
             if (dmg > 0) ctx.deal_damage_to_monster(static_cast<int>(i), dmg);
         }
     }
 }
 int BattleEngine::get_player_block_impl() const {                      // 获取玩家格挡
     return state_.player.block;
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
     if (state_.player.energy > state_.player.maxEnergy) state_.player.energy = state_.player.maxEnergy;
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