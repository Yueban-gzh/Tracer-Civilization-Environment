/**
 * C - 卡牌系统实现（桩）
 */
#include "../../include/CardSystem/CardSystem.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"
#include "../../include/DataLayer/DataLayer.hpp"
#include <algorithm>
#include <random>

namespace tce {

// 构造：注入 get_card_by_id（数据层只读查询）
CardSystem::CardSystem(GetCardByIdFn get_card_by_id)
    : get_card_by_id_(std::move(get_card_by_id)) {}

// 初始化永久牌组（master deck）：通常由存档/主流程调用；不参与战斗洗抽弃逻辑
void CardSystem::init_master_deck(const std::vector<CardId>& card_ids) {
    master_deck_.clear();
    InstanceId next_id = 0;
    for (const auto& id : card_ids) {
        CardInstance c;
        c.instanceId = ++next_id;
        c.id = id;
        c.temporary = false;
        master_deck_.push_back(std::move(c));
    }
}

// 获取永久牌组（只读引用）
const std::vector<CardInstance>& CardSystem::get_master_deck() const {
    return master_deck_;
}

// 将永久牌组转换为 CardId 列表（开战时用于 init_deck）
std::vector<CardId> CardSystem::get_master_deck_card_ids() const {
    std::vector<CardId> ids;
    ids.reserve(master_deck_.size());
    for (const auto& c : master_deck_) ids.push_back(c.id);
    return ids;
}

// 永久添加一张牌到 master deck（奖励/商店/事件）
void CardSystem::add_to_master_deck(CardId id) {
    CardInstance c;
    c.instanceId = master_deck_.empty() ? 1 : (master_deck_.back().instanceId + 1);
    c.id = std::move(id);
    c.temporary = false;
    master_deck_.push_back(std::move(c));
}

// 从 master deck 删除一张牌（按实例 id）
bool CardSystem::remove_from_master_deck(InstanceId instance_id) {
    auto it = std::find_if(master_deck_.begin(), master_deck_.end(),
                           [instance_id](const CardInstance& c) { return c.instanceId == instance_id; });
    if (it == master_deck_.end()) return false;
    master_deck_.erase(it);
    return true;
}

// 升级 master deck 中某张牌（按实例 id；将 id 改为升级版 id，如 strike→strike+）
bool CardSystem::upgrade_card_in_master_deck(InstanceId instance_id) {
    auto it = std::find_if(master_deck_.begin(), master_deck_.end(),
                           [instance_id](const CardInstance& c) { return c.instanceId == instance_id; });
    if (it == master_deck_.end()) return false;
    if (!it->id.empty() && it->id.back() != '+') it->id += "+";
    return true;
}

// 生成临时牌到手牌（不进入 master deck；temporary=true；手牌满则落入弃牌堆）
void CardSystem::generate_to_hand(CardId id) {
    CardInstance c;
    c.id = std::move(id);
    c.temporary = true;
    add_to_hand(c);
}

// 生成临时牌到抽牌堆（洗入 draw pile；temporary=true）
void CardSystem::generate_to_draw_pile(CardId id) {
    CardInstance c;
    c.id = std::move(id);
    c.temporary = true;
    add_to_deck(c);
}

// 生成临时牌到弃牌堆（temporary=true）
void CardSystem::generate_to_discard_pile(CardId id) {
    CardInstance c;
    c.id = std::move(id);
    c.temporary = true;
    add_to_discard(c);
}

// 生成临时牌到消耗堆（temporary=true；本场战斗不再参与抽洗）
void CardSystem::generate_to_exhaust_pile(CardId id) {
    CardInstance c;
    c.id = std::move(id);
    c.temporary = true;
    add_to_exhaust(c);
}

// 战斗开始初始化战斗牌堆：清空四堆，将给定 CardId 生成实例并随机放入抽牌堆
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
        c.temporary  = false;
        draw_pile_.push_back(c);
    }
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(draw_pile_.begin(), draw_pile_.end(), g);
}

