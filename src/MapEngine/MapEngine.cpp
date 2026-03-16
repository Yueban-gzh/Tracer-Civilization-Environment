// src/MapEngine/MapEngine.cpp
#include "../../include/MapEngine/MapEngine.hpp" 
#include "../../MapConfig.hpp"
#include <queue>
#include <stack>
#include <set>
#include <random>
#include <algorithm>
#include <iostream>

namespace MapEngine {

    // ==== 修改：构造函数初始化所有成员 ====
    MapEngine::MapEngine()
        : total_layers_(0)
        , m_contentIdGenerator(nullptr)
        , m_nodeEnterCallback(nullptr) {
    }

    MapEngine::~MapEngine() = default;

    NodeId MapEngine::generate_node_id(int layer, int index) {
        return std::to_string(layer) + "-" + std::to_string(index);
    }

    NodeType MapEngine::random_node_type(int layer, int total_layers) {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 100);

        int roll = dis(gen);

        if (layer == 0) {
            return NodeType::Enemy;
        }

        if (layer == total_layers - 1) {
            return NodeType::Boss;
        }

        if (roll < 40) return NodeType::Enemy;
        else if (roll < 65) return NodeType::Event;
        else if (roll < 80) return NodeType::Rest;
        else return NodeType::Merchant;
    }

    void MapEngine::init_map(int layers, int nodes_per_layer,
        const std::vector<NodeType>& layer_types) {

        nodes_.clear();
        layers_.clear();
        total_layers_ = layers;

        std::cout << "开始生成地图：" << layers << "层，每层约"
            << nodes_per_layer << "个节点" << std::endl;

        // 生成所有节点
        for (int layer = 0; layer < layers; ++layer) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, nodes_per_layer);
            int node_count = dis(gen);

            if (layer == 0 || layer == layers - 1) {
                node_count = 1;
            }

            std::cout << "  第" << layer << "层生成" << node_count << "个节点" << std::endl;

            for (int i = 0; i < node_count; ++i) {
                MapNode node;
                node.id = this->generate_node_id(layer, i);
                node.layer = layer;

                if (!layer_types.empty() && layer < static_cast<int>(layer_types.size())) {
                    node.type = layer_types[layer];
                }
                else {
                    node.type = this->random_node_type(layer, layers);
                }

                // ==== 修改：使用生成器填充 content_id ====
                if (m_contentIdGenerator) {
                    node.content_id = m_contentIdGenerator(node.type, layer, i);
                }
                else {
                    node.content_id = "content_" + std::to_string(static_cast<int>(node.type)) + "_" + node.id;
                }
                // ==== 修改结束 ====

                node.position.x = 100.0f + i * 200.0f;
                node.position.y = 100.0f + layer * 100.0f;

                nodes_[node.id] = node;
                layers_[layer].push_back(node.id);
            }
        }

        this->build_connections();

        if (!this->validate_path_exists()) {
            std::cerr << "错误：起点到Boss不可达，重新生成..." << std::endl;
            this->init_map(layers, nodes_per_layer, layer_types);
            return;
        }

        std::cout << "地图生成完成！节点总数：" << nodes_.size() << std::endl;
    }

    void MapEngine::init_fixed_map(const MapConfig& config) {
        nodes_.clear();
        layers_.clear();

        auto layerTypes = config.getLayerTypes();
        auto positions = config.getNodePositions();

        total_layers_ = static_cast<int>(layerTypes.size());

        std::cout << "初始化地图: " << config.getName() << std::endl;
        std::cout << "描述: " << config.getDescription() << std::endl;
        std::cout << "共 " << total_layers_ << " 层" << std::endl;

        // 创建节点
        for (int layer = 0; layer < total_layers_; ++layer) {
            const auto& types = layerTypes[layer];
            const auto& pos = positions[layer];

            for (size_t i = 0; i < types.size(); ++i) {
                MapNode node;
                node.id = this->generate_node_id(layer, static_cast<int>(i));
                node.layer = layer;
                node.type = static_cast<NodeType>(types[i]);

                // ==== 修改：使用生成器填充 content_id ====
                if (m_contentIdGenerator) {
                    node.content_id = m_contentIdGenerator(node.type, layer, static_cast<int>(i));
                }
                else {
                    node.content_id = "content_" + std::to_string(static_cast<int>(node.type)) + "_" + node.id;
                    std::cout << "警告：未设置内容ID生成器，使用默认ID: " << node.content_id << std::endl;
                }
                // ==== 修改结束 ====

                node.position = pos[i];
                node.is_visited = false;
                node.is_current = false;
                node.is_reachable = (layer == 0);

                nodes_[node.id] = node;
                layers_[layer].push_back(node.id);

                std::cout << "  创建节点 " << node.id << " 于位置 ("
                    << pos[i].x << ", " << pos[i].y << ")" << std::endl;
            }
        }

        // 构建连接
        auto connections = config.getConnections();
        this->build_fixed_connections(connections);

        std::cout << "地图初始化完成！节点总数：" << nodes_.size() << std::endl;
    }

    void MapEngine::build_fixed_connections(
        const std::vector<std::vector<std::pair<int, int>>>& connections) {

        for (int layer = 0; layer < static_cast<int>(connections.size()); ++layer) {
            const auto& layerConnections = connections[layer];

            std::cout << "处理第 " << layer << " 层到第 " << (layer + 1) << " 层的连接:" << std::endl;

            for (const auto& conn : layerConnections) {
                int fromIdx = conn.first;
                int toIdx = conn.second;

                NodeId fromId = this->generate_node_id(layer, fromIdx);
                NodeId toId = this->generate_node_id(layer + 1, toIdx);

                // 检查节点是否存在
                if (nodes_.find(fromId) == nodes_.end()) {
                    std::cerr << "错误: 源节点 " << fromId << " 不存在!" << std::endl;
                    continue;
                }
                if (nodes_.find(toId) == nodes_.end()) {
                    std::cerr << "错误: 目标节点 " << toId << " 不存在!" << std::endl;
                    continue;
                }

                // 添加连接
                nodes_[fromId].next_nodes.push_back(toId);
                nodes_[toId].prev_nodes.push_back(fromId);

                std::cout << "  连接: " << fromId << " -> " << toId << std::endl;
            }
        }
    }

    void MapEngine::build_connections() {
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int layer = 0; layer < total_layers_ - 1; ++layer) {
            const auto& current_layer_ids = layers_[layer];
            const auto& next_layer_ids = layers_[layer + 1];

            for (const auto& curr_id : current_layer_ids) {
                std::uniform_int_distribution<> dis(1, 2);
                int connection_count = std::min(dis(gen),
                    static_cast<int>(next_layer_ids.size()));

                std::vector<NodeId> targets = next_layer_ids;
                std::shuffle(targets.begin(), targets.end(), gen);

                for (int i = 0; i < connection_count; ++i) {
                    NodeId target_id = targets[i];

                    nodes_[curr_id].next_nodes.push_back(target_id);
                    nodes_[target_id].prev_nodes.push_back(curr_id);
                }
            }

            for (const auto& next_id : next_layer_ids) {
                if (nodes_[next_id].prev_nodes.empty()) {
                    std::uniform_int_distribution<> dis(0,
                        static_cast<int>(current_layer_ids.size()) - 1);
                    int random_idx = dis(gen);
                    NodeId source_id = current_layer_ids[random_idx];

                    nodes_[source_id].next_nodes.push_back(next_id);
                    nodes_[next_id].prev_nodes.push_back(source_id);
                }
            }
        }
    }

    bool MapEngine::validate_path_exists() {
        if (layers_.empty()) return false;

        // 检查是否有第0层
        if (layers_.find(0) == layers_.end() || layers_.at(0).empty()) return false;

        // 检查是否有最后一层
        if (layers_.find(total_layers_ - 1) == layers_.end() ||
            layers_.at(total_layers_ - 1).empty()) return false;

        NodeId start = layers_.at(0)[0];
        NodeId end = layers_.at(total_layers_ - 1)[0];

        return this->is_reachable(start, end);
    }

    std::vector<MapNode> MapEngine::get_nodes_at_layer(int layer) const {
        std::vector<MapNode> result;

        auto it = layers_.find(layer);
        if (it != layers_.end()) {
            for (const auto& node_id : it->second) {
                auto node_it = nodes_.find(node_id);
                if (node_it != nodes_.end()) {
                    result.push_back(node_it->second);
                }
            }
        }

        return result;
    }

    MapNode MapEngine::get_node_by_id(const NodeId& node_id) const {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end()) {
            return it->second;
        }
        return MapNode{};
    }

    std::vector<MapNode> MapEngine::get_next_nodes(const NodeId& node_id) const {
        std::vector<MapNode> result;

        auto it = nodes_.find(node_id);
        if (it != nodes_.end()) {
            for (const auto& next_id : it->second.next_nodes) {
                auto next_it = nodes_.find(next_id);
                if (next_it != nodes_.end()) {
                    result.push_back(next_it->second);
                }
            }
        }

        return result;
    }

    std::vector<MapNode> MapEngine::get_prev_nodes(const NodeId& node_id) const {
        std::vector<MapNode> result;

        auto it = nodes_.find(node_id);
        if (it != nodes_.end()) {
            for (const auto& prev_id : it->second.prev_nodes) {
                auto prev_it = nodes_.find(prev_id);
                if (prev_it != nodes_.end()) {
                    result.push_back(prev_it->second);
                }
            }
        }

        return result;
    }

    bool MapEngine::is_reachable(const NodeId& from_node, const NodeId& to_node) const {
        if (nodes_.find(from_node) == nodes_.end() ||
            nodes_.find(to_node) == nodes_.end()) {
            return false;
        }

        std::queue<NodeId> q;
        std::set<NodeId> visited;

        q.push(from_node);
        visited.insert(from_node);

        while (!q.empty()) {
            NodeId current = q.front();
            q.pop();

            if (current == to_node) {
                return true;
            }

            auto it = nodes_.find(current);
            if (it != nodes_.end()) {
                for (const auto& next_id : it->second.next_nodes) {
                    if (visited.find(next_id) == visited.end()) {
                        visited.insert(next_id);
                        q.push(next_id);
                    }
                }
            }
        }

        return false;
    }

    std::vector<NodeId> MapEngine::find_shortest_path(const NodeId& from_node,
        const NodeId& to_node) const {
        std::vector<NodeId> path;

        if (nodes_.find(from_node) == nodes_.end() ||
            nodes_.find(to_node) == nodes_.end()) {
            return path;
        }

        std::queue<NodeId> q;
        std::set<NodeId> visited;
        std::unordered_map<NodeId, NodeId> parent;

        q.push(from_node);
        visited.insert(from_node);
        parent[from_node] = "";

        bool found = false;

        while (!q.empty() && !found) {
            NodeId current = q.front();
            q.pop();

            auto it = nodes_.find(current);
            if (it != nodes_.end()) {
                for (const auto& next_id : it->second.next_nodes) {
                    if (visited.find(next_id) == visited.end()) {
                        visited.insert(next_id);
                        parent[next_id] = current;
                        q.push(next_id);

                        if (next_id == to_node) {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }

        if (found) {
            NodeId current = to_node;
            while (current != "") {
                path.push_back(current);
                auto it = parent.find(current);
                if (it == parent.end() || it->second == "") break;
                current = it->second;
            }
            std::reverse(path.begin(), path.end());
        }

        return path;
    }

    std::vector<std::vector<NodeId>> MapEngine::find_all_paths_to_boss() const {
        std::vector<std::vector<NodeId>> all_paths;

        if (layers_.empty()) return all_paths;

        NodeId start = layers_.at(0)[0];
        NodeId boss = layers_.at(total_layers_ - 1)[0];

        std::stack<std::vector<NodeId>> path_stack;
        path_stack.push({ start });

        while (!path_stack.empty()) {
            std::vector<NodeId> current_path = path_stack.top();
            path_stack.pop();

            NodeId current = current_path.back();

            if (current == boss) {
                all_paths.push_back(current_path);
                continue;
            }

            auto it = nodes_.find(current);
            if (it != nodes_.end()) {
                for (const auto& next_id : it->second.next_nodes) {
                    bool already_in_path = false;
                    for (const auto& node : current_path) {
                        if (node == next_id) {
                            already_in_path = true;
                            break;
                        }
                    }

                    if (!already_in_path) {
                        std::vector<NodeId> new_path = current_path;
                        new_path.push_back(next_id);
                        path_stack.push(new_path);
                    }
                }
            }
        }

        return all_paths;
    }

    MapSnapshot MapEngine::get_map_snapshot() const {
        MapSnapshot snapshot;
        snapshot.total_layers = total_layers_;

        for (const auto& pair : nodes_) {
            snapshot.all_nodes.push_back(pair.second);
        }

        for (const auto& pair : nodes_) {
            const MapNode& node = pair.second;
            for (const auto& next_id : node.next_nodes) {
                Edge edge;
                edge.from = node.id;
                edge.to = next_id;
                snapshot.all_edges.push_back(edge);
            }
        }

        return snapshot;
    }

    void MapEngine::set_node_visited(const NodeId& node_id) {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end()) {
            it->second.is_visited = true;
        }
    }

    void MapEngine::set_current_node(const NodeId& node_id) {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end()) {
            // 先将所有节点的 is_current 设为 false
            for (auto& pair : nodes_) {
                pair.second.is_current = false;
            }

            // 设置新的当前节点
            it->second.is_current = true;

            // ==== 添加：触发节点进入回调 ====
            if (m_nodeEnterCallback) {
                m_nodeEnterCallback(it->second);
                std::cout << "触发节点进入回调: " << node_id << std::endl;
            }
            // ==== 添加结束 ====
        }
    }

    void MapEngine::update_reachable_nodes() {
        // 先将所有节点的可达性设为 false
        for (auto& pair : nodes_) {
            pair.second.is_reachable = false;
        }

        // 找到当前节点
        NodeId currentId;
        for (const auto& pair : nodes_) {
            if (pair.second.is_current) {
                currentId = pair.first;
                break;
            }
        }

        if (currentId.empty()) return;

        // 获取当前节点的下一层节点（直接连接）
        auto& currentNode = nodes_[currentId];
        for (const auto& nextId : currentNode.next_nodes) {
            nodes_[nextId].is_reachable = true;
        }

        // 当前节点本身也是可达的（用于显示）
        nodes_[currentId].is_reachable = true;
    }

    // ==== 新增：hasCurrentNode 方法的实现 ====
    bool MapEngine::hasCurrentNode() const {
        for (const auto& pair : nodes_) {
            if (pair.second.is_current) {
                return true;
            }
        }
        return false;
    }

}