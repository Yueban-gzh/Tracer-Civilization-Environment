#include "GameFlow/CharacterSelectScreen.hpp"

#include <SFML/Graphics.hpp>
#include <algorithm>

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

bool try_load_bg(sf::Texture& tex) {
    return tex.loadFromFile("assets/backgrounds/main_bg.png") || tex.loadFromFile("./assets/backgrounds/main_bg.png");
}

void draw_texture_cover(sf::RenderWindow& window, const sf::Texture& tex) {
    const sf::Vector2u ws = window.getSize();
    const sf::Vector2u ts = tex.getSize();
    if (ws.x == 0 || ws.y == 0 || ts.x == 0 || ts.y == 0) return;
    sf::Sprite sp(tex);
    const float sx = static_cast<float>(ws.x) / static_cast<float>(ts.x);
    const float sy = static_cast<float>(ws.y) / static_cast<float>(ts.y);
    const float s = std::max(sx, sy);
    sp.setScale(sf::Vector2f(s, s));
    sp.setOrigin(sf::Vector2f(ts.x * 0.5f, ts.y * 0.5f));
    sp.setPosition(sf::Vector2f(ws.x * 0.5f, ws.y * 0.5f));
    window.draw(sp);
}

void draw_ink_backdrop(sf::RenderWindow& window, float W, float H) {
    sf::RectangleShape v(sf::Vector2f(W, H));
    v.setFillColor(sf::Color(8, 10, 16, 88));
    window.draw(v);

    sf::RectangleShape centerBand(sf::Vector2f(W * 0.58f, H * 0.7f));
    centerBand.setOrigin(sf::Vector2f(centerBand.getSize().x * 0.5f, centerBand.getSize().y * 0.5f));
    centerBand.setPosition(sf::Vector2f(W * 0.5f, H * 0.47f));
    centerBand.setFillColor(sf::Color(12, 14, 22, 58));
    window.draw(centerBand);
}

} // namespace

