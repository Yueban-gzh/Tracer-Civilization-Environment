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

```cpp
// 节点类型枚举，对应界面上的不同图标和交互
// 定义在公共头文件（如 include/Common/Types.hpp）中
enum class NodeType {
    Battle,   // 战斗
    Event,    // 事件
    Rest,     // 休息
    Shop,     // 商店
    Boss      // BOSS战（战斗的一种特殊形式，但为了清晰可单独列出）
};

// 节点唯一标识符，由主流程或地图生成时分配
// 简单起见，可以用整数或字符串，例如 "1-2" 表示第1层第2个节点
using NodeId = std::string; // 或 int

// 内容ID：指向具体战斗的怪物组ID、具体事件的ID等
// 与E模块定义的ID类型保持一致
using ContentId = std::string; // 例如 "monster_group_1", "event_1"

2.2 A模块内部数据结构
（1）地图节点（Node）
地图由多个节点构成。每个节点包含其在图中的静态信息。
struct MapNode {
    NodeId id;                 // 节点唯一标识，如 "1-2"
    NodeType type;             // 节点类型（战斗、事件等）
    ContentId contentId;       // 内容ID，用于主流程加载具体内容（例如是打哪个怪物）
    int layer;                 // 节点所在的层数（从0或1开始，由团队约定）
    std::vector<NodeId> prevNodes; // 前置节点ID列表（从哪些节点可以过来）
    std::vector<NodeId> nextNodes; // 后置节点ID列表（可以去哪些节点）

    // 可选：节点坐标（用于UI展示，如果UI层不负责布局，可以在此记录位置）
    // float x, y;
};

（2）地图数据（MapData）

整个地图的容器。这是 A 模块的核心数据结构，由 init_map 生成并持有。
class MapData {
private:
    std::unordered_map<NodeId, MapNode> nodes_;        // 哈希表/BST：通过NodeId快速查找节点
    std::unordered_map<int, std::vector<NodeId>> layers_; // 按层索引：每层有哪些节点
    // 其他成员...
public:
    // 图的构建、查询接口...
};

课设落点说明：

图（Graph）：nodes_ 中存储的 MapNode 及其内部的 prevNodes/nextNodes 共同构成了一个有向图。

哈希/BST（Hash/BST）：nodes_ 使用 std::unordered_map（哈希表）或 std::map（树）来实现通过 NodeId 对节点的快速查找，这本身就是一种查找数据结构的应用。

## 三、A 对外接口（函数规格）

## 三、A 对外接口（函数规格）
| 接口名称 | 接口签名 | 接口功能描述 |
| :--- | :--- | :--- |
| init_map | `void init_map(int layers, int nodes_per_layer, const std::vector<NodeType>& content_types)` | 生成指定层数、每层节点数的DAG结构地图，构建节点与连接关系，保证起点到Boss节点可达 |
| get_nodes_at_layer | `std::vector<MapNode> get_nodes_at_layer(int layer) const` | 获取指定层数的所有地图节点信息，用于UI层渲染当前层节点 |
| get_node_by_id | `MapNode get_node_by_id(NodeId node_id) const` | 根据节点唯一ID查询对应节点完整信息 |
| get_next_nodes | `std::vector<MapNode> get_next_nodes(NodeId node_id) const` | 查询当前节点可直接到达的所有后置节点列表 |
| get_prev_nodes | `std::vector<MapNode> get_prev_nodes(NodeId node_id) const` | 查询可到达当前节点的所有前置节点列表 |
| is_reachable | `bool is_reachable(NodeId from, NodeId to) const` | 使用BFS/DFS判断两个节点之间是否存在可达路径 |
| find_shortest_path | `std::vector<NodeId> find_shortest_path(NodeId from, NodeId to) const` | 使用BFS查找两点之间的最短路径并返回节点ID序列 |
| find_all_paths_to_boss | `std::vector<std::vector<NodeId>> find_all_paths_to_boss() const` | 使用DFS查找从起点到最终Boss节点的所有可行路径 |
| get_map_snapshot | `MapSnapshot get_map_snapshot() const` | 获取整张地图的完整快照，包含所有节点与边信息，用于全局绘制 |

struct MapSnapshot {
    // 可以用邻接表或边列表的形式返回，让UI可以自由绘制
    std::vector<MapNode> all_nodes; // 所有节点
    // 或者更明确地给出连接关系
    struct Edge {
        NodeId from;
        NodeId to;
    };
    std::vector<Edge> all_edges;
    int total_layers;
};

3.1 与规则条文对照
地图结构：地图是一个多层的、有向无环图（DAG）。每一层有多个节点，玩家只能在相邻层之间沿固定方向前进。

节点类型：节点分为战斗、事件、休息、商店和Boss。

路径选择：玩家在一个节点通关后，可以解锁并选择其一个或多个后置节点继续前进。选择具有策略性。

Boss战：最后一层为Boss节点，击败Boss即通关当前幕。

四、A 依赖其他模块的接口
A 模块无任何依赖。它不调用 B、C、D、E 的任何接口。NodeType 和 ContentId 对于 A 而言只是与团队约定的、透明传递的数据。地图的结构和连接关系的生成完全由 A 自身逻辑控制。

五、文件与符号归属
内容	放置位置
公共类型 (NodeType, NodeId, ContentId)	include/Common/Types.hpp（与团队协商决定）
A模块：MapNode, MapSnapshot, MapEngine 类声明	include/MapEngine/MapEngine.hpp
A模块：上述接口实现	src/MapEngine/MapEngine.cpp
A模块可能用到的内部辅助结构或算法	src/MapEngine/ 下的其他 .cpp 或 .hpp 文件

六、小结：实现清单（阶段 1 目标）
A - MapEngine

数据结构：

定义 MapNode 结构体（包含 id, type, contentId, layer, prev/next 连接）。

设计 MapData 类，内部使用 std::unordered_map<NodeId, MapNode> 存储节点，使用 std::unordered_map<int, std::vector<NodeId>> 建立层索引。

核心接口实现：

init_map(layers, nodes_per_layer, ...)：根据规则生成 DAG，保证起点到终点可达。

get_nodes_at_layer(layer)：返回指定层的节点列表。

get_node_by_id(node_id)：返回指定ID的节点信息。

get_next_nodes(node_id) / get_prev_nodes(node_id)：获取前后置节点。

算法演示接口（用于课设报告）：

is_reachable(from, to)：基于 BFS 或 DFS 实现。

find_shortest_path(from, to)：基于 BFS 实现。

find_all_paths_to_boss()：基于 DFS 实现。

测试与验证：

编写简单的测试代码（可以是 main 函数中的调试代码），验证地图生成符合 DAG 结构。

验证起点到 Boss 的路径必然存在。

验证 BFS/DFS 算法的正确性，并能打印/输出结果供报告使用。