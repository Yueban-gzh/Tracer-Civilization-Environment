# A - 地图模块设计与接口（定稿即按此实现）

> 本文档定义 **MapEngine（A）** 的模块边界、需实现的数据结构、以及对外部（主要是主流程）的接口规格。
> 与 [设计与接口.md](设计与接口.md) 一致；id 类型等若阶段 0 有变更，仅改本节类型定义即可。

---

## 一、模块划分与职责

| 模块 | 职责 | 课设数据结构落点 |
| :--- | :--- | :--- |
| **A - MapEngine** | 管理游戏地图：生成一个多层的、有向无环图（DAG）结构的地图；提供按层查询节点、查询前后置节点、验证路径可达性等功能；核心是图的构建与遍历。 | **图**：存储地图节点及连接关系。**BFS/DFS**：用于路径可达性分析（如验证起点到Boss是否有路）或寻找最短路径（用于演示）。 |

**依赖**：A 模块**不依赖**其他任何业务模块（B、C、D、E）。地图的节点类型（战斗、事件等）和内容ID（如怪物ID、事件ID）仅作为数据载体，由主流程在构建地图时传入或由A内部根据常量生成。A本身不关心这些ID的具体含义。

---

## 二、公共类型与数据结构

以下类型在 A 的实现中**直接使用**；部分类型与全团队共用，部分为 A 模块内部定义。

### 2.1 公共标识类型（与团队约定一致）

| 类型名 | 定义 | 说明 |
|:---|:---|:---|
| `NodeType` | `enum class NodeType { Battle, Event, Rest, Shop, Boss }` | 节点类型枚举，对应界面上的不同图标和交互。定义在公共头文件（如 `include/Common/Types.hpp`）中。 |
| `NodeId` | `using NodeId = std::string;` | 节点唯一标识符，由主流程或地图生成时分配。例如 "1-2" 表示第1层第2个节点。 |
| `ContentId` | `using ContentId = std::string;` | 内容ID，指向具体战斗的怪物组ID、具体事件的ID等。与E模块定义的ID类型保持一致。 |

### 2.2 A 模块内部数据结构

**（1）地图节点（Node）**

地图由多个节点构成。每个节点包含其在图中的静态信息。

| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `id` | `NodeId` | 节点唯一标识，如 "1-2" |
| `type` | `NodeType` | 节点类型（战斗、事件等） |
| `contentId` | `ContentId` | 内容ID，用于主流程加载具体内容（例如是打哪个怪物） |
| `layer` | `int` | 节点所在的层数（从0或1开始，由团队约定） |
| `prevNodes` | `std::vector<NodeId>` | 前置节点ID列表（从哪些节点可以过来） |
| `nextNodes` | `std::vector<NodeId>` | 后置节点ID列表（可以去哪些节点） |
| `x, y`（可选） | `float` | 节点坐标，用于UI展示（如果UI层不负责布局，可以在此记录位置） |

**（2）地图数据（MapData）**

整个地图的容器。这是 A 模块的核心数据结构，由 `init_map` 生成并持有。

| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `nodes_` | `std::unordered_map<NodeId, MapNode>` | 哈希表/BST：通过NodeId快速查找节点 |
| `layers_` | `std::unordered_map<int, std::vector<NodeId>>` | 按层索引：每层有哪些节点 |

- **课设落点说明**：
    - **图（Graph）**：`nodes_` 中存储的 `MapNode` 及其内部的 `prevNodes`/`nextNodes` 共同构成了一个有向图。
    - **哈希/BST（Hash/BST）**：`nodes_` 使用 `std::unordered_map`（哈希表）或 `std::map`（树）来实现通过 `NodeId` 对节点的快速查找，这本身就是一种查找数据结构的应用。

---

## 三、A 对外接口（函数规格）

