/**
 * Battle UI - 严格按照参考图：顶栏(名字/HP/金币/药水/球槽)、遗物行、战场、底栏(能量/抽牌/手牌/结束回合/弃牌)
 * 牌组界面：顶栏+遗物栏不变，中间展示牌堆网格，支持滚轮滚动、返回按钮关闭。
 */
#pragma once

#include "BattleUIData.hpp"
#include "../CardSystem/CardSystem.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace tce {

struct BattleStateSnapshot;

class BattleUI {
public:
    BattleUI(unsigned width, unsigned height);

    bool loadFont(const std::string& path);
    /** 加载中文字体（用于显示中文，若未加载则用主字体可能显示为方框） */
    bool loadChineseFont(const std::string& path);

    bool handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos);
    void setMousePosition(sf::Vector2f pos);

    void draw(sf::RenderWindow& window, IBattleUIDataProvider& data);

    /** 轮询一次是否有“打出牌”的请求，若有则返回手牌下标与目标怪物下标 */
    bool pollPlayCardRequest(int& outHandIndex, int& outTargetMonsterIndex);

    /** 牌组界面：设置要展示的牌列表（手牌+抽牌堆+弃牌堆+消耗堆合并），并打开/关闭牌组界面 */
    void set_deck_view_cards(std::vector<CardInstance> cards);
    void set_deck_view_active(bool active);
    bool is_deck_view_active() const { return deck_view_active_; }
    /** 轮询一次是否请求打开牌组界面；outMode: 1=牌组(右上角)，2=抽牌堆(左下角)，3=弃牌堆(右下角)，4=消耗堆(弃牌堆上方) */
    bool pollOpenDeckViewRequest(int& outMode);

    /** 屏幕中央短时提示（如“抽牌堆为空”），seconds 为显示秒数 */
    void showTip(std::wstring text, float seconds = 1.2f);

private:
    void drawDeckView(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawTopBar(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawRelicsRow(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawBattleCenter(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawBottomBar(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawTopRight(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void show_center_tip(std::wstring text, float seconds);
    void draw_center_tip(sf::RenderWindow& window);
    bool can_pay_selected_card_cost() const;

    unsigned width_;
    unsigned height_;
    sf::Font font_;
    bool fontLoaded_ = false;
    sf::Font fontChinese_;
    bool fontChineseLoaded_ = false;

    const sf::Font& fontForChinese() const { return fontChineseLoaded_ ? fontChinese_ : font_; }
    sf::FloatRect endTurnButton_;
    sf::Vector2f mousePos_ = {-9999.f, -9999.f};

    // --- 出牌/瞄准相关临时状态 ---
    int  selectedHandIndex_ = -1;              // 当前选中的手牌索引，-1 表示未选中
    bool isAimingCard_ = false;                // 是否正在瞄准（选择目标）
    bool selectedCardTargetsEnemy_ = false;    // 选中牌是否需要敌人目标，否则为玩家自身

    // 模型矩形（用于点击/高亮）
    sf::FloatRect playerModelRect_;
    sf::FloatRect monsterModelRect_;

    // 等待主循环实际调用 engine.play_card 的请求
    int pendingPlayHandIndex_ = -1;
    int pendingPlayTargetMonsterIndex_ = -1;   // -1 表示玩家自身

    // 选中牌“原位矩形”与跟随状态
    sf::Vector2f selectedCardOriginPos_{0.f, 0.f};
    bool         selectedCardIsFollowing_ = false;
    bool         selectedCardInsideOriginRect_ = true;

    // 选中牌当前屏幕中心位置（用于绘制瞄准箭头）
    sf::Vector2f selectedCardScreenPos_{0.f, 0.f};

    // 最近一次绘制使用的快照（用于在事件处理阶段校验能量等 UI 侧逻辑）
    const BattleStateSnapshot* lastSnapshot_ = nullptr;

    // 屏幕中间提示（如“能量不足”）
    sf::Clock    centerTipClock_;
    float        centerTipSeconds_ = 0.f;
    std::wstring centerTipText_;

    // 牌组界面
    bool                          deck_view_active_ = false;
    std::vector<CardInstance>     deck_view_cards_;
    float                         deck_view_scroll_y_ = 0.f;
    int                           pending_deck_view_mode_ = 0;  // 0 无，1 整个牌组，2 抽牌堆，3 弃牌堆
    sf::FloatRect                 deckViewReturnButton_;
};

} // namespace tce
