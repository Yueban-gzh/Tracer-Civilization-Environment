/**
 * C - 卡牌系统：四牌堆、抽弃洗、效果执行
 */
#pragma once

#include "../Common/Types.hpp"
#include <vector>
#include <functional>
#include <unordered_map>

namespace tce {

struct CardData;
class EffectContext;

struct CardInstance {
    InstanceId instanceId = 0;
    CardId     id;
    // 战斗中临时生成的牌（如“复制/生成到手牌”）应标记为 temporary，
    // 以便“牌组(Deck)”视图只展示玩家真实牌组（不包含临时生成牌）。
    bool       temporary = false;
};

class CardSystem {
public:
    using GetCardByIdFn = std::function<const CardData*(CardId)>;

    explicit CardSystem(GetCardByIdFn get_card_by_id);

    /** 战斗中“生成”一张临时牌：不会进入玩家真实牌组（右上角牌组视图会过滤掉 temporary=true）。 */
    void generate_to_hand(CardId id);
    void generate_to_draw_pile(CardId id);
    void generate_to_discard_pile(CardId id);
    void generate_to_exhaust_pile(CardId id);

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
    /** 牌组界面用：获取各牌堆列表以合并展示（手牌用 get_hand） */
    const std::vector<CardInstance>& get_draw_pile() const;
    const std::vector<CardInstance>& get_discard_pile() const;
    const std::vector<CardInstance>& get_exhaust_pile() const;
    bool upgrade_card_in_deck(InstanceId instance_id);
    void execute_effect(CardId id, EffectContext& ctx);

    /** 注册卡牌效果：CardId → 效果函数，出牌时由 execute_effect 查表调用 */
    void register_card_effect(CardId id, std::function<void(EffectContext&)> fn);

private:
    GetCardByIdFn get_card_by_id_;
    int           next_instance_id_ = 0;
    static constexpr int hand_limit_ = 10;

    std::vector<CardInstance> draw_pile_;
    std::vector<CardInstance> hand_;
    std::vector<CardInstance> discard_pile_;
    std::vector<CardInstance> exhaust_pile_;
    std::unordered_map<CardId, std::function<void(EffectContext&)>> effect_registry_;
};

} // namespace tce
