/**
 * 休息界面绘制（从 EventShopRestUI 拆出）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tce {
using namespace esr_detail;

void EventShopRestUI::drawRestScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;

    if (restShowUpgradeList_) {
        const sf::Vector2f vsz = window.getView().getSize();
        const float lw = std::max(1.f, vsz.x);
        const float lh = std::max(1.f, vsz.y);

        sf::RectangleShape dim(sf::Vector2f(lw, lh));
        dim.setPosition(sf::Vector2f(0.f, 0.f));
        dim.setFillColor(sf::Color(12, 10, 18, 210));
        window.draw(dim);

        // 顶部有全局状态栏与遗物栏，列表区域进一步下移避免与首行卡牌重叠
        constexpr float kTopMargin = 148.f;
        constexpr float kBottomBand = 96.f;
        constexpr float kGridPadTop = 14.f;
        constexpr float kGridPadBottom = 22.f;
        constexpr float kSideMargin = 40.f;
        constexpr float kScrollbarGap = 8.f;
        constexpr float kScrollbarW = 14.f;
        const float gridTop = kTopMargin;
        const float gridBottom = lh - kBottomBand;
        const float gridLeft = kSideMargin;
        const float gridRight = lw - kSideMargin - kScrollbarGap - kScrollbarW;
        const float contentW = std::max(100.f, gridRight - gridLeft);
        const float scale = std::clamp(lw / 780.f, 0.78f, 1.4f);
        const unsigned bodySize = static_cast<unsigned>(std::round(BODY_CHAR_SIZE * scale));

        drawMasterDeckCardPickGrid(window, restData_.deckForUpgrade, sf::String(), false, gridLeft, contentW,
            gridTop + kGridPadTop, gridBottom - kGridPadBottom, gridTop, gridBottom, scale, bodySize, lw, lh,
            restDeckPickScrollOffset_, restDeckPickScrollMax_, eventCardScrollStep_, restUpgradeRects_,
            restForgeListViewportRect_, restForgeViewUpgrade_, true, true, 5);

        const float vTop = restForgeListViewportRect_.position.y;
        const float vH = restForgeListViewportRect_.size.y;
        const float sbX = gridRight + kScrollbarGap;
        restForgeScrollbarTrackRect_ = sf::FloatRect(sf::Vector2f(sbX, vTop), sf::Vector2f(kScrollbarW, vH));

        sf::RectangleShape track(restForgeScrollbarTrackRect_.size);
        track.setPosition(restForgeScrollbarTrackRect_.position);
        track.setFillColor(sf::Color(28, 26, 34, 220));
        track.setOutlineThickness(1.f);
        track.setOutlineColor(sf::Color(60, 55, 70));
        window.draw(track);

        if (restDeckPickScrollMax_ > 0.5f) {
            const float thumbH = std::max(28.f, vH * vH / (vH + restDeckPickScrollMax_));
            restForgeScrollbarThumbH_ = thumbH;
            const float maxTravel = std::max(0.f, vH - thumbH);
            const float thumbTop =
                vTop + (restDeckPickScrollMax_ > 0.f ? (restDeckPickScrollOffset_ / restDeckPickScrollMax_) * maxTravel : 0.f);
            restForgeScrollbarThumbRect_ =
                sf::FloatRect(sf::Vector2f(sbX + 1.f, thumbTop), sf::Vector2f(kScrollbarW - 2.f, thumbH));
        } else {
            restForgeScrollbarThumbH_ = vH;
            restForgeScrollbarThumbRect_ =
                sf::FloatRect(sf::Vector2f(sbX + 1.f, vTop), sf::Vector2f(kScrollbarW - 2.f, vH));
        }
        sf::RectangleShape thumbSh(restForgeScrollbarThumbRect_.size);
        thumbSh.setPosition(restForgeScrollbarThumbRect_.position);
        thumbSh.setFillColor(sf::Color(200, 115, 65, 230));
        thumbSh.setOutlineThickness(1.f);
        thumbSh.setOutlineColor(sf::Color(255, 200, 140));
        window.draw(thumbSh);

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

        sf::Text hint(fontForText(), sf::String(L"选择1张牌来升级"), static_cast<unsigned>(bodySize + 2u));
        hint.setFillColor(sf::Color(248, 240, 220));
        hint.setOutlineColor(sf::Color(20, 18, 24));
        hint.setOutlineThickness(1.f);
        const sf::FloatRect hb = hint.getLocalBounds();
        hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
        hint.setPosition(sf::Vector2f(lw * 0.5f, bandY + kBottomBand * 0.52f));
        window.draw(hint);

        if (restForgeUpgradeConfirmOpen_) drawRestForgeUpgradeConfirmOverlay(window);

        restHealButton_ = sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f));
        restUpgradeChoiceButton_ = sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(0.f, 0.f));
        return;
    }

    // 布局：卷轴在下，问句画在卷轴上（先画卷轴再画问句，问句居中于卷轴）
    const float totalW = REST_CHOICE_BTN_W * 2.f + REST_CHOICE_GAP;
    const float blockCx = cx - REST_BLOCK_OFFSET_LEFT;
    const float blockCy = cy - REST_BLOCK_OFFSET_UP;
    const float scrollH = 88.f;
    const float scrollW = 380.f;
    const float scrollGap = 14.f;
    const float scrollCy = blockCy - 75.f;  // 卷轴中心（整体稍下移、离两按钮更近）
    const float scrollTop = scrollCy - scrollH * 0.5f;
    if (restScrollLoaded_ && restScrollTexture_.getSize().x > 0) {
        constexpr std::uint8_t scrollAlpha = 195;
        constexpr float scrollRotationDeg = 0.f;
        sf::Sprite scrollSpr(restScrollTexture_);
        const sf::Vector2u tsz = restScrollTexture_.getSize();
        scrollSpr.setOrigin(sf::Vector2f(tsz.x * 0.5f, tsz.y * 0.5f));
        scrollSpr.setPosition(sf::Vector2f(blockCx, scrollCy));
        scrollSpr.setScale(sf::Vector2f(scrollW / static_cast<float>(tsz.x), scrollH / static_cast<float>(tsz.y)));
        scrollSpr.setRotation(sf::degrees(scrollRotationDeg));
        scrollSpr.setColor(sf::Color(255, 255, 255, scrollAlpha));
        window.draw(scrollSpr);
    }
    sf::Text prompt(fontForText(), sf::String(L"要做些什么？"), static_cast<unsigned>(TITLE_CHAR_SIZE + 4));
    prompt.setFillColor(sf::Color::Black);
    const sf::FloatRect pr = prompt.getLocalBounds();
    prompt.setOrigin(sf::Vector2f(pr.position.x + pr.size.x * 0.5f, pr.position.y + pr.size.y * 0.5f));
    prompt.setPosition(sf::Vector2f(blockCx, scrollCy));  // 问句在卷轴正中，如同写在卷轴上
    window.draw(prompt);

    const float btnTop = scrollTop + scrollH + scrollGap;
    const float leftBtnX = blockCx - totalW * 0.5f;
    const float rightBtnX = blockCx + REST_CHOICE_GAP * 0.5f;
    restHealButton_ = sf::FloatRect(sf::Vector2f(leftBtnX, btnTop), sf::Vector2f(REST_CHOICE_BTN_W, REST_CHOICE_BTN_H));
    restUpgradeChoiceButton_ = sf::FloatRect(sf::Vector2f(rightBtnX, btnTop), sf::Vector2f(REST_CHOICE_BTN_W, REST_CHOICE_BTN_H));

    // 打坐按钮：图标严格在框内上部，文字「打坐」在框内下部，不重叠
    const float iconSz = REST_CHOICE_ICON_SIZE;
    const float iconTop = btnTop + REST_CHOICE_ICON_TOP;
    const float textCenterY = btnTop + REST_CHOICE_BTN_H - REST_CHOICE_TEXT_BOTTOM;
    bool healHover = restHealButton_.contains(mousePos_);
    sf::RectangleShape healBtn(sf::Vector2f(REST_CHOICE_BTN_W, REST_CHOICE_BTN_H));
    healBtn.setPosition(restHealButton_.position);
    healBtn.setFillColor(healHover ? REST_HEAL_HOVER : REST_HEAL_BG);
    healBtn.setOutlineThickness(2.f);
    healBtn.setOutlineColor(sf::Color(60, 120, 75));
    window.draw(healBtn);
    if (restHealIconLoaded_ && restHealIconTexture_.getSize().x > 0) {
        draw_texture_fit(window, restHealIconTexture_,
            sf::FloatRect(sf::Vector2f(leftBtnX + (REST_CHOICE_BTN_W - iconSz) * 0.5f, iconTop), sf::Vector2f(iconSz, iconSz)));
    }
    sf::Text healLabel(fontForText(), sf::String(L"打坐"), static_cast<unsigned>(BODY_CHAR_SIZE + 2));
    healLabel.setFillColor(sf::Color::White);
    const sf::FloatRect hr = healLabel.getLocalBounds();
    healLabel.setOrigin(sf::Vector2f(hr.position.x + hr.size.x * 0.5f, hr.position.y + hr.size.y * 0.5f));
    healLabel.setPosition(sf::Vector2f(leftBtnX + REST_CHOICE_BTN_W * 0.5f, textCenterY));
    window.draw(healLabel);

    // 锻造按钮：图标在框内上部，文字「锻造」在框内下部
    bool smithHover = restUpgradeChoiceButton_.contains(mousePos_);
    sf::RectangleShape smithBtn(sf::Vector2f(REST_CHOICE_BTN_W, REST_CHOICE_BTN_H));
    smithBtn.setPosition(restUpgradeChoiceButton_.position);
    smithBtn.setFillColor(smithHover ? REST_SMITH_HOVER : REST_SMITH_BG);
    smithBtn.setOutlineThickness(2.f);
    smithBtn.setOutlineColor(sf::Color(120, 65, 55));
    window.draw(smithBtn);
    if (restSmithIconLoaded_ && restSmithIconTexture_.getSize().x > 0) {
        draw_texture_fit(window, restSmithIconTexture_,
            sf::FloatRect(sf::Vector2f(rightBtnX + (REST_CHOICE_BTN_W - iconSz) * 0.5f, iconTop), sf::Vector2f(iconSz, iconSz)));
    }
    sf::Text smithLabel(fontForText(), sf::String(L"锻造"), static_cast<unsigned>(BODY_CHAR_SIZE + 2));
    smithLabel.setFillColor(sf::Color::White);
    const sf::FloatRect sr = smithLabel.getLocalBounds();
    smithLabel.setOrigin(sf::Vector2f(sr.position.x + sr.size.x * 0.5f, sr.position.y + sr.size.y * 0.5f));
    smithLabel.setPosition(sf::Vector2f(rightBtnX + REST_CHOICE_BTN_W * 0.5f, textCenterY));
    window.draw(smithLabel);

    // 悬停时显示效果说明
    const float btnBottom = btnTop + REST_CHOICE_BTN_H;
    const float tipY = btnBottom + 14.f;
    const unsigned tipSize = static_cast<unsigned>(BODY_CHAR_SIZE);
    if (healHover) {
        std::wstring tip = L"恢复 " + std::to_wstring(restData_.healAmount) + L" 点生命值";
        sf::Text tipText(fontForText(), tip, tipSize);
        tipText.setFillColor(sf::Color(220, 255, 220));
        const sf::FloatRect tr = tipText.getLocalBounds();
        float tipX = leftBtnX + REST_CHOICE_BTN_W * 0.5f;
        tipText.setOrigin(sf::Vector2f(tr.position.x + tr.size.x * 0.5f, tr.position.y + tr.size.y * 0.5f));
        tipText.setPosition(sf::Vector2f(tipX, tipY));
        window.draw(tipText);
    }
    if (smithHover) {
        sf::Text tipText(fontForText(), sf::String(L"选择一张牌组中的牌进行升级"), tipSize);
        tipText.setFillColor(sf::Color(255, 220, 200));
        const sf::FloatRect tr = tipText.getLocalBounds();
        float tipX = rightBtnX + REST_CHOICE_BTN_W * 0.5f;
        tipText.setOrigin(sf::Vector2f(tr.position.x + tr.size.x * 0.5f, tr.position.y + tr.size.y * 0.5f));
        tipText.setPosition(sf::Vector2f(tipX, tipY));
        window.draw(tipText);
    }

    restUpgradeRects_.clear();
}

} // namespace tce
