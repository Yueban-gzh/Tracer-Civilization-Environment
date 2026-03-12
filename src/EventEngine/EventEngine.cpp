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

// 构造函数：通过可调用对象 get_event_by_id 将 EventEngine 与 DataLayer 解耦
// - 正式联调时传入 DataLayerImpl::get_event_by_id 的包装 lambda；
// - 单元测试或 E 未就绪时可以传入 Mock 函数（返回写死的事件树）。
EventEngine::EventEngine(EventEngine::GetEventByIdFn get_event_by_id)
    : get_event_by_id_(std::move(get_event_by_id)) {}

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
        const Event* e = get_event_by_id_(id);
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

// 以下接口为阶段 1 的占位实现：
// - 阶段 2 中会注入 C 模块的接口（如 add_to_deck / upgrade_card_in_deck）
//   并与玩家状态联动，真正实现商店与休息逻辑。
void EventEngine::open_shop() {
    // 阶段 1 占位：仅表示进入商店状态；阶段 2 实现商品列表等
}

bool EventEngine::buy_card(const std::string& /* card_id */) {
    // 阶段 2 注入 C 的 add_to_deck 后实现
    return false;
}

void EventEngine::rest_heal(int /* amount */) {
    // 血量由主流程/B 维护时由主流程直接改；此处占位
}

bool EventEngine::rest_upgrade_card(int /* instance_id */) {
    // 阶段 2 注入 C 的 upgrade_card_in_deck 后实现
    return false;
}

} // namespace tce
