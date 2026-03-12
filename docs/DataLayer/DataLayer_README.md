# 数据层（DataLayer）说明

> 数据表格式与对外接口的集中说明，与 [设计与接口.md](设计与接口.md) 第二节、第五节 E 对应。  
> 代码位置：`include/DataLayer/`、`src/DataLayer/`；数据位置：`data/`。

---

## 一、职责与位置

- **职责**：加载并缓存卡牌表、怪物表、事件表；按 id 提供 O(1) 查找；提供按稀有度排序（奖励展示）及排行榜排序。
- **静态数据原则**：E模块仅提供静态数据（id、name、type、maxHp等），不包含动态行为逻辑（如intentPattern、attackDamage、blockAmount、卡牌效果数值等）。动态行为由B/C模块通过ID调用对应的效果函数实现。
- **依赖**：无；被 BattleEngine(B)、CardSystem(C)、EventEngine(D) 及主流程调用。
- **课设落点**：哈希表查找、排序算法。

---

## 二、数据文件

### 路径与编码

| 文件 | 路径 | 编码 |
|------|------|------|
| 卡牌表 | `data/cards.json` | UTF-8 |
| 怪物表 | `data/monsters.json` | UTF-8 |
| 事件表 | `data/events.json` | UTF-8 |

加载时传入**基础目录**（如 `"."` 表示当前工作目录）：内部会拼成 `基础目录/data/xxx.json`。若传入的已是完整 `.json` 路径，则直接使用。**运行程序时请将工作目录设为项目根目录**，否则会找不到 `data/`。

### 卡牌表 `cards.json`

- **格式**：JSON 数组，每项为一张卡牌对象。
- **字段**（与 BC 模块设计文档一致，词条由 E 提供、效果由 C 的 id→效果函数实现）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| id | string | 是 | 唯一 id；已升级用另一条 id，如 `"card_001+"` |
| name | string | 是 | 卡牌名称 |
| cardType | string | 是 | 直接写枚举名：`"Attack"` / `"Skill"` / `"Power"` / `"Status"` / `"Curse"`（对应 攻击/技能/能力/状态/诅咒）；实现侧大小写不敏感（`"attack"` 等也能被解析），推荐仍按枚举名大小写书写 |
| cost | int | 是 | 消耗墨力（特殊值如 -1 表示 X，由 B/C 约定） |
| rarity | string | 是 | `"common"` / `"uncommon"` / `"rare"`（JSON中使用小写字符串，解析时映射到 `tce::Rarity` 枚举） |
| description | string | 是 | 展示用文学描述（不含数值效果，数值由C模块的效果函数实现） |
| exhaust | bool | 是 | 词条：消耗（打出后进消耗堆） |
| ethereal | bool | 是 | 词条：虚无（回合末未打出则进消耗堆） |
| innate | bool | 是 | 词条：固有（首回合必入手牌） |
| retain | bool | 是 | 词条：保留（回合末不弃掉） |
| unplayable | bool | 是 | 词条：不能被打出 |

- **示例一条**：

```json
{
  "id": "card_001",
  "name": "与子同袍",
  "cardType": "Attack",
  "cost": 1,
  "rarity": "common",
  "description": "吟诵《秦风·无衣》，与幻境中的同袍之志共鸣，挥出刚劲一击。",
  "exhaust": false,
  "ethereal": false,
  "innate": false,
  "retain": false,
  "unplayable": false
}
```

### 怪物表 `monsters.json`

- **格式**：JSON 数组，每项为一个怪物对象。
- **字段**（E模块仅提供静态数据，意图与行为由B的怪物行为函数决定）：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| id | string | 是 | 唯一 id，如 `"monster_001"`、`"monster_boss_1"` |
| name | string | 是 | 怪物名称 |
| type | string | 是 | `"normal"` / `"elite"` / `"boss"`（JSON中使用小写字符串，解析时映射到 `tce::MonsterType` 枚举） |
| maxHp | int | 是 | 最大生命值 |

- **示例一条**：

```json
{
  "id": "monster_001",
  "name": "残鼎之灵",
  "type": "normal",
  "maxHp": 28
}
```

### 事件表 `events.json`

- **格式**：JSON 数组，每项为一个事件对象；事件树通过**一条记录内嵌 options** 实现，选项里用 `next` 跳转子事件或 `result` 结束。
- **字段**：

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| id | string | 是 | 唯一 id |
| title | string | 是 | 事件标题 |
| description | string | 是 | 事件描述 |
| options | array | 是 | 选项列表 |

- **options 每项**：
  - `text` (string)：选项文案。
  - `next` (string)：有则表示跳转到对应 EventId 的子事件。
  - `result` (object)：有则表示该选项的结局，由 EventEngine 解析；含 `type`（如 `"gold"` / `"heal"` / `"card_reward"` / `"none"` / `"relic"` / `"max_hp"` / `"remove_card"`）和 `value` (int)。

- **示例**：一个选项跳转子事件，一个选项直接给结果。

```json
{
  "id": "event_001",
  "title": "拾遗卷",
  "description": "你在幻境中遇到一卷残破的古籍……",
  "options": [
    { "text": "小心拂去尘埃，尝试解读", "next": "event_001a" },
    { "text": "将残卷收好，留待日后", "result": { "type": "gold", "value": 15 } }
  ]
}
```

