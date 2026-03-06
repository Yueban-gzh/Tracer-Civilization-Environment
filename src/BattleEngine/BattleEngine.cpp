/**
 * B - 战斗引擎实现（桩）
 */
#include "../../include/BattleEngine/BattleEngine.hpp"
#include <algorithm>

namespace tce {

BattleEngine::BattleEngine(CardSystem& card_system, GetMonsterByIdFn get_monster, GetCardByIdFn get_card)
    : card_system_(&card_system)
    , get_monster_by_id_(std::move(get_monster))
    , get_card_by_id_(std::move(get_card)) {}

void BattleEngine::start_battle(const std::vector<MonsterId>& monster_ids,
                                const PlayerBattleState&      player_state,
                                const std::vector<CardId>&    deck_card_ids,
                                const std::vector<RelicId>&   relic_ids) {
    monsters_.clear();
    for (const auto& mid : monster_ids) {
        const MonsterData* md = get_monster_by_id_ ? get_monster_by_id_(mid) : nullptr;
        MonsterInBattle m;
        m.id         = mid;
        m.maxHp      = md ? md->maxHp : 10;
        m.currentHp  = m.maxHp;
        m.currentIntent = "";
        monsters_.push_back(m);
    }
    player_state_   = player_state;
    relic_ids_      = relic_ids;
    player_statuses_.clear();
    monster_statuses_.resize(monsters_.size());
    turn_number_    = 0;
    if (card_system_)
        card_system_->init_deck(deck_card_ids);
    // 首回合抽牌、固有等由实现补充
}

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
    s.potions          = player_state_.potions;
    s.relics           = relic_ids_;
    s.playerStatuses   = player_statuses_;
    s.monsters         = monsters_;
    s.monsterStatuses  = monster_statuses_;
    s.turnNumber       = turn_number_;
    if (card_system_) {
        s.hand            = card_system_->get_hand();
        s.drawPileSize    = card_system_->get_deck_size();
        s.discardPileSize = card_system_->get_discard_size();
        s.exhaustPileSize = card_system_->get_exhaust_size();
    }
    return s;
}

bool BattleEngine::play_card(int hand_index, int target_monster_index) {
    if (!card_system_) return false;
    const auto& hand = card_system_->get_hand();
    if (hand_index < 0 || static_cast<size_t>(hand_index) >= hand.size())
        return false;
    const CardData* cd = get_card_by_id_ ? get_card_by_id_(hand[static_cast<size_t>(hand_index)].id) : nullptr;
    if (cd && cd->unplayable) return false;
    int cost = cd ? cd->cost : 0;
    if (player_state_.energy < cost) return false;
    player_state_.energy -= cost;
    EffectContext ctx;
    ctx.target_monster_index = target_monster_index;
    card_system_->execute_effect(hand[static_cast<size_t>(hand_index)].id, ctx);
    CardInstance played = card_system_->remove_from_hand(hand_index);
    if (cd && cd->exhaust)
        card_system_->add_to_exhaust(played);
    else if (cd && cd->cardType == CardType::Power)
        { /* 能力牌不移入任何牌堆 */ }
    else
        card_system_->add_to_discard(played);
    return true;
}

void BattleEngine::end_turn() {
    if (!card_system_) return;
    // 手牌置入弃牌堆（保留/虚无按规则，此处简化：全部弃牌）
    const auto& hand = card_system_->get_hand();
    std::vector<CardInstance> to_discard(hand.begin(), hand.end());
    for (size_t i = to_discard.size(); i > 0; --i)
        card_system_->remove_from_hand(static_cast<int>(i - 1));
    for (auto& c : to_discard)
        card_system_->add_to_discard(c);
    // 敌方行动（桩：无）
    for (auto& list : monster_statuses_) {
        for (auto& s : list)
            if (s.duration > 0) --s.duration;
        list.erase(std::remove_if(list.begin(), list.end(), [](const StatusInstance& x) { return x.duration == 0; }), list.end());
    }
    for (auto& s : player_statuses_) {
        if (s.duration > 0) --s.duration;
    }
    player_statuses_.erase(
        std::remove_if(player_statuses_.begin(), player_statuses_.end(), [](const StatusInstance& x) { return x.duration == 0; }),
        player_statuses_.end());
    player_state_.block = 0;
    ++turn_number_;
    card_system_->draw_cards(5);
    player_state_.energy = player_state_.maxEnergy;
}

bool BattleEngine::is_battle_over() const {
    if (player_state_.currentHp <= 0) return true;
    return std::all_of(monsters_.begin(), monsters_.end(), [](const MonsterInBattle& m) { return m.currentHp <= 0; });
}

std::vector<CardId> BattleEngine::get_reward_cards(int count) {
    (void)count;
    return {};
}

void BattleEngine::apply_status_to_player(StatusId id, int stacks, int duration) {
    for (auto& s : player_statuses_) {
        if (s.id == id) {
            s.stacks += stacks;
            if (duration >= 0) s.duration = duration;
            return;
        }
    }
    player_statuses_.push_back(StatusInstance{std::move(id), stacks, duration});
}

void BattleEngine::apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration) {
    if (monster_index < 0 || static_cast<size_t>(monster_index) >= monster_statuses_.size())
        return;
    auto& list = monster_statuses_[static_cast<size_t>(monster_index)];
    for (auto& s : list) {
        if (s.id == id) {
            s.stacks += stacks;
            if (duration >= 0) s.duration = duration;
            return;
        }
    }
    list.push_back(StatusInstance{std::move(id), stacks, duration});
}

bool BattleEngine::use_potion(int slot_index) {
    if (slot_index < 0 || static_cast<size_t>(slot_index) >= player_state_.potions.size())
        return false;
    // 药水效果由 B 内注册表执行，此处仅移除
    player_state_.potions.erase(player_state_.potions.begin() + slot_index);
    return true;
}

} // namespace tce
