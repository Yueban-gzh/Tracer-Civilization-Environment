#include "GameFlow/CharacterSelectScreen.hpp"

#include <SFML/Graphics.hpp>

namespace tce {

namespace {

sf::Font load_ui_font() {
    sf::Font f;
    if (f.openFromFile("assets/fonts/Sanji.ttf")) return f;
    if (f.openFromFile("data/font.ttf")) return f;
    (void)f.openFromFile("C:/Windows/Fonts/msyh.ttc");
    return f;
}

void draw_centered_text(sf::RenderWindow& window, const sf::Font& font, const std::wstring& text, unsigned size,
                        const sf::Vector2f& center, const sf::Color& color) {
    sf::Text t(font, text, size);
    t.setFillColor(color);
    sf::FloatRect b = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f));
    t.setPosition(center);
    window.draw(t);
}

} // namespace

std::optional<CharacterClass> runCharacterSelectScreen(sf::RenderWindow& window) {
    sf::Font font = load_ui_font();

    std::optional<CharacterClass> selected;
    bool running = true;

    while (window.isOpen() && running) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                return std::nullopt;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    return std::nullopt;
                }
            }
            if (const auto* mouse = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button != sf::Mouse::Button::Left) continue;
                sf::Vector2f mp = window.mapPixelToCoords(mouse->position);

                const sf::Vector2u sz = window.getSize();
                const float W = static_cast<float>(sz.x);
                const float H = static_cast<float>(sz.y);

                const float cardW = 420.f;
                const float cardH = 520.f;
                const float gapX  = 80.f;
                const float cy    = H * 0.5f + 10.f;
                const float leftX = W * 0.5f - (cardW * 1.f + gapX * 0.5f);
                const float rightX = W * 0.5f + gapX * 0.5f;

                sf::FloatRect ironRect(sf::Vector2f(leftX, cy - cardH * 0.5f), sf::Vector2f(cardW, cardH));
                sf::FloatRect silentRect(sf::Vector2f(rightX, cy - cardH * 0.5f), sf::Vector2f(cardW, cardH));

                const float btnW = 240.f;
                const float btnH = 64.f;
                sf::FloatRect confirmRect(sf::Vector2f(W * 0.5f - btnW * 0.5f, H - 120.f), sf::Vector2f(btnW, btnH));

                if (ironRect.contains(mp)) {
                    selected = CharacterClass::Ironclad;
                } else if (silentRect.contains(mp)) {
                    selected = CharacterClass::Silent;
                } else if (confirmRect.contains(mp) && selected.has_value()) {
                    return selected;
                }
            }
        }

        const sf::Vector2u sz = window.getSize();
        const float W = static_cast<float>(sz.x);
        const float H = static_cast<float>(sz.y);

        window.clear(sf::Color(14, 12, 20));

        draw_centered_text(window, font, L"选择职业", 44, sf::Vector2f(W * 0.5f, 90.f), sf::Color(245, 240, 230));
        draw_centered_text(window, font, L"点击角色卡选择，然后点击确认开始游戏", 22, sf::Vector2f(W * 0.5f, 138.f),
                           sf::Color(200, 190, 170));

        const float cardW = 420.f;
        const float cardH = 520.f;
        const float gapX  = 80.f;
        const float cy    = H * 0.5f + 10.f;
        const float leftX = W * 0.5f - (cardW * 1.f + gapX * 0.5f);
        const float rightX = W * 0.5f + gapX * 0.5f;

        auto drawCharCard = [&](CharacterClass cc, const sf::Vector2f& pos, const std::wstring& name,
                                const std::wstring& desc, const sf::Color& accent) {
            const bool isSel = selected.has_value() && *selected == cc;
            sf::RectangleShape bg(sf::Vector2f(cardW, cardH));
            bg.setPosition(pos);
            bg.setFillColor(isSel ? sf::Color(56, 48, 78) : sf::Color(34, 30, 48));
            bg.setOutlineColor(isSel ? accent : sf::Color(120, 110, 140));
            bg.setOutlineThickness(isSel ? 4.f : 2.f);
            window.draw(bg);

            sf::RectangleShape topBar(sf::Vector2f(cardW, 78.f));
            topBar.setPosition(pos + sf::Vector2f(0.f, 0.f));
            topBar.setFillColor(sf::Color(22, 20, 28));
            topBar.setOutlineColor(sf::Color(0, 0, 0, 0));
            window.draw(topBar);

            draw_centered_text(window, font, name, 32, pos + sf::Vector2f(cardW * 0.5f, 40.f),
                               sf::Color(245, 240, 235));

            // 简化“立绘区域”占位
            sf::RectangleShape art(sf::Vector2f(cardW - 42.f, 250.f));
            art.setPosition(pos + sf::Vector2f(21.f, 98.f));
            art.setFillColor(sf::Color(24, 22, 30));
            art.setOutlineColor(isSel ? sf::Color(accent.r, accent.g, accent.b, 220) : sf::Color(170, 160, 190));
            art.setOutlineThickness(2.f);
            window.draw(art);

            draw_centered_text(window, font, desc, 22, pos + sf::Vector2f(cardW * 0.5f, 390.f),
                               sf::Color(218, 212, 204));

            sf::Text hint(font, isSel ? L"已选择" : L"点击选择", 20);
            hint.setFillColor(isSel ? accent : sf::Color(160, 150, 175));
            sf::FloatRect hb = hint.getLocalBounds();
            hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
            hint.setPosition(pos + sf::Vector2f(cardW * 0.5f, cardH - 40.f));
            window.draw(hint);
        };

        drawCharCard(CharacterClass::Ironclad, sf::Vector2f(leftX, cy - cardH * 0.5f),
                     L"铁甲战士", L"以力量与重甲碾碎敌人。", sf::Color(220, 80, 70));
        drawCharCard(CharacterClass::Silent, sf::Vector2f(rightX, cy - cardH * 0.5f),
                     L"静默猎手", L"以技巧与毒刃悄然取胜。", sf::Color(90, 210, 150));

        const float btnW = 240.f;
        const float btnH = 64.f;
        sf::FloatRect confirmRect(sf::Vector2f(W * 0.5f - btnW * 0.5f, H - 120.f), sf::Vector2f(btnW, btnH));
        sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
        btn.setPosition(confirmRect.position);
        const bool enabled = selected.has_value();
        btn.setFillColor(enabled ? sf::Color(80, 70, 110) : sf::Color(45, 42, 58));
        btn.setOutlineColor(sf::Color(200, 190, 150));
        btn.setOutlineThickness(2.f);
        window.draw(btn);
        draw_centered_text(window, font, L"确认", 28, confirmRect.position + sf::Vector2f(btnW * 0.5f, btnH * 0.5f),
                           enabled ? sf::Color(245, 240, 235) : sf::Color(140, 140, 155));

        draw_centered_text(window, font, L"Esc 返回", 18, sf::Vector2f(W - 90.f, H - 30.f), sf::Color(150, 145, 160));

        window.display();
    }

    return std::nullopt;
}

} // namespace tce

