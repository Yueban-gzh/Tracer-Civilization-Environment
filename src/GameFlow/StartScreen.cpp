#include <SFML/Graphics.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

bool tryLoadTexture(sf::Texture& tex, const std::initializer_list<std::string>& paths) {
    for (const auto& p : paths) {
        if (tex.loadFromFile(p)) return true;
    }
    return false;
}

void drawTextureCover(sf::RenderWindow& window, const sf::Texture& tex) {
    const sf::Vector2u ws = window.getSize();
    const sf::Vector2u ts = tex.getSize();
    if (ws.x == 0 || ws.y == 0 || ts.x == 0 || ts.y == 0) return;
    sf::Sprite sp(tex);
    const float sx = static_cast<float>(ws.x) / static_cast<float>(ts.x);
    const float sy = static_cast<float>(ws.y) / static_cast<float>(ts.y);
    const float s = std::max(sx, sy);
    sp.setScale(sf::Vector2f(s, s));
    sp.setOrigin(sf::Vector2f(ts.x * 0.5f, ts.y * 0.5f));
    sp.setPosition(sf::Vector2f(static_cast<float>(ws.x) * 0.5f, static_cast<float>(ws.y) * 0.5f));
    window.draw(sp);
}

struct MainMenuLayout {
    std::array<sf::FloatRect, 7> rows{};
    float titleY = 0.f;
    float subtitleY = 0.f;
    float hintY = 0.f;
};

MainMenuLayout buildMainMenuLayout(float ww, float hh) {
    MainMenuLayout L;
    const float centerX = ww * 0.5f;
    const float titleBaseY = hh * 0.185f;
    L.titleY = titleBaseY;
    L.subtitleY = titleBaseY + 78.f;
    L.hintY = titleBaseY + 118.f;

    const float startY = hh * 0.345f;
    const std::array<float, 7> heights = {60.f, 60.f, 52.f, 52.f, 52.f, 52.f, 52.f};
    const std::array<float, 7> widths = {392.f, 392.f, 350.f, 350.f, 350.f, 350.f, 350.f};
    const std::array<float, 7> gaps = {14.f, 22.f, 18.f, 10.f, 10.f, 14.f, 0.f};

    float y = startY;
    for (int i = 0; i < 7; ++i) {
        const float w = widths[static_cast<std::size_t>(i)];
        const float h = heights[static_cast<std::size_t>(i)];
        L.rows[static_cast<std::size_t>(i)] =
            sf::FloatRect(sf::Vector2f(centerX - w * 0.5f, y), sf::Vector2f(w, h));
        y += h + gaps[static_cast<std::size_t>(i)];
    }
    return L;
}

void drawInkMistBackdrop(sf::RenderWindow& window, float ww, float hh) {
    sf::RectangleShape v(sf::Vector2f(ww, hh));
    v.setPosition(sf::Vector2f(0.f, 0.f));
    v.setFillColor(sf::Color(8, 10, 16, 74));
    window.draw(v);

    sf::RectangleShape centerBand(sf::Vector2f(ww * 0.44f, hh * 0.64f));
    centerBand.setOrigin(sf::Vector2f(centerBand.getSize().x * 0.5f, centerBand.getSize().y * 0.5f));
    centerBand.setPosition(sf::Vector2f(ww * 0.5f, hh * 0.44f));
    centerBand.setFillColor(sf::Color(12, 14, 22, 54));
    centerBand.setOutlineThickness(0.f);
    window.draw(centerBand);

    const sf::Color markColor(178, 150, 96, 48);
    const float markW = 26.f;
    const float markH = 1.f;
    const float markOffsetX = centerBand.getSize().x * 0.5f - 28.f;
    const float markTopY = centerBand.getPosition().y - centerBand.getSize().y * 0.5f + 24.f;
    const float markBotY = centerBand.getPosition().y + centerBand.getSize().y * 0.5f - 24.f;
    sf::RectangleShape mark(sf::Vector2f(markW, markH));
    mark.setFillColor(markColor);
    mark.setPosition(sf::Vector2f(centerBand.getPosition().x - markOffsetX, markTopY));
    window.draw(mark);
    mark.setPosition(sf::Vector2f(centerBand.getPosition().x + markOffsetX - markW, markTopY));
    window.draw(mark);
    mark.setPosition(sf::Vector2f(centerBand.getPosition().x - markOffsetX, markBotY));
    window.draw(mark);
    mark.setPosition(sf::Vector2f(centerBand.getPosition().x + markOffsetX - markW, markBotY));
    window.draw(mark);
}

