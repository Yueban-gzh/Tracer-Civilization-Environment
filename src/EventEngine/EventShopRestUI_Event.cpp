/**
 * 事件界面绘制（从 EventShopRestUI 拆出）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include "Common/ImagePath.hpp"
#include "DataLayer/DataLayer.h"
#include "UI/CardVisual.hpp"
#include <SFML/Graphics.hpp>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace tce {
using namespace esr_detail;

namespace {

std::wstring relic_name_w(const std::string& id) {
    static const std::unordered_map<std::string, std::wstring> k = {
        {"burning_blood", L"燃烧之血"},       {"marble_bag", L"弹珠袋"},
        {"small_blood_vial", L"小血瓶"},       {"copper_scales", L"铜制鳞片"},
        {"centennial_puzzle", L"百年积木"},   {"clockwork_boots", L"发条靴"},
        {"happy_flower", L"开心小花"},         {"lantern", L"灯笼"},
        {"smooth_stone", L"意外光滑的石头"},   {"orichalcum", L"奥利哈钢"},
        {"red_skull", L"红头骨"},             {"snake_skull", L"异蛇头骨"},
        {"strawberry", L"草莓"},               {"potion_belt", L"灵液腰带"},
        {"vajra", L"金刚杵"},                  {"nunchaku", L"双截棍"},
        {"ceramic_fish", L"陶瓷小鱼"},         {"hand_drum", L"手摇鼓"},
        {"pen_nib", L"钢笔尖"},               {"toy_ornithopter", L"玩具扑翼飞机"},
        {"preparation_pack", L"准备背包"},     {"anchor", L"锚"},
        {"art_of_war", L"孙子兵法"},           {"relic_strength_plus", L"力量遗物"},
        {"akabeko", L"赤牛"},                 {"data_disk", L"数据磁盘"},
    };
    const auto it = k.find(id);
    if (it != k.end()) return it->second;
    return utf8_to_wstring(id);
}

std::wstring potion_name_w(const std::string& id) {
    static const std::unordered_map<std::string, std::wstring> k = {
        {"block_potion", L"砌墙灵液"},         {"strength_potion", L"蛮力灵液"},
        {"poison_potion", L"淬毒灵液"},       {"attack_potion", L"攻击灵液"},
        {"dexterity_potion", L"敏捷灵液"},   {"energy_potion", L"能量灵液"},
        {"blood_potion", L"鲜血灵液"},       {"fire_potion", L"火焰灵液"},
        {"weak_potion", L"虚弱灵液"},        {"speed_potion", L"疾速灵液"},
    };
    const auto it = k.find(id);
    if (it != k.end()) return it->second;
    return utf8_to_wstring(id);
}

/** 事件结果页：卡牌下方遗物条（柔光图标 + 类型 + 名称，无矩形描边） */
void draw_relic_combo_strip(sf::RenderWindow& window, const sf::Font& font,
    float illustLeft, float illustW, float rowY, float rowH,
    const std::string& relicId, const sf::Texture* relicTex, unsigned bodySize) {
    sf::RectangleShape hair(sf::Vector2f(illustW - 18.f, 1.f));
    hair.setPosition(sf::Vector2f(illustLeft + 9.f, rowY));
    hair.setFillColor(sf::Color(210, 175, 110, 85));
    window.draw(hair);

    const float iconCy = rowY + rowH * 0.5f;
    const float iconCx = illustLeft + 46.f;
    const float iconR = std::min(26.f, rowH * 0.34f);
    const float maxHaloR = std::max(8.f, rowH * 0.5f - 2.f);
    const float haloR = std::min(iconR + 14.f, maxHaloR);

    sf::CircleShape halo(haloR);
    halo.setOrigin(sf::Vector2f(halo.getRadius(), halo.getRadius()));
    halo.setPosition(sf::Vector2f(iconCx, iconCy));
    halo.setFillColor(sf::Color(120, 90, 55, 28));
    window.draw(halo);

    const float padR = std::max(4.f, std::min(iconR + 5.f, maxHaloR - 4.f));
    sf::CircleShape pad(padR);
    pad.setOrigin(sf::Vector2f(pad.getRadius(), pad.getRadius()));
    pad.setPosition(sf::Vector2f(iconCx, iconCy));
    pad.setFillColor(sf::Color(38, 33, 30, 210));
    window.draw(pad);

    if (relicTex && relicTex->getSize().x > 0 && relicTex->getSize().y > 0) {
        sf::Sprite rs(*relicTex);
        const sf::Vector2u ts = relicTex->getSize();
        const float target = iconR * 2.15f;
        const float s = std::min(target / static_cast<float>(ts.x), target / static_cast<float>(ts.y));
        rs.setScale(sf::Vector2f(s, s));
        rs.setOrigin(sf::Vector2f(ts.x * 0.5f, ts.y * 0.5f));
        rs.setPosition(sf::Vector2f(iconCx, iconCy));
        rs.setColor(sf::Color(255, 252, 245, 255));
        window.draw(rs);
    }

    const float textLeft = illustLeft + 88.f;
    const float nameMaxW = std::max(80.f, illustW - 96.f);
    const std::wstring nameW = relic_name_w(relicId);
    const unsigned catSize = static_cast<unsigned>(std::max(11u, bodySize - 5u));
    const unsigned nameSize = static_cast<unsigned>(std::max(13u, bodySize - 1u));

    sf::Text cat(font, sf::String(L"遗物"), catSize);
    cat.setFillColor(sf::Color(185, 165, 125));
    cat.setLetterSpacing(0.6f);
    cat.setPosition(sf::Vector2f(textLeft, rowY + 10.f));
    window.draw(cat);

    sf::String nameStr = truncate_to_width(font, sf::String(nameW), nameSize, nameMaxW);
    sf::Text nameText(font, nameStr, nameSize);
    nameText.setFillColor(sf::Color(255, 238, 210));
    nameText.setPosition(sf::Vector2f(textLeft, rowY + 10.f + catSize * 1.05f));
    window.draw(nameText);
}

