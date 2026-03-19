//include/MapEngine/MapEngine.hpp
#pragma once
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

        // 在 MapEngine 类的 public 部分添加
        int get_current_layer() const {
            for (const auto& pair : nodes_) {
                if (pair.second.is_current) {
                    return pair.second.layer;
                }
            }
            return -1;
        }

        MapEngine();
        ~MapEngine();

        // 核心接口
        void init_map(int layers, int nodes_per_layer,
            const std::vector<NodeType>& layer_types = {});
        void init_fixed_map(const class MapConfig& config);  // 需要前向声明 MapConfig

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

        // 节点状态更新
        void set_node_visited(const NodeId& node_id);
        void set_current_node(const NodeId& node_id);
        void update_reachable_nodes();

        // ==== 新增：检查是否有当前节点 ====
        bool hasCurrentNode() const;

        // ==== 新增：设置回调函数 ====
        void setContentIdGenerator(ContentIdGenerator generator) { m_contentIdGenerator = generator; }
        void setNodeEnterCallback(NodeEnterCallback callback) { m_nodeEnterCallback = callback; }

    private:
        std::unordered_map<NodeId, MapNode> nodes_;
        std::unordered_map<int, std::vector<NodeId>> layers_;
        int total_layers_;

        // ==== 新增：回调函数成员 ====
        ContentIdGenerator m_contentIdGenerator;
        NodeEnterCallback m_nodeEnterCallback;

        NodeId generate_node_id(int layer, int index);
        NodeType random_node_type(int layer, int total_layers);
        void build_connections();
        bool validate_path_exists();
        void build_fixed_connections(const std::vector<std::vector<std::pair<int, int>>>& connections);
    };

}