void drawMainTitle(sf::RenderWindow& window, const sf::Font& font, float ww, const MainMenuLayout& L, float t) {
    const float pulse = 0.5f + 0.5f * std::sin(t * 1.2f);

    sf::Text titleGlow(font, sf::String(L"溯源者"), 80u);
    titleGlow.setFillColor(sf::Color(214, 182, 112, static_cast<std::uint8_t>(24 + 20 * pulse)));
    const sf::FloatRect gb = titleGlow.getLocalBounds();
    titleGlow.setOrigin(sf::Vector2f(gb.position.x + gb.size.x * 0.5f, gb.position.y + gb.size.y * 0.5f));
    titleGlow.setPosition(sf::Vector2f(ww * 0.5f, L.titleY + 2.f));
    window.draw(titleGlow);

    sf::Text title(font, sf::String(L"溯源者"), 75u);
    title.setLetterSpacing(1.2f);
    title.setFillColor(sf::Color(240, 231, 210));
    title.setOutlineColor(sf::Color(34, 28, 18, 228));
    title.setOutlineThickness(2.2f);
    const sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    title.setPosition(sf::Vector2f(ww * 0.5f, L.titleY));
    window.draw(title);

    sf::RectangleShape sep(sf::Vector2f(220.f, 1.f));
    sep.setOrigin(sf::Vector2f(sep.getSize().x * 0.5f, 0.f));
    sep.setPosition(sf::Vector2f(ww * 0.5f, L.titleY + 47.f));
    sep.setFillColor(sf::Color(182, 154, 98, 162));
    window.draw(sep);

    sf::Text subtitle(font, sf::String(L"文明环境"), 31u);
    subtitle.setLetterSpacing(1.26f);
    subtitle.setFillColor(sf::Color(208, 199, 182));
    subtitle.setOutlineColor(sf::Color(20, 18, 24, 180));
    subtitle.setOutlineThickness(1.f);
    const sf::FloatRect sb = subtitle.getLocalBounds();
    subtitle.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
    subtitle.setPosition(sf::Vector2f(ww * 0.5f, L.subtitleY));
    window.draw(subtitle);
}

void drawMenuButton(sf::RenderWindow& window, const sf::Font& font, const sf::FloatRect& r, const std::wstring& label,
                    bool enabled, float hover01, float press01, bool primary, bool subdued) {
    const float lift = hover01 * 2.2f - press01 * 1.8f;
    sf::ConvexShape shape(6);
    const float bevel = 16.f;
    shape.setPoint(0, sf::Vector2f(r.position.x + bevel, r.position.y + lift));
    shape.setPoint(1, sf::Vector2f(r.position.x + r.size.x - bevel, r.position.y + lift));
    shape.setPoint(2, sf::Vector2f(r.position.x + r.size.x, r.position.y + r.size.y * 0.5f + lift));
    shape.setPoint(3, sf::Vector2f(r.position.x + r.size.x - bevel, r.position.y + r.size.y + lift));
    shape.setPoint(4, sf::Vector2f(r.position.x + bevel, r.position.y + r.size.y + lift));
    shape.setPoint(5, sf::Vector2f(r.position.x, r.position.y + r.size.y * 0.5f + lift));

    const sf::Color fillBase = primary ? sf::Color(34, 42, 62, 198)
                                       : (subdued ? sf::Color(26, 32, 48, 182) : sf::Color(30, 36, 54, 190));
    const sf::Color fillHover = primary ? sf::Color(42, 52, 76, 214)
                                        : (subdued ? sf::Color(34, 42, 62, 196) : sf::Color(38, 46, 68, 204));
    const sf::Color lineBase = enabled ? (subdued ? sf::Color(132, 112, 78, 166) : sf::Color(150, 125, 82, 184))
                                       : sf::Color(96, 94, 92, 150);
    const sf::Color lineHover = enabled ? (subdued ? sf::Color(188, 162, 108, 212) : sf::Color(214, 182, 116, 232))
                                        : sf::Color(116, 114, 112, 168);

    const auto lerpColor = [](const sf::Color& a, const sf::Color& b, float t) {
        auto ch = [t](std::uint8_t x, std::uint8_t y) -> std::uint8_t {
            return static_cast<std::uint8_t>(x + (y - x) * std::clamp(t, 0.f, 1.f));
        };
        return sf::Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
    };

    shape.setFillColor(lerpColor(fillBase, fillHover, hover01));
    shape.setOutlineColor(lerpColor(lineBase, lineHover, hover01));
    shape.setOutlineThickness(1.55f + hover01 * 0.75f);
    window.draw(shape);

    sf::RectangleShape inner(sf::Vector2f(r.size.x - 24.f, std::max(6.f, r.size.y * 0.45f)));
    inner.setOrigin(sf::Vector2f(inner.getSize().x * 0.5f, 0.f));
    inner.setPosition(sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + 5.f + lift));
    inner.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(8 + 14 * hover01)));
    window.draw(inner);

    const unsigned fontSize = primary ? 31u : (subdued ? 24u : 26u);
    sf::Text txt(font, sf::String(label), fontSize);
    txt.setFillColor(enabled ? (subdued ? sf::Color(214, 206, 190) : sf::Color(236, 228, 208)) : sf::Color(136, 136, 142));
    txt.setOutlineColor(sf::Color(16, 14, 20, 190));
    txt.setOutlineThickness(1.f);
    const sf::FloatRect tb = txt.getLocalBounds();
    txt.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    txt.setPosition(sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.5f + lift));
    window.draw(txt);
}

