# D - 事件/商店/休息模块设计与接口（定稿即按此实现）

> 本文档定义 **EventEngine（D）** 的模块边界、事件树数据结构、以及对外/对 E、C 的接口规格。  
> 与 [设计与接口.md](设计与接口.md) 一致；id 类型等若阶段 0 有变更，仅改本节类型定义即可。

---

## 一、模块划分与职责

| 模块 | 职责 | 课设数据结构落点 |
|------|------|------------------|
| **D - EventEngine** | 事件树执行（选项→跳转/结果）、商店（买牌、删一张牌）、休息（回血 or 升级一张牌） | **树**：事件决策树、先序/层序遍历 |

**依赖**：D 依赖 **E**（get_event_by_id）、**C**（add_to_deck、remove_from_deck、upgrade_card_in_deck）。

---

## 二、公共类型与 E 提供的数据结构

### 2.1 标识类型（与 E/设计与接口 约定一致）

- **EventId**：事件唯一 id，`std::string`（如 `"event_001"`），与 [DataLayer/DataTypes.h](../include/DataLayer/DataTypes.h) 中 `DataLayer::EventId` 一致。
- **CardId**、**InstanceId**：与 C/Common 约定一致，D 调用 C 时使用（如 `add_to_deck(CardInstance)`、`remove_from_deck(InstanceId)`、`upgrade_card_in_deck(InstanceId)`）。

### 2.2 由 E 提供、D 只读使用的数据结构

事件表一条记录（E 的 `get_event_by_id` 返回）在 [DataLayer/DataTypes.h](../include/DataLayer/DataTypes.h) 中已定义：

- **Event**：id, title, description, options（`std::vector<EventOption>`）。
- **EventOption**：text, next（有则跳转 EventId）, result（有则结束，`EventResult`）。
- **EventResult**：type（如 `"gold"`/`"heal"`/`"card_reward"`/`"none"`）, value（int）。

事件树在数据上为**多条 Event 记录通过 options[].next 引用子事件 id** 形成的逻辑树；D 通过 `get_event_by_id(next)` 访问子节点，不要求 E 预先建好指针树。

---

## 三、D - 事件树：数据结构与接口

### 3.1 需实现的数据结构（课设：树）

**（1）事件树节点结构（D 内部）**

- 用于**先序/层序遍历**与当前「正在执行的事件」状态。可选用以下之一：
  - **方案 A**：直接使用 E 的 `const Event*` 作为「当前节点」，子节点通过 `option.next` 再调 `get_event_by_id(next)` 得到，不额外建树；遍历时动态从 E 取子节点。
  - **方案 B**：D 内建一棵「事件树」结构，节点类型包含 id、title、description、options，以及**子节点指针或 EventId 列表**；从 E 加载后递归建树，遍历在本地树上做。

课设要求「树」的落点：**事件决策树、先序/层序遍历**。若采用方案 A，则「树」是逻辑树（由 E 的 Event + next 引用构成），先序/层序通过递归/队列访问 `get_event_by_id` 得到的节点实现；若采用方案 B，则「树」是显式树结构，先序/层序在显式节点上遍历。任选其一，在报告中说明选择与复杂度。

**（2）当前状态（一次事件流程）**

- **当前事件节点**：当前展示的 Event（id、title、description、options）。
- **是否已结束**：玩家选到带 `result` 的选项后，事件结束，D 可返回该 result 供主流程应用（加金、回血、发牌奖励等）。

### 3.2 事件相关对外接口（函数规格）

| 接口 | 签名（逻辑） | 行为 |
|------|--------------|------|
| start_event | `void start_event(EventId id)` | 开始一次事件流程：根据 id 从 E 取事件（get_event_by_id），设为当前节点；清空「已结束」状态。若 id 无效则当前节点为空，后续 choose_option 无效。 |
| get_current_event | `const Event* get_current_event() const` 或返回当前 title/description/options | 返回当前事件节点信息，供 UI 展示标题、描述、选项列表。无当前节点时返回 null 或空。 |
| choose_option | `bool choose_option(int option_index)` | 玩家选择第 option_index 个选项（0-based）。若该选项有 **next**：从 E 取 get_event_by_id(next) 设为当前节点，返回 true；若该选项有 **result**：记录 result，标记事件结束，返回 true。option_index 越界或当前无节点则返回 false。 |
| get_event_result | `bool get_event_result(EventResult& out) const` 或 `optional<EventResult> get_event_result() const` | 若事件已结束（选到带 result 的选项），将结果写入 out 或返回 optional；否则返回 false 或 nullopt。主流程根据 result.type/value 调用 C（加牌）、或改金钱/血量等。 |

**EventResult 应用（主流程或 D 内）**：  
- `type == "gold"`：加金钱，value 为数值。  
- `type == "heal"`：加血量，value 为数值。  
- `type == "card_reward"`：发牌奖励，value 为张数，主流程可调 C 的 add_to_deck 或由 D 调 C。  
- `type == "none"`：无效果。

约定：**谁应用 result** 可在阶段 2 与主流程约定（主流程调 D 的 get_event_result 后自己改状态并调 C，或 D 内部调 C/E 的接口直接应用）；接口上 D 至少提供 get_event_result。

