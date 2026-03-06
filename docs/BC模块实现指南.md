# B（战斗引擎）与 C（卡牌系统）实现指南

> 本文档说明 **实现顺序、Mock 约定与报告落点**。  
> **模块划分、数据结构和接口规格**以 [BC模块设计与接口.md](BC模块设计与接口.md) 为准，实现时请按该文档定义的类型与接口编码。

---

## 一、实现顺序与依赖


| 顺序    | 模块           | 原因                             |
| ----- | ------------ | ------------------------------ |
| **1** | **C - 卡牌系统** | B 依赖 C；C 只依赖 E，E 未就绪时用 Mock 即可 |
| **2** | **B - 战斗引擎** | 调用 C 的接口 + E 的怪物数据（E 可 Mock）   |


**依赖关系**：`E ← C`，`C ← B`。先实现 C 并自测通过，再实现 B；B 阶段 1 可用 Mock C + Mock E 单测战斗流程，阶段 2 联调时替换为真 E。

---

## 二、文件放置位置


| 模块       | 源文件                      | 头文件                                  |
| -------- | ------------------------ | ------------------------------------ |
| C - 卡牌系统 | `src/CardSystem/*.cpp`   | `include/CardSystem/*.h` 或 `*.hpp`   |
| B - 战斗引擎 | `src/BattleEngine/*.cpp` | `include/BattleEngine/*.h` 或 `*.hpp` |


建议每个模块至少一个「对外接口头文件」+ 一个实现文件，例如：

- C：`include/CardSystem/CardSystem.hpp`，`src/CardSystem/CardSystem.cpp`
- B：`include/BattleEngine/BattleEngine.hpp`，`src/BattleEngine/BattleEngine.cpp`

---

## 三、C / B 的模块、数据结构与接口

**以 [BC模块设计与接口.md](BC模块设计与接口.md) 为准**，其中已定义：**C** 的 `CardInstance`、三个牌堆（线性表/链表）、全部对外接口及依赖 E 的 get_card_by_id；**B** 的 `PlayerBattleState`、`MonsterInBattle`、`BattleStateSnapshot`，栈/队列落点，全部对外接口及依赖 C、E 的约定。实现时按该设计文档逐项编码即可。

---

### （以下为原实现要点摘要，详细规格见设计文档）

**C - 课设数据结构**：线性表/链表（drawPile、hand、discardPile）。

### 3.2 核心数据与类型

- **单张牌**：至少需要「卡牌 id」+ 可选「是否已升级」。具体属性（name, cost, effectType, effectValue）由 **E 的 get_card_by_id** 查表得到，C 只存 id（及升级标记即可）。
- **CardId**：与《设计与接口》一致，用 `string` 或 `int`，需与 E 约定一致。
- 三个牌堆建议用 `std::vector<CardInstance>` 或自定义链表，其中 `CardInstance` 可定义为 `{ CardId id; bool upgraded; }`。

### 3.3 对外接口清单（按设计文档）


| 接口                                       | 功能说明                                | 实现注意                                                     |
| ---------------------------------------- | ----------------------------------- | -------------------------------------------------------- |
| `init_deck(initial_card_ids)`            | 用初始卡牌 id 列表初始化抽牌堆，清空 hand 和 discard | 可调用 E 的 get_card_by_id 校验 id 是否存在，E 未就绪则跳过或 Mock         |
| `draw_cards(n)`                          | 从抽牌堆抽 n 张到手牌                        | 若抽牌堆不足 n 张：先抽m张（设m张为抽牌堆现有卡牌数目），随后将弃牌堆中的牌洗牌后重新放入抽牌堆再抽n-m张 |
| `get_hand()`                             | 返回当前手牌（只读）                          | 返回 const 引用或拷贝，便于 UI/战斗读取                                |
| `remove_from_hand(idx 或 id)`             | 从手牌移除一张牌（出牌时用）                      | 需与 B 约定：按索引还是按 id 移除                                     |
| `add_to_discard(card)`                   | 将一张牌加入弃牌堆                           | 出牌后由 B 调用                                                |
| `shuffle_discard_into_draw()`            | 将弃牌堆全部洗入抽牌堆，清空弃牌堆                   | 抽牌堆空时由 draw_cards 内部调用                                   |
| `add_to_deck(card)`                      | 往抽牌堆或牌组加牌（奖励/商店）                    | 与《设计与接口》中「牌组」含义一致：通常指加入 draw 或单独「牌组」列表，需与主流程约定           |
| `remove_from_deck(cardId 或 索引)`          | 从牌组删一张牌（商店删牌）                       | 若牌在 hand/discard 也需能删，或约定仅从「整副牌」中删，需与 D/主流程约定            |
| `get_deck_size()` / `get_discard_size()` | 当前抽牌堆/弃牌堆张数                         | 供 B 或 UI 显示                                              |
| `upgrade_card_in_deck(cardId 或 索引)`      | 升级牌组中某张牌（休息/事件）                     | 将对应牌的 upgraded 置为 true；若有多张同 id，需约定升级哪一张（如第一张）           |


