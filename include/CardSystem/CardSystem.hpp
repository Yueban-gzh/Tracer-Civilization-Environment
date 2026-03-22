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
    // 结茧等：本场战斗中该实例的**技能牌**耗能视为 0（打出时由 BattleEngine 结算）。
    bool       combat_cost_zero = false;
};

class CardSystem {
public:
    using GetCardByIdFn = std::function<const CardData*(CardId)>;

    /** 构造卡牌系统；通过回调从数据层按 id 获取 CardData（只读）。 */
    explicit CardSystem(GetCardByIdFn get_card_by_id);

    // --- 永久牌组（master deck，跨战斗持久化）---
    /** 初始化永久牌组（主流程/存档读取时调用）；会清空旧 master deck 并重新生成实例 id。 */
    void init_master_deck(const std::vector<CardId>& card_ids);
    /** 获取永久牌组实例列表（只读引用）。 */
    const std::vector<CardInstance>& get_master_deck() const;
    /** 获取永久牌组的 CardId 列表（用于开战时喂给 BattleEngine）。 */
    std::vector<CardId> get_master_deck_card_ids() const;
    /** 永久加入一张牌到 master deck（如战斗奖励选牌、商店买牌）。 */
    void add_to_master_deck(CardId id);
    /** 从 master deck 移除指定实例（如商店删牌）；返回是否成功。若 out_removed_id 非空则写入被删牌的 id。 */
    bool remove_from_master_deck(InstanceId instance_id, CardId* out_removed_id = nullptr);
    /** 升级 master deck 中指定实例（如营火升级）；返回是否成功。 */
    bool upgrade_card_in_master_deck(InstanceId instance_id);

    /** 战斗中“生成”一张临时牌：不会进入玩家真实牌组（右上角牌组视图会过滤掉 temporary=true）。 */
    /** 生成一张临时牌并加入手牌（若手牌已满则按规则落入弃牌堆）。 */
    void generate_to_hand(CardId id);
    /** 生成一张临时牌并洗入抽牌堆（加入 draw pile）。 */
    void generate_to_draw_pile(CardId id);
    /** 同上，并可标记本场战斗中技能耗能视为 0（结茧）。 */
    void generate_to_draw_pile(CardId id, bool combat_cost_zero_skill);
    /** 生成一张临时牌并加入弃牌堆。 */
    void generate_to_discard_pile(CardId id);
    /** 生成一张临时牌并加入消耗堆。 */
    void generate_to_exhaust_pile(CardId id);