std::optional<CharacterClass> runCharacterSelectScreen(sf::RenderWindow& window) {
    sf::Font font = load_ui_font();
    sf::Texture bgTex;
    const bool bgLoaded = try_load_bg(bgTex);

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
        const sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (bgLoaded) draw_texture_cover(window, bgTex);
        else window.clear(sf::Color(14, 12, 20));
        draw_ink_backdrop(window, W, H);

        {
            sf::Text title(font, L"选择职业", 50);
            title.setLetterSpacing(1.15f);
            title.setFillColor(sf::Color(240, 232, 210));
            title.setOutlineColor(sf::Color(32, 26, 18, 220));
            title.setOutlineThickness(2.f);
            sf::FloatRect tb = title.getLocalBounds();
            title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
            title.setPosition(sf::Vector2f(W * 0.5f, 86.f));
            window.draw(title);

            sf::RectangleShape sep(sf::Vector2f(220.f, 1.f));
            sep.setOrigin(sf::Vector2f(110.f, 0.f));
            sep.setPosition(sf::Vector2f(W * 0.5f, 114.f));
            sep.setFillColor(sf::Color(182, 154, 98, 158));
            window.draw(sep);
        }
        draw_centered_text(window, font, L"点击职业卡选择，然后点击确认开始游戏", 21, sf::Vector2f(W * 0.5f, 143.f),
                           sf::Color(198, 188, 170));

        const float cardW = 408.f;
        const float cardH = 500.f;
        const float gapX  = 76.f;
        const float cy    = H * 0.5f + 6.f;
        const float leftX = W * 0.5f - (cardW * 1.f + gapX * 0.5f);
        const float rightX = W * 0.5f + gapX * 0.5f;

        auto drawCharCard = [&](CharacterClass cc, const sf::Vector2f& pos, const std::wstring& name,
                                const std::wstring& desc, const sf::Color& accent) {
            const bool isSel = selected.has_value() && *selected == cc;
            const sf::FloatRect rect(pos, sf::Vector2f(cardW, cardH));
            const bool hover = rect.contains(mouseWorld);
            const float hoverLift = hover ? 3.f : 0.f;

            sf::RectangleShape bg(sf::Vector2f(cardW, cardH));
            bg.setPosition(pos + sf::Vector2f(0.f, hoverLift));
            bg.setFillColor(isSel ? sf::Color(46, 54, 78, 220) : (hover ? sf::Color(38, 44, 66, 210) : sf::Color(30, 34, 54, 198)));
            bg.setOutlineColor(isSel ? sf::Color(220, 186, 118) : (hover ? sf::Color(186, 160, 108) : sf::Color(136, 120, 90)));
            bg.setOutlineThickness(isSel ? 2.6f : 1.8f);
            window.draw(bg);

            sf::RectangleShape topBar(sf::Vector2f(cardW - 24.f, 72.f));
            topBar.setPosition(pos + sf::Vector2f(12.f, 10.f + hoverLift));
            topBar.setFillColor(sf::Color(18, 20, 30, 210));
            window.draw(topBar);

            draw_centered_text(window, font, name, 36, pos + sf::Vector2f(cardW * 0.5f, 44.f + hoverLift),
                               sf::Color(244, 236, 218));

            sf::RectangleShape art(sf::Vector2f(cardW - 38.f, 246.f));
            art.setPosition(pos + sf::Vector2f(19.f, 98.f + hoverLift));
            art.setFillColor(sf::Color(20, 20, 30, 220));
            art.setOutlineColor(isSel ? sf::Color(214, 184, 124, 230) : sf::Color(150, 138, 112, 188));
            art.setOutlineThickness(1.8f);
            window.draw(art);

            draw_centered_text(window, font, desc, 23, pos + sf::Vector2f(cardW * 0.5f, 389.f + hoverLift),
                               sf::Color(220, 213, 198));

            sf::Text hint(font, isSel ? L"已选择此职业" : L"点击选择", 21);
            hint.setFillColor(isSel ? sf::Color(230, 208, 162) : sf::Color(168, 156, 136));
            sf::FloatRect hb = hint.getLocalBounds();
            hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
            hint.setPosition(pos + sf::Vector2f(cardW * 0.5f, cardH - 44.f + hoverLift));
            window.draw(hint);
        };

        drawCharCard(CharacterClass::Ironclad, sf::Vector2f(leftX, cy - cardH * 0.5f),
                     L"铁甲战士", L"以力量与重甲碾碎敌人。", sf::Color(220, 80, 70));
        drawCharCard(CharacterClass::Silent, sf::Vector2f(rightX, cy - cardH * 0.5f),
                     L"静默猎手", L"以技巧与毒刃悄然取胜。", sf::Color(90, 210, 150));

        const float btnW = 268.f;
        const float btnH = 64.f;
        sf::FloatRect confirmRect(sf::Vector2f(W * 0.5f - btnW * 0.5f, H - 116.f), sf::Vector2f(btnW, btnH));
        const bool enabled = selected.has_value();
        const bool hoverBtn = confirmRect.contains(mouseWorld);
        sf::ConvexShape btn(6);
        const float b = 14.f;
        btn.setPoint(0, sf::Vector2f(confirmRect.position.x + b, confirmRect.position.y));
        btn.setPoint(1, sf::Vector2f(confirmRect.position.x + btnW - b, confirmRect.position.y));
        btn.setPoint(2, sf::Vector2f(confirmRect.position.x + btnW, confirmRect.position.y + btnH * 0.5f));
        btn.setPoint(3, sf::Vector2f(confirmRect.position.x + btnW - b, confirmRect.position.y + btnH));
        btn.setPoint(4, sf::Vector2f(confirmRect.position.x + b, confirmRect.position.y + btnH));
        btn.setPoint(5, sf::Vector2f(confirmRect.position.x, confirmRect.position.y + btnH * 0.5f));
        btn.setFillColor(enabled ? (hoverBtn ? sf::Color(44, 56, 82, 226) : sf::Color(34, 42, 62, 218))
                                 : sf::Color(38, 38, 50, 188));
        btn.setOutlineColor(enabled ? (hoverBtn ? sf::Color(222, 186, 118, 238) : sf::Color(162, 136, 92, 220))
                                    : sf::Color(104, 100, 96, 170));
        btn.setOutlineThickness(2.f);
        window.draw(btn);

        draw_centered_text(window, font, L"确认", 28, confirmRect.position + sf::Vector2f(btnW * 0.5f, btnH * 0.5f),
                           enabled ? sf::Color(242, 234, 216) : sf::Color(142, 142, 152));

        draw_centered_text(window, font, L"Esc 返回", 18, sf::Vector2f(W - 90.f, H - 30.f), sf::Color(170, 162, 146));

        window.display();
    }

    return std::nullopt;
}

} // namespace tce

