/**
 * 商店界面：布局骨架对齐《杀戮尖塔》商户界面（仅坐标/比例，不换皮）：
 * 上排 5 卡为主视觉；下半左 2 副卡与上排前两列对齐；中右为遗物/灵液两行；最右竖条为净简（仅下半区高度）；
 * 「离开」贴在面板左缘外侧偏下。主商品在净简左侧 mainW 内排布；遗物/灵液三格总宽与左下两卡列等宽；净简与整段商品区（上排至下半底）等高。
 * 内边距对齐事件界面 illustMarginH。无顶栏 HUD、无底部删牌区。
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include "Common/ImagePath.hpp"
#include "DataLayer/DataLayer.h"
#include "UI/CardVisual.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace tce {
using namespace esr_detail;

namespace {

constexpr float BATTLE_CARD_ASPECT_H_OVER_W = 300.f / 206.f;
// 全店统一：价格在商品几何体下方的间距规则（与 StS「价签在物下」一致）
constexpr float kPriceGapBelowCard = 12.f;
constexpr float kPriceReserveBelowCard = 20.f; // 价签字形占用（略加大以配合价格字号）
// 遗物/灵液：框下名称 + 价签预留高度（与 draw_icon_offer 一致）
constexpr float kOfferGapBelowBox = 4.f;
constexpr float kOfferGapNameToPrice = 3.f;

const sf::Texture* resolve_offer_icon_texture(const std::string& id, bool relicStyle) {
    static std::unordered_map<std::string, sf::Texture> relicCache;
    static std::unordered_map<std::string, sf::Texture> potionCache;
    auto& cache = relicStyle ? relicCache : potionCache;
    auto it = cache.find(id);
    if (it != cache.end()) return &it->second;

    sf::Texture tex;
    const std::string baseDir = relicStyle ? "assets/relics/" : "assets/potions/";
    const std::string resolved = resolve_image_path(baseDir + id);
    if (!resolved.empty() && tex.loadFromFile(resolved)) {
        auto [insIt, _] = cache.emplace(id, std::move(tex));
        return &insIt->second;
    }
    return nullptr;
}

void draw_card_tile(sf::RenderWindow& window, const sf::Font& font, const sf::Vector2f& mousePos,
    CardId cid, int price, const sf::FloatRect& rect, bool canAfford, unsigned shopPriceFont,
    std::wstring& outTooltip, bool& outHover) {
    outHover = rect.contains(mousePos);
    outTooltip.clear();
    constexpr sf::Color kOutline(210, 175, 95);
    draw_detailed_card_at(window, font, cid, rect.position.x, rect.position.y, rect.size.x, rect.size.y, kOutline, 8.f);
    if (outHover) {
        sf::RectangleShape hi(sf::Vector2f(rect.size.x + 6.f, rect.size.y + 6.f));
        hi.setPosition(sf::Vector2f(rect.position.x - 3.f, rect.position.y - 3.f));
        hi.setFillColor(sf::Color::Transparent);
        hi.setOutlineColor(sf::Color(255, 230, 160));
        hi.setOutlineThickness(2.5f);
        window.draw(hi);
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

// 与 CardVisual / 战斗手牌同一套完整卡面；用于删牌列表、锻造对比、净简预览等（无价签）。
void draw_battle_card_tile_no_price(sf::RenderWindow& window, const sf::Font& font,
    const sf::Vector2f& mousePos, CardId cid, const std::wstring& fallbackName,
    const sf::FloatRect& rect, bool& outHover, bool forgeOrangeHover = false, bool /*drawDescription*/ = false,
    bool /*upgradedGreenStyle*/ = false, bool forceGoldBorder = false) {
    outHover = rect.contains(mousePos);
    const bool forgeGoldOn = forgeOrangeHover && (outHover || forceGoldBorder);
    if (forgeGoldOn) {
        sf::RectangleShape outer(sf::Vector2f(rect.size.x + 10.f, rect.size.y + 10.f));
        outer.setPosition(sf::Vector2f(rect.position.x - 5.f, rect.position.y - 5.f));
        outer.setFillColor(sf::Color::Transparent);
        outer.setOutlineColor(sf::Color(255, 150, 70, 140));
        outer.setOutlineThickness(4.f);
        window.draw(outer);
    }

    if (cid.empty()) {
        sf::RectangleShape bg(rect.size);
        bg.setPosition(rect.position);
        bg.setFillColor(sf::Color(55, 50, 48));
        bg.setOutlineColor(forgeGoldOn ? sf::Color(255, 200, 110)
                                       : (outHover ? sf::Color(255, 230, 160) : sf::Color(160, 150, 140)));
        bg.setOutlineThickness(forgeGoldOn ? 4.f : (outHover ? 2.5f : 2.f));
        window.draw(bg);
        const sf::String lab = fallbackName.empty() ? sf::String(L"?") : sf::String(fallbackName);
        sf::Text nameText(font, lab, static_cast<unsigned>(std::max(10.f, rect.size.y * 0.055f)));
        nameText.setFillColor(sf::Color::White);
        const sf::FloatRect nb = nameText.getLocalBounds();
        nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
        nameText.setPosition(
            sf::Vector2f(rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f));
        window.draw(nameText);
        return;
    }

    constexpr sf::Color kOutline(210, 175, 95);
    draw_detailed_card_at(window, font, cid, rect.position.x, rect.position.y, rect.size.x, rect.size.y, kOutline, 8.f);
    if (forgeGoldOn) {
        sf::RectangleShape hi(sf::Vector2f(rect.size.x + 6.f, rect.size.y + 6.f));
        hi.setPosition(sf::Vector2f(rect.position.x - 3.f, rect.position.y - 3.f));
        hi.setFillColor(sf::Color::Transparent);
        hi.setOutlineColor(sf::Color(255, 200, 110));
        hi.setOutlineThickness(3.f);
        window.draw(hi);
    } else if (outHover) {
        sf::RectangleShape hi(sf::Vector2f(rect.size.x + 6.f, rect.size.y + 6.f));
        hi.setPosition(sf::Vector2f(rect.position.x - 3.f, rect.position.y - 3.f));
        hi.setFillColor(sf::Color::Transparent);
        hi.setOutlineColor(sf::Color(255, 230, 160));
        hi.setOutlineThickness(2.5f);
        window.draw(hi);
    }
}

