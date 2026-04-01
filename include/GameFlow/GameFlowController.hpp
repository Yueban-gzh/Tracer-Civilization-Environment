#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "BattleCoreRefactor/BattleEngine.hpp"
#include "CardSystem/CardSystem.hpp"
#include "Common/RunRng.hpp"
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

    /** 存档用：当前 Run 伪随机状态（与抽牌/奖励等同源）。读档后 set 再继续游戏可复现序列。 */
    uint64_t get_run_rng_state() const { return runRng_.get_state(); }
    void     set_run_rng_state(uint64_t s) { runRng_.set_state(s); }

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
    bool runTreasureScene();

    int firstAliveMonsterIndex(const BattleState& state) const;
    void drawHud();
    std::string nodeTypeToString(NodeType nodeType) const;

private:
    sf::RenderWindow& window_;
    sf::Font hudFont_;
    bool hudFontLoaded_ = false;

    RunRng                 runRng_;
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
};

} // namespace tce