---

## 三、数据类型（C++）

- **卡牌/怪物**：定义在 `include/DataLayer/DataLayer.hpp` 命名空间 `tce` 下，与 B/C 共用。
- **事件与 id 类型**：定义在 `include/DataLayer/DataTypes.h` 命名空间 `DataLayer` 下。

| 类型 | 说明 |
|------|------|
| `tce::CardData` | id, name, cardType, cost, rarity, description, exhaust, ethereal, innate, retain, unplayable |
| `tce::MonsterData` | id, name, type (Normal/Elite/Boss), maxHp |
| `Event` | id, title, description, options (`vector<EventOption>`) |
| `EventOption` | text, next, result (`EventResult`) |
| `EventResult` | type, value |
| `CardId` / `MonsterId` / `EventId` | 均为 `std::string`（DataLayer 中与 tce 中一致） |

---

## 四、对外接口（当前方法）

类名：`DataLayer::DataLayerImpl`，头文件：`include/DataLayer/DataLayer.h`。

### 加载

| 方法 | 说明 |
|------|------|
| `bool load_cards(const std::string& path_or_base_dir)` | 加载卡牌表；成功返回 true |
| `bool load_monsters(const std::string& path_or_base_dir)` | 加载怪物表 |
| `bool load_events(const std::string& path_or_base_dir)` | 加载事件表 |

建议在主流程或 GameMain 启动时各调一次；若传入 `"."`，则从当前工作目录下的 `data/` 读取对应 json。

### 按 id 查找（哈希表，O(1)）

| 方法 | 返回值 | 说明 |
|------|--------|------|
| `const tce::CardData* get_card_by_id(const CardId& id) const` | 指针或 nullptr | id 不存在时返回 nullptr |
| `const tce::MonsterData* get_monster_by_id(const MonsterId& id) const` | 同上 | |
| `const Event* get_event_by_id(const EventId& id) const` | 同上 | |

返回的是内部缓存的只读指针，调用方不要修改指向内容。

### 排序

| 方法 | 说明 |
|------|------|
| `std::vector<CardId> sort_cards_by_rarity(const std::vector<CardId>& card_ids) const` | 按稀有度升序：common < uncommon < rare；用于战斗奖励展示 |
| `std::vector<LeaderboardEntry> sort_leaderboard(std::vector<LeaderboardEntry> entries) const` | 按 score 降序；可选，用于排行榜 |

`LeaderboardEntry` 含 `playerId` (string)、`score` (int)、`time` (int)。

---

## 五、使用示例

```cpp
#include "DataLayer/DataLayer.h"

using namespace DataLayer;
DataLayerImpl data;

// 加载（工作目录为项目根）
if (!data.load_cards(".") || !data.load_monsters(".") || !data.load_events("."))
    return -1;

// 查找
const tce::CardData* c = data.get_card_by_id("card_001");
if (c) { /* c->name, c->cost, c->description ... */ }
const tce::MonsterData* m = data.get_monster_by_id("monster_boss_1");
const Event* e = data.get_event_by_id("event_001");

// 按稀有度排序卡牌 id
std::vector<CardId> ids = { "card_004", "card_001", "card_015" };
ids = data.sort_cards_by_rarity(ids);
```

---

## 六、实现要点

- **存储**：卡牌/怪物数据存放在命名空间 `tce` 下的 `s_cards`、`s_monsters`（`std::unordered_map`），由 `load_cards`/`load_monsters` 填充，供 DataLayer 与 B/C 共用；事件表存放在 `DataLayerImpl::events_`。按 id 查找平均 O(1)。
- **JSON**：自实现简易解析（`JsonParser.cpp`），无第三方库，仅支持当前 data 所用格式；解析时会自动跳过 UTF-8 BOM，并对 `cardType` / `rarity` 做大小写不敏感匹配（如 `"attack"` / `"rare"` 也能正常解析，遇到未知取值会在控制台输出告警并使用安全默认值）。
- **加载与校验**：`load_cards` / `load_monsters` / `load_events` 会在加载前清空旧数据，逐条校验：非对象条目会被忽略；缺少 `id` 的记录会被跳过并统计数量；同一 id 多次出现时保留**第一条**并忽略后续，均会在控制台输出 `[E][DataLayer] ...` 形式的调试信息。若某张表没有加载到任何有效记录则返回 `false`。
- **编码**：文件按 UTF-8 读写；Windows 控制台需在程序开头设置 `SetConsoleOutputCP(65001)` 才能正确显示中文。

---

## 七、注意事项

1. **工作目录**：运行 exe 时当前目录需为项目根（或包含 `data/` 的目录），否则 `load_*(".")` 会失败；在 VS 调试时可把“工作目录”设为 `$(ProjectDir)`。
2. **id 唯一性**：三张表内 id 不可重复；卡牌/怪物/事件 id 互不冲突即可，可同用 string 如 `"card_001"`、`"monster_boss_1"`。
3. **扩展数据**：新增卡牌/怪物/事件只需在对应 json 中追加条目并保证字段一致，无需改代码（除非增新字段）。