void draw_icon_offer(sf::RenderWindow& window, const sf::Font& font, const sf::Vector2f& mousePos,
    const sf::FloatRect& rect, const std::wstring& name, int price, bool relicStyle, bool canAfford,
    unsigned nameFontSize, unsigned priceFontSize, const sf::Texture* iconTexture,
    std::wstring& outTooltip, bool& outHover) {
    outHover = rect.contains(mousePos);
    outTooltip.clear();

    if (iconTexture && iconTexture->getSize().x > 0 && iconTexture->getSize().y > 0) {
        sf::Sprite icon(*iconTexture);
        const sf::Vector2u ts = iconTexture->getSize();
        // 商店遗物/药水改为纯图标展示：去外框，并略微放大图标
        const float innerPad = std::max(1.5f, std::min(rect.size.x, rect.size.y) * 0.04f);
        const float targetW = std::max(6.f, rect.size.x - innerPad * 2.f);
        const float targetH = std::max(6.f, rect.size.y - innerPad * 2.f);
        const float sx = targetW / static_cast<float>(ts.x);
        const float sy = targetH / static_cast<float>(ts.y);
        const float baseS = std::min(sx, sy);
        const float hoverMul = outHover ? 1.08f : 1.0f;
        const float s = baseS * hoverMul;
        icon.setScale(sf::Vector2f(s, s));
        icon.setOrigin(sf::Vector2f(ts.x * 0.5f, ts.y * 0.5f));
        icon.setPosition(sf::Vector2f(rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f));

        if (outHover) {
            const sf::Color glowCol = relicStyle ? sf::Color(255, 220, 140, 120) : sf::Color(150, 220, 255, 110);
            sf::CircleShape glow(std::max(rect.size.x, rect.size.y) * 0.42f);
            glow.setOrigin(sf::Vector2f(glow.getRadius(), glow.getRadius()));
            glow.setPosition(sf::Vector2f(rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f));
            glow.setFillColor(glowCol);
            window.draw(glow);
        }

        icon.setColor(canAfford
            ? (outHover ? sf::Color(255, 255, 255, 255) : sf::Color(255, 255, 255, 245))
            : sf::Color(165, 165, 165, 215));
        window.draw(icon);
    }

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

void EventShopRestUI::drawMasterDeckCardPickGrid(sf::RenderWindow& window,
    const std::vector<MasterDeckCardDisplay>& deck, const sf::String& tipText, bool drawTipLine, float contentLeft,
    float contentW, float regionTop, float regionBottom, float clipTop, float clipBottom, float scale,
    unsigned bodySize, float layoutW, float layoutH, float& scrollOffset, float& scrollMax, float& cardScrollStep,
    std::vector<sf::FloatRect>& outHitRects, sf::FloatRect& outListViewportRect, bool previewUpgrade,
    bool forgeOrangeHover, bool showCardDescription) {
    constexpr float kCardAspect = 300.f / 206.f;
    float listTop = regionTop;
    if (drawTipLine && !tipText.isEmpty()) {
        sf::Text tip(fontForText(), tipText, bodySize);
        tip.setFillColor(sf::Color(236, 227, 206));
        tip.setOutlineColor(sf::Color(20, 18, 24));
        tip.setOutlineThickness(1.f);
        tip.setPosition(sf::Vector2f(contentLeft + 4.f, regionTop));
        window.draw(tip);
        listTop = regionTop + std::max(20.f, static_cast<float>(fontForText().getLineSpacing(bodySize)) + 8.f);
    }
    const float listTopClamped = std::max(listTop, clipTop + 10.f * scale);
    const float listBottom = std::min(regionBottom, clipBottom - 10.f * scale);
    const float listH = std::max(50.f, listBottom - listTopClamped);
    outListViewportRect = sf::FloatRect(sf::Vector2f(contentLeft, listTopClamped), sf::Vector2f(contentW, listH));

    constexpr int kCols = 5;
    const float sidePadX = 18.f * scale;
    const float gapX = 16.f * scale;
    const float gapY = 14.f * scale;
    const float baseTileW = (contentW - sidePadX * 2.f - gapX * (kCols - 1)) / static_cast<float>(kCols);
    const float shrinkMul = showCardDescription ? 0.86f : 0.80f;
    const float tileW = std::max(62.f, baseTileW * shrinkMul);
    const float tileH = tileW * kCardAspect;
    const int n = static_cast<int>(deck.size());
    const int rows = (n + kCols - 1) / kCols;
    const float totalH = rows > 0 ? (rows * tileH + (rows - 1) * gapY) : 0.f;
    scrollMax = std::max(0.f, totalH - listH);
    if (scrollOffset > scrollMax) scrollOffset = scrollMax;
    if (scrollOffset < 0.f) scrollOffset = 0.f;
    cardScrollStep = tileH + gapY;
    outHitRects.assign(deck.size(), sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f)));

    const float lw = std::max(1.f, layoutW);
    const float lh = std::max(1.f, layoutH);
    const float scissorOutY =
        (forgeOrangeHover || showCardDescription) ? 10.f : 0.f;
    const float scTopPx = std::max(0.f, listTopClamped - scissorOutY);
    const float scBotPx = std::min(lh, listTopClamped + listH + scissorOutY);
    const float scHPx = std::max(1.f, scBotPx - scTopPx);

    const sf::View viewBeforeCards = window.getView();
    if (listH > 1.f && contentW > 1.f) {
        sf::View clipView = window.getDefaultView();
        clipView.setScissor(
            sf::FloatRect(sf::Vector2f(contentLeft / lw, scTopPx / lh), sf::Vector2f(contentW / lw, scHPx / lh)));
        window.setView(clipView);
    }

    const float viewportBottom = listTopClamped + listH;
    const float cullOverhang = forgeOrangeHover ? 8.f : 2.f;

    for (int i = 0; i < n; ++i) {
        const int row = i / kCols;
        const int col = i % kCols;
        const float rowDrawW = static_cast<float>(kCols) * tileW + static_cast<float>(kCols - 1) * gapX;
        const float startX = contentLeft + (contentW - rowDrawW) * 0.5f;
        const float x = startX + col * (tileW + gapX);
        const float y = listTopClamped + row * (tileH + gapY) - scrollOffset;
        sf::FloatRect r(sf::Vector2f(x, y), sf::Vector2f(tileW, tileH));
        outHitRects[static_cast<size_t>(i)] = r;
        if (r.position.y + r.size.y <= listTopClamped - cullOverhang || r.position.y >= viewportBottom + cullOverhang)
            continue;

        const auto& dc = deck[static_cast<size_t>(i)];
        CardId displayId = dc.cardId;
        std::wstring fallbackName = dc.cardName;
        if (previewUpgrade && !dc.cardId.empty() && dc.cardId.back() != '+') {
            const CardId upId = dc.cardId + "+";
            if (get_card_by_id(upId)) {
                displayId = upId;
                const CardData* uc = get_card_by_id(upId);
                if (uc) fallbackName = utf8_to_wstring(uc->name);
            }
        }
        bool hover = false;
        draw_battle_card_tile_no_price(window, fontForText(), mousePos_, displayId, fallbackName, r, hover,
            forgeOrangeHover, showCardDescription, false, false);
    }

    window.setView(viewBeforeCards);
}

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
        draw_texture_fit_crop_bottom(window, eventPanelFrameTexture_,
            sf::FloatRect(sf::Vector2f(frameLeft, frameTop), sf::Vector2f(frameWidth, frameHeight)),
            event_panel_frame_bottom_crop_px(eventPanelFrameTexture_));
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
        // 与休息锻造一致：描述 + 金边悬停 + 裁切外扩，避免首尾行与边框被裁断、标题用战斗卡面 UTF-8 路径
        drawMasterDeckCardPickGrid(window, shopData_.deckForRemove, sf::String(L"请选择一张卡牌进行移除"), true,
            contentLeft, contentW, pageTop, pageBottom, clipTop, clipBottom, scale, bodySize, lw, lh,
            shopDeckScrollOffset_, shopDeckScrollMax_, eventCardScrollStep_, shopRemoveRects_, shopDeckAreaRect_,
            false, true, true);

        drawLeaveButton();
        if (shopRemoveConfirmOpen_) drawShopRemoveConfirmOverlay(window);
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
        const sf::Texture* iconTex = resolve_offer_icon_texture(o.id, true);
        draw_icon_offer(window, fontForText(), mousePos_, r, o.name, o.price, true, canAfford, shopOfferNameFont,
            shopPriceFont, iconTex, tip, hover);
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
        const sf::Texture* iconTex = resolve_offer_icon_texture(o.id, false);
        draw_icon_offer(window, fontForText(), mousePos_, r, o.name, o.price, false, canAfford, shopOfferNameFont,
            shopPriceFont, iconTex, tip, hover);
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

