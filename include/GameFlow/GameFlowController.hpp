#pragma once

#include <SFML/Graphics.hpp>
#include <cstdint>

namespace sf {
struct ContextSettings;
}
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BattleCoreRefactor/BattleEngine.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "CardSystem/CardSystem.hpp"
#include "Common/MusicManager.hpp"
#include "Common/RunRng.hpp"
#include "DataLayer/DataLayer.h"
#include "EventEngine/EventEngine.hpp"
#include "MapEngine/MapConfig.hpp"
#include "MapEngine/MapEngine.hpp"
#include "MapEngine/MapUI.hpp"
#include "EventEngine/TreasureRoomLogic.hpp"

namespace tce {

enum class CharacterClass;

class GameFlowController {
public:
    enum class LastSceneKind {
        Map,
        Battle,
        BattleReward,
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

    /** 删除 Run 存档文件（默认：saves/run_auto_save.json）。成功删除或文件本就不存在时返回 true。 */
    bool deleteRunSave(const std::string& path = "saves/run_auto_save.json") const;

    /** 存档用：当前 Run 伪随机状态（与抽牌/奖励等同源）。读档后 set 再继续游戏可复现序列。 */
    uint64_t get_run_rng_state() const { return runRng_.get_state(); }
    void     set_run_rng_state(uint64_t s) { runRng_.set_state(s); }

    /** 若用户在设置中申请了分辨率变更，则重建窗口并同步 HUD 尺寸（每帧或每轮循环开头调用） */
    void applyPendingVideoAndHudResize(const sf::ContextSettings& ctx);

private:
    bool tryMoveToNode(const std::string& nodeId);
    void resolveNode(const MapEngine::MapNode& node);
    void captureCheckpointForCurrentNode();  // 固定存档点：进入节点瞬间的 Run 状态

    bool runBattleScene(NodeType nodeType);
    bool runBattleRewardOnlyScene(); // 读档：从战斗胜利奖励界面继续（不重新战斗）
    void resolveEvent(const std::string& contentId);
    bool runEventScene(const std::string& contentId);
    bool runShopScene();
    bool runRestScene();
    void resolveRest();
    void resolveMerchant();
    bool runTreasureScene();
    void runBattleEntryAnimation();
    void runCinematicDialog(const std::vector<std::wstring>& lines, const std::string& backgroundPath = "assets/backgrounds/dialog_bg.png");
    void playCinematicVideoIfAvailable(const std::string& videoPath);

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

    MusicManager           musicManager_{};
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

    // 读档/存档：战斗胜利奖励界面（选项制）状态
    int                      savedBattleRewardGold_ = 0;
    bool                     savedBattleRewardCardPicked_ = false;
    std::vector<std::string> savedBattleRewardCards_;
    std::vector<std::string> savedBattleRewardRelicOffers_;
    std::vector<std::string> savedBattleRewardPotionOffers_;

    /** 读档进入宝箱房：使用存档中已固定的随机结果，不再 roll（与 battle_reward 同理）。 */
    bool                hasPendingTreasureOutcome_ = false;
    TreasureRoomOutcome pendingTreasureOutcome_{};

    /** 当前宝箱界面已确定的结算（const saveRun 序列化用）；离开房间后清除。 */
    mutable bool                treasureOutcomeSnapshotValid_ = false;
    mutable TreasureRoomOutcome treasureOutcomeSnapshot_{};

    // 从暂停菜单“保存并退出”返回开始界面，而不是直接关游戏
    bool exitToStartRequested_ = false;

    bool map_cheat_free_travel_ = false;  // F2：作弊模式（地图任意节点可达；战斗中 K 秒杀全部怪物）
    bool            map_browse_overlay_active_ = false;
    sf::FloatRect   map_browse_return_rect_;

    // 事件去重：同一地图层本轮尚未抽过的根事件；一轮抽完后清空再开下一轮（不立刻加权放回全池）
    std::unordered_map<int, std::unordered_set<std::string>> seenEventRootsByLayer_;
};

/** 开始界面：在进入 GameFlowController::run 之前调用，提供“新游戏 / 继续游戏”选项。 */
void runStartScreen(sf::RenderWindow& window, GameFlowController& controller);

} // namespace tce
