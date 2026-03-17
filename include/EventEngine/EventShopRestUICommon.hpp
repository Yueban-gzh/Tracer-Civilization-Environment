/**
 * 事件/商店/休息 UI 公共常量与辅助函数（供 Event / Shop / Rest 三个实现文件共用）
 */
#pragma once

#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tce {

namespace esr_detail {

// ---------- 事件界面常量 ----------
constexpr float PANEL_W = 700.f;
constexpr float PANEL_H = 420.f;
constexpr float BANNER_H = 72.f;
constexpr float BANNER_EXTRA_H = 28.f;
constexpr float BANNER_EXTRA_W = 16.f;
constexpr float LEFT_ILLUST_RATIO = 0.4f;
constexpr float TITLE_CHAR_SIZE = 30.f;
constexpr float BODY_CHAR_SIZE = 20.f;
constexpr float BUTTON_H = 40.f;
constexpr float BUTTON_GAP = 16.f;
constexpr float BUTTON_TIP_W = 24.f;
constexpr float CRYSTAL_SZ = 14.f;
constexpr float PAD = 20.f;
constexpr float PANEL_H_MIN = 320.f;

const sf::Color PANEL_BG(22, 20, 28);
const sf::Color PANEL_BG_TOP(28, 26, 34);
const sf::Color PANEL_BG_BOTTOM(18, 16, 24);
const sf::Color PANEL_OUTLINE(55, 52, 65);
const sf::Color PANEL_INNER(45, 42, 55);
const sf::Color PANEL_INNER_WARM(200, 170, 100);
const sf::Color BANNER_BG(115, 95, 75);
const sf::Color BANNER_OUTLINE(90, 75, 58);
const sf::Color BANNER_TITLE_COLOR(35, 28, 22);
const sf::Color ILLUST_BG(14, 12, 18);
const sf::Color TEXT_COLOR(240, 235, 225);
const sf::Color DESC_COLOR(195, 190, 180);
const sf::Color BUTTON_BG_STS(32, 58, 58);
const sf::Color BUTTON_HOVER_STS(48, 82, 82);
const sf::Color BUTTON_OUTLINE_STS(55, 105, 100);
const sf::Color BUTTON_BEVEL(60, 115, 110);
const sf::Color CRYSTAL_COLOR(220, 195, 90);
const sf::Color BUTTON_BG(60, 56, 70);
const sf::Color BUTTON_HOVER(90, 84, 100);

// 休息界面（参考 StS：大图标 + 下方文字）
constexpr float REST_CHOICE_BTN_W = 220.f;
constexpr float REST_CHOICE_BTN_H = 268.f;      // 增高以容纳 200×200 图标 + 文字区且不重叠
constexpr float REST_CHOICE_GAP = 40.f;
// 图标再放大 2 倍感、在框内且不与「打坐/锻造」文字重叠（框不变，取最大不重叠尺寸）
constexpr float REST_CHOICE_ICON_SIZE = 220.f;  // 图标 220×220（顶满框宽，不超出、不与文字叠）
constexpr float REST_CHOICE_ICON_TOP = 8.f;     // 图标距按钮顶
constexpr float REST_CHOICE_TEXT_BOTTOM = 20.f; // 文字距按钮底（为放大图标留出空间且不重叠）
constexpr float REST_BLOCK_OFFSET_LEFT = 120.f; // 整块（标题+按钮）再往左上移
constexpr float REST_BLOCK_OFFSET_UP = 100.f;   // 整块向上偏移
const sf::Color REST_HEAL_BG(45, 85, 55);
const sf::Color REST_HEAL_HOVER(60, 110, 72);
const sf::Color REST_SMITH_BG(90, 48, 42);
const sf::Color REST_SMITH_HOVER(115, 62, 55);

inline bool try_load_texture(sf::Texture& tex, const std::vector<std::string>& candidates) {
    for (const auto& p : candidates) {
        if (tex.loadFromFile(p))
            return true;
    }
    return false;
}

inline void draw_texture_fit(sf::RenderWindow& window, const sf::Texture& tex, const sf::FloatRect& dest) {
    if (tex.getSize().x == 0 || tex.getSize().y == 0) return;
    sf::Sprite s(tex);
    const sf::Vector2u sz = tex.getSize();
    const float sx = dest.size.x / static_cast<float>(sz.x);
    const float sy = dest.size.y / static_cast<float>(sz.y);
    s.setPosition(dest.position);
    s.setScale(sf::Vector2f(sx, sy));
    window.draw(s);
}

inline std::wstring utf8_to_wstring(const std::string& utf8) {
#ifdef _WIN32
    if (utf8.empty()) return {};
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (wlen <= 0) return {};
    std::wstring out(static_cast<size_t>(wlen), 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], wlen);
    return out;
#else
    std::wstring out;
    for (unsigned char c : utf8) out += static_cast<wchar_t>(c);
    return out;
#endif
}

