/**
 * B - 战斗引擎实现（桩）
 */
#include "../../include/BattleEngine/BattleEngine.hpp"
#include "../../include/DataLayer/DataLayer.hpp"
#include <algorithm>
#include <thread>
#include <chrono>

namespace tce {

// 构造函数：注入卡牌系统引用以及「按 id 查怪物/卡牌」的回调，供战斗内取数据
BattleEngine::BattleEngine(CardSystem& card_system, GetMonsterByIdFn get_monster, GetCardByIdFn get_card)
    : card_system_(&card_system)
    , get_monster_by_id_(std::move(get_monster))
    , get_card_by_id_(std::move(get_card)) {}

// start_battle：开始战斗。根据怪物 id 列表生成场上怪物、拷贝玩家状态与遗物列表、初始化牌组，回合数置 0
void BattleEngine::start_battle(const std::vector<MonsterId>& monster_ids,
                                const PlayerBattleState&      player_state,
                                const std::vector<CardId>&    deck_card_ids,
                                const std::vector<RelicId>&   relic_ids) {
    monsters_.clear();
    for (const auto& mid : monster_ids) {
        const MonsterData* md = get_monster_by_id_ ? get_monster_by_id_(mid) : nullptr;  // 若注入了查怪回调则取数据，否则用默认
        MonsterInBattle m;
        m.id         = mid;
        m.maxHp      = md ? md->maxHp : 10;   // 有怪物数据则用表内最大血量，否则默认 10
        m.currentHp  = m.maxHp;
        m.currentIntent = "";
        monsters_.push_back(m);
    }
    player_state_   = player_state;
    player_state_.statuses.clear();   // 新战斗清空玩家身上的效果
    relic_ids_      = relic_ids;
    turn_number_    = 0;
    if (card_system_) {  // 若已注入卡牌系统则初始化牌组，否则跳过（无 C 时仅做战斗状态初始化）
        card_system_->init_deck(deck_card_ids);
        // 首回合抽牌：若带有抽牌减少，则下 N 个回合各少抽 1 张
        int draw_count = player_state_.cardsToDrawPerTurn;
        int draw_red   = get_status_stacks(player_state_.statuses, "draw_reduction");
        if (draw_red > 0) {
            --draw_count;
            reduce_status_stacks(player_state_.statuses, "draw_reduction", 1);
        }
        if (draw_count < 0) draw_count = 0;
        card_system_->draw_cards(draw_count);   // 首回合抽牌也受抽牌减少影响
    }
}

// get_battle_state：获取当前战斗状态快照，供 UI 展示（玩家/怪物/手牌/牌堆张数/增益减益/药水遗物等）
BattleStateSnapshot BattleEngine::get_battle_state() const {
    BattleStateSnapshot s;
    s.playerName       = player_state_.playerName;
    s.character        = player_state_.character;
    s.currentHp        = player_state_.currentHp;
    s.maxHp            = player_state_.maxHp;
    s.block            = player_state_.block;
    s.gold             = player_state_.gold;
    s.energy           = player_state_.energy;
    s.maxEnergy        = player_state_.maxEnergy;
    s.stance           = player_state_.stance;
    s.orbSlotCount     = player_state_.orbSlotCount;
    s.orbSlots         = player_state_.orbSlots;
    s.potionSlotCount  = player_state_.potionSlotCount;
    s.potions          = player_state_.potions;
    s.relics           = relic_ids_;
    s.playerStatuses   = player_state_.statuses;
    s.monsters         = monsters_;
    s.monsterStatuses.clear();
    for (const auto& m : monsters_)
        s.monsterStatuses.push_back(m.statuses);
    s.turnNumber       = turn_number_;
    switch (turn_phase_) {
    case TurnPhase::PlayerTurnEnd:   s.phaseDebugLabel = L"玩家回合结束"; break;
    case TurnPhase::EnemyTurnStart:  s.phaseDebugLabel = L"敌人回合开始"; break;
    case TurnPhase::EnemyTurnActions:s.phaseDebugLabel = L"敌人行动";     break;
    case TurnPhase::EnemyTurnEnd:    s.phaseDebugLabel = L"敌人回合结束"; break;
    case TurnPhase::PlayerTurnStart: s.phaseDebugLabel = L"玩家回合开始"; break;
    case TurnPhase::Idle:
    default:                         s.phaseDebugLabel.clear();          break;
    }
    if (card_system_) {  // 若已注入卡牌系统则从 C 取手牌与各牌堆张数填入快照，否则快照中牌相关字段保持默认
        s.hand            = card_system_->get_hand();
        s.drawPileSize    = card_system_->get_deck_size();
        s.discardPileSize = card_system_->get_discard_size();
        s.exhaustPileSize = card_system_->get_exhaust_size();
    }
    return s;
}

// play_card：打出手牌中指定位置的牌。校验可打出、扣能量、执行效果，再根据词条将牌移入弃牌堆/消耗堆或移除（能力牌）
bool BattleEngine::play_card(int hand_index, int target_monster_index) {
    if (!card_system_) return false;   // 未注入卡牌系统则无法出牌
    const auto& hand = card_system_->get_hand();
    if (hand_index < 0 || static_cast<size_t>(hand_index) >= hand.size())
        return false;   // 手牌下标越界则拒绝
    const CardData* cd = get_card_by_id_ ? get_card_by_id_(hand[static_cast<size_t>(hand_index)].id) : nullptr;
    if (cd && cd->unplayable) return false;   // 该牌带「不能打出」词条则拒绝
    int cost = cd ? cd->cost : 0;
    if (player_state_.energy < cost) return false;   // 当前能量不足则拒绝
    player_state_.energy -= cost;
    EffectContext ctx;
    ctx.target_monster_index = target_monster_index;
    fill_effect_context(ctx);
    card_system_->execute_effect(hand[static_cast<size_t>(hand_index)].id, ctx);
    CardInstance played = card_system_->remove_from_hand(hand_index);
    if (cd && cd->exhaust)
        card_system_->add_to_exhaust(played);   // 消耗词条：打出后移入消耗堆
    else if (cd && cd->cardType == CardType::Power)
        { /* 能力牌不移入任何牌堆，本场战斗内从游戏中移除 */ }
    else
        card_system_->add_to_discard(played);   // 普通攻击/技能牌入弃牌堆
    return true;
}

// end_turn：结束回合。内部按「玩家回合结束 → 敌人回合开始 → 敌人行动 → 敌人回合结束 → 玩家回合开始」五个阶段运行
void BattleEngine::end_turn() {
    if (!card_system_) return;
    if (turn_phase_ != TurnPhase::Idle) return; // 动画进行中，忽略重复点击
    turn_phase_ = TurnPhase::PlayerTurnEnd;
}

void BattleEngine::phase_player_turn_end(EffectContext& ctx) {
    // ① 手牌：保留留在手牌，虚无进消耗堆，其余进弃牌堆（从后往前删避免下标错位）
    const auto& hand = card_system_->get_hand();
    for (int i = static_cast<int>(hand.size()) - 1; i >= 0; --i) {
        const CardData* cd = get_card_by_id_ ? get_card_by_id_(hand[static_cast<size_t>(i)].id) : nullptr;
        if (cd && cd->retain) continue;
        CardInstance c = card_system_->remove_from_hand(i);
        if (cd && cd->ethereal)
            card_system_->add_to_exhaust(c);
        else
            card_system_->add_to_discard(c);
    }

    // ①.5 缠绕（仅玩家）：回合结束时受到层数点伤害（先扣格挡再扣血）
    int entangle = get_status_stacks(player_state_.statuses, "entangle");
    if (entangle > 0)
        deal_damage_to_player(entangle);

    // ①.6 玩家回合末状态 tick（金属化等），中毒已在敌人/己方回合开始单独处理
    for (const auto& s : player_state_.statuses) {
        if (s.id == "poison") continue;
        auto it = status_tick_registry_.find(s.id);
        if (it != status_tick_registry_.end())
            it->second(s.stacks, true, -1, ctx);
    }
}

void BattleEngine::phase_enemy_turn_start(EffectContext& tick_ctx) {
    // ②.0 敌人回合开始：先清空所有怪物的格挡，再结算中毒
    for (auto& m : monsters_)
        m.block = 0;
    for (size_t i = 0; i < monsters_.size(); ++i) {
        if (monsters_[i].currentHp <= 0) continue;
        int poison_stacks = get_status_stacks(monsters_[i].statuses, "poison");
        if (poison_stacks > 0) {
            tick_ctx.deal_damage_to_monster_ignoring_block(static_cast<int>(i), poison_stacks);
            reduce_status_stacks(monsters_[i].statuses, "poison", 1);
        }
    }
}

void BattleEngine::phase_enemy_turn_actions(EffectContext&) {
    // ② 敌方行动（按队列顺序；当前为桩，后续接入怪物行为函数）
    for (size_t i = 0; i < monsters_.size(); ++i) {
        if (monsters_[i].currentHp <= 0) continue;
        (void)i;
    }
}

void BattleEngine::phase_enemy_turn_end(EffectContext& tick_ctx) {
    // ②.4 镣铐（仅对敌人）：敌人回合结束时，每个怪物回复对应层数力量
    for (size_t i = 0; i < monsters_.size(); ++i) {
        int shackles = get_status_stacks(monsters_[i].statuses, "shackles");
        if (shackles > 0)
            apply_status_to_monster(static_cast<int>(i), "strength", shackles, -1);
    }

    // ②.5 敌人回合末状态 tick：只处理怪物身上的状态（玩家状态已在 ①.6 处理）
    for (size_t i = 0; i < monsters_.size(); ++i) {
        for (const auto& s : monsters_[i].statuses) {
            if (s.id == "poison") continue;  // 已在 ②.0 敌人回合开始时结算
            auto it = status_tick_registry_.find(s.id);
            if (it != status_tick_registry_.end())
                it->second(s.stacks, false, static_cast<int>(i), tick_ctx);
        }
    }

    // ②.6 活动肌肉：回合结束时扣对应层数力量；敏捷下降：回合结束时扣对应层数敏捷（仅玩家）
    int flex_stacks = get_status_stacks(player_state_.statuses, "flex");
    if (flex_stacks > 0)
        reduce_status_stacks(player_state_.statuses, "strength", flex_stacks);
    int dex_down_stacks = get_status_stacks(player_state_.statuses, "dexterity_down");
    if (dex_down_stacks > 0)
        reduce_status_stacks(player_state_.statuses, "dexterity", dex_down_stacks);

    // ③ 按实例做 duration 减一，移除到期的
    for (auto& s : player_state_.statuses)
        if (s.duration > 0) --s.duration;
    player_state_.statuses.erase(
        std::remove_if(player_state_.statuses.begin(), player_state_.statuses.end(), [](const StatusInstance& x) { return x.duration == 0; }),
        player_state_.statuses.end());
    for (auto& m : monsters_) {
        for (auto& s : m.statuses)
            if (s.duration > 0) --s.duration;
        m.statuses.erase(std::remove_if(m.statuses.begin(), m.statuses.end(), [](const StatusInstance& x) { return x.duration == 0; }), m.statuses.end());
    }

    // ④ 清空玩家格挡（怪物回合结束）
    player_state_.block = 0;
}

void BattleEngine::phase_player_turn_start(EffectContext&) {
    // ⑤ 下一回合开始：中毒先扣玩家生命并降低中毒层数；抽牌数受抽牌减少影响（仅玩家），能量回满
    ++turn_number_;
    // 玩家中毒：在玩家回合开始时，损失 N 点生命，然后中毒层数减 1
    int player_poison = get_status_stacks(player_state_.statuses, "poison");
    if (player_poison > 0) {
        deal_damage_to_player_ignoring_block(player_poison);
        reduce_status_stacks(player_state_.statuses, "poison", 1);
    }
    // 抽牌减少：下 N 个回合内，各少抽 1 张牌
    int draw_count = player_state_.cardsToDrawPerTurn;
    int draw_red   = get_status_stacks(player_state_.statuses, "draw_reduction");
    if (draw_red > 0) {
        --draw_count;
        reduce_status_stacks(player_state_.statuses, "draw_reduction", 1);
    }
    if (draw_count < 0) draw_count = 0;
    card_system_->draw_cards(draw_count);
    player_state_.energy = player_state_.maxEnergy;

    // 整个回合流程完成，回到空闲态
    turn_phase_ = TurnPhase::Idle;
}

// 每帧调用一次，按阶段推进一小步，用于 UI 动画
void BattleEngine::step_turn_phase() {
    if (!card_system_) return;
    if (turn_phase_ == TurnPhase::Idle) return;

    EffectContext tick_ctx;
    fill_effect_context(tick_ctx);

    switch (turn_phase_) {
    case TurnPhase::PlayerTurnEnd:
        phase_player_turn_end(tick_ctx);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        turn_phase_ = TurnPhase::EnemyTurnStart;
        break;
    case TurnPhase::EnemyTurnStart:
        phase_enemy_turn_start(tick_ctx);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        turn_phase_ = TurnPhase::EnemyTurnActions;
        break;
    case TurnPhase::EnemyTurnActions:
        phase_enemy_turn_actions(tick_ctx);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        turn_phase_ = TurnPhase::EnemyTurnEnd;
        break;
    case TurnPhase::EnemyTurnEnd:
        phase_enemy_turn_end(tick_ctx);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        turn_phase_ = TurnPhase::PlayerTurnStart;
        break;
    case TurnPhase::PlayerTurnStart:
        phase_player_turn_start(tick_ctx);
        // phase_player_turn_start 内部会把 turn_phase_ 设回 Idle
        break;
    case TurnPhase::Idle:
    default:
        break;
    }
}

// is_battle_over：战斗是否结束。玩家血量≤0 为失败，所有怪物血量≤0 为胜利
bool BattleEngine::is_battle_over() const {
    if (player_state_.currentHp <= 0) return true;   // 玩家已死亡则战斗结束（失败）
    return std::all_of(monsters_.begin(), monsters_.end(), [](const MonsterInBattle& m) { return m.currentHp <= 0; });   // 或所有怪物血量≤0 则胜利
}

// get_reward_cards：战斗胜利后获取可选奖励卡牌的 id 列表（桩实现返回空）
std::vector<CardId> BattleEngine::get_reward_cards(int count) {
    (void)count;
    return {};
}

// apply_status_to_player：对玩家施加增益/减益，写入玩家实例的 statuses
void BattleEngine::apply_status_to_player(StatusId id, int stacks, int duration) {
    for (auto& s : player_state_.statuses) {
        if (s.id == id) {
            s.stacks += stacks;
            if (duration >= 0) s.duration = duration;
            return;
        }
    }
    player_state_.statuses.push_back(StatusInstance{std::move(id), stacks, duration});
}

// add_block_to_player：增加玩家格挡值（卡牌/效果调用）
void BattleEngine::add_block_to_player(int amount) {
    if (amount <= 0) return;
    player_state_.block += amount;
}

// deal_damage_to_player：对玩家造成伤害，先由格挡吸收，再扣血
void BattleEngine::deal_damage_to_player(int amount) {
    if (amount <= 0) return;
    int absorbed = (amount < player_state_.block) ? amount : player_state_.block;
    player_state_.block -= absorbed;
    int hp_damage = amount - absorbed;
    player_state_.currentHp -= hp_damage;
    if (player_state_.currentHp < 0) player_state_.currentHp = 0;
}

// add_block_to_monster：为指定怪物增加格挡（效果/怪物行为调用）
void BattleEngine::add_block_to_monster(int monster_index, int amount) {
    if (amount <= 0) return;
    if (monster_index < 0 || static_cast<size_t>(monster_index) >= monsters_.size()) return;
    monsters_[static_cast<size_t>(monster_index)].block += amount;
}

// deal_damage_to_monster：对指定怪物造成伤害，先由格挡吸收再扣血
void BattleEngine::deal_damage_to_monster(int monster_index, int amount) {
    if (amount <= 0) return;
    if (monster_index < 0 || static_cast<size_t>(monster_index) >= monsters_.size()) return;
    MonsterInBattle& m = monsters_[static_cast<size_t>(monster_index)];
    int absorbed = (amount < m.block) ? amount : m.block;
    m.block -= absorbed;
    int hp_damage = amount - absorbed;
    m.currentHp -= hp_damage;
    if (m.currentHp < 0) m.currentHp = 0;
}

// deal_damage_to_player_ignoring_block：无视格挡对玩家造成伤害（如中毒）
void BattleEngine::deal_damage_to_player_ignoring_block(int amount) {
    if (amount <= 0) return;
    player_state_.currentHp -= amount;
    if (player_state_.currentHp < 0) player_state_.currentHp = 0;
}

// deal_damage_to_monster_ignoring_block：无视格挡对怪物造成伤害（如中毒）
void BattleEngine::deal_damage_to_monster_ignoring_block(int monster_index, int amount) {
    if (amount <= 0) return;
    if (monster_index < 0 || static_cast<size_t>(monster_index) >= monsters_.size()) return;
    MonsterInBattle& m = monsters_[static_cast<size_t>(monster_index)];
    m.currentHp -= amount;
    if (m.currentHp < 0) m.currentHp = 0;
}

void BattleEngine::fill_effect_context(EffectContext& ctx) {
    ctx.add_block_to_player_ = [this](int n) { add_block_to_player(n); };
    ctx.add_block_to_monster_ = [this](int i, int n) { add_block_to_monster(i, n); };
    ctx.deal_damage_to_player_ = [this](int n) { deal_damage_to_player(n); };
    ctx.deal_damage_to_monster_ = [this](int i, int n) { deal_damage_to_monster(i, n); };
    ctx.deal_damage_to_player_ignoring_block_ = [this](int n) { deal_damage_to_player_ignoring_block(n); };
    ctx.deal_damage_to_monster_ignoring_block_ = [this](int i, int n) { deal_damage_to_monster_ignoring_block(i, n); };
    ctx.get_effective_damage_dealt_by_player_ = [this](int base, int target) { return get_effective_damage_dealt_by_player(base, target); };
    ctx.get_effective_damage_dealt_to_player_ = [this](int base, int attacker) { return get_effective_damage_dealt_to_player(base, attacker); };
    ctx.get_effective_block_for_player_ = [this](int base) { return get_effective_block_for_player(base); };
    ctx.apply_status_to_player_ = [this](StatusId id, int stacks, int duration) { apply_status_to_player(std::move(id), stacks, duration); };
    ctx.apply_status_to_monster_ = [this](int i, StatusId id, int stacks, int duration) { apply_status_to_monster(i, std::move(id), stacks, duration); };
}

int BattleEngine::get_status_stacks(const std::vector<StatusInstance>& list, const StatusId& id) {
    for (const auto& s : list)
        if (s.id == id) return s.stacks;
    return 0;
}

void BattleEngine::reduce_status_stacks(std::vector<StatusInstance>& list, const StatusId& id, int amount) {
    if (amount <= 0) return;
    for (auto it = list.begin(); it != list.end(); ++it) {
        if (it->id == id) {
            it->stacks -= amount;
            if (it->stacks <= 0)
                list.erase(it);
            return;
        }
    }
}

// 数值规则：力量 +1/层，易伤 1.5x，虚弱 0.75x；活动肌肉在回合末扣力量、不参与本回合公式
int BattleEngine::get_effective_damage_dealt_by_player(int base_damage, int target_monster_index) const {
    if (base_damage <= 0) return 0;
    if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= monsters_.size())
        return base_damage;
    int str = get_status_stacks(player_state_.statuses, "strength");
    int dmg = base_damage + str;
    if (dmg <= 0) return 0;
    int vuln = get_status_stacks(monsters_[static_cast<size_t>(target_monster_index)].statuses, "vulnerable");
    if (vuln > 0) dmg = dmg * 3 / 2;
    int weak = get_status_stacks(player_state_.statuses, "weak");
    if (weak > 0) dmg = dmg * 3 / 4;
    return dmg > 0 ? dmg : 0;
}

