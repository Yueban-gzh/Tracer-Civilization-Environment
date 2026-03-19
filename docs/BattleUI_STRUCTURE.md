# BattleUI.cpp 代码结构与实现逻辑

> 供进一步优化 UI 时参考

## 一、整体架构

```
主循环 (main.cpp)
    │
    ├─ engine.get_battle_state()        → BattleState
    ├─ make_snapshot_from_core_refactor() → BattleStateSnapshot
    ├─ ui.handleEvent(ev, mousePos)     → 处理点击、悬停、瞄准
    ├─ ui.draw(window, adapter)         → 绘制整帧
    ├─ engine.tick_damage_displays()    → 伤害数字计时
    └─ engine.step_turn_phase()         → 推进回合
```

**数据流**：`BattleState`（引擎）→ `BattleStateSnapshot`（快照）→ `IBattleUIDataProvider`（适配器）→ `BattleUI::draw()`

---

## 二、文件结构概览

### 1. 匿名命名空间（约 17~215 行）

- **布局常量**：`TOP_BAR_BG_H`、`RELICS_ROW_Y`、`BOTTOM_BAR_Y_RATIO`、`CARD_W`、`HP_BAR_W` 等
- **辅助函数**：
  - `get_relic_display_info(id)` / `get_potion_display_info(id)`：遗物/药水名称与描述
  - `card_targets_enemy(s, handIndex)`：判断牌是否需要敌人目标
  - `draw_wrapped_text(...)`：自动换行、省略号截断

### 2. 成员函数分组

| 函数 | 行号 | 作用 |
|------|------|------|
| 构造/加载 | 216~283 | `BattleUI`、`loadFont`、`loadMonsterTexture` 等 |
| 事件与状态 | 288~526 | `setMousePosition`、`handleEvent`、`show_center_tip` |
| 轮询接口 | 529~641 | `pollPlayCardRequest`、`pollPotionRequest`、`set_reward_data` 等 |
| **draw 主入口** | **643~703** | **`draw()`** |
| drawTopBar | 706~811 | 顶栏：钥匙槽、名字、HP、金币、药水槽 |
| drawRelicsRow | 813~837 | 遗物行 |
| drawRelicPotionTooltip | 839~907 | 遗物/药水悬停提示 |
| drawRewardScreen | 909~1059 | 奖励界面 |
| drawDeckView | 1062~1203 | 牌组界面 |
| **drawBattleCenter** | **1206~1863** | **战场：玩家、怪物、伤害数字** |
| **drawBottomBar** | **1866~2335** | **底栏：能量、手牌、结束回合、牌堆** |
| drawTopRight | 2337~ | 右上角按钮、回合数 |

---

## 三、draw() 主流程（643~703 行）

```
draw(window, data)
    │
    ├─ s = data.get_snapshot()
    ├─ lastSnapshot_ = &s                    // 供 handleEvent 使用
    │
    ├─ 背景图 + 顶栏背景色块
    ├─ drawTopBar() + drawRelicsRow()
    │
    ├─ if (deck_view_active_)   → drawDeckView + return
    ├─ if (reward_screen_active_) → drawBattleCenter + drawBottomBar + drawRewardScreen + return
    │
    └─ 正常战斗：
        ├─ drawBattleCenter()     // 战场中央
        ├─ drawRelicPotionTooltip()
        ├─ 手牌区背景色块
        ├─ drawBottomBar()        // 底栏
        ├─ drawTopRight()
        └─ draw_center_tip()      // 中央提示（如“能量不足”）
```

---

## 四、drawBattleCenter（1206~1863 行）

战场核心区域，按顺序绘制：

1. **玩家区**（约 1217~1380 行）
   - `playerModelRect_`：玩家模型矩形（用于点击检测）
   - 模型：`playerTextures_` 或灰色占位
   - 血条：HP 比例 + 格挡
   - 玩家状态：`s.playerStatuses`（金属化、易伤等）

