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
| `NodeType` | `enum class NodeType { Enemy, Elite, Event, Rest, Merchant, Treasure, Boss }` | 节点类型枚举，对应界面上的不同图标和交互。定义在公共头文件（如 `include/Common/NodeTypes.hpp`）中。 |
| `NodeId` | `using NodeId = std::string;` | 节点唯一标识符，由主流程或地图生成时分配。例如 "1-2" 表示第1层第2个节点。 |
| `ContentId` | `using ContentId = std::string;` | 内容ID，指向具体战斗的怪物组ID、具体事件的ID等。与E模块定义的ID类型保持一致。 |
| `Vector2` | `struct Vector2 { float x; float y; };` | 2D坐标，用于UI展示节点位置。 |

### 2.2 A 模块内部数据结构

**（1）地图节点（Node）**

地图由多个节点构成。每个节点包含其在图中的静态信息和运行时状态。

| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `id` | `NodeId` | 节点唯一标识，如 "1-2" |
| `type` | `NodeType` | 节点类型（战斗、事件等） |
| `contentId` | `ContentId` | 内容ID，用于主流程加载具体内容（例如是打哪个怪物） |
| `layer` | `int` | 节点所在的层数（从0开始） |
| `prevNodes` | `std::vector<NodeId>` | 前置节点ID列表（从哪些节点可以过来） |
| `nextNodes` | `std::vector<NodeId>` | 后置节点ID列表（可以去哪些节点） |
| `position` | `Vector2` | 节点坐标，用于UI展示 |
| `isVisited` | `bool` | 是否已访问过 |
| `isCurrent` | `bool` | 是否是当前所在节点 |
| `isReachable` | `bool` | 从当前节点是否可达 |

**（2）地图数据（MapData）**

整个地图的容器。这是 A 模块的核心数据结构，由 `initMap` 生成并持有。

| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `nodes_` | `std::unordered_map<NodeId, MapNode>` | 哈希表：通过NodeId快速查找节点 |
| `layers_` | `std::unordered_map<int, std::vector<NodeId>>` | 按层索引：每层有哪些节点 |

- **课设落点说明**：
    - **图（Graph）**：`nodes_` 中存储的 `MapNode` 及其内部的 `prevNodes`/`nextNodes` 共同构成了一个有向图。
    - **哈希表（Hash Table）**：`nodes_` 使用 `std::unordered_map` 实现通过 `NodeId` 对节点的快速查找，这本身就是一种查找数据结构的应用。

---

## 三、A 对外接口（函数规格）

以下为 A 模块**必须实现**的对外 API。入参/出参类型为逻辑描述，实现时用 C++ 类型替换。

### 3.1 回调机制

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **setContentIdGenerator** | `void setContentIdGenerator(std::function<ContentId(NodeType type, int layer, int index)> generator)` | 设置内容ID生成器回调。在初始化地图时，对每个节点调用此回调生成 `contentId`。 |
| **setNodeEnterCallback** | `void setNodeEnterCallback(std::function<void(const MapNode& node)> callback)` | 设置节点进入回调。当玩家通过 `setCurrentNode` 进入节点时触发，通知主流程。 |

### 3.2 地图初始化

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **initMap** | `void initMap(int layers, int nodes_per_layer, const std::vector<NodeType>& layer_types = {})` | **随机生成地图**。生成指定层数的随机地图，每层节点数可随机变化。如果提供了 `layer_types`，则按指定类型设置节点。 |
| **initFixedMap** | `void initFixedMap(const MapConfig& config)` | **固定地图初始化**。根据预定义的 `MapConfig` 配置生成地图，用于三张固定地图。 |

### 3.3 节点查询

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **getNodesAtLayer** | `std::vector<MapNode> getNodesAtLayer(int layer) const` | 返回指定 `layer` 的所有节点信息。主流程用此接口在UI上渲染当前层的节点。 |
| **getNodeById** | `MapNode getNodeById(NodeId node_id) const` | 返回指定 `node_id` 的节点信息。主流程在玩家选中某个节点后，需要获取该节点的详细信息（如类型、内容ID）。 |
| **getNextNodes** | `std::vector<MapNode> getNextNodes(NodeId node_id) const` | 返回从给定 `node_id` 出发可以到达的所有后置节点。主流程用于在玩家通关当前节点后，解锁下一层的节点。 |
| **getPrevNodes** | `std::vector<MapNode> getPrevNodes(NodeId node_id) const` | 返回可以到达给定 `node_id` 的所有前置节点。可用于UI显示路径或验证。 |

### 3.4 节点状态管理

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **setNodeVisited** | `void setNodeVisited(NodeId node_id)` | 将指定节点标记为已访问。 |
| **setCurrentNode** | `void setCurrentNode(NodeId node_id)` | 设置当前节点（玩家所在位置），并触发节点进入回调。 |
| **updateReachableNodes** | `void updateReachableNodes()` | 基于当前节点，使用BFS更新所有节点的可达性状态。 |
| **hasCurrentNode** | `bool hasCurrentNode() const` | 判断是否已有当前节点（游戏是否已开始）。 |