// 怪物对玩家造成伤害：怪物力量 +1/层，玩家易伤 1.5x
int BattleEngine::get_effective_damage_dealt_to_player(int base_damage, int attacker_monster_index) const {
    if (base_damage <= 0) return 0;
    int dmg = base_damage;
    if (attacker_monster_index >= 0 && static_cast<size_t>(attacker_monster_index) < monsters_.size()) {
        int str = get_status_stacks(monsters_[static_cast<size_t>(attacker_monster_index)].statuses, "strength");
        dmg += str;
    }
    if (dmg <= 0) return 0;
    int vuln = get_status_stacks(player_state_.statuses, "vulnerable");
    if (vuln > 0) dmg = dmg * 3 / 2;
    return dmg > 0 ? dmg : 0;
}

// 格挡数值规则：敏捷 +1/层，脆弱 0.75x；敏捷下降在回合末扣敏捷、不参与本回合公式
int BattleEngine::get_effective_block_for_player(int base_block) const {
    if (base_block <= 0) return 0;
    int dex = get_status_stacks(player_state_.statuses, "dexterity");
    int block = base_block + dex;
    if (block <= 0) return 0;
    int frail = get_status_stacks(player_state_.statuses, "frail");
    if (frail > 0) block = block * 3 / 4;
    return block > 0 ? block : 0;
}