namespace {

/** 用折线画勾，避免 U+2713 在中文字体中缺字变「口」 */
void draw_outline_checkmark(sf::RenderWindow& window, sf::Vector2f center, float em, sf::Color stroke) {
    const float k = em * 0.5f;
    sf::VertexArray segs(sf::PrimitiveType::Lines, 4);
    segs[0].position = sf::Vector2f(center.x - k * 0.75f, center.y + k * 0.02f);
    segs[0].color = stroke;
    segs[1].position = sf::Vector2f(center.x - k * 0.08f, center.y + k * 0.65f);
    segs[1].color = stroke;
    segs[2].position = sf::Vector2f(center.x - k * 0.08f, center.y + k * 0.65f);
    segs[2].color = stroke;
    segs[3].position = sf::Vector2f(center.x + k * 0.92f, center.y - k * 0.7f);
    segs[3].color = stroke;
    window.draw(segs);
}

} // namespace

void EventShopRestUI::drawRestForgeUpgradeConfirmOverlay(sf::RenderWindow& window) {
    if (!restForgeUpgradeConfirmOpen_ || restForgeConfirmInstanceId_ == 0) return;

    const MasterDeckCardDisplay* pick = nullptr;
    for (const auto& c : restData_.deckForUpgrade) {
        if (c.instanceId == restForgeConfirmInstanceId_) {
            pick = &c;
            break;
        }
    }
    if (!pick) return;

    const CardId& baseId = pick->cardId;
    if (baseId.empty() || baseId.back() == '+') return;
    const CardId upId = baseId + "+";
    if (!get_card_by_id(upId)) return;

    const sf::Vector2f vsz = window.getView().getSize();
    const float lw = std::max(1.f, vsz.x);
    const float lh = std::max(1.f, vsz.y);

    sf::RectangleShape dim2(sf::Vector2f(lw, lh));
    dim2.setPosition(sf::Vector2f(0.f, 0.f));
    dim2.setFillColor(sf::Color(5, 4, 8, 195));
    window.draw(dim2);

    const float scale = std::clamp(lw / 780.f, 0.78f, 1.4f);
    const unsigned bodySize = static_cast<unsigned>(std::round(BODY_CHAR_SIZE * scale));

    sf::Text title(fontForText(), sf::String(L"升级预览"), static_cast<unsigned>(bodySize + 8u));
    title.setFillColor(sf::Color(255, 230, 180));
    title.setOutlineColor(sf::Color(20, 16, 12));
    title.setOutlineThickness(1.f);
    const sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin(
        sf::Vector2f(titleBounds.position.x + titleBounds.size.x * 0.5f, titleBounds.position.y + titleBounds.size.y * 0.5f));
    title.setPosition(sf::Vector2f(lw * 0.5f, 42.f));
    window.draw(title);

    constexpr float kBattleCardW = 206.f;
    constexpr float kBattleCardH = 300.f;
    const float cardMul = 1.36f;
    const float cardW = kBattleCardW * scale * cardMul;
    const float cardH = kBattleCardH * scale * cardMul;
    // 金边在卡矩形外再扩约 5px，中间须留出「箭头带」避免与卡面重叠
    const float gapMid = 40.f * scale + 96.f;
    const float pairW = cardW * 2.f + gapMid;
    const float centerY = lh * 0.44f;
    const float leftX = lw * 0.5f - pairW * 0.5f;
    const sf::FloatRect leftRect(sf::Vector2f(leftX, centerY - cardH * 0.5f), sf::Vector2f(cardW, cardH));
    const sf::FloatRect rightRect(sf::Vector2f(leftX + cardW + gapMid, centerY - cardH * 0.5f), sf::Vector2f(cardW, cardH));

    bool hoverL = false;
    bool hoverR = false;
    draw_battle_card_tile_no_price(window, fontForText(), mousePos_, baseId, pick->cardName, leftRect, hoverL, true, true,
        false, true);
    draw_battle_card_tile_no_price(window, fontForText(), mousePos_, upId, pick->cardName, rightRect, hoverR, true, true,
        true, true);

    // ASCII「>」各字体必有；U+203A「›」在多数中文字体中无字形会显示为「口」
    sf::Text arrows(fontForText(), sf::String(L"> > >"), static_cast<unsigned>(bodySize + 20u));
    arrows.setFillColor(sf::Color(255, 205, 75));
    arrows.setOutlineColor(sf::Color(55, 35, 8));
    arrows.setOutlineThickness(1.f);
    const sf::FloatRect ab = arrows.getLocalBounds();
    arrows.setOrigin(sf::Vector2f(ab.position.x + ab.size.x * 0.5f, ab.position.y + ab.size.y * 0.5f));
    arrows.setPosition(sf::Vector2f(leftX + cardW + gapMid * 0.5f, centerY));
    window.draw(arrows);

    constexpr float kBottomBand = 96.f;
    constexpr float kSideMargin = 40.f;
    const float backW = 118.f;
    const float backH = 44.f;
    const float backLeft = kSideMargin;
    const float bandY = lh - kBottomBand;
    const float backTop = bandY + (kBottomBand - backH) * 0.5f;
    restBackButton_ = sf::FloatRect(sf::Vector2f(backLeft, backTop), sf::Vector2f(backW, backH));
    const bool backHover = restBackButton_.contains(mousePos_);
    sf::RectangleShape backBtn(restBackButton_.size);
    backBtn.setPosition(restBackButton_.position);
    backBtn.setFillColor(backHover ? sf::Color(155, 42, 38) : sf::Color(128, 36, 32));
    backBtn.setOutlineThickness(2.f);
    backBtn.setOutlineColor(sf::Color(220, 160, 120));
    window.draw(backBtn);
    sf::Text backLab(fontForText(), sf::String(L"← 返回"), static_cast<unsigned>(std::max(14u, bodySize - 1u)));
    backLab.setFillColor(sf::Color(255, 248, 238));
    const sf::FloatRect br = backLab.getLocalBounds();
    backLab.setOrigin(sf::Vector2f(br.position.x + br.size.x * 0.5f, br.position.y + br.size.y * 0.5f));
    backLab.setPosition(sf::Vector2f(backLeft + backW * 0.5f, backTop + backH * 0.5f));
    window.draw(backLab);

    constexpr float kCbSize = 20.f;
    const float cbX = backLeft + backW + 24.f;
    const float cbY = backTop + (backH - kCbSize) * 0.5f;
    constexpr float kLabelGap = 10.f;
    sf::Text vu(fontForText(), sf::String(L"查看升级"), static_cast<unsigned>(std::max(14u, bodySize - 2u)));
    vu.setFillColor(sf::Color(230, 225, 215));
    const sf::FloatRect vb = vu.getLocalBounds();
    const float toggleW = kCbSize + kLabelGap + vb.size.x + 12.f;
    restForgeViewUpgradeToggleRect_ =
        sf::FloatRect(sf::Vector2f(cbX - 4.f, backTop), sf::Vector2f(toggleW, backH));

    sf::RectangleShape cb(sf::Vector2f(kCbSize, kCbSize));
    cb.setPosition(sf::Vector2f(cbX, cbY));
    cb.setFillColor(sf::Color(40, 38, 48));
    cb.setOutlineThickness(2.f);
    cb.setOutlineColor(sf::Color(140, 130, 120));
    window.draw(cb);
    if (restForgeViewUpgrade_) {
        sf::RectangleShape inner(sf::Vector2f(kCbSize - 8.f, kCbSize - 8.f));
        inner.setPosition(sf::Vector2f(cbX + 4.f, cbY + 4.f));
        inner.setFillColor(sf::Color(240, 170, 80));
        window.draw(inner);
    }
    vu.setPosition(sf::Vector2f(cbX + kCbSize + kLabelGap, cbY + (kCbSize - vb.size.y) * 0.5f));
    window.draw(vu);

    sf::Text hint(fontForText(), sf::String(L"确认升级该卡牌？"), static_cast<unsigned>(bodySize + 2u));
    hint.setFillColor(sf::Color(248, 240, 220));
    hint.setOutlineColor(sf::Color(20, 18, 24));
    hint.setOutlineThickness(1.f);
    const sf::FloatRect hb = hint.getLocalBounds();
    hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
    hint.setPosition(sf::Vector2f(lw * 0.5f, bandY + kBottomBand * 0.52f));
    window.draw(hint);

    const float okW = 118.f;
    const float okH = 44.f;
    const float okLeft = lw - kSideMargin - okW;
    restForgeConfirmOkRect_ = sf::FloatRect(sf::Vector2f(okLeft, backTop), sf::Vector2f(okW, okH));
    const bool okHover = restForgeConfirmOkRect_.contains(mousePos_);
    sf::RectangleShape okBtn(restForgeConfirmOkRect_.size);
    okBtn.setPosition(restForgeConfirmOkRect_.position);
    okBtn.setFillColor(okHover ? sf::Color(72, 195, 178) : sf::Color(48, 155, 142));
    okBtn.setOutlineThickness(2.f);
    okBtn.setOutlineColor(sf::Color(180, 255, 240));
    window.draw(okBtn);
    const float okEm = static_cast<float>(std::max(22u, bodySize + 4u));
    const sf::Vector2f okCenter(okLeft + okW * 0.5f, backTop + okH * 0.5f);
    draw_outline_checkmark(window, okCenter, okEm, sf::Color(255, 255, 250));
    draw_outline_checkmark(window, okCenter + sf::Vector2f(1.f, 0.f), okEm, sf::Color(255, 255, 250));
    draw_outline_checkmark(window, okCenter + sf::Vector2f(-1.f, 0.f), okEm, sf::Color(255, 255, 250));
}

