/**
 * D - 事件/商店/休息：事件树执行、先序/层序遍历
 * 见 docs/D模块设计与接口.md
 *
 * 课设数据结构：树（事件决策树、先序/层序遍历）
 */
#pragma once

#include "DataLayer/DataTypes.h"
#include <functional>
#include <vector>

namespace tce {

class EventEngine {
public:
    using EventId   = DataLayer::EventId;
    using Event       = DataLayer::Event;
    using EventResult = DataLayer::EventResult;

    /// E 提供：按 id 获取事件；E 未就绪时可注入 Mock（返回写死的事件树）
    using GetEventByIdFn = std::function<const Event*(EventId)>;

    explicit EventEngine(GetEventByIdFn get_event_by_id);

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
    bool buy_card(const std::string& card_id);  // 向牌组加一张牌，实际调 C 需阶段 2 注入

    // ---------- 休息 ----------
    void rest_heal(int amount);             // 占位：实际加血由主流程或 B 维护时主流程调
    bool rest_upgrade_card(int instance_id); // 调 C 的 upgrade_card_in_deck，需阶段 2 注入 C

private:
    GetEventByIdFn get_event_by_id_;
    const Event*   current_event_ = nullptr;
    bool           has_result_    = false;
    EventResult    last_result_{};
};

} // namespace tce
