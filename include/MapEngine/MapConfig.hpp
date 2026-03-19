//MapConfig.hpp
#pragma once
#include "../Common/NodeTypes.hpp"
#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <iostream>

namespace MapEngine {

    // 地图配置基类
    class MapConfig {
    public:
        virtual ~MapConfig() = default;
        virtual std::vector<std::vector<NodeType>> getLayerTypes() const = 0;
        virtual std::vector<std::vector<Vector2>> getNodePositions() const = 0;
        virtual std::vector<std::vector<std::pair<int, int>>> getConnections() const = 0;
        virtual std::string getName() const = 0;
        virtual std::string getDescription() const = 0;
        virtual int getLayerCount() const = 0;
    };

    // 地图1：标准地图 - 6层
    class MapConfig1 : public MapConfig {
    public:
        int getLayerCount() const override { return 6; }

        std::vector<std::vector<NodeType>> getLayerTypes() const override {
            std::vector<std::vector<NodeType>> layers;

            // 第0层 - 3个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Enemy
                });

            // 第1层 - 4个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Elite,    // 新增精英节点
                NodeType::Rest
                });

            // 第2层 - 4个节点
            layers.push_back({
                NodeType::Event,
                NodeType::Elite,     // 新增精英节点
                NodeType::Merchant,
                NodeType::Rest
                });

            // 第3层 - 4个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Treasure,  // 新增宝藏节点
                NodeType::Rest
                });

            // 第4层 - 3个节点
            layers.push_back({
                NodeType::Merchant,
                NodeType::Elite,      // 新增精英节点
                NodeType::Treasure    // 新增宝藏节点
                });

            // 第5层 - Boss
            layers.push_back({ NodeType::Boss });

            return layers;
        }

        // MapConfig1 的完整修改
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // 第0层 - 原来900左右，改为1000
            positions.push_back({
                Vector2{450.f, 1100.f},  // Y: 900 -> 1000
                Vector2{950.f, 1070.f},  // Y: 870 -> 970
                Vector2{1550.f, 1130.f}  // Y: 880 -> 980
                });

            // 第1层 - 原来750左右，改为850
            positions.push_back({
                Vector2{250.f, 950.f},   // Y: 750 -> 850
                Vector2{700.f, 900.f},   // Y: 730 -> 830
                Vector2{1150.f, 940.f},  // Y: 740 -> 840
                Vector2{1600.f, 920.f}   // Y: 720 -> 820
                });

            // 第2层 - 原来600左右，改为700
            positions.push_back({
                Vector2{300.f, 700.f},   // Y: 600 -> 700
                Vector2{750.f, 730.f},   // Y: 580 -> 680
                Vector2{1200.f, 690.f},  // Y: 590 -> 690
                Vector2{1500.f, 700.f}   // Y: 570 -> 670
                });

            // 第3层 - 原来450左右，改为550
            positions.push_back({
                Vector2{350.f, 550.f},   // Y: 450 -> 550
                Vector2{800.f, 530.f},   // Y: 430 -> 530
                Vector2{1250.f, 540.f},  // Y: 440 -> 540
                Vector2{1700.f, 520.f}   // Y: 420 -> 520
                });

            // 第4层 - 原来300左右，改为400
            positions.push_back({
                Vector2{450.f, 400.f},   // Y: 300 -> 400
                Vector2{960.f, 380.f},   // Y: 280 -> 380
                Vector2{1550.f, 390.f}   // Y: 290 -> 390
                });

            // 第5层 - Boss - 原来150，改为250
            positions.push_back({ Vector2{960.f, 150.f} });  // Y: 150 -> 250

            return positions;
        }

        std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
            std::vector<std::vector<std::pair<int, int>>> connections;

            // 第0层(3) -> 第1层(4)
            std::vector<std::pair<int, int>> layer0to1;
            layer0to1.push_back(std::make_pair(0, 0));
            layer0to1.push_back(std::make_pair(0, 1));
            layer0to1.push_back(std::make_pair(1, 1));
            layer0to1.push_back(std::make_pair(1, 2));
            layer0to1.push_back(std::make_pair(2, 2));
            layer0to1.push_back(std::make_pair(2, 3));
            connections.push_back(layer0to1);

            // 第1层(4) -> 第2层(4)
            std::vector<std::pair<int, int>> layer1to2;
            layer1to2.push_back(std::make_pair(0, 0));
            layer1to2.push_back(std::make_pair(0, 1));
            layer1to2.push_back(std::make_pair(1, 1));
            layer1to2.push_back(std::make_pair(1, 2));
            layer1to2.push_back(std::make_pair(2, 2));
            layer1to2.push_back(std::make_pair(2, 3));
            layer1to2.push_back(std::make_pair(3, 3));
            connections.push_back(layer1to2);

            // 第2层(4) -> 第3层(4)
            std::vector<std::pair<int, int>> layer2to3;
            layer2to3.push_back(std::make_pair(0, 0));
            layer2to3.push_back(std::make_pair(1, 0));
            layer2to3.push_back(std::make_pair(1, 1));
            layer2to3.push_back(std::make_pair(2, 1));
            layer2to3.push_back(std::make_pair(2, 2));
            layer2to3.push_back(std::make_pair(3, 2));
            layer2to3.push_back(std::make_pair(3, 3));
            connections.push_back(layer2to3);

            // 第3层(4) -> 第4层(3)
            std::vector<std::pair<int, int>> layer3to4;
            layer3to4.push_back(std::make_pair(0, 0));
            layer3to4.push_back(std::make_pair(1, 0));
            layer3to4.push_back(std::make_pair(1, 1));
            layer3to4.push_back(std::make_pair(2, 1));
            layer3to4.push_back(std::make_pair(2, 2));
            layer3to4.push_back(std::make_pair(3, 2));
            connections.push_back(layer3to4);

            // 第4层(3) -> 第5层(1)
            std::vector<std::pair<int, int>> layer4to5;
            layer4to5.push_back(std::make_pair(0, 0));
            layer4to5.push_back(std::make_pair(1, 0));
            layer4to5.push_back(std::make_pair(2, 0));
            connections.push_back(layer4to5);

            return connections;
        }

        std::string getName() const override { return "标准地图"; }
        std::string getDescription() const override { return "6层地图，包含所有节点类型"; }
    };

    // 地图2：森林主题 - 5层
    class MapConfig2 : public MapConfig {
    public:
        int getLayerCount() const override { return 5; }

        std::vector<std::vector<NodeType>> getLayerTypes() const override {
            std::vector<std::vector<NodeType>> layers;

            // 第0层 - 3个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Enemy
                });

            // 第1层 - 4个节点
            layers.push_back({
                NodeType::Event,
                NodeType::Elite,     // 精英节点
                NodeType::Rest,
                NodeType::Treasure    // 宝藏节点
                });

            // 第2层 - 4个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Merchant,
                NodeType::Elite,      // 精英节点
                NodeType::Rest
                });

            // 第3层 - 3个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Treasure,   // 宝藏节点
                NodeType::Elite        // 精英节点
                });

            // 第4层 - Boss
            layers.push_back({ NodeType::Boss });

            return layers;
        }

        // MapConfig2 的完整修改
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // 第0层 - 3个节点，水平分散
            positions.push_back({
                Vector2{400.f, 1100.f},   // 左
                Vector2{900.f, 1070.f},   // 中
                Vector2{1500.f, 1100.f}   // 右
                });

            // 第1层 - 4个节点
            positions.push_back({
                Vector2{200.f, 850.f},   // 左1
                Vector2{650.f, 850.f},   // 左2
                Vector2{1100.f, 830.f},  // 右2
                Vector2{1550.f, 880.f}   // 右1
                });

            // 第2层 - 4个节点
            positions.push_back({
                Vector2{250.f, 650.f},   // 左1
                Vector2{700.f, 620.f},   // 左2
                Vector2{1150.f, 630.f},  // 右2
                Vector2{1600.f, 610.f}   // 右1
                });

            // 第3层 - 3个节点
            positions.push_back({
                Vector2{400.f, 400.f},   // 左
                Vector2{950.f, 450.f},   // 中
                Vector2{1500.f, 420.f}   // 右
                });

            // 第4层 - Boss
            positions.push_back({ Vector2{960.f, 170.f} });

            return positions;
        }

        std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
            std::vector<std::vector<std::pair<int, int>>> connections;

            // 第0层(3) -> 第1层(4)
            std::vector<std::pair<int, int>> layer0to1;
            layer0to1.push_back(std::make_pair(0, 0));
            layer0to1.push_back(std::make_pair(0, 1));
            layer0to1.push_back(std::make_pair(1, 1));
            layer0to1.push_back(std::make_pair(1, 2));
            layer0to1.push_back(std::make_pair(2, 2));
            layer0to1.push_back(std::make_pair(2, 3));
            connections.push_back(layer0to1);

            // 第1层(4) -> 第2层(4)
            std::vector<std::pair<int, int>> layer1to2;
            layer1to2.push_back(std::make_pair(0, 0));
            layer1to2.push_back(std::make_pair(1, 0));
            layer1to2.push_back(std::make_pair(1, 1));
            layer1to2.push_back(std::make_pair(2, 1));
            layer1to2.push_back(std::make_pair(2, 2));
            layer1to2.push_back(std::make_pair(3, 2));
            layer1to2.push_back(std::make_pair(3, 3));
            connections.push_back(layer1to2);

            // 第2层(4) -> 第3层(3)
            std::vector<std::pair<int, int>> layer2to3;
            layer2to3.push_back(std::make_pair(0, 0));
            layer2to3.push_back(std::make_pair(1, 0));
            layer2to3.push_back(std::make_pair(1, 1));
            layer2to3.push_back(std::make_pair(2, 1));
            layer2to3.push_back(std::make_pair(2, 2));
            layer2to3.push_back(std::make_pair(3, 2));
            connections.push_back(layer2to3);

            // 第3层(3) -> 第4层(1)
            std::vector<std::pair<int, int>> layer3to4;
            layer3to4.push_back(std::make_pair(0, 0));
            layer3to4.push_back(std::make_pair(1, 0));
            layer3to4.push_back(std::make_pair(2, 0));
            connections.push_back(layer3to4);

            return connections;
        }

        std::string getName() const override { return "森林地图"; }
        std::string getDescription() const override { return "5层地图，事件和宝藏节点较多"; }
    };

    // 地图3：沙漠主题 - 6层
    class MapConfig3 : public MapConfig {
    public:
        int getLayerCount() const override { return 6; }

        std::vector<std::vector<NodeType>> getLayerTypes() const override {
            std::vector<std::vector<NodeType>> layers;

            // 第0层 - 4个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Enemy
                });

            // 第1层 - 4个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Rest,
                NodeType::Elite,     // 精英节点
                NodeType::Event
                });

            // 第2层 - 4个节点
            layers.push_back({
                NodeType::Event,
                NodeType::Elite,      // 精英节点
                NodeType::Merchant,
                NodeType::Enemy
                });

            // 第3层 - 4个节点
            layers.push_back({
                NodeType::Enemy,
                NodeType::Rest,
                NodeType::Treasure,   // 宝藏节点
                NodeType::Event
                });

            // 第4层 - 3个节点
            layers.push_back({
                NodeType::Merchant,
                NodeType::Elite,       // 精英节点
                NodeType::Treasure      // 宝藏节点
                });

            // 第5层 - Boss
            layers.push_back({ NodeType::Boss });

            return layers;
        }

        // MapConfig3 的完整修改
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // 第0层 - 4个节点，从最左到最右均匀分布
            positions.push_back({
                Vector2{250.f, 1200.f},   // 左1
                Vector2{650.f, 1170.f},   // 左2
                Vector2{1050.f, 1160.f},  // 右2
                Vector2{1450.f, 1220.f}   // 右1
                });

            // 第1层 - 4个节点
            positions.push_back({
                Vector2{300.f, 950.f},   // 左1
                Vector2{700.f, 930.f},   // 左2
                Vector2{1100.f, 960.f},  // 右2
                Vector2{1500.f, 920.f}   // 右1
                });

            // 第2层 - 4个节点
            positions.push_back({
                Vector2{200.f, 770.f},   // 左1
                Vector2{750.f, 780.f},   // 左2
                Vector2{1150.f, 790.f},  // 右2
                Vector2{1700.f, 770.f}   // 右1
                });

            // 第3层 - 4个节点
            positions.push_back({
                Vector2{400.f, 620.f},   // 左1
                Vector2{800.f, 580.f},   // 左2
                Vector2{1200.f, 610.f},  // 右2
                Vector2{1600.f, 550.f}   // 右1
                });

            // 第4层 - 3个节点
            positions.push_back({
                Vector2{450.f, 400.f},   // 左
                Vector2{1000.f, 400.f},  // 中
                Vector2{1550.f, 380.f}   // 右
                });

            // 第5层 - Boss
            positions.push_back({ Vector2{960.f, 150.f} });

            return positions;
        }

        std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
            std::vector<std::vector<std::pair<int, int>>> connections;

            // 第0层(4) -> 第1层(4)
            std::vector<std::pair<int, int>> layer0to1;
            layer0to1.push_back(std::make_pair(0, 0));
            layer0to1.push_back(std::make_pair(0, 1));
            layer0to1.push_back(std::make_pair(1, 1));
            layer0to1.push_back(std::make_pair(1, 2));
            layer0to1.push_back(std::make_pair(2, 2));
            layer0to1.push_back(std::make_pair(2, 3));
            layer0to1.push_back(std::make_pair(3, 3));
            connections.push_back(layer0to1);

            // 第1层(4) -> 第2层(4)
            std::vector<std::pair<int, int>> layer1to2;
            layer1to2.push_back(std::make_pair(0, 0));
            layer1to2.push_back(std::make_pair(0, 1));
            layer1to2.push_back(std::make_pair(1, 1));
            layer1to2.push_back(std::make_pair(1, 2));
            layer1to2.push_back(std::make_pair(2, 2));
            layer1to2.push_back(std::make_pair(2, 3));
            layer1to2.push_back(std::make_pair(3, 3));
            connections.push_back(layer1to2);

            // 第2层(4) -> 第3层(4)
            std::vector<std::pair<int, int>> layer2to3;
            layer2to3.push_back(std::make_pair(0, 0));
            layer2to3.push_back(std::make_pair(1, 0));
            layer2to3.push_back(std::make_pair(1, 1));
            layer2to3.push_back(std::make_pair(2, 1));
            layer2to3.push_back(std::make_pair(2, 2));
            layer2to3.push_back(std::make_pair(3, 2));
            layer2to3.push_back(std::make_pair(3, 3));
            connections.push_back(layer2to3);

            // 第3层(4) -> 第4层(3)
            std::vector<std::pair<int, int>> layer3to4;
            layer3to4.push_back(std::make_pair(0, 0));
            layer3to4.push_back(std::make_pair(1, 0));
            layer3to4.push_back(std::make_pair(1, 1));
            layer3to4.push_back(std::make_pair(2, 1));
            layer3to4.push_back(std::make_pair(2, 2));
            layer3to4.push_back(std::make_pair(3, 2));
            connections.push_back(layer3to4);

            // 第4层(3) -> 第5层(1)
            std::vector<std::pair<int, int>> layer4to5;
            layer4to5.push_back(std::make_pair(0, 0));
            layer4to5.push_back(std::make_pair(1, 0));
            layer4to5.push_back(std::make_pair(2, 0));
            connections.push_back(layer4to5);

            return connections;
        }

        std::string getName() const override { return "沙漠地图"; }
        std::string getDescription() const override { return "6层地图，战斗和精英节点较多"; }
    };

    // 地图配置管理器
    class MapConfigManager {
    private:
        std::vector<std::unique_ptr<MapConfig>> configs;
        int currentConfigIndex = 0;

    public:
        MapConfigManager() {
            configs.push_back(std::make_unique<MapConfig1>());  // 标准地图 - 6层
            configs.push_back(std::make_unique<MapConfig2>());  // 森林地图 - 5层
            configs.push_back(std::make_unique<MapConfig3>());  // 沙漠地图 - 6层
        }

        MapConfig* getCurrentConfig() const {
            if (configs.empty()) return nullptr;
            return configs[currentConfigIndex].get();
        }

        void nextMap() {
            if (!configs.empty()) {
                currentConfigIndex = (currentConfigIndex + 1) % configs.size();
            }
        }

        void prevMap() {
            if (!configs.empty()) {
                currentConfigIndex = (currentConfigIndex - 1 + configs.size()) % configs.size();
            }
        }

        std::string getCurrentMapName() const {
            auto* config = getCurrentConfig();
            return config ? config->getName() : "未知地图";
        }

        std::string getCurrentMapDescription() const {
            auto* config = getCurrentConfig();
            return config ? config->getDescription() : "";
        }

        int getCurrentLayerCount() const {
            auto* config = getCurrentConfig();
            return config ? config->getLayerCount() : 0;
        }

        size_t getMapCount() const { return configs.size(); }
        int getCurrentIndex() const { return currentConfigIndex; }
    };

}