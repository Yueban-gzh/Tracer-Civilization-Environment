#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BattleCoreRefactor/BattleEngine.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "CardSystem/CardSystem.hpp"
#include "Common/RunRng.hpp"
#include "DataLayer/DataLayer.h"
#include "EventEngine/EventEngine.hpp"
#include "MapEngine/MapConfig.hpp"
#include "MapEngine/MapEngine.hpp"
#include "MapEngine/MapUI.hpp"

namespace tce {

enum class CharacterClass;

class GameFlowController {
public:
    enum class LastSceneKind {
        Map,
        Battle,
        Event,
        Shop,
        Rest,
        Treasure
    };

    explicit GameFlowController(sf::RenderWindow& window);
    bool initialize();  // 默认：铁甲战士
    bool initialize(CharacterClass cc);
    void run();

    /** 将当前 Run 状态写入存档文件（默认：saves/run_auto_save.json）。成功返回 true。 */
    bool saveRun(const std::string& path = "saves/run_auto_save.json") const;

    /** 从存档文件读取 Run 状态并恢复到当前控制器，成功返回 true。 */
    bool loadRun(const std::string& path = "saves/run_auto_save.json");

    /** 存档用：当前 Run 伪随机状态（与抽牌/奖励等同源）。读档后 set 再继续游戏可复现序列。 */
    uint64_t get_run_rng_state() const { return runRng_.get_state(); }
    void     set_run_rng_state(uint64_t s) { runRng_.set_state(s); }

private:
    bool tryMoveToNode(const std::string& nodeId);
    void resolveNode(const MapEngine::MapNode& node);
    void captureCheckpointForCurrentNode();  // 固定存档点：进入节点瞬间的 Run 状态

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

    /** 非主地图界面：全屏仅滚动查看地图；再点「地图」或点「返回」关闭 */
    void open_map_browse_overlay(tce::BattleUI* battleUiOrNull);
    void close_map_browse_overlay(tce::BattleUI* battleUiOrNull);
    void poll_map_browse_toggle(tce::BattleUI* battleUiOrNull);
    void layout_map_browse_return_button();
    bool hit_map_browse_return_button(const sf::Vector2f& p);
    void draw_map_browse_return_button();
    /** 作弊模式开启时：屏幕角落提示（F2 开关） */
    void draw_cheat_mode_hint();

private:
    sf::RenderWindow& window_;
    sf::Font hudFont_;
    bool hudFontLoaded_ = false;
    // 地图 / 事件界面共用的战斗顶栏 + 遗物栏 UI
    BattleUI hudBattleUi_;

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
    std::string statusText_;

    // 固定存档点：每次进入节点（房间）瞬间记录一份检查点；之后任意时刻存档都只写这个检查点，
    // 防止通过 SL 改变宝箱/事件等奖励结果（run_rng_state 不随房间内操作变化）。
    bool            checkpointValid_ = false;
    uint64_t        checkpointRunRngState_ = 0;
    PlayerBattleState checkpointPlayerState_{};
    std::vector<CardId> checkpointMasterDeck_{};
    int             checkpointCurrentLayer_ = 0;
    std::string     checkpointCurrentNodeId_;

    // 存档/读档用：记录最后所在界面，以及读档后应直接进入的界面
    LastSceneKind lastSceneForSave_         = LastSceneKind::Map;
    LastSceneKind sceneAfterLoad_           = LastSceneKind::Map;
    bool          hasPendingSceneAfterLoad_ = false;

    // 从暂停菜单“保存并退出”返回开始界面，而不是直接关游戏
    bool exitToStartRequested_ = false;

    bool map_cheat_free_travel_ = false;  // F2：作弊模式（地图任意节点可达；战斗中 K 秒杀全部怪物）
    bool            map_browse_overlay_active_ = false;
    sf::FloatRect   map_browse_return_rect_;

    // 事件去重：同一地图层中已经触发过的根事件 id（尽量避免同层重复事件）
    std::unordered_map<int, std::unordered_set<std::string>> seenEventRootsByLayer_;
};

/** 开始界面：在进入 GameFlowController::run 之前调用，提供“新游戏 / 继续游戏”选项。 */
void runStartScreen(sf::RenderWindow& window, GameFlowController& controller);

} // namespace tce
