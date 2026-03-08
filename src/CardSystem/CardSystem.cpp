/**
 * C - 卡牌系统实现（桩）
 */
#include "../../include/CardSystem/CardSystem.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"
#include "../../include/DataLayer/DataLayer.hpp"
#include <algorithm>
#include <random>

namespace tce {

CardSystem::CardSystem(GetCardByIdFn get_card_by_id)
    : get_card_by_id_(std::move(get_card_by_id)) {}

void CardSystem::init_deck(const std::vector<CardId>& initial_card_ids) {
    next_instance_id_ = 0;
    draw_pile_.clear();
    hand_.clear();
    discard_pile_.clear();
    exhaust_pile_.clear();
    for (const auto& id : initial_card_ids) {
        CardInstance c;
        c.instanceId = ++next_instance_id_;
        c.id         = id;
        draw_pile_.push_back(c);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(draw_pile_.begin(), draw_pile_.end(), g);
}

void CardSystem::draw_cards(int n) {
    for (int i = 0; i < n && static_cast<int>(hand_.size()) < hand_limit_; ++i) {
        if (draw_pile_.empty()) {
            shuffle_discard_into_draw();
            if (draw_pile_.empty()) break;
        }
        hand_.push_back(draw_pile_.back());
        draw_pile_.pop_back();
    }
}

const std::vector<CardInstance>& CardSystem::get_hand() const {
    return hand_;
}

CardInstance CardSystem::remove_from_hand(int hand_index) {
    CardInstance c = hand_.at(static_cast<size_t>(hand_index));
    hand_.erase(hand_.begin() + hand_index);
    return c;
}

void CardSystem::add_to_hand(CardInstance card) {
    if (card.instanceId == 0)
        card.instanceId = ++next_instance_id_;
    if (static_cast<int>(hand_.size()) >= hand_limit_) {
        discard_pile_.push_back(card);
        return;
    }
    hand_.push_back(card);
}

void CardSystem::add_to_discard(CardInstance card) {
    if (card.instanceId == 0)
        card.instanceId = ++next_instance_id_;
    discard_pile_.push_back(card);
}

void CardSystem::add_to_exhaust(CardInstance card) {
    if (card.instanceId == 0)
        card.instanceId = ++next_instance_id_;
    exhaust_pile_.push_back(card);
}

void CardSystem::shuffle_discard_into_draw() {
    for (auto& c : discard_pile_)
        draw_pile_.push_back(c);
    discard_pile_.clear();
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(draw_pile_.begin(), draw_pile_.end(), g);
}

void CardSystem::add_to_deck(CardInstance card) {
    if (card.instanceId == 0)
        card.instanceId = ++next_instance_id_;
    draw_pile_.push_back(card);
}

bool CardSystem::remove_from_deck(InstanceId instance_id) {
    auto pred = [instance_id](const CardInstance& c) { return c.instanceId == instance_id; };
    auto it = std::find_if(hand_.begin(), hand_.end(), pred);
    if (it != hand_.end()) { hand_.erase(it); return true; }
    it = std::find_if(draw_pile_.begin(), draw_pile_.end(), pred);
    if (it != draw_pile_.end()) { draw_pile_.erase(it); return true; }
    it = std::find_if(discard_pile_.begin(), discard_pile_.end(), pred);
    if (it != discard_pile_.end()) { discard_pile_.erase(it); return true; }
    return false;
}

int CardSystem::get_deck_size() const {
    return static_cast<int>(draw_pile_.size());
}

int CardSystem::get_discard_size() const {
    return static_cast<int>(discard_pile_.size());
}

int CardSystem::get_exhaust_size() const {
    return static_cast<int>(exhaust_pile_.size());
}

bool CardSystem::upgrade_card_in_deck(InstanceId instance_id) {
    auto pred = [instance_id](const CardInstance& c) { return c.instanceId == instance_id; };
    for (auto* pile : {&hand_, &draw_pile_, &discard_pile_}) {
        auto it = std::find_if(pile->begin(), pile->end(), pred);
        if (it != pile->end()) {
            if (it->id.back() != '+')
                it->id += "+";
            return true;
        }
    }
    return false;
}

void CardSystem::execute_effect(CardId id, EffectContext& ctx) {
    (void)id;
    (void)ctx;
    // 效果由注册表查找并调用，此处桩为空
}

} // namespace tce
