// MapEngine.hpp
// A模块：地图引擎的头文件
// 这里只声明"有什么"，具体"怎么做"在 .cpp 文件中实现

#pragma once
#include "../Common/NodeTypes.hpp" // 引入公共类型
#include <vector>
#include <unordered_map>
#include <string>

namespace MapEngine {  // 使用命名空间避免名字冲突

    // 地图节点结构体：存储一个节点的所有信息
    struct MapNode {
        NodeId id;                      // 节点唯一标识，如 "1-2"
        NodeType type;                  // 节点类型（战斗/事件/休息/商店/Boss）
        ContentId content_id;            // 内容ID，告诉主流程具体加载什么
        int layer;                      // 所在层数（从0开始）
        std::vector<NodeId> prev_nodes;  // 前置节点（可以从哪些节点过来）
        std::vector<NodeId> next_nodes;  // 后置节点（可以去哪些节点）
        float x = 0.0f;                 // UI坐标X（可选）
        float y = 0.0f;                 // UI坐标Y（可选）
    };

    // 边结构体：用于UI显示连接关系
    struct Edge {
        NodeId from;  // 起点
        NodeId to;    // 终点
    };

    // 地图快照：一次性获取整个地图的信息，方便UI绘制
    struct MapSnapshot {
        std::vector<MapNode> all_nodes;      // 所有节点
        std::vector<Edge> all_edges;         // 所有连接
        int total_layers = 0;                // 总层数
    };

    // 地图引擎类：核心类，管理整个地图
    class MapEngine {
    public:
        // 构造函数和析构函数
        MapEngine();
        ~MapEngine();

        // ========== 核心接口 ==========

        // 初始化地图：生成一个多层的有向无环图（DAG）
        // layers: 总层数（比如15层）
        // nodesPerLayer: 每层大约多少个节点（比如2-3个）
        // layerTypes: 每层默认的节点类型（可选）
        void init_map(int layers, int nodesPerLayer,
            const std::vector<NodeType>& layerTypes = {});

        // 获取指定层的所有节点
        std::vector<MapNode> get_nodes_at_layer(int layer) const;

        // 根据ID获取单个节点
        MapNode get_node_by_id(const NodeId& nodeId) const;

        // 获取某个节点的后置节点（可以去哪里）
        std::vector<MapNode> get_next_nodes(const NodeId& nodeId) const;

        // 获取某个节点的前置节点（可以从哪里来）
        std::vector<MapNode> get_prev_nodes(const NodeId& nodeId) const;

        // ========== 算法接口（课设重点） ==========

        // BFS/DFS：判断从fromNode能否到达toNode
        bool is_reachable(const NodeId& fromNode, const NodeId& toNode) const;

        // BFS：找最短路径（返回节点ID列表）
        std::vector<NodeId> find_shortest_path(const NodeId& fromNode,
            const NodeId& toNode) const;

        // DFS：找从起点到Boss的所有路径（用于演示DFS）
        std::vector<std::vector<NodeId>> find_all_paths_to_boss() const;

        // 获取地图快照（用于UI一次性绘制）
        MapSnapshot get_map_snapshot() const;

    private:
        // ========== 内部数据结构 ==========

        // 存储所有节点：用哈希表实现快速查找（NodeId -> MapNode）
        // 这就是课设要求的"查找数据结构"应用
        std::unordered_map<NodeId, MapNode> nodes_;

        // 层索引：快速知道某层有哪些节点（layer -> 节点ID列表）
        std::unordered_map<int, std::vector<NodeId>> layers_;

        // 总层数
        int total_layers_ = 0;

        // ========== 内部辅助函数 ==========

        // 生成节点ID：比如第1层第2个节点 -> "1-2"
        NodeId generate_node_id(int layer, int index);

        // 随机生成节点类型
        NodeType random_node_type(int layer, int totalLayers);

        // 构建层与层之间的连接（保证DAG和可达性）
        void build_connections();

        // 验证起点到终点是否有路径（用于生成时自检）
        bool validate_path_exists();
    };

} // namespace MapEngine