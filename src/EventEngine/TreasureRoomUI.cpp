#include "EventEngine/TreasureRoomUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"

#include <SFML/Graphics.hpp>
#include <cmath>

namespace tce {
using namespace esr_detail;

namespace {

bool load_pair_for_kind(TreasureChestKind k, sf::Texture& closeTex, sf::Texture& openTex) {
    std::vector<std::string> closePaths;
    std::vector<std::string> openPaths;
    switch (k) {
    case TreasureChestKind::Small:
        closePaths = { "assets/ui/treasure/small_close.png", "./assets/ui/treasure/small_close.png" };
        openPaths  = { "assets/ui/treasure/small_open.png", "./assets/ui/treasure/small_open.png" };
        break;
    case TreasureChestKind::Medium:
        closePaths = { "assets/ui/treasure/middle_close.png", "./assets/ui/treasure/middle_close.png" };
        // 资源文件名拼写为 midlle_open
        openPaths  = { "assets/ui/treasure/midlle_open.png", "./assets/ui/treasure/midlle_open.png" };
        break;
    case TreasureChestKind::Large:
    default:
        closePaths = { "assets/ui/treasure/big_close.png", "./assets/ui/treasure/big_close.png" };
        openPaths  = { "assets/ui/treasure/big_open.png", "./assets/ui/treasure/big_open.png" };
        break;
    }
    const bool c = try_load_texture(closeTex, closePaths);
    const bool o = try_load_texture(openTex, openPaths);
    return c && o;
}

} // namespace

TreasureRoomUI::TreasureRoomUI(unsigned width, unsigned height)
    : width_(width)
    , height_(height) {}

bool TreasureRoomUI::loadFont(const std::string& path) {
    fontLoaded_ = font_.openFromFile(path);
    return fontLoaded_;
}

bool TreasureRoomUI::loadChineseFont(const std::string& path) {
    fontChineseLoaded_ = fontChinese_.openFromFile(path);
    return fontChineseLoaded_;
}

void TreasureRoomUI::setup(const TreasureRoomDisplayData& data) {
    data_ = data;
    opened_ = false;
    pendingLeave_ = false;
    relicLoaded_ = false;
    closeLoaded_ = false;
    openLoaded_ = false;

    closeLoaded_ = openLoaded_ = load_pair_for_kind(data_.chest_kind, closeTex_, openTex_);
    if (!data_.relic_icon_path.empty()) {
        relicLoaded_ = try_load_texture(relicTex_, { data_.relic_icon_path, std::string("./") + data_.relic_icon_path });
    }
    layoutContinueButton();
}

void TreasureRoomUI::layoutContinueButton() {
    const float w = 200.f;
    const float h = 48.f;
    continueBtnRect_ = sf::FloatRect(
        sf::Vector2f(static_cast<float>(width_) * 0.5f - w * 0.5f, static_cast<float>(height_) - 80.f),
        sf::Vector2f(w, h));
}

bool TreasureRoomUI::hitContinue(const sf::Vector2f& p) const { return opened_ && continueBtnRect_.contains(p); }

bool TreasureRoomUI::handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos) {
    if (ev.is<sf::Event::MouseButtonPressed>()) {
        const auto* btn = ev.getIf<sf::Event::MouseButtonPressed>();
        if (!btn || btn->button != sf::Mouse::Button::Left) return false;
        if (!opened_) {
            opened_ = true;
            return true;
        }
        if (hitContinue(mousePos)) {
            pendingLeave_ = true;
            return true;
        }
        return false;
    }

    if (ev.is<sf::Event::KeyPressed>()) {
        const auto* key = ev.getIf<sf::Event::KeyPressed>();
        if (!key) return false;
        if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
            if (!opened_) {
                opened_ = true;
                return true;
            }
            pendingLeave_ = true;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Escape) {
            if (opened_) {
                pendingLeave_ = true;
                return true;
            }
        }
    }
    return false;
}

void TreasureRoomUI::setMousePosition(sf::Vector2f pos) { mousePos_ = pos; }

bool TreasureRoomUI::pollLeave() {
    if (!pendingLeave_) return false;
    pendingLeave_ = false;
    return true;
}

