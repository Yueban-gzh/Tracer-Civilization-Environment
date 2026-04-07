// src/MapEngine/MapEngine.cpp
#include "../../include/MapEngine/MapEngine.hpp" 
#include "../../include/MapEngine/MapConfig.hpp"
#include "../../include/Common/RunRng.hpp"
#include <queue>
#include <stack>
#include <set>
#include <random>
#include <algorithm>
#include <iostream>
#include <map>

namespace MapEngine {

    namespace {

        int rand_int_fallback(tce::RunRng* rng, int lo, int hi) {
            if (rng) return rng->uniform_int(lo, hi);
            static thread_local std::mt19937 gen{ std::random_device{}() };
            return std::uniform_int_distribution<int>(lo, hi)(gen);
        }

        void shuffle_nodes_fallback(tce::RunRng* rng, std::vector<NodeId>& v) {
            if (rng) std::shuffle(v.begin(), v.end(), *rng);
            else {
                static thread_local std::mt19937 gen{ std::random_device{}() };
                std::shuffle(v.begin(), v.end(), gen);
            }
        }

    } // namespace

        // ==== 构造函数初始化所有成员 ====
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
        int roll = rand_int_fallback(run_rng_, 0, 100);

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

        std::cout << "开始生成地图" << layers << " 层，每层约 "
            << nodes_per_layer << " 个节点" << std::endl;

        // 生成所有节点
        for (int layer = 0; layer < layers; ++layer) {
            int node_count = rand_int_fallback(run_rng_, 1, nodes_per_layer);

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

                // ==== 使用生成器填充 content_id ====
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
            std::cerr << "错误：起点到boss不可达，重新生成" << std::endl;
            this->init_map(layers, nodes_per_layer, layer_types);
            return;
        }

