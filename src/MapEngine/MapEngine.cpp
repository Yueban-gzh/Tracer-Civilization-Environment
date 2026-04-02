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

        // 1. 确定主干：每层选一个节点作为主干节点
        std::vector<NodeId> backbone(total_layers_);
        for (int layer = 0; layer < total_layers_; ++layer) {
            const auto& ids = layers_[layer];
            if (ids.empty()) continue;

            if (layer == 0 && ids.size() > 1) {
                int idx = rand_int_fallback(run_rng_, 0, static_cast<int>(ids.size()) - 1);
                backbone[layer] = ids[idx];
            }
            else {
                backbone[layer] = ids[0];
            }
        }

        // 2. 连接主干节点（纵向路径）
        for (int layer = 0; layer < total_layers_ - 1; ++layer) {
            NodeId from = backbone[layer];
            NodeId to = backbone[layer + 1];

            bool already = false;
            for (const auto& nid : nodes_[from].next_nodes) {
                if (nid == to) { already = true; break; }
            }
            if (!already) {
                nodes_[from].next_nodes.push_back(to);
                nodes_[to].prev_nodes.push_back(from);
            }
        }

        // 3. 【关键修复】第0层特殊处理：限制连接数量和距离
        if (total_layers_ > 1) {
            const auto& start_ids = layers_[0];
            const auto& next_ids = layers_[1];

            for (const auto& start_id : start_ids) {
                if (start_id == backbone[0]) continue;

                std::vector<NodeId> candidates;
                float start_x = nodes_[start_id].position.x;

                // 【修复1】缩小距离阈值：150.0f 替代 500.0f
                for (const auto& next_id : next_ids) {
                    float next_x = nodes_[next_id].position.x;
                    if (std::abs(next_x - start_x) < 100.0f) {
                        candidates.push_back(next_id);
                    }
                }

                // 【修复2】如果近的没有，只选最近的1个，而不是全部
                if (candidates.empty()) {
                    // 找最近的那个
                    float min_dist = std::numeric_limits<float>::max();
                    NodeId nearest = next_ids[0];
                    for (const auto& next_id : next_ids) {
                        float dist = std::abs(nodes_[next_id].position.x - start_x);
                        if (dist < min_dist) {
                            min_dist = dist;
                            nearest = next_id;
                        }
                    }
                    candidates.push_back(nearest);
                }

                // 【修复3】严格限制：起点只能连1-2个，且优先1个
                int max_candidates = std::min(2, static_cast<int>(candidates.size()));
                // 70%概率连1个，30%概率连2个
                int num_conn = (rand_int_fallback(run_rng_, 0, 9) < 7) ? 1 : max_candidates;
                num_conn = std::max(1, num_conn); // 至少1个

                // 打乱
                for (int i = static_cast<int>(candidates.size()) - 1; i > 0; --i) {
                    int j = rand_int_fallback(run_rng_, 0, i);
                    std::swap(candidates[i], candidates[j]);
                }

                for (int i = 0; i < num_conn; ++i) {
                    NodeId to_id = candidates[i];

                    bool exists = false;
                    for (const auto& nid : nodes_[start_id].next_nodes) {
                        if (nid == to_id) { exists = true; break; }
                    }
                    if (!exists) {
                        nodes_[start_id].next_nodes.push_back(to_id);
                        nodes_[to_id].prev_nodes.push_back(start_id);
                    }
                }
            }
        }

        // 4. 【正向】自顶向下：确保每个节点都能从起点到达
        for (int layer = 1; layer < total_layers_; ++layer) {
            const auto& curr_ids = layers_[layer];
            const auto& prev_ids = layers_[layer - 1];

            for (const auto& curr_id : curr_ids) {
                if (!nodes_[curr_id].prev_nodes.empty()) continue;

                std::vector<NodeId> candidates;
                float curr_x = nodes_[curr_id].position.x;

                for (const auto& prev_id : prev_ids) {
                    float prev_x = nodes_[prev_id].position.x;
                    if (std::abs(prev_x - curr_x) < 500.0f) {
                        candidates.push_back(prev_id);
                    }
                }

                if (candidates.empty()) {
                    candidates = prev_ids;
                }

                int idx = rand_int_fallback(run_rng_, 0, static_cast<int>(candidates.size()) - 1);
                NodeId from_id = candidates[idx];

                bool exists = false;
                for (const auto& nid : nodes_[from_id].next_nodes) {
                    if (nid == curr_id) { exists = true; break; }
                }
                if (!exists) {
                    nodes_[from_id].next_nodes.push_back(curr_id);
                    nodes_[curr_id].prev_nodes.push_back(from_id);
                }
            }
        }

        // 5. 【反向】自底向上：确保每个节点都能到达Boss
        for (int layer = total_layers_ - 2; layer >= 0; --layer) {
            const auto& curr_ids = layers_[layer];
            const auto& next_ids = layers_[layer + 1];

            for (const auto& curr_id : curr_ids) {
                if (!nodes_[curr_id].next_nodes.empty()) continue;

                std::vector<NodeId> candidates;
                float curr_x = nodes_[curr_id].position.x;

                for (const auto& next_id : next_ids) {
                    float next_x = nodes_[next_id].position.x;
                    if (std::abs(next_x - curr_x) < 500.0f) {
                        candidates.push_back(next_id);
                    }
                }

                if (candidates.empty()) {
                    candidates = next_ids;
                }

                int idx = rand_int_fallback(run_rng_, 0, static_cast<int>(candidates.size()) - 1);
                NodeId to_id = candidates[idx];

                bool exists = false;
                for (const auto& nid : nodes_[curr_id].next_nodes) {
                    if (nid == to_id) { exists = true; break; }
                }
                if (!exists) {
                    nodes_[curr_id].next_nodes.push_back(to_id);
                    nodes_[to_id].prev_nodes.push_back(curr_id);
                }
            }
        }

        // 6. 添加额外随机连接（不包括第0层，起点已经处理过）
        for (int layer = 1; layer < total_layers_ - 1; ++layer) {
            const auto& curr_ids = layers_[layer];
            const auto& next_ids = layers_[layer + 1];

            for (const auto& from_id : curr_ids) {
                int existing = static_cast<int>(nodes_[from_id].next_nodes.size());
                if (existing >= 3) continue;

                std::vector<NodeId> candidates;
                float from_x = nodes_[from_id].position.x;

                for (const auto& to_id : next_ids) {
                    bool already = false;
                    for (const auto& nid : nodes_[from_id].next_nodes) {
                        if (nid == to_id) { already = true; break; }
                    }
                    if (already) continue;

                    float to_x = nodes_[to_id].position.x;
                    if (std::abs(to_x - from_x) < 300.0f) {
                        candidates.push_back(to_id);
                    }
                }

                int max_extra = std::min(2, static_cast<int>(candidates.size()));
                if (max_extra <= 0) continue;

                int extra = rand_int_fallback(run_rng_, 0, max_extra);

                for (int i = static_cast<int>(candidates.size()) - 1; i > 0; --i) {
                    int j = rand_int_fallback(run_rng_, 0, i);
                    std::swap(candidates[i], candidates[j]);
                }

                for (int i = 0; i < extra && existing + i < 3; ++i) {
                    NodeId to_id = candidates[i];

                    bool exists = false;
                    for (const auto& nid : nodes_[from_id].next_nodes) {
                        if (nid == to_id) { exists = true; break; }
                    }
                    if (!exists) {
                        nodes_[from_id].next_nodes.push_back(to_id);
                        nodes_[to_id].prev_nodes.push_back(from_id);
                    }
                }
            }
        }

        std::cout << "连接构建完成" << std::endl;
    }


    void MapEngine::auto_layout_nodes() {
        const float start_x = 300.0f;
        const float end_x = 1620.0f;

        // 固定每层之间的垂直间距（像素）- 增大到 120 像素
        const float LAYER_SPACING = 120.0f;

        // 第0层的起始Y坐标（底部附近）
        const float start_y = 980.0f;  // 更靠近窗口底部

        for (int layer = 0; layer < total_layers_; ++layer) {
            // 使用固定间距，layer 0 在最底部，layer 11 在最顶部
            float y = start_y - layer * LAYER_SPACING;

            const auto& node_ids = layers_[layer];
            int count = static_cast<int>(node_ids.size());

            if (count == 1) {
                // 只有一个节点，居中显示
                float x = (start_x + end_x) / 2.0f;
                for (const auto& id : node_ids) {
                    nodes_[id].position.x = x;
                    nodes_[id].position.y = y;
                }
            }
            else if (count == 2) {
                // 两个节点，左右对称分布，间距加大
                float spacing = 2000.0f;  // 水平间距增大到 300
                float center_x = (start_x + end_x) / 2.0f;
                float x1 = center_x - spacing / 2.0f;
                float x2 = center_x + spacing / 2.0f;
                std::vector<float> xs = { x1, x2 };
                int idx = 0;
                for (const auto& id : node_ids) {
                    nodes_[id].position.x = xs[idx];
                    nodes_[id].position.y = y;
                    idx++;
                }
            }
            else if (count == 3) {
                // 三个节点，均匀分布，间距加大
                float spacing = 1050.0f;  // 水平间距增大到 350
                float center_x = (start_x + end_x) / 2.0f;
                std::vector<float> xs = {
                    center_x - spacing,
                    center_x,
                    center_x + spacing
                };
                int idx = 0;
                for (const auto& id : node_ids) {
                    nodes_[id].position.x = xs[idx];
                    nodes_[id].position.y = y;
                    idx++;
                }
            }
        }

        // 微调：根据连接关系调整水平位置（让连线更自然）
        adjust_positions_by_connections();

        // 额外：确保同一层的节点之间不会重叠
        for (int layer = 0; layer < total_layers_; ++layer) {
            const auto& node_ids = layers_[layer];
            int count = static_cast<int>(node_ids.size());

            if (count >= 2) {
                // 检查并调整同一层节点的水平间距
                for (size_t i = 0; i < node_ids.size(); ++i) {
                    for (size_t j = i + 1; j < node_ids.size(); ++j) {
                        const auto& node_a = nodes_[node_ids[i]];
                        const auto& node_b = nodes_[node_ids[j]];
                        float dx = std::abs(node_a.position.x - node_b.position.x);

                        // 最小水平间距为 200 像素
                        if (dx < 250.0f) {
                            // 如果太近，向两边推开
                            float adjust = (200.0f - dx) / 2.0f;
                            nodes_[node_ids[i]].position.x -= adjust;
                            nodes_[node_ids[j]].position.x += adjust;

                            // 边界限制
                            nodes_[node_ids[i]].position.x = std::max(200.0f, std::min(1720.0f, nodes_[node_ids[i]].position.x));
                            nodes_[node_ids[j]].position.x = std::max(200.0f, std::min(1720.0f, nodes_[node_ids[j]].position.x));
                        }
                    }
                }
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