/** 仅遗物/灵液占左栏时的居中展示 */
void draw_event_item_solo(sf::RenderWindow& window, const sf::Font& font,
    float cx, float topY, float regionH, float regionW,
    const std::string& itemId, const sf::Texture* tex, unsigned bodySize, bool isRelic) {
    if (!tex || tex->getSize().x == 0 || tex->getSize().y == 0) return;

    const float iconMax = std::min(regionW * 0.42f, regionH * 0.5f);
    const float iconCy = topY + regionH * 0.38f;
    const sf::Vector2u tsu = tex->getSize();
    const float s = std::min(iconMax / static_cast<float>(tsu.x), iconMax / static_cast<float>(tsu.y));

    const float maxHaloR = std::max(8.f, regionH * 0.22f);
    const float haloR = std::min(iconMax * 0.55f, maxHaloR);
    sf::CircleShape halo(haloR);
    halo.setOrigin(sf::Vector2f(halo.getRadius(), halo.getRadius()));
    halo.setPosition(sf::Vector2f(cx, iconCy));
    halo.setFillColor(sf::Color(110, 85, 55, 32));
    window.draw(halo);

    sf::CircleShape pad(std::max(4.f, std::min(iconMax * 0.48f, maxHaloR - 3.f)));
    pad.setOrigin(sf::Vector2f(pad.getRadius(), pad.getRadius()));
    pad.setPosition(sf::Vector2f(cx, iconCy));
    pad.setFillColor(sf::Color(38, 34, 30, 200));
    window.draw(pad);

    sf::Sprite sp(*tex);
    sp.setScale(sf::Vector2f(s, s));
    sp.setOrigin(sf::Vector2f(tsu.x * 0.5f, tsu.y * 0.5f));
    sp.setPosition(sf::Vector2f(cx, iconCy));
    sp.setColor(sf::Color(255, 252, 245, 255));
    window.draw(sp);

    const std::wstring nameW = isRelic ? relic_name_w(itemId) : potion_name_w(itemId);
    const unsigned catSize = static_cast<unsigned>(std::max(11u, bodySize - 4u));
    const unsigned nameSize = static_cast<unsigned>(std::max(14u, bodySize + 1u));
    const sf::String catStr = isRelic ? sf::String(L"遗物") : sf::String(L"灵液");

    sf::Text cat(font, catStr, catSize);
    cat.setFillColor(sf::Color(190, 175, 140));
    cat.setLetterSpacing(0.5f);
    const sf::FloatRect cb = cat.getLocalBounds();
    cat.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
    cat.setPosition(sf::Vector2f(cx, iconCy + iconMax * 0.55f));
    window.draw(cat);

    sf::String nameStr = truncate_to_width(font, sf::String(nameW), nameSize, regionW - 24.f);
    sf::Text nm(font, nameStr, nameSize);
    nm.setFillColor(sf::Color(255, 235, 200));
    const sf::FloatRect nb = nm.getLocalBounds();
    nm.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
    nm.setPosition(sf::Vector2f(cx, iconCy + iconMax * 0.55f + catSize + 6.f));
    window.draw(nm);
}

} // namespace

static std::vector<sf::String> wrap_lines_no_ellipsis(const sf::Font& font,
    const sf::String& text,
    unsigned charSize,
    float maxWidth,
    int maxLines) {
    std::vector<sf::String> lines;
    if (maxLines <= 0 || text.isEmpty()) return lines;
    if (maxWidth <= 1.f) {
        lines.push_back(text);
        return lines;
    }

    sf::Text measure(font, sf::String(L""), charSize);
    sf::String current;

    for (std::size_t i = 0; i < text.getSize(); ++i) {
        const char32_t ch = text[i];
        if (ch == U'\r') continue;
        if (ch == U'\n') {
            if (!current.isEmpty()) {
                lines.push_back(current);
                current.clear();
            }
            if (static_cast<int>(lines.size()) >= maxLines) break;
            continue;
        }

        sf::String candidate = current;
        candidate += ch;
        measure.setString(candidate);

        if (!current.isEmpty() && measure.getLocalBounds().size.x > maxWidth) {
            if (!current.isEmpty()) {
                lines.push_back(current);
                current.clear();
            }
            if (static_cast<int>(lines.size()) >= maxLines) break;
            current += ch;  // 新行先放入当前字符
        } else {
            current = std::move(candidate);
        }
    }

    if (!current.isEmpty() && static_cast<int>(lines.size()) < maxLines)
        lines.push_back(current);
    return lines;
}