        std::cout << "地图生成完成，节点总数" << nodes_.size() << std::endl;
    }

    void MapEngine::init_fixed_map(const MapConfig& config) {
        nodes_.clear();
        layers_.clear();

        auto layerTypes = config.getLayerTypes();
        auto positions = config.getNodePositions();

        total_layers_ = static_cast<int>(layerTypes.size());

        std::cout << "初始化地图" << config.getName() << std::endl;
        std::cout << "描述" << config.getDescription() << std::endl;
        std::cout << "共" << total_layers_ << "层" << std::endl;

        // 创建节点
        for (int layer = 0; layer < total_layers_; ++layer) {
            const auto& types = layerTypes[layer];
            const auto& pos = positions[layer];

            for (size_t i = 0; i < types.size(); ++i) {
                MapNode node;
                node.id = this->generate_node_id(layer, static_cast<int>(i));
                node.layer = layer;
                node.type = static_cast<NodeType>(types[i]);

                // ==== 使用生成器填充 content_id ====
                if (m_contentIdGenerator) {
                    node.content_id = m_contentIdGenerator(node.type, layer, static_cast<int>(i));
                }
                else {
                    node.content_id = "content_" + std::to_string(static_cast<int>(node.type)) + "_" + node.id;
                    std::cout << "未设置内容ID生成器，使用默认ID" << node.content_id << std::endl;
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

        // 构成连接
        auto connections = config.getConnections();
        this->build_fixed_connections(connections);

        std::cout << "地图初始化完成，节点总数" << nodes_.size() << std::endl;
    }

    void MapEngine::build_fixed_connections(
        const std::vector<std::vector<std::pair<int, int>>>& connections) {

        for (int layer = 0; layer < static_cast<int>(connections.size()); ++layer) {
            const auto& layerConnections = connections[layer];

            std::cout << "处理第 " << layer << " 层到第 " << (layer + 1) << " 层的连接" << std::endl;

            for (const auto& conn : layerConnections) {
                int fromIdx = conn.first;
                int toIdx = conn.second;

                NodeId fromId = this->generate_node_id(layer, fromIdx);
                NodeId toId = this->generate_node_id(layer + 1, toIdx);

                // 检查节点是否存在
                if (nodes_.find(fromId) == nodes_.end()) {
                    std::cerr << "错误：源节点" << fromId << std::endl;
                    continue;
                }
                if (nodes_.find(toId) == nodes_.end()) {
                    std::cerr << "错误：目标节点" << toId << std::endl;
                    continue;
                }

                // 添加连接
                nodes_[fromId].next_nodes.push_back(toId);
                nodes_[toId].prev_nodes.push_back(fromId);

                std::cout << "  连接 " << fromId << " -> " << toId << std::endl;
            }
        }
    }

    void MapEngine::build_connections() {
        for (int layer = 0; layer < total_layers_ - 1; ++layer) {
            const auto& current_layer_ids = layers_[layer];
            const auto& next_layer_ids = layers_[layer + 1];

            for (const auto& curr_id : current_layer_ids) {
                int connection_count = std::min(rand_int_fallback(run_rng_, 1, 2),
                    static_cast<int>(next_layer_ids.size()));

                std::vector<NodeId> targets = next_layer_ids;
                shuffle_nodes_fallback(run_rng_, targets);

                for (int i = 0; i < connection_count; ++i) {
                    NodeId target_id = targets[i];

                    nodes_[curr_id].next_nodes.push_back(target_id);
                    nodes_[target_id].prev_nodes.push_back(curr_id);
                }
            }

            for (const auto& next_id : next_layer_ids) {
                if (nodes_[next_id].prev_nodes.empty()) {
                    int random_idx = rand_int_fallback(run_rng_, 0,
                        static_cast<int>(current_layer_ids.size()) - 1);
                    NodeId source_id = current_layer_ids[static_cast<size_t>(random_idx)];

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

    void MapEngine::restore_from_snapshot(const MapSnapshot& snapshot) {
        nodes_.clear();
        layers_.clear();
        total_layers_ = snapshot.total_layers > 0 ? snapshot.total_layers : 12;

        for (const MapNode& n : snapshot.all_nodes) {
            nodes_[n.id] = n;
        }

        for (const auto& pair : nodes_) {
            const MapNode& n = pair.second;
            if (n.layer >= 0)
                layers_[n.layer].push_back(n.id);
        }
        for (auto& kv : layers_) {
            auto& ids = kv.second;
            std::sort(ids.begin(), ids.end());
        }

        if (total_layers_ <= 0 && !layers_.empty()) {
            int maxL = 0;
            for (const auto& kv : layers_)
                maxL = std::max(maxL, kv.first);
            total_layers_ = maxL + 1;
        }
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
            // 清除所有节点的当前状态
            for (auto& pair : nodes_) {
                pair.second.is_current = false;
            }

            // 设置新当前节点
            it->second.is_current = true;
            it->second.is_visited = true;      // 标记为已访问
            it->second.is_completed = true;    // 【新增】标记为已完成

            // 触发回调（仅第一次进入时触发，重复点击同一节点不会重复触发）
            if (m_nodeEnterCallback) {
                m_nodeEnterCallback(it->second);
                std::cout << "触发节点进入回调: " << node_id << std::endl;
            }
        }
    }

    void MapEngine::update_reachable_nodes() {
        // 所有节点先设为不可达
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

        // 【修改】当前节点本身设为可达（用于显示当前位置），但已完成节点不能作为"可进入"的目标
        nodes_[currentId].is_reachable = true;

        // 设置下一层节点为可达（排除已完成的）
        auto& currentNode = nodes_[currentId];
        for (const auto& nextId : currentNode.next_nodes) {
            // 【关键】已完成的节点不再可达，防止重复进入
            if (!nodes_[nextId].is_completed) {
                nodes_[nextId].is_reachable = true;
            }
        }
    }

    // ==== hasCurrentNode方法的实现 ====
    bool MapEngine::hasCurrentNode() const {
        for (const auto& pair : nodes_) {
            if (pair.second.is_current) {
                return true;
            }
        }
        return false;
    }

}


// ========== 添加到 MapEngine.cpp 末尾 ==========

namespace MapEngine {

    // 辅助函数：按权重随机选择
    static NodeType random_by_weight(tce::RunRng* rng) {
        // 权重：怪物3，事件1.5，火堆1.5，商店1
        // 放大10倍变成整数：30, 15, 15, 10
        int roll = rand_int_fallback(rng, 0, 69);  // 30+15+15+10-1 = 69

        if (roll < 30) return NodeType::Enemy;      // 怪物
        if (roll < 45) return NodeType::Event;      // 事件
        if (roll < 60) return NodeType::Rest;       // 火堆
        return NodeType::Merchant;                   // 商店
    }

    // 将怪物类型转换为 Enemy 或 Elite（整张图最多一个精英）
    static NodeType get_monster_type(tce::RunRng* rng, bool& has_elite) {
        if (!has_elite) {
            // 约1/7概率出精英
            int roll = rand_int_fallback(rng, 0, 6);
            if (roll == 0) {
                has_elite = true;
                return NodeType::Elite;
            }
        }
        return NodeType::Enemy;
    }

    void MapEngine::init_random_map(int map_index) {
        // 添加范围检查
        if (map_index < 0 || map_index >= 3) {
            std::cerr << "错误：无效的地图索引 " << map_index << "，重置为 0" << std::endl;
            map_index = 0;
        }
        nodes_.clear();
        layers_.clear();

        // 地图主题名称
        const char* map_names[] = { "标准地图", "森林地图", "沙漠地图" };
        const char* map_descs[] = { "12层随机地图", "12层随机地图", "12层随机地图" };

        total_layers_ = 12;

        std::cout << "===== 生成" << map_names[map_index] << " =====" << std::endl;
        std::cout << "描述: " << map_descs[map_index] << std::endl;
        std::cout << "层数: " << total_layers_ << std::endl;

        // 记录是否已经生成精英
        bool has_elite = false;

        // 为每一层生成节点
        for (int layer = 0; layer < total_layers_; ++layer) {
            int node_count;

            // layer 0: 节点数随机1~3
            if (layer == 0) {
                node_count = rand_int_fallback(run_rng_, 1, 3);
            }
            // layer 5: 宝箱层，节点数随机1~3
            else if (layer == 5) {
                node_count = rand_int_fallback(run_rng_, 1, 3);
            }
            // layer 9,10,11: 固定怪、火堆、Boss，每层1个节点
            else if (layer == 9 || layer == 10 || layer == 11) {
                node_count = 1;
            }
            // 其他层：随机1~3个节点
            else {
                node_count = rand_int_fallback(run_rng_, 1, 3);
            }

            std::cout << "  第" << layer << "层: " << node_count << "个节点" << std::endl;

            // 生成本层的节点
            for (int idx = 0; idx < node_count; ++idx) {
                MapNode node;
                node.id = generate_node_id(layer, idx);
                node.layer = layer;

                // 根据层确定节点类型
                if (layer == 0) {
                    node.type = NodeType::Enemy;
                }
                else if (layer == 5) {
                    if (idx == 0) {
                        node.type = NodeType::Treasure;
                    }
                    else {
                        node.type = random_by_weight(run_rng_);
                        if (node.type == NodeType::Enemy) {
                            node.type = get_monster_type(run_rng_, has_elite);
                        }
                    }
                }
                else if (layer == 9) {
                    node.type = NodeType::Enemy;
                }
                else if (layer == 10) {
                    node.type = NodeType::Rest;
                }
                else if (layer == 11) {
                    node.type = NodeType::Boss;
                }
                else {
                    node.type = random_by_weight(run_rng_);
                    if (node.type == NodeType::Enemy) {
                        node.type = get_monster_type(run_rng_, has_elite);
                    }
                }

                // 生成 content_id
                if (m_contentIdGenerator) {
                    node.content_id = m_contentIdGenerator(node.type, layer, idx);
                }
                else {
                    node.content_id = "content_" + std::to_string(static_cast<int>(node.type)) + "_" + node.id;
                }

                // 临时位置（后续会被 auto_layout_nodes 重新计算）
                node.position.x = 200.0f + idx * 300.0f;
                node.position.y = 100.0f + layer * 80.0f;

                node.is_visited = false;
                node.is_current = false;
                node.is_reachable = (layer == 0);

                nodes_[node.id] = node;
                layers_[layer].push_back(node.id);

                std::cout << "    " << node.id << ": ";
                switch (node.type) {
                case NodeType::Enemy: std::cout << "普通怪"; break;
                case NodeType::Elite: std::cout << "精英怪"; break;
                case NodeType::Event: std::cout << "事件"; break;
                case NodeType::Rest: std::cout << "火堆"; break;
                case NodeType::Merchant: std::cout << "商店"; break;
                case NodeType::Treasure: std::cout << "宝箱"; break;
                case NodeType::Boss: std::cout << "Boss"; break;
                default: std::cout << "未知"; break;
                }
                std::cout << std::endl;
            }
        }

        // 构建连接
        build_random_connections();

        // 自动布局节点位置
        auto_layout_nodes();

        // 验证路径
        if (!validate_path_exists()) {
            std::cerr << "警告：路径验证失败，重新生成连接..." << std::endl;
            build_random_connections();
            auto_layout_nodes();
        }

        std::cout << "地图生成完成，共 " << nodes_.size() << " 个节点" << std::endl;
    }

    void MapEngine::build_random_connections() {
        // 清空所有现有连接
        for (auto& pair : nodes_) {
            pair.second.next_nodes.clear();
            pair.second.prev_nodes.clear();
        }

        // 对每一层的节点按X坐标排序，确定"相邻"关系
        for (int layer = 0; layer < total_layers_; ++layer) {
            auto& ids = layers_[layer];
            std::sort(ids.begin(), ids.end(), [this](const NodeId& a, const NodeId& b) {
                return nodes_[a].position.x < nodes_[b].position.x;
                });
        }

        // 1. 构建主干路径（从最左侧开始，每层连接到下一层最近的节点）
        NodeId current = layers_[0][0]; // 第0层最左节点
        for (int layer = 0; layer < total_layers_ - 1; ++layer) {
            const auto& next_ids = layers_[layer + 1];

            // 找到下一层中X坐标最接近当前节点的节点
            NodeId closest = next_ids[0];
            float min_dist = std::abs(nodes_[next_ids[0]].position.x - nodes_[current].position.x);

            for (const auto& nid : next_ids) {
                float dist = std::abs(nodes_[nid].position.x - nodes_[current].position.x);
                if (dist < min_dist) {
                    min_dist = dist;
                    closest = nid;
                }
            }

            // 添加连接
            nodes_[current].next_nodes.push_back(closest);
            nodes_[closest].prev_nodes.push_back(current);
            current = closest;
        }

        // 2. 为每个节点添加额外连接（只连接相邻的1-2个节点）
        for (int layer = 0; layer < total_layers_ - 1; ++layer) {
            const auto& curr_ids = layers_[layer];
            const auto& next_ids = layers_[layer + 1];

            for (const auto& from_id : curr_ids) {
                // 计算当前节点在下一层中的最佳连接目标（按X距离排序）
                std::vector<std::pair<float, NodeId>> candidates;
                for (const auto& to_id : next_ids) {
                    // 避免重复连接
                    bool already = false;
                    for (const auto& existing : nodes_[from_id].next_nodes) {
                        if (existing == to_id) { already = true; break; }
                    }
                    if (already) continue;

                    float dist = std::abs(nodes_[to_id].position.x - nodes_[from_id].position.x);
                    candidates.push_back({ dist, to_id });
                }

                // 按距离排序
                std::sort(candidates.begin(), candidates.end());

                // 只连接最近的1-2个，且距离不能超过阈值（如150像素）
                const float MAX_DIST = 150.0f;
                int max_connections = rand_int_fallback(run_rng_, 1, 2); // 随机1-2个

                for (int i = 0; i < std::min(max_connections, (int)candidates.size()); ++i) {
                    if (candidates[i].first > MAX_DIST) break; // 太远了，不连

                    NodeId to_id = candidates[i].second;
                    nodes_[from_id].next_nodes.push_back(to_id);
                    nodes_[to_id].prev_nodes.push_back(from_id);
                }
            }
        }

        // 3. 确保连通性（没有前驱的节点必须连接到至少一个前一层节点）
        for (int layer = 1; layer < total_layers_; ++layer) {
            for (const auto& curr_id : layers_[layer]) {
                if (nodes_[curr_id].prev_nodes.empty()) {
                    // 找到前一层最近的节点
                    const auto& prev_ids = layers_[layer - 1];
                    NodeId nearest = prev_ids[0];
                    float min_dist = std::abs(nodes_[prev_ids[0]].position.x - nodes_[curr_id].position.x);

                    for (const auto& prev_id : prev_ids) {
                        float dist = std::abs(nodes_[prev_id].position.x - nodes_[curr_id].position.x);
                        if (dist < min_dist) {
                            min_dist = dist;
                            nearest = prev_id;
                        }
                    }

                    nodes_[nearest].next_nodes.push_back(curr_id);
                    nodes_[curr_id].prev_nodes.push_back(nearest);
                }
            }
        }

        // 4. 确保能到达终点（没有后继的节点必须连接到下一层）
        for (int layer = 0; layer < total_layers_ - 1; ++layer) {
            for (const auto& curr_id : layers_[layer]) {
                if (nodes_[curr_id].next_nodes.empty()) {
                    const auto& next_ids = layers_[layer + 1];
                    NodeId nearest = next_ids[0];
                    float min_dist = std::abs(nodes_[next_ids[0]].position.x - nodes_[curr_id].position.x);

                    for (const auto& next_id : next_ids) {
                        float dist = std::abs(nodes_[next_id].position.x - nodes_[curr_id].position.x);
                        if (dist < min_dist) {
                            min_dist = dist;
                            nearest = next_id;
                        }
                    }

                    nodes_[curr_id].next_nodes.push_back(nearest);
                    nodes_[nearest].prev_nodes.push_back(curr_id);
                }
            }
        }
    }


    // src/MapEngine/MapEngine.cpp
    // 替换 auto_layout_nodes 函数，增大节点间距

    void MapEngine::auto_layout_nodes() {
        const float start_x = 200.0f;   // 左边界缩小，给更多空间
        const float end_x = 1720.0f;    // 右边界扩大

        // 固定每层之间的垂直间距（像素）
        const float LAYER_SPACING = 120.0f;
        // 【新增】Boss层额外间距（让Boss更高）
        const float BOSS_EXTRA_SPACING = 100.0f;  // 额外增加100像素，总共220

        // 第0层的起始Y坐标（底部附近）
        const float start_y = 980.0f;

        // 节点之间的最小水平间距（增大到250像素，更宽松）
        const float MIN_HORIZONTAL_SPACING = 250.0f;

        // 可用水平范围（扩大可用区域）
        const float AVAILABLE_WIDTH = end_x - start_x;  // 1520 像素

        for (int layer = 0; layer < total_layers_; ++layer) {
            // 【修改】计算Y坐标，Boss层使用更大的间距
            float y;
            if (layer == total_layers_ - 1) {
                // Boss层：使用更大的间距（120 + 100 = 220）
                y = start_y - (layer - 1) * LAYER_SPACING - (LAYER_SPACING + BOSS_EXTRA_SPACING);
            }
            else {
                // 普通层：正常间距
                y = start_y - layer * LAYER_SPACING;
            }

            const auto& node_ids = layers_[layer];
            int count = static_cast<int>(node_ids.size());

            if (count == 0) continue;

            std::vector<float> x_positions;

            if (count == 1) {
                // 单个节点居中
                float x = (start_x + end_x) / 2.0f;
                x_positions.push_back(x);
            }
            else if (count == 2) {
                // 两个节点：对称分布，间距600像素（更宽松）
                float spacing = 600.0f;
                float center_x = (start_x + end_x) / 2.0f;
                float x1 = center_x - spacing / 2.0f;
                float x2 = center_x + spacing / 2.0f;

                // 边界保护
                x1 = std::max(start_x, std::min(end_x, x1));
                x2 = std::max(start_x, std::min(end_x, x2));

                x_positions.push_back(x1);
                x_positions.push_back(x2);
            }
            else if (count == 3) {
                // 三个节点：均匀分布，总宽度1000像素（更宽松）
                float total_width = 1000.0f;
                float start_pos = (start_x + end_x - total_width) / 2.0f;
                float spacing = total_width / (count - 1);  // 500像素间距

                for (int i = 0; i < count; ++i) {
                    float x = start_pos + i * spacing;
                    x = std::max(start_x, std::min(end_x, x));
                    x_positions.push_back(x);
                }
            }
            else if (count == 4) {
                // 四个节点：均匀分布，总宽度1200像素
                float total_width = 1200.0f;
                float start_pos = (start_x + end_x - total_width) / 2.0f;
                float spacing = total_width / (count - 1);  // 400像素间距

                for (int i = 0; i < count; ++i) {
                    float x = start_pos + i * spacing;
                    x = std::max(start_x, std::min(end_x, x));
                    x_positions.push_back(x);
                }
            }
            else {
                // 5个或更多节点：动态计算，确保最小间距250像素
                float max_total_width = std::min(AVAILABLE_WIDTH, static_cast<float>(count) * MIN_HORIZONTAL_SPACING);
                float start_pos = (start_x + end_x - max_total_width) / 2.0f;
                float spacing = max_total_width / (count - 1);

                for (int i = 0; i < count; ++i) {
                    float x = start_pos + i * spacing;
                    x = std::max(start_x, std::min(end_x, x));
                    x_positions.push_back(x);
                }
            }

            // 应用X坐标
            for (int i = 0; i < count; ++i) {
                nodes_[node_ids[i]].position.x = x_positions[i];
                nodes_[node_ids[i]].position.y = y;
            }
        }

        // 微调：根据连接关系调整水平位置（降低权重，避免破坏间距）
        adjust_positions_by_connections();

        // 额外：强制确保同一层的节点之间满足最小间距（增大到220像素）
        const float MIN_SPACING = 220.0f;
        for (int layer = 0; layer < total_layers_; ++layer) {
            const auto& node_ids = layers_[layer];
            int count = static_cast<int>(node_ids.size());

            if (count < 2) continue;

            // 多次迭代直到满足最小间距
            for (int iteration = 0; iteration < 15; ++iteration) {
                bool adjusted = false;

                // 按X坐标排序，便于处理
                std::vector<std::pair<float, NodeId>> sorted_nodes;
                for (const auto& id : node_ids) {
                    sorted_nodes.push_back({ nodes_[id].position.x, id });
                }
                std::sort(sorted_nodes.begin(), sorted_nodes.end());

                for (int i = 0; i < count - 1; ++i) {
                    float& x_left = nodes_[sorted_nodes[i].second].position.x;
                    float& x_right = nodes_[sorted_nodes[i + 1].second].position.x;
                    float dx = x_right - x_left;

                    if (dx < MIN_SPACING) {
                        adjusted = true;
                        float adjust = (MIN_SPACING - dx) / 2.0f;
                        x_left -= adjust;
                        x_right += adjust;

                        // 边界限制
                        x_left = std::max(start_x, std::min(end_x, x_left));
                        x_right = std::max(start_x, std::min(end_x, x_right));
                    }
                }

                if (!adjusted) break;
            }
        }
    }


    void MapEngine::adjust_positions_by_connections() {
        for (int iter = 0; iter < 3; ++iter) {
            // 从下往上调整
            for (int layer = total_layers_ - 2; layer >= 0; --layer) {
                const auto& node_ids = layers_[layer];

                for (const auto& parent_id : node_ids) {
                    const auto& children = nodes_[parent_id].next_nodes;
                    if (children.empty()) continue;

                    float sum_x = 0.0f;
                    for (const auto& child_id : children) {
                        sum_x += nodes_[child_id].position.x;
                    }
                    float avg_child_x = sum_x / children.size();

                    float parent_x = nodes_[parent_id].position.x;
                    float new_x = parent_x * 0.6f + avg_child_x * 0.4f;

                    new_x = std::max(200.0f, std::min(1720.0f, new_x));
                    nodes_[parent_id].position.x = new_x;
                }
            }

            // 从上往下调整
            for (int layer = 1; layer < total_layers_; ++layer) {
                const auto& node_ids = layers_[layer];

                for (const auto& child_id : node_ids) {
                    const auto& parents = nodes_[child_id].prev_nodes;
                    if (parents.empty()) continue;

                    float sum_x = 0.0f;
                    for (const auto& parent_id : parents) {
                        sum_x += nodes_[parent_id].position.x;
                    }
                    float avg_parent_x = sum_x / parents.size();

                    float child_x = nodes_[child_id].position.x;
                    float new_x = child_x * 0.6f + avg_parent_x * 0.4f;

                    new_x = std::max(200.0f, std::min(1720.0f, new_x));
                    nodes_[child_id].position.x = new_x;
                }
            }
        }
    }

} // namespace MapEngine
