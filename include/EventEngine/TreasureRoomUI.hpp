#pragma once

#include "EventEngine/TreasureRoomLogic.hpp"
#include <SFML/Graphics.hpp>
#include <string>

namespace tce {

/** 宝箱房 UI 展示数据（由主流程根据 TreasureRoomOutcome + 中文名等填充） */
struct TreasureRoomDisplayData {
    TreasureChestKind chest_kind = TreasureChestKind::Small;
    std::wstring title_line;   // 如：小型宝箱
    bool         has_gold = false;
    int          gold_amount = 0;
    bool         has_relic = false;
    std::wstring relic_line;   // 如：遗物「草莓」
    std::string  relic_icon_path; // assets/relics/xxx.png，空则仅文字
};

/**
 * 宝箱房界面：全屏背景在「关闭 / 开启」两套图之间切换（无单独宝箱图层）。
 * 关闭态点击（或 Enter/Space）→ 开启态展示收获 +「继续」。
 */
class TreasureRoomUI {
public:
    explicit TreasureRoomUI(unsigned width, unsigned height);

    bool loadFont(const std::string& path);
    bool loadChineseFont(const std::string& path);

    /** 加载对应体型的开关图与可选遗物图标；并重置为关闭态 */
    void setup(const TreasureRoomDisplayData& data);

    bool handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos);
    void setMousePosition(sf::Vector2f pos);
    void draw(sf::RenderWindow& window);

    /** 开箱后玩家点击「继续」或等价按键后返回 true（每帧最多一次） */
    bool pollLeave();

private:
    void layoutContinueButton();
    bool hitContinue(const sf::Vector2f& p) const;

    unsigned width_;
    unsigned height_;

    sf::Font font_;
    bool fontLoaded_ = false;
    sf::Font fontChinese_;
    bool fontChineseLoaded_ = false;
    const sf::Font& fontForText() const { return fontChineseLoaded_ ? fontChinese_ : font_; }

    TreasureRoomDisplayData data_{};
    bool opened_ = false;

    sf::Texture closeTex_;
    sf::Texture openTex_;
    sf::Texture relicTex_;
    bool closeLoaded_ = false;
    bool openLoaded_ = false;
    bool relicLoaded_ = false;

    sf::Vector2f mousePos_{ -9999.f, -9999.f };
    sf::FloatRect continueBtnRect_{};
    bool pendingLeave_ = false;
};

} // namespace tce
