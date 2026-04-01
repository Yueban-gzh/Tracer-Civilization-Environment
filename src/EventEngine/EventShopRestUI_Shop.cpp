/**
 * 商店界面：布局骨架对齐《杀戮尖塔》商户界面（仅坐标/比例，不换皮）：
 * 上排 5 卡为主视觉；下半左 2 副卡与上排前两列对齐；中右为遗物/灵液两行；最右竖条为净简（仅下半区高度）；
 * 「离开」贴在面板左缘外侧偏下。主商品在净简左侧 mainW 内排布；遗物/灵液三格总宽与左下两卡列等宽；净简与整段商品区（上排至下半底）等高。
 * 内边距对齐事件界面 illustMarginH。无顶栏 HUD、无底部删牌区。
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include "DataLayer/DataLayer.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

namespace tce {
using namespace esr_detail;

namespace {

constexpr float BATTLE_CARD_ASPECT_H_OVER_W = 300.f / 190.f;
// 全店统一：价格在商品几何体下方的间距规则（与 StS「价签在物下」一致）
constexpr float kPriceGapBelowCard = 12.f;
constexpr float kPriceReserveBelowCard = 20.f; // 价签字形占用（略加大以配合价格字号）
// 遗物/灵液：框下名称 + 价签预留高度（与 draw_icon_offer 一致）
constexpr float kOfferGapBelowBox = 4.f;
constexpr float kOfferGapNameToPrice = 3.f;

sf::Color rarity_outline(Rarity r) {
    switch (r) {
    case Rarity::Uncommon: return sf::Color(80, 180, 120);
    case Rarity::Rare: return sf::Color(230, 180, 70);
    case Rarity::Special: return sf::Color(180, 100, 220);
    default: return sf::Color(210, 175, 95);
    }
}

void draw_card_tile(sf::RenderWindow& window, const sf::Font& font, const sf::Vector2f& mousePos,
    CardId cid, int price, const sf::FloatRect& rect, bool canAfford, unsigned shopPriceFont,
    std::wstring& outTooltip, bool& outHover) {
    const CardData* cd = get_card_by_id(cid);
    outHover = rect.contains(mousePos);
    outTooltip.clear();

    const sf::Color borderCol = cd ? rarity_outline(cd->rarity) : sf::Color(160, 150, 140);
    sf::RectangleShape bg(rect.size);
    bg.setPosition(rect.position);
    bg.setFillColor(sf::Color(55, 50, 48));
    bg.setOutlineColor(outHover ? sf::Color(255, 230, 160) : borderCol);
    bg.setOutlineThickness(outHover ? 2.5f : 2.f);
    window.draw(bg);

    const float w = rect.size.x;
    const float h = rect.size.y;
    const float pad = 5.f;
    const float innerL = rect.position.x + pad;
    const float innerT = rect.position.y + pad;
    const float innerW = w - pad * 2.f;
    const float titleH = std::max(16.f, h * 0.10f);
    const float artH = std::max(22.f, h * 0.22f);
    const float typeH = std::max(14.f, h * 0.07f);

    sf::RectangleShape titleBar(sf::Vector2f(innerW - 6.f, titleH));
    titleBar.setPosition(sf::Vector2f(innerL + 3.f, innerT + 4.f));
    titleBar.setFillColor(sf::Color(72, 68, 65));
    titleBar.setOutlineColor(sf::Color(90, 85, 82));
    titleBar.setOutlineThickness(1.f);
    window.draw(titleBar);

    const float artTop = innerT + 4.f + titleH + 3.f;
    sf::ConvexShape artPanel;
    artPanel.setPointCount(8);
    artPanel.setPoint(0, sf::Vector2f(innerL, artTop));
    artPanel.setPoint(1, sf::Vector2f(innerL + innerW, artTop));
    artPanel.setPoint(2, sf::Vector2f(innerL + innerW, artTop + artH - 6.f));
    artPanel.setPoint(3, sf::Vector2f(innerL + innerW * 0.75f, artTop + artH));
    artPanel.setPoint(4, sf::Vector2f(innerL + innerW * 0.5f, artTop + artH - 6.f));
    artPanel.setPoint(5, sf::Vector2f(innerL + innerW * 0.25f, artTop + artH));
    artPanel.setPoint(6, sf::Vector2f(innerL, artTop + artH - 6.f));
    artPanel.setPoint(7, sf::Vector2f(innerL, artTop));
    artPanel.setFillColor(sf::Color(95, 42, 38));
    artPanel.setOutlineColor(sf::Color(120, 55, 48));
    artPanel.setOutlineThickness(1.f);
    window.draw(artPanel);

    const float typeTop = artTop + artH + 3.f;
    sf::RectangleShape typeBar(sf::Vector2f(innerW - 10.f, typeH));
    typeBar.setPosition(sf::Vector2f(innerL + 5.f, typeTop));
    typeBar.setFillColor(sf::Color(72, 68, 65));
    typeBar.setOutlineThickness(1.f);
    typeBar.setOutlineColor(sf::Color(90, 85, 82));
    window.draw(typeBar);

    const sf::String name = sf::String(cd ? utf8_to_wstring(cd->name) : utf8_to_wstring(cid));
    sf::Text nameText(font, name, static_cast<unsigned>(std::max(10.f, h * 0.055f)));
    nameText.setFillColor(sf::Color::White);
    const sf::FloatRect nb = nameText.getLocalBounds();
    nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
    nameText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, innerT + 4.f + titleH * 0.5f));
    window.draw(nameText);

    sf::Text typeText(font, sf::String(L"招式"), static_cast<unsigned>(std::max(9.f, h * 0.042f)));
    typeText.setFillColor(sf::Color(220, 215, 205));
    const sf::FloatRect tb = typeText.getLocalBounds();
    typeText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    typeText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, typeTop + typeH * 0.5f));
    window.draw(typeText);

    if (cd) {
        const float costR = std::max(9.f, h * 0.048f);
        const float costCx = innerL + costR;
        const float costCy = innerT + costR;
        sf::CircleShape costCircle(costR);
        costCircle.setPosition(sf::Vector2f(costCx - costR, costCy - costR));
        costCircle.setFillColor(sf::Color(200, 55, 50));
        costCircle.setOutlineColor(sf::Color(255, 190, 90));
        costCircle.setOutlineThickness(1.f);
        window.draw(costCircle);
        sf::String costStr = (cd->cost < 0) ? sf::String(L"X") : sf::String(std::to_string(cd->cost));
        sf::Text costText(font, costStr, static_cast<unsigned>(std::max(8.f, h * 0.045f)));
        costText.setFillColor(sf::Color::White);
        const sf::FloatRect cb = costText.getLocalBounds();
        costText.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
        costText.setPosition(sf::Vector2f(costCx, costCy));
        window.draw(costText);
    }

    std::wstring priceStr = std::to_wstring(price) + L" 文钱";
    sf::Text priceText(font, sf::String(priceStr), shopPriceFont);
    priceText.setFillColor(canAfford ? sf::Color(255, 220, 120) : sf::Color(160, 150, 140));
    const sf::FloatRect pb = priceText.getLocalBounds();
    priceText.setOrigin(sf::Vector2f(pb.position.x + pb.size.x * 0.5f, pb.position.y + pb.size.y * 0.5f));
    priceText.setPosition(sf::Vector2f(rect.position.x + rect.size.x * 0.5f,
                                       rect.position.y + rect.size.y + kPriceGapBelowCard));
    window.draw(priceText);
}

// 战斗系统卡面（复用 StS 风格）：无价签、不绘制描述，用于商店删牌选择页
void draw_battle_card_tile_no_price(sf::RenderWindow& window, const sf::Font& font,
    const sf::Vector2f& mousePos, CardId cid, const std::wstring& fallbackName,
    const sf::FloatRect& rect, bool& outHover) {
    const CardData* cd = get_card_by_id(cid);
    outHover = rect.contains(mousePos);

    const sf::Color borderCol = cd ? rarity_outline(cd->rarity) : sf::Color(160, 150, 140);
    sf::RectangleShape bg(rect.size);
    bg.setPosition(rect.position);
    bg.setFillColor(sf::Color(55, 50, 48));
    bg.setOutlineColor(outHover ? sf::Color(255, 230, 160) : borderCol);
    bg.setOutlineThickness(outHover ? 2.5f : 2.f);
    window.draw(bg);

    const float w = rect.size.x;
    const float h = rect.size.y;
    const float pad = 5.f;
    const float innerL = rect.position.x + pad;
    const float innerT = rect.position.y + pad;
    const float innerW = w - pad * 2.f;
    const float titleH = std::max(16.f, h * 0.10f);
    const float artH = std::max(22.f, h * 0.22f);
    const float typeH = std::max(14.f, h * 0.07f);

    sf::RectangleShape titleBar(sf::Vector2f(innerW - 6.f, titleH));
    titleBar.setPosition(sf::Vector2f(innerL + 3.f, innerT + 4.f));
    titleBar.setFillColor(sf::Color(72, 68, 65));
    titleBar.setOutlineColor(sf::Color(90, 85, 82));
    titleBar.setOutlineThickness(1.f);
    window.draw(titleBar);

    const float artTop = innerT + 4.f + titleH + 3.f;
    sf::ConvexShape artPanel;
    artPanel.setPointCount(8);
    artPanel.setPoint(0, sf::Vector2f(innerL, artTop));
    artPanel.setPoint(1, sf::Vector2f(innerL + innerW, artTop));
    artPanel.setPoint(2, sf::Vector2f(innerL + innerW, artTop + artH - 6.f));
    artPanel.setPoint(3, sf::Vector2f(innerL + innerW * 0.75f, artTop + artH));
    artPanel.setPoint(4, sf::Vector2f(innerL + innerW * 0.5f, artTop + artH - 6.f));
    artPanel.setPoint(5, sf::Vector2f(innerL + innerW * 0.25f, artTop + artH));
    artPanel.setPoint(6, sf::Vector2f(innerL, artTop + artH - 6.f));
    artPanel.setPoint(7, sf::Vector2f(innerL, artTop));
    artPanel.setFillColor(sf::Color(95, 42, 38));
    artPanel.setOutlineColor(sf::Color(120, 55, 48));
    artPanel.setOutlineThickness(1.f);
    window.draw(artPanel);

    const float typeTop = artTop + artH + 3.f;
    sf::RectangleShape typeBar(sf::Vector2f(innerW - 10.f, typeH));
    typeBar.setPosition(sf::Vector2f(innerL + 5.f, typeTop));
    typeBar.setFillColor(sf::Color(72, 68, 65));
    typeBar.setOutlineThickness(1.f);
    typeBar.setOutlineColor(sf::Color(90, 85, 82));
    window.draw(typeBar);

    const sf::String name = cd ? sf::String(utf8_to_wstring(cd->name)) : sf::String(fallbackName);
    sf::Text nameText(font, name, static_cast<unsigned>(std::max(10.f, h * 0.055f)));
    nameText.setFillColor(sf::Color::White);
    const sf::FloatRect nb = nameText.getLocalBounds();
    nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
    nameText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, innerT + 4.f + titleH * 0.5f));
    window.draw(nameText);

    sf::Text typeText(font, sf::String(L"招式"), static_cast<unsigned>(std::max(9.f, h * 0.042f)));
    typeText.setFillColor(sf::Color(220, 215, 205));
    const sf::FloatRect tb = typeText.getLocalBounds();
    typeText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    typeText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, typeTop + typeH * 0.5f));
    window.draw(typeText);

    if (cd) {
        const float costR = std::max(9.f, h * 0.048f);
        const float costCx = innerL + costR;
        const float costCy = innerT + costR;
        sf::CircleShape costCircle(costR);
        costCircle.setPosition(sf::Vector2f(costCx - costR, costCy - costR));
        costCircle.setFillColor(sf::Color(200, 55, 50));
        costCircle.setOutlineColor(sf::Color(255, 190, 90));
        costCircle.setOutlineThickness(1.f);
        window.draw(costCircle);

        sf::String costStr = (cd->cost < 0) ? sf::String(L"X") : sf::String(std::to_string(cd->cost));
        sf::Text costText(font, costStr, static_cast<unsigned>(std::max(8.f, h * 0.045f)));
        costText.setFillColor(sf::Color::White);
        const sf::FloatRect cb = costText.getLocalBounds();
        costText.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
        costText.setPosition(sf::Vector2f(costCx, costCy));
        window.draw(costText);
    }
}

void draw_icon_offer(sf::RenderWindow& window, const sf::Font& font, const sf::Vector2f& mousePos,
    const sf::FloatRect& rect, const std::wstring& name, int price, bool relicStyle, bool canAfford,
    unsigned nameFontSize, unsigned priceFontSize,
    std::wstring& outTooltip, bool& outHover) {
    outHover = rect.contains(mousePos);
    outTooltip.clear();

    const sf::Color fill = relicStyle ? sf::Color(55, 48, 38) : sf::Color(38, 48, 58);
    const sf::Color edge = relicStyle ? sf::Color(200, 170, 90) : sf::Color(130, 180, 210);
    sf::RectangleShape box(rect.size);
    box.setPosition(rect.position);
    box.setFillColor(fill);
    box.setOutlineColor(outHover ? sf::Color(255, 235, 180) : edge);
    box.setOutlineThickness(outHover ? 2.5f : 2.f);
    window.draw(box);

    sf::Text shortName(font, truncate_to_width(font, sf::String(name), nameFontSize, rect.size.x - 2.f), nameFontSize);
    shortName.setFillColor(sf::Color(220, 215, 208));
    const sf::FloatRect nb = shortName.getLocalBounds();
    shortName.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y));
    const float nameTopY = rect.position.y + rect.size.y + kOfferGapBelowBox;
    shortName.setPosition(sf::Vector2f(rect.position.x + rect.size.x * 0.5f, nameTopY));
    window.draw(shortName);

    std::wstring ps = std::to_wstring(price);
    sf::Text pt(font, sf::String(ps), priceFontSize);
    pt.setFillColor(canAfford ? sf::Color(255, 210, 100) : sf::Color(140, 135, 130));
    const sf::FloatRect pbb = pt.getLocalBounds();
    pt.setOrigin(sf::Vector2f(pbb.position.x + pbb.size.x * 0.5f, pbb.position.y));
    const float priceTopY = nameTopY + nb.size.y + kOfferGapNameToPrice;
    pt.setPosition(sf::Vector2f(rect.position.x + rect.size.x * 0.5f, priceTopY));
    window.draw(pt);
}

} // namespace

void EventShopRestUI::drawShopScreen(sf::RenderWindow& window) {
    // 使用当前视图尺寸而非构造时的 width_/height_：全屏或缩放窗口后二者会不一致，否则面板偏置、删牌区 scissor 与几何错位
    const sf::Vector2f layoutSize = window.getView().getSize();
    const float lw = std::max(1.f, layoutSize.x);
    const float lh = std::max(1.f, layoutSize.y);

    // 整体较前一版缩小约 1/5（×0.8），与事件面板比例仍可同屏协调
    const float panelW = std::max(640.f, lw * 0.56f);
    const float scale = panelW / PANEL_W;
    const unsigned titleSize = static_cast<unsigned>(std::round(TITLE_CHAR_SIZE * scale));
    const unsigned bodySize = static_cast<unsigned>(std::round(BODY_CHAR_SIZE * scale));
    // 全店统一价签；遗物/灵液名称比价签略大
    const unsigned shopPriceFont =
        static_cast<unsigned>(std::clamp(static_cast<int>(bodySize) - 4, 12, 15));
    const unsigned shopOfferNameFont =
        static_cast<unsigned>(std::clamp(static_cast<int>(bodySize) - 1, 15, 20));

    const float cx = lw * 0.5f;
    const float panelH = std::clamp(lh * 0.624f, 448.f, 656.f);
    const float cy = lh * 0.5f;
    const float panelTop = cy - panelH * 0.5f;
    const float panelLeft = cx - panelW * 0.5f;
    const float bannerBoxH = BANNER_H + BANNER_EXTRA_H;
    constexpr float BANNER_INNER_OVERHANG = 12.f;

    draw_panel_gradient(window, cx, cy, panelW, panelH, PANEL_BG_TOP, PANEL_BG_BOTTOM);
    sf::RectangleShape panelOutline(sf::Vector2f(panelW, panelH));
    panelOutline.setOrigin(sf::Vector2f(panelW * 0.5f, panelH * 0.5f));
    panelOutline.setPosition(sf::Vector2f(cx, cy));
    panelOutline.setFillColor(sf::Color::Transparent);
    panelOutline.setOutlineThickness(2.f);
    panelOutline.setOutlineColor(PANEL_OUTLINE);
    window.draw(panelOutline);
    sf::RectangleShape innerBorder(sf::Vector2f(panelW - 6.f, panelH - 6.f));
    innerBorder.setOrigin(sf::Vector2f(innerBorder.getSize().x * 0.5f, innerBorder.getSize().y * 0.5f));
    innerBorder.setPosition(sf::Vector2f(cx, cy));
    innerBorder.setFillColor(sf::Color::Transparent);
    innerBorder.setOutlineThickness(1.f);
    innerBorder.setOutlineColor(PANEL_INNER);
    window.draw(innerBorder);
    sf::RectangleShape innerWarm(sf::Vector2f(panelW - 14.f, panelH - 14.f));
    innerWarm.setOrigin(sf::Vector2f(innerWarm.getSize().x * 0.5f, innerWarm.getSize().y * 0.5f));
    innerWarm.setPosition(sf::Vector2f(cx, cy));
    innerWarm.setFillColor(sf::Color::Transparent);
    innerWarm.setOutlineThickness(1.f);
    innerWarm.setOutlineColor(sf::Color(PANEL_INNER_WARM.r, PANEL_INNER_WARM.g, PANEL_INNER_WARM.b, 90));
    window.draw(innerWarm);

    constexpr float FRAME_MARGIN_SIDES = 28.f;
    constexpr float FRAME_MARGIN_TOP = 56.f;
    constexpr float FRAME_SCALE = 1.22f;
    const float baseW = panelW + FRAME_MARGIN_SIDES * 2.f;
    const float baseH = panelH + FRAME_MARGIN_TOP + FRAME_MARGIN_SIDES;
    const float frameWidth = baseW * FRAME_SCALE;
    const float frameHeight = baseH * FRAME_SCALE;
    const float frameLeft = cx - frameWidth * 0.5f;
    const float frameTop = cy - frameHeight * 0.5f;
    if (eventPanelFrameLoaded_) {
        draw_texture_fit(window, eventPanelFrameTexture_,
            sf::FloatRect(sf::Vector2f(frameLeft, frameTop), sf::Vector2f(frameWidth, frameHeight)));
    }

    const float bannerY = panelTop - BANNER_INNER_OVERHANG;
    const float shopTitleOffsetY = -50.f;          // 标题条整体上移一点
    const float shopTitleHeightScale = 2.f;     // 竖向略增宽
    // 与 EventShopRestUI_Event：contentTop = bannerY + bannerBoxH + 2.f 一致；主内容区顶距同事件插图区 illustMarginV
    const float contentTop = bannerY + bannerBoxH + 2.f;
    const float bannerW = panelW + BANNER_EXTRA_W * 2.f;
    const float bannerDrawY = bannerY + shopTitleOffsetY;
    const float bannerDrawH = bannerBoxH * shopTitleHeightScale;
    const sf::FloatRect bannerDest(
        sf::Vector2f(cx - bannerW * 0.5f, bannerDrawY), sf::Vector2f(bannerW, bannerDrawH));
    if (shopTitleLoaded_) {
        // 复用事件羊皮卷逻辑：同位置同尺寸，仅替换纹理
        draw_texture_fit(window, shopTitleTexture_, bannerDest);
    } else if (eventBannerLoaded_) {
        draw_texture_fit(window, eventBannerTexture_, bannerDest);
    } else {
        sf::RectangleShape banner(sf::Vector2f(bannerW, bannerDrawH));
        banner.setOrigin(sf::Vector2f(bannerW * 0.5f, 0.f));
        banner.setPosition(sf::Vector2f(cx, bannerDrawY));
        banner.setFillColor(BANNER_BG);
        banner.setOutlineThickness(2.f);
        banner.setOutlineColor(BANNER_OUTLINE);
        window.draw(banner);
    }

    sf::Text bannerTitle(fontForText(), sf::String(L"学宫集"), titleSize);
    bannerTitle.setFillColor(sf::Color::White);
    bannerTitle.setStyle(sf::Text::Bold);
    const sf::FloatRect btb = bannerTitle.getLocalBounds();
    bannerTitle.setOrigin(sf::Vector2f(btb.position.x + btb.size.x * 0.5f, btb.position.y + btb.size.y * 0.5f));
    bannerTitle.setPosition(sf::Vector2f(cx, bannerDrawY + bannerDrawH * 0.5f));
    window.draw(bannerTitle);

    // 与 EventShopRestUI_Event：内边距 illustMarginH=20、illustMarginV=24 同量级（见事件插图区）
    const float innerPad = 14.f * scale;
    constexpr float kEventMerchInsetH = 20.f;
    const float merchInsetL = kEventMerchInsetH * scale;
    const float merchInsetR = kEventMerchInsetH * scale;
    const float contentLeft = panelLeft + innerPad + merchInsetL;
    const float contentRight = panelLeft + panelW - innerPad - merchInsetR;
    const float contentW = contentRight - contentLeft;
    const float leaveBtnW = 112.f;
    const float leaveBtnH = 40.f;
    const float leaveReserve = 12.f;
    auto drawLeaveButton = [&]() {
        const float leaveStick = 46.f;
        const float lx = panelLeft - leaveStick;
        const float ly = panelTop + panelH * 0.61f - leaveBtnH * 0.5f;
        shopLeaveButtonRect_ = sf::FloatRect(sf::Vector2f(lx, ly), sf::Vector2f(leaveBtnW, leaveBtnH));
        bool lh = shopLeaveButtonRect_.contains(mousePos_);
        sf::RectangleShape leaveBox(sf::Vector2f(leaveBtnW, leaveBtnH));
        leaveBox.setPosition(sf::Vector2f(lx, ly));
        leaveBox.setFillColor(lh ? sf::Color(118, 46, 40) : sf::Color(92, 38, 34));
        leaveBox.setOutlineThickness(2.f);
        leaveBox.setOutlineColor(sf::Color(195, 155, 115));
        window.draw(leaveBox);
        sf::Text lt(fontForText(), sf::String(L"离开"), static_cast<unsigned>(bodySize - 1u));
        lt.setFillColor(sf::Color(250, 240, 228));
        const sf::FloatRect lb = lt.getLocalBounds();
        lt.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
        lt.setPosition(sf::Vector2f(lx + leaveBtnW * 0.5f, ly + leaveBtnH * 0.5f));
        window.draw(lt);
    };

    const float clusterGap = 16.f;
    float removeW = std::clamp(contentW * 0.092f * 2.f, 164.f, 200.f);
    removeW = std::min(removeW, std::max(140.f, contentW * 0.38f));
    const float removeRightInset = 6.f;
    const float removeLeft = contentRight - removeW - removeRightInset;
    const float mainW = std::max(120.f, removeLeft - clusterGap - contentLeft);

    constexpr float kShopTopContentPad = 24.f;
    const float contentTopPad = kShopTopContentPad * scale;
    const float contentTopEffective = contentTop + contentTopPad;
    const float contentBottom = panelTop + panelH - innerPad - leaveReserve;
    const float usableH = std::max(120.f, contentBottom - contentTopEffective);
    const bool showRemoveDeckPage = (shopData_.removeServicePaid && !shopData_.removeServiceSoldOut);
    if (showRemoveDeckPage) {
        // 仅显示删牌页：一排 5 张，可滚动，点击即提交删除
        shopBuyRects_.clear();
        shopColorlessRects_.clear();
        shopRelicRects_.clear();
        shopPotionRects_.clear();
        shopRemoveServiceRect_ = sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f));

        // 提示区与卡牌整体上移：仅在删牌子页生效
        const float removePageShiftUp = 18.f * scale;
        const float clipTop = panelTop + 8.f * scale;
        const float clipBottom = panelTop + panelH - 8.f * scale;
        const float pageTop = std::max(clipTop, std::max(contentTop + 4.f, contentTopEffective + 6.f - removePageShiftUp));
        const float pageBottom = std::min(clipBottom, contentBottom - 8.f);
        const float pageH = std::max(120.f, pageBottom - pageTop);
        // 按需求：删牌选择页不额外铺背景（只画卡面本体）
        sf::Text tip(fontForText(), sf::String(L"请选择一张卡牌进行移除"), static_cast<unsigned>(bodySize));
        tip.setFillColor(sf::Color(236, 227, 206));
        tip.setOutlineColor(sf::Color(20, 18, 24));
        tip.setOutlineThickness(1.f);
        tip.setPosition(sf::Vector2f(contentLeft + 4.f, pageTop));
        window.draw(tip);

        // 卡牌底板有描边外扩：为避免“漏出木框”，可视窗口向内收缩一点点
        const float listTopRaw = pageTop + std::max(20.f,
            static_cast<float>(fontForText().getLineSpacing(bodySize)) + 8.f);
        const float listTop = std::max(listTopRaw, clipTop + 10.f * scale);
        const float listBottom = std::min(pageBottom, clipBottom - 10.f * scale);
        const float listH = std::max(50.f, listBottom - listTop);
        shopDeckAreaRect_ = sf::FloatRect(sf::Vector2f(contentLeft, listTop), sf::Vector2f(contentW, listH));

        constexpr int kCols = 5;
        const float sidePadX = 18.f * scale; // 两侧留白
        const float gapX = 16.f * scale;     // 牌间距更大
        const float gapY = 14.f * scale;
        // 进一步缩小卡面，避免出框；并保持 190:300 比例
        const float baseTileW = (contentW - sidePadX * 2.f - gapX * (kCols - 1)) / static_cast<float>(kCols);
        const float shrinkMul = 0.80f;
        const float tileW = std::max(62.f, baseTileW * shrinkMul);
        const float tileH = tileW * BATTLE_CARD_ASPECT_H_OVER_W;
        const int n = static_cast<int>(shopData_.deckForRemove.size());
        const int rows = (n + kCols - 1) / kCols;
        const float totalH = rows > 0 ? (rows * tileH + (rows - 1) * gapY) : 0.f;
        shopDeckScrollMax_ = std::max(0.f, totalH - listH);
        if (shopDeckScrollOffset_ > shopDeckScrollMax_) shopDeckScrollOffset_ = shopDeckScrollMax_;
        if (shopDeckScrollOffset_ < 0.f) shopDeckScrollOffset_ = 0.f;
        eventCardScrollStep_ = tileH + gapY;
        shopRemoveRects_.assign(shopData_.deckForRemove.size(),
            sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f)));

        // 卡牌仅部分落在可视区内时仍会整卡绘制，描边会穿出底板；用裁剪区限制到 list 矩形（与事件删牌区思路一致，但需真正 scissor）
        const sf::View viewBeforeCards = window.getView();
        if (listH > 1.f && contentW > 1.f) {
            sf::View clipView = window.getDefaultView();
            clipView.setScissor(sf::FloatRect(sf::Vector2f(contentLeft / lw, listTop / lh),
                sf::Vector2f(contentW / lw, listH / lh)));
            window.setView(clipView);
        }

        for (int i = 0; i < n; ++i) {
            const int row = i / kCols;
            const int col = i % kCols;
            const float rowDrawW = static_cast<float>(kCols) * tileW + static_cast<float>(kCols - 1) * gapX;
            const float startX = contentLeft + (contentW - rowDrawW) * 0.5f;
            const float x = startX + col * (tileW + gapX);
            const float y = listTop + row * (tileH + gapY) - shopDeckScrollOffset_;
            sf::FloatRect r(sf::Vector2f(x, y), sf::Vector2f(tileW, tileH));
            shopRemoveRects_[static_cast<size_t>(i)] = r;
            if (r.position.y + r.size.y < listTop || r.position.y > listBottom) continue;

            const auto& dc = shopData_.deckForRemove[static_cast<size_t>(i)];
            bool hover = false;
            draw_battle_card_tile_no_price(window, fontForText(), mousePos_, dc.cardId, dc.cardName, r, hover);
        }

        window.setView(viewBeforeCards);

        drawLeaveButton();
        return;
    }

    const float gap12 = std::max(15.f, usableH * 0.028f);
    constexpr float kRow1Share = 0.50f;
    float row1BlockH = (usableH - gap12) * kRow1Share;
    float row2BlockH = usableH - gap12 - row1BlockH;

    const float row1Y = contentTopEffective;
    const float cardOfferBottom = kPriceGapBelowCard + kPriceReserveBelowCard;
    const float row1TopPad = 6.f;
    const float maxCardH = std::max(52.f, row1BlockH - row1TopPad - cardOfferBottom);
    const float cardGap = 11.f;
    const size_t nCards = std::min<std::size_t>(5, shopData_.forSale.size());

    constexpr float kRowSidePad = 6.f;
    const float availRowW = mainW - 2.f * kRowSidePad;
    float cardW = (availRowW - 4.f * cardGap) / 5.f;
    float cardH = cardW * BATTLE_CARD_ASPECT_H_OVER_W;
    if (cardH > maxCardH) {
        cardH = maxCardH;
        cardW = cardH / BATTLE_CARD_ASPECT_H_OVER_W;
    }
    const float topRowW = 5.f * cardW + 4.f * cardGap;
    const float row1StartX = contentLeft + kRowSidePad + (availRowW - topRowW) * 0.5f;

    const float cardBlockH = row1TopPad + cardH + cardOfferBottom;
    const float row1CardY = row1Y + row1TopPad;
    shopBuyRects_.clear();
    shopBuyRects_.resize(nCards);
    for (size_t i = 0; i < nCards; ++i) {
        const float x = row1StartX + static_cast<float>(i) * (cardW + cardGap);
        const sf::FloatRect crect(sf::Vector2f(x, row1CardY), sf::Vector2f(cardW, cardH));
        shopBuyRects_[i] = crect;
        const auto& off = shopData_.forSale[i];
        bool canAfford = shopData_.playerGold >= off.price;
        std::wstring tip;
        bool hover = false;
        draw_card_tile(window, fontForText(), mousePos_, off.id, off.price, crect, canAfford, shopPriceFont, tip,
            hover);
    }

    // ---------- 下半：左 2 副卡 | 中遗物/灵液 | 右净简（底留白上移整块） ----------
    const float row2TopY = row1Y + row1BlockH + gap12;
    const float row2ContentBottomPad = 24.f * scale;
    const float row2LayoutH = std::max(80.f, row2BlockH - row2ContentBottomPad);
    const float leftBlockW = 2.f * cardW + cardGap;
    const float midLeft = row1StartX + leftBlockW + clusterGap;
    const float midWAvailable = std::max(0.f, removeLeft - clusterGap - midLeft);

    // 遗物标题略大、灵液标题更弱（三级视觉）；两行标题带高度分别计算，避免「居中模块感」
    float relicTitleBand = std::max(16.f, bodySize * 0.82f);
    float potionTitleBand = std::max(13.f, bodySize * 0.68f);
    float relicLblGap = std::max(4.f, row2LayoutH * 0.014f);
    float potionLblGap = std::max(3.f, row2LayoutH * 0.012f);
    float interRowGap = std::max(10.f, row2LayoutH * 0.028f);
    // 框下名称+价签：按字体行高预留，避免与方块或其它行重叠
    const sf::Font& shopFont = fontForText();
    const float kOfferNameH = static_cast<float>(shopFont.getLineSpacing(shopOfferNameFont));
    const float kOfferPriceH = static_cast<float>(shopFont.getLineSpacing(shopPriceFont));
    const float kOfferBelowTotal =
        kOfferGapBelowBox + kOfferNameH + kOfferGapNameToPrice + kOfferPriceH + 3.f;
    float rowItemH = (row2LayoutH - relicTitleBand - relicLblGap - potionTitleBand - potionLblGap - interRowGap) * 0.5f;
    if (rowItemH < kOfferBelowTotal + 30.f) {
        relicTitleBand = std::max(14.f, row2LayoutH * 0.075f);
        potionTitleBand = std::max(12.f, row2LayoutH * 0.062f);
        relicLblGap = 4.f;
        potionLblGap = 3.f;
        interRowGap = std::max(8.f, row2LayoutH * 0.022f);
        rowItemH = (row2LayoutH - relicTitleBand - relicLblGap - potionTitleBand - potionLblGap - interRowGap) * 0.5f;
    }
    rowItemH = std::max(kOfferBelowTotal + 30.f, rowItemH);
    const float iconBoxH0 = rowItemH - kOfferBelowTotal;

    // 遗物/灵液：占位方块竖向 2×（相对压缩前基准 iconBoxH0）；仅在中间栏内分配标题/间距，底边仍为 row2TopY+row2LayoutH
    constexpr float kRelicPotionIconVMul = 2.f;
    constexpr float kRelicPotionShiftDown = 5.f;
    const float rtbMin = 11.f;
    const float ptbMin = 9.f;
    const float rlgMin = 2.f;
    const float plgMin = 2.f;
    const float irgMin = 5.f;
    const float S_floor = rtbMin + rlgMin + ptbMin + plgMin + irgMin;
    const float rowItemHFor2x = iconBoxH0 * kRelicPotionIconVMul + kOfferBelowTotal;
    const float S_for2x = row2LayoutH - 2.f * rowItemHFor2x;

    if (S_for2x >= S_floor) {
        const float extra = S_for2x - S_floor;
        relicTitleBand = rtbMin + extra * 0.33f;
        relicLblGap = rlgMin + extra * 0.12f;
        potionTitleBand = ptbMin + extra * 0.22f;
        potionLblGap = plgMin + extra * 0.12f;
        interRowGap = irgMin + extra * 0.21f;
    } else {
        relicTitleBand = rtbMin;
        relicLblGap = rlgMin;
        potionTitleBand = ptbMin;
        potionLblGap = plgMin;
        interRowGap = irgMin;
    }
    rowItemH = (row2LayoutH - relicTitleBand - relicLblGap - potionTitleBand - potionLblGap - interRowGap) * 0.5f;
    float iconH = std::min(iconBoxH0 * kRelicPotionIconVMul, std::max(0.f, rowItemH - kOfferBelowTotal));

    // 两行方块整体略下移，底部与其它区对齐（总 S 不变）
    relicLblGap += kRelicPotionShiftDown;
    potionLblGap += kRelicPotionShiftDown;
    interRowGap = std::max(6.f, interRowGap - 2.f * kRelicPotionShiftDown);
    rowItemH = (row2LayoutH - relicTitleBand - relicLblGap - potionTitleBand - potionLblGap - interRowGap) * 0.5f;
    iconH = std::min(iconH, std::max(0.f, rowItemH - kOfferBelowTotal));

    const float iconGap = 11.f;
    float iconWRel = (leftBlockW - 2.f * iconGap) / 3.f;
    float midDrawLeft = midLeft;
    if (midWAvailable > leftBlockW + 0.5f)
        midDrawLeft = midLeft + (midWAvailable - leftBlockW) * 0.5f;
    else if (midWAvailable + 0.5f < leftBlockW)
        iconWRel = std::max(22.f, (midWAvailable - 2.f * iconGap) / 3.f);
    const float iconWPot = iconWRel;

    const float relicLabelY = row2TopY;
    const float relicIconsY = row2TopY + relicTitleBand + relicLblGap;
    const float potionLabelY = relicIconsY + rowItemH + interRowGap;
    const float potionIconsY = potionLabelY + potionTitleBand + potionLblGap;
    const float relicLabelX = midDrawLeft + 1.f;

    {
        sf::Text labRel(fontForText(), sf::String(L"遗物"), static_cast<unsigned>(bodySize - 5u));
        labRel.setFillColor(sf::Color(205, 192, 168));
        labRel.setOutlineColor(sf::Color(18, 16, 22));
        labRel.setOutlineThickness(1.5f);
        {
            const sf::FloatRect lb = labRel.getLocalBounds();
            const float ty = relicLabelY + (relicTitleBand - (lb.position.y + lb.size.y)) * 0.5f - lb.position.y;
            labRel.setPosition(sf::Vector2f(relicLabelX, ty));
        }
        window.draw(labRel);
        sf::Text labPot(fontForText(), sf::String(L"灵液"), static_cast<unsigned>(bodySize - 6u));
        labPot.setFillColor(sf::Color(165, 188, 208));
        labPot.setOutlineColor(sf::Color(18, 16, 22));
        labPot.setOutlineThickness(1.2f);
        {
            const sf::FloatRect lb = labPot.getLocalBounds();
            const float ty = potionLabelY + (potionTitleBand - (lb.position.y + lb.size.y)) * 0.5f - lb.position.y;
            labPot.setPosition(sf::Vector2f(relicLabelX, ty));
        }
        window.draw(labPot);
    }

    shopRelicRects_.clear();
    for (size_t i = 0; i < std::min<std::size_t>(3, shopData_.relicsForSale.size()); ++i) {
        const float x = midDrawLeft + static_cast<float>(i) * (iconWRel + iconGap);
        sf::FloatRect r(sf::Vector2f(x, relicIconsY), sf::Vector2f(iconWRel, iconH));
        shopRelicRects_.push_back(r);
        const auto& o = shopData_.relicsForSale[i];
        bool canAfford = shopData_.playerGold >= o.price;
        std::wstring tip;
        bool hover = false;
        draw_icon_offer(window, fontForText(), mousePos_, r, o.name, o.price, true, canAfford, shopOfferNameFont,
            shopPriceFont, tip, hover);
    }

    shopPotionRects_.clear();
    for (size_t i = 0; i < std::min<std::size_t>(3, shopData_.potionsForSale.size()); ++i) {
        const float x = midDrawLeft + static_cast<float>(i) * (iconWPot + iconGap);
        sf::FloatRect r(sf::Vector2f(x, potionIconsY), sf::Vector2f(iconWPot, iconH));
        shopPotionRects_.push_back(r);
        const auto& o = shopData_.potionsForSale[i];
        bool canAfford = shopData_.playerGold >= o.price;
        std::wstring tip;
        bool hover = false;
        draw_icon_offer(window, fontForText(), mousePos_, r, o.name, o.price, false, canAfford, shopOfferNameFont,
            shopPriceFont, tip, hover);
    }

    const size_t nCol = std::min<std::size_t>(2, shopData_.colorlessForSale.size());
    shopColorlessRects_.clear();
    shopColorlessRects_.resize(nCol);
    const float colCardY = row2TopY + std::max(4.f, (row2LayoutH - cardBlockH) * 0.5f);
    for (size_t i = 0; i < nCol; ++i) {
        const float x = row1StartX + static_cast<float>(i) * (cardW + cardGap);
        const sf::FloatRect crect(sf::Vector2f(x, colCardY), sf::Vector2f(cardW, cardH));
        shopColorlessRects_[i] = crect;
        const auto& off = shopData_.colorlessForSale[i];
        bool canAfford = shopData_.playerGold >= off.price;
        std::wstring tip;
        bool hover = false;
        draw_card_tile(window, fontForText(), mousePos_, off.id, off.price, crect, canAfford, shopPriceFont, tip,
            hover);
    }

    // 净简列底与灵液区同底（含 row2ContentBottomPad）；标题在竖条上方不压边框
    const float removeColumnBottom = row2TopY + row2LayoutH;
    const float removeTop = row1Y;
    const float removeColumnH = removeColumnBottom - removeTop;
    const float removePriceBand = 22.f;
    const unsigned svcLine1Size = static_cast<unsigned>(bodySize - 3u);
    const unsigned svcLine2Size = static_cast<unsigned>(bodySize - 4u);
    sf::Text svcLine1(fontForText(), sf::String(L"卡牌移除"), svcLine1Size);
    svcLine1.setFillColor(sf::Color(198, 192, 182));
    const sf::FloatRect lbSvc1 = svcLine1.getLocalBounds();
    svcLine1.setOrigin(sf::Vector2f(lbSvc1.position.x + lbSvc1.size.x * 0.5f, lbSvc1.position.y));
    const float titlePadTop = 1.f;
    const float svcLineGap = 3.f;
    const float ySvc1 = removeTop + titlePadTop;
    svcLine1.setPosition(sf::Vector2f(removeLeft + removeW * 0.5f, ySvc1));
    sf::Text svcLine2(fontForText(), sf::String(L"服务"), svcLine2Size);
    svcLine2.setFillColor(sf::Color(175, 168, 158));
    const sf::FloatRect lbSvc2 = svcLine2.getLocalBounds();
    svcLine2.setOrigin(sf::Vector2f(lbSvc2.position.x + lbSvc2.size.x * 0.5f, lbSvc2.position.y));
    const float ySvc2 = ySvc1 + lbSvc1.size.y + svcLineGap;
    svcLine2.setPosition(sf::Vector2f(removeLeft + removeW * 0.5f, ySvc2));
    const float removeTitlesBottom = ySvc2 + lbSvc2.size.y;
    const float removeSlabTop = removeTitlesBottom + 8.f;
    float removeSlabH = removeColumnBottom - removeSlabTop - removePriceBand;
    removeSlabH = std::max(28.f, removeSlabH);
    shopRemoveServiceRect_ =
        sf::FloatRect(sf::Vector2f(removeLeft, removeTop), sf::Vector2f(removeW, removeColumnH));
    {
        bool sold = shopData_.removeServiceSoldOut;
        bool paid = shopData_.removeServicePaid;
        bool hover = shopRemoveServiceRect_.contains(mousePos_) && !sold;
        const float svcInset = 3.f;

        window.draw(svcLine1);
        window.draw(svcLine2);

        sf::RectangleShape slab(sf::Vector2f(removeW - svcInset * 2.f, removeSlabH - svcInset * 2.f));
        slab.setPosition(sf::Vector2f(removeLeft + svcInset, removeSlabTop + svcInset));
        if (!shopDeleteBgLoaded_) {
            slab.setFillColor(sold ? sf::Color(36, 34, 32) : sf::Color(44, 40, 36));
        } else {
            slab.setFillColor(sf::Color::Transparent);
        }
        slab.setOutlineThickness(2.f);
        slab.setOutlineColor(hover && !sold ? sf::Color(238, 208, 125) : sf::Color(155, 128, 78));
        window.draw(slab);
        if (shopDeleteBgLoaded_) {
            const sf::FloatRect bgDest(sf::Vector2f(removeLeft + svcInset, removeSlabTop + svcInset),
                sf::Vector2f(removeW - svcInset * 2.f, removeSlabH - svcInset * 2.f));
            const sf::Color bgTint = sold ? sf::Color(120, 118, 120, 160) : sf::Color(255, 255, 255, 220);
            draw_texture_fit(window, shopDeleteBgTexture_, bgDest, bgTint);
        }

        const float slabInnerW = removeW - svcInset * 2.f;
        const float slabInnerH = std::max(20.f, removeSlabH - svcInset * 2.f);
        const float iconPad = 4.f;
        const unsigned iconLabelSize = static_cast<unsigned>(std::clamp(static_cast<int>(bodySize) - 2, 14, 20));
        const float iconLabelReserve = static_cast<float>(fontForText().getLineSpacing(iconLabelSize)) + 4.f;
        if (shopDeleteCardLoaded_) {
            const sf::FloatRect iconDest(
                sf::Vector2f(removeLeft + svcInset + iconPad, removeSlabTop + svcInset + iconPad),
                sf::Vector2f(std::max(8.f, slabInnerW - iconPad * 2.f),
                    std::max(8.f, slabInnerH - iconPad * 2.f - iconLabelReserve)));
            const sf::Color iconTint =
                sold ? sf::Color(95, 92, 100) : (hover && !sold ? sf::Color(255, 248, 235) : sf::Color::White);
            draw_texture_contain(window, shopDeleteCardTexture_, iconDest, iconTint);
            sf::Text iconLabel(fontForText(), sf::String(L"净简"), iconLabelSize);
            iconLabel.setFillColor(sold ? sf::Color(130, 125, 120) : sf::Color(232, 222, 198));
            const sf::FloatRect ilb = iconLabel.getLocalBounds();
            iconLabel.setOrigin(sf::Vector2f(ilb.position.x + ilb.size.x * 0.5f, ilb.position.y));
            iconLabel.setPosition(
                sf::Vector2f(removeLeft + removeW * 0.5f, removeSlabTop + svcInset + slabInnerH - iconLabelReserve));
            window.draw(iconLabel);
        } else {
            const float midY = removeSlabTop + svcInset + slabInnerH * 0.46f;
            const unsigned jingSize = static_cast<unsigned>(std::clamp(static_cast<int>(bodySize) - 1, 16, 22));
            sf::Text j1(fontForText(), sf::String(L"净"), jingSize);
            j1.setFillColor(sold ? sf::Color(110, 105, 100) : sf::Color(232, 222, 198));
            j1.setPosition(sf::Vector2f(removeLeft + removeW * 0.5f - j1.getLocalBounds().size.x * 0.5f, midY));
            window.draw(j1);
            sf::Text j2(fontForText(), sf::String(L"简"), jingSize);
            j2.setFillColor(sold ? sf::Color(110, 105, 100) : sf::Color(232, 222, 198));
            j2.setPosition(sf::Vector2f(removeLeft + removeW * 0.5f - j2.getLocalBounds().size.x * 0.5f,
                midY + j1.getLocalBounds().size.y + 3.f));
            window.draw(j2);
        }

        std::wstring priceLabel = sold ? L"—" : (paid ? L"已付" : std::to_wstring(shopData_.removeServicePrice));
        sf::Text pt(fontForText(), sf::String(priceLabel), shopPriceFont);
        pt.setFillColor(sf::Color(255, 200, 90));
        const sf::FloatRect pbb = pt.getLocalBounds();
        pt.setOrigin(sf::Vector2f(pbb.position.x + pbb.size.x * 0.5f, pbb.position.y + pbb.size.y * 0.5f));
        pt.setPosition(sf::Vector2f(removeLeft + removeW * 0.5f, removeColumnBottom - 12.f));
        window.draw(pt);

    }

    shopRemoveRects_.clear();
    shopDeckScrollMax_ = 0.f;
    shopDeckScrollOffset_ = 0.f;
    shopDeckAreaRect_ = sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f));

    // StS：离开贴在面板左缘外侧偏下（侧挂），略压边框形成「咬住」感
    drawLeaveButton();
}

} // namespace tce
