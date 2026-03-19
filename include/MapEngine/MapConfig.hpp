//MapConfig.hpp
#pragma once
#include "../Common/NodeTypes.hpp"
#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <iostream>

namespace MapEngine {

    // ��ͼ���û���
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

    // ��ͼ1����׼��ͼ - 6��
    class MapConfig1 : public MapConfig {
    public:
        int getLayerCount() const override { return 6; }

        std::vector<std::vector<NodeType>> getLayerTypes() const override {
            std::vector<std::vector<NodeType>> layers;

            // ��0�� - 3���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Enemy
                });

            // ��1�� - 4���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Elite,    // ������Ӣ�ڵ�
                NodeType::Rest
                });

            // ��2�� - 4���ڵ�
            layers.push_back({
                NodeType::Event,
                NodeType::Elite,     // ������Ӣ�ڵ�
                NodeType::Merchant,
                NodeType::Rest
                });

            // ��3�� - 4���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Treasure,  // �������ؽڵ�
                NodeType::Rest
                });

            // ��4�� - 3���ڵ�
            layers.push_back({
                NodeType::Merchant,
                NodeType::Elite,      // ������Ӣ�ڵ�
                NodeType::Treasure    // �������ؽڵ�
                });

            // ��5�� - Boss
            layers.push_back({ NodeType::Boss });

            return layers;
        }

        // MapConfig1 �������޸�
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // ��0�� - ԭ��900���ң���Ϊ1000
            positions.push_back({
                Vector2(450, 1100),  // Y: 900 -> 1000
                Vector2(950, 1070),   // Y: 870 -> 970
                Vector2(1550, 1130)   // Y: 880 -> 980
                });

            // ��1�� - ԭ��750���ң���Ϊ850
            positions.push_back({
                Vector2(250, 950),   // Y: 750 -> 850
                Vector2(700, 900),   // Y: 730 -> 830
                Vector2(1150, 940),  // Y: 740 -> 840
                Vector2(1600, 920)   // Y: 720 -> 820
                });

            // ��2�� - ԭ��600���ң���Ϊ700
            positions.push_back({
                Vector2(300, 700),   // Y: 600 -> 700
                Vector2(750, 730),   // Y: 580 -> 680
                Vector2(1200, 690),  // Y: 590 -> 690
                Vector2(1500, 700)   // Y: 570 -> 670
                });

            // ��3�� - ԭ��450���ң���Ϊ550
            positions.push_back({
                Vector2(350, 550),   // Y: 450 -> 550
                Vector2(800, 530),   // Y: 430 -> 530
                Vector2(1250, 540),  // Y: 440 -> 540
                Vector2(1700, 520)   // Y: 420 -> 520
                });

            // ��4�� - ԭ��300���ң���Ϊ400
            positions.push_back({
                Vector2(450, 400),   // Y: 300 -> 400
                Vector2(960, 380),   // Y: 280 -> 380
                Vector2(1550, 390)   // Y: 290 -> 390
                });

            // ��5�� - Boss - ԭ��150����Ϊ250
            positions.push_back({ Vector2(960, 150) });  // Y: 150 -> 250

            return positions;
        }

        std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
            std::vector<std::vector<std::pair<int, int>>> connections;

            // ��0��(3) -> ��1��(4)
            std::vector<std::pair<int, int>> layer0to1;
            layer0to1.push_back(std::make_pair(0, 0));
            layer0to1.push_back(std::make_pair(0, 1));
            layer0to1.push_back(std::make_pair(1, 1));
            layer0to1.push_back(std::make_pair(1, 2));
            layer0to1.push_back(std::make_pair(2, 2));
            layer0to1.push_back(std::make_pair(2, 3));
            connections.push_back(layer0to1);

            // ��1��(4) -> ��2��(4)
            std::vector<std::pair<int, int>> layer1to2;
            layer1to2.push_back(std::make_pair(0, 0));
            layer1to2.push_back(std::make_pair(0, 1));
            layer1to2.push_back(std::make_pair(1, 1));
            layer1to2.push_back(std::make_pair(1, 2));
            layer1to2.push_back(std::make_pair(2, 2));
            layer1to2.push_back(std::make_pair(2, 3));
            layer1to2.push_back(std::make_pair(3, 3));
            connections.push_back(layer1to2);

            // ��2��(4) -> ��3��(4)
            std::vector<std::pair<int, int>> layer2to3;
            layer2to3.push_back(std::make_pair(0, 0));
            layer2to3.push_back(std::make_pair(1, 0));
            layer2to3.push_back(std::make_pair(1, 1));
            layer2to3.push_back(std::make_pair(2, 1));
            layer2to3.push_back(std::make_pair(2, 2));
            layer2to3.push_back(std::make_pair(3, 2));
            layer2to3.push_back(std::make_pair(3, 3));
            connections.push_back(layer2to3);

            // ��3��(4) -> ��4��(3)
            std::vector<std::pair<int, int>> layer3to4;
            layer3to4.push_back(std::make_pair(0, 0));
            layer3to4.push_back(std::make_pair(1, 0));
            layer3to4.push_back(std::make_pair(1, 1));
            layer3to4.push_back(std::make_pair(2, 1));
            layer3to4.push_back(std::make_pair(2, 2));
            layer3to4.push_back(std::make_pair(3, 2));
            connections.push_back(layer3to4);

            // ��4��(3) -> ��5��(1)
            std::vector<std::pair<int, int>> layer4to5;
            layer4to5.push_back(std::make_pair(0, 0));
            layer4to5.push_back(std::make_pair(1, 0));
            layer4to5.push_back(std::make_pair(2, 0));
            connections.push_back(layer4to5);

            return connections;
        }

        std::string getName() const override { return "标准地图"; }
        std::string getDescription() const override { return "6 层地图，节点类型分布均衡"; }
    };

    // ��ͼ2��ɭ������ - 5��
    class MapConfig2 : public MapConfig {
    public:
        int getLayerCount() const override { return 5; }

        std::vector<std::vector<NodeType>> getLayerTypes() const override {
            std::vector<std::vector<NodeType>> layers;

            // ��0�� - 3���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Enemy
                });

            // ��1�� - 4���ڵ�
            layers.push_back({
                NodeType::Event,
                NodeType::Elite,     // ��Ӣ�ڵ�
                NodeType::Rest,
                NodeType::Treasure    // ���ؽڵ�
                });

            // ��2�� - 4���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Merchant,
                NodeType::Elite,      // ��Ӣ�ڵ�
                NodeType::Rest
                });

            // ��3�� - 3���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Treasure,   // ���ؽڵ�
                NodeType::Elite        // ��Ӣ�ڵ�
                });

            // ��4�� - Boss
            layers.push_back({ NodeType::Boss });

            return layers;
        }

        // MapConfig2 �������޸�
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // ��0�� - 3���ڵ㣬ˮƽ��ɢ
            positions.push_back({
                Vector2(400, 1100),   // ��
                Vector2(900, 1070),   // ��
                Vector2(1500, 1100)   // ��
                });

            // ��1�� - 4���ڵ�
            positions.push_back({
                Vector2(200, 850),   // ��1
                Vector2(650, 850),   // ��2
                Vector2(1100, 830),  // ��2
                Vector2(1550, 880)   // ��1
                });

            // ��2�� - 4���ڵ�
            positions.push_back({
                Vector2(250, 650),   // ��1
                Vector2(700, 620),   // ��2
                Vector2(1150, 630),  // ��2
                Vector2(1600, 610)   // ��1
                });

            // ��3�� - 3���ڵ�
            positions.push_back({
                Vector2(400, 400),   // ��
                Vector2(950, 450),   // ��
                Vector2(1500, 420)   // ��
                });

            // ��4�� - Boss
            positions.push_back({ Vector2(960, 170) });

            return positions;
        }

        std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
            std::vector<std::vector<std::pair<int, int>>> connections;

            // ��0��(3) -> ��1��(4)
            std::vector<std::pair<int, int>> layer0to1;
            layer0to1.push_back(std::make_pair(0, 0));
            layer0to1.push_back(std::make_pair(0, 1));
            layer0to1.push_back(std::make_pair(1, 1));
            layer0to1.push_back(std::make_pair(1, 2));
            layer0to1.push_back(std::make_pair(2, 2));
            layer0to1.push_back(std::make_pair(2, 3));
            connections.push_back(layer0to1);

            // ��1��(4) -> ��2��(4)
            std::vector<std::pair<int, int>> layer1to2;
            layer1to2.push_back(std::make_pair(0, 0));
            layer1to2.push_back(std::make_pair(1, 0));
            layer1to2.push_back(std::make_pair(1, 1));
            layer1to2.push_back(std::make_pair(2, 1));
            layer1to2.push_back(std::make_pair(2, 2));
            layer1to2.push_back(std::make_pair(3, 2));
            layer1to2.push_back(std::make_pair(3, 3));
            connections.push_back(layer1to2);

            // ��2��(4) -> ��3��(3)
            std::vector<std::pair<int, int>> layer2to3;
            layer2to3.push_back(std::make_pair(0, 0));
            layer2to3.push_back(std::make_pair(1, 0));
            layer2to3.push_back(std::make_pair(1, 1));
            layer2to3.push_back(std::make_pair(2, 1));
            layer2to3.push_back(std::make_pair(2, 2));
            layer2to3.push_back(std::make_pair(3, 2));
            connections.push_back(layer2to3);

            // ��3��(3) -> ��4��(1)
            std::vector<std::pair<int, int>> layer3to4;
            layer3to4.push_back(std::make_pair(0, 0));
            layer3to4.push_back(std::make_pair(1, 0));
            layer3to4.push_back(std::make_pair(2, 0));
            connections.push_back(layer3to4);

            return connections;
        }

        std::string getName() const override { return "森林地图"; }
        std::string getDescription() const override { return "5 层地图，事件与宝藏节点较多"; }
    };

    // ��ͼ3��ɳĮ���� - 6��
    class MapConfig3 : public MapConfig {
    public:
        int getLayerCount() const override { return 6; }

        std::vector<std::vector<NodeType>> getLayerTypes() const override {
            std::vector<std::vector<NodeType>> layers;

            // ��0�� - 4���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Enemy,
                NodeType::Event,
                NodeType::Enemy
                });

            // ��1�� - 4���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Rest,
                NodeType::Elite,     // ��Ӣ�ڵ�
                NodeType::Event
                });

            // ��2�� - 4���ڵ�
            layers.push_back({
                NodeType::Event,
                NodeType::Elite,      // ��Ӣ�ڵ�
                NodeType::Merchant,
                NodeType::Enemy
                });

            // ��3�� - 4���ڵ�
            layers.push_back({
                NodeType::Enemy,
                NodeType::Rest,
                NodeType::Treasure,   // ���ؽڵ�
                NodeType::Event
                });

            // ��4�� - 3���ڵ�
            layers.push_back({
                NodeType::Merchant,
                NodeType::Elite,       // ��Ӣ�ڵ�
                NodeType::Treasure      // ���ؽڵ�
                });

            // ��5�� - Boss
            layers.push_back({ NodeType::Boss });

            return layers;
        }

        // MapConfig3 �������޸�
        std::vector<std::vector<Vector2>> getNodePositions() const override {
            std::vector<std::vector<Vector2>> positions;

            // ��0�� - 4���ڵ㣬���������Ҿ��ȷֲ�
            positions.push_back({
                Vector2(250, 1200),   // ��1
                Vector2(650, 1170),   // ��2
                Vector2(1050, 1160),  // ��2
                Vector2(1450, 1220)   // ��1
                });

            // ��1�� - 4���ڵ�
            positions.push_back({
                Vector2(300, 950),   // ��1
                Vector2(700, 930),   // ��2
                Vector2(1100, 960),  // ��2
                Vector2(1500, 920)   // ��1
                });

            // ��2�� - 4���ڵ�
            positions.push_back({
                Vector2(200, 770),   // ��1
                Vector2(750, 780),   // ��2
                Vector2(1150, 790),  // ��2
                Vector2(1700, 770)   // ��1
                });

            // ��3�� - 4���ڵ�
            positions.push_back({
                Vector2(400, 620),   // ��1
                Vector2(800, 580),   // ��2
                Vector2(1200, 610),  // ��2
                Vector2(1600, 550)   // ��1
                });

            // ��4�� - 3���ڵ�
            positions.push_back({
                Vector2(450, 400),   // ��
                Vector2(1000, 400),   // ��
                Vector2(1550, 380)   // ��
                });

            // ��5�� - Boss
            positions.push_back({ Vector2(960, 150) });

            return positions;
        }

        std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
            std::vector<std::vector<std::pair<int, int>>> connections;

            // ��0��(4) -> ��1��(4)
            std::vector<std::pair<int, int>> layer0to1;
            layer0to1.push_back(std::make_pair(0, 0));
            layer0to1.push_back(std::make_pair(0, 1));
            layer0to1.push_back(std::make_pair(1, 1));
            layer0to1.push_back(std::make_pair(1, 2));
            layer0to1.push_back(std::make_pair(2, 2));
            layer0to1.push_back(std::make_pair(2, 3));
            layer0to1.push_back(std::make_pair(3, 3));
            connections.push_back(layer0to1);

            // ��1��(4) -> ��2��(4)
            std::vector<std::pair<int, int>> layer1to2;
            layer1to2.push_back(std::make_pair(0, 0));
            layer1to2.push_back(std::make_pair(0, 1));
            layer1to2.push_back(std::make_pair(1, 1));
            layer1to2.push_back(std::make_pair(1, 2));
            layer1to2.push_back(std::make_pair(2, 2));
            layer1to2.push_back(std::make_pair(2, 3));
            layer1to2.push_back(std::make_pair(3, 3));
            connections.push_back(layer1to2);

            // ��2��(4) -> ��3��(4)
            std::vector<std::pair<int, int>> layer2to3;
            layer2to3.push_back(std::make_pair(0, 0));
            layer2to3.push_back(std::make_pair(1, 0));
            layer2to3.push_back(std::make_pair(1, 1));
            layer2to3.push_back(std::make_pair(2, 1));
            layer2to3.push_back(std::make_pair(2, 2));
            layer2to3.push_back(std::make_pair(3, 2));
            layer2to3.push_back(std::make_pair(3, 3));
            connections.push_back(layer2to3);

            // ��3��(4) -> ��4��(3)
            std::vector<std::pair<int, int>> layer3to4;
            layer3to4.push_back(std::make_pair(0, 0));
            layer3to4.push_back(std::make_pair(1, 0));
            layer3to4.push_back(std::make_pair(1, 1));
            layer3to4.push_back(std::make_pair(2, 1));
            layer3to4.push_back(std::make_pair(2, 2));
            layer3to4.push_back(std::make_pair(3, 2));
            connections.push_back(layer3to4);

            // ��4��(3) -> ��5��(1)
            std::vector<std::pair<int, int>> layer4to5;
            layer4to5.push_back(std::make_pair(0, 0));
            layer4to5.push_back(std::make_pair(1, 0));
            layer4to5.push_back(std::make_pair(2, 0));
            connections.push_back(layer4to5);

            return connections;
        }

        std::string getName() const override { return "沙漠地图"; }
        std::string getDescription() const override { return "6 层地图，战斗与精英节点较多"; }
    };

    // ��ͼ���ù�����
    class MapConfigManager {
    private:
        std::vector<std::unique_ptr<MapConfig>> configs;
        int currentConfigIndex = 0;

    public:
        MapConfigManager() {
            configs.push_back(std::make_unique<MapConfig1>());  // ��׼��ͼ - 6��
            configs.push_back(std::make_unique<MapConfig2>());  // ɭ�ֵ�ͼ - 5��
            configs.push_back(std::make_unique<MapConfig3>());  // ɳĮ��ͼ - 6��
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