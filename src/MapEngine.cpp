// MapEngine.cpp

#include "../../include/MapEngine/MapEngine.hpp"
#include <queue>
#include <stack>
#include <set>
#include <random>
#include <algorithm>
#include <iostream>

namespace MapEngine {

    MapEngine::MapEngine() : total_layers_(0) {
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
            return NodeType::Battle;
        }

        if (layer == total_layers - 1) {
            return NodeType::Boss;
        }

        if (roll < 40) return NodeType::Battle;
        else if (roll < 65) return NodeType::Event;
        else if (roll < 80) return NodeType::Rest;
        else return NodeType::Shop;
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
                node.id = generate_node_id(layer, i);
                node.layer = layer;

                if (!layer_types.empty() && layer < static_cast<int>(layer_types.size())) {
                    node.type = layer_types[layer];
                }
                else {
                    node.type = random_node_type(layer, layers);
                }

                node.content_id = "content_" + std::to_string(static_cast<int>(node.type))
                    + "_" + node.id;

                node.x = 100.0f + i * 200.0f;
                node.y = 100.0f + layer * 100.0f;

                nodes_[node.id] = node;
                layers_[layer].push_back(node.id);
            }
        }

        build_connections();

        if (!validate_path_exists()) {
            std::cerr << "错误：起点到Boss不可达，重新生成..." << std::endl;
            init_map(layers, nodes_per_layer, layer_types);
            return;
        }

        std::cout << "地图生成完成！节点总数：" << nodes_.size() << std::endl;
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

        NodeId start = layers_[0][0];
        NodeId end = layers_[total_layers_ - 1][0];

        return is_reachable(start, end);
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

}