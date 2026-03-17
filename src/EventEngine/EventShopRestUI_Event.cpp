/**
 * 事件界面绘制（从 EventShopRestUI 拆出）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include <SFML/Graphics.hpp>
#include <cmath>

namespace tce {
using namespace esr_detail;

void EventShopRestUI::drawEventScreen(sf::RenderWindow& window) {
    const float cx = width_ * 0.5f;
    const float cy = height_ * 0.5f;
    const float panelW = std::max(720.f, static_cast<float>(width_) * 0.68f);
    const float panelH = std::max(440.f, static_cast<float>(height_) * 0.71f);
    const float panelLeft = cx - panelW * 0.5f;
    const float panelTop = cy - panelH * 0.5f;
    const float leftW = panelW * LEFT_ILLUST_RATIO;
    const float rightW = panelW - leftW;
    const float bannerBoxH = BANNER_H + BANNER_EXTRA_H;
    constexpr float BANNER_INNER_OVERHANG = 12.f;
    const float scale = panelW / PANEL_W;
    const unsigned titleSize = static_cast<unsigned>(std::round(TITLE_CHAR_SIZE * scale));
    const unsigned bodySize = static_cast<unsigned>(std::round(BODY_CHAR_SIZE * scale));

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

    const float bannerY = panelTop - BANNER_INNER_OVERHANG;
    const float contentTop = bannerY + bannerBoxH + 2.f;
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

    const float illustMarginV = 24.f;
    const float illustMarginH = 20.f;
    const float illustLeft = panelLeft + illustMarginH;
    const float illustW = leftW - illustMarginH * 2.f;
    const float illustH = panelH - (contentTop - panelTop) - illustMarginV * 2.f;
    const float illustTop = contentTop + illustMarginV;
    const float illustCx = illustLeft + illustW * 0.5f;
    const float illustCy = illustTop + illustH * 0.5f;
    if (eventIllustLoaded_ && eventIllustTexture_.getSize().x > 0) {
        sf::Sprite sprite(eventIllustTexture_);
        const sf::Vector2u texSize = eventIllustTexture_.getSize();
        const float scaleX = illustW / static_cast<float>(texSize.x);
        const float scaleY = illustH / static_cast<float>(texSize.y);
        const float s = std::min(scaleX, scaleY);
        sprite.setScale(sf::Vector2f(s, s));
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

    const float rightLeft = panelLeft + leftW;
    const float contentPad = 38.f;
    const float textAreaW = rightW - contentPad * 2.f;
    const float descMaxH = std::min(180.f, panelH * 0.26f);
    float y = contentTop + contentPad;
    const sf::String effectiveDesc = eventData_.description.empty() ? sf::String(L"(无描述)") : sf::String(eventData_.description);
    draw_wrapped_text(window, fontForText(), effectiveDesc,
        bodySize, sf::Vector2f(rightLeft + contentPad, y), textAreaW, descMaxH, DESC_COLOR);
    y += descMaxH + 16.f;
    sf::RectangleShape sep1(sf::Vector2f(textAreaW, 1.f));
    sep1.setPosition(sf::Vector2f(rightLeft + contentPad, y));
    sep1.setFillColor(sf::Color(90, 85, 95));
    window.draw(sep1);
    sf::RectangleShape sep2(sf::Vector2f(textAreaW, 1.f));
    sep2.setPosition(sf::Vector2f(rightLeft + contentPad, y + 2.f));
    sep2.setFillColor(sf::Color(55, 52, 65));
    window.draw(sep2);
    y += 22.f;

    eventOptionRects_.clear();
    const float btnW = textAreaW;
    const float btnH = BUTTON_H * scale;
    const float tipW = BUTTON_TIP_W * scale;
    const float crystalSz = CRYSTAL_SZ * scale;
    const float optTextLeft = 14.f + crystalSz + 8.f;
    const float optMaxW = btnW - tipW - optTextLeft - 14.f;
    const float optionAreaBottom = panelTop + panelH - contentPad;
    for (size_t i = 0; i < eventData_.optionTexts.size(); ++i) {
        const float by = y + (btnH + BUTTON_GAP) * static_cast<float>(i);
        if (by + btnH > optionAreaBottom) break;
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

} // namespace tce