// 抽牌：从抽牌堆顶部抽 n 张进手牌；抽牌堆空则洗弃牌入抽牌堆再继续；手牌最多 hand_limit_
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

// 获取当前手牌（只读引用）
const std::vector<CardInstance>& CardSystem::get_hand() const {
    return hand_;
}

// 从手牌按下标移除并返回（出牌/弃牌均可用；去向由调用方决定）
CardInstance CardSystem::remove_from_hand(int hand_index) {
    CardInstance c = hand_.at(static_cast<size_t>(hand_index));
    hand_.erase(hand_.begin() + hand_index);
    return c;
}

// 加入手牌：如 instanceId==0 则分配新实例；若手牌已满则改为进入弃牌堆
void CardSystem::add_to_hand(CardInstance card) {
    if (card.instanceId == 0) {
        card.instanceId = ++next_instance_id_;
    }
    if (static_cast<int>(hand_.size()) >= hand_limit_) {
        discard_pile_.push_back(card);
        return;
    }
    hand_.push_back(card);
}

// 加入弃牌堆：如 instanceId==0 则分配新实例
void CardSystem::add_to_discard(CardInstance card) {
    if (card.instanceId == 0) {
        card.instanceId = ++next_instance_id_;
    }
    discard_pile_.push_back(card);
}

// 加入消耗堆：如 instanceId==0 则分配新实例；消耗堆不参与洗牌与抽牌
void CardSystem::add_to_exhaust(CardInstance card) {
    if (card.instanceId == 0) {
        card.instanceId = ++next_instance_id_;
    }
    exhaust_pile_.push_back(card);
}

// 洗弃牌入抽牌堆：把弃牌堆全部移入抽牌堆并打乱；清空弃牌堆
void CardSystem::shuffle_discard_into_draw() {
    for (auto& c : discard_pile_)
        draw_pile_.push_back(c);
    discard_pile_.clear();
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(draw_pile_.begin(), draw_pile_.end(), g);
}

// 加入抽牌堆（常用于“洗入抽牌堆”）：如 instanceId==0 则分配新实例
void CardSystem::add_to_deck(CardInstance card) {
    if (card.instanceId == 0) {
        card.instanceId = ++next_instance_id_;
    }
    draw_pile_.push_back(card);
}

// 从战斗循环的三堆（手牌/抽牌堆/弃牌堆）移除指定实例（不包含消耗堆）
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

// 抽牌堆大小（用于 UI 计数）
int CardSystem::get_deck_size() const {
    return static_cast<int>(draw_pile_.size());
}

// 弃牌堆大小（用于 UI 计数）
int CardSystem::get_discard_size() const {
    return static_cast<int>(discard_pile_.size());
}

// 消耗堆大小（用于 UI 计数）
int CardSystem::get_exhaust_size() const {
    return static_cast<int>(exhaust_pile_.size());
}

// 获取抽牌堆（只读引用）
const std::vector<CardInstance>& CardSystem::get_draw_pile() const {
    return draw_pile_;
}

// 获取弃牌堆（只读引用）
const std::vector<CardInstance>& CardSystem::get_discard_pile() const {
    return discard_pile_;
}

// 获取消耗堆（只读引用）
const std::vector<CardInstance>& CardSystem::get_exhaust_pile() const {
    return exhaust_pile_;
}

// 升级战斗中（手/抽/弃）任意位置的一张牌（按实例 id；将 id 改为升级版）
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

// 注册卡牌效果函数（CardId → EffectFn）
void CardSystem::register_card_effect(CardId id, std::function<void(EffectContext&)> fn) {
    if (fn) effect_registry_[std::move(id)] = std::move(fn);
}

// 执行卡牌效果：按 CardId 查表调用
void CardSystem::execute_effect(CardId id, EffectContext& ctx) {
    auto it = effect_registry_.find(id);
    if (it != effect_registry_.end())
        it->second(ctx);
}

} // namespace tce