inline void draw_wrapped_text(sf::RenderTarget& target, const sf::Font& font,
    const sf::String& text, unsigned charSize, sf::Vector2f pos, float maxWidth, float maxHeight, sf::Color color) {
    if (maxWidth <= 1.f || maxHeight <= 1.f || text.isEmpty()) return;
    sf::Text measure(font, sf::String(L""), charSize);
    const float lineH = font.getLineSpacing(charSize);
    const int maxLines = static_cast<int>(std::floor(maxHeight / lineH));
    if (maxLines <= 0) return;
    std::vector<sf::String> lines;
    lines.reserve(static_cast<size_t>(maxLines));
    sf::String current;
    for (std::size_t i = 0; i < text.getSize(); ++i) {
        const char32_t ch = text[i];
        if (ch == U'\r') continue;
        if (ch == U'\n') {
            if (!current.isEmpty()) { lines.push_back(current); current.clear(); }
            if (static_cast<int>(lines.size()) >= maxLines) break;
            continue;
        }
        sf::String candidate = current;
        candidate += ch;
        measure.setString(candidate);
        if (!current.isEmpty() && measure.getLocalBounds().size.x > maxWidth) {
            if (!current.isEmpty()) { lines.push_back(current); current.clear(); }
            if (static_cast<int>(lines.size()) >= maxLines) break;
            current += ch;
        } else {
            current = std::move(candidate);
        }
    }
    if (!current.isEmpty() && static_cast<int>(lines.size()) < maxLines)
        lines.push_back(current);
    if (lines.empty()) return;
    std::size_t joinedLen = 0;
    for (const auto& l : lines) joinedLen += l.getSize();
    if (static_cast<int>(lines.size()) == maxLines && joinedLen + 1 < text.getSize()) {
        sf::String& last = lines.back();
        const sf::String ell = sf::String(L"\u2026");
        while (true) {
            measure.setString(last + ell);
            if (measure.getLocalBounds().size.x <= maxWidth) break;
            if (last.isEmpty()) break;
            last.erase(last.getSize() - 1, 1);
        }
        last += ell;
    }
    for (std::size_t li = 0; li < lines.size(); ++li) {
        sf::Text t(font, lines[li], charSize);
        t.setFillColor(color);
        t.setPosition(sf::Vector2f(pos.x, pos.y + static_cast<float>(li) * lineH));
        target.draw(t);
    }
}

inline sf::String truncate_to_width(const sf::Font& font, sf::String str, unsigned charSize, float maxW) {
    if (str.isEmpty() || maxW <= 0.f) return str;
    sf::Text m(font, str, charSize);
    if (m.getLocalBounds().size.x <= maxW) return str;
    const sf::String ell = sf::String(L"\u2026");
    while (str.getSize() > 0) {
        str.erase(str.getSize() - 1, 1);
        m.setString(str + ell);
        if (m.getLocalBounds().size.x <= maxW) return str + ell;
    }
    return ell;
}

inline void draw_panel_gradient(sf::RenderTarget& target, float cx, float cy, float w, float h,
    sf::Color topColor, sf::Color bottomColor) {
    const float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f;
    const float y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;
    sf::VertexArray va(sf::PrimitiveType::Triangles, 6);
    va[0].position = sf::Vector2f(x0, y0); va[0].color = topColor;
    va[1].position = sf::Vector2f(x1, y0); va[1].color = topColor;
    va[2].position = sf::Vector2f(x0, y1); va[2].color = bottomColor;
    va[3].position = sf::Vector2f(x1, y0); va[3].color = topColor;
    va[4].position = sf::Vector2f(x1, y1); va[4].color = bottomColor;
    va[5].position = sf::Vector2f(x0, y1); va[5].color = bottomColor;
    target.draw(va);
}

} // namespace esr_detail
} // namespace tce
