#include <SFML/Graphics.hpp>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "GameFlow/CardCatalogScreen.hpp"
#include "GameFlow/PotionCatalogScreen.hpp"
#include "GameFlow/RelicCatalogScreen.hpp"
#include "GameFlow/CharacterSelectScreen.hpp"
#include "GameFlow/GameFlowController.hpp"
#include "Common/UserSettings.hpp"

namespace tce {

namespace {

std::filesystem::path get_executable_directory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::filesystem::path p(buf);
    return p.parent_path();
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path guess_project_root_from_exe() {
    namespace fs = std::filesystem;
    fs::path d = get_executable_directory();
    for (int i = 0; i < 6 && !d.empty(); ++i) {
        if (fs::exists(d / "src") && fs::exists(d / "include")) return d;
        d = d.parent_path();
    }
    return {};
}

std::filesystem::path resolve_save_path(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path p = fs::u8path(path);
    if (p.is_absolute()) return p;
    if (fs::path root = guess_project_root_from_exe(); !root.empty()) return root / p;
    return p;
}

bool saveFileExists(const std::string& path) {
    return std::filesystem::exists(resolve_save_path(path));
}

struct StartSettingsLayout {
    sf::FloatRect volDown{};
    sf::FloatRect volUp{};
    sf::FloatRect resPrev{};
    sf::FloatRect resNext{};
    sf::FloatRect applyVideo{};
    sf::FloatRect back{};