### 3.3 先序/层序遍历（课设要求）

- **先序遍历**：对当前事件树（从某根 EventId 起）先访问根节点，再依次对每个 option 的 next 子事件递归先序遍历。接口示例：`void traverse_preorder(EventId root_id, std::function<void(const Event&)> visit)` 或输出到 vector。
- **层序遍历**：从 root_id 起，按层（BFS）依次访问事件节点。接口示例：`void traverse_level_order(EventId root_id, std::function<void(const Event&)> visit)`。

遍历时通过 E 的 get_event_by_id 获取节点；若 E 未就绪，则用 Mock E 返回一棵写死的事件树（如固定 1～2 个事件节点）。

### 3.4 商店相关对外接口

| 接口 | 签名（逻辑） | 行为 |
|------|--------------|------|
| open_shop | `void open_shop()` 或 `bool open_shop()` | 打开商店（阶段 1 可仅占位：设置「商店已打开」状态；阶段 2 实现商品列表、价格等）。 |
| buy_card | `bool buy_card(CardId id, int cost)` 或由主流程扣钱后调 C | 购买一张牌加入牌组：主流程扣减金钱后，调用 C 的 add_to_deck；若 D 负责扣钱则需持有或查询当前金钱。约定：MVP 由主流程扣钱，D 只调 C 的 add_to_deck(CardInstance)。 |
| remove_card_from_deck | 由主流程调 **C** 的 `remove_from_deck(InstanceId)` | 商店「删一张牌」：主流程或 D 调 C 的 remove_from_deck(instance_id)，不单独在 D 再定义接口；D 若提供「选一张牌删」的 UI 逻辑，内部调 C。 |

文档约定：**商店买牌** 由主流程扣金钱 + 调 D 的 buy_card 或直接调 C 的 add_to_deck；**商店删牌** 统一用 C 的 remove_from_deck(InstanceId)。D 的 open_shop 表示进入商店状态，buy_card 可仅为「向牌组加一张牌」的封装（入参 CardId，内部构造 CardInstance 调 C）。

### 3.5 休息相关对外接口

| 接口 | 签名（逻辑） | 行为 |
|------|--------------|------|
| rest_heal | `void rest_heal(int amount)` | 休息回血：将玩家当前血量增加 amount（不超过上限）。血量由主流程或 B 维护时，可由主流程调本接口或直接改状态；若 D 不持有血量，则本接口仅为占位或调主流程回调。 |
| rest_upgrade_card | `bool rest_upgrade_card(InstanceId instance_id)` | 休息升级一张牌：调用 C 的 upgrade_card_in_deck(instance_id)，返回 C 的返回值。 |

约定：**rest_heal** 若主流程持有玩家状态，可由主流程在选「回血」时直接加血，D 仅提供接口名便于主流程统一调用；**rest_upgrade_card** 必须调 C 的 upgrade_card_in_deck。

---

## 四、D 依赖 E 的接口（D 调用）

| 接口 | 约定签名（逻辑） | 说明 |
|------|------------------|------|
| get_event_by_id | `const Event* get_event_by_id(EventId id) const` 或通过注入 | D 在 start_event、choose_option（选 next 时）、先序/层序遍历时调用。E 未就绪时由 D 自写 Mock E 返回一棵写死的事件树。 |

---

## 五、D 依赖 C 的接口（D 调用）

| 接口 | 说明 |
|------|------|
| add_to_deck(CardInstance) | 事件/商店「获得一张牌」时调用；CardInstance.id 为 CardId，instanceId 可由 C 分配。 |
| remove_from_deck(InstanceId) | 商店「删一张牌」时由主流程或 D 调用。 |
| upgrade_card_in_deck(InstanceId) | 休息「升级一张牌」时调用。 |

---

## 六、文件与符号归属（建议）

| 内容 | 放置位置 |
|------|----------|
| D：事件树节点类型（若采用显式树）、EventEngine 类声明、遍历声明 | `include/EventEngine/EventEngine.hpp` |
| D：上述接口实现、先序/层序遍历实现 | `src/EventEngine/EventEngine.cpp` |
| EventId、Event、EventOption、EventResult | 使用 E 的 `DataLayer::DataTypes.h`，D 仅包含该头文件 |

---

## 七、阶段 0/1 小结：D 实现清单

**阶段 0（本阶段）**  
- 对外接口与依赖已在本文档写清，与《设计与接口》第五节一致。  
- 全员按本约定开发；D 实现时以本文档为准。

**阶段 1**  
- 定义事件树节点结构（或采用逻辑树 + get_event_by_id）。  
- 实现 start_event、get_current_event、choose_option、get_event_result；实现 1～2 个示例事件可跑通（选项→next/result）。  
- 实现先序、层序遍历（接口可暴露 traverse_preorder、traverse_level_order）。  
- E 未就绪时自写 Mock E（返回一棵写死的事件树）；联调时替换为真 E。  
- open_shop、buy_card、rest_heal、rest_upgrade_card 可先占位或简单实现（如 rest_upgrade_card 直接调 C）。

以上为 D 模块的完整设计与接口定稿。