### 3.4 依赖 E 的接口

- **get_card_by_id(CardId)**：返回卡牌数据（id, name, cost, cardType, effectType, effectValue 等）。  
- **Mock 方式**：在 C 的测试或 B 未联调 E 时，写一个「Mock E」：固定返回一张牌（例如 cost=1 的攻击牌），保证 C 的 init_deck、draw_cards、get_hand 等能跑通。

### 3.5 实现步骤建议

1. 定义 `CardId`、`CardInstance`（id + upgraded）及三个牌堆类型（vector 或链表）。
2. 实现 `init_deck`、`get_hand`、`get_deck_size`、`get_discard_size`。
3. 实现 `shuffle_discard_into_draw`（洗牌算法）。
4. 实现 `draw_cards`（含抽空时先洗弃牌堆再抽）。
5. 实现 `remove_from_hand`、`add_to_discard`（出牌流程由 B 按顺序调用）。
6. 实现 `add_to_deck`、`remove_from_deck`、`upgrade_card_in_deck`。
7. 用 Mock E 写简单单测：初始化 → 抽牌 → 出几张牌 → 弃牌 → 再抽到需要洗牌，检查数量与顺序是否符合预期。

---

## 四、B - 战斗引擎实现要点

### 4.1 课设要求的数据结构

- **栈**：回合内「出的牌」或「结算顺序」——例如每张牌的效果入栈，回合末按栈顺序结算（或反向结算），写报告时可对应「栈」。
- **队列**：**行动顺序**（敌我行动顺序）、**战斗日志**（先发生先输出），写报告时可对应「队列」。

### 4.2 核心数据与类型

- **战斗状态**：当前层/场上的怪物列表（id + 当前血量、意图等）、玩家当前血量、格挡值、能量（每回合重置）。
- **MonsterId**：与 E 约定一致（string 或 int）。怪物属性（maxHp, intentPattern, attackDamage, blockAmount）由 **E 的 get_monster_by_id** 提供。
- 回合流程：玩家出牌（扣能量、按牌效果改怪物血量/己方格挡）→ 结束回合 → 敌方按队列行动（造成伤害等）→ 下一回合（抽牌、补能量）。

### 4.3 对外接口清单（按设计文档）


| 接口                                        | 功能说明                                             | 实现注意                                                                              |
| ----------------------------------------- | ------------------------------------------------ | --------------------------------------------------------------------------------- |
| `start_battle(monster_ids, player_state)` | 初始化一场战斗：怪物列表、玩家血量/能量；并让 C 初始化本局牌堆（或接收已 init 的牌堆） | player_state 至少含初始血量、初始能量；可在此调用 C 的 init_deck                                     |
| `get_battle_state()`                      | 返回当前局面：己方血量/格挡/能量、敌方列表与血量/意图、手牌等                 | 供 UI 或主流程展示，可返回结构体或 JSON                                                          |
| `play_card(card_idx, target?)`            | 打出一张手牌：扣能量、结算效果（伤害/格挡等）、从手牌移除并加入弃牌堆              | 内部调 C：get_hand → 计算效果 → remove_from_hand → add_to_discard；若需指定目标（如多怪）可加 target 参数 |
| `end_turn()`                              | 结束回合：敌方按队列行动、清空格挡（或按规则）、下一回合抽牌并重置能量              | 内部调 C 的 draw_cards、可能 shuffle_discard_into_draw                                   |
| `is_battle_over()`                        | 是否战斗结束（敌方全灭 或 玩家血量≤0）                            | 每回合末或出牌后检查                                                                        |
| `get_reward_cards()`                      | 战斗胜利后可选奖励卡牌（若干张 id）                              | 可调 E 的 sort_cards_by_rarity 后取前 N 张，或随机；E 未就绪时返回固定列表                              |


