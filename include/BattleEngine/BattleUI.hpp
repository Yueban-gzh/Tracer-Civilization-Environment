/**
 * Battle UI - 严格按照参考图：顶栏(名字/HP/金币/药水/球槽)、遗物行、战场、底栏(能量/抽牌/手牌/结束回合/弃牌)
 */
#pragma once

#include "BattleUIData.hpp"
#include <SFML/Graphics.hpp>
#include <string>

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

private:
    void drawTopBar(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawRelicsRow(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawBattleCenter(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawBottomBar(sf::RenderWindow& window, const BattleStateSnapshot& s);
    void drawTopRight(sf::RenderWindow& window, const BattleStateSnapshot& s);

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
};

} // namespace tce