void TreasureRoomUI::draw(sf::RenderWindow& window) {
    const sf::FloatRect full(
        sf::Vector2f(0.f, 0.f),
        sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_)));

    const sf::Texture* bg = nullptr;
    if (!opened_ && closeLoaded_)
        bg = &closeTex_;
    else if (opened_ && openLoaded_)
        bg = &openTex_;
    else if (closeLoaded_)
        bg = &closeTex_;
    else if (openLoaded_)
        bg = &openTex_;

    if (bg)
        draw_texture_fit(window, *bg, full);
    else {
        sf::RectangleShape fill(full.size);
        fill.setPosition(full.position);
        fill.setFillColor(sf::Color(8, 6, 12));
        window.draw(fill);
    }

    const unsigned hintSize = static_cast<unsigned>(std::round(22.f * std::min(1.f, static_cast<float>(width_) / 1280.f)));
    const unsigned titleSize = static_cast<unsigned>(std::round(26.f * std::min(1.f, static_cast<float>(width_) / 1280.f)));

    if (!data_.title_line.empty() && (fontLoaded_ || fontChineseLoaded_)) {
        sf::Text title(fontForText(), sf::String(data_.title_line), titleSize);
        title.setFillColor(sf::Color(245, 238, 220));
        title.setOutlineColor(sf::Color(0, 0, 0, 180));
        title.setOutlineThickness(2.f);
        const auto lb = title.getLocalBounds();
        title.setPosition(sf::Vector2f(static_cast<float>(width_) * 0.5f - lb.size.x * 0.5f - lb.position.x, 36.f));
        window.draw(title);
    }

    if (!opened_) {
        if (fontLoaded_ || fontChineseLoaded_) {
            sf::Text hint(fontForText(), sf::String(L"点击开启宝箱"), hintSize);
            hint.setFillColor(sf::Color(230, 220, 200, 230));
            hint.setOutlineColor(sf::Color(0, 0, 0, 160));
            hint.setOutlineThickness(1.5f);
            const auto lb = hint.getLocalBounds();
            hint.setPosition(
                sf::Vector2f(static_cast<float>(width_) * 0.5f - lb.size.x * 0.5f - lb.position.x,
                    static_cast<float>(height_) - 120.f));
            window.draw(hint);
        }
        return;
    }

    // 开启态：底部半透明信息条 + 奖励文案 + 继续
    const float barH = std::min(220.f, static_cast<float>(height_) * 0.28f);
    sf::RectangleShape bar(sf::Vector2f(static_cast<float>(width_) - 48.f, barH));
    bar.setPosition(sf::Vector2f(24.f, static_cast<float>(height_) - barH - 24.f));
    bar.setFillColor(sf::Color(12, 10, 18, 210));
    bar.setOutlineThickness(2.f);
    bar.setOutlineColor(sf::Color(55, 50, 65));
    window.draw(bar);

    float textLeft = 48.f;
    const float barTop = static_cast<float>(height_) - barH - 24.f;
    const unsigned bodySize = static_cast<unsigned>(std::round(20.f * std::min(1.f, static_cast<float>(width_) / 1280.f)));

    if (relicLoaded_) {
        sf::Sprite sp(relicTex_);
        const sf::Vector2u tsz = relicTex_.getSize();
        const float iconTarget = 72.f;
        const float scale = tsz.x > 0 ? iconTarget / static_cast<float>(tsz.x) : 1.f;
        sp.setScale(sf::Vector2f(scale, scale));
        sp.setPosition(sf::Vector2f(textLeft, barTop + 24.f));
        window.draw(sp);
        textLeft += iconTarget + 16.f;
    }

    float ty = barTop + 28.f;
    if (fontLoaded_ || fontChineseLoaded_) {
        auto drawLine = [&](const sf::String& s, const sf::Color& col) {
            if (s.isEmpty()) return;
            sf::Text line(fontForText(), s, bodySize);
            line.setFillColor(col);
            line.setPosition(sf::Vector2f(textLeft, ty));
            window.draw(line);
            ty += line.getLocalBounds().size.y + 10.f;
        };

        if (data_.has_gold)
            drawLine(sf::String(L"金币 +" + std::to_wstring(data_.gold_amount)), sf::Color(240, 210, 120));
        if (data_.has_relic && !data_.relic_line.empty())
            drawLine(sf::String(data_.relic_line), TEXT_COLOR);
        else if (!data_.has_relic)
            drawLine(sf::String(L"遗物池已空"), DESC_COLOR);
    }

    layoutContinueButton();
    const bool hover = continueBtnRect_.contains(mousePos_);
    sf::RectangleShape btn(continueBtnRect_.size);
    btn.setPosition(continueBtnRect_.position);
    btn.setFillColor(hover ? BUTTON_HOVER_STS : BUTTON_BG_STS);
    btn.setOutlineThickness(2.f);
    btn.setOutlineColor(BUTTON_OUTLINE_STS);
    window.draw(btn);

    if (fontLoaded_ || fontChineseLoaded_) {
        sf::Text bt(fontForText(), sf::String(L"继续"), hintSize);
        bt.setFillColor(TEXT_COLOR);
        const auto lb = bt.getLocalBounds();
        bt.setPosition(sf::Vector2f(continueBtnRect_.position.x + continueBtnRect_.size.x * 0.5f - lb.size.x * 0.5f - lb.position.x,
            continueBtnRect_.position.y + continueBtnRect_.size.y * 0.5f - lb.size.y * 0.5f - lb.position.y));
        window.draw(bt);
    }
}

} // namespace tce
