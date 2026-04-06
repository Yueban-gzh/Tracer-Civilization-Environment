// include/MapEngine/MapEngine.hpp
#pragma once

namespace tce {
    class RunRng;
}

#include "../Common/NodeTypes.hpp"
#include <vector>
#include <unordered_map>
#include <string>
#include <queue>
#include <set>
#include <functional>  
#include <iostream>

namespace MapEngine {

    struct MapNode {
        NodeId id;
        NodeType type;
        ContentId content_id;
        int layer;
        std::vector<NodeId> prev_nodes;
        std::vector<NodeId> next_nodes;
        Vector2 position;
        bool is_visited = false;
        bool is_current = false;
        bool is_reachable = true;
        bool is_completed = false;  // 【新增】节点是否已完成（战斗/事件已处理）

        MapNode() : layer(0) {}
    };

    struct Edge {
        NodeId from;
        NodeId to;
    };

    struct MapSnapshot {
        std::vector<MapNode> all_nodes;
        std::vector<Edge> all_edges;
        int total_layers = 0;
    };

    class MapEngine {
    public:
        // 类型定义
        using ContentIdGenerator = std::function<ContentId(NodeType type, int layer, int index)>;
        using NodeEnterCallback = std::function<void(const MapNode& node)>;

        // 辅助方法
        int get_current_layer() const {
            for (const auto& pair : nodes_) {
                if (pair.second.is_current) {
                    return pair.second.layer;
                }
            }
            return -1;
        }

        int get_total_layers() const { return total_layers_; }

        MapEngine();
        ~MapEngine();

        // 核心接口
        void init_map(int layers, int nodes_per_layer,
            const std::vector<NodeType>& layer_types = {});
        void init_fixed_map(const class MapConfig& config);

        // ========== 新增：随机地图生成 ==========
        void init_random_map(int map_index);

        // 查询接口
        std::vector<MapNode> get_nodes_at_layer(int layer) const;
        MapNode get_node_by_id(const NodeId& node_id) const;
        std::vector<MapNode> get_next_nodes(const NodeId& node_id) const;
        std::vector<MapNode> get_prev_nodes(const NodeId& node_id) const;

        // 算法接口
        bool is_reachable(const NodeId& from_node, const NodeId& to_node) const;
        std::vector<NodeId> find_shortest_path(const NodeId& from_node,
            const NodeId& to_node) const;
        std::vector<std::vector<NodeId>> find_all_paths_to_boss() const;
        MapSnapshot get_map_snapshot() const;
        /** 从快照完整恢复地图拓扑、位置与节点标记（用于读档）。 */
        void restore_from_snapshot(const MapSnapshot& snapshot);

        // 节点状态更新
        void set_node_visited(const NodeId& node_id);
        void set_current_node(const NodeId& node_id);
        void update_reachable_nodes();

        // 检查是否有当前节点
        bool hasCurrentNode() const;

        // 设置回调函数
        void setContentIdGenerator(ContentIdGenerator generator) { m_contentIdGenerator = generator; }
        void setNodeEnterCallback(NodeEnterCallback callback) { m_nodeEnterCallback = callback; }
        void set_run_rng(tce::RunRng* rng) { run_rng_ = rng; }

    private:
        // ========== 成员变量 ==========
        std::unordered_map<NodeId, MapNode> nodes_;
        std::unordered_map<int, std::vector<NodeId>> layers_;
        int total_layers_;

        ContentIdGenerator m_contentIdGenerator;
        NodeEnterCallback m_nodeEnterCallback;
        tce::RunRng* run_rng_ = nullptr;

        // ========== 私有方法 ==========
        NodeId generate_node_id(int layer, int index);
        NodeType random_node_type(int layer, int total_layers);
        void build_connections();
        bool validate_path_exists();
        void build_fixed_connections(const std::vector<std::vector<std::pair<int, int>>>& connections);

        // ========== 随机地图生成辅助方法 ==========
        void build_random_connections();       // 构建随机连接
        void auto_layout_nodes();              // 自动布局节点
        void adjust_positions_by_connections(); // 根据连接微调位置

    };

}