### 4.4 依赖 C 的接口

B 需调用 C 的：`init_deck`、`draw_cards`、`get_hand`、`remove_from_hand`、`add_to_discard`、`shuffle_discard_into_draw`、`get_deck_size`、`get_discard_size`。  
阶段 1 若 C 未就绪，可自写 **Mock C**：用内存中的 list 模拟手牌/牌堆，固定返回几张「假牌」，保证 B 的回合与胜负逻辑能跑通。

### 4.5 依赖 E 的接口

- **get_monster_by_id(MonsterId)**：返回怪物数据（maxHp, intentPattern, attackDamage, blockAmount 等）。  
- **Mock 方式**：固定返回 1～2 种怪物，血量、伤害写死，便于单测。

### 4.6 实现步骤建议

1. 定义战斗状态结构（怪物列表、玩家血量/格挡/能量）、回合数。
2. 实现 `start_battle`：初始化怪物与玩家状态，调用 C 的 `init_deck`（或接入 Mock C）。
3. 实现 `get_battle_state`、`is_battle_over`。
4. 实现 `play_card`：查手牌、扣费、结算伤害/格挡、调 C 的 remove_from_hand + add_to_discard。
5. 实现 `end_turn`：敌方行动（队列遍历）、清空格挡、下一回合调 C 的 draw_cards。
6. 实现 `get_reward_cards`（可先返回固定列表，联调时再接 E 的排序）。
7. 用 Mock C + Mock E 跑通一局：开局 → 出几张牌 → 结束回合 → 敌方扣血 → 直到胜负，检查状态是否正确。

---

## 五、Mock 约定（与《设计与接口》第六节一致）


| 开发方 | 依赖  | Mock 要点                                                                                                                                         |
| --- | --- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| C   | E   | Mock E：get_card_by_id 始终返回同一张牌（含 cost、name、effectType、effectValue）。                                                                             |
| B   | C、E | Mock C：用内存列表实现 init_deck/draw_cards/get_hand/remove_from_hand/add_to_discard/shuffle/get_*_size，返回固定手牌或简单逻辑。Mock E：get_monster_by_id 返回固定血量/意图。 |


联调时用 E 的真实实现替换 Mock E，用 C 的真实实现替换 Mock C 即可。

---

## 六、与主流程 / 其他模块的对接点

- **主流程**：在「选节点 → 战斗」时调用 B 的 `start_battle`，循环中调用 `get_battle_state`、`play_card`、`end_turn`，直到 `is_battle_over`；胜利后调用 `get_reward_cards` 让玩家选牌，再调 C 的 `add_to_deck` 加入牌组。
- **D（事件/商店/休息）**：商店买牌/删牌、休息升级牌时，直接调 C 的 `add_to_deck`、`remove_from_deck`、`upgrade_card_in_deck`，不需经过 B。

---

## 七、报告可写的数据结构落点


| 模块  | 数据结构   | 可写内容                                                        |
| --- | ------ | ----------------------------------------------------------- |
| C   | 线性表/链表 | deck、hand、discard 的存储与增删、洗牌算法（如 Fisher-Yates）时间复杂度；抽牌、弃牌流程。 |
| B   | 栈      | 回合内出牌顺序或效果结算顺序的栈结构。                                         |
| B   | 队列     | 敌方行动顺序、战斗日志的队列结构。                                           |


---

## 八、阶段 0 需与全员确认的细节（见《设计与接口》第七节）

- 卡牌/怪物 id 用 **string** 还是 **int**？（C、B 与 E 需一致）
- 出牌时 `remove_from_hand` 按 **手牌索引** 还是 **CardId**？（建议索引，避免同名牌歧义）
- 层号从 0 还是 1 开始？（B 不直接涉及，但主流程会用到）

确认后可在《设计与接口》中定稿，本实现指南按该定稿执行即可。