/**
 * 事件 / 商店 / 休息 统一 UI
 * 与 BattleUI 同风格：SFML 绘制、handleEvent、轮询用户选择
 */
#pragma once

#include "EventShopRestUIData.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace DataLayer { struct Event; }

namespace tce {

class EventShopRestUI {
public:
    EventShopRestUI(unsigned width, unsigned height);

    bool loadFont(const std::string& path);
    bool loadChineseFont(const std::string& path);

    void setScreen(EventShopRestScreen screen);
    EventShopRestScreen getScreen() const { return screen_; }

    void setEventData(const EventDisplayData& data);
    void setShopData(const ShopDisplayData& data);
    void setRestData(const RestDisplayData& data);

    /** 从 UTF-8 字符串设置事件展示（主流程从 Event 填充时用）；imagePath 为空则显示占位 */
    void setEventDataFromUtf8(const std::string& title, const std::string& description,
                              const std::vector<std::string>& optionTexts,
                              const std::string& imagePath = "",
                              const std::vector<std::string>& optionEffectTexts = {});

    /** 设置为“事件结果”页：同一界面展示结果文案与“确定”，可选传入 imagePath（支持 "__cardid:<id>"） */
    void setEventResultFromUtf8(const std::string& resultSummary, const std::string& imagePath = "");

    /** 从 DataLayer::Event 填充事件展示（主流程用 EventEngine::get_current_event() 取得后传入） */
    void setEventDataFromEvent(const DataLayer::Event* event);

    bool handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos);
    void setMousePosition(sf::Vector2f pos);
    void draw(sf::RenderWindow& window);

    // ---------- 轮询：每帧最多消费一次，主流程在 draw 之后调用 ----------
    /** 是否选择了事件选项；若 true，outIndex 为选项下标 (0-based) */
    bool pollEventOption(int& outIndex);
    /** 是否选择了购买某张牌；若 true，outCardId 为卡牌 id */
    bool pollShopBuyCard(CardId& outCardId);
    /** 是否选择了删除牌组中某张牌；若 true，outInstanceId 为实例 id */
    bool pollShopRemoveCard(InstanceId& outInstanceId);
    /** 是否选择了休息回血 */
    bool pollRestHeal();
    /** 是否选择了升级某张牌；若 true，outInstanceId 为实例 id */
    bool pollRestUpgradeCard(InstanceId& outInstanceId);

private:
    void drawEventScreen(sf::RenderWindow& window);
    void drawShopScreen(sf::RenderWindow& window);
    void drawRestScreen(sf::RenderWindow& window);
    void drawPanel(sf::RenderWindow& window, float centerX, float centerY, float w, float h);
    bool clickInRect(const sf::Vector2f& pos, const sf::FloatRect& r) const;

    unsigned width_;
    unsigned height_;
    sf::Font font_;
    bool fontLoaded_ = false;
    sf::Font fontChinese_;
    bool fontChineseLoaded_ = false;
    const sf::Font& fontForText() const { return fontChineseLoaded_ ? fontChinese_ : font_; }

    EventShopRestScreen screen_ = EventShopRestScreen::None;
    EventDisplayData eventData_;
    sf::Texture eventBgTexture_;
    bool eventBgLoaded_ = false;
    sf::Texture eventBannerTexture_;
    bool eventBannerLoaded_ = false;
    sf::Texture eventPanelFrameTexture_;
    bool eventPanelFrameLoaded_ = false;
    sf::Texture eventIllustTexture_;
    std::string eventIllustPath_;
    bool eventIllustLoaded_ = false;
    sf::Texture restBgTexture_;
    bool restBgLoaded_ = false;
    sf::Texture restHealIconTexture_;
    bool restHealIconLoaded_ = false;
    sf::Texture restSmithIconTexture_;
    bool restSmithIconLoaded_ = false;
    sf::Texture restScrollTexture_;   // 休息界面下方卷轴装饰（原云朵，路径仍为 cloud.png）
    bool restScrollLoaded_ = false;
    ShopDisplayData shopData_;
    RestDisplayData restData_;

    sf::Vector2f mousePos_{ -9999.f, -9999.f };

    // 选项/按钮矩形（按顺序），用于点击检测
    std::vector<sf::FloatRect> eventOptionRects_;
    std::vector<sf::FloatRect> shopBuyRects_;   // 与 forSale 下标对应
    std::vector<sf::FloatRect> shopRemoveRects_; // 与 deckForRemove 下标对应
    sf::FloatRect restHealButton_;
    sf::FloatRect restUpgradeChoiceButton_;  // 「升级」大按钮（进入选牌前）
    sf::FloatRect restBackButton_;           // 升级列表中「返回」
    std::vector<sf::FloatRect> restUpgradeRects_;
    bool restShowUpgradeList_ = false;       // true = 已选「升级」，显示选牌列表

    int eventOptionPressedIndex_ = -1;  // 当前按下的事件选项下标（用于按下态绘制）
    int selectedEventOption_ = 0;        // 键盘焦点选项下标
    float eventCardScrollOffset_ = 0.f;  // 事件“卡牌面板模式”滚动偏移
    float eventCardScrollMax_ = 0.f;     // 当前可滚动最大偏移
    float eventCardScrollStep_ = 42.f;   // 卡牌模式每档滚动步长（按行）

    // 待消费的选择（轮询一次即清空）
    int pendingEventOption_ = -1;
    CardId pendingShopBuyCard_;
    bool pendingShopBuy_ = false;
    InstanceId pendingShopRemoveInstance_ = -1;
    bool pendingShopRemove_ = false;
    bool pendingRestHeal_ = false;
    InstanceId pendingRestUpgradeInstance_ = -1;
    bool pendingRestUpgrade_ = false;
};

} // namespace tce
