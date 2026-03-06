/**
 * C - 卡牌系统：四牌堆、抽弃洗、效果执行
 */
#pragma once

#include "../Common/Types.hpp"
#include <vector>
#include <functional>

namespace tce {

struct CardData;
class EffectContext;

struct CardInstance {
    InstanceId instanceId = 0;
    CardId     id;
};

class CardSystem {
public:
    using GetCardByIdFn = std::function<const CardData*(CardId)>;

    explicit CardSystem(GetCardByIdFn get_card_by_id);

    void init_deck(const std::vector<CardId>& initial_card_ids);
    void draw_cards(int n);
    const std::vector<CardInstance>& get_hand() const;
    CardInstance remove_from_hand(int hand_index);
    void add_to_hand(CardInstance card);
    void add_to_discard(CardInstance card);
    void add_to_exhaust(CardInstance card);
    void shuffle_discard_into_draw();
    void add_to_deck(CardInstance card);
    bool remove_from_deck(InstanceId instance_id);
    int  get_deck_size() const;
    int  get_discard_size() const;
    int  get_exhaust_size() const;
    bool upgrade_card_in_deck(InstanceId instance_id);
    void execute_effect(CardId id, EffectContext& ctx);

private:
    GetCardByIdFn get_card_by_id_;
    int           next_instance_id_ = 0;
    static constexpr int hand_limit_ = 10;

    std::vector<CardInstance> draw_pile_;
    std::vector<CardInstance> hand_;
    std::vector<CardInstance> discard_pile_;
    std::vector<CardInstance> exhaust_pile_;
};

} // namespace tce
