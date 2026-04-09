#include "GameFlow/RelicCatalogScreen.hpp"

#include <algorithm>

#include "BattleEngine/BattleStateSnapshot.hpp"
#include "BattleEngine/BattleUI.hpp"

namespace tce {

namespace {

bool load_battle_ui_fonts_for_catalog(BattleUI& ui) {
    bool ok = false;
    ok = ui.loadFont("assets/fonts/Sanji.ttf") || ok;
    ok = ui.loadFont("assets/fonts/default.ttf") || ok;
    ok = ui.loadFont("data/font.ttf") || ok;
    ok = ui.loadFont("C:/Windows/Fonts/arial.ttf") || ok;
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
        (void)ui.loadChineseFont("assets/fonts/simkai.ttf");
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                (void)ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
            }
        }
    }
    return ok;
}

bool load_ui_font(sf::Font& font) {
    if (font.openFromFile("assets/fonts/Sanji.ttf")) return true;
    if (font.openFromFile("data/font.ttf")) return true;
    if (font.openFromFile("C:/Windows/Fonts/msyh.ttc")) return true;
    if (font.openFromFile("C:/Windows/Fonts/simhei.ttf")) return true;
    if (font.openFromFile("C:/Windows/Fonts/simsun.ttc")) return true;
    return false;
}

} // namespace

void runRelicCatalogScreen(sf::RenderWindow& window) {
    const sf::Vector2u sz = window.getSize();
    BattleUI ui(sz.x, sz.y);
    load_battle_ui_fonts_for_catalog(ui);
    ui.set_hide_top_right_map_button(true);

    sf::Font font;
    (void)load_ui_font(font);

    std::vector<std::string> ids = ui_get_all_known_relic_ids();
    std::sort(ids.begin(), ids.end());

    int selected = -1;
    float scrollY = 0.f;

    BattleStateSnapshot snap{};
    snap.currentHp = 1;
    snap.maxHp = 1;
    snap.potionSlotCount = 0;
    snap.turnNumber = 0;

    while (window.isOpen()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                return;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) return;
            }

            sf::Vector2f mp;
            if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mp = window.mapPixelToCoords(m2->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                mp = window.mapPixelToCoords(mr->position);
            } else {
                mp = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            }
            ui.setMousePosition(mp);

            if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheel->wheel == sf::Mouse::Wheel::Vertical) {
                    scrollY -= wheel->delta * 70.f;
                    if (scrollY < 0.f) scrollY = 0.f;
                }
            }

            if (const auto* mpress = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mpress->button == sf::Mouse::Button::Left) {
                    const float left = 34.f;
                    const float top = 86.f;
                    const float rowH = 64.f;
                    const float w = static_cast<float>(window.getSize().x) - left * 2.f;
                    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
                        const float y = top + i * rowH - scrollY;
                        sf::FloatRect r(sf::Vector2f(left, y), sf::Vector2f(w, rowH - 10.f));
                        if (r.contains(mp)) {
                            selected = i;
                            break;
                        }
                    }
                }
            }

            (void)ui.handleEvent(*ev, mp);
        }

        window.clear(sf::Color(18, 16, 24));

        // 顶部标题
        {
            sf::Text t(font, sf::String(L"遗物总览"), 34);
            t.setFillColor(sf::Color(245, 240, 230));
            const sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f));
            t.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) * 0.5f, 42.f));
            window.draw(t);
        }

        const float left = 34.f;
        const float top = 86.f;
        const float rowH = 64.f;
        const float w = static_cast<float>(window.getSize().x) - left * 2.f;
        const float viewBottom = static_cast<float>(window.getSize().y) - 20.f;

        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
            const float y = top + i * rowH - scrollY;
            if (y + rowH < top - 40.f || y > viewBottom) continue;

            const bool isSel = (i == selected);
            sf::RectangleShape bg(sf::Vector2f(w, rowH - 10.f));
            bg.setPosition(sf::Vector2f(left, y));
            bg.setFillColor(isSel ? sf::Color(70, 64, 92) : sf::Color(38, 34, 52));
            bg.setOutlineColor(isSel ? sf::Color(210, 195, 150) : sf::Color(80, 78, 92));
            bg.setOutlineThickness(isSel ? 2.5f : 2.f);
            window.draw(bg);

            auto [name, desc] = ui_get_relic_display_info(ids[static_cast<size_t>(i)]);
            sf::Text nameText(font, sf::String(name), 22);
            nameText.setFillColor(sf::Color(245, 240, 235));
            nameText.setPosition(sf::Vector2f(left + 18.f, y + 9.f));
            window.draw(nameText);

            sf::Text idText(font, sf::String(ids[static_cast<size_t>(i)]), 16);
            idText.setFillColor(sf::Color(160, 155, 175));
            idText.setPosition(sf::Vector2f(left + 18.f, y + 34.f));
            window.draw(idText);

            sf::Text descText(font, sf::String(desc), 18);
            descText.setFillColor(sf::Color(210, 205, 195));
            descText.setPosition(sf::Vector2f(left + 260.f, y + 18.f));
            window.draw(descText);
        }

        ui.drawDeckViewOnly(window, snap);
        window.display();
    }
}

} // namespace tce

