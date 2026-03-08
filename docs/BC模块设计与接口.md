# B、C 模块设计与接口（定稿即按此实现）

> 本文档定义 **CardSystem（C）** 与 **BattleEngine（B）** 的模块边界、需实现的数据结构、以及对外/对 E 的接口规格。  
> 与 [设计与接口.md](设计与接口.md) 一致；id 类型等若阶段 0 有变更，仅改本节类型定义即可。

---

## 一、模块划分与职责


| 模块                   | 职责                                                                     | 课设数据结构落点                               |
| -------------------- | ---------------------------------------------------------------------- | -------------------------------------- |
| **C - CardSystem**   | 管理牌组：抽牌堆(draw)、手牌(hand)、弃牌堆(discard)、消耗牌堆(exhaust)；提供抽/出/弃/消耗/洗/增删/升级。 | **线性表/链表**：四个牌堆的存储与操作。                 |
| **B - BattleEngine** | 管理单场战斗：回合、敌我状态、出牌结算、敌方行动、胜负与奖励。                                        | **栈**：回合内出牌/效果结算顺序。**队列**：敌方行动顺序、战斗日志。 |


**依赖**：C 仅依赖 E（get_card_by_id）。B 依赖 C（牌堆接口）与 E（get_monster_by_id）。

---

## 二、公共类型与 E 提供的数据结构

以下类型在 BC 实现中**直接使用**；CardData / MonsterData 由 **E 定义并实现**，C/B 通过指针或引用使用，或在本模块头文件中**前向声明 + 约定结构体字段**以便编译。

### 2.1 标识类型（与 E 约定一致）

```cpp
// 卡牌/怪物模板 id，对应数据表中的一条记录
using CardId    = std::string;
using MonsterId = std::string;

// 牌组内唯一 id：用于区分多张同 CardId 的牌（如 5 张打击各有一个 InstanceId）
// 新生成的牌（战斗中临时添加）也必须有新实例，InstanceId 由 C 接续前面已分配的值自增（或等价方式），保证本场战斗内不重复即可
// 类型由实现定，如 int 自增、或 uint64_t
using InstanceId = int;  // 示例

// 增益/减益状态 id，如 "strength"/"vulnerable"/"weak"
using StatusId = std::string;  // 或 int，与实现约定一致

// 玩家姿态（枚举），影响造成/受到的伤害等；由卡牌或效果切换
enum class Stance {
    Neutral,  // 无姿态
    Calm,     // 平静等
    Wrath,    // 愤怒等
    Divinity, // 神格等
    // 具体取值由实现与角色设计定
};
```

### 2.2 由 E 提供、C/B 只读使用的数据结构

**CardData**（E 的 get_card_by_id 返回）：  
对标杀戮尖塔，卡牌含**基础属性**与**词条**。**效果不由 E 提供**，而是由 C 通过「卡牌 id → 效果函数」查找并调用（见第三节）。

**基础属性**


