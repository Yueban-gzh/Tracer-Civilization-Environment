/**
 * 事件 / 商店 / 休息 UI 实现
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "DataLayer/DataTypes.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tce {

namespace {

// 事件界面：杀戮尖塔风格 - 大面板 + 卷轴标题 + 左图右文 + 右侧尖角选项按钮
constexpr float PANEL_W = 700.f;
constexpr float PANEL_H = 420.f;
constexpr float BANNER_H = 72.f;          // 标题条基准高度（足够承载标题字不拥挤）
constexpr float BANNER_EXTRA_H = 28.f;   // 卷轴条总高 = BANNER_H + BANNER_EXTRA_H
constexpr float BANNER_EXTRA_W = 16.f;
constexpr float LEFT_ILLUST_RATIO = 0.4f;  // 左图略窄、右侧略宽，贴近 StS
constexpr float TITLE_CHAR_SIZE = 30.f;   // 基准标题字号（最终会随面板 scale 放大）
constexpr float BODY_CHAR_SIZE = 20.f;
constexpr float BUTTON_H = 40.f;
constexpr float BUTTON_GAP = 16.f;
constexpr float BUTTON_TIP_W = 24.f;      // 选项按钮右侧尖角宽度（StS 箭头形）
constexpr float CRYSTAL_SZ = 14.f;       // 选项左侧水晶图标尺寸
constexpr float PAD = 20.f;
const sf::Color PANEL_BG(22, 20, 28);
const sf::Color PANEL_BG_TOP(28, 26, 34);   // 面板顶部略亮（渐变用）
const sf::Color PANEL_BG_BOTTOM(18, 16, 24);
const sf::Color PANEL_OUTLINE(55, 52, 65);
const sf::Color PANEL_INNER(45, 42, 55);
const sf::Color PANEL_INNER_WARM(200, 170, 100);  // 内圈暖色描边（琥珀/金）
const sf::Color BANNER_BG(115, 95, 75);   // 羊皮纸/卷轴色
const sf::Color BANNER_OUTLINE(90, 75, 58);
const sf::Color BANNER_TITLE_COLOR(35, 28, 22);  // 卷轴上标题用深色（StS 金神像）
const sf::Color ILLUST_BG(14, 12, 18);
const sf::Color TEXT_COLOR(240, 235, 225);
const sf::Color DESC_COLOR(195, 190, 180);
const sf::Color BUTTON_BG_STS(32, 58, 58);
const sf::Color BUTTON_HOVER_STS(48, 82, 82);
const sf::Color BUTTON_OUTLINE_STS(55, 105, 100);
const sf::Color BUTTON_BEVEL(60, 115, 110);      // 按钮高光边
const sf::Color CRYSTAL_COLOR(220, 195, 90);    // 选项前小水晶图标
// 商店/休息界面用通用按钮色
const sf::Color BUTTON_BG(60, 56, 70);
const sf::Color BUTTON_HOVER(90, 84, 100);
constexpr float PANEL_H_MIN = 320.f;

static bool try_load_texture(sf::Texture& tex, const std::vector<std::string>& candidates) {
    for (const auto& p : candidates) {
        if (tex.loadFromFile(p))
            return true;
    }
    return false;
}

static void draw_texture_fit(sf::RenderWindow& window, const sf::Texture& tex, const sf::FloatRect& dest) {
    if (tex.getSize().x == 0 || tex.getSize().y == 0) return;
    sf::Sprite s(tex);
    const sf::Vector2u sz = tex.getSize();
    const float sx = dest.size.x / static_cast<float>(sz.x);
    const float sy = dest.size.y / static_cast<float>(sz.y);
    s.setPosition(dest.position);
    s.setScale(sf::Vector2f(sx, sy));
    window.draw(s);
}

static std::wstring utf8_to_wstring(const std::string& utf8) {
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

// 描述文字按最大宽度自动换行，限制在 maxHeight 内，超出用省略号
static void draw_wrapped_text(sf::RenderTarget& target, const sf::Font& font,
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

// 若 str 显示宽度超过 maxW 则截断并加省略号（按字符从尾删）
static sf::String truncate_to_width(const sf::Font& font, sf::String str, unsigned charSize, float maxW) {
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

} // namespace

EventShopRestUI::EventShopRestUI(unsigned width, unsigned height)
    : width_(width), height_(height) {
    eventBgLoaded_ = try_load_texture(eventBgTexture_, {
        "assets/backgrounds/event_bg_cn.png",
        "assets/backgrounds/event_bg_cn..png",
        "./assets/backgrounds/event_bg_cn.png",
        "./assets/backgrounds/event_bg_cn..png"
    });
    eventBannerLoaded_ = try_load_texture(eventBannerTexture_, {
        "assets/ui/event/banner_scroll_cn.png",
        "./assets/ui/event/banner_scroll_cn.png"
    });
    eventPanelFrameLoaded_ = try_load_texture(eventPanelFrameTexture_, {
        "assets/ui/event/panel_frame_cn.png",
        "./assets/ui/event/panel_frame_cn.png"
    });
}

bool EventShopRestUI::loadFont(const std::string& path) {
    fontLoaded_ = font_.openFromFile(path);
    return fontLoaded_;
}

bool EventShopRestUI::loadChineseFont(const std::string& path) {
    fontChineseLoaded_ = fontChinese_.openFromFile(path);
    return fontChineseLoaded_;
}

void EventShopRestUI::setScreen(EventShopRestScreen screen) {
    screen_ = screen;
    pendingEventOption_ = -1;
    eventOptionPressedIndex_ = -1;
    if (screen == EventShopRestScreen::Event) selectedEventOption_ = 0;
    pendingShopBuy_ = false;
    pendingShopRemove_ = false;
    pendingRestHeal_ = false;
    pendingRestUpgrade_ = false;
}

void EventShopRestUI::setEventData(const EventDisplayData& data) {
    eventData_ = data;
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventOptionRects_.clear();
    if (data.imagePath != eventIllustPath_) {
        eventIllustPath_ = data.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
}

void EventShopRestUI::setEventDataFromUtf8(const std::string& title, const std::string& description,
                                           const std::vector<std::string>& optionTexts,
                                           const std::string& imagePath) {
    eventData_.title = utf8_to_wstring(title);
    eventData_.description = utf8_to_wstring(description);
    eventData_.optionTexts.clear();
    for (const auto& s : optionTexts)
        eventData_.optionTexts.push_back(utf8_to_wstring(s));
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventData_.imagePath = imagePath;
    eventOptionRects_.clear();
    if (eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
}

void EventShopRestUI::setEventResultFromUtf8(const std::string& resultSummary) {
    eventData_.title = std::wstring(L"事件结果");
    eventData_.description = utf8_to_wstring(resultSummary);
    eventData_.optionTexts.assign(1, std::wstring(L"确定"));
    eventData_.imagePath.clear();
    eventOptionRects_.clear();
    if (!eventIllustPath_.empty()) {
        eventIllustPath_.clear();
        eventIllustLoaded_ = false;
    }
}

void EventShopRestUI::setEventDataFromEvent(const DataLayer::Event* event) {
    if (!event) return;
    eventData_.title = utf8_to_wstring(event->title);
    eventData_.description = utf8_to_wstring(event->description);
    eventData_.optionTexts.clear();
    for (const auto& opt : event->options)
        eventData_.optionTexts.push_back(utf8_to_wstring(opt.text));
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventData_.imagePath = event->image;
    eventOptionRects_.clear();
    if (eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
}

void EventShopRestUI::setShopData(const ShopDisplayData& data) {
    shopData_ = data;
    shopBuyRects_.clear();
    shopRemoveRects_.clear();
}

void EventShopRestUI::setRestData(const RestDisplayData& data) {
    restData_ = data;
    restUpgradeRects_.clear();
}

void EventShopRestUI::setMousePosition(sf::Vector2f pos) {
    mousePos_ = pos;
}

bool EventShopRestUI::clickInRect(const sf::Vector2f& pos, const sf::FloatRect& r) const {
    return r.contains(pos);
}

void EventShopRestUI::drawPanel(sf::RenderWindow& window, float centerX, float centerY, float w, float h) {
    sf::RectangleShape rect(sf::Vector2f(w, h));
    rect.setOrigin(sf::Vector2f(w * 0.5f, h * 0.5f));
    rect.setPosition(sf::Vector2f(centerX, centerY));
    rect.setFillColor(PANEL_BG);
    rect.setOutlineThickness(2.f);
    rect.setOutlineColor(PANEL_OUTLINE);
    window.draw(rect);
}

// 绘制垂直渐变矩形（上略亮、下略暗，增加层次）
static void draw_panel_gradient(sf::RenderTarget& target, float cx, float cy, float w, float h,
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

void EventShopRestUI::drawEventScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;  // 面板垂直居中，保证边框四周不被窗口裁剪
    // 事件区域：略缩小占屏比并留出边框放大空间，避免边框右侧/底部被裁切
    const float panelW = std::max(720.f, static_cast<float>(width_) * 0.68f);
    const float panelH = std::max(440.f, static_cast<float>(height_) * 0.71f);
    const float panelLeft = cx - panelW * 0.5f;
    const float panelTop = cy - panelH * 0.5f;
    const float leftW = panelW * LEFT_ILLUST_RATIO;
    const float rightW = panelW - leftW;
    const float bannerBoxH = BANNER_H + BANNER_EXTRA_H;
    // 半嵌入式卷轴：上缘压住内层顶部，不挡最外层木框，像“挂在面板顶部”
    constexpr float BANNER_INNER_OVERHANG = 12.f;  // 卷轴上缘伸入内层顶部的像素
    const float scale = panelW / PANEL_W;  // 随 3/4 屏放大时略微放大字号
    const unsigned titleSize = static_cast<unsigned>(std::round(TITLE_CHAR_SIZE * scale));
    const unsigned bodySize = static_cast<unsigned>(std::round(BODY_CHAR_SIZE * scale));

    // 1. 先画面板与卷轴，边框在卷轴之后画以便顶部不被盖住
    draw_panel_gradient(window, cx, cy, panelW, panelH, PANEL_BG_TOP, PANEL_BG_BOTTOM);
    sf::RectangleShape panelOutline(sf::Vector2f(panelW, panelH));
    panelOutline.setOrigin(sf::Vector2f(panelW * 0.5f, panelH * 0.5f));
    panelOutline.setPosition(sf::Vector2f(cx, cy));
    panelOutline.setFillColor(sf::Color::Transparent);
    panelOutline.setOutlineThickness(2.f);
    panelOutline.setOutlineColor(PANEL_OUTLINE);
    window.draw(panelOutline);
    sf::RectangleShape innerBorder(sf::Vector2f(panelW - 6.f, panelH - 6.f));
    innerBorder.setOrigin(sf::Vector2f(innerBorder.getSize().x * 0.5f, innerBorder.getSize().y * 0.5f));
    innerBorder.setPosition(sf::Vector2f(cx, cy));
    innerBorder.setFillColor(sf::Color::Transparent);
    innerBorder.setOutlineThickness(1.f);
    innerBorder.setOutlineColor(PANEL_INNER);
    window.draw(innerBorder);
    sf::RectangleShape innerWarm(sf::Vector2f(panelW - 14.f, panelH - 14.f));
    innerWarm.setOrigin(sf::Vector2f(innerWarm.getSize().x * 0.5f, innerWarm.getSize().y * 0.5f));
    innerWarm.setPosition(sf::Vector2f(cx, cy));
    innerWarm.setFillColor(sf::Color::Transparent);
    innerWarm.setOutlineThickness(1.f);
    innerWarm.setOutlineColor(sf::Color(PANEL_INNER_WARM.r, PANEL_INNER_WARM.g, PANEL_INNER_WARM.b, 90));
    window.draw(innerWarm);

    // 边框贴图先画（在面板之上、卷轴之下），避免挡住标题卷轴
    constexpr float FRAME_MARGIN_SIDES = 28.f;
    constexpr float FRAME_MARGIN_TOP  = 56.f;
    constexpr float FRAME_SCALE       = 1.22f;
    if (eventPanelFrameLoaded_) {
        const float baseW = panelW + FRAME_MARGIN_SIDES * 2.f;
        const float baseH = panelH + FRAME_MARGIN_TOP + FRAME_MARGIN_SIDES;
        const float frameWidth  = baseW * FRAME_SCALE;
        const float frameHeight = baseH * FRAME_SCALE;
        const float frameLeft   = cx - frameWidth * 0.5f;
        const float frameTop    = cy - frameHeight * 0.5f;
        draw_texture_fit(window, eventPanelFrameTexture_,
            sf::FloatRect(sf::Vector2f(frameLeft, frameTop), sf::Vector2f(frameWidth, frameHeight)));
    }

    // 2. 半嵌入式标题卷轴：整体在框内，上缘压住内层顶部，不遮外框雕花
    const float bannerY = panelTop - BANNER_INNER_OVERHANG;
    const float contentTop = bannerY + bannerBoxH + 2.f;  // 内容区紧贴卷轴下缘
    const float bannerW = panelW + BANNER_EXTRA_W * 2.f;
    if (eventBannerLoaded_) {
        draw_texture_fit(window, eventBannerTexture_,
            sf::FloatRect(sf::Vector2f(cx - bannerW * 0.5f, bannerY), sf::Vector2f(bannerW, bannerBoxH)));
    } else {
        sf::RectangleShape banner(sf::Vector2f(bannerW, bannerBoxH));
        banner.setOrigin(sf::Vector2f(bannerW * 0.5f, 0.f));
        banner.setPosition(sf::Vector2f(cx, bannerY));
        banner.setFillColor(BANNER_BG);
        banner.setOutlineThickness(2.f);
        banner.setOutlineColor(BANNER_OUTLINE);
        window.draw(banner);
    }

    const float bannerMaxTitleW = bannerW - 28.f;
    const sf::String effectiveTitle = eventData_.title.empty() ? sf::String(L"事件") : sf::String(eventData_.title);
    sf::String titleStr = truncate_to_width(fontForText(), effectiveTitle, titleSize, bannerMaxTitleW);
    sf::Text title(fontForText(), titleStr, titleSize);
    title.setFillColor(BANNER_TITLE_COLOR);
    const sf::FloatRect tb = title.getLocalBounds();
    const float titleCx = tb.position.x + tb.size.x * 0.5f;
    const float titleCy = tb.position.y + tb.size.y * 0.5f;
    title.setOrigin(sf::Vector2f(titleCx, titleCy));
    title.setPosition(sf::Vector2f(cx, bannerY + bannerBoxH * 0.5f));
    const float titleHalfW = tb.size.x * 0.5f;
    const float diamondSz = 6.f * scale;
    const float bannerCenterY = bannerY + bannerBoxH * 0.5f;
    auto draw_title_diamond = [&](float dx) {
        sf::ConvexShape d(4);
        d.setPoint(0, sf::Vector2f(0, -diamondSz));
        d.setPoint(1, sf::Vector2f(diamondSz, 0));
        d.setPoint(2, sf::Vector2f(0, diamondSz));
        d.setPoint(3, sf::Vector2f(-diamondSz, 0));
        d.setPosition(sf::Vector2f(cx + dx, bannerCenterY));
        d.setFillColor(sf::Color(220, 190, 100));
        d.setOutlineThickness(0.5f);
        d.setOutlineColor(sf::Color(180, 150, 70));
        window.draw(d);
    };
    draw_title_diamond(-titleHalfW - 18.f);
    draw_title_diamond(titleHalfW + 18.f);
    window.draw(title);

    // 3. 左侧插图区：加大上下左右余量，不描边，避免“明显框”
    const float illustMarginV = 24.f;
    const float illustMarginH = 20.f;
    const float illustLeft = panelLeft + illustMarginH;
    const float illustW = leftW - illustMarginH * 2.f;
    const float illustH = panelH - (contentTop - panelTop) - illustMarginV * 2.f;
    const float illustTop = contentTop + illustMarginV;
    const float illustCx = illustLeft + illustW * 0.5f;
    const float illustCy = illustTop + illustH * 0.5f;
    // 不再绘制左侧黑底矩形，避免出现黑框；有图时直接贴图，无图时仅显示占位文字
    if (eventIllustLoaded_ && eventIllustTexture_.getSize().x > 0) {
        sf::Sprite sprite(eventIllustTexture_);
        const sf::Vector2u texSize = eventIllustTexture_.getSize();
        const float scaleX = illustW / static_cast<float>(texSize.x);
        const float scaleY = illustH / static_cast<float>(texSize.y);
        const float scale = std::min(scaleX, scaleY);
        sprite.setScale(sf::Vector2f(scale, scale));
        sprite.setOrigin(sf::Vector2f(texSize.x * 0.5f, texSize.y * 0.5f));
        sprite.setPosition(sf::Vector2f(illustCx, illustCy));
        window.draw(sprite);
    } else {
        sf::Text illustHint(fontForText(), sf::String(L"[ 插图 ]"), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        illustHint.setFillColor(sf::Color(80, 78, 90));
        const sf::FloatRect ihb = illustHint.getLocalBounds();
        illustHint.setOrigin(sf::Vector2f(ihb.position.x + ihb.size.x * 0.5f, ihb.position.y + ihb.size.y * 0.5f));
        illustHint.setPosition(sf::Vector2f(illustCx, illustCy));
        window.draw(illustHint);
    }

    // 4. 右侧：描述 + 分隔线 + 选项，加大与边框的余量
    const float rightLeft = panelLeft + leftW;
    const float contentPad = 38.f;
    const float textAreaW = rightW - contentPad * 2.f;
    const float descMaxH = std::min(180.f, panelH * 0.26f);
    float y = contentTop + contentPad;
    const sf::String effectiveDesc = eventData_.description.empty() ? sf::String(L"(无描述)") : sf::String(eventData_.description);
    draw_wrapped_text(window, fontForText(), effectiveDesc,
        bodySize, sf::Vector2f(rightLeft + contentPad, y), textAreaW, descMaxH, DESC_COLOR);
    y += descMaxH + 16.f;
    // 描述与选项之间的双线分隔
    sf::RectangleShape sep1(sf::Vector2f(textAreaW, 1.f));
    sep1.setPosition(sf::Vector2f(rightLeft + contentPad, y));
    sep1.setFillColor(sf::Color(90, 85, 95));
    window.draw(sep1);
    sf::RectangleShape sep2(sf::Vector2f(textAreaW, 1.f));
    sep2.setPosition(sf::Vector2f(rightLeft + contentPad, y + 2.f));
    sep2.setFillColor(sf::Color(55, 52, 65));
    window.draw(sep2);
    y += 22.f;  // 选项块整体下移，与左图下缘更对齐

    eventOptionRects_.clear();
    const float btnW = textAreaW;
    const float btnH = BUTTON_H * scale;
    const float tipW = BUTTON_TIP_W * scale;
    const float crystalSz = CRYSTAL_SZ * scale;
    const float optTextLeft = 14.f + crystalSz + 8.f;
    const float optMaxW = btnW - tipW - optTextLeft - 14.f;
    const float optionAreaBottom = panelTop + panelH - contentPad;  // 选项区下边界，超出不绘制、不响应
    for (size_t i = 0; i < eventData_.optionTexts.size(); ++i) {
        const float by = y + (btnH + BUTTON_GAP) * static_cast<float>(i);
        if (by + btnH > optionAreaBottom) break;  // 只绘制能放入面板的选项
        sf::FloatRect rect(sf::Vector2f(rightLeft + contentPad, by), sf::Vector2f(btnW, btnH));
        eventOptionRects_.push_back(rect);
        bool hover = rect.contains(mousePos_);
        bool pressed = (eventOptionPressedIndex_ == static_cast<int>(i));
        bool keyFocus = (selectedEventOption_ == static_cast<int>(i));
        sf::ConvexShape btn(5);
        btn.setPoint(0, sf::Vector2f(0, 0));
        btn.setPoint(1, sf::Vector2f(0, btnH));
        btn.setPoint(2, sf::Vector2f(btnW - tipW, btnH));
        btn.setPoint(3, sf::Vector2f(btnW, btnH * 0.5f));
        btn.setPoint(4, sf::Vector2f(btnW - tipW, 0));
        btn.setPosition(sf::Vector2f(rightLeft + contentPad, by));
        btn.setFillColor(pressed ? sf::Color(BUTTON_OUTLINE_STS.r, BUTTON_OUTLINE_STS.g, BUTTON_OUTLINE_STS.b, 200) : ((hover || keyFocus) ? BUTTON_HOVER_STS : BUTTON_BG_STS));
        btn.setOutlineThickness(1.5f);
        btn.setOutlineColor(BUTTON_OUTLINE_STS);
        window.draw(btn);
        sf::VertexArray bevel(sf::PrimitiveType::LineStrip, 3);
        bevel[0].position = rect.position + sf::Vector2f(0, 0);
        bevel[0].color = BUTTON_BEVEL;
        bevel[1].position = rect.position + sf::Vector2f(btnW - tipW, 0);
        bevel[1].color = BUTTON_BEVEL;
        bevel[2].position = rect.position + sf::Vector2f(btnW, btnH * 0.5f);
        bevel[2].color = sf::Color(BUTTON_BEVEL.r, BUTTON_BEVEL.g, BUTTON_BEVEL.b, 120);
        window.draw(bevel);
        const float cxX = rect.position.x + 14.f + crystalSz * 0.5f;
        const float cxY = rect.position.y + btnH * 0.5f;
        sf::ConvexShape crystal(4);
        crystal.setPoint(0, sf::Vector2f(0, -crystalSz * 0.5f));
        crystal.setPoint(1, sf::Vector2f(crystalSz * 0.5f, 0));
        crystal.setPoint(2, sf::Vector2f(0, crystalSz * 0.5f));
        crystal.setPoint(3, sf::Vector2f(-crystalSz * 0.5f, 0));
        crystal.setPosition(sf::Vector2f(cxX, cxY));
        crystal.setFillColor(CRYSTAL_COLOR);
        crystal.setOutlineThickness(0.5f);
        crystal.setOutlineColor(sf::Color(180, 160, 70));
        window.draw(crystal);
        const sf::String rawLabel = eventData_.optionTexts[i].empty()
            ? sf::String(L"[ 选项 ]")
            : (sf::String(L"[ ") + sf::String(eventData_.optionTexts[i]) + L" ]");
        sf::String label = truncate_to_width(fontForText(), rawLabel, bodySize, optMaxW);
        sf::Text opt(fontForText(), label, bodySize);
        opt.setFillColor(TEXT_COLOR);
        opt.setPosition(sf::Vector2f(rect.position.x + optTextLeft, rect.position.y + (btnH - static_cast<float>(bodySize)) * 0.5f));
        window.draw(opt);
    }
    if (!eventOptionRects_.empty()) {
        if (selectedEventOption_ < 0) selectedEventOption_ = 0;
        if (selectedEventOption_ >= static_cast<int>(eventOptionRects_.size())) selectedEventOption_ = static_cast<int>(eventOptionRects_.size()) - 1;
    }
}

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

void EventShopRestUI::drawRestScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float panelH = std::max(PANEL_H_MIN, 180.f + static_cast<float>(restData_.deckForUpgrade.size()) * (BUTTON_H * 0.8f + 4.f));
    drawPanel(window, cx, cy, PANEL_W, panelH);

    float y = cy - panelH * 0.5f + PAD;
    sf::Text title(fontForText(), sf::String(L"休息"), static_cast<unsigned>(TITLE_CHAR_SIZE));
    title.setFillColor(TEXT_COLOR);
    title.setPosition(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, y));
    window.draw(title);
    y += 40.f;

    const float btnW = PANEL_W - PAD * 2.f;
    restHealButton_ = sf::FloatRect(sf::Vector2f(cx - btnW * 0.5f, y), sf::Vector2f(btnW, BUTTON_H));
    bool healHover = restHealButton_.contains(mousePos_);
    sf::RectangleShape healBtn(restHealButton_.size);
    healBtn.setPosition(restHealButton_.position);
    healBtn.setFillColor(healHover ? BUTTON_HOVER : BUTTON_BG);
    healBtn.setOutlineThickness(1.f);
    healBtn.setOutlineColor(PANEL_OUTLINE);
    window.draw(healBtn);
    sf::Text healText(fontForText(), L"回血 (+" + std::to_wstring(restData_.healAmount) + L")", static_cast<unsigned>(BODY_CHAR_SIZE));
    healText.setFillColor(TEXT_COLOR);
    healText.setPosition(sf::Vector2f(restHealButton_.position.x + 12.f, restHealButton_.position.y + (BUTTON_H - BODY_CHAR_SIZE) * 0.5f));
    window.draw(healText);
    y += BUTTON_H + 20.f;

    sf::Text upgradeLabel(fontForText(), sf::String(L"升级一张牌"), static_cast<unsigned>(BODY_CHAR_SIZE));
    upgradeLabel.setFillColor(sf::Color(180, 180, 200));
    upgradeLabel.setPosition(sf::Vector2f(cx - PANEL_W * 0.5f + PAD, y));
    window.draw(upgradeLabel);
    y += 28.f;

    restUpgradeRects_.clear();
    for (size_t i = 0; i < restData_.deckForUpgrade.size(); ++i) {
        const float by = y + static_cast<float>(i) * (BUTTON_H * 0.8f + 4.f);
        sf::FloatRect rect(sf::Vector2f(cx - btnW * 0.5f, by), sf::Vector2f(btnW, BUTTON_H * 0.8f));
        restUpgradeRects_.push_back(rect);
        bool hover = rect.contains(mousePos_);
        sf::RectangleShape btn(rect.size);
        btn.setPosition(rect.position);
        btn.setFillColor(hover ? BUTTON_HOVER : BUTTON_BG);
        btn.setOutlineThickness(1.f);
        btn.setOutlineColor(PANEL_OUTLINE);
        window.draw(btn);
        sf::Text t(fontForText(), sf::String(restData_.deckForUpgrade[i].cardName), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        t.setFillColor(TEXT_COLOR);
        t.setPosition(sf::Vector2f(rect.position.x + 8.f, rect.position.y + (rect.size.y - (BODY_CHAR_SIZE - 2)) * 0.5f));
        window.draw(t);
    }
}

bool EventShopRestUI::handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos) {
    if (screen_ == EventShopRestScreen::None) return false;

    if (ev.is<sf::Event::MouseButtonReleased>()) {
        auto const* rel = ev.getIf<sf::Event::MouseButtonReleased>();
        if (rel && rel->button == sf::Mouse::Button::Left)
            eventOptionPressedIndex_ = -1;
        return false;
    }
    if (ev.is<sf::Event::MouseButtonPressed>()) {
        auto const* btn = ev.getIf<sf::Event::MouseButtonPressed>();
        if (!btn || btn->button != sf::Mouse::Button::Left) return false;

        if (screen_ == EventShopRestScreen::Event) {
            for (size_t i = 0; i < eventOptionRects_.size(); ++i) {
                if (eventOptionRects_[i].contains(mousePos)) {
                    eventOptionPressedIndex_ = static_cast<int>(i);
                    pendingEventOption_ = static_cast<int>(i);
                    return true;
                }
            }
        } else if (screen_ == EventShopRestScreen::Shop) {
            for (size_t i = 0; i < shopBuyRects_.size() && i < shopData_.forSale.size(); ++i) {
                if (shopBuyRects_[i].contains(mousePos)) {
                    pendingShopBuyCard_ = shopData_.forSale[i].id;
                    pendingShopBuy_ = true;
                    return true;
                }
            }
            for (size_t i = 0; i < shopRemoveRects_.size() && i < shopData_.deckForRemove.size(); ++i) {
                if (shopRemoveRects_[i].contains(mousePos)) {
                    pendingShopRemoveInstance_ = shopData_.deckForRemove[i].instanceId;
                    pendingShopRemove_ = true;
                    return true;
                }
            }
        } else if (screen_ == EventShopRestScreen::Rest) {
            if (restHealButton_.contains(mousePos)) {
                pendingRestHeal_ = true;
                return true;
            }
            for (size_t i = 0; i < restUpgradeRects_.size() && i < restData_.deckForUpgrade.size(); ++i) {
                if (restUpgradeRects_[i].contains(mousePos)) {
                    pendingRestUpgradeInstance_ = restData_.deckForUpgrade[i].instanceId;
                    pendingRestUpgrade_ = true;
                    return true;
                }
            }
        }
        return false;
    }

    if (ev.is<sf::Event::KeyPressed>() && screen_ == EventShopRestScreen::Event && !eventOptionRects_.empty()) {
        auto const* key = ev.getIf<sf::Event::KeyPressed>();
        if (!key) return false;
        const int n = static_cast<int>(eventOptionRects_.size());
        if (key->code == sf::Keyboard::Key::Up) {
            selectedEventOption_ = (selectedEventOption_ - 1 + n) % n;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Down) {
            selectedEventOption_ = (selectedEventOption_ + 1) % n;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
            pendingEventOption_ = selectedEventOption_;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Escape) {
            selectedEventOption_ = 0;
            return true;
        }
    }
    return false;
}

void EventShopRestUI::draw(sf::RenderWindow& window) {
    if (screen_ == EventShopRestScreen::None) return;
    if (eventBgLoaded_) {
        draw_texture_fit(window, eventBgTexture_, sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_))));
    }
    switch (screen_) {
    case EventShopRestScreen::Event:
        drawEventScreen(window);
        break;
    case EventShopRestScreen::Shop:
        drawShopScreen(window);
        break;
    case EventShopRestScreen::Rest:
        drawRestScreen(window);
        break;
    default:
        break;
    }
}

bool EventShopRestUI::pollEventOption(int& outIndex) {
    if (pendingEventOption_ < 0) return false;
    outIndex = pendingEventOption_;
    pendingEventOption_ = -1;
    return true;
}

bool EventShopRestUI::pollShopBuyCard(CardId& outCardId) {
    if (!pendingShopBuy_) return false;
    outCardId = pendingShopBuyCard_;
    pendingShopBuy_ = false;
    return true;
}

bool EventShopRestUI::pollShopRemoveCard(InstanceId& outInstanceId) {
    if (!pendingShopRemove_) return false;
    outInstanceId = pendingShopRemoveInstance_;
    pendingShopRemove_ = false;
    return true;
}

bool EventShopRestUI::pollRestHeal() {
    if (!pendingRestHeal_) return false;
    pendingRestHeal_ = false;
    return true;
}

bool EventShopRestUI::pollRestUpgradeCard(InstanceId& outInstanceId) {
    if (!pendingRestUpgrade_) return false;
    outInstanceId = pendingRestUpgradeInstance_;
    pendingRestUpgrade_ = false;
    return true;
}

} // namespace tce
