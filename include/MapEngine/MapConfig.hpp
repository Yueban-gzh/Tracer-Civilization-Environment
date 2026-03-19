/**
 * 地图配置：固定地图的层、节点类型、位置、连接关系
 * 供 MapEngine::init_fixed_map 使用
 */
#pragma once

#include "../Common/NodeTypes.hpp"
#include <memory>
#include <string>
#include <vector>

namespace MapEngine {

class MapConfig {
public:
    virtual ~MapConfig() = default;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    /** 每层的节点类型，layers[layer][index] = NodeType */
    virtual std::vector<std::vector<NodeType>> getLayerTypes() const = 0;
    /** 每层的节点位置，positions[layer][index] = Vector2 */
    virtual std::vector<std::vector<Vector2>> getNodePositions() const = 0;
    /** 层间连接 connections[layer] = { {fromIndex, toIndex}, ... } 表示 layer 的 fromIndex 连到 layer+1 的 toIndex */
    virtual std::vector<std::vector<std::pair<int, int>>> getConnections() const = 0;
};

/** 简单固定地图：3 层，每层 1/2/1 节点，用于测试 */
class TestMapConfig : public MapConfig {
public:
    std::string getName() const override { return "TestMap"; }
    std::string getDescription() const override { return "测试用 3 层地图"; }
    std::vector<std::vector<NodeType>> getLayerTypes() const override {
        return {
            { NodeType::Enemy },
            { NodeType::Event, NodeType::Rest },
            { NodeType::Boss }
        };
    }
    std::vector<std::vector<Vector2>> getNodePositions() const override {
        return {
            { { 960.f, 150.f } },
            { { 760.f, 540.f }, { 1160.f, 540.f } },
            { { 960.f, 900.f } }
        };
    }
    std::vector<std::vector<std::pair<int, int>>> getConnections() const override {
        return {
            { { 0, 0 }, { 0, 1 } },
            { { 0, 0 }, { 1, 0 } }
        };
    }
};

/** 管理多张固定地图切换 */
class MapConfigManager {
public:
    MapConfigManager() {
        configs_.push_back(std::make_unique<TestMapConfig>());
        currentIndex_ = 0;
    }
    MapConfig* getCurrentConfig() const {
        return configs_.empty() ? nullptr : configs_[currentIndex_].get();
    }
    void nextMap() {
        if (configs_.empty()) return;
        currentIndex_ = (currentIndex_ + 1) % configs_.size();
    }
    void prevMap() {
        if (configs_.empty()) return;
        currentIndex_ = currentIndex_ == 0 ? static_cast<size_t>(configs_.size() - 1) : currentIndex_ - 1;
    }
    std::string getCurrentMapName() const {
        return getCurrentConfig() ? getCurrentConfig()->getName() : "";
    }
    size_t getMapCount() const { return configs_.size(); }

private:
    std::vector<std::unique_ptr<MapConfig>> configs_;
    size_t currentIndex_;
};

} // namespace MapEngine
