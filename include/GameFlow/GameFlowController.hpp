#pragma once

#include <SFML/Graphics.hpp>
#include <random>
#include <string>
#include <vector>

#include "BattleCoreRefactor/BattleEngine.hpp"
#include "CardSystem/CardSystem.hpp"
#include "DataLayer/DataLayer.h"
#include "EventEngine/EventEngine.hpp"
#include "MapEngine/MapConfig.hpp"
#include "MapEngine/MapEngine.hpp"
#include "MapEngine/MapUI.hpp"

namespace tce {

class GameFlowController {
public:
    explicit GameFlowController(sf::RenderWindow& window);
    bool initialize();
    void run();

private:
    bool tryMoveToNode(const std::string& nodeId);
    void resolveNode(const MapEngine::MapNode& node);

    bool runBattleScene(NodeType nodeType);
    void resolveEvent(const std::string& contentId);
    bool runEventScene(const std::string& contentId);
    bool runShopScene();
    bool runRestScene();
    void resolveRest();
    void resolveMerchant();
    void resolveTreasure();

    int firstAliveMonsterIndex(const BattleState& state) const;
    void drawHud();
    std::string nodeTypeToString(NodeType nodeType) const;

private:
    sf::RenderWindow& window_;
    sf::Font hudFont_;
    bool hudFontLoaded_ = false;

    DataLayer::DataLayerImpl dataLayer_;
    CardSystem cardSystem_;
    BattleEngine battleEngine_;
    EventEngine eventEngine_;

    MapEngine::MapConfigManager mapConfigManager_;
    MapEngine::MapEngine mapEngine_;
    MapEngine::MapUI mapUI_;

    PlayerBattleState playerState_{};
    bool gameOver_ = false;
    bool gameCleared_ = false;
    std::string statusText_;

    std::mt19937 rng_;
};

} // namespace tce
