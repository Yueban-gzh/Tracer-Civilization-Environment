#include "GameFlow/PotionCatalogScreen.hpp"

#include <algorithm>
#include <unordered_map>

#include "BattleEngine/BattleStateSnapshot.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "Common/ImagePath.hpp"

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

sf::Texture* get_potion_icon_texture(std::unordered_map<std::string, sf::Texture>& cache, const std::string& id) {
    auto it = cache.find(id);
    if (it != cache.end()) return &it->second;
    const std::string path = resolve_image_path("assets/potions/" + id);
    if (path.empty()) return nullptr;
    sf::Texture tex;
    if (!tex.loadFromFile(path)) return nullptr;
    tex.setSmooth(true);
    auto inserted = cache.emplace(id, std::move(tex));
    return &inserted.first->second;
}

} // namespace

void runPotionCatalogScreen(sf::RenderWindow& window) {
    const sf::Vector2u sz = window.getSize();
    BattleUI ui(sz.x, sz.y);
    load_battle_ui_fonts_for_catalog(ui);
    ui.set_hide_top_right_map_button(true);

    sf::Font font;
    (void)load_ui_font(font);

    std::vector<std::string> ids = ui_get_all_known_potion_ids();
    std::sort(ids.begin(), ids.end());

    int selected = -1;
    float scrollY = 0.f;
    std::unordered_map<std::string, sf::Texture> iconCache;

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
                    const float top = 102.f;
                    const float rowH = 74.f;
                    const float w = static_cast<float>(window.getSize().x) - left * 2.f;
                    for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
                        const float y = top + i * rowH - scrollY;
                        sf::FloatRect r(sf::Vector2f(left, y), sf::Vector2f(w, rowH - 12.f));
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

        const sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        // 顶部标题（黑金风）
        {
            sf::Text t(font, sf::String(L"灵液总览"), 42);
            t.setLetterSpacing(1.1f);
            t.setFillColor(sf::Color(238, 228, 205));
            t.setOutlineColor(sf::Color(20, 16, 14, 220));
            t.setOutlineThickness(2.f);
            const sf::FloatRect b = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(b.position.x + b.size.x * 0.5f, b.position.y + b.size.y * 0.5f));
            t.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) * 0.5f, 46.f));
            window.draw(t);

            sf::RectangleShape sep(sf::Vector2f(220.f, 1.f));
            sep.setOrigin(sf::Vector2f(110.f, 0.f));
            sep.setPosition(sf::Vector2f(static_cast<float>(window.getSize().x) * 0.5f, 74.f));
            sep.setFillColor(sf::Color(182, 154, 98, 150));
            window.draw(sep);
        }

        const float left = 34.f;
        const float top = 102.f;
        const float rowH = 74.f;
        const float w = static_cast<float>(window.getSize().x) - left * 2.f;
        const float iconBox = 48.f;
        const float textLeft = left + 18.f + iconBox + 14.f;
        const float viewBottom = static_cast<float>(window.getSize().y) - 20.f;

        // 列表
        for (int i = 0; i < static_cast<int>(ids.size()); ++i) {
            const float y = top + i * rowH - scrollY;
            if (y + rowH < top - 40.f || y > viewBottom) continue;

            const bool isSel = (i == selected);
            const sf::FloatRect rowRect(sf::Vector2f(left, y), sf::Vector2f(w, rowH - 12.f));
            const bool hover = rowRect.contains(mouseWorld);

            sf::ConvexShape bg(6);
            const float bevel = 16.f;
            bg.setPoint(0, sf::Vector2f(rowRect.position.x + bevel, rowRect.position.y));
            bg.setPoint(1, sf::Vector2f(rowRect.position.x + rowRect.size.x - bevel, rowRect.position.y));
            bg.setPoint(2, sf::Vector2f(rowRect.position.x + rowRect.size.x, rowRect.position.y + rowRect.size.y * 0.5f));
            bg.setPoint(3, sf::Vector2f(rowRect.position.x + rowRect.size.x - bevel, rowRect.position.y + rowRect.size.y));
            bg.setPoint(4, sf::Vector2f(rowRect.position.x + bevel, rowRect.position.y + rowRect.size.y));
            bg.setPoint(5, sf::Vector2f(rowRect.position.x, rowRect.position.y + rowRect.size.y * 0.5f));
            bg.setFillColor(isSel ? sf::Color(48, 58, 84, 218) : (hover ? sf::Color(42, 50, 74, 206) : sf::Color(32, 38, 56, 192)));
            bg.setOutlineColor(isSel ? sf::Color(220, 186, 118, 236) : (hover ? sf::Color(188, 160, 108, 210) : sf::Color(136, 120, 88, 180)));
            bg.setOutlineThickness(isSel ? 2.3f : 1.6f);
            window.draw(bg);

            sf::RectangleShape iconSlot(sf::Vector2f(iconBox, iconBox));
            iconSlot.setPosition(sf::Vector2f(left + 14.f, y + 7.f));
            iconSlot.setFillColor(sf::Color(20, 20, 30, 228));
            iconSlot.setOutlineColor(isSel ? sf::Color(214, 184, 124) : sf::Color(108, 98, 82));
            iconSlot.setOutlineThickness(1.5f);
            window.draw(iconSlot);
            if (sf::Texture* icon = get_potion_icon_texture(iconCache, ids[static_cast<size_t>(i)])) {
                sf::Sprite sp(*icon);
                const sf::FloatRect tr = sp.getLocalBounds();
                if (tr.size.x > 0.f && tr.size.y > 0.f) {
                    const float target = iconBox - 6.f;
                    const float sc = std::min(target / tr.size.x, target / tr.size.y);
                    sp.setScale(sf::Vector2f(sc, sc));
                    const float sw = tr.size.x * sc;
                    const float sh = tr.size.y * sc;
                    sp.setPosition(sf::Vector2f(iconSlot.getPosition().x + (iconBox - sw) * 0.5f,
                                                iconSlot.getPosition().y + (iconBox - sh) * 0.5f));
                    window.draw(sp);
                }
            }

            auto [name, desc] = ui_get_potion_display_info(ids[static_cast<size_t>(i)]);
            sf::Text nameText(font, sf::String(name), 22);
            nameText.setFillColor(sf::Color(245, 240, 235));
            nameText.setPosition(sf::Vector2f(textLeft, y + 8.f));
            window.draw(nameText);

            sf::Text idText(font, sf::String(ids[static_cast<size_t>(i)]), 16);
            idText.setFillColor(sf::Color(168, 160, 146));
            idText.setPosition(sf::Vector2f(textLeft, y + 36.f));
            window.draw(idText);

            sf::Text descText(font, sf::String(desc), 17);
            descText.setFillColor(sf::Color(214, 208, 194));
            descText.setPosition(sf::Vector2f(textLeft + 300.f, y + 20.f));
            window.draw(descText);
        }

        // 复用 BattleUI：绘制右上角返回按钮/基础交互（无顶栏布局）
        ui.drawDeckViewOnly(window, snap);

        window.display();
    }
}

} // namespace tce

