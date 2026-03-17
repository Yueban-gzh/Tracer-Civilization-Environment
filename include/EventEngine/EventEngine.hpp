/**
 * D - 事件/商店/休息：事件树执行、先序/层序遍历
 * 见 docs/D模块设计与接口.md
 *
 * 课设数据结构：树（事件决策树、先序/层序遍历）
 */
#pragma once

#include "DataLayer/DataTypes.h"
#include "Common/Types.hpp"
#include <functional>
#include <vector>

namespace tce {

class EventEngine {
public:
    using EventId     = DataLayer::EventId;
    using Event       = DataLayer::Event;
    using EventResult = DataLayer::EventResult;

    /// E 提供：按 id 获取事件；E 未就绪时可注入 Mock（返回写死的事件树）
    using GetEventByIdFn = std::function<const Event*(EventId)>;

    /// C 提供：永久牌组操作（事件奖励/商店/休息时使用）
    using AddToMasterDeckFn           = std::function<void(CardId)>;
    using RemoveFromMasterDeckFn     = std::function<bool(InstanceId)>;
    using UpgradeCardInMasterDeckFn  = std::function<bool(InstanceId)>;

    /** 构造：必须传入 get_event_by_id；C 的接口可选，不传则商店/休息相关接口为 no-op。 */
    explicit EventEngine(
        GetEventByIdFn get_event_by_id,
        AddToMasterDeckFn add_to_master_deck = {},
        RemoveFromMasterDeckFn remove_from_master_deck = {},
        UpgradeCardInMasterDeckFn upgrade_card_in_master_deck = {}
    );

    // ---------- 事件流程 ----------
    void start_event(EventId id);
    const Event* get_current_event() const;
    bool choose_option(int option_index);
    bool get_event_result(EventResult& out) const;

    // ---------- 先序/层序遍历（课设要求）----------
    void traverse_preorder(EventId root_id, std::function<void(const Event&)> visit) const;
    void traverse_level_order(EventId root_id, std::function<void(const Event&)> visit) const;

    // ---------- 商店（阶段 1 可占位）----------
    void open_shop();
    bool buy_card(CardId card_id);           // 向永久牌组加一张牌，内部调 C 的 add_to_master_deck
    bool remove_card_from_master_deck(InstanceId instance_id); // 商店删牌，内部调 C 的 remove_from_master_deck

    // ---------- 休息 ----------
    void rest_heal(int amount);              // 占位：实际加血由主流程或 B 维护时主流程调
    bool rest_upgrade_card(InstanceId instance_id); // 休息升级一张牌，内部调 C 的 upgrade_card_in_master_deck

    /** 应用事件结果：根据 result.type 加牌(card_reward)/加金/回血；gold 与 heal 通过回调通知主流程。 */
    void apply_event_result(const EventResult& result,
        std::function<void(int)> on_gold_add = {},
        std::function<void(int)> on_heal = {});

private:
    GetEventByIdFn get_event_by_id_;
    AddToMasterDeckFn add_to_master_deck_;
    RemoveFromMasterDeckFn remove_from_master_deck_;
    UpgradeCardInMasterDeckFn upgrade_card_in_master_deck_;
    const Event* current_event_ = nullptr;
    bool         has_result_    = false;
    EventResult  last_result_{};
};

} // namespace tce
