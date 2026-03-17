/**
 * 休息界面绘制（从 EventShopRestUI 拆出）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include <SFML/Graphics.hpp>
#include <cmath>

namespace tce {
using namespace esr_detail;

void EventShopRestUI::drawRestScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;

    if (restShowUpgradeList_) {
        const float panelH = std::max(PANEL_H_MIN, 200.f + static_cast<float>(restData_.deckForUpgrade.size()) * (BUTTON_H * 0.8f + 4.f));
        drawPanel(window, cx, cy, PANEL_W, panelH);
        float y = cy - panelH * 0.5f + PAD;
        sf::Text title(fontForText(), sf::String(L"升级一张牌"), static_cast<unsigned>(TITLE_CHAR_SIZE));
        title.setFillColor(TEXT_COLOR);
        title.setPosition(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, y));
        window.draw(title);
        y += 36.f;
        const float btnW = PANEL_W - PAD * 2.f;
        restBackButton_ = sf::FloatRect(sf::Vector2f(cx - btnW * 0.5f, y), sf::Vector2f(120.f, BUTTON_H * 0.9f));
        bool backHover = restBackButton_.contains(mousePos_);
        sf::RectangleShape backBtn(restBackButton_.size);
        backBtn.setPosition(restBackButton_.position);
        backBtn.setFillColor(backHover ? BUTTON_HOVER : BUTTON_BG);
        backBtn.setOutlineThickness(1.f);
        backBtn.setOutlineColor(PANEL_OUTLINE);
        window.draw(backBtn);
        sf::Text backText(fontForText(), sf::String(L"← 返回"), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        backText.setFillColor(TEXT_COLOR);
        backText.setPosition(sf::Vector2f(restBackButton_.position.x + 10.f, restBackButton_.position.y + (restBackButton_.size.y - (BODY_CHAR_SIZE - 2)) * 0.5f));
        window.draw(backText);
        y += BUTTON_H * 0.9f + 16.f;
        restUpgradeRects_.clear();
        for (size_t i = 0; i < restData_.deckForUpgrade.size(); ++i) {
            const float by = y + static_cast<float>(i) * (BUTTON_H * 0.8f + 4.f);
            sf::FloatRect rect(sf::Vector2f(cx - btnW * 0.5f, by), sf::Vector2f(btnW, BUTTON_H * 0.8f));
            restUpgradeRects_.push_back(rect);
            bool hover = rect.contains(mousePos_);
            sf::RectangleShape btn(rect.size);
            btn.setPosition(rect.position);
            btn.setFillColor(hover ? BUTTON_HOVER : BUTTON_BG);
            btn.setOutlineThickness(1.f);
            btn.setOutlineColor(PANEL_OUTLINE);
            window.draw(btn);
            sf::Text t(fontForText(), sf::String(restData_.deckForUpgrade[i].cardName), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
            t.setFillColor(TEXT_COLOR);
            t.setPosition(sf::Vector2f(rect.position.x + 8.f, rect.position.y + (rect.size.y - (BODY_CHAR_SIZE - 2)) * 0.5f));
            window.draw(t);
        }
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
        constexpr float scrollAlpha = 195u;
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
        sf::Text tipText(fontForText(), sf::String(L"选择一张手牌进行升级"), tipSize);
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