void EventShopRestUI::drawShopRemoveConfirmOverlay(sf::RenderWindow& window) {
    if (!shopRemoveConfirmOpen_ || shopRemoveConfirmInstanceId_ == 0) return;

    const MasterDeckCardDisplay* pick = nullptr;
    for (const auto& c : shopData_.deckForRemove) {
        if (c.instanceId == shopRemoveConfirmInstanceId_) {
            pick = &c;
            break;
        }
    }
    if (!pick || pick->cardId.empty()) return;

    const sf::Vector2f vsz = window.getView().getSize();
    const float lw = std::max(1.f, vsz.x);
    const float lh = std::max(1.f, vsz.y);

    sf::RectangleShape dim2(sf::Vector2f(lw, lh));
    dim2.setPosition(sf::Vector2f(0.f, 0.f));
    dim2.setFillColor(sf::Color(5, 4, 8, 195));
    window.draw(dim2);

    const float scale = std::clamp(lw / 780.f, 0.78f, 1.4f);
    const unsigned bodySize = static_cast<unsigned>(std::round(BODY_CHAR_SIZE * scale));

    sf::Text title(fontForText(), sf::String(L"净简预览"), static_cast<unsigned>(bodySize + 8u));
    title.setFillColor(sf::Color(255, 230, 180));
    title.setOutlineColor(sf::Color(20, 16, 12));
    title.setOutlineThickness(1.f);
    const sf::FloatRect titleBounds = title.getLocalBounds();
    title.setOrigin(
        sf::Vector2f(titleBounds.position.x + titleBounds.size.x * 0.5f, titleBounds.position.y + titleBounds.size.y * 0.5f));
    title.setPosition(sf::Vector2f(lw * 0.5f, 42.f));
    window.draw(title);

    constexpr float kBattleCardW = 206.f;
    constexpr float kBattleCardH = 300.f;
    const float cardMul = 1.42f;
    const float cardW = kBattleCardW * scale * cardMul;
    const float cardH = kBattleCardH * scale * cardMul;
    const float centerY = lh * 0.44f;
    const sf::FloatRect cardRect(sf::Vector2f(lw * 0.5f - cardW * 0.5f, centerY - cardH * 0.5f),
        sf::Vector2f(cardW, cardH));
    bool hoverPreview = false;
    draw_battle_card_tile_no_price(window, fontForText(), mousePos_, pick->cardId, pick->cardName, cardRect, hoverPreview,
        true, true, false, true);

    constexpr float kBottomBand = 96.f;
    constexpr float kSideMargin = 40.f;
    const float backW = 118.f;
    const float backH = 44.f;
    const float backLeft = kSideMargin;
    const float bandY = lh - kBottomBand;
    const float backTop = bandY + (kBottomBand - backH) * 0.5f;
    shopRemoveConfirmBackRect_ = sf::FloatRect(sf::Vector2f(backLeft, backTop), sf::Vector2f(backW, backH));
    const bool backHover = shopRemoveConfirmBackRect_.contains(mousePos_);
    sf::RectangleShape backBtn(shopRemoveConfirmBackRect_.size);
    backBtn.setPosition(shopRemoveConfirmBackRect_.position);
    backBtn.setFillColor(backHover ? sf::Color(155, 42, 38) : sf::Color(128, 36, 32));
    backBtn.setOutlineThickness(2.f);
    backBtn.setOutlineColor(sf::Color(220, 160, 120));
    window.draw(backBtn);
    sf::Text backLab(fontForText(), sf::String(L"← 返回"), static_cast<unsigned>(std::max(14u, bodySize - 1u)));
    backLab.setFillColor(sf::Color(255, 248, 238));
    const sf::FloatRect br = backLab.getLocalBounds();
    backLab.setOrigin(sf::Vector2f(br.position.x + br.size.x * 0.5f, br.position.y + br.size.y * 0.5f));
    backLab.setPosition(sf::Vector2f(backLeft + backW * 0.5f, backTop + backH * 0.5f));
    window.draw(backLab);

    sf::Text hint(fontForText(), sf::String(L"确认移除此卡牌？"), static_cast<unsigned>(bodySize + 2u));
    hint.setFillColor(sf::Color(248, 240, 220));
    hint.setOutlineColor(sf::Color(20, 18, 24));
    hint.setOutlineThickness(1.f);
    const sf::FloatRect hb = hint.getLocalBounds();
    hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
    hint.setPosition(sf::Vector2f(lw * 0.5f, bandY + kBottomBand * 0.52f));
    window.draw(hint);

    const float okW = 118.f;
    const float okH = 44.f;
    const float okLeft = lw - kSideMargin - okW;
    shopRemoveConfirmOkRect_ = sf::FloatRect(sf::Vector2f(okLeft, backTop), sf::Vector2f(okW, okH));
    const bool okHover = shopRemoveConfirmOkRect_.contains(mousePos_);
    sf::RectangleShape okBtn(shopRemoveConfirmOkRect_.size);
    okBtn.setPosition(shopRemoveConfirmOkRect_.position);
    okBtn.setFillColor(okHover ? sf::Color(72, 195, 178) : sf::Color(48, 155, 142));
    okBtn.setOutlineThickness(2.f);
    okBtn.setOutlineColor(sf::Color(180, 255, 240));
    window.draw(okBtn);
    const float okEm = static_cast<float>(std::max(22u, bodySize + 4u));
    const sf::Vector2f okCenter(okLeft + okW * 0.5f, backTop + okH * 0.5f);
    draw_outline_checkmark(window, okCenter, okEm, sf::Color(255, 255, 250));
    draw_outline_checkmark(window, okCenter + sf::Vector2f(1.f, 0.f), okEm, sf::Color(255, 255, 250));
    draw_outline_checkmark(window, okCenter + sf::Vector2f(-1.f, 0.f), okEm, sf::Color(255, 255, 250));
}

} // namespace tce