| 字段          | 类型          | 含义                                                                                                            |
| ----------- | ----------- | ------------------------------------------------------------------------------------------------------------- |
| id          | CardId      | 卡牌 id                                                                                                         |
| name        | std::string | 名称                                                                                                            |
| cardType    | 枚举          | 攻击/技能/**能力**等。**能力**牌打出后不移入弃牌堆也不移入消耗堆，即本场战斗内从游戏中移除（B 只调用 remove_from_hand，不调用 add_to_discard/add_to_exhaust）。 |
| cost        | int         | 费用（特殊消耗如 X 由 E/B 约定，如 -1 表示 X，-2表示无）                                                                          |
| rarity      | 枚举          | 普通/罕见/稀有                                                                                                      |
| description | std::string | 展示用描述（升级版为另一条 id，如 strike+，对应另一条描述）                                                                           |


**词条（关键词）**  
以下为布尔词条，由 E 在 CardData 中提供；升级可能改变词条（如升级后不再虚无），故 E 可按「未升级/已升级」返回不同 CardData 或同一结构内用字段区分。


| 词条    | 英文/常用名     | 类型   | 含义                                                                                                                                                |
| ----- | ---------- | ---- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| 消耗    | Exhaust    | bool | 打出后从本场战斗移除，**不进入弃牌堆而是进入消耗牌堆**；由 B 出牌时查 E 决定放入弃牌堆还是进入消耗牌堆。                                                                                         |
| 虚无    | Ethereal   | bool | 若回合结束时仍在手牌，则从本场战斗移除，进入消耗牌堆。                                                                                                                       |
| 固有    | Innate     | bool | 开局抽牌时视为必入手牌（首回合抽牌前或抽牌时由 B/C 保证在初始手牌中）。                                                                                                            |
| 保留    | Retain     | bool | 回合结束时**不弃掉**，留在手牌进入下一回合；由 B 在 end_turn 时根据 E 的该词条决定哪些牌保留在手牌。                                                                                      |
| 不能被打出 | Unplayable | bool | 该牌不可被主动打出；B 的 play_card 若目标为该牌则返回失败（如状态牌、诅咒等）。                                                                                                    |
| 已升级   | Upgraded   | —    | **不由 CardInstance 单独存**：是否升级直接看卡牌 id。约定未升级/已升级为两条 CardData（如 id 为 "strike" 与 "strike+"），实例的 id 存哪条即表示是否升级；E 的 get_card_by_id(CardId) 按 id 返回对应模板。 |


**效果不由 E 提供**  
卡牌效果采用「**每张卡对应一个效果函数**」：由 **C** 维护「卡牌 id（及是否升级）→ 效果函数」的注册表，通过查找 id 调用对应函数执行效果。E 只提供上述基础属性与词条，不提供 effectSpec 或任何效果数据。效果函数的注册与实现见第三节 C 的接口。

**CardData 建议字段汇总**


| 字段          | 类型          | 含义             |
| ----------- | ----------- | -------------- |
| id          | CardId      | 卡牌 id          |
| name        | std::string | 名称             |
| cardType    | 枚举          | 攻击/技能/能力/状态/诅咒 |
| cost        | int         | 费用             |
| rarity      | 枚举          | 稀有度            |
| description | std::string | 展示描述           |
| exhaust     | bool        | 词条：消耗          |
| ethereal    | bool        | 词条：虚无          |
| innate      | bool        | 词条：固有          |
| retain      | bool        | 词条：保留          |
| unplayable  | bool        | 词条：不能被打出       |


未升级/已升级由 E 用不同 id 表示（如 "strike" 与 "strike+"），get_card_by_id(id) 按 id 返回对应一条 CardData。

---

**MonsterData**（E 的 get_monster_by_id 返回）：  
与卡牌一致，**意图、攻击伤害、格挡量不由 E 提供**；E 只提供怪物静态数据。具体意图与行动由 **B** 通过「怪物 id → 行为函数」查找并调用（见第四节）。


| 字段    | 类型          | 含义         |
| ----- | ----------- | ---------- |
| id    | MonsterId   | 怪物 id      |
| name  | std::string | 名称         |
| type  | 枚举          | 普通/精英/Boss |
| maxHp | int         | 最大血量       |


C/B 仅依赖上述字段的**存在**与**只读**使用。意图模式、攻击伤害、格挡量等由 B 的怪物行为函数在运行时决定并执行。

---

## 三、C - 卡牌系统：数据结构与接口

### 3.1 需实现的数据结构

**（1）单张牌实例**（牌堆/手牌中实际存在的牌）

- **牌组内唯一 id（InstanceId）**：多张同 CardId 的牌靠 InstanceId 区分；**新生成的牌（战斗中临时添加）也必须有新实例**。C 维护「下一个可用的 InstanceId」（如自增计数器），init_deck 与后续 add_to_hand / add_to_deck 等为未带 instanceId 的牌分配时**接续前面已分配的值**，保证本场战斗内不重复即可。
- **卡牌模板 id（CardId）**：对应 E 的 CardData；是否升级直接看 id（如约定 `"strike"` 未升级、`"strike+"` 已升级，E 提供两条卡牌数据），不再单独存 `upgraded` 字段。

```cpp
struct CardInstance {
    InstanceId instanceId;  // 牌组内唯一，用于区分多张 id 相同的牌
    CardId     id;          // 卡牌模板 id，对应 E 的 CardData；升级后改为升级版 id（如 strike+）
};
```

消耗、虚无、固有、保留、不能被打出等词条来自 E 的 CardData(id)，不在 CardInstance 上冗余存储；B 出牌时查 E 决定是否可打出、放入弃牌堆还是**消耗牌堆**；回合末查 E 决定保留/弃掉/消耗（虚无进消耗牌堆）。C 只负责牌堆与手牌的增删。

**（2）四个牌堆的存储（课设：线性表/链表）**

一场战斗中，卡牌只可能处于四个位置：**抽牌堆（drawPile）**、**手牌（hand）**、**弃牌堆（discardPile）**、**消耗堆（exhaustPile）**。

- **drawPile**（抽牌堆）：`std::vector<CardInstance>` 或自定义链表。**抽牌** = 从抽牌堆**顶部**依次取牌入手牌；实现时需约定「顶部」对应容器的哪一端（如向量尾部为顶，则从尾部取）。战斗开始时牌组中所有卡牌以**随机顺序**放入抽牌堆（见 init_deck）。
- **hand**（手牌）：`std::vector<CardInstance>` 或链表；按**手牌索引**访问，出牌时按索引移除。**手牌数上限**见下。一般情况下只能打出手牌中的牌。
- **discardPile**（弃牌堆）：`std::vector<CardInstance>` 或链表。打出后**非消耗、非能力**的牌追加到此；回合结束时**非保留**手牌放入弃牌堆（虚无牌除外，见下）；**弃牌** = 将手牌中的牌放入弃牌堆而不打出。抽牌堆空时，弃牌堆以随机顺序移入抽牌堆后再抽牌。
- **exhaustPile**（消耗堆）：`std::vector<CardInstance>` 或链表。**消耗**词条牌打出后、效果令某牌**被消耗**时移入此处；**虚无**牌在回合结束时若仍在手牌也移入此处。本场战斗内**不再参与抽牌与洗牌**，仅用于展示/统计。新战斗开始时清空。

要求：四个容器**明确使用线性表或链表**一种，并在报告中说明选择与复杂度。

**手牌数上限**  
手牌数量有上限，默认 **10 张**。  

- **draw_cards(n)**：抽牌时若当前手牌数 + 本次欲抽张数 > 上限，则**只抽到满上限为止**，多出的张数不抽（留在 drawPile）。  
- **add_to_hand(card)**：若当前手牌数已达上限，则**不再加入手牌**；该牌由 C 放入 discardPile（弃牌堆），或由实现约定（如返回 bool 表示是否成功加入手牌，调用方决定溢出牌去向）。  
- 出牌、弃牌、消耗会减少手牌，不会因上限产生冲突。

**（3）效果函数注册表（C 维护）**

- C 维护「**CardId → 效果函数**」的注册表（是否升级已体现在 id 中，如 strike+ 对应升级效果）。
- 每张卡对应一个效果函数；效果函数签名由 B/C 约定，例如接收「效果上下文」（目标怪物下标、玩家/怪物状态引用等），在函数内完成伤害、格挡等结算。
- 注册表在 C 初始化时填充（效果函数实现可放在 C 内或独立 CardEffects 模块，由 C 注册）。E 不参与效果逻辑。

**（4）CardSystem 状态**

- 一个类或命名空间内持有：`drawPile`、`hand`、`discardPile`、**exhaustPile**、效果函数注册表。
- 依赖注入：**获取卡牌数据**通过「可调用对象」从 E 获取，例如 `std::function<const CardData*(CardId)> get_card_by_id`（id 已含升级信息，如 strike+），便于 Mock 与联调。

### 3.2 C 对外接口（函数规格）

以下为 C 模块**必须实现**的对外 API。入参/出参类型为逻辑描述，实现时用 C++ 类型替换（如 `const std::vector<CardId>&`）。


| 接口                                 | 签名（逻辑）                                                        | 行为                                                                                                                                                                                |
| ---------------------------------- | ------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| init_deck（初始化牌组）                   | `void init_deck(const std::vector<CardId>& initial_card_ids)` | 战斗开始时调用：按 initial_card_ids 为每张牌生成 CardInstance，分配 InstanceId，以**随机顺序**放入 drawPile（抽牌堆）；清空 hand、discardPile、exhaustPile。即「牌组中所有卡牌以随机顺序放入抽牌堆」。**固有**词条在首次抽牌时由 B 与 C 配合保证。           |
| draw_cards（抽牌）                     | `void draw_cards(int n)`                                      | **抽牌** = 将抽牌堆**顶部**的若干张牌依次移动到手牌。从 drawPile 顶部取最多 n 张加入 hand；手牌不超过上限（默认 10），超过则只抽到满上限。若抽牌堆空：先将 discardPile **以随机顺序**移入 drawPile（shuffle_discard_into_draw），再抽；若此时弃牌堆也空，抽牌效果无效（不抽）。 |
| get_hand（获取手牌）                     | `const std::vector<CardInstance>& get_hand() const`           | 返回当前 hand（手牌）的只读引用（或返回拷贝，由实现约定）。                                                                                                                                                  |
| remove_from_hand（从手牌移除）            | `CardInstance remove_from_hand(int hand_index)`               | 从 hand 中移除下标为 hand_index 的牌并返回该牌。调用方负责后续去向：**弃牌堆**（add_to_discard）、**消耗堆**（add_to_exhaust）、或**不加入任何牌堆**（能力牌打出后从本场战斗移除）。**弃牌**（不打出而放入弃牌堆）也通过 remove_from_hand + add_to_discard 实现。 |
| add_to_hand（加入手牌）                  | `void add_to_hand(CardInstance card)`                         | 将 card（牌实例）**直接加入 hand（手牌）**，用于战斗中临时生成牌入手牌。若 hand 已达**手牌上限（默认 10）**，则不改 hand，将 card 放入 discardPile（弃牌堆）；若 card 尚未分配 instanceId，由 C 接续已分配 id 分配新实例（保证不重复）。                         |
| add_to_discard（加入弃牌堆）              | `void add_to_discard(CardInstance card)`                      | 将 card（牌实例）追加到 discardPile（弃牌堆）。**可用于出牌后弃牌，也可用于战斗中临时生成牌放入弃牌堆**（如「将一张伤口加入弃牌堆」）；若 card 尚未分配 instanceId，由 C 接续已分配 id 分配新实例（保证不重复）。                                                   |
| add_to_exhaust（加入消耗牌堆）             | `void add_to_exhaust(CardInstance card)`                      | 将 card（牌实例）追加到 exhaustPile（消耗牌堆）；本场战斗内不再参与抽牌与洗牌。**战斗中临时生成牌放入消耗牌堆**时也可使用；若 card 尚未分配 instanceId，由 C 接续已分配 id 分配新实例（保证不重复）。                                                         |
| shuffle_discard_into_draw（洗弃牌入抽牌堆） | `void shuffle_discard_into_draw()`                            | 将 discardPile（弃牌堆）中所有牌移入 drawPile（抽牌堆）并打乱 drawPile 顺序；清空 discardPile。exhaustPile（消耗牌堆）不参与洗牌。洗牌算法自选（如 Fisher-Yates）。                                                               |
| add_to_deck（加入牌组）                  | `void add_to_deck(CardInstance card)`                         | 将一张牌加入牌组；若 card（牌实例）尚未分配 instanceId，由 C 接续已分配 id 分配新实例（保证不重复）。加入 drawPile（抽牌堆）末尾（或按约定）。**战斗中临时生成牌入抽牌堆**时同样使用本接口（如「将一张 X 洗入抽牌堆」）。                                                  |
| remove_from_deck（从牌组移除）            | `bool remove_from_deck(InstanceId instance_id)`               | 从整副牌（drawPile 抽牌堆 / hand 手牌 / discardPile 弃牌堆；**不含 exhaustPile 消耗牌堆**，消耗牌已移出本场循环）中移除指定 instance_id（实例 id）对应的实例，返回是否成功。商店删牌等由调用方指定要删哪一张。                                           |
| get_deck_size（抽牌堆张数）               | `int get_deck_size() const`                                   | 返回当前 drawPile（抽牌堆）的牌数。                                                                                                                                                            |
| get_discard_size（弃牌堆张数）            | `int get_discard_size() const`                                | 返回当前 discardPile（弃牌堆）的牌数。                                                                                                                                                         |
| get_exhaust_size（消耗牌堆张数）           | `int get_exhaust_size() const`                                | 返回当前 exhaustPile（消耗牌堆）的牌数（可选，供 UI 或调试）。                                                                                                                                           |
| upgrade_card_in_deck（升级牌组中某张牌）     | `bool upgrade_card_in_deck(InstanceId instance_id)`           | 将指定 instance_id（实例 id）对应实例的卡牌 id（模板 id）改为升级版（如 "strike" → "strike+"）；效果与词条以 E 的升级版 CardData（卡牌数据）为准。返回是否成功。                                                                       |
| execute_effect（执行卡牌效果）             | `void execute_effect(CardId id, EffectContext& ctx)`          | 根据 id（卡牌模板 id）查找注册表中的效果函数并调用，传入 ctx（效果上下文）；B 在 play_card（出牌）时构造 ctx 并调用本接口执行该卡效果。无对应注册时行为由实现约定。                                                                                   |


**EffectContext**（由 B 定义或 B/C 共用头文件）：包含效果执行所需信息，如目标怪物下标、当前玩家状态指针、怪物列表指针、**施加/查询增益减益的接口**（调用 B 的 apply_status_to_player、apply_status_to_monster 及读取当前玩家/怪物状态列表）等，供效果函数完成伤害/格挡/上状态等结算。具体字段由 B/C 约定。

**战斗中临时添加卡牌实例**  
效果或 B 需要「本场战斗内新生成一张牌」时（如「获得一张打击入手牌」「将一张伤口洗入抽牌堆」）：  

1. 构造 CardInstance，仅填 id（卡牌模板 id，如 "strike" / "wound"）；instanceId 可不填或填 0 表示「需由 C 分配新实例」。
2. 根据效果目标调用 C 的 **add_to_hand**（入手牌）、**add_to_deck**（入抽牌堆）、**add_to_discard**（入弃牌堆）或 **add_to_exhaust**（入消耗牌堆）。
3. C 在收到尚未分配 instanceId 的牌时，**接续前面已分配的 InstanceId 分配新值**（如自增），保证不重复，再将牌加入对应牌堆。新生成牌与初始牌组共用同一套编号空间。

这样同一套接口既支持「从手牌移到弃牌/消耗」，也支持「战斗中临时生成并放入任意牌堆」。

**与规则条文对照**  
以下与「抽牌堆、手牌、弃牌堆、消耗堆」规则一致：  

- 一场战斗中卡牌只处于四者之一：抽牌堆、手牌、弃牌堆、消耗堆。  
- 战斗开始时，牌组中所有卡牌以随机顺序放入抽牌堆（init_deck 后 drawPile 为随机顺序）。  
- 抽牌 = 将抽牌堆顶部的若干张牌依次移动到手牌；回合开始时抽五张（end_turn 下一回合调 draw_cards(5)）。  
- 一般情况下只能打出手牌中的牌（play_card(hand_index)）。  
- 打出后通常放入弃牌堆；**消耗**牌放入消耗堆；**能力**牌不进入任何区域（本场战斗内移除）。  
- 回合结束时，非保留手牌放入弃牌堆，虚无牌放入消耗堆，保留牌留在手牌。  
- 弃牌 = 手牌中的牌放入弃牌堆而不打出（remove_from_hand + add_to_discard）。  
- 抽牌时若抽牌堆空，弃牌堆以随机顺序放入抽牌堆再抽；若弃牌堆也空，抽牌无效。  
- 效果令某牌被消耗时，该牌进入消耗堆（add_to_exhaust）。

### 3.3 C 依赖 E 的接口（C 调用）


| 接口                          | 约定签名（逻辑）                                                                                          | 说明                                                                                                                    |
| --------------------------- | ------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| get_card_by_id（按 id 获取卡牌数据） | `const CardData* get_card_by_id(CardId id) const` 或通过注入的 `std::function<const CardData*(CardId)>` | C 在 init_deck 或需要卡牌属性（含费用、词条）时调用；id 已区分未升级/已升级（如 strike 与 strike+）。**E 不提供效果**，仅提供基础属性与词条。E 未就绪时由 Mock 返回固定 CardData。 |


---

## 四、B - 战斗引擎：数据结构与接口

### 4.1 需实现的数据结构

**（1）玩家战斗状态**

对应界面中的**玩家信息**：名字、角色、生命值、能量等。B 在单场战斗中维护或从主流程接收以下状态（供 get_battle_state 暴露给 UI）。

```cpp
struct PlayerBattleState {
    std::string playerName;   // 玩家名（创建存档时的名字）
    std::string character;    // 当前使用的角色
    int currentHp;            // 生命值（HP）；若降至 0 或以下则玩家死亡，对局失败并进入分数结算
    int maxHp;                // 最大血量（开局设定）
    int block;                // 当前格挡值（每回合开始或结束按规则可清零）
    int energy;               // 当前能量（打出卡牌消耗能量，每回合开始重置）
    int maxEnergy;            // 本回合最大能量（如 3）
    Stance stance;            // 姿态（枚举），影响造成/受到的伤害等，由卡牌或效果切换
    int orbSlotCount;         // 充能槽位数量（可由角色或遗物增加），当前最多可容纳的球数
    std::vector<OrbSlot> orbSlots;  // 充能球栏位：当前各槽位中的充能球列表，orbSlots.size() <= orbSlotCount
    int gold;                 // 金币（当前持有，战斗内通常不变）
    std::vector<PotionId> potions;  // 药水栏位：当前持有的药水列表（或固定槽位），见下（7）
    // 遗物栏：由 B 在 start_battle 时接收只读列表或通过注入查询，见下（7）
};
```

**生命值**：currentHp 降至 0 或以下时，玩家死亡，**终止对局并进行分数结算**，本局判定为失败（is_battle_over 返回 true 且我方败）。

**姿态（stance）**：类型为**枚举 Stance**（如 Neutral / Calm / Wrath / Divinity，具体取值由实现定）。由卡牌或效果切换；不同姿态可改变造成/受到的伤害（如愤怒姿态下造成与受到伤害加倍等），结算时由 B 或 EffectContext 参与计算。

**充能槽位数量（orbSlotCount）**：玩家当前拥有的充能槽位个数，可由角色初始值或遗物/卡牌效果增加；channel 新球时不得超过该数量（已满则按规则挤出最左球等，由实现定）。

**充能球栏位（orbSlots）**：当前各槽位中的充能球列表，`orbSlots.size()` ≤ `orbSlotCount`；每个槽位一颗球（类型如闪电/冰霜/黑暗，含层数或触发值）。卡牌可「 channel 」新球入槽、触发「 evoke 」、增加 **orbSlotCount** 等。B 维护 orbSlotCount 与 orbSlots，get_battle_state 返回供 UI 展示。

- 示例：`OrbSlot` 可含 `OrbId type`、`int stacks`；B 可提供 `channel_orb(OrbId, int)`、`evoke_orb(int slot_index)`、`add_orb_slot()`（使 orbSlotCount += 1）等接口供效果调用，细节由实现定。

**（2）场上怪物实例**

对应界面中的**敌人形象**与**意图**（位于敌人上方，悬停可查看意图详情）。B 维护每名敌人的状态与当前意图。

```cpp
struct MonsterInBattle {
    MonsterId id;           // 怪物 id，对应 E 的 MonsterData
    int       currentHp;
    int       maxHp;        // 来自 MonsterData
    // 意图：由 B 调用怪物行为函数时写入，供 UI 在敌人上方展示；悬停可显示意图详细信息
    std::string currentIntent;  // 或 Intent 结构体（攻击/防御等 + 数值）
};
```

**（3）怪物行为函数注册表（B 维护）**

- B 维护「**怪物 id → 行为函数**」的注册表（与 C 的卡牌效果注册表同理），每只怪对应一个函数。
- 行为函数签名由 B 约定，例如接收「怪物自身状态、战斗上下文」（玩家状态、怪物列表等），在函数内决定本回合意图（攻击/防御等）并执行：对玩家造成伤害、为己方获得格挡等；可选地写入当前意图供 UI 展示。
- 在 **end_turn** 敌方行动阶段，B 按队列对每个怪物根据 id 查找并调用对应行为函数，不再依赖 E 的 intentPattern / attackDamage / blockAmount。注册表在 B 初始化时填充（实现可放在 B 内或独立 MonsterBehaviours 模块）。E 不参与怪物行为逻辑。

**（4）课设数据结构落点**

- **栈（Stack）**：回合内「本回合打出的牌」或「效果结算顺序」。例如 `std::stack<CardPlayRecord>`，每张 play_card 将一次出牌记录入栈，回合末或结算时按栈顺序处理（或仅用于日志/报告演示）。
- **队列（Queue）**：  
  - 敌方**行动顺序**：`std::queue<MonsterInBattle*>` 或按怪物列表顺序依次行动。  
  - **战斗日志**：`std::queue<std::string>` 或 `std::queue<LogEntry>`，先发生先入队、先输出。

**（5）战斗状态总览**

- 当前层怪物列表：`std::vector<MonsterInBattle>`（或类似）。
- 玩家状态：`PlayerBattleState`（含玩家名、角色、HP、金币、药水、能量、**姿态**、**充能槽位数量**、**充能球栏位**等）。
- **怪物行为函数注册表**（MonsterId → 行为函数）。
- 当前回合数、是否玩家回合等（按规则需要则增加）。
- 对手牌与牌堆的访问：通过**调用 C 的接口**，B 不直接持有牌堆容器。
- 可选：**层数与进阶等级**（由主流程传入或 B 持有），供 UI 显示当前层（左）与进阶等级（右）；未开启进阶时不显示进阶等级。
- 可选：**遗物栏**（已获得遗物列表）、**增益/减益栏**（见下（6）），B 维护，供 UI 在角色与敌人下方展示；悬停可查看详情。

**（6）增益与减益**

战斗中获得的**增益**（如力量 + 伤害）或被施加的**减益**（如虚弱、易伤）由 **B 统一维护**，卡牌/怪物/药水效果通过 B 的接口施加或查询，结算时参与伤害、格挡等计算。

**数据结构**  

- 每个状态（增益或减益）可表示为：**状态 id**（如 "strength" / "vulnerable" / "weak"）、**层数 stacks**（可叠加）、**剩余回合数 duration**（每回合结束时减 1，为 0 时移除；-1 或特殊值表示持续到战斗结束或直到被清除）。  
- **玩家**持有一份增益/减益列表（或合并为一份「状态列表」）；**每个 MonsterInBattle** 持有一份（敌人可被上易伤、虚弱等）。  
- 实现可用 `std::vector<StatusEffect>` 或 `std::map<StatusId, int>`（只存层数，duration 另表或按 id 约定）。

```cpp
// 示例：单条状态
struct StatusInstance {
    StatusId id;     // 状态 id，如 "strength"/"vulnerable"/"weak"
    int      stacks; // 层数
    int      duration; // 剩余回合数，-1 表示永久或至战斗结束
};
// 玩家与每名怪物各持有一份 std::vector<StatusInstance> 或等价结构
```

**施加与移除**  

- B 提供**施加状态**的接口，供卡牌效果、怪物行为、药水等调用（通过 EffectContext 或直接调 B）：  
  - 例如 `apply_status_to_player(StatusId id, int stacks, int duration)`、`apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration)`；  
  - 若已存在同 id 状态则叠加层数或刷新 duration，由实现约定。
- **移除/清除**：B 可提供 `remove_status_from_player(StatusId id)`、`remove_status_from_monster(monster_index, StatusId id)` 或仅在回合末按 duration 自动移除。

**参与结算**  

- 伤害、格挡等计算时需考虑当前状态（如力量加伤害、易伤乘伤害、虚弱减攻击伤害）。  
- 方式一：B 提供 **get_effective_damage(baseDamage, source_is_player, target_monster_index)**，内部根据双方状态计算最终伤害。  
- 方式二：EffectContext 中提供**当前玩家与目标怪物的状态列表**（只读），效果函数自行按规则计算伤害/格挡后，再调用 B 的 **deal_damage_to_monster** / **deal_damage_to_player** / **add_block_to_player** 等写接口，由 B 统一扣血、加格挡。  
- 具体采用哪种由实现定，文档约定「B 负责持有状态，并在结算时参与计算或向效果方暴露状态供其计算」。

**回合末处理**  

- 在 **end_turn** 中，敌方行动结束后（或回合开始时）对**玩家**与**所有怪物**身上的状态做 **duration 递减**；duration 变为 0 的条目移除。若有「回合开始时」触发的状态，可在下一回合开始时处理。

**EffectContext 与增益/减益**  

- EffectContext 中需能**施加状态**（调用 B 的 apply_status_to_player / apply_status_to_monster）及**查询当前状态**（读玩家/指定怪物的状态列表），以便卡牌效果、药水效果中实现「获得 X 层力量」「对目标施加易伤」等。

**UI**  

- get_battle_state 返回的 BattleStateSnapshot 中包含**玩家增益/减益列表**与**每个怪物的增益/减益列表**，供界面在角色与敌人下方展示，悬停显示详情（名称、层数、剩余回合）。

**（7）药水与遗物**

**药水**  
- **含义**：可储存、**使用时产生一次性效果**的资源（如回血、造成伤害、获得格挡等）。  
- **数据结构**：B 维护**药水栏位**，即当前持有的药水列表；可用 `std::vector<PotionId>`（已放在 PlayerBattleState.potions）或固定数量槽位。`PotionId` 为药水类型 id（与 E 或数据表约定）。  
- **归属**：进入战斗时由主流程将当前药水列表通过 **PlayerBattleState.potions** 传入 B；战斗内 B 持有该列表，**使用后从列表中移除**该瓶药水。  
- **使用**：B 提供 **use_potion(slot_index)**。调用时执行该药水的**一次性效果**（回血、上状态、造成伤害等），执行方式可与卡牌效果类似：B 维护「PotionId → 药水效果函数」注册表，或通过 EffectContext 调用；执行完毕后从药水栏位移除该瓶。  
- **UI**：BattleStateSnapshot 中包含当前 **potions** 列表，供界面在金币右方展示药水栏位。

**遗物**  
- **含义**：跨战斗持久的被动或主动物品，提供被动效果（如每回合开始回血、初始能量+1）或在满足条件时触发；战斗内为**只读**，不在此局内获得或失去。  
- **数据来源**：遗物由**主流程/存档**管理。进入战斗时主流程将**当前已获得遗物列表**（只读）传入 B，在 **start_battle(..., const std::vector<RelicId>& relic_ids)** 中传入；也可通过注入 `get_relics()` 由 B 在需要时查询。B 不修改遗物列表，只读当前列表用于结算与展示。  
- **战斗内参与**：  
  - **被动效果**：如「每回合开始回复 2 血」「战斗开始时获得 1 能量」→ B 在 end_turn 下一回合开始、或 start_battle 时遍历遗物列表，根据 RelicId 调用对应遗物效果（或查表执行）。  
  - **触发式效果**：如「打出攻击牌时造成 3 点伤害」→ 在 play_card 或效果函数中由 B 查询当前遗物并执行。实现可维护「RelicId → 遗物效果/钩子」注册表。  
- **UI**：BattleStateSnapshot 中包含当前**遗物列表**（只读，如 `std::vector<RelicId> relics`），供界面在信息栏下方展示遗物栏；悬停可查看遗物详情。

**小结**  
- 药水：B 维护药水栏位（PlayerBattleState.potions），提供 **use_potion(slot_index)**；药水效果通过 B 内「PotionId → 效果函数」或 EffectContext 执行，使用后移除该瓶。  
- 遗物：B **不拥有**遗物，仅在 start_battle 时接收 **relic_ids** 或通过注入只读当前遗物列表；战斗内根据 RelicId 在相应时机（回合开始、出牌时等）执行遗物被动/触发效果；Snapshot 中返回 **relics** 列表供 UI 展示遗物栏。

### 4.2 B 对外接口（函数规格）


| 接口                               | 签名（逻辑）                                                                                                                                          | 行为                                                                                                                                                                                                                                                                   |
| -------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| start_battle（开始战斗）               | `void start_battle(const std::vector<MonsterId>& monster_ids, const PlayerBattleState& player_state, const std::vector<CardId>& deck_card_ids, const std::vector<RelicId>& relic_ids)` | 初始化战斗：根据 monster_ids 从 E 取 MonsterData，填充 MonsterInBattle 列表；设置玩家状态（含药水栏位 potions）；调用 C 的 init_deck(deck_card_ids)；保存 relic_ids 为只读遗物列表，用于本场战斗内遗物效果与 Snapshot 展示。首回合抽牌时若有**固有**词条，需保证固有牌在初始手牌中（实现上与 C 约定）。                                                                                                  |
| get_battle_state（获取战斗状态）         | `BattleStateSnapshot get_battle_state() const`                                                                                                  | 返回当前局面快照：玩家血量/格挡/能量、怪物列表（id、当前血量、当前意图等；意图由怪物行为函数写入或按需调用「取意图」）、当前手牌（通过 C 的 get_hand）。BattleStateSnapshot 结构体由 B 定义。                                                                                                                                                   |
| play_card（出牌）                    | `bool play_card(int hand_index, int target_monster_index)`                                                                                      | 若该手牌**不能被打出**（E 的 unplayable），直接返回 false。否则扣减能量（通过 E 查 cost）；构造 EffectContext，调用 **C 的 execute_effect(card.id, ctx)** 执行效果；调用 C 的 remove_from_hand(hand_index)。打出后：若该牌**消耗**则调用 C 的 **add_to_exhaust**；若为**能力**则不移入任何牌堆（本场战斗内从游戏中移除）；否则调用 C 的 add_to_discard。返回是否成功。 |
| end_turn（结束回合）                   | `void end_turn()`                                                                                                                               | 对应「结束回合」按钮：**手牌中所有牌将被置入弃牌堆**（保留/虚无按规则）。按**队列**顺序执行敌方行动。敌方行动后：对玩家与所有怪物身上的**增益/减益**做 **duration 递减**，duration 为 0 的移除；清空或按规则更新 block；玩家下一回合开始：调用 C 的 draw_cards(5)、重置 energy。                                                                                          |
| is_battle_over（战斗是否结束）           | `bool is_battle_over() const`                                                                                                                   | 若所有怪物 currentHp<=0 返回 true（胜利）；若玩家 currentHp<=0 返回 true（**玩家死亡，对局失败**，终止对局并进入分数结算）；否则 false。                                                                                                                                                                         |
| get_reward_cards（获取奖励卡牌列表）       | `std::vector<CardId> get_reward_cards(int count)`                                                                                               | 战斗胜利后调用。返回 count 张可选奖励卡牌的 id 列表；可调用 E 的 sort_cards_by_rarity 后随机或按规则选取。E 未就绪时返回固定列表。                                                                                                                                                                                 |
| apply_status_to_player（对玩家施加状态）  | `void apply_status_to_player(StatusId id, int stacks, int duration)`                                                                            | 对玩家施加增益或减益；若已有同 id 则叠加层数或刷新 duration（由实现约定）。duration 为 -1 表示持续到战斗结束或直到被清除。卡牌/药水效果通过 EffectContext 调用。                                                                                                                                                                |
| apply_status_to_monster（对怪物施加状态） | `void apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration)`                                                        | 对指定怪物施加状态（如易伤、虚弱）；若已有同 id 则叠加或刷新。卡牌效果通过 EffectContext 调用。                                                                                                                                                                                                            |
| use_potion（使用药水）                   | `bool use_potion(int slot_index)`                                                                                                               | 使用药水栏位中 slot_index 位置的一瓶药水：执行该药水的一次性效果（通过 B 内注册的药水效果函数或 EffectContext），执行成功后从药水列表移除该瓶，返回 true；若 slot_index 无效或该格无药水则返回 false。                                                                                                                              |


**BattleStateSnapshot** 建议字段（供 UI 展示战斗界面）：  

- **玩家信息**：playerName, character, currentHp, maxHp, block, gold, energy, maxEnergy；**stance（姿态）**、**orbSlotCount（充能槽位数量）**、**orbSlots（充能球栏位）**；**potions（药水栏位）**、**relics（遗物栏，只读列表）**、增益/减益栏。  
- **牌堆与手牌**：hand（来自 C 的 get_hand）、drawPile 张数、discardPile 张数、exhaustPile 张数（来自 C 的 get_deck_size 等）；抽牌堆/弃牌堆/消耗堆的详细列表可由 C 提供或按需查询。  
- **敌人**：monsters（id、name、currentHp、maxHp、当前意图等）。  
- **回合与层数**：当前回合数、层数、进阶等级（若适用）。

**战斗界面与状态对照**  
以下与界面/规则条文对应，B 需暴露或维护的状态由 get_battle_state 返回，供 UI 展示：  

- **玩家信息**：玩家名、当前角色 → PlayerBattleState.playerName, character。  
- **生命值（HP）**：当前血量；降至 0 或以下则玩家死亡、对局失败、分数结算 → currentHp, maxHp；is_battle_over 为 true 时判定失败。  
- **金币**：当前持有 → gold（战斗内通常不变）。  
- **药水栏位**：当前持有药水；药水可储存，**使用时产生一次性效果** → B 维护药水列表（PlayerBattleState.potions），使用药水调用 **use_potion(slot_index)**，效果结算与卡牌类似（见 4.1（7））。  
- **能量**：当前能量，出牌消耗 → energy, maxEnergy。  
- **姿态**：当前姿态，影响造成/受到的伤害等 → stance；由卡牌或效果切换。  
- **充能槽位数量**：当前最多可容纳的球数 → orbSlotCount；可由遗物/卡牌增加（如 add_orb_slot）。
- **充能球栏位**：当前各槽位中的充能球（如闪电/冰霜/黑暗球）→ orbSlots（size ≤ orbSlotCount）；B 维护，可提供 channel_orb、evoke_orb 等接口供效果调用。  
- **抽牌堆 / 手牌 / 弃牌堆 / 消耗堆**：由 C 管理；B 通过 C 的 get_hand、get_deck_size、get_discard_size、get_exhaust_size 等填入 BattleStateSnapshot。消耗堆在「消耗了至少一张牌后」才在 UI 显示，可由 exhaustPile 张数 > 0 判断。  
- **结束回合**：按下后手牌全部置入弃牌堆（保留/虚无按规则）→ end_turn()。  
- **层数与进阶等级**：当前层（左）、进阶等级（右）；未开启进阶则不显示 → 可由主流程传入 B 或 B 持有，Snapshot 中返回。  
- **遗物栏**：已获得遗物 → 由主流程在 start_battle 时传入遗物列表（只读），B 用于被动/触发效果与 Snapshot 展示（见 4.1（7））。  
- **增益/减益栏**：战斗中获得的增益或被施加的减益 → 由 **B 维护**（见 4.1（6））；通过 apply_status_to_player / apply_status_to_monster 施加；Snapshot 中返回玩家与每个怪物的状态列表，悬停详情由 UI 根据数据展示。  
- **意图**：敌人攻击意图，位于敌人上方 → MonsterInBattle.currentIntent。  
- 版本号、种子号、地图/牌组/设置等由主流程或 UI 管理，不纳入 B 的 BattleStateSnapshot。

### 4.3 B 依赖 C 的接口（B 调用）

B 在实现中调用 C 的以下接口（签名与第三节一致）：

- `init_deck(initial_card_ids)`
- `draw_cards(n)`
- `get_hand()`
- `remove_from_hand(hand_index)`
- `add_to_discard(card)`、`add_to_exhaust(card)`（消耗词条牌打出后）、`add_to_hand(card)`（战斗中临时生成牌入手牌）
- `shuffle_discard_into_draw()`（若 C 在 draw_cards 内部已处理抽空则可能不显式调用）
- `**execute_effect(card_id, effect_context)`**（出牌时执行该卡效果）
- `get_deck_size()`, `get_discard_size()`, `get_exhaust_size()`（可选，用于 UI 或调试）

### 4.4 B 依赖 E 的接口（B 调用）


| 接口                             | 约定签名（逻辑）                                                         | 说明                                                                                                                        |
| ------------------------------ | ---------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| get_monster_by_id（按 id 获取怪物数据） | `const MonsterData* get_monster_by_id(MonsterId id) const` 或通过注入 | B 在 start_battle 时取怪物静态数据（id、name、type、maxHp）。**E 不提供意图、攻击伤害、格挡量**；敌方行动由 B 调用自身维护的怪物行为函数执行。E 未就绪时由 Mock 返回固定 MonsterData。 |
| get_card_by_id（按 id 获取卡牌数据）    | 用于 play_card 时查费用与词条（unplayable、exhaust 等）                       | B 查 E 取得费用与词条；**效果不查 E**，由 B 调 C 的 execute_effect 执行。                                                                     |
| sort_cards_by_rarity（按稀有度排序卡牌） | 可选，用于 get_reward_cards                                           | E 提供排序后的卡牌 id 列表或可迭代结构，B 取前 N 张或随机。                                                                                       |


---

## 五、文件与符号归属


| 内容                                                                              | 放置位置                                                                   |
| ------------------------------------------------------------------------------- | ---------------------------------------------------------------------- |
| C：CardInstance、牌堆类型、CardSystem 类或命名空间声明                                         | `include/CardSystem/CardSystem.hpp`（或 Types.hpp + CardSystem.hpp）      |
| C：上述接口实现                                                                        | `src/CardSystem/CardSystem.cpp`                                        |
| B：PlayerBattleState、MonsterInBattle、BattleStateSnapshot、栈/队列类型、BattleEngine 类声明 | `include/BattleEngine/BattleEngine.hpp`                                |
| B：上述接口实现                                                                        | `src/BattleEngine/BattleEngine.cpp`                                    |
| 公共类型 CardId、MonsterId                                                           | 可与 E 共用同一头文件（如 `include/Common/Types.hpp`），或分别在 C/B、E 中各自 using，保持一致即可 |
| E 提供的 CardData、MonsterData                                                      | 由 E 在 `include/DataLayer/` 下定义；C/B 仅包含其头文件或前向声明 + 约定字段                 |


---

## 六、小结：实现清单

**C - CardSystem**

- 数据结构：`CardInstance`（InstanceId + CardId）；**drawPile / hand / discardPile / exhaustPile**（四个牌堆，线性表或链表）；**效果函数注册表**（CardId → 效果函数）。
- 接口：init_deck, draw_cards, get_hand, remove_from_hand，**add_to_hand**（战斗中临时加入手牌）, add_to_discard, add_to_exhaust，shuffle_discard_into_draw, add_to_deck, remove_from_deck(instance_id)，get_deck_size, get_discard_size, get_exhaust_size，upgrade_card_in_deck(instance_id)；execute_effect(card_id, effect_context)。
- 依赖：E 的 get_card_by_id（仅基础属性与词条，E 不提供效果）；效果由 C 内注册的每卡一函数实现。

**B - BattleEngine**

- 数据结构：`PlayerBattleState`、`MonsterInBattle`、`BattleStateSnapshot`；**怪物行为函数注册表**（MonsterId → 行为函数）；**玩家与怪物的增益/减益列表**（StatusInstance：id、stacks、duration）；栈（出牌/结算）、队列（行动顺序/日志）。
- 接口：start_battle, get_battle_state, play_card, end_turn, is_battle_over, get_reward_cards；**apply_status_to_player**、**apply_status_to_monster**（施加增益/减益，供效果与 EffectContext 调用）。
- 依赖：C 的上述接口（含 execute_effect）；E 的 get_monster_by_id（仅静态数据）、get_card_by_id、sort_cards_by_rarity。**怪物意图与行动**由 B 内注册的每怪一函数实现；**增益/减益**由 B 维护，回合末做 duration 递减并移除为 0 的项。

以上为 B、C 模块的完整设计与接口定稿，实现时以本文档为准。