2. **怪物区**（约 1380~1823 行）
   - `monsterModelRects_`、`monsterIntentRects_`：每只怪物的模型和意图矩形
   - 意图：攻击/防御/ buff 等
   - 模型：`monsterTextures_` 或灰色占位
   - 血条 + 怪物状态：`s.monsterStatuses`
   - 悬停状态：`hoveredMonsterIndex` → 状态图标 tooltip

3. **伤害数字**（约 1825~1862 行）
   - `s.pendingDamageDisplays`
   - 玩家受伤：左侧，向左下错位
   - 怪物受伤：右侧，向右下错位
   - 错开出现：`age = 180 - frames_remaining`，仅当 `age >= idx * 12` 时绘制

---

## 五、drawBottomBar（1866~2335 行）

底栏布局：**抽牌堆 | 能量 | 手牌（扇形） | 结束回合 | 弃牌堆 | 消耗堆**

### 手牌区逻辑（约 1916~2200 行）

- **扇形布局**：`getCardPos(idx, addHoverLift, cx, cy, angleDeg)` 计算每张牌中心
- **悬停/选中**：`hoverIndex`、`selectedHandIndex_`
- **选中状态**：
  - 需敌人目标：原位内跟随鼠标 → 离开后固定到屏幕中下方，进入瞄准
  - 自选目标：始终跟随鼠标，高亮玩家模型
- **drawOneCard**：单张牌绘制（名字、描述、费用、类型等）

### 关键矩形

- `endTurnButton_`：结束回合按钮
- `potionSlotRects_`：药水槽
- `relicSlotRects_`：遗物槽
- `playerModelRect_`：玩家模型（自选目标牌）
- `monsterModelRects_`：怪物模型（攻击牌目标）

---

## 六、handleEvent 与交互流程

1. **奖励界面**：卡牌选择、跳过、继续
2. **牌组界面**：滚轮滚动、返回按钮
3. **牌组/牌堆入口**：点击顶栏牌组、抽牌堆、弃牌堆、消耗堆
4. **左键**：
   - 药水瞄准 → 点击怪物确认
   - 牌瞄准 → 点击怪物确认
   - 自选目标牌 → 离开原位后点击玩家模型高亮区域
   - 药水槽 → 需目标则进入瞄准
   - 结束回合按钮 → 返回 `true`
5. **右键**：取消选中牌/药水

---

## 七、修改 UI 时的常见入口

| 修改目标 | 修改位置 | 说明 |
|----------|----------|------|
| 布局/尺寸 | 匿名命名空间常量 | `TOP_BAR_BG_H`、`CARD_W`、`HP_BAR_W` 等 |
| 顶栏内容 | `drawTopBar` | 约 706~811 行 |
| 遗物/药水悬停 | `drawRelicPotionTooltip` | 约 839~907 行 |
| 战场布局 | `drawBattleCenter` | 玩家区、怪物区、伤害数字 |
| 手牌样式 | `drawBottomBar` 内 `drawOneCard` | 约 1970~2200 行 |
| 牌组界面 | `drawDeckView` | 约 1062~1203 行 |
| 奖励界面 | `drawRewardScreen` | 约 909~1059 行 |
| 点击逻辑 | `handleEvent` | 约 339~526 行 |
| 悬停检测 | 各 draw 函数内 + `mousePos_` | 需在绘制时更新矩形 |

---

## 八、修改建议

1. **布局常量**：优先在匿名命名空间修改，避免魔法数字
2. **新增区域**：在 `draw()` 中按顺序插入新的 `drawXXX` 调用
3. **点击检测**：在对应 draw 中更新 `sf::FloatRect`，在 `handleEvent` 中检查 `contains(mousePos)`
4. **请求传递**：用 `pendingXXX` 变量 + `pollXXXRequest()` 与主循环解耦
5. **数据来源**：所有绘制数据来自 `BattleStateSnapshot`，需在 `BattleCoreRefactorSnapshotAdapter` 中补充字段
