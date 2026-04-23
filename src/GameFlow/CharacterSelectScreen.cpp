#include "GameFlow/CharacterSelectScreen.hpp"
#include "Common/ImagePath.hpp"

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace tce {

namespace {

sf::Font load_ui_font() {
    sf::Font f;
    if (f.openFromFile("assets/fonts/Sanji.ttf")) return f;
    if (f.openFromFile("data/font.ttf")) return f;
    if (f.openFromFile("C:/Windows/Fonts/msyh.ttc")) return f;
    if (f.openFromFile("C:/Windows/Fonts/simhei.ttf")) return f;
    if (f.openFromFile("C:/Windows/Fonts/arial.ttf")) return f;
    return f;
}

bool load_tex_utf8(sf::Texture& tex, const std::string& utf8Path) {
    if (utf8Path.empty())
        return false;
    namespace fs = std::filesystem;
    try {
        const fs::path p = fs::u8path(utf8Path);
        if (tex.loadFromFile(p))
            return true;
        if (p.is_absolute())
            return false;
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (ec)
            return false;
        return tex.loadFromFile(cwd / p);
    } catch (...) {
        return false;
    }
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
    if (const std::string p = resolve_image_path("assets/backgrounds/main_bg"); !p.empty()) {
        if (load_tex_utf8(tex, p))
            return tex.getSize().x > 0u && tex.getSize().y > 0u;
    }
    return load_tex_utf8(tex, "assets/backgrounds/main_bg.png") || load_tex_utf8(tex, "./assets/backgrounds/main_bg.png");
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

/** 先 assets/ui/character_select/<id>，再回退 assets/player/<id> */
bool try_load_character_portrait(const std::string& character_id, sf::Texture& out) {
    try {
        std::string p = resolve_image_path("assets/ui/character_select/" + character_id);
        if (p.empty())
            p = resolve_image_path("assets/player/" + character_id);
        if (p.empty())
            return false;
        if (!load_tex_utf8(out, p))
            return false;
        return out.getSize().x > 0 && out.getSize().y > 0;
    } catch (...) {
        return false;
    }
}

void draw_ink_backdrop(sf::RenderWindow& window, float W, float H) {
    sf::RectangleShape v(sf::Vector2f(W, H));
    v.setFillColor(sf::Color(8, 10, 16, 88));
    window.draw(v);

    sf::RectangleShape centerBand(sf::Vector2f(W * 0.62f, H * 0.76f));
    centerBand.setOrigin(sf::Vector2f(centerBand.getSize().x * 0.5f, centerBand.getSize().y * 0.5f));
    centerBand.setPosition(sf::Vector2f(W * 0.5f, H * 0.5f));
    centerBand.setFillColor(sf::Color(12, 14, 22, 58));
    window.draw(centerBand);
}

/** 角色卡布局：中央为正方形 1:1 插图视口（contain），整体加高以突出立绘 */
struct CharSelectCardLayout {
    float cardW{};
    float cardH{};
    float gapX{};
    float pad{};
    float topBarH{};
    float gapUnderTitle{};
    float portraitSide{};
    float gapUnderPortrait{};
};

constexpr CharSelectCardLayout make_card_layout() {
    CharSelectCardLayout L{};
    L.pad              = 14.f;
    L.topBarH          = 52.f;
    L.gapUnderTitle    = 12.f;
    L.portraitSide     = 420.f;  // 1:1 视口边长（较原横向条显著加大）
    L.gapUnderPortrait = 14.f;
    L.gapX             = 48.f;
    L.cardW            = L.pad * 2.f + L.portraitSide;
    // 顶栏 + 方图 + 说明 + 底部提示 + 内边距
    L.cardH            = L.pad + L.topBarH + L.gapUnderTitle + L.portraitSide + L.gapUnderPortrait + 58.f + 40.f + L.pad;
    return L;
}

void layout_select_screen(float W, float H, const CharSelectCardLayout& L, float& outCy, float& outLeftX, float& outRightX) {
    outCy     = H * 0.5f + 10.f;
    outLeftX  = W * 0.5f - (L.cardW + L.gapX * 0.5f);
    outRightX = W * 0.5f + L.gapX * 0.5f;
}

} // namespace

std::optional<CharacterClass> runCharacterSelectScreen(sf::RenderWindow& window) {
    sf::Font font = load_ui_font();

    sf::Texture bgTex;
    sf::Texture portraitIronTex;
    sf::Texture portraitSilentTex;
    bool        bgLoaded           = false;
    bool        portraitIronOk    = false;
    bool        portraitSilentOk  = false;
    bool        characterArtReady = false;

    std::optional<CharacterClass> selected;
    bool running = true;

    const CharSelectCardLayout kLay = make_card_layout();

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
                const sf::Vector2u szEv = window.getSize();
                if (szEv.x < 8u || szEv.y < 8u)
                    continue;
                sf::Vector2f mp = window.mapPixelToCoords(mouse->position);

                const sf::Vector2u sz = window.getSize();
                const float W = static_cast<float>(sz.x);
                const float H = static_cast<float>(sz.y);

                float cy = 0.f;
                float leftX = 0.f;
                float rightX = 0.f;
                layout_select_screen(W, H, kLay, cy, leftX, rightX);

                sf::FloatRect ironRect(sf::Vector2f(leftX, cy - kLay.cardH * 0.5f),
                                       sf::Vector2f(kLay.cardW, kLay.cardH));
                sf::FloatRect silentRect(sf::Vector2f(rightX, cy - kLay.cardH * 0.5f),
                                         sf::Vector2f(kLay.cardW, kLay.cardH));

                const float btnW = 288.f;
                const float btnH = 58.f;
                // 确认键紧贴角色卡下方，避免像贴在屏幕底边；极矮窗口时再压到距底边留边
                const float confirmY =
                    std::min(cy + kLay.cardH * 0.5f + 28.f, H - btnH - 36.f);
                sf::FloatRect confirmRect(sf::Vector2f(W * 0.5f - btnW * 0.5f, confirmY), sf::Vector2f(btnW, btnH));

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
        if (sz.x < 8u || sz.y < 8u) {
            window.display();
            std::this_thread::sleep_for(std::chrono::milliseconds(32));
            continue;
        }

        // 在主循环内、首帧绘制之前加载贴图：此时已与 StartScreen 同属一条显示链路，避免入口 setActive/冷上下文上传 GPU 异常
        if (!characterArtReady) {
            try {
                bgLoaded          = try_load_bg(bgTex);
                portraitIronOk    = try_load_character_portrait("Ironclad", portraitIronTex);
                portraitSilentOk = try_load_character_portrait("Silent", portraitSilentTex);
            } catch (const std::exception& e) {
                std::cerr << "[CharacterSelect] asset load: " << e.what() << "\n";
            } catch (...) {
                std::cerr << "[CharacterSelect] asset load: unknown exception\n";
            }
            characterArtReady = true;
        }

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
        draw_centered_text(window, font, L"点击职业卡选择，然后点击确认开始游戏", 20, sf::Vector2f(W * 0.5f, 136.f),
                           sf::Color(198, 188, 170));

        float cy = 0.f;
        float leftX = 0.f;
        float rightX = 0.f;
        layout_select_screen(W, H, kLay, cy, leftX, rightX);

        const float cardW = kLay.cardW;
        const float cardH = kLay.cardH;

        auto drawCharCard =
            [&](CharacterClass cc, const sf::Vector2f& pos, const std::wstring& name, const std::wstring& desc,
                const sf::Color& accent, const sf::Texture& portraitTex, bool portraitOk) {
                (void)accent;
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

                sf::RectangleShape topBar(sf::Vector2f(cardW - kLay.pad * 2.f, kLay.topBarH));
                topBar.setPosition(pos + sf::Vector2f(kLay.pad, kLay.pad + hoverLift));
                topBar.setFillColor(sf::Color(18, 20, 30, 210));
                window.draw(topBar);

                draw_centered_text(window, font, name, 34,
                                   sf::Vector2f(pos.x + cardW * 0.5f,
                                                pos.y + kLay.pad + kLay.topBarH * 0.5f + hoverLift),
                                   sf::Color(244, 236, 218));

                const float artX = pos.x + kLay.pad;
                const float artY = pos.y + kLay.pad + kLay.topBarH + kLay.gapUnderTitle + hoverLift;
                const float side = kLay.portraitSide;
                sf::RectangleShape artFrame(sf::Vector2f(side, side));
                artFrame.setPosition(sf::Vector2f(artX, artY));
                artFrame.setFillColor(sf::Color(10, 10, 16, 240));
                artFrame.setOutlineColor(isSel ? sf::Color(214, 184, 124, 230) : sf::Color(150, 138, 112, 188));
                artFrame.setOutlineThickness(2.f);
                window.draw(artFrame);

                if (portraitOk) {
                    const auto tsz = portraitTex.getSize();
                    if (tsz.x > 0u && tsz.y > 0u) {
                        sf::Sprite sp(portraitTex);
                        // 正方形视口内等比「包含」，不裁切原图
                        const float sc = std::min(side / static_cast<float>(tsz.x),
                                                  side / static_cast<float>(tsz.y));
                        sp.setOrigin(sf::Vector2f(static_cast<float>(tsz.x) * 0.5f,
                                                    static_cast<float>(tsz.y) * 0.5f));
                        sp.setScale(sf::Vector2f(sc, sc));
                        sp.setPosition(sf::Vector2f(artX + side * 0.5f, artY + side * 0.5f));
                        window.draw(sp);
                    } else {
                        sf::RectangleShape ph(sf::Vector2f(side - 6.f, side - 6.f));
                        ph.setPosition(sf::Vector2f(artX + 3.f, artY + 3.f));
                        ph.setFillColor(sf::Color(22, 22, 32, 230));
                        window.draw(ph);
                    }
                } else {
                    sf::RectangleShape ph(sf::Vector2f(side - 6.f, side - 6.f));
                    ph.setPosition(sf::Vector2f(artX + 3.f, artY + 3.f));
                    ph.setFillColor(sf::Color(22, 22, 32, 230));
                    window.draw(ph);
                }

                const float descCy = artY + side + kLay.gapUnderPortrait + 26.f;
                draw_centered_text(window, font, desc, 22,
                                   sf::Vector2f(pos.x + cardW * 0.5f, descCy + hoverLift),
                                   sf::Color(220, 213, 198));

                sf::Text hint(font, isSel ? L"已选择此职业" : L"点击选择", 20);
                hint.setFillColor(isSel ? sf::Color(230, 208, 162) : sf::Color(168, 156, 136));
                sf::FloatRect hb = hint.getLocalBounds();
                hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
                hint.setPosition(
                    sf::Vector2f(pos.x + cardW * 0.5f, pos.y + cardH - kLay.pad - 20.f + hoverLift));
                window.draw(hint);
            };

        drawCharCard(CharacterClass::Ironclad, sf::Vector2f(leftX, cy - cardH * 0.5f),
                     L"战士", L"以力量与重甲碾碎敌人，善于抓住敌人破绽制敌死亡。", sf::Color(220, 80, 70), portraitIronTex, portraitIronOk);
        drawCharCard(CharacterClass::Silent, sf::Vector2f(rightX, cy - cardH * 0.5f),
                     L"镖客", L"以技巧与毒刃悄然取胜，善于涣散敌人的意志制敌死亡", sf::Color(90, 210, 150), portraitSilentTex, portraitSilentOk);

        const float btnW = 288.f;
        const float btnH = 58.f;
        const float confirmY = std::min(cy + cardH * 0.5f + 28.f, H - btnH - 36.f);
        sf::FloatRect confirmRect(sf::Vector2f(W * 0.5f - btnW * 0.5f, confirmY), sf::Vector2f(btnW, btnH));
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

