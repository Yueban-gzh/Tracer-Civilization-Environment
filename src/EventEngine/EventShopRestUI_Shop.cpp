/**
 * 商店界面绘制（从 EventShopRestUI 拆出）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include <SFML/Graphics.hpp>
#include <cmath>

namespace tce {
using namespace esr_detail;

void EventShopRestUI::drawShopScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float panelH = 420.f;
    drawPanel(window, cx, cy, PANEL_W, panelH);

    float y = cy - panelH * 0.5f + PAD;
    sf::Text title(fontForText(), sf::String(L"商店"), static_cast<unsigned>(TITLE_CHAR_SIZE));
    title.setFillColor(TEXT_COLOR);
    title.setPosition(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, y));
    window.draw(title);

    sf::Text gold(fontForText(), L"金币: " + std::to_wstring(shopData_.playerGold), static_cast<unsigned>(BODY_CHAR_SIZE));
    gold.setFillColor(sf::Color(255, 220, 100));
    gold.setPosition(sf::Vector2f(cx + PANEL_W * 0.5f - 120.f, y));
    window.draw(gold);
    y += 36.f;

    sf::Text buyLabel(fontForText(), sf::String(L"可购买"), static_cast<unsigned>(BODY_CHAR_SIZE));
    buyLabel.setFillColor(sf::Color(180, 180, 200));
    buyLabel.setPosition(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, y));
    window.draw(buyLabel);
    y += 28.f;

    shopBuyRects_.clear();
    const float btnW = (PANEL_W - PAD * 2.f - 20.f) * 0.5f;
    for (size_t i = 0; i < shopData_.forSale.size(); ++i) {
        const float col = i % 2;
        const float row = std::floor(static_cast<float>(i) * 0.5f);
        const float bx = cx - PANEL_W * 0.5f + PAD + col * (btnW + 20.f);
        const float by = y + row * (BUTTON_H + 8.f);
        sf::FloatRect rect(sf::Vector2f(bx, by), sf::Vector2f(btnW, BUTTON_H));
        shopBuyRects_.push_back(rect);
        bool hover = rect.contains(mousePos_);
        sf::RectangleShape btn(sf::Vector2f(btnW, BUTTON_H));
        btn.setPosition(sf::Vector2f(bx, by));
        btn.setFillColor(hover ? BUTTON_HOVER : BUTTON_BG);
        btn.setOutlineThickness(1.f);
        btn.setOutlineColor(PANEL_OUTLINE);
        window.draw(btn);
        std::wstring label = shopData_.forSale[i].name + L" (" + std::to_wstring(shopData_.forSale[i].price) + L"金)";
        sf::Text t(fontForText(), sf::String(label), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        t.setFillColor(TEXT_COLOR);
        t.setPosition(sf::Vector2f(bx + 8.f, by + (BUTTON_H - (BODY_CHAR_SIZE - 2)) * 0.5f));
        window.draw(t);
    }
    y += (shopData_.forSale.size() + 1) / 2 * (BUTTON_H + 8.f) + 16.f;

    sf::Text removeLabel(fontForText(), sf::String(L"删除一张牌"), static_cast<unsigned>(BODY_CHAR_SIZE));
    removeLabel.setFillColor(sf::Color(180, 180, 200));
    removeLabel.setPosition(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, y));
    window.draw(removeLabel);
    y += 26.f;

    shopRemoveRects_.clear();
    for (size_t i = 0; i < shopData_.deckForRemove.size(); ++i) {
        const float by = y + static_cast<float>(i) * (BUTTON_H * 0.8f + 4.f);
        sf::FloatRect rect(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, by), sf::Vector2f(PANEL_W - PAD * 2.f, BUTTON_H * 0.8f));
        shopRemoveRects_.push_back(rect);
        bool hover = rect.contains(mousePos_);
        sf::RectangleShape btn(rect.size);
        btn.setPosition(rect.position);
        btn.setFillColor(hover ? BUTTON_HOVER : BUTTON_BG);
        btn.setOutlineThickness(1.f);
        btn.setOutlineColor(PANEL_OUTLINE);
        window.draw(btn);
        sf::Text t(fontForText(), sf::String(shopData_.deckForRemove[i].cardName), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        t.setFillColor(TEXT_COLOR);
        t.setPosition(sf::Vector2f(rect.position.x + 8.f, rect.position.y + (rect.size.y - (BODY_CHAR_SIZE - 2)) * 0.5f));
        window.draw(t);
    }
}

} // namespace tce
