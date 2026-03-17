/**
 * 事件 / 商店 / 休息 UI 实现
 */
#include "EventEngine/EventShopRestUI.hpp"
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
constexpr float BANNER_H = 48.f;
constexpr float BANNER_EXTRA_W = 16.f;   // 卷轴条比面板左右多出的宽度
constexpr float LEFT_ILLUST_RATIO = 0.42f;
constexpr float TITLE_CHAR_SIZE = 26.f;
constexpr float BODY_CHAR_SIZE = 20.f;
constexpr float BUTTON_H = 40.f;
constexpr float BUTTON_GAP = 10.f;
constexpr float BUTTON_TIP_W = 24.f;      // 选项按钮右侧尖角宽度（StS 箭头形）
constexpr float CRYSTAL_SZ = 14.f;       // 选项左侧水晶图标尺寸
constexpr float PAD = 20.f;
const sf::Color PANEL_BG(22, 20, 28);
const sf::Color PANEL_OUTLINE(55, 52, 65);
const sf::Color PANEL_INNER(45, 42, 55);   // 内边框
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
    : width_(width), height_(height) {}

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
    pendingShopBuy_ = false;
    pendingShopRemove_ = false;
    pendingRestHeal_ = false;
    pendingRestUpgrade_ = false;
}

void EventShopRestUI::setEventData(const EventDisplayData& data) {
    eventData_ = data;
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
    eventData_.imagePath = imagePath;
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

void EventShopRestUI::drawEventScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float panelLeft = cx - PANEL_W * 0.5f;
    const float panelTop = cy - PANEL_H * 0.5f;
    const float leftW = PANEL_W * LEFT_ILLUST_RATIO;
    const float rightW = PANEL_W - leftW;
    const float contentTop = panelTop + BANNER_H;

    // 1. 主面板（深色 + 内边框，贴近 StS）
    drawPanel(window, cx, cy, PANEL_W, PANEL_H);
    sf::RectangleShape innerBorder(sf::Vector2f(PANEL_W - 6.f, PANEL_H - 6.f));
    innerBorder.setOrigin(sf::Vector2f(innerBorder.getSize().x * 0.5f, innerBorder.getSize().y * 0.5f));
    innerBorder.setPosition(sf::Vector2f(cx, cy));
    innerBorder.setFillColor(sf::Color::Transparent);
    innerBorder.setOutlineThickness(1.f);
    innerBorder.setOutlineColor(PANEL_INNER);
    window.draw(innerBorder);

    // 2. 顶部卷轴条（羊皮纸色，比面板略宽、略突出）
    const float bannerY = panelTop - 5.f;
    const float bannerW = PANEL_W + BANNER_EXTRA_W * 2.f;
    const float bannerBoxH = BANNER_H + 8.f;
    sf::RectangleShape banner(sf::Vector2f(bannerW, bannerBoxH));
    banner.setOrigin(sf::Vector2f(bannerW * 0.5f, 0.f));
    banner.setPosition(sf::Vector2f(cx, bannerY));
    banner.setFillColor(BANNER_BG);
    banner.setOutlineThickness(2.f);
    banner.setOutlineColor(BANNER_OUTLINE);
    window.draw(banner);

    const float bannerMaxTitleW = bannerW - 28.f;
    sf::String titleStr = truncate_to_width(fontForText(), sf::String(eventData_.title), static_cast<unsigned>(TITLE_CHAR_SIZE), bannerMaxTitleW);
    sf::Text title(fontForText(), titleStr, static_cast<unsigned>(TITLE_CHAR_SIZE));
    title.setFillColor(BANNER_TITLE_COLOR);
    const sf::FloatRect tb = title.getLocalBounds();
    title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, 0.f));
    title.setPosition(sf::Vector2f(cx, bannerY + (bannerBoxH - tb.size.y) * 0.5f));
    window.draw(title);

    // 3. 左侧插图区：有图则贴图，无图则占位
    const float illustW = leftW - 2.f;
    const float illustH = PANEL_H - BANNER_H - 4.f;
    sf::RectangleShape illust(sf::Vector2f(illustW, illustH));
    illust.setPosition(sf::Vector2f(panelLeft + 2.f, contentTop + 2.f));
    illust.setFillColor(ILLUST_BG);
    illust.setOutlineThickness(1.f);
    illust.setOutlineColor(PANEL_OUTLINE);
    window.draw(illust);
    if (eventIllustLoaded_ && eventIllustTexture_.getSize().x > 0) {
        sf::Sprite sprite(eventIllustTexture_);
        const sf::Vector2u texSize = eventIllustTexture_.getSize();
        const float scaleX = illustW / static_cast<float>(texSize.x);
        const float scaleY = illustH / static_cast<float>(texSize.y);
        const float scale = std::min(scaleX, scaleY);
        sprite.setScale(sf::Vector2f(scale, scale));
        sprite.setOrigin(sf::Vector2f(texSize.x * 0.5f, texSize.y * 0.5f));
        sprite.setPosition(sf::Vector2f(panelLeft + leftW * 0.5f, contentTop + illustH * 0.5f));
        window.draw(sprite);
    } else {
        sf::Text illustHint(fontForText(), sf::String(L"[ 插图 ]"), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        illustHint.setFillColor(sf::Color(80, 78, 90));
        const sf::FloatRect ihb = illustHint.getLocalBounds();
        illustHint.setOrigin(sf::Vector2f(ihb.position.x + ihb.size.x * 0.5f, ihb.position.y + ihb.size.y * 0.5f));
        illustHint.setPosition(sf::Vector2f(panelLeft + leftW * 0.5f, contentTop + illustH * 0.5f));
        window.draw(illustHint);
    }

    // 4. 右侧：描述（自动换行不超出边框）+ 选项按钮（过长截断）
    const float rightLeft = panelLeft + leftW;
    const float textAreaW = rightW - PAD * 2.f;
    const float descMaxH = 88.f;
    float y = contentTop + PAD;
    draw_wrapped_text(window, fontForText(), sf::String(eventData_.description),
        static_cast<unsigned>(BODY_CHAR_SIZE), sf::Vector2f(rightLeft + PAD, y), textAreaW, descMaxH, DESC_COLOR);
    y += descMaxH + 8.f;

    eventOptionRects_.clear();
    const float btnW = textAreaW;
    const float optTextLeft = 14.f + CRYSTAL_SZ + 8.f;   // 水晶图标 + 间距后开始排字
    const float optMaxW = btnW - BUTTON_TIP_W - optTextLeft - 14.f;
    for (size_t i = 0; i < eventData_.optionTexts.size(); ++i) {
        const float by = y + (BUTTON_H + BUTTON_GAP) * static_cast<float>(i);
        sf::FloatRect rect(sf::Vector2f(rightLeft + PAD, by), sf::Vector2f(btnW, BUTTON_H));
        eventOptionRects_.push_back(rect);
        bool hover = rect.contains(mousePos_);
        // StS 风格：右侧尖角按钮（五边形）
        sf::ConvexShape btn(5);
        btn.setPoint(0, sf::Vector2f(0, 0));
        btn.setPoint(1, sf::Vector2f(0, BUTTON_H));
        btn.setPoint(2, sf::Vector2f(btnW - BUTTON_TIP_W, BUTTON_H));
        btn.setPoint(3, sf::Vector2f(btnW, BUTTON_H * 0.5f));
        btn.setPoint(4, sf::Vector2f(btnW - BUTTON_TIP_W, 0));
        btn.setPosition(rect.position);
        btn.setFillColor(hover ? BUTTON_HOVER_STS : BUTTON_BG_STS);
        btn.setOutlineThickness(1.5f);
        btn.setOutlineColor(BUTTON_OUTLINE_STS);
        window.draw(btn);
        // 高光边（左上细线，增强立体感）
        sf::VertexArray bevel(sf::PrimitiveType::LineStrip, 3);
        bevel[0].position = rect.position + sf::Vector2f(0, 0);
        bevel[0].color = BUTTON_BEVEL;
        bevel[1].position = rect.position + sf::Vector2f(btnW - BUTTON_TIP_W, 0);
        bevel[1].color = BUTTON_BEVEL;
        bevel[2].position = rect.position + sf::Vector2f(btnW, BUTTON_H * 0.5f);
        bevel[2].color = sf::Color(BUTTON_BEVEL.r, BUTTON_BEVEL.g, BUTTON_BEVEL.b, 120);
        window.draw(bevel);
        // 选项前小水晶图标（菱形，无贴图时用形状代替）
        const float cxX = rect.position.x + 14.f + CRYSTAL_SZ * 0.5f;
        const float cxY = rect.position.y + BUTTON_H * 0.5f;
        sf::ConvexShape crystal(4);
        crystal.setPoint(0, sf::Vector2f(0, -CRYSTAL_SZ * 0.5f));
        crystal.setPoint(1, sf::Vector2f(CRYSTAL_SZ * 0.5f, 0));
        crystal.setPoint(2, sf::Vector2f(0, CRYSTAL_SZ * 0.5f));
        crystal.setPoint(3, sf::Vector2f(-CRYSTAL_SZ * 0.5f, 0));
        crystal.setPosition(sf::Vector2f(cxX, cxY));
        crystal.setFillColor(CRYSTAL_COLOR);
        crystal.setOutlineThickness(0.5f);
        crystal.setOutlineColor(sf::Color(180, 160, 70));
        window.draw(crystal);
        sf::String label = truncate_to_width(fontForText(), sf::String(L"[ " + eventData_.optionTexts[i] + L" ]"),
            static_cast<unsigned>(BODY_CHAR_SIZE), optMaxW);
        sf::Text opt(fontForText(), label, static_cast<unsigned>(BODY_CHAR_SIZE));
        opt.setFillColor(TEXT_COLOR);
        opt.setPosition(sf::Vector2f(rect.position.x + optTextLeft, rect.position.y + (BUTTON_H - BODY_CHAR_SIZE) * 0.5f));
        window.draw(opt);
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
    if (!ev.is<sf::Event::MouseButtonPressed>()) return false;
    auto const* btn = ev.getIf<sf::Event::MouseButtonPressed>();
    if (!btn || btn->button != sf::Mouse::Button::Left) return false;

    if (screen_ == EventShopRestScreen::Event) {
        for (size_t i = 0; i < eventOptionRects_.size(); ++i) {
            if (eventOptionRects_[i].contains(mousePos)) {
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

void EventShopRestUI::draw(sf::RenderWindow& window) {
    if (screen_ == EventShopRestScreen::None) return;
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
