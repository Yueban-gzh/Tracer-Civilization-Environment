## 存档系统设计草案（v0）

### 1. 目标与基本原则

- **目标**：支持“整局 Run 级别”的存档与读档，保证在同一逻辑路径下读档后可以复现：
  - 地图进度与当前所在节点
  - 玩家数据（血量/牌组/遗物/药水/金币等）
  - 当前战斗或事件/商店/休息房间的状态
  - 伪随机序列（抽牌顺序、事件结果、奖励掉落等）
- **原则**：
  - 存档格式尽量可读（JSON），方便调试。
  - 存档结构中不直接存 UI 相关的临时状态（例如飘字、动画进度），读档后由逻辑层重建 UI 快照。
  - 存档与逻辑版本号分离，未来做兼容时可以做简单迁移。

### 2. 存档触发时机（第一版）

第一版先做**单一自动存档槽**，触发时机：

- 进入新地图节点（移动成功并锁定到新节点后）。
- 进入一场战斗之前（用于“读档重打这场战斗”）。
- 战斗结算完成、回到地图后（带上奖励后的新状态）。

后续可以扩展为：

- 多存档槽（`save_slot_1.json` / `save_slot_2.json`）。
- 手动存档 / 仅在安全房间（休息点、商店、宝箱前等）允许手动存档。

### 3. 存档文件位置与命名

- 目录：`saves/`（位于工程根目录下；不存在时自动创建）。
- 文件名（第一版）：`run_auto_save.json`
  - 后续如支持多槽，可扩展为：`run_slot_1.json`、`run_slot_2.json` 等。

### 4. 存档 JSON 顶层结构（草案）

```jsonc
{
  "schema_version": 1,
  "run_rng_state": "1234567890123456789",   // string 包一层，避免 JS 精度坑
  "map": {
    "seed": 12345,
    "current_floor": 1,
    "current_node_id": "floor1_node_3",
    "nodes_state": [
      // 每个节点的类型、是否已访问、是否已完成、是否可选等
    ]
  },
  "player": {
    // 与 PlayerBattleState 对齐的结构
  },
  "battle": {
    "in_battle": false,
    "state": { /* BattleState 序列化 */ }
  },
  "event": {
    "active": false,
    "content_id": "",
    "internal_state": {}
  },
  "shop": {
    "active": false,
    "goods": []
  },
  "rest": {
    "active": false,
    "options": []
  }
}
```

第一版中，**优先实现 `run_rng_state` / `player` / `map` / `battle` 四块**，`event` / `shop` / `rest` 可以先只记录“当前界面类型 + 内容 id”，必要时再拓展细节。

### 5. 关键结构序列化细节

#### 5.1 Run 级 RNG 状态

- 字段：`run_rng_state: string`
- 内容来自：`GameFlowController::get_run_rng_state()`（内部是 `RunRng::get_state()` 返回的 `uint64_t`）。
- 读档时，在构造完 `GameFlowController`、恢复完其它状态后，调用：
  - `gameFlow.set_run_rng_state(parsed_state);`
- 所有战斗 / 地图 / 事件的随机逻辑使用同一个 `RunRng`，保证读档后在同一逻辑路径下重现抽牌顺序/奖励/事件分支。

#### 5.2 玩家状态 `player`（对齐 PlayerBattleState）

这里不直接复刻 C++ 结构体的字段名，而是采用稳定的 JSON 结构，读档时再填回 `PlayerBattleState`：

```jsonc
"player": {
  "name": "测试角色",
  "character_id": "ironclad",
  "current_hp": 72,
  "max_hp": 80,
  "gold": 123,
  "max_energy": 3,
  "deck": [
    { "id": "strike", "upgraded": false },
    { "id": "bash+", "upgraded": true }
  ],
  "relics": ["burning_blood", "marble_bag"],
  "potions": ["block_potion"],
  "potion_slots": 3,
  "statuses": [
    { "id": "strength", "stacks": 2, "duration": -1 }
  ]
}
```

注意：

- 牌组使用**卡牌 ID + 是否升级**的轻量结构，而不是战斗内的 `CardInstance`。
- 遗物 / 药水只需要 ID 列表，效果由逻辑层根据 ID 解读。

#### 5.3 战斗状态 `battle`

```jsonc
"battle": {
  "in_battle": true,
  "node_type": "monster",         // 当前战斗类型（普通怪 / 精英 / Boss）
  "monster_ids": ["slime_small", "slime_small"],
  "state": {
    // BattleState 序列化：玩家战斗内状态 + 手牌/抽弃消耗堆 + 怪物数组 + 回合数 + 阶段等
  }
}
```

序列化要点：

- **玩家部分**：使用 `BattleState::player` 里的字段（当前 HP/格挡/能量/回合内已出牌数/战斗内状态列表）。
- **牌堆部分**：`drawPile` / `discardPile` / `exhaustPile` / `hand`，使用 `CardInstance` 的可序列字段：
  - `"id"`：卡牌 ID
  - `"upgraded"`：是否升级
  - `"instance_id"`：实例唯一 ID（用于各种“选择手牌”的效果）
  - `"combat_cost_zero"` / `"combatCostDiscount"`：本场战斗的临时减费/免费信息
- **怪物部分**：`monsters` 列表 + 每只怪物的：
  - `"id"`、`"max_hp"`、`"current_hp"`、`"current_intent"`、
  - `"statuses"`（`StatusInstance`）、
  - 以及必要的内部辅助字段（例如部分怪物需要记“本回合是否释放过某技能”等）。
- **阶段与回合数**：`turnNumber` + `phase`（可以用字符串，例如 `"PlayerTurn"`, `"MonstersTurn"`，避免枚举值直接写到 JSON）。

UI 相关：

- `pendingDamageDisplays` 可以不存档，读档后清空即可，由后续战斗 flow 再产生新的飘字事件。

### 6. 读档流程（高层流程草案）

1. 从 `saves/run_auto_save.json` 读取并解析 JSON。
2. 校验 `schema_version`，不兼容时给出提示或做简单迁移。
3. 初始化 `GameFlowController`，再依次恢复：
   - `run_rng_state` → `set_run_rng_state(...)`
   - `player` → 构造 `PlayerBattleState` 填回 `playerState_`
   - `map` → 重建 `MapEngine` 的当前楼层、节点状态、可选路径
   - 若 `battle.in_battle == true`：
     - 用 `battle.state` 重建 `BattleState`，注入到 `BattleEngine`。
     - 直接跳到战斗循环界面。
   - 否则根据当前节点类型跳转到：
     - 地图界面 / 事件 UI / 商店 UI / 休息 UI 等。

### 7. 后续扩展点

- **多存档槽**：在顶层增加 `slot_id` 或用多个文件。
- **校验与防篡改（可选）**：增加简单的 hash 或签名字段，防止意外修改导致崩溃。
- **压缩**：当 JSON 过大时，可以在最终写入前做 gzip 压缩（需要额外依赖，再评估）。

---

以上是第一版存档系统的设计草案，接下来可以按这个文档逐步实现：

1. 定义对应的序列化/反序列化函数（`to_json/from_json` 或手写）。
2. 在 `GameFlowController` 中增加 `saveRun()` / `loadRun()` 接口，并在合适节点调用。
3. 在实现过程中，如果发现与现有结构不匹配，再迭代本文档。

