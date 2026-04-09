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
                              const std::vector<std::string>& optionEffectTexts = {},
                              const std::vector<std::string>& optionCardIds = {});

    /** 设置为“事件结果”页：同一界面展示结果文案与“确定”；imagePath 支持 "__cardid:" / "__cards:" 等；为空则左侧仍显示原事件插图 */
    void setEventResultFromUtf8(const std::string& resultSummary, const std::string& imagePath = "");

    /** 从 DataLayer::Event 填充事件展示（主流程用 EventEngine::get_current_event() 取得后传入） */
    void setEventDataFromEvent(const DataLayer::Event* event);

    bool handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos);
    void setMousePosition(sf::Vector2f pos);
    void draw(sf::RenderWindow& window);
    /** 每帧在 setMousePosition 之后调用：锻造页拖动滚动条时平滑跟随鼠标 */
    void syncRestForgeScrollbarDrag(const sf::Vector2f& mousePos);
    /** 若当前为锻造升级确认层，关闭确认并返回 true（供 Escape 优先消费） */
    bool tryDismissRestForgeUpgradeConfirm();
    /** 若当前为商店净简删牌确认层，关闭确认并返回 true（供 Escape 优先消费） */
    bool tryDismissShopRemoveConfirm();

    // ---------- 轮询：每帧最多消费一次，主流程在 draw 之后调用 ----------
    /** 是否选择了事件选项；若 true，outIndex 为选项下标 (0-based) */
    bool pollEventOption(int& outIndex);
    /** 是否选择了购买某张牌；若 true，outCardId 为卡牌 id */
    bool pollShopBuyCard(CardId& outCardId);
    /** 是否点击「净简」准备支付（由主流程扣费后设 removeServicePaid） */
    bool pollShopPayRemoveService();
    /** 是否购买遗物；若 true，outIndex 为 relicsForSale 下标 */
    bool pollShopBuyRelic(int& outIndex);
    /** 是否购买药剂；若 true，outIndex 为 potionsForSale 下标 */
    bool pollShopBuyPotion(int& outIndex);
    /** 是否点击边缘「离开」离开商店 */
    bool pollShopLeave();
    /** 是否选择了删除牌组中某张牌；若 true，outInstanceId 为实例 id（需已支付净简） */
    bool pollShopRemoveCard(InstanceId& outInstanceId);
    /** 是否选择了休息回血 */
    bool pollRestHeal();
    /** 是否选择了升级某张牌；若 true，outInstanceId 为实例 id */
    bool pollRestUpgradeCard(InstanceId& outInstanceId);