    void build(float ww, float hh) {
        const float panelW = 720.f;
        const float panelH = 508.f;
        const float px     = (ww - panelW) * 0.5f;
        const float py     = (hh - panelH) * 0.5f;
        const float cx     = px + panelW * 0.5f;
        float       y      = py + 88.f;
        const float rowH   = 44.f;
        volDown = sf::FloatRect(sf::Vector2f(cx - 210.f, y), sf::Vector2f(52.f, rowH));
        volUp   = sf::FloatRect(sf::Vector2f(cx + 158.f, y), sf::Vector2f(52.f, rowH));
        y += 72.f;
        // 与音量按钮对齐：同列、同宽，避免视觉跳动
        resPrev = sf::FloatRect(sf::Vector2f(cx - 210.f, y), sf::Vector2f(52.f, rowH));
        resNext = sf::FloatRect(sf::Vector2f(cx + 158.f, y), sf::Vector2f(52.f, rowH));
        y += 76.f;
        const float appW = 300.f;
        const float appH = 48.f;
        applyVideo = sf::FloatRect(sf::Vector2f(cx - appW * 0.5f, y), sf::Vector2f(appW, appH));
        const float bw = 240.f;
        const float bh = 56.f;
        back = sf::FloatRect(sf::Vector2f(px + (panelW - bw) * 0.5f, py + panelH - bh - 36.f), sf::Vector2f(bw, bh));
    }
};

static void drawSmallBtn(sf::RenderWindow& window, const sf::Font& font, const sf::FloatRect& r, const std::wstring& label,
                         bool hover) {
    sf::RectangleShape b(r.size);
    b.setPosition(r.position);
    b.setFillColor(hover ? sf::Color(86, 80, 104) : sf::Color(62, 58, 78));
    b.setOutlineColor(hover ? sf::Color(220, 205, 155) : sf::Color(170, 160, 130));
    b.setOutlineThickness(hover ? 2.5f : 2.f);
    window.draw(b);
    sf::Text t(font, sf::String(label), 22);
    t.setFillColor(sf::Color(235, 230, 220));
    const sf::FloatRect lb = t.getLocalBounds();
    t.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
    t.setPosition(sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.5f));
    window.draw(t);
}

static void drawStartSettingsOverlay(sf::RenderWindow& window, const sf::Font& font, const sf::Vector2f& mouse,
                                     const StartSettingsLayout& L, int previewPreset) {
    const sf::Vector2u wsz = window.getSize();
    sf::RectangleShape dim(sf::Vector2f(static_cast<float>(wsz.x), static_cast<float>(wsz.y)));
    dim.setFillColor(sf::Color(10, 10, 16, 230));
    window.draw(dim);

    const float panelW = 720.f;
    const float panelH = 508.f;
    const float px     = (static_cast<float>(wsz.x) - panelW) * 0.5f;
    const float py     = (static_cast<float>(wsz.y) - panelH) * 0.5f;
    sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
    panel.setPosition(sf::Vector2f(px, py));
    panel.setFillColor(sf::Color(28, 26, 36, 248));
    panel.setOutlineColor(sf::Color(205, 192, 150));
    panel.setOutlineThickness(2.25f);
    window.draw(panel);

    sf::Text title(font, L"设置", 40);
    title.setFillColor(sf::Color(245, 240, 225));
    const sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    title.setPosition(sf::Vector2f(px + panelW * 0.5f, py + 48.f));
    window.draw(title);

    sf::Text labVol(font, L"主音量", 24);
    labVol.setFillColor(sf::Color(210, 205, 195));
    labVol.setPosition(sf::Vector2f(px + 40.f, L.volDown.position.y + 2.f));
    window.draw(labVol);

    drawSmallBtn(window, font, L.volDown, L"－", L.volDown.contains(mouse));
    drawSmallBtn(window, font, L.volUp, L"＋", L.volUp.contains(mouse));
    {
        const int v = static_cast<int>(UserSettings::instance().masterVolume() + 0.5f);
        sf::Text tv(font, sf::String(std::to_wstring(v) + L" %"), 24);
        tv.setFillColor(sf::Color(245, 235, 220));
        const sf::FloatRect lb = tv.getLocalBounds();
        tv.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
        const float cx = px + panelW * 0.5f;
        const float cy = L.volDown.position.y + L.volDown.size.y * 0.5f;
        tv.setPosition(sf::Vector2f(cx, cy));
        window.draw(tv);
    }

    sf::Text labRes(font, L"分辨率", 24);
    labRes.setFillColor(sf::Color(210, 205, 195));
    labRes.setPosition(sf::Vector2f(px + 40.f, L.resPrev.position.y + 2.f));
    window.draw(labRes);

    // 用 ASCII 箭头，避免字体缺字导致的方块/乱码
    drawSmallBtn(window, font, L.resPrev, L"<", L.resPrev.contains(mouse));
    drawSmallBtn(window, font, L.resNext, L">", L.resNext.contains(mouse));
    {
        const std::wstring label = UserSettings::instance().videoPresetLabelForIndex(previewPreset);
        sf::Text tr(font, sf::String(label), 22);
        tr.setFillColor(sf::Color(245, 235, 220));
        const sf::FloatRect lb = tr.getLocalBounds();
        tr.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
        tr.setPosition(sf::Vector2f(px + panelW * 0.5f, L.resPrev.position.y + L.resPrev.size.y * 0.5f));
        window.draw(tr);
    }

    {
        const bool hv = L.applyVideo.contains(mouse);
        sf::RectangleShape ab(L.applyVideo.size);
        ab.setPosition(L.applyVideo.position);
        ab.setFillColor(hv ? sf::Color(72, 110, 86) : sf::Color(52, 82, 64));
        ab.setOutlineColor(hv ? sf::Color(190, 230, 170) : sf::Color(140, 180, 130));
        ab.setOutlineThickness(hv ? 2.5f : 2.f);
        window.draw(ab);
        sf::Text ta(font, L"应用分辨率", 24);
        ta.setFillColor(sf::Color(235, 245, 230));
        const sf::FloatRect lb = ta.getLocalBounds();
        ta.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
        ta.setPosition(sf::Vector2f(L.applyVideo.position.x + L.applyVideo.size.x * 0.5f,
                                    L.applyVideo.position.y + L.applyVideo.size.y * 0.5f));
        window.draw(ta);
    }

    drawSmallBtn(window, font, L.back, L"返回", L.back.contains(mouse));
}

} // namespace