    /** 战斗开始时初始化战斗牌堆：根据给定 CardId 列表生成实例并随机放入抽牌堆，同时清空手牌/弃牌/消耗堆。 */
    void init_deck(const std::vector<CardId>& initial_card_ids);
    /** 抽牌：从抽牌堆顶部抽 n 张到手牌；抽牌堆空时会把弃牌堆洗回抽牌堆后继续抽；手牌最多 10。 */
    void draw_cards(int n);
    /** 获取当前手牌列表（只读引用）。 */
    const std::vector<CardInstance>& get_hand() const;
    /** 从手牌按下标移除并返回该牌实例（出牌/弃牌都用它），后续去向由调用方决定。 */
    CardInstance remove_from_hand(int hand_index);
    /** 将牌实例加入手牌：若 instanceId 为 0 则分配新实例 id；若手牌已满则直接进入弃牌堆。 */
    void add_to_hand(CardInstance card);
    /** 将牌实例加入弃牌堆：若 instanceId 为 0 则分配新实例 id。 */
    void add_to_discard(CardInstance card);
    /** 将牌实例加入消耗堆：若 instanceId 为 0 则分配新实例 id；进入消耗堆后本场战斗不再参与抽洗。 */
    void add_to_exhaust(CardInstance card);
    /** 将弃牌堆全部洗回抽牌堆并打乱顺序（消耗堆不参与）。 */
    void shuffle_discard_into_draw();
    /** 将牌实例加入抽牌堆（常用于“洗入抽牌堆”）：若 instanceId 为 0 则分配新实例 id。 */
    void add_to_deck(CardInstance card);
    /** 从手牌/抽牌堆/弃牌堆中移除指定实例（不包含消耗堆）；返回是否成功。 */
    bool remove_from_deck(InstanceId instance_id);
    /** 获取抽牌堆张数。 */
    int  get_deck_size() const;
    /** 获取弃牌堆张数。 */
    int  get_discard_size() const;
    /** 获取消耗堆张数。 */
    int  get_exhaust_size() const;
    /** 牌组界面用：获取各牌堆列表以合并展示（手牌用 get_hand） */
    /** 获取抽牌堆（只读引用）。 */
    const std::vector<CardInstance>& get_draw_pile() const;
    /** 获取弃牌堆（只读引用）。 */
    const std::vector<CardInstance>& get_discard_pile() const;
    /** 获取消耗堆（只读引用）。 */
    const std::vector<CardInstance>& get_exhaust_pile() const;
    /** 升级战斗中（手/抽/弃）任意位置的指定实例：将 id 改为升级版（如 strike→strike+）；返回是否成功。 */
    bool upgrade_card_in_deck(InstanceId instance_id);
    /** 升级当前战斗中的所有牌（手/抽/弃/消耗）；返回实际升级张数。 */
    int  upgrade_all_cards_in_combat();
    /** 将当前手牌全部移入消耗堆；返回消耗张数。 */
    int  exhaust_all_hand_cards();
    /** 从手牌随机弃掉 count 张（移入弃牌堆）；返回实际弃牌张数。 */
    int  discard_random_hand_cards(int count);
    /** 从手牌按实例 id 弃掉 1 张；成功返回 1，失败返回 0。 */
    int  discard_hand_card_by_instance_id(InstanceId instance_id);
    /** 将手牌全部弃掉（移入弃牌堆）；返回实际弃牌张数。 */
    int  discard_all_hand_cards();
    /** 从手牌随机消耗 count 张（移入消耗堆）；返回实际消耗张数。 */
    int  exhaust_random_hand_cards(int count);
    /** 从手牌按实例 id 消耗 1 张；成功返回 1，失败返回 0。 */
    int  exhaust_hand_card_by_instance_id(InstanceId instance_id);
    /** 消耗手牌中所有非攻击牌；返回实际消耗张数。 */
    int  exhaust_non_attack_hand_cards();
    /** 从抽牌堆中随机将攻击牌移入手牌（抽牌堆空时会先洗弃牌入抽牌堆）；返回实际入手张数。 */
    int  draw_random_attack_cards_from_draw_pile(int max_count);
    /** 从抽牌堆中随机将技能牌（仅 Skill，不含 Power）移入手牌；返回实际入手张数。 */
    int  draw_random_skill_cards_from_draw_pile(int max_count);
    /** 牌堆间移动（统一视为「起点 pile → 终点 pile」）：消耗堆实例 → 手牌（发掘等）。 */
    bool move_exhaust_card_to_hand(InstanceId instance_id);
    /** 随机升级手牌中的 count 张可升级卡；返回实际升级张数。 */
    int  upgrade_random_cards_in_hand(int count);
    /** 升级手牌中全部可升级卡；返回实际升级张数。 */
    int  upgrade_all_cards_in_hand();
    /** 执行卡牌效果：按 CardId 查找注册表并调用对应效果函数。 */
    void execute_effect(CardId id, EffectContext& ctx);
    /** 检查某卡牌是否已注册效果（未注册则打出会消耗能量但无效果）。 */
    bool has_effect_registered(CardId id) const;

    /** 注册卡牌效果：CardId → 效果函数，出牌时由 execute_effect 查表调用 */
    /** 注册某张卡的效果函数（CardId → EffectFn）。 */
    void register_card_effect(CardId id, std::function<void(EffectContext&)> fn);

private:
    GetCardByIdFn get_card_by_id_;
    int           next_instance_id_ = 0;
    static constexpr int hand_limit_ = 10;

    // 玩家真实牌组（永久）：不参与战斗四牌堆的洗牌/抽弃逻辑
    std::vector<CardInstance> master_deck_;

    std::vector<CardInstance> draw_pile_;
    std::vector<CardInstance> hand_;
    std::vector<CardInstance> discard_pile_;
    std::vector<CardInstance> exhaust_pile_;
    std::unordered_map<CardId, std::function<void(EffectContext&)>> effect_registry_;
};

} // namespace tce
