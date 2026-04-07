#include <SFML/Graphics.hpp>
#include <filesystem>

#include "GameFlow/CardCatalogScreen.hpp"
#include "GameFlow/PotionCatalogScreen.hpp"
#include "GameFlow/RelicCatalogScreen.hpp"
#include "GameFlow/CharacterSelectScreen.hpp"
#include "GameFlow/GameFlowController.hpp"

namespace tce {

namespace {

bool saveFileExists(const std::string& path) {
    return std::filesystem::exists(std::filesystem::u8path(path));
}

} // namespace

// 简单开始界面：提供“开始新游戏”“继续游戏（若有存档）”两个按钮
// 点击任一按钮后调用对应逻辑并返回，由 main 继续进入 GameFlowController::run()
void runStartScreen(sf::RenderWindow& window, GameFlowController& controller) {
    const std::string savePath = "saves/run_auto_save.json";
    bool hasSave = saveFileExists(savePath);
    auto slot_path = [](int slot) -> std::string {
        return "saves/slot_" + std::to_string(slot) + ".json";
    };

    sf::Font font;
    if (!font.openFromFile("assets/fonts/Sanji.ttf")) {
        (void)font.openFromFile("data/font.ttf");
    }

    bool running = true;
    bool requestedContinue = false;
    std::string requestedLoadPath;

    while (window.isOpen() && running) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                return;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                    return;
                }
            }
            if (const auto* mouse = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mp = window.mapPixelToCoords(mouse->position);

                    const sf::Vector2u sz = window.getSize();
                    const float cx = static_cast<float>(sz.x) * 0.5f;
                    const float cy = static_cast<float>(sz.y) * 0.5f;
                    const float btnW = 320.f;
                    const float btnH = 70.f;
                    const float gapY = 30.f;

                    const float rowGap = btnH + gapY;
                    sf::FloatRect newGameRect(
                        sf::Vector2f(cx - btnW * 0.5f, cy - rowGap),
                        sf::Vector2f(btnW, btnH));
                    sf::FloatRect contRect(
                        sf::Vector2f(cx - btnW * 0.5f, cy),
                        sf::Vector2f(btnW, btnH));
                    sf::FloatRect loadRect(
                        sf::Vector2f(cx - btnW * 0.5f, cy + (btnH + gapY) * 0.5f),
                        sf::Vector2f(btnW, btnH));
                    sf::FloatRect catalogRect(
                        sf::Vector2f(cx - btnW * 0.5f, cy + rowGap),
                        sf::Vector2f(btnW, btnH));
                    sf::FloatRect potionCatalogRect(
                        sf::Vector2f(cx - btnW * 0.5f, cy + rowGap * 2.f),
                        sf::Vector2f(btnW, btnH));
                    sf::FloatRect relicCatalogRect(
                        sf::Vector2f(cx - btnW * 0.5f, cy + rowGap * 3.f),
                        sf::Vector2f(btnW, btnH));

                    if (newGameRect.contains(mp)) {
                        // 新游戏：进入职业选择界面；确认后再初始化 Run
                        if (const auto chosen = runCharacterSelectScreen(window); chosen.has_value()) {
                            (void)controller.initialize(*chosen);
                            running = false;
                            break;
                        }
                        // 取消则回到开始界面
                        break;
                    }
                    if (hasSave && contRect.contains(mp)) {
                        requestedContinue = true;
                        running = false;
                        break;
                    }
                    if (loadRect.contains(mp)) {
                        // 读档：三槽选择
                        bool picking = true;
                        while (window.isOpen() && picking) {
                            while (const std::optional ev2 = window.pollEvent()) {
                                if (ev2->is<sf::Event::Closed>()) { window.close(); return; }
                                if (const auto* k2 = ev2->getIf<sf::Event::KeyPressed>()) {
                                    if (k2->scancode == sf::Keyboard::Scancode::Escape) { picking = false; break; }
                                }
                                if (const auto* m2 = ev2->getIf<sf::Event::MouseButtonPressed>()) {
                                    if (m2->button != sf::Mouse::Button::Left) continue;
                                    const sf::Vector2f p = window.mapPixelToCoords(m2->position);
                                    const float subW = 420.f, subH = 70.f, subGap = 22.f;
                                    const float baseY = cy - (subH + subGap);
                                    for (int s = 1; s <= 3; ++s) {
                                        sf::FloatRect r(sf::Vector2f(cx - subW * 0.5f, baseY + (s - 1) * (subH + subGap)),
                                                       sf::Vector2f(subW, subH));
                                        if (r.contains(p)) {
                                            const std::string pth = slot_path(s);
                                            if (saveFileExists(pth)) {
                                                requestedLoadPath = pth;
                                                running = false;
                                            }
                                            picking = false;
                                            break;
                                        }
                                    }
                                }
                            }
                            if (!picking || !window.isOpen()) break;
                            window.clear(sf::Color(18, 16, 24));
                            sf::Text title2(font, L"选择存档槽", 38);
                            title2.setFillColor(sf::Color(245, 240, 230));
                            sf::FloatRect tb2 = title2.getLocalBounds();
                            title2.setOrigin(sf::Vector2f(tb2.position.x + tb2.size.x * 0.5f, tb2.position.y + tb2.size.y * 0.5f));
                            title2.setPosition(sf::Vector2f(cx, cy - 170.f));
                            window.draw(title2);
                            const float subW = 420.f, subH = 70.f, subGap = 22.f;
                            const float baseY = cy - (subH + subGap);
                            for (int s = 1; s <= 3; ++s) {
                                const bool ok = saveFileExists(slot_path(s));
                                sf::RectangleShape b(sf::Vector2f(subW, subH));
                                b.setPosition(sf::Vector2f(cx - subW * 0.5f, baseY + (s - 1) * (subH + subGap)));
                                b.setFillColor(ok ? sf::Color(80, 70, 110) : sf::Color(50, 50, 70));
                                b.setOutlineColor(sf::Color(200, 190, 150));
                                b.setOutlineThickness(2.f);
                                window.draw(b);
                                std::wstring label = L"槽位 " + std::to_wstring(s) + (ok ? L"（可用）" : L"（空）");
                                sf::Text t(font, label, 26);
                                t.setFillColor(ok ? sf::Color(245, 240, 235) : sf::Color(130, 130, 150));
                                sf::FloatRect rb = t.getLocalBounds();
                                t.setOrigin(sf::Vector2f(rb.position.x + rb.size.x * 0.5f, rb.position.y + rb.size.y * 0.5f));
                                t.setPosition(sf::Vector2f(cx, b.getPosition().y + subH * 0.5f));
                                window.draw(t);
                            }
                            sf::Text hint(font, L"Esc 返回", 20);
                            hint.setFillColor(sf::Color(200, 190, 170));
                            hint.setPosition(sf::Vector2f(24.f, static_cast<float>(sz.y) - 46.f));
                            window.draw(hint);
                            window.display();
                        }
                        break;
                    }
                    if (catalogRect.contains(mp)) {
                        runCardCatalogScreen(window);
                        // 返回后可能已有存档变化，刷新一下
                        hasSave = saveFileExists(savePath);
                        break;
                    }
                    if (potionCatalogRect.contains(mp)) {
                        runPotionCatalogScreen(window);
                        hasSave = saveFileExists(savePath);
                        break;
                    }
                    if (relicCatalogRect.contains(mp)) {
                        runRelicCatalogScreen(window);
                        hasSave = saveFileExists(savePath);
                        break;
                    }
                }
            }
        }

        window.clear(sf::Color(18, 16, 24));

        const sf::Vector2u sz = window.getSize();
        const float cx = static_cast<float>(sz.x) * 0.5f;
        const float cy = static_cast<float>(sz.y) * 0.5f;

        // 标题
        sf::Text title(font, L"溯源者：文明环境", 46);
        title.setFillColor(sf::Color(245, 240, 230));
        sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        title.setPosition(sf::Vector2f(cx, cy - 160.f));
        window.draw(title);

        // 子标题
        sf::Text subtitle(font, L"按下按钮开始旅程", 24);
        subtitle.setFillColor(sf::Color(200, 190, 170));
        sf::FloatRect sb = subtitle.getLocalBounds();
        subtitle.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
        subtitle.setPosition(sf::Vector2f(cx, cy - 110.f));
        window.draw(subtitle);

        const float btnW = 320.f;
        const float btnH = 70.f;
        const float gapY = 30.f;

        auto drawButton = [&](const std::wstring& text, float centerY, bool enabled) {
            sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
            btn.setPosition(sf::Vector2f(cx - btnW * 0.5f, centerY - btnH * 0.5f));
            btn.setFillColor(enabled ? sf::Color(80, 70, 110) : sf::Color(50, 50, 70));
            btn.setOutlineColor(sf::Color(200, 190, 150));
            btn.setOutlineThickness(2.f);
            window.draw(btn);

            sf::Text t(font, text, 28);
            t.setFillColor(enabled ? sf::Color(245, 240, 235) : sf::Color(130, 130, 150));
            sf::FloatRect rb = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(rb.position.x + rb.size.x * 0.5f, rb.position.y + rb.size.y * 0.5f));
            t.setPosition(sf::Vector2f(cx, centerY));
            window.draw(t);
        };

        const float rowGap = btnH + gapY;
        drawButton(L"开始新游戏", cy - rowGap + btnH * 0.5f, true);
        drawButton(L"继续游戏",   cy + btnH * 0.5f, hasSave);
        drawButton(L"读档",       cy + (btnH + gapY) * 0.5f + btnH * 0.5f, true);
        drawButton(L"卡牌总览",   cy + rowGap + btnH * 0.5f, true);
        drawButton(L"药水总览",   cy + rowGap * 2.f + btnH * 0.5f, true);
        drawButton(L"遗物总览",   cy + rowGap * 3.f + btnH * 0.5f, true);

        window.display();
    }

    if (!window.isOpen()) return;

    if (!requestedLoadPath.empty()) {
        if (!controller.loadRun(requestedLoadPath)) {
            if (const auto chosen = runCharacterSelectScreen(window); chosen.has_value()) {
                (void)controller.initialize(*chosen);
            }
        }
        return;
    }

    if (requestedContinue) {
        if (!controller.loadRun(savePath)) {
            // 读档失败则退回到新游戏（带职业选择）
            if (const auto chosen = runCharacterSelectScreen(window); chosen.has_value()) {
                (void)controller.initialize(*chosen);
            }
        }
    }
}

} // namespace tce