void runStartScreen(sf::RenderWindow& window, GameFlowController& controller) {
    const std::string savePath = "saves/run_auto_save.json";
    bool              hasSave  = saveFileExists(savePath);
    auto              slot_path = [](int slot) -> std::string {
        return "saves/slot_" + std::to_string(slot) + ".json";
    };

    sf::Font font;
    if (!font.openFromFile("assets/fonts/Sanji.ttf")) {
        (void)font.openFromFile("data/font.ttf");
    }

    sf::ContextSettings ctx;
    ctx.antiAliasingLevel = 2u;

    bool        running              = true;
    bool        requestedContinue    = false;
    std::string requestedLoadPath;
    bool        inStartSettings      = false;
    int         startSettingsPreview = 0;

    while (window.isOpen() && running) {
        controller.applyPendingVideoAndHudResize(ctx);

        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                return;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    if (inStartSettings) {
                        inStartSettings = false;
                        continue;
                    }
                    window.close();
                    return;
                }
            }
            if (const auto* mouse = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button != sf::Mouse::Button::Left) continue;
                sf::Vector2f mp = window.mapPixelToCoords(mouse->position);

                const sf::Vector2u sz = window.getSize();
                const float        cx = static_cast<float>(sz.x) * 0.5f;
                const float        cy = static_cast<float>(sz.y) * 0.5f;

                if (inStartSettings) {
                    StartSettingsLayout L;
                    L.build(static_cast<float>(sz.x), static_cast<float>(sz.y));
                    if (L.volDown.contains(mp))
                        UserSettings::instance().setMasterVolume(UserSettings::instance().masterVolume() - 5.f);
                    else if (L.volUp.contains(mp))
                        UserSettings::instance().setMasterVolume(UserSettings::instance().masterVolume() + 5.f);
                    else if (L.resPrev.contains(mp)) {
                        startSettingsPreview =
                            (startSettingsPreview - 1 + UserSettings::kVideoModeCount) % UserSettings::kVideoModeCount;
                    } else if (L.resNext.contains(mp)) {
                        startSettingsPreview = (startSettingsPreview + 1) % UserSettings::kVideoModeCount;
                    } else if (L.applyVideo.contains(mp)) {
                        UserSettings::instance().setVideoPreset(startSettingsPreview);
                        UserSettings::instance().markVideoApplyPending();
                        UserSettings::instance().save();
                    } else if (L.back.contains(mp))
                        inStartSettings = false;
                    continue;
                }

                const float btnW  = 360.f;
                const float btnH  = 68.f;
                const float gapY  = 16.f;
                const float startY = cy - 105.f;
                auto        rectAt = [&](int idx) -> sf::FloatRect {
                    const float centerY = startY + idx * (btnH + gapY);
                    return sf::FloatRect(sf::Vector2f(cx - btnW * 0.5f, centerY - btnH * 0.5f), sf::Vector2f(btnW, btnH));
                };

                const sf::FloatRect newGameRect       = rectAt(0);
                const sf::FloatRect contRect          = rectAt(1);
                const sf::FloatRect loadRect          = rectAt(2);
                const sf::FloatRect catalogRect       = rectAt(3);
                const sf::FloatRect potionCatalogRect = rectAt(4);
                const sf::FloatRect relicCatalogRect  = rectAt(5);
                const sf::FloatRect settingsRect      = rectAt(6);

                if (newGameRect.contains(mp)) {
                    if (const auto chosen = runCharacterSelectScreen(window); chosen.has_value()) {
                        (void)controller.initialize(*chosen);
                        running = false;
                        break;
                    }
                    break;
                }
                if (hasSave && contRect.contains(mp)) {
                    requestedContinue = true;
                    running           = false;
                    break;
                }
                if (loadRect.contains(mp)) {
                    bool picking = true;
                    while (window.isOpen() && picking) {
                        controller.applyPendingVideoAndHudResize(ctx);
                        while (const std::optional ev2 = window.pollEvent()) {
                            if (ev2->is<sf::Event::Closed>()) {
                                window.close();
                                return;
                            }
                            if (const auto* k2 = ev2->getIf<sf::Event::KeyPressed>()) {
                                if (k2->scancode == sf::Keyboard::Scancode::Escape) {
                                    picking = false;
                                    break;
                                }
                            }
                            if (const auto* m2 = ev2->getIf<sf::Event::MouseButtonPressed>()) {
                                if (m2->button != sf::Mouse::Button::Left) continue;
                                const sf::Vector2f p = window.mapPixelToCoords(m2->position);
                                const float        subW = 420.f, subH = 70.f, subGap = 22.f;
                                const float        baseY = cy - (subH + subGap);
                                for (int s = 1; s <= 3; ++s) {
                                    sf::FloatRect r(sf::Vector2f(cx - subW * 0.5f, baseY + (s - 1) * (subH + subGap)),
                                                    sf::Vector2f(subW, subH));
                                    if (r.contains(p)) {
                                        const std::string pth = slot_path(s);
                                        if (saveFileExists(pth)) {
                                            requestedLoadPath = pth;
                                            running           = false;
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
                        title2.setOrigin(
                            sf::Vector2f(tb2.position.x + tb2.size.x * 0.5f, tb2.position.y + tb2.size.y * 0.5f));
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
                if (settingsRect.contains(mp)) {
                    inStartSettings      = true;
                    startSettingsPreview = UserSettings::instance().videoPreset();
                    break;
                }
            }
        }

        window.clear(sf::Color(18, 16, 24));

        const sf::Vector2u szu = window.getSize();
        const float        cx  = static_cast<float>(szu.x) * 0.5f;
        const float        cy  = static_cast<float>(szu.y) * 0.5f;
        const sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (inStartSettings) {
            StartSettingsLayout L;
            L.build(static_cast<float>(szu.x), static_cast<float>(szu.y));
            drawStartSettingsOverlay(window, font, mouseWorld, L, startSettingsPreview);
            window.display();
            continue;
        }

        sf::Text title(font, L"溯源者：文明环境", 46);
        title.setFillColor(sf::Color(245, 240, 230));
        sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        title.setPosition(sf::Vector2f(cx, cy - 210.f));
        window.draw(title);

        sf::Text subtitle(font, L"按下按钮开始旅程", 24);
        subtitle.setFillColor(sf::Color(200, 190, 170));
        sf::FloatRect sb = subtitle.getLocalBounds();
        subtitle.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
        subtitle.setPosition(sf::Vector2f(cx, cy - 155.f));
        window.draw(subtitle);

        const float btnW  = 360.f;
        const float btnH  = 68.f;
        const float gapY  = 16.f;
        const float startY = cy - 105.f;

        auto drawButton = [&](const std::wstring& text, float centerY, bool enabled) {
            sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
            btn.setPosition(sf::Vector2f(cx - btnW * 0.5f, centerY - btnH * 0.5f));
            btn.setFillColor(enabled ? sf::Color(78, 70, 110) : sf::Color(46, 46, 62));
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

        drawButton(L"开始新游戏", startY + 0.f * (btnH + gapY), true);
        drawButton(L"继续游戏", startY + 1.f * (btnH + gapY), hasSave);
        drawButton(L"读档", startY + 2.f * (btnH + gapY), true);
        drawButton(L"卡牌总览", startY + 3.f * (btnH + gapY), true);
        drawButton(L"药水总览", startY + 4.f * (btnH + gapY), true);
        drawButton(L"遗物总览", startY + 5.f * (btnH + gapY), true);
        drawButton(L"设置", startY + 6.f * (btnH + gapY), true);

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
            if (const auto chosen = runCharacterSelectScreen(window); chosen.has_value()) {
                (void)controller.initialize(*chosen);
            }
        }
    }
}

} // namespace tce