### 3.5 路径算法（用于课设演示）

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **isReachable** | `bool isReachable(NodeId from_node, NodeId to_node) const` | **BFS/DFS 应用**。判断是否存在一条从 `from_node` 到 `to_node` 的路径。主流程可用此来验证起点到Boss是否有路。 |
| **findShortestPath** | `std::vector<NodeId> findShortestPath(NodeId from_node, NodeId to_node) const` | **BFS 应用**。返回从 `from_node` 到 `to_node` 的最短路径（节点ID列表）。主要用于演示BFS算法。 |
| **findAllPathsToBoss** | `std::vector<std::vector<NodeId>> findAllPathsToBoss() const` | **DFS 应用**。找到从起点（第一层某节点）到终点（最后一层Boss节点）的所有可能路径。主要用于演示DFS算法。 |
| **getMapSnapshot** | `MapSnapshot getMapSnapshot() const` | 返回当前地图的完整快照，用于UI一次性绘制整个地图。 |

**MapSnapshot 建议结构（供 UI 展示）**：

| 字段 | 类型 | 说明 |
|:---|:---|:---|
| `all_nodes` | `std::vector<MapNode>` | 所有节点 |
| `all_edges` | `std::vector<Edge>` | 所有连接关系，`Edge` 包含 `from` 和 `to` 两个 `NodeId` |
| `total_layers` | `int` | 总层数 |

### 3.6 地图配置管理（用于三张固定地图）

| 接口 | 签名（逻辑） | 行为 |
|:---|:---|:---|
| **MapConfigManager** | 构造函数 | 管理三张固定地图配置（标准、森林、沙漠）。 |
| **getCurrentConfig** | `MapConfig* getCurrentConfig() const` | 获取当前选中的地图配置。 |
| **nextMap** | `void nextMap()` | 切换到下一张地图。 |
| **prevMap** | `void prevMap()` | 切换到上一张地图。 |
| **getCurrentMapName** | `std::string getCurrentMapName() const` | 获取当前地图名称。 |
| **getCurrentMapDescription** | `std::string getCurrentMapDescription() const` | 获取当前地图描述。 |
| **getMapCount** | `size_t getMapCount() const` | 获取地图总数。 |

### 3.7 与规则条文对照

- **地图结构**：地图是一个多层的、有向无环图（DAG）。每一层有多个节点，玩家只能在相邻层之间沿固定方向前进。
- **节点类型**：节点分为战斗、事件、休息、商店、宝藏和Boss。
- **路径选择**：玩家在一个节点通关后，可以解锁并选择其一个或多个后置节点继续前进。选择具有策略性。
- **Boss战**：最后一层为Boss节点，击败Boss即通关当前幕。

---

## 四、A 依赖其他模块的接口

**A 模块无任何依赖**。它不调用 B、C、D、E 的任何接口。`NodeType` 和 `ContentId` 对于 A 而言只是与团队约定的、透明传递的数据。地图的结构和连接关系的生成完全由 A 自身逻辑控制。

A 模块通过回调机制与主流程交互：
1. **内容ID生成器**：主流程（或E模块）提供，用于为每个节点生成具体的内容ID
2. **节点进入回调**：当玩家进入节点时，A模块通知主流程，由主流程根据节点类型和内容ID调用相应的模块（B、D等）

---

## 五、文件与符号归属

| 内容 | 放置位置 |
| :--- | :--- |
| 公共类型 (`NodeType`, `NodeId`, `ContentId`, `Vector2`) | `include/Common/NodeTypes.hpp` |
| 地图配置 (`MapConfig` 基类及派生类) | `include/MapEngine/MapConfig.hpp` |
| A模块：`MapNode`, `Edge`, `MapSnapshot`, `MapEngine` 类声明 | `include/MapEngine/MapEngine.hpp` |
| A模块：上述接口实现 | `src/MapEngine/MapEngine.cpp` |
| A模块可能用到的内部辅助结构或算法 | `src/MapEngine/` 下的其他 `.cpp` 或 `.hpp` 文件 |

---

## 六、小结：实现清单（阶段 1 目标）

**A - MapEngine**

- **数据结构**：
    - [ ] 定义 `MapNode` 结构体（包含 id, type, contentId, layer, prev/next 连接, position, 状态字段）。
    - [ ] 设计 `MapEngine` 类，内部使用 `std::unordered_map<NodeId, MapNode>` 存储节点，使用 `std::unordered_map<int, std::vector<NodeId>>` 建立层索引。
- **回调机制**：
    - [ ] 实现 `setContentIdGenerator` 和 `setNodeEnterCallback`。
- **核心接口实现**：
    - [ ] `initMap` / `initFixedMap`：根据规则生成 DAG，保证起点到终点可达。
    - [ ] `getNodesAtLayer`：返回指定层的节点列表。
    - [ ] `getNodeById`：返回指定ID的节点信息。
    - [ ] `getNextNodes` / `getPrevNodes`：获取前后置节点。
    - [ ] `setCurrentNode` / `updateReachableNodes` / `hasCurrentNode`：状态管理。
- **算法演示接口（用于课设报告）**：
    - [ ] `isReachable`：基于 **BFS** 或 **DFS** 实现。
    - [ ] `findShortestPath`：基于 **BFS** 实现。
    - [ ] `findAllPathsToBoss`：基于 **DFS** 实现。
- **地图配置**：
    - [ ] 实现 `MapConfigManager` 管理三张固定地图。
- **测试与验证**：
    - [ ] 编写简单的测试代码，验证地图生成符合 DAG 结构。
    - [ ] 验证起点到 Boss 的路径必然存在。
    - [ ] 验证 BFS/DFS 算法的正确性，并能打印/输出结果供报告使用。