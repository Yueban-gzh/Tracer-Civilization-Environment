/**
 * D - EventEngine implementation: event tree and traversals
 *
 * 设计意图：
 * - EventEngine 自己不保存整棵事件树的数据，只保存「当前事件指针」和「本次事件流程的最终结果」。
 * - 通过构造函数注入的 GetEventByIdFn，从 DataLayer(E) 或 Mock 中按 id 动态查询事件节点。
 * - start_event / choose_option 负责驱动一次「从根事件到结果」的流程；
 * - traverse_preorder / traverse_level_order 提供树结构的先序/层序遍历，用于课设中展示「树 + 遍历」。
 */
#include "EventEngine/EventEngine.hpp"
#include <queue>
#include <unordered_set>

namespace tce {

// 构造函数：注入 E 的 get_event_by_id（必选）与 C 的永久牌组操作（可选）。
// - 不传 C 的回调时，buy_card / rest_upgrade_card / remove_card_from_master_deck 为 no-op 或返回 false。
EventEngine::EventEngine(
    EventEngine::GetEventByIdFn get_event_by_id,
    AddToMasterDeckFn add_to_master_deck,
    RemoveFromMasterDeckFn remove_from_master_deck,
    UpgradeCardInMasterDeckFn upgrade_card_in_master_deck)
    : get_event_by_id_(std::move(get_event_by_id))
    , add_to_master_deck_(std::move(add_to_master_deck))
    , remove_from_master_deck_(std::move(remove_from_master_deck))
    , upgrade_card_in_master_deck_(std::move(upgrade_card_in_master_deck))
{}

// 开始一次事件流程：根据根事件 id 初始化 current_event_，
// 同时清空上一次事件留下的结果标记。
void EventEngine::start_event(EventEngine::EventId id) {
    has_result_  = false;
    current_event_ = get_event_by_id_ ? get_event_by_id_(id) : nullptr;
}

// 读取当前事件节点（只读指针）：
// - UI 层可据此展示标题、描述与选项；
// - 若 current_event_ 为空，表示尚未开始事件或当前事件流程已结束。
const EventEngine::Event* EventEngine::get_current_event() const {
    return current_event_;
}

// 玩家选择一个选项：
// - 若该选项有 next，则跳转到子事件（事件流程继续）；
// - 若该选项只有 result，则记录结果并将 current_event_ 置空（本次事件流程结束）。
bool EventEngine::choose_option(int option_index) {
    if (!current_event_ || option_index < 0 ||
        static_cast<size_t>(option_index) >= current_event_->options.size())
        return false;

    const DataLayer::EventOption& opt = current_event_->options[option_index];

    if (!opt.next.empty()) {
        current_event_ = get_event_by_id_ ? get_event_by_id_(opt.next) : nullptr;
        return true;
    }

    last_result_ = opt.result;
    has_result_  = true;
    current_event_ = nullptr;  // 事件结束
    return true;
}

// 读取本次事件流程的最终结果：
// - 只有在 has_result_ 为 true（即某个选项产生 result）时才返回 true，并填充 out；
// - 否则返回 false，调用方应当认为「当前还没有事件结果」。
bool EventEngine::get_event_result(EventEngine::EventResult& out) const {
    if (!has_result_)
        return false;
    out = last_result_;
    return true;
}

// 先序遍历事件树：
// - 以 root_id 为根节点，访问顺序为「当前节点 -> 每个 next 子节点递归」；
// - visit 回调由调用者提供，可用于统计节点数、打印标题等。
void EventEngine::traverse_preorder(EventEngine::EventId root_id,
                                    std::function<void(const EventEngine::Event&)> visit) const {
    const EventEngine::Event* e = get_event_by_id_ ? get_event_by_id_(root_id) : nullptr;
    if (!e) return;

    visit(*e);
    for (const auto& opt : e->options) {
        if (!opt.next.empty())
            traverse_preorder(opt.next, visit);
    }
}

// 层序遍历事件树（BFS）：
// - 使用队列按层推进，从 root_id 开始逐层访问所有可达事件；
// - 使用 seen 防止事件间存在环时进入死循环。
void EventEngine::traverse_level_order(EventEngine::EventId root_id,
                                       std::function<void(const EventEngine::Event&)> visit) const {
    if (!get_event_by_id_) return;

    std::queue<EventEngine::EventId>           q;
    std::unordered_set<EventEngine::EventId>   seen;
    q.push(root_id);
    seen.insert(root_id);

    while (!q.empty()) {
        EventEngine::EventId id = q.front();
        q.pop();
        const EventEngine::Event* e = get_event_by_id_(id);
        if (!e) continue;

        visit(*e);
        for (const auto& opt : e->options) {
            if (!opt.next.empty() && seen.find(opt.next) == seen.end()) {
                seen.insert(opt.next);
                q.push(opt.next);
            }
        }
    }
}

// ---------- 商店 ----------
void EventEngine::open_shop() {
    // 占位：主流程可据此设置「商店已打开」状态；商品列表/价格由主流程或后续扩展维护。
}

bool EventEngine::buy_card(CardId card_id) {
    if (!add_to_master_deck_) return false;
    add_to_master_deck_(card_id);
    return true;
}

bool EventEngine::remove_card_from_master_deck(InstanceId instance_id) {
    if (!remove_from_master_deck_) return false;
    return remove_from_master_deck_(instance_id);
}

// ---------- 休息 ----------
void EventEngine::rest_heal(int /* amount */) {
    // 血量由主流程/B 维护：主流程在选「回血」时直接改玩家 HP，或通过 apply_event_result 的 on_heal 回调处理。
}

bool EventEngine::rest_upgrade_card(InstanceId instance_id) {
    if (!upgrade_card_in_master_deck_) return false;
    return upgrade_card_in_master_deck_(instance_id);
}

// 应用事件结果：根据 result.type 调用相应逻辑；card_reward 的选牌由主流程根据 get_event_result 自行调 C。
void EventEngine::apply_event_result(const EventResult& result,
    std::function<void(int)> on_gold_add,
    std::function<void(int)> on_heal) {
    auto applyOne = [&](const DataLayer::EventEffect& eff) {
        if (eff.type == "gold" && on_gold_add) on_gold_add(eff.value);
        else if (eff.type == "heal" && on_heal) on_heal(eff.value);
    };

    if (!result.effects.empty()) {
        for (const auto& eff : result.effects) applyOne(eff);
        return;
    }

    // 兼容旧数据结构：type/value
    DataLayer::EventEffect eff{ result.type, result.value };
    applyOne(eff);

    // card_reward/remove_card/relic 等：本函数当前不处理，主流程/其他模块自行结算
}

} // namespace tce