void EventShopRestUI::drawEventScreen(sf::RenderWindow& window) {
    constexpr float BATTLE_CARD_ASPECT_H_OVER_W = 300.f / 206.f; // 与 BattleUI 保持一致
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
    static sf::Clock sClock;
    const float t = sClock.getElapsedTime().asSeconds();

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
        draw_texture_fit_crop_bottom(window, eventPanelFrameTexture_,
            sf::FloatRect(sf::Vector2f(frameLeft, frameTop), sf::Vector2f(frameWidth, frameHeight)),
            event_panel_frame_bottom_crop_px(eventPanelFrameTexture_));
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
    const bool illustIsCardsRelic = (eventData_.imagePath.rfind("__cards_relic:", 0) == 0);
    const bool illustIsCard = illustIsCardsRelic ||
                              (eventData_.imagePath.rfind("__cardid:", 0) == 0) ||
                              (eventData_.imagePath.rfind("__cards:", 0) == 0);
    const bool illustIsRelic = (eventData_.imagePath.rfind("__relic:", 0) == 0);
    const bool illustIsPotion = (eventData_.imagePath.rfind("__potion:", 0) == 0);
    auto resolveOfferIconTexture = [](const std::string& kind, const std::string& id) -> const sf::Texture* {
        if (id.empty()) return nullptr;
        static std::unordered_map<std::string, sf::Texture> cache;
        static std::unordered_map<std::string, bool> loaded;
        const std::string key = kind + ":" + id;
        const auto itLoaded = loaded.find(key);
        if (itLoaded != loaded.end()) {
            if (!itLoaded->second) return nullptr;
            auto it = cache.find(key);
            return (it != cache.end()) ? &it->second : nullptr;
        }
        loaded[key] = false;
        std::string path;
        if (kind == "relic") path = resolve_image_path("assets/relics/" + id);
        else if (kind == "potion") path = resolve_image_path("assets/potions/" + id);
        else return nullptr;
        if (path.empty()) return nullptr;
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return nullptr;
        tex.setSmooth(true);
        cache.emplace(key, std::move(tex));
        loaded[key] = true;
        auto it = cache.find(key);
        return (it != cache.end()) ? &it->second : nullptr;
    };
    const sf::View viewBeforeIllust = window.getView();
    const float lwIll = std::max(1.f, static_cast<float>(width_));
    const float lhIll = std::max(1.f, static_cast<float>(height_));
    if (illustW > 1.f && illustH > 1.f) {
        sf::View clipVI = window.getDefaultView();
        clipVI.setScissor(sf::FloatRect(
            sf::Vector2f(illustLeft / lwIll, illustTop / lhIll),
            sf::Vector2f(illustW / lwIll, illustH / lhIll)));
        window.setView(clipVI);
    }
    auto drawIllustTexFit = [&](const sf::Texture& tex) {
        if (tex.getSize().x == 0) return;
        sf::Sprite sprite(tex);
        const sf::Vector2u texSize = tex.getSize();
        const float scaleX = illustW / static_cast<float>(texSize.x);
        const float scaleY = illustH / static_cast<float>(texSize.y);
        const float s = std::min(scaleX, scaleY);
        sprite.setScale(sf::Vector2f(s, s));
        sprite.setOrigin(sf::Vector2f(texSize.x * 0.5f, texSize.y * 0.5f));
        sprite.setPosition(sf::Vector2f(illustCx, illustCy));
        window.draw(sprite);
    };
    if (eventIllustLoaded_ && eventIllustTexture_.getSize().x > 0) {
        drawIllustTexFit(eventIllustTexture_);
    } else if (illustIsCard) {
        std::vector<std::string> cardIds;
        std::string comboRelicId;
        if (illustIsCardsRelic) {
            const std::string payload = eventData_.imagePath.substr(14); // "__cards_relic:"
            const std::size_t hashPos = payload.find('#');
            const std::string cardPayload = (hashPos == std::string::npos) ? payload : payload.substr(0, hashPos);
            if (hashPos != std::string::npos && hashPos + 1 < payload.size()) {
                comboRelicId = payload.substr(hashPos + 1);
            }
            std::size_t s = 0;
            while (s <= cardPayload.size()) {
                const std::size_t p = cardPayload.find('|', s);
                const std::string id = (p == std::string::npos) ? cardPayload.substr(s) : cardPayload.substr(s, p - s);
                if (!id.empty()) cardIds.push_back(id);
                if (p == std::string::npos) break;
                s = p + 1;
            }
        } else if (eventData_.imagePath.rfind("__cardid:", 0) == 0) {
            cardIds.push_back(eventData_.imagePath.substr(9));
        } else if (eventData_.imagePath.rfind("__cards:", 0) == 0) {
            const std::string payload = eventData_.imagePath.substr(8);
            std::size_t s = 0;
            while (s <= payload.size()) {
                const std::size_t p = payload.find('|', s);
                const std::string id = (p == std::string::npos) ? payload.substr(s) : payload.substr(s, p - s);
                if (!id.empty()) cardIds.push_back(id);
                if (p == std::string::npos) break;
                s = p + 1;
            }
        }
        if (cardIds.size() > 4) cardIds.resize(4);

        auto drawCardInRect = [&](const std::string& cid, const sf::FloatRect& rect) {
            constexpr sf::Color kOutline(210, 175, 95);
            draw_detailed_card_at(window, fontForText(), cid, rect.position.x, rect.position.y, rect.size.x, rect.size.y,
                kOutline, 8.f);
        };

        const float relicRowH = (!comboRelicId.empty()) ? std::max(72.f, illustH * 0.19f) : 0.f;
        const float availW = std::max(40.f, illustW - 16.f);
        const float availH = std::max(60.f, illustH - 16.f - relicRowH - (relicRowH > 0.f ? 4.f : 0.f));
        const float illustCyCards = illustTop + 8.f + availH * 0.5f;
        if (cardIds.size() >= 2) {
            // 结果卡：两列网格排版（2 张=一行，3-4 张=两行）
            const int cols = 2;
            const int rows = static_cast<int>((cardIds.size() + 1) / 2);
            const float gapX = 10.f;
            const float gapY = 10.f;
            const float eachWByWidth = (availW - gapX * static_cast<float>(cols - 1)) / static_cast<float>(cols);
            const float eachWByHeight = (availH - gapY * static_cast<float>(rows - 1)) /
                (static_cast<float>(rows) * BATTLE_CARD_ASPECT_H_OVER_W);
            const float eachW = std::max(40.f, std::min(eachWByWidth, eachWByHeight));
            const float eachH = eachW * BATTLE_CARD_ASPECT_H_OVER_W;
            const float totalW = eachW * static_cast<float>(cols) + gapX * static_cast<float>(cols - 1);
            const float totalH = eachH * static_cast<float>(rows) + gapY * static_cast<float>(rows - 1);
            const float startX = illustCx - totalW * 0.5f;
            const float startY = illustCyCards - totalH * 0.5f;
            for (std::size_t i = 0; i < cardIds.size(); ++i) {
                const int c = static_cast<int>(i % 2);
                const int r = static_cast<int>(i / 2);
                const float x = startX + static_cast<float>(c) * (eachW + gapX);
                const float y = startY + static_cast<float>(r) * (eachH + gapY);
                drawCardInRect(cardIds[i], sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(eachW, eachH)));
            }
        } else if (!cardIds.empty()) {
            // 结果页单卡缩小一些，避免占满左侧
            const float cardW = std::min(availW * 0.84f, (availH * 0.84f) / BATTLE_CARD_ASPECT_H_OVER_W);
            const float cardH = cardW * BATTLE_CARD_ASPECT_H_OVER_W;
            drawCardInRect(cardIds[0],
                sf::FloatRect(sf::Vector2f(illustCx - cardW * 0.5f, illustCyCards - cardH * 0.5f), sf::Vector2f(cardW, cardH)));
        }

        if (!comboRelicId.empty()) {
            const sf::Texture* relicTex = resolveOfferIconTexture("relic", comboRelicId);
            const float rowY = illustTop + illustH - relicRowH;
            draw_relic_combo_strip(window, fontForText(), illustLeft, illustW, rowY, relicRowH, comboRelicId, relicTex,
                bodySize);
        }
        if (cardIds.empty() && comboRelicId.empty() && eventIllustSceneBackupLoaded_ &&
            eventIllustSceneBackupTexture_.getSize().x > 0) {
            drawIllustTexFit(eventIllustSceneBackupTexture_);
        }
    } else if (illustIsRelic || illustIsPotion) {
        const std::string itemId = eventData_.imagePath.substr(illustIsRelic ? 8 : 9);
        const sf::Texture* tex = resolveOfferIconTexture(illustIsRelic ? "relic" : "potion", itemId);
        if (tex && tex->getSize().x > 0 && tex->getSize().y > 0) {
            draw_event_item_solo(window, fontForText(), illustCx, illustTop, illustH, illustW, itemId, tex, bodySize,
                illustIsRelic);
        } else if (eventIllustSceneBackupLoaded_ && eventIllustSceneBackupTexture_.getSize().x > 0) {
            drawIllustTexFit(eventIllustSceneBackupTexture_);
        } else {
            sf::Text illustHint(fontForText(), sf::String(L"[ 图标加载失败 ]"), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
            illustHint.setFillColor(sf::Color(120, 95, 95));
            const sf::FloatRect ihb = illustHint.getLocalBounds();
            illustHint.setOrigin(sf::Vector2f(ihb.position.x + ihb.size.x * 0.5f, ihb.position.y + ihb.size.y * 0.5f));
            illustHint.setPosition(sf::Vector2f(illustCx, illustCy));
            window.draw(illustHint);
        }
    } else if (eventIllustSceneBackupLoaded_ && eventIllustSceneBackupTexture_.getSize().x > 0) {
        drawIllustTexFit(eventIllustSceneBackupTexture_);
    } else {
        sf::Text illustHint(fontForText(), sf::String(L"[ 插图 ]"), static_cast<unsigned>(BODY_CHAR_SIZE - 2));
        illustHint.setFillColor(sf::Color(80, 78, 90));
        const sf::FloatRect ihb = illustHint.getLocalBounds();
        illustHint.setOrigin(sf::Vector2f(ihb.position.x + ihb.size.x * 0.5f, ihb.position.y + ihb.size.y * 0.5f));
        illustHint.setPosition(sf::Vector2f(illustCx, illustCy));
        window.draw(illustHint);
    }
    window.setView(viewBeforeIllust);

    const float rightLeft = panelLeft + leftW;
    const float contentPad = 38.f;
    const float textAreaW = rightW - contentPad * 2.f;
    const bool isEventResultPage = (eventData_.title == L"事件结果");
    const float descRegionH = isEventResultPage ? std::min(300.f, panelH * 0.48f) : std::min(180.f, panelH * 0.26f);
    float y = contentTop + contentPad;
    const sf::String effectiveDesc = eventData_.description.empty() ? sf::String(L"(无描述)") : sf::String(eventData_.description);
    const float wrapMaxH = descRegionH;
    draw_wrapped_text(window, fontForText(), effectiveDesc,
        bodySize, sf::Vector2f(rightLeft + contentPad, y), textAreaW, wrapMaxH, DESC_COLOR);
    y += descRegionH + 16.f;
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
    // 事件选项需要承载“选项文本 + 效果红字”两行：略微加高按钮以减少高度截断
    const float btnH = BUTTON_H * scale * 1.15f;
    const float tipW = BUTTON_TIP_W * scale;
    const float crystalSz = CRYSTAL_SZ * scale;
    const float optTextLeft = 14.f + crystalSz + 12.f;
    // 事件选项当前实现中未真正绘制“tip 区”，预留 tipW 会导致文本过早截断。
    // 这里把 tipW 从可用宽度中移除，让效果预览有足够空间换行展示。
    const float optMaxW = btnW - optTextLeft - 14.f;
    const float optionAreaBottom = panelTop + panelH - contentPad;

    const bool cardPickMode = (eventData_.title == L"择术" || eventData_.title == L"断舍" || eventData_.title == L"精修");
    if (cardPickMode) {
        const std::size_t n = eventData_.optionTexts.size();
        if (n > 0) {
            const int cols = 3; // 固定三列基准，保证多轮选择时卡牌比例与尺寸稳定
            const int rows = static_cast<int>((n + static_cast<std::size_t>(cols) - 1) / static_cast<std::size_t>(cols));
            const float gapX = 10.f * scale;
            const float gapY = 10.f * scale;
            const float cardsTop = y + 4.f * scale; // 给顶部留一点安全边距
            const float cardsBottom = optionAreaBottom - 4.f * scale; // 底部也留边距
            const float availH = std::max(60.f, cardsBottom - cardsTop);
            const float cardW = (textAreaW - gapX * static_cast<float>(cols - 1)) / static_cast<float>(cols);
            // 严格使用战斗系统同款比例：190:300（W:H），不做额外压缩，避免“变胖”
            const float cardH = cardW * BATTLE_CARD_ASPECT_H_OVER_W;
            const float rowStep = cardH + gapY;
            const float totalH = static_cast<float>(rows) * cardH + static_cast<float>(rows - 1) * gapY;
            eventCardScrollStep_ = rowStep;
            eventCardScrollMax_ = std::max(0.f, totalH - availH);
            if (eventCardScrollOffset_ < 0.f) eventCardScrollOffset_ = 0.f;
            if (eventCardScrollOffset_ > eventCardScrollMax_) eventCardScrollOffset_ = eventCardScrollMax_;
            if (eventCardScrollStep_ > 1.f) {
                eventCardScrollOffset_ = std::round(eventCardScrollOffset_ / eventCardScrollStep_) * eventCardScrollStep_;
                if (eventCardScrollOffset_ < 0.f) eventCardScrollOffset_ = 0.f;
                if (eventCardScrollOffset_ > eventCardScrollMax_) eventCardScrollOffset_ = eventCardScrollMax_;
            }
            const float scrollOff = eventCardScrollOffset_;
            const sf::FloatRect viewport(sf::Vector2f(rightLeft + contentPad, cardsTop), sf::Vector2f(textAreaW, availH));
            const sf::View viewBeforeCards = window.getView();
            const float lwCards = std::max(1.f, static_cast<float>(width_));
            const float lhCards = std::max(1.f, static_cast<float>(height_));
            if (viewport.size.x > 1.f && viewport.size.y > 1.f) {
                // 给 scissor 留出描边/高亮安全边距，避免边沿卡牌被裁掉外框。
                const float clipPad = std::max(6.f, 9.f * scale);
                // 允许裁剪区域扩展到“事件主面板内边界”，而不是卡死在内容区边界，
                // 否则左/上/右三侧描边会被持续截断。
                const float panelInset = std::max(8.f, 10.f * scale);
                const float hardL = panelLeft + panelInset;
                const float hardT = panelTop + panelInset;
                const float hardR = panelLeft + panelW - panelInset;
                const float hardB = panelTop + panelH - panelInset;
                const float clipL = std::max(hardL, viewport.position.x - clipPad);
                const float clipT = std::max(hardT, viewport.position.y - clipPad);
                const float clipR = std::min(hardR, viewport.position.x + viewport.size.x + clipPad);
                const float clipB = std::min(hardB, viewport.position.y + viewport.size.y + clipPad);
                sf::View clipVC = window.getDefaultView();
                clipVC.setScissor(sf::FloatRect(
                    sf::Vector2f(clipL / lwCards, clipT / lhCards),
                    sf::Vector2f(std::max(1.f, clipR - clipL) / lwCards, std::max(1.f, clipB - clipT) / lhCards)));
                window.setView(clipVC);
            }

            for (std::size_t i = 0; i < n; ++i) {
                const int c = static_cast<int>(i % static_cast<std::size_t>(cols));
                const int r = static_cast<int>(i / static_cast<std::size_t>(cols));
                const float bx = rightLeft + contentPad + static_cast<float>(c) * (cardW + gapX);
                const float by = cardsTop + static_cast<float>(r) * (cardH + gapY) - scrollOff;
                sf::FloatRect rect(sf::Vector2f(bx, by), sf::Vector2f(cardW, cardH));
                eventOptionRects_.push_back(rect);
                // 与可视区无交集则跳过；有交集即绘制，交由 scissor 做边界裁剪。
                if (rect.position.y + rect.size.y < viewport.position.y ||
                    rect.position.y > viewport.position.y + viewport.size.y) {
                    continue;
                }

                const bool hover = rect.contains(mousePos_);
                const bool pressed = (eventOptionPressedIndex_ == static_cast<int>(i));
                const bool keyFocus = (selectedEventOption_ == static_cast<int>(i));

                if (i < eventData_.optionCardIds.size() && !eventData_.optionCardIds[i].empty()) {
                    constexpr sf::Color kCardOutline(210, 175, 95);
                    draw_detailed_card_at(window, fontForText(), eventData_.optionCardIds[i], rect.position.x, rect.position.y,
                        rect.size.x, rect.size.y, kCardOutline, 8.f);
                    if (hover || keyFocus) {
                        sf::RectangleShape hi(sf::Vector2f(rect.size.x + 8.f, rect.size.y + 8.f));
                        hi.setPosition(sf::Vector2f(rect.position.x - 4.f, rect.position.y - 4.f));
                        hi.setFillColor(sf::Color::Transparent);
                        hi.setOutlineColor(sf::Color(220, 185, 95));
                        hi.setOutlineThickness(5.f);
                        window.draw(hi);
                    }
                    continue;
                }

                const float w = rect.size.x;
                const float h = rect.size.y;
                const float pad = 4.f;
                const float innerL = rect.position.x + pad;
                const float innerT = rect.position.y + pad;
                const float innerW = w - pad * 2.f;

                // 复用战斗卡面结构（底板/标题条/中部图区/类型条）
                sf::RectangleShape cardBg(sf::Vector2f(w, h));
                cardBg.setPosition(rect.position);
                cardBg.setFillColor(sf::Color(55, 50, 48));
                cardBg.setOutlineColor((hover || keyFocus) ? sf::Color(220, 185, 95) : sf::Color(180, 50, 45));
                cardBg.setOutlineThickness((hover || keyFocus) ? 6.f : 4.f);
                window.draw(cardBg);

                const float titleY = innerT + 18.f;
                const float titleH = 28.f;
                sf::RectangleShape titleBar(sf::Vector2f(innerW - 12.f, titleH));
                titleBar.setPosition(sf::Vector2f(innerL + 6.f, titleY));
                titleBar.setFillColor(sf::Color(72, 68, 65));
                titleBar.setOutlineColor(sf::Color(90, 85, 82));
                titleBar.setOutlineThickness(1.f);
                window.draw(titleBar);

                const float artTop = titleY + titleH + 4.f;
                const float artH = std::max(34.f, h * 0.25f);
                sf::ConvexShape artPanel;
                artPanel.setPointCount(8);
                artPanel.setPoint(0, sf::Vector2f(innerL, artTop));
                artPanel.setPoint(1, sf::Vector2f(innerL + innerW, artTop));
                artPanel.setPoint(2, sf::Vector2f(innerL + innerW, artTop + artH - 8.f));
                artPanel.setPoint(3, sf::Vector2f(innerL + innerW * 0.75f, artTop + artH));
                artPanel.setPoint(4, sf::Vector2f(innerL + innerW * 0.5f, artTop + artH - 7.f));
                artPanel.setPoint(5, sf::Vector2f(innerL + innerW * 0.25f, artTop + artH));
                artPanel.setPoint(6, sf::Vector2f(innerL, artTop + artH - 8.f));
                artPanel.setPoint(7, sf::Vector2f(innerL, artTop));
                artPanel.setFillColor(sf::Color(120, 45, 42));
                artPanel.setOutlineColor(sf::Color(100, 38, 35));
                artPanel.setOutlineThickness(1.f);
                window.draw(artPanel);

                const float typeY = artTop + artH + 5.f;
                const float typeH = 22.f;
                sf::RectangleShape typeBar(sf::Vector2f(innerW - 16.f, typeH));
                typeBar.setPosition(sf::Vector2f(innerL + 8.f, typeY));
                typeBar.setFillColor(sf::Color(72, 68, 65));
                typeBar.setOutlineColor(sf::Color(90, 85, 82));
                typeBar.setOutlineThickness(1.f);
                window.draw(typeBar);

                const sf::String titleText = eventData_.optionTexts[i].empty()
                    ? sf::String(L"[ 卡牌 ]")
                    : sf::String(eventData_.optionTexts[i]);
                const std::wstring effW = (i < eventData_.optionEffectTexts.size()) ? eventData_.optionEffectTexts[i] : std::wstring();
                const sf::String effectText = effW.empty() ? sf::String(L"无效果说明") : sf::String(effW);
                const unsigned cardTitleSize = static_cast<unsigned>(std::max(10.f, bodySize * 0.62f));
                const unsigned cardEffectSize = static_cast<unsigned>(std::max(9.f, bodySize * 0.50f));

                sf::Text nameText(fontForText(), titleText, cardTitleSize);
                nameText.setFillColor(sf::Color::White);
                const sf::FloatRect nb = nameText.getLocalBounds();
                nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
                nameText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, titleY + titleH * 0.5f));
                window.draw(nameText);

                sf::Text typeText(fontForText(), sf::String(L"卡牌"), static_cast<unsigned>(std::max(11.f, bodySize * 0.55f)));
                typeText.setFillColor(sf::Color::White);
                const sf::FloatRect tb = typeText.getLocalBounds();
                typeText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
                typeText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, typeY + typeH * 0.5f));
                window.draw(typeText);

                // 从 “费X｜描述” 中提取费用，画战斗同款费用圈
                int parsedCost = -999;
                if (!effW.empty() && effW[0] == L'费') {
                    int j = 1;
                    int sign = 1;
                    if (j < static_cast<int>(effW.size()) && effW[static_cast<std::size_t>(j)] == L'-') { sign = -1; ++j; }
                    int v = 0; bool any = false;
                    while (j < static_cast<int>(effW.size())) {
                        const wchar_t wc = effW[static_cast<std::size_t>(j)];
                        if (wc >= L'0' && wc <= L'9') { any = true; v = v * 10 + (wc - L'0'); ++j; }
                        else break;
                    }
                    if (any) parsedCost = v * sign;
                }
                if (parsedCost != -999) {
                    const float costR = 16.f;
                    const float costCx = innerL + costR - 6.f;
                    const float costCy = innerT + costR - 6.f;
                    sf::CircleShape costRing(costR + 3.f);
                    costRing.setPosition(sf::Vector2f(costCx - costR - 3.f, costCy - costR - 3.f));
                    costRing.setFillColor(sf::Color(0, 0, 0, 0));
                    costRing.setOutlineColor(sf::Color(255, 190, 90));
                    costRing.setOutlineThickness(2.f);
                    window.draw(costRing);
                    sf::CircleShape costCircle(costR);
                    costCircle.setPosition(sf::Vector2f(costCx - costR, costCy - costR));
                    costCircle.setFillColor(sf::Color(200, 55, 50));
                    costCircle.setOutlineColor(sf::Color(255, 190, 90));
                    costCircle.setOutlineThickness(1.5f);
                    window.draw(costCircle);
                    sf::String costStr = (parsedCost == -1) ? sf::String(L"X") : sf::String(std::to_string(parsedCost));
                    sf::Text costText(fontForText(), costStr, static_cast<unsigned>(std::max(12.f, bodySize * 0.62f)));
                    costText.setFillColor(sf::Color::White);
                    const sf::FloatRect cb = costText.getLocalBounds();
                    costText.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
                    costText.setPosition(sf::Vector2f(costCx, costCy));
                    window.draw(costText);
                }

                const float descX = innerL + 8.f;
                const float descY = typeY + typeH + 10.f;
                const float descMaxW = innerW - 16.f;
                const float descMaxH = rect.position.y + rect.size.y - descY - 8.f;
                draw_wrapped_text(window, fontForText(), effectText, cardEffectSize,
                    sf::Vector2f(descX, descY), descMaxW, descMaxH,
                    sf::Color(240, 238, 235));
            }
            window.setView(viewBeforeCards);

            // 滚动条提示（可下滑）
            if (eventCardScrollMax_ > 0.f) {
                const float barW = 6.f;
                const float barX = viewport.position.x + viewport.size.x - barW - 2.f;
                sf::RectangleShape rail(sf::Vector2f(barW, viewport.size.y));
                rail.setPosition(sf::Vector2f(barX, viewport.position.y));
                rail.setFillColor(sf::Color(55, 55, 62, 150));
                window.draw(rail);

                const float thumbH = std::max(26.f, viewport.size.y * (viewport.size.y / (viewport.size.y + eventCardScrollMax_)));
                const float t = (eventCardScrollMax_ <= 0.001f) ? 0.f : (eventCardScrollOffset_ / eventCardScrollMax_);
                const float thumbY = viewport.position.y + (viewport.size.y - thumbH) * t;
                sf::RectangleShape thumb(sf::Vector2f(barW, thumbH));
                thumb.setPosition(sf::Vector2f(barX, thumbY));
                thumb.setFillColor(sf::Color(200, 170, 95, 200));
                window.draw(thumb);
            }
        }
    } else {
    eventCardScrollOffset_ = 0.f;
    eventCardScrollMax_ = 0.f;
    for (size_t i = 0; i < eventData_.optionTexts.size(); ++i) {
        const float optionGap = BUTTON_GAP * scale * 0.65f;
        const float by = y + (btnH + optionGap) * static_cast<float>(i);
        if (by + btnH > optionAreaBottom) break;
        sf::FloatRect rect(sf::Vector2f(rightLeft + contentPad, by), sf::Vector2f(btnW, btnH));
        eventOptionRects_.push_back(rect);
        bool hover = rect.contains(mousePos_);
        bool pressed = (eventOptionPressedIndex_ == static_cast<int>(i));
        bool keyFocus = (selectedEventOption_ == static_cast<int>(i));

        const bool disabled = false; // 当前选项均可用；保留 disabled 态样式位
        const bool focused = hover || keyFocus;
        float shimmer = focused ? (0.5f + 0.5f * std::sin(t * 6.f + static_cast<float>(i) * 1.7f)) : 0.f;
        if (pressed) shimmer *= 0.35f; // 点击态光效收敛

        const sf::Color gold(190, 165, 85);
        const sf::Color goldDark(110, 88, 35);

        const sf::Color tealBase(28, 58, 58, 220);
        const sf::Color tealHover(38, 74, 74, 235);
        const sf::Color tealPressed(20, 44, 44, 220);
        const sf::Color tealDisabled(48, 48, 48, 170);

        const sf::Color outlineCol = disabled
            ? sf::Color(120, 120, 120)
            : (keyFocus ? sf::Color(210, 175, 95) : goldDark);
        const sf::Color fillCol = disabled
            ? tealDisabled
            : (pressed ? tealPressed : (keyFocus ? tealHover : (hover ? tealHover : tealBase)));

        const float hoverOffY = (!pressed && hover) ? (-2.f * scale) : 0.f; // hover 轻微上浮
        const float pressOffY = pressed ? (2.f * scale) : 0.f;
        const sf::Vector2f drawOff(0.f, pressOffY + hoverOffY);

        // 主形状：斜切器物签条（保持原有“右侧斜角”结构）
        sf::ConvexShape btn(5);
        btn.setPoint(0, sf::Vector2f(0, 0));
        btn.setPoint(1, sf::Vector2f(0, btnH));
        btn.setPoint(2, sf::Vector2f(btnW - tipW, btnH));
        btn.setPoint(3, sf::Vector2f(btnW, btnH * 0.5f));
        btn.setPoint(4, sf::Vector2f(btnW - tipW, 0));
        btn.setPosition(sf::Vector2f(rightLeft + contentPad, by) + drawOff);
        btn.setFillColor(fillCol);
        btn.setOutlineThickness(1.1f * scale);
        btn.setOutlineColor(outlineCol);
        window.draw(btn);

        // 外层铜饰压边：形成更强层级与“被镶嵌”的仪式感
        sf::ConvexShape frameShape = btn;
        frameShape.setFillColor(sf::Color::Transparent);
        frameShape.setOutlineThickness((3.4f + 0.4f * shimmer) * scale);
        frameShape.setOutlineColor(sf::Color(gold.r, gold.g, gold.b,
            static_cast<uint8_t>(disabled ? 90 : (90 + 80 * shimmer))));
        window.draw(frameShape);

        // 上方弱高光条已移除：避免出现按钮内部“黄色细线”

        // 右侧“云纹/磨损”极淡纹理：少量竖线，远看不乱
        sf::VertexArray cloud(sf::PrimitiveType::Lines);
        const int cloudCount = 5;
        for (int ci = 0; ci < cloudCount; ++ci) {
            const float x = rightLeft + contentPad + (btnW - tipW * 0.2f) + ci * 7.f * scale;
            const float a = 0.0f + (focused ? (0.10f + 0.15f * shimmer) : 0.08f);
            // sf::Vertex 在本项目的 SFML 版本中是聚合体，没有构造函数，需用聚合初始化
            cloud.append(sf::Vertex{ sf::Vector2f(x, by + 10.f * scale) + drawOff,
                sf::Color(70, 60, 40, static_cast<uint8_t>(255 * a)) });
            cloud.append(sf::Vertex{ sf::Vector2f(x, by + btnH - 10.f * scale) + drawOff,
                sf::Color(70, 60, 40, static_cast<uint8_t>(255 * a * 0.7f)) });
        }
        window.draw(cloud);

        // 极淡“尘光”：选中态额外一点点旧金粒子，增强点击吸引力（克制、不拖尾）
        if (keyFocus && !disabled) {
            sf::CircleShape dust(static_cast<float>(1.4f * scale));
            dust.setFillColor(sf::Color(gold.r, gold.g, gold.b, static_cast<uint8_t>(35 + 50 * shimmer)));
            for (int p = 0; p < 3; ++p) {
                const float px = rightLeft + contentPad + (btnW * 0.68f) + std::sin(t * 7.f + p * 1.9f) * (7.f * scale);
                const float py = by + btnH * 0.35f + std::cos(t * 5.f + p * 2.3f) * (6.f * scale);
                dust.setPosition(sf::Vector2f(px, py) + drawOff);
                window.draw(dust);
            }
        }

        // 左侧“玉符印”锚点：金线描边的玉菱纹样
        const float cxX = rect.position.x + 14.f + crystalSz * 0.5f;
        const float cxY = rect.position.y + btnH * 0.5f + drawOff.y;
        sf::ConvexShape badge(4);
        badge.setPoint(0, sf::Vector2f(0, -crystalSz * 0.52f));
        badge.setPoint(1, sf::Vector2f(crystalSz * 0.52f, 0));
        badge.setPoint(2, sf::Vector2f(0, crystalSz * 0.52f));
        badge.setPoint(3, sf::Vector2f(-crystalSz * 0.52f, 0));
        badge.setPosition(sf::Vector2f(cxX, cxY));
        badge.setFillColor(disabled ? sf::Color(120, 120, 120, 160) : sf::Color(210, 185, 95, 220));
        badge.setOutlineThickness(0.8f * scale);
        badge.setOutlineColor(disabled ? sf::Color(160, 160, 160, 120) : sf::Color(120, 92, 35, 200));
        window.draw(badge);

        // 内部交叉刻纹（极淡）
        sf::VertexArray cross(sf::PrimitiveType::Lines, 4);
        const float r = crystalSz * 0.28f;
        cross[0].position = sf::Vector2f(cxX - r, cxY - r);
        cross[1].position = sf::Vector2f(cxX + r, cxY + r);
        cross[2].position = sf::Vector2f(cxX - r, cxY + r);
        cross[3].position = sf::Vector2f(cxX + r, cxY - r);
        for (int ci = 0; ci < 4; ++ci) {
            cross[ci].color = sf::Color(70, 60, 40, disabled ? 90 : static_cast<uint8_t>(120 + 40 * shimmer));
        }
        window.draw(cross);
        const sf::String rawLabel = eventData_.optionTexts[i].empty()
            ? sf::String(L"[ 选项 ]")
            : (sf::String(L"[ ") + sf::String(eventData_.optionTexts[i]) + L" ]");

        const std::wstring effW = (i < eventData_.optionEffectTexts.size()) ? eventData_.optionEffectTexts[i] : std::wstring();
        const sf::String effectText = effW.empty() ? sf::String() : sf::String(effW);
        const bool hasEffect = !effectText.isEmpty();

        const float innerTop = rect.position.y + 4.f + drawOff.y;
        const float innerBottom = rect.position.y + btnH - 4.f + drawOff.y;
        const float innerH = std::max(10.f, innerBottom - innerTop);

        // 选项文字稍大；效果信息更精致（代价/收益分色）
        const unsigned optionTextSize = static_cast<unsigned>(std::max(14.f, bodySize * 0.85f));
        const unsigned effectTextSize = static_cast<unsigned>(std::max(10.f, bodySize * 0.62f));
        const sf::Color costRed(180, 60, 70);     // 代价：低饱和朱砂红
        const sf::Color gainGold(210, 175, 85);   // 收益：低饱和旧金
        const sf::Color neutralInk(170, 160, 150);

        const float optionLineH = fontForText().getLineSpacing(optionTextSize);
        const float effectLineH = fontForText().getLineSpacing(effectTextSize);

        // 预留至少一行效果高度，让效果总能落在“选项下一行”
        const float effectReserveH = hasEffect ? (effectLineH + 2.f) : 0.f;
        const float optionMaxH = std::max(0.f, innerH - effectReserveH);
        const int maxOptionLines = hasEffect
            ? std::max(1, static_cast<int>(std::floor(optionMaxH / std::max(0.001f, optionLineH))))
            : std::max(1, static_cast<int>(std::floor(innerH / std::max(0.001f, optionLineH))));

        const auto optionLines = wrap_lines_no_ellipsis(fontForText(), rawLabel, optionTextSize, optMaxW,
            maxOptionLines);

        const float optionTextX = rect.position.x + optTextLeft;
        const float extraDownY = 1.25f * scale; // 选项文字整体略向下（克制一点点）

        if (hasEffect) {
            const float optionBlockH = static_cast<float>(optionLines.size()) * optionLineH;
            const float gapY = 2.f; // 固定“效果在选项下一行”的行距
            const float effectAvailableH = std::max(0.f, innerH - optionBlockH - gapY);
            const int maxEffectLines = std::max(1, static_cast<int>(std::floor(effectAvailableH / std::max(0.001f, effectLineH))));

            // 效果预览：按“；”切分 token，再按宽度贪心换行（不使用省略号）。
            std::vector<sf::String> tokens;
            sf::String cur;
            for (std::size_t ci = 0; ci < effectText.getSize(); ++ci) {
                const char32_t ch = effectText[ci];
                if (ch == U'；') {
                    if (!cur.isEmpty()) tokens.push_back(cur);
                    cur.clear();
                } else {
                    cur += ch;
                }
            }
            if (!cur.isEmpty()) tokens.push_back(cur);

            auto pickColor = [&](const sf::String& tk) -> sf::Color {
                if (tk.isEmpty()) return neutralInk;
                const char32_t first = tk[0];
                if (first == U'-') return costRed;
                if (first == U'+') return gainGold;

                // SFML 不同版本的 sf::String 可能没有 indexOf/find；这里用字符扫描保证可编译
                auto hasChar = [&](char32_t c) -> bool {
                    for (std::size_t j = 0; j < tk.getSize(); ++j) {
                        if (tk[j] == c) return true;
                    }
                    return false;
                };
                auto hasSub2 = [&](char32_t a, char32_t b) -> bool {
                    for (std::size_t j = 0; j + 1 < tk.getSize(); ++j) {
                        if (tk[j] == a && tk[j + 1] == b) return true;
                    }
                    return false;
                };

                if (hasChar(U'金')) return gainGold;
                if (hasSub2(U'遗', U'物')) return gainGold;
                if (hasChar(U'牌')) return gainGold;
                if (hasChar(U'血')) return costRed; // 未带 +/- 时，血通常视为代价
                return gainGold;
            };

            sf::Text measure(fontForText(), L"", effectTextSize);
            const float sepGap = 6.f * scale;

            std::vector<std::vector<std::size_t>> tokenLines;
            std::vector<std::size_t> curLine;
            float curW = 0.f;
            for (std::size_t ti = 0; ti < tokens.size(); ++ti) {
                const sf::String& tk = tokens[ti];
                if (tk.isEmpty()) continue;
                measure.setString(tk);
                const float w = measure.getLocalBounds().size.x;
                const float addW = (curLine.empty() ? 0.f : sepGap) + w;
                if (!curLine.empty() && curW + addW > optMaxW) {
                    tokenLines.push_back(curLine);
                    curLine.clear();
                    curW = 0.f;
                    if (static_cast<int>(tokenLines.size()) >= maxEffectLines) break;
                }
                curLine.push_back(ti);
                curW += addW;
            }
            if (!curLine.empty() && static_cast<int>(tokenLines.size()) < maxEffectLines) {
                tokenLines.push_back(curLine);
            }

            // 垂直居中：确保“选项描述 + 效果(含gap)”落在选项框正中间
            const float effectBlockH = static_cast<float>(tokenLines.size()) * effectLineH;
            const float totalH = optionBlockH + gapY + effectBlockH;
            const float contentTop = innerTop + std::max(0.f, (innerH - totalH) * 0.5f) + extraDownY;

            // 先画选项描述
            for (std::size_t li = 0; li < optionLines.size(); ++li) {
                sf::Text t(fontForText(), optionLines[li], optionTextSize);
                t.setFillColor(TEXT_COLOR);
                t.setPosition(sf::Vector2f(optionTextX, contentTop + static_cast<float>(li) * optionLineH));
                window.draw(t);
            }

            // 再画效果：从“选项下一行”开始
            const float effectStartY = contentTop + optionBlockH + gapY;
            for (std::size_t li = 0; li < tokenLines.size(); ++li) {
                float x = optionTextX;
                const float y = effectStartY + static_cast<float>(li) * effectLineH;
                for (std::size_t k = 0; k < tokenLines[li].size(); ++k) {
                    const std::size_t idx = tokenLines[li][k];
                    const sf::String& tk = tokens[idx];
                    if (tk.isEmpty()) continue;
                    measure.setString(tk);
                    sf::Text t(fontForText(), tk, effectTextSize);
                    t.setFillColor(pickColor(tk));
                    t.setPosition(sf::Vector2f(x, y));
                    window.draw(t);
                    x += measure.getLocalBounds().size.x + sepGap;
                }
            }
        }
        else {
            const float optionBlockH = static_cast<float>(optionLines.size()) * optionLineH;
            const float totalH = optionBlockH;
            const float contentTop = innerTop + std::max(0.f, (innerH - totalH) * 0.5f) + extraDownY;
            for (std::size_t li = 0; li < optionLines.size(); ++li) {
                sf::Text t(fontForText(), optionLines[li], optionTextSize);
                t.setFillColor(TEXT_COLOR);
                t.setPosition(sf::Vector2f(optionTextX, contentTop + static_cast<float>(li) * optionLineH));
                window.draw(t);
            }
        }
    }
    }
    if (!eventOptionRects_.empty()) {
        if (selectedEventOption_ < 0) selectedEventOption_ = 0;
        if (selectedEventOption_ >= static_cast<int>(eventOptionRects_.size())) selectedEventOption_ = static_cast<int>(eventOptionRects_.size()) - 1;
    }
}

} // namespace tce