void drawLoadSlotButton(sf::RenderWindow& window, const sf::Font& font, const sf::FloatRect& r, const std::wstring& label,
                        bool enabled, float hover01) {
    sf::ConvexShape shape(6);
    const float bevel = 18.f;
    shape.setPoint(0, sf::Vector2f(r.position.x + bevel, r.position.y));
    shape.setPoint(1, sf::Vector2f(r.position.x + r.size.x - bevel, r.position.y));
    shape.setPoint(2, sf::Vector2f(r.position.x + r.size.x, r.position.y + r.size.y * 0.5f));
    shape.setPoint(3, sf::Vector2f(r.position.x + r.size.x - bevel, r.position.y + r.size.y));
    shape.setPoint(4, sf::Vector2f(r.position.x + bevel, r.position.y + r.size.y));
    shape.setPoint(5, sf::Vector2f(r.position.x, r.position.y + r.size.y * 0.5f));

    const sf::Color fillBase = enabled ? sf::Color(34, 42, 62, 202) : sf::Color(28, 32, 46, 178);
    const sf::Color fillHover = enabled ? sf::Color(46, 56, 82, 218) : sf::Color(32, 38, 54, 188);
    const sf::Color edgeBase = enabled ? sf::Color(160, 132, 86, 188) : sf::Color(108, 104, 100, 150);
    const sf::Color edgeHover = enabled ? sf::Color(222, 186, 118, 232) : sf::Color(126, 120, 114, 164);

    const auto lerpColor = [](const sf::Color& a, const sf::Color& b, float t) {
        const float k = std::clamp(t, 0.f, 1.f);
        auto ch = [k](std::uint8_t x, std::uint8_t y) -> std::uint8_t {
            return static_cast<std::uint8_t>(x + (y - x) * k);
        };
        return sf::Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
    };

    shape.setFillColor(lerpColor(fillBase, fillHover, hover01));
    shape.setOutlineColor(lerpColor(edgeBase, edgeHover, hover01));
    shape.setOutlineThickness(1.7f + hover01 * 0.8f);
    window.draw(shape);

    sf::RectangleShape hl(sf::Vector2f(r.size.x - 28.f, std::max(8.f, r.size.y * 0.42f)));
    hl.setOrigin(sf::Vector2f(hl.getSize().x * 0.5f, 0.f));
    hl.setPosition(sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + 5.f));
    hl.setFillColor(sf::Color(255, 255, 255, static_cast<std::uint8_t>(10 + 18 * hover01)));
    window.draw(hl);

    sf::Text txt(font, sf::String(label), 27u);
    txt.setFillColor(enabled ? sf::Color(236, 228, 208) : sf::Color(140, 140, 146));
    txt.setOutlineColor(sf::Color(16, 14, 20, 190));
    txt.setOutlineThickness(1.f);
    const sf::FloatRect tb = txt.getLocalBounds();
    txt.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    txt.setPosition(sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.5f));
    window.draw(txt);
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

    sf::Texture mainBgTex;
    const bool mainBgLoaded = tryLoadTexture(mainBgTex, {
        "assets/backgrounds/main_bg.png",
        "./assets/backgrounds/main_bg.png",
    });

    bool        running              = true;
    bool        requestedContinue    = false;
    std::string requestedLoadPath;
    bool        inStartSettings      = false;
    int         startSettingsPreview = 0;
    std::array<float, 7> hoverAnim{};
    std::array<float, 7> pressAnim{};
    sf::Clock frameClock;
    sf::Clock animClock;

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

                const MainMenuLayout menuL = buildMainMenuLayout(static_cast<float>(sz.x), static_cast<float>(sz.y));
                const sf::FloatRect& newGameRect       = menuL.rows[0];
                const sf::FloatRect& contRect          = menuL.rows[1];
                const sf::FloatRect& loadRect          = menuL.rows[2];
                const sf::FloatRect& catalogRect       = menuL.rows[3];
                const sf::FloatRect& potionCatalogRect = menuL.rows[4];
                const sf::FloatRect& relicCatalogRect  = menuL.rows[5];
                const sf::FloatRect& settingsRect      = menuL.rows[6];

                if (newGameRect.contains(mp)) {
                    pressAnim[0] = 1.f;
                    if (const auto chosen = runCharacterSelectScreen(window); chosen.has_value()) {
                        (void)controller.initialize(*chosen);
                        running = false;
                        break;
                    }
                    break;
                }
                if (hasSave && contRect.contains(mp)) {
                    pressAnim[1] = 1.f;
                    requestedContinue = true;
                    running           = false;
                    break;
                }
                if (loadRect.contains(mp)) {
                    pressAnim[2] = 1.f;
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
                                const float        subW = 428.f, subH = 74.f, subGap = 19.f;
                                const float        baseY = cy - 72.f;
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
                        if (mainBgLoaded) {
                            drawTextureCover(window, mainBgTex);
                        } else {
                            window.clear(sf::Color(18, 16, 24));
                        }
                        drawInkMistBackdrop(window, static_cast<float>(sz.x), static_cast<float>(sz.y));

                        sf::RectangleShape panel(sf::Vector2f(560.f, 430.f));
                        panel.setOrigin(sf::Vector2f(panel.getSize().x * 0.5f, panel.getSize().y * 0.5f));
                        panel.setPosition(sf::Vector2f(cx, cy + 10.f));
                        panel.setFillColor(sf::Color(12, 16, 24, 132));
                        panel.setOutlineThickness(1.f);
                        panel.setOutlineColor(sf::Color(180, 150, 92, 42));
                        window.draw(panel);

                        sf::Text title2(font, L"读档", 52);
                        title2.setFillColor(sf::Color(238, 228, 205));
                        title2.setOutlineColor(sf::Color(22, 18, 16, 212));
                        title2.setOutlineThickness(2.f);
                        sf::FloatRect tb2 = title2.getLocalBounds();
                        title2.setOrigin(
                            sf::Vector2f(tb2.position.x + tb2.size.x * 0.5f, tb2.position.y + tb2.size.y * 0.5f));
                        title2.setPosition(sf::Vector2f(cx, cy - 188.f));
                        window.draw(title2);

                        sf::Text subline(font, L"选择存档槽位", 23);
                        subline.setFillColor(sf::Color(194, 184, 166, 206));
                        const sf::FloatRect slb = subline.getLocalBounds();
                        subline.setOrigin(sf::Vector2f(slb.position.x + slb.size.x * 0.5f, slb.position.y + slb.size.y * 0.5f));
                        subline.setPosition(sf::Vector2f(cx, cy - 138.f));
                        window.draw(subline);

                        sf::RectangleShape sep(sf::Vector2f(220.f, 1.f));
                        sep.setOrigin(sf::Vector2f(sep.getSize().x * 0.5f, 0.f));
                        sep.setPosition(sf::Vector2f(cx, cy - 116.f));
                        sep.setFillColor(sf::Color(182, 154, 98, 148));
                        window.draw(sep);

                        const float subW = 428.f, subH = 74.f, subGap = 19.f;
                        const float baseY = cy - 72.f;
                        const sf::Vector2f mouseP = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                        for (int s = 1; s <= 3; ++s) {
                            const bool ok = saveFileExists(slot_path(s));
                            sf::FloatRect r(sf::Vector2f(cx - subW * 0.5f, baseY + (s - 1) * (subH + subGap)),
                                            sf::Vector2f(subW, subH));
                            const bool hover = r.contains(mouseP);
                            std::wstring label = L"槽位 " + std::to_wstring(s) + (ok ? L" · 可读取" : L" · 空");
                            drawLoadSlotButton(window, font, r, label, ok, hover ? 1.f : 0.f);
                        }
                        sf::Text hint(font, L"Esc 返回主菜单", 18);
                        hint.setFillColor(sf::Color(182, 172, 154, 176));
                        hint.setPosition(sf::Vector2f(24.f, static_cast<float>(sz.y) - 42.f));
                        window.draw(hint);
                        window.display();
                    }
                    break;
                }
                if (catalogRect.contains(mp)) {
                    pressAnim[3] = 1.f;
                    runCardCatalogScreen(window);
                    hasSave = saveFileExists(savePath);
                    break;
                }
                if (potionCatalogRect.contains(mp)) {
                    pressAnim[4] = 1.f;
                    runPotionCatalogScreen(window);
                    hasSave = saveFileExists(savePath);
                    break;
                }
                if (relicCatalogRect.contains(mp)) {
                    pressAnim[5] = 1.f;
                    runRelicCatalogScreen(window);
                    hasSave = saveFileExists(savePath);
                    break;
                }
                if (settingsRect.contains(mp)) {
                    pressAnim[6] = 1.f;
                    inStartSettings      = true;
                    startSettingsPreview = UserSettings::instance().videoPreset();
                    break;
                }
            }
        }

        const float dt = std::min(0.05f, frameClock.restart().asSeconds());
        const float t = animClock.getElapsedTime().asSeconds();

        const sf::Vector2u szu = window.getSize();
        const float        cx  = static_cast<float>(szu.x) * 0.5f;
        const sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));

        if (inStartSettings) {
            if (mainBgLoaded) {
                drawTextureCover(window, mainBgTex);
            } else {
                window.clear(sf::Color(18, 16, 24));
            }
            StartSettingsLayout L;
            L.build(static_cast<float>(szu.x), static_cast<float>(szu.y));
            drawStartSettingsOverlay(window, font, mouseWorld, L, startSettingsPreview);
            window.display();
            continue;
        }

        if (mainBgLoaded) {
            drawTextureCover(window, mainBgTex);
        } else {
            window.clear(sf::Color(18, 16, 24));
        }
        drawInkMistBackdrop(window, static_cast<float>(szu.x), static_cast<float>(szu.y));

        const MainMenuLayout menuL = buildMainMenuLayout(static_cast<float>(szu.x), static_cast<float>(szu.y));
        drawMainTitle(window, font, static_cast<float>(szu.x), menuL, t);

        sf::Text hint(font, sf::String(L"溯文明遗痕，辨失序真相"), 18);
        hint.setFillColor(sf::Color(196, 188, 170, 186));
        const sf::FloatRect hb = hint.getLocalBounds();
        hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
        hint.setPosition(sf::Vector2f(cx, menuL.hintY));
        window.draw(hint);

        const std::array<std::wstring, 7> labels = {
            L"开始新游戏", L"继续游戏", L"读档", L"卡牌总览", L"灵液总览", L"遗物总览", L"设置"};
        const std::array<bool, 7> enabled = {true, hasSave, true, true, true, true, true};
        const std::array<bool, 7> primary = {true, true, false, false, false, false, false};
        const std::array<bool, 7> subdued = {false, false, false, false, false, false, true};

        for (int i = 0; i < 7; ++i) {
            const bool hover = menuL.rows[static_cast<std::size_t>(i)].contains(mouseWorld) && enabled[static_cast<std::size_t>(i)];
            const float hoverSpeed = hover ? 8.5f : 7.5f;
            hoverAnim[static_cast<std::size_t>(i)] =
                std::clamp(hoverAnim[static_cast<std::size_t>(i)] + (hover ? 1.f : -1.f) * hoverSpeed * dt, 0.f, 1.f);
            pressAnim[static_cast<std::size_t>(i)] =
                std::max(0.f, pressAnim[static_cast<std::size_t>(i)] - 4.6f * dt);
            drawMenuButton(window, font, menuL.rows[static_cast<std::size_t>(i)], labels[static_cast<std::size_t>(i)],
                           enabled[static_cast<std::size_t>(i)], hoverAnim[static_cast<std::size_t>(i)],
                           pressAnim[static_cast<std::size_t>(i)], primary[static_cast<std::size_t>(i)],
                           subdued[static_cast<std::size_t>(i)]);
        }

        sf::Text footer(font, sf::String(L"Build 0.1.0  |  文脉崩坏档案"), 16);
        footer.setFillColor(sf::Color(172, 164, 148, 150));
        footer.setPosition(sf::Vector2f(24.f, static_cast<float>(szu.y) - 34.f));
        window.draw(footer);

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
