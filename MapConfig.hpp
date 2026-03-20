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

        // MapConfig1 适配1920×1080的坐标
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // 第0层 - Y: 900左右（留出顶部空间）
            positions.push_back({
                Vector2(340, 900),   // 原(450,1100) -> X:450*0.75=337.5≈340, Y下调
                Vector2(710, 870),   // 原(950,1070) -> X:950*0.75=712.5≈710
                Vector2(1160, 900)   // 原(1550,1130) -> X:1550*0.75=1162.5≈1160
                });

            // 第1层 - Y: 750左右
            positions.push_back({
                Vector2(190, 750),   // 原(250,950) -> X:250*0.75=187.5≈190
                Vector2(525, 720),   // 原(700,900) -> X:700*0.75=525
                Vector2(860, 740),   // 原(1150,940) -> X:1150*0.75=862.5≈860
                Vector2(1200, 720)   // 原(1600,920) -> X:1600*0.75=1200
                });

            // 第2层 - Y: 600左右
            positions.push_back({
                Vector2(225, 600),   // 原(300,700) -> X:300*0.75=225
                Vector2(560, 620),   // 原(750,730) -> X:750*0.75=562.5≈560
                Vector2(900, 590),   // 原(1200,690) -> X:1200*0.75=900
                Vector2(1125, 600)   // 原(1500,700) -> X:1500*0.75=1125
                });

            // 第3层 - Y: 450左右
            positions.push_back({
                Vector2(260, 450),   // 原(350,550) -> X:350*0.75=262.5≈260
                Vector2(600, 430),   // 原(800,530) -> X:800*0.75=600
                Vector2(940, 440),   // 原(1250,540) -> X:1250*0.75=937.5≈940
                Vector2(1275, 420)   // 原(1700,520) -> X:1700*0.75=1275
                });

            // 第4层 - Y: 300左右
            positions.push_back({
                Vector2(340, 300),   // 原(450,400) -> X:450*0.75=337.5≈340
                Vector2(720, 280),   // 原(960,380) -> X:960*0.75=720
                Vector2(1160, 290)   // 原(1550,390) -> X:1550*0.75=1162.5≈1160
                });

            // 第5层 - Boss - Y: 120左右
            positions.push_back({ Vector2(720, 120) });  // 原(960,150) -> X:960*0.75=720, Y下调

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

        // MapConfig2 适配1920×1080的坐标
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // 第0层 - 3个节点，Y: 900左右
            positions.push_back({
                Vector2(300, 900),   // 原(400,1100) -> X:400*0.75=300
                Vector2(675, 870),   // 原(900,1070) -> X:900*0.75=675
                Vector2(1125, 900)   // 原(1500,1100) -> X:1500*0.75=1125
                });

            // 第1层 - 4个节点，Y: 700左右
            positions.push_back({
                Vector2(150, 700),   // 原(200,850) -> X:200*0.75=150
                Vector2(490, 700),   // 原(650,850) -> X:650*0.75=487.5≈490
                Vector2(825, 680),   // 原(1100,830) -> X:1100*0.75=825
                Vector2(1160, 720)   // 原(1550,880) -> X:1550*0.75=1162.5≈1160
                });

            // 第2层 - 4个节点，Y: 520左右
            positions.push_back({
                Vector2(190, 520),   // 原(250,650) -> X:250*0.75=187.5≈190
                Vector2(525, 500),   // 原(700,620) -> X:700*0.75=525
                Vector2(860, 510),   // 原(1150,630) -> X:1150*0.75=862.5≈860
                Vector2(1200, 500)   // 原(1600,610) -> X:1600*0.75=1200
                });

            // 第3层 - 3个节点，Y: 330左右
            positions.push_back({
                Vector2(300, 330),   // 原(400,400) -> X:400*0.75=300
                Vector2(710, 360),   // 原(950,450) -> X:950*0.75=712.5≈710
                Vector2(1125, 340)   // 原(1500,420) -> X:1500*0.75=1125
                });

            // 第4层 - Boss，Y: 120左右
            positions.push_back({ Vector2(720, 120) });  // 原(960,170) -> X:960*0.75=720

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

        // MapConfig3 适配1920×1080的坐标
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // 第0层 - 4个节点，Y: 950左右
            positions.push_back({
                Vector2(190, 950),   // 原(250,1200) -> X:250*0.75=187.5≈190
                Vector2(490, 920),   // 原(650,1170) -> X:650*0.75=487.5≈490
                Vector2(790, 910),   // 原(1050,1160) -> X:1050*0.75=787.5≈790
                Vector2(1090, 950)   // 原(1450,1220) -> X:1450*0.75=1087.5≈1090
                });

            // 第1层 - 4个节点，Y: 760左右
            positions.push_back({
                Vector2(225, 760),   // 原(300,950) -> X:300*0.75=225
                Vector2(525, 740),   // 原(700,930) -> X:700*0.75=525
                Vector2(825, 760),   // 原(1100,960) -> X:1100*0.75=825
                Vector2(1125, 730)   // 原(1500,920) -> X:1500*0.75=1125
                });

            // 第2层 - 4个节点，Y: 610左右
            positions.push_back({
                Vector2(150, 610),   // 原(200,770) -> X:200*0.75=150
                Vector2(560, 620),   // 原(750,780) -> X:750*0.75=562.5≈560
                Vector2(860, 620),   // 原(1150,790) -> X:1150*0.75=862.5≈860
                Vector2(1275, 610)   // 原(1700,770) -> X:1700*0.75=1275
                });

            // 第3层 - 4个节点，Y: 470左右
            positions.push_back({
                Vector2(300, 470),   // 原(400,620) -> X:400*0.75=300
                Vector2(600, 440),   // 原(800,580) -> X:800*0.75=600
                Vector2(900, 460),   // 原(1200,610) -> X:1200*0.75=900
                Vector2(1200, 420)   // 原(1600,550) -> X:1600*0.75=1200
                });

            // 第4层 - 3个节点，Y: 310左右
            positions.push_back({
                Vector2(340, 310),   // 原(450,400) -> X:450*0.75=337.5≈340
                Vector2(750, 310),   // 原(1000,400) -> X:1000*0.75=750
                Vector2(1160, 290)   // 原(1550,380) -> X:1550*0.75=1162.5≈1160
                });

            // 第5层 - Boss，Y: 100左右
            positions.push_back({ Vector2(720, 100) });  // 原(960,150) -> X:960*0.75=720

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