void BattleEngine::register_status_tick(StatusId id, StatusTickFn fn) {
    if (fn) status_tick_registry_[std::move(id)] = std::move(fn);
}

// apply_status_to_monster：对指定怪物施加增益/减益，写入该怪物实例的 statuses
void BattleEngine::apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration) {
    if (monster_index < 0 || static_cast<size_t>(monster_index) >= monsters_.size())
        return;
    auto& list = monsters_[static_cast<size_t>(monster_index)].statuses;
    for (auto& s : list) {
        if (s.id == id) {   // 找到已有同 id 状态则叠加层数
            s.stacks += stacks;
            if (duration >= 0) s.duration = duration;   // 若传入的 duration 非负则刷新剩余回合数
            return;
        }
    }
    list.push_back(StatusInstance{std::move(id), stacks, duration});
}

// use_potion：使用药水栏位中指定位置的一瓶药水。执行效果后从列表移除该瓶；此处桩实现仅做移除，效果由 B 内药水注册表扩展
bool BattleEngine::use_potion(int slot_index) {
    if (slot_index < 0 || static_cast<size_t>(slot_index) >= player_state_.potions.size())
        return false;   // 药水槽位下标越界或该格无药水则拒绝使用
    // 药水效果由 B 内注册表执行，此处仅移除
    player_state_.potions.erase(player_state_.potions.begin() + slot_index);
    return true;
}

} // namespace tce