private:
    void drawEventScreen(sf::RenderWindow& window);
    void drawShopScreen(sf::RenderWindow& window);
    void drawRestScreen(sf::RenderWindow& window);
    /** 锻造选牌后「升级确认」：双卡对比 + 确认/返回（在 drawRestScreen 内调用） */
    void drawRestForgeUpgradeConfirmOverlay(sf::RenderWindow& window);
    /** 商店净简选牌后「移除确认」：单卡放大预览（在 drawShopScreen 删牌页内调用） */
    void drawShopRemoveConfirmOverlay(sf::RenderWindow& window);
    /** 成功加载普通事件插图时写入，供结果页/预览缺省时不留白 */
    void syncEventIllustrationSceneBackup(const std::string& path);
    void drawPanel(sf::RenderWindow& window, float centerX, float centerY, float w, float h);
    /**
     * 主牌组选卡网格（商店删牌 / 休息锻造共用）：5 列、裁剪、滚轮偏移；可选顶部提示行。
     * previewUpgrade：为 true 时在数据存在时绘制 id+「+」的预览卡面（锻造「查看升级」）。
     */
    void drawMasterDeckCardPickGrid(sf::RenderWindow& window, const std::vector<MasterDeckCardDisplay>& deck,
        const sf::String& tipText, bool drawTipLine, float contentLeft, float contentW, float regionTop,
        float regionBottom, float clipTop, float clipBottom, float scale, unsigned bodySize, float layoutW,
        float layoutH, float& scrollOffset, float& scrollMax, float& cardScrollStep,
        std::vector<sf::FloatRect>& outHitRects, sf::FloatRect& outListViewportRect, bool previewUpgrade,
        bool forgeOrangeHover, bool showCardDescription);
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
    sf::Texture eventIllustSceneBackupTexture_;
    bool eventIllustSceneBackupLoaded_ = false;
    sf::Texture restBgTexture_;
    bool restBgLoaded_ = false;
    sf::Texture shopBgTexture_;
    bool shopBgLoaded_ = false;
    sf::Texture shopTitleTexture_;
    bool shopTitleLoaded_ = false;
    sf::Texture shopDeleteCardTexture_;
    bool shopDeleteCardLoaded_ = false;
    sf::Texture shopDeleteBgTexture_;
    bool shopDeleteBgLoaded_ = false;
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
    std::vector<sf::FloatRect> shopBuyRects_;       // 与 forSale 下标对应
    std::vector<sf::FloatRect> shopColorlessRects_; // 与 colorlessForSale 下标对应
    std::vector<sf::FloatRect> shopRelicRects_;
    std::vector<sf::FloatRect> shopPotionRects_;
    sf::FloatRect shopRemoveServiceRect_;
    sf::FloatRect shopLeaveButtonRect_;
    std::vector<sf::FloatRect> shopRemoveRects_;   // 与 deckForRemove 下标对应
    float shopDeckScrollOffset_ = 0.f;
    float shopDeckScrollMax_ = 0.f;
    sf::FloatRect shopDeckAreaRect_;
    bool shopRemoveConfirmOpen_ = false;
    InstanceId shopRemoveConfirmInstanceId_ = 0;
    sf::FloatRect shopRemoveConfirmBackRect_;
    sf::FloatRect shopRemoveConfirmOkRect_;
    sf::FloatRect restHealButton_;
    sf::FloatRect restUpgradeChoiceButton_;  // 「升级」大按钮（进入选牌前）
    sf::FloatRect restBackButton_;           // 升级列表中「返回」
    std::vector<sf::FloatRect> restUpgradeRects_;
    bool restShowUpgradeList_ = false;       // true = 已选「升级」，显示选牌列表
    float restDeckPickScrollOffset_ = 0.f;
    float restDeckPickScrollMax_ = 0.f;
    sf::FloatRect restForgeListViewportRect_;
    sf::FloatRect restForgeScrollbarTrackRect_;
    sf::FloatRect restForgeScrollbarThumbRect_;
    float restForgeScrollbarThumbH_ = 0.f;
    bool restForgeViewUpgrade_ = false;
    sf::FloatRect restForgeViewUpgradeToggleRect_;
    bool restForgeScrollbarDragging_ = false;
    float restForgeScrollbarGrabY_ = 0.f;
    bool restForgeUpgradeConfirmOpen_ = false;
    InstanceId restForgeConfirmInstanceId_ = 0;
    sf::FloatRect restForgeConfirmOkRect_;

    int eventOptionPressedIndex_ = -1;  // 当前按下的事件选项下标（用于按下态绘制）
    int selectedEventOption_ = 0;        // 键盘焦点选项下标
    float eventCardScrollOffset_ = 0.f;  // 事件“卡牌面板模式”滚动偏移
    float eventCardScrollMax_ = 0.f;     // 当前可滚动最大偏移
    float eventCardScrollStep_ = 42.f;   // 卡牌模式每档滚动步长（按行）

    // 待消费的选择（轮询一次即清空）
    int pendingEventOption_ = -1;
    CardId pendingShopBuyCard_;
    bool pendingShopBuy_ = false;
    bool pendingShopPayRemove_ = false;
    int pendingShopRelicIndex_ = -1;
    bool pendingShopRelic_ = false;
    int pendingShopPotionIndex_ = -1;
    bool pendingShopPotion_ = false;
    bool pendingShopLeave_ = false;
    InstanceId pendingShopRemoveInstance_ = -1;
    bool pendingShopRemove_ = false;
    bool pendingRestHeal_ = false;
    InstanceId pendingRestUpgradeInstance_ = -1;
    bool pendingRestUpgrade_ = false;
};

} // namespace tce