以下为 A 模块**必须实现**的对外 API。入参/出参类型为逻辑描述，实现时用 C++ 类型替换。

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **init_map（初始化地图）** | `void init_map(int layers, int nodes_per_layer, const std::vector<NodeType>& layer_content_types)` | **核心接口**。生成一个 `layers` 层，每层约 `nodes_per_layer` 个节点的有向无环图（DAG）。<br><br>1. **生成节点**：为每一层创建指定数量的 `MapNode`，分配唯一的 `NodeId`，并根据 `layer_content_types`（或内部逻辑）设置 `type` 和 `contentId`。<br>2. **构建连接**：在相邻层之间随机建立连接，保证图是DAG。通常，一个节点可以连接到下一层的1-2个节点。必须保证**第一层（起点）到最后一层（Boss）是可达的**。<br>3. **建立索引**：填充 `nodes_` 和 `layers_` 等内部数据结构。 |
| **get_nodes_at_layer（获取指定层的所有节点）** | `std::vector<MapNode> get_nodes_at_layer(int layer) const` | 返回指定 `layer` 的所有节点信息（`MapNode`）。主流程用此接口在UI上渲染当前层的节点。 |
| **get_node_by_id（根据ID获取节点）** | `MapNode get_node_by_id(NodeId node_id) const` | 返回指定 `node_id` 的节点信息。主流程在玩家选中某个节点后，需要获取该节点的详细信息（如类型、内容ID）。 |
| **get_next_nodes（获取某节点的后置节点）** | `std::vector<MapNode> get_next_nodes(NodeId node_id) const` | 返回从给定 `node_id` 出发可以到达的所有后置节点。主流程用于在玩家通关当前节点后，解锁下一层的节点。 |
| **get_prev_nodes（获取某节点的前置节点）** | `std::vector<MapNode> get_prev_nodes(NodeId node_id) const` | 返回可以到达给定 `node_id` 的所有前置节点。可用于UI显示路径或验证。 |
| **is_reachable（验证可达性）** | `bool is_reachable(NodeId from_node, NodeId to_node) const` | **BFS/DFS 应用**。判断是否存在一条从 `from_node` 到 `to_node` 的路径。主流程可用此来验证起点到Boss是否有路，或用于某些特殊事件/遗物的效果（如“传送到一个可达节点”）。 |
| **find_shortest_path（寻找最短路径）** | `std::vector<NodeId> find_shortest_path(NodeId from_node, NodeId to_node) const` | **BFS 应用**。返回从 `from_node` 到 `to_node` 的最短路径（节点ID列表）。主要用于演示BFS算法，或供UI绘制引导线。 |
| **find_all_paths_to_boss（找到所有通往Boss的路径）** | `std::vector<std::vector<NodeId>> find_all_paths_to_boss() const` | **DFS 应用**。找到从起点（第一层某节点）到终点（最后一层Boss节点）的所有可能路径。主要用于演示DFS算法。 |
| **get_map_snapshot（获取地图快照）** | `MapSnapshot get_map_snapshot() const` | 返回当前地图的完整快照，用于UI一次性绘制整个地图。`MapSnapshot` 可定义为包含所有 `MapNode` 及连接关系的结构（如邻接表）。 |

**MapSnapshot 建议结构（供 UI 展示）**：

| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `all_nodes` | `std::vector<MapNode>` | 所有节点 |
| `all_edges` | `std::vector<Edge>` | 所有连接关系，`Edge` 包含 `from` 和 `to` 两个 `NodeId` 字段 |
| `total_layers` | `int` | 总层数 |

### 3.1 与规则条文对照

- **地图结构**：地图是一个多层的、有向无环图（DAG）。每一层有多个节点，玩家只能在相邻层之间沿固定方向前进。
- **节点类型**：节点分为战斗、事件、休息、商店和Boss。
- **路径选择**：玩家在一个节点通关后，可以解锁并选择其一个或多个后置节点继续前进。选择具有策略性。
- **Boss战**：最后一层为Boss节点，击败Boss即通关当前幕。

---

## 四、A 依赖其他模块的接口

**A 模块无任何依赖**。它不调用 B、C、D、E 的任何接口。`NodeType` 和 `ContentId` 对于 A 而言只是与团队约定的、透明传递的数据。地图的结构和连接关系的生成完全由 A 自身逻辑控制。

---

## 五、文件与符号归属

| 内容 | 放置位置 |
| :--- | :--- |
| 公共类型 (`NodeType`, `NodeId`, `ContentId`) | `include/Common/Types.hpp`（与团队协商决定） |
| A模块：`MapNode`, `MapSnapshot`, `MapEngine` 类声明 | `include/MapEngine/MapEngine.hpp` |
| A模块：上述接口实现 | `src/MapEngine/MapEngine.cpp` |
| A模块可能用到的内部辅助结构或算法 | `src/MapEngine/` 下的其他 `.cpp` 或 `.hpp` 文件 |

---

## 六、小结：实现清单（阶段 1 目标）

**A - MapEngine**

- **数据结构**：
    - [ ] 定义 `MapNode` 结构体（包含 id, type, contentId, layer, prev/next 连接）。
    - [ ] 设计 `MapData` 类，内部使用 `std::unordered_map<NodeId, MapNode>` 存储节点，使用 `std::unordered_map<int, std::vector<NodeId>>` 建立层索引。
- **核心接口实现**：
    - [ ] `init_map(layers, nodes_per_layer, ...)`：根据规则生成 DAG，保证起点到终点可达。
    - [ ] `get_nodes_at_layer(layer)`：返回指定层的节点列表。
    - [ ] `get_node_by_id(node_id)`：返回指定ID的节点信息。
    - [ ] `get_next_nodes(node_id)` / `get_prev_nodes(node_id)`：获取前后置节点。
- **算法演示接口（用于课设报告）**：
    - [ ] `is_reachable(from, to)`：基于 **BFS** 或 **DFS** 实现。
    - [ ] `find_shortest_path(from, to)`：基于 **BFS** 实现。
    - [ ] `find_all_paths_to_boss()`：基于 **DFS** 实现。
- **测试与验证**：
    - [ ] 编写简单的测试代码（可以是 main 函数中的调试代码），验证地图生成符合 DAG 结构。
    - [ ] 验证起点到 Boss 的路径必然存在。
    - [ ] 验证 BFS/DFS 算法的正确性，并能打印/输出结果供报告使用。

