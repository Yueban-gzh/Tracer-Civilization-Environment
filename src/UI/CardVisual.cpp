/**
 * 与 BattleUI 手牌/奖励/选牌共用的完整卡面绘制（供事件/商店/休息等复用）。
 */
#include "UI/CardVisual.hpp"
#include "BattleEngine/BattleStateSnapshot.hpp"
#include "CardSystem/CardSystem.hpp"
#include "DataLayer/DataLayer.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

namespace tce {

namespace {

std::unordered_map<std::string, sf::Texture>& card_art_cache() {
    static std::unordered_map<std::string, sf::Texture> m;
    return m;
}

int snapshot_player_status_stacks(const BattleStateSnapshot& s, const std::string& statusId) {
    int t = 0;
    for (const auto& st : s.playerStatuses) {
        if (st.id == statusId) t += st.stacks;
    }
    return t;
}

int effective_hand_card_energy_cost(const CardData* cd, const CardInstance& inst, const BattleStateSnapshot* snap_opt) {
    if (!cd) return 0;
    int c = cd->cost;
    if (c < 0) return c;
    if (snap_opt) {
        if (cd->cardType == CardType::Skill && snapshot_player_status_stacks(*snap_opt, "corruption") > 0) return 0;
        if (cd->cardType == CardType::Skill && inst.combat_cost_zero) return 0;
        if (cd->cardType == CardType::Attack && snapshot_player_status_stacks(*snap_opt, "free_attack") > 0) return 0;
        const std::string& hid = inst.id;
        if (hid == "eviscerate" || hid == "eviscerate+") {
            const int disc = snapshot_player_status_stacks(*snap_opt, "discarded_this_turn");
            return std::max(0, cd->cost - disc - inst.combatCostDiscount);
        }
    }
    int e = c - inst.combatCostDiscount;
    return e < 0 ? 0 : e;
}

constexpr float kCardFacePi = 3.14159265f;

void build_round_rect_poly(std::vector<sf::Vector2f>& out, float x, float y, float rw, float rh, float r, int seg) {
    out.clear();
    r = std::max(0.f, std::min(r, std::min(rw, rh) * 0.5f - 0.5f));
    if (r < 0.5f) {
        out.push_back(sf::Vector2f(x, y));
        out.push_back(sf::Vector2f(x + rw, y));
        out.push_back(sf::Vector2f(x + rw, y + rh));
        out.push_back(sf::Vector2f(x, y + rh));
        return;
    }
    auto add_arc = [&](float cx, float cy, float a0, float a1) {
        for (int j = 1; j <= seg; ++j) {
            const float t = static_cast<float>(j) / static_cast<float>(seg);
            const float a = a0 + (a1 - a0) * t;
            out.push_back(sf::Vector2f(cx + r * std::cos(a), cy + r * std::sin(a)));
        }
    };
    out.push_back(sf::Vector2f(x + r, y));
    out.push_back(sf::Vector2f(x + rw - r, y));
    add_arc(x + rw - r, y + r, -kCardFacePi * 0.5f, 0.f);
    out.push_back(sf::Vector2f(x + rw, y + rh - r));
    add_arc(x + rw - r, y + rh - r, 0.f, kCardFacePi * 0.5f);
    out.push_back(sf::Vector2f(x + r, y + rh));
    add_arc(x + r, y + rh - r, kCardFacePi * 0.5f, kCardFacePi);
    out.push_back(sf::Vector2f(x, y + r));
    add_arc(x + r, y + r, kCardFacePi, kCardFacePi * 1.5f);
}

void draw_convex_poly(sf::RenderWindow& window, const std::vector<sf::Vector2f>& pts, const sf::Color& fill,
    const sf::Color& stroke, float strokeTh, const sf::RenderStates& rs) {
    if (pts.size() < 3) return;
    sf::ConvexShape poly;
    poly.setPointCount(pts.size());
    for (std::size_t j = 0; j < pts.size(); ++j)
        poly.setPoint(j, pts[j]);
    poly.setFillColor(fill);
    poly.setOutlineColor(stroke);
    poly.setOutlineThickness(strokeTh);
    window.draw(poly, rs);
}

void draw_wrapped_text(sf::RenderTarget& target, const sf::Font& font, const sf::String& text, unsigned int charSize,
    sf::Vector2f pos, float maxWidth, float maxHeight, sf::Color color, const sf::RenderStates& states = sf::RenderStates::Default) {
    if (maxWidth <= 1.f || maxHeight <= 1.f || text.isEmpty()) return;
    sf::Text measure(font, sf::String(L""), charSize);
    measure.setFillColor(color);
    const float lineH = font.getLineSpacing(charSize);
    const int maxLines = static_cast<int>(std::floor(maxHeight / lineH));
    if (maxLines <= 0) return;
    std::vector<sf::String> lines;
    lines.reserve(static_cast<size_t>(maxLines));
    sf::String current;
    auto flush_line = [&]() {
        lines.push_back(current);
        current.clear();
    };
    for (std::size_t ii = 0; ii < text.getSize(); ++ii) {
        const char32_t ch = text[ii];
        if (ch == U'\r') continue;
        if (ch == U'\n') {
            flush_line();
            if (static_cast<int>(lines.size()) >= maxLines) break;
            continue;
        }
        sf::String candidate = current;
        candidate += ch;
        measure.setString(candidate);
        const float w = measure.getLocalBounds().size.x;
        if (!current.isEmpty() && w > maxWidth) {
            flush_line();
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
        const sf::String ell = sf::String(L"…");
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
        target.draw(t, states);
    }
}

} // namespace

void draw_detailed_card_at(sf::RenderWindow& window, const sf::Font& fontZh, const std::string& card_id, float cardX, float cardY,
    float w, float h, const sf::Color& outlineColor, float outlineThickness, const sf::RenderStates& states,
    const BattleStateSnapshot* handSnap, const CardInstance* handInst) {
    auto base_art_key = [](const std::string& id) -> std::string {
        // 用于纹理缓存 key：尽量把同一卡的变体归并（+ / _green 等）
        std::string k = id;
        if (!k.empty() && k.back() == '+') k.pop_back();
        const std::string suffixes[] = {"_green", "_blue", "_red", "_purple"};
        for (const auto& sfx : suffixes) {
            if (k.size() >= sfx.size() && k.compare(k.size() - sfx.size(), sfx.size(), sfx) == 0) {
                k.erase(k.size() - sfx.size());
                break;
            }
        }
        return k;
    };

    auto base_id_for_art_fallback = [](const std::string& id) -> std::string {
        // 升级版优先回退到基础 id 的 art（例如 strike+ -> strike）
        if (!id.empty() && id.back() == '+') return id.substr(0, id.size() - 1u);
        return id;
    };

    const CardData* cd = get_card_by_id(card_id);
    // 详情预览的 card_id 可能带有后缀（如 "+", "_green" 等），这里做一次回退查找，
    // 避免 cd 为空导致诅咒/状态等被当作默认 Common 灰色渲染。
    if (!cd) cd = get_card_by_id(base_art_key(card_id));
    const bool isUpgradedCardId = !card_id.empty() && card_id.back() == '+';
    const CardData* baseCdForUpgrade = nullptr;
    if (isUpgradedCardId && card_id.size() > 1u)
        baseCdForUpgrade = get_card_by_id(card_id.substr(0, card_id.size() - 1u));
    // 升级版：只在“静态费用低于原版”时把费用数字染绿；牌名不变色
    constexpr sf::Color kUpgradeFeeDownGreen(200, 255, 225);
    const bool upgradedStaticFeeLower =
        isUpgradedCardId && cd && baseCdForUpgrade && baseCdForUpgrade->cost >= 0 && cd->cost >= 0 &&
        cd->cost < baseCdForUpgrade->cost;
    char buf[32];
    std::vector<sf::Vector2f> rr;

    // 以预览牌高度 410 为参考，默认牌与预览同一套比例，仅整体缩放
    constexpr float kCardLayoutRefH = 410.f;
    const float s = h / kCardLayoutRefH;

    const bool greenCharacterCard = cd && cd->color == CardColor::Green;
    const bool colorlessCard      = cd && cd->color == CardColor::Colorless;
    const bool curseCard          = cd && cd->color == CardColor::Curse;
    const bool statusCard         = cd && cd->cardType == CardType::Status;
    const float thickOutline = std::max(1.f, outlineThickness * s);
    const bool cardOutlineEmphasis = thickOutline > 9.5f;
    sf::Color outerOutlineUse = outlineColor;
    if (greenCharacterCard)
        outerOutlineUse = cardOutlineEmphasis ? sf::Color(140, 250, 175) : sf::Color(72, 175, 108);
    else if (colorlessCard)
        outerOutlineUse = cardOutlineEmphasis ? sf::Color(240, 240, 242) : sf::Color(170, 170, 174);
    else if (curseCard)
        outerOutlineUse = cardOutlineEmphasis ? sf::Color(185, 165, 205) : sf::Color(110, 100, 125);

    const float outerR = 11.f * s;
    const float frameInset = 7.f * s;
    const float innerR = 6.f * s;

    // 投影（略偏移的圆角矩形）
    build_round_rect_poly(rr, cardX + 4.f * s, cardY + 5.f * s, w - 2.f * s, h - 2.f * s, std::max(4.f, outerR - 1.f * s), 3);
    draw_convex_poly(window, rr, sf::Color(0, 0, 0, 72), sf::Color::Transparent, 0.f, states);
    // 外框：红/绿/无色/诅咒按卡色分皮肤
    build_round_rect_poly(rr, cardX, cardY, w, h, outerR, 3);
    draw_convex_poly(window, rr,
                     curseCard ? sf::Color(46, 44, 52)
                              : colorlessCard ? sf::Color(128, 128, 132)
                              : greenCharacterCard ? sf::Color(42, 138, 78)
                                                  : sf::Color(128, 72, 54),
                     outerOutlineUse, thickOutline, states);

    const float ix = cardX + frameInset;
    const float iy = cardY + frameInset;
    const float iw = w - frameInset * 2.f;
    const float ih = h - frameInset * 2.f;
    build_round_rect_poly(rr, ix, iy, iw, ih, innerR, 3);
    draw_convex_poly(window, rr,
                     curseCard ? sf::Color(22, 20, 28)
                              : colorlessCard ? sf::Color(56, 56, 58)
                              : greenCharacterCard ? sf::Color(34, 46, 38)
                                                  : sf::Color(46, 42, 40),
                     curseCard ? sf::Color(10, 10, 14)
                              : colorlessCard ? sf::Color(28, 28, 30)
                              : greenCharacterCard ? sf::Color(22, 32, 26)
                                                  : sf::Color(28, 24, 22),
                     std::max(0.5f, 1.f * s), states);

    const float padIn = 6.f * s;
    const float ribbonX = ix + padIn;
    const float ribbonW = iw - padIn * 2.f;
    const float titleY = iy + 9.f * s;
    const float titleH = 30.f * s;
    const float ribTop = titleY + 2.f * s;
    const float ribBot = titleY + titleH;
    constexpr int kRibbonArchSeg = 8;
    sf::ConvexShape ribbon;
    ribbon.setPointCount(static_cast<std::size_t>(2 + kRibbonArchSeg + 1));
    ribbon.setPoint(0, sf::Vector2f(ribbonX, ribBot - 1.f * s));
    ribbon.setPoint(1, sf::Vector2f(ribbonX + ribbonW, ribBot - 1.f * s));
    for (int i = 0; i <= kRibbonArchSeg; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kRibbonArchSeg);
        const float omt = 1.f - t;
        const float px = omt * omt * (ribbonX + ribbonW) + 2.f * omt * t * (ribbonX + ribbonW * 0.5f) + t * t * ribbonX;
        const float py = omt * omt * (ribBot - 7.f * s) + 2.f * omt * t * ribTop + t * t * (ribBot - 7.f * s);
        ribbon.setPoint(static_cast<std::size_t>(2 + i), sf::Vector2f(px, py));
    }
    const Rarity cardRarity = cd ? cd->rarity : Rarity::Common;
    sf::Color ribFill;
    sf::Color ribOutline;
    float ribOutlineTh = 2.f * s;
    sf::Color ribGloss(255, 255, 255, 42);
    if (statusCard) {
        // 状态牌：保持偏冷的灰系（不走稀有度金/蓝/灰），与诅咒区分
        ribFill = sf::Color(138, 142, 150);
        ribOutline = sf::Color(64, 68, 78);
        ribGloss = sf::Color(255, 255, 255, 38);
    } else if (curseCard) {
        // 诅咒牌：固定紫系（不随稀有度变灰），与状态/无色区分
        ribFill = sf::Color(146, 112, 184);
        ribOutline = sf::Color(72, 44, 110);
        ribGloss = sf::Color(255, 245, 255, 48);
        ribOutlineTh = 2.5f * s;
    } else if (colorlessCard) {
        switch (cardRarity) {
        case Rarity::Common:
            ribFill = sf::Color(190, 190, 194);
            ribOutline = sf::Color(92, 92, 98);
            ribGloss = sf::Color(255, 255, 255, 46);
            break;
        case Rarity::Uncommon:
            ribFill = sf::Color(40, 108, 215);
            ribOutline = sf::Color(14, 52, 142);
            ribGloss = sf::Color(185, 225, 255, 62);
            ribOutlineTh = 2.5f * s;
            break;
        case Rarity::Rare:
        case Rarity::Special:
            ribFill = sf::Color(224, 182, 72);
            ribOutline = sf::Color(118, 78, 28);
            ribGloss = sf::Color(255, 248, 200, 72);
            ribOutlineTh = 3.f * s;
            break;
        }
    } else if (greenCharacterCard) {
        switch (cardRarity) {
        case Rarity::Common:
            ribFill = sf::Color(158, 162, 166);
            ribOutline = sf::Color(88, 94, 90);
            ribGloss = sf::Color(255, 255, 255, 50);
            break;
        case Rarity::Uncommon:
            ribFill = sf::Color(48, 118, 210);
            ribOutline = sf::Color(18, 62, 148);
            ribGloss = sf::Color(210, 235, 255, 62);
            ribOutlineTh = 2.5f * s;
            break;
        case Rarity::Rare:
        case Rarity::Special:
            ribFill = sf::Color(224, 182, 72);
            ribOutline = sf::Color(118, 78, 28);
            ribGloss = sf::Color(255, 248, 200, 72);
            ribOutlineTh = 3.f * s;
            break;
        }
    } else {
        switch (cardRarity) {
        case Rarity::Common:
            ribFill = sf::Color(122, 116, 112);
            ribOutline = sf::Color(52, 48, 46);
            ribGloss = sf::Color(255, 255, 255, 45);
            break;
        case Rarity::Uncommon:
            ribFill = sf::Color(40, 108, 215);
            ribOutline = sf::Color(14, 52, 142);
            ribGloss = sf::Color(185, 225, 255, 62);
            ribOutlineTh = 2.5f * s;
            break;
        case Rarity::Rare:
        case Rarity::Special:
            ribFill = sf::Color(224, 182, 72);
            ribOutline = sf::Color(118, 78, 28);
            ribGloss = sf::Color(255, 248, 200, 72);
            ribOutlineTh = 3.f * s;
            break;
        }
    }
    ribbon.setFillColor(ribFill);
    ribbon.setOutlineColor(ribOutline);
    ribbon.setOutlineThickness(ribOutlineTh);
    window.draw(ribbon, states);
    {
        const sf::Vector2f ribC(ribbonX + ribbonW * 0.5f, ribTop + (ribBot - ribTop) * 0.52f);
        sf::ConvexShape gloss;
        const std::size_t npt = ribbon.getPointCount();
        gloss.setPointCount(npt);
        for (std::size_t i = 0; i < npt; ++i) {
            const sf::Vector2f p = ribbon.getPoint(i);
            gloss.setPoint(i, ribC + (p - ribC) * 0.84f);
        }
        gloss.setFillColor(ribGloss);
        gloss.setOutlineColor(sf::Color::Transparent);
        gloss.setOutlineThickness(0.f);
        window.draw(gloss, states);
    }

    const float ax = ix + padIn;
    const float aw = iw - padIn * 2.f;
    const float artTop = ribBot + 5.f * s;
    const CardType artKind = (cd && cd->cardType == CardType::Attack) ? CardType::Attack
        : (cd && cd->cardType == CardType::Power) ? CardType::Power
        : CardType::Skill;
    float artH = h * 0.42f;
    if (artKind == CardType::Skill)
        artH = std::max(h * 0.30f, artH - 15.f * s);
    else if (artKind == CardType::Power)
        artH += 10.f * s;  // 能力牌立绘区略长于攻击牌，底弧位置适中
    sf::ConvexShape artPanel;
    if (artKind == CardType::Skill) {
        artPanel.setPointCount(4);
        artPanel.setPoint(0, sf::Vector2f(ax, artTop));
        artPanel.setPoint(1, sf::Vector2f(ax + aw, artTop));
        artPanel.setPoint(2, sf::Vector2f(ax + aw, artTop + artH));
        artPanel.setPoint(3, sf::Vector2f(ax, artTop + artH));
    } else if (artKind == CardType::Attack) {
        const float shoulder = artH * 0.22f;
        const float tipY = artTop + artH;
        const float shoulderY = tipY - shoulder;
        artPanel.setPointCount(5);
        artPanel.setPoint(0, sf::Vector2f(ax, artTop));
        artPanel.setPoint(1, sf::Vector2f(ax + aw, artTop));
        artPanel.setPoint(2, sf::Vector2f(ax + aw, shoulderY));
        artPanel.setPoint(3, sf::Vector2f(ax + aw * 0.5f, tipY));
        artPanel.setPoint(4, sf::Vector2f(ax, shoulderY));
    } else {
        constexpr int kArcSeg = 22;
        // 能力牌：底边弧线（arcInset 控制弧垂，越大底越鼓）
        const float arcInset = std::max(20.f * s, artH * 0.22f);
        // 弧线与左右竖边相接处略下移；数值越小弧线整体越靠上
        const float arcStartLower = 6.f * s;
        const float yMid = artTop + artH;
        const float ySide = std::min(yMid - 2.f * s, artTop + artH - arcInset + arcStartLower);
        artPanel.setPointCount(static_cast<std::size_t>(2 + kArcSeg + 1));
        artPanel.setPoint(0, sf::Vector2f(ax, artTop));
        artPanel.setPoint(1, sf::Vector2f(ax + aw, artTop));
        for (int i = 0; i <= kArcSeg; ++i) {
            const float t = static_cast<float>(kArcSeg - i) / static_cast<float>(kArcSeg);
            const float sn = std::sin(kCardFacePi * t);
            artPanel.setPoint(static_cast<std::size_t>(2 + i),
                              sf::Vector2f(ax + aw * t, ySide + (yMid - ySide) * sn));
        }
    }
    artPanel.setFillColor(curseCard ? sf::Color(58, 22, 62)
                                    : colorlessCard ? sf::Color(92, 92, 96)
                                    : greenCharacterCard ? sf::Color(52, 78, 62)
                                                        : sf::Color(88, 32, 34));
    // 立绘区边框：更粗，稀有度与丝带一致（用丝带主体 fill 色，避免 outline 过深）
    artPanel.setOutlineColor(ribFill);
    artPanel.setOutlineThickness(std::max(3.f, 6.5f * s));
    window.draw(artPanel, states);

    // 立绘：数据驱动（cards.json: art 字段）。为空则不绘制立绘。
    {
        std::string artPath;
        if (cd && !cd->art.empty()) {
            artPath = cd->art;
        } else if (const CardData* baseCd = get_card_by_id(base_id_for_art_fallback(card_id)); baseCd && !baseCd->art.empty()) {
            artPath = baseCd->art;
        }

        if (!artPath.empty()) {
            const std::string key = base_art_key(card_id);
            auto& cache = card_art_cache(); auto it = cache.find(key);
            if (it == cache.end()) {
                sf::Texture tex;
                if (tex.loadFromFile(artPath)) it = cache.emplace(key, std::move(tex)).first;
            }
            if (it != cache.end()) {
                const sf::Vector2u tsz = it->second.getSize();
                if (tsz.x > 0u && tsz.y > 0u) {
                    // 用与 artPanel 相同的凸多边形承载纹理：天然裁剪，图片不会超出形状
                    sf::ConvexShape artImg = artPanel;
                    artImg.setFillColor(sf::Color::White);
                    artImg.setOutlineThickness(0.f);
                    artImg.setTexture(&it->second);

                    // textureRect 采用 cover（填满立绘区，必要时裁切纹理边缘）
                    const sf::FloatRect ab = artImg.getLocalBounds();
                    const float panelAspect = (ab.size.y > 0.f) ? (ab.size.x / ab.size.y) : 1.f;
                    const float texAspect = static_cast<float>(tsz.x) / static_cast<float>(tsz.y);

                    int rx = 0, ry = 0;
                    int rw = static_cast<int>(tsz.x);
                    int rh = static_cast<int>(tsz.y);
                    if (texAspect > panelAspect) {
                        // 纹理更“宽” -> 裁左右
                        rw = static_cast<int>(static_cast<float>(tsz.y) * panelAspect);
                        rx = (static_cast<int>(tsz.x) - rw) / 2;
                    } else if (texAspect < panelAspect) {
                        // 纹理更“高” -> 裁上下
                        rh = static_cast<int>(static_cast<float>(tsz.x) / panelAspect);
                        ry = (static_cast<int>(tsz.y) - rh) / 2;
                    }
                    artImg.setTextureRect(sf::IntRect({rx, ry}, {rw, rh}));
                    window.draw(artImg, states);
                }
            }
        }
    }

    sf::String cardName;
    if (cd && !cd->name.empty())
        cardName = sf::String::fromUtf8(cd->name.begin(), cd->name.end());
    else if (card_id.empty())
        cardName = sf::String(L"?");
    else
        cardName = sf::String(card_id);
    const unsigned namePt = static_cast<unsigned>(std::max(15.f, std::round(31.f * s)));
    sf::Text nameText(fontZh, cardName, namePt);
    const sf::FloatRect nb = nameText.getLocalBounds();
    nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
    const sf::Vector2f namePos(ribbonX + ribbonW * 0.5f, (ribTop + ribBot) * 0.5f + 2.f * s);
    nameText.setPosition(namePos);
    nameText.setFillColor(sf::Color::White);
    window.draw(nameText, states);

    sf::String typeStr = sf::String(L"?");
    if (cd) {
        switch (cd->cardType) {
        case CardType::Attack: typeStr = sf::String(L"攻击"); break;
        case CardType::Skill:  typeStr = sf::String(L"技能"); break;
        case CardType::Power:  typeStr = sf::String(L"能力"); break;
        case CardType::Status: typeStr = sf::String(L"状态"); break;
        case CardType::Curse:  typeStr = sf::String(L"诅咒"); break;
        }
    }
    const unsigned typePt = static_cast<unsigned>(std::max(10.f, std::round(17.f * s)));
    sf::Text typeText(fontZh, typeStr, typePt);
    typeText.setFillColor(curseCard ? sf::Color(30, 28, 36)
                                    : colorlessCard ? sf::Color(48, 48, 52)
                                    : greenCharacterCard ? sf::Color(48, 52, 50)
                                                        : sf::Color(198, 194, 188));
    const float tagCx = ax + aw * 0.5f;
    // 类型条中心压在立绘区底边（平底 / 三角尖端 / 弧底最低点均为 artTop + artH）
    const float artBottomY = artTop + artH;
    const sf::FloatRect tbPre = typeText.getLocalBounds();
    const float pillW = std::max(48.f * s, tbPre.size.x + 20.f * s);
    const float pillH = 22.f * s;
    const float tagCy = artBottomY;
    sf::RectangleShape typePill(sf::Vector2f(pillW, pillH));
    typePill.setOrigin(sf::Vector2f(pillW * 0.5f, pillH * 0.5f));
    typePill.setPosition(sf::Vector2f(tagCx, tagCy));
    typePill.setFillColor(curseCard ? sf::Color(150, 150, 154)
                                    : colorlessCard ? sf::Color(175, 175, 178)
                                    : greenCharacterCard ? sf::Color(118, 124, 118)
                                                        : sf::Color(94, 90, 86));
    typePill.setOutlineColor(curseCard ? sf::Color(90, 90, 98)
                                       : colorlessCard ? sf::Color(110, 110, 118)
                                       : greenCharacterCard ? sf::Color(78, 84, 80)
                                                           : sf::Color(62, 58, 54));
    typePill.setOutlineThickness(std::max(0.5f, 1.f * s));
    window.draw(typePill, states);
    const sf::FloatRect tb = typeText.getLocalBounds();
    typeText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
    typeText.setPosition(sf::Vector2f(tagCx, tagCy));
    window.draw(typeText, states);

    const float descBoxTop = tagCy + pillH * 0.5f + 7.f * s;
    const float descBoxH = (iy + ih) - descBoxTop - 8.f * s;
    if (descBoxH > 18.f * s) {
        const float dPad = 5.f * s;
        build_round_rect_poly(rr, ix + dPad, descBoxTop, iw - dPad * 2.f, descBoxH, std::max(3.f, 5.f * s), 2);
        draw_convex_poly(window, rr,
                         curseCard ? sf::Color(16, 14, 18)
                                  : colorlessCard ? sf::Color(26, 26, 28)
                                  : greenCharacterCard ? sf::Color(24, 34, 28)
                                                      : sf::Color(22, 20, 24),
                         curseCard ? sf::Color(56, 52, 62)
                                  : colorlessCard ? sf::Color(60, 60, 66)
                                  : greenCharacterCard ? sf::Color(48, 62, 52)
                                                      : sf::Color(40, 36, 38),
                         std::max(0.5f, 1.f * s), states);
    }
    sf::String descStr;
    if (cd && !cd->description.empty())
        descStr = sf::String::fromUtf8(cd->description.begin(), cd->description.end());
    const float descX = ix + 11.f * s;
    const float descY = descBoxTop + 8.f * s;
    const float descMaxW = iw - 22.f * s;
    const float descMaxH = std::max(8.f, (iy + ih) - descY - 10.f * s);
    const unsigned descPt = static_cast<unsigned>(std::max(11.f, std::round(23.f * s)));
    draw_wrapped_text(window, fontZh, descStr, descPt,
                      sf::Vector2f(descX, descY), descMaxW, descMaxH,
                      curseCard ? sf::Color(240, 240, 245)
                               : colorlessCard ? sf::Color(236, 236, 236)
                               : greenCharacterCard ? sf::Color(232, 238, 232)
                                                   : sf::Color(218, 212, 204), states);

    const bool handCostPath = (handSnap != nullptr && handInst != nullptr);
    const bool showCostCircle = cd && cd->cost != -2;
    int displayCost = 0;
    if (showCostCircle) {
        displayCost = handCostPath ? effective_hand_card_energy_cost(cd, *handInst, handSnap) : cd->cost;
        // 费用球保持默认红/金；数字仅当「升级版且静态费用低于原版」时为亮绿
        const float costCx = cardX + 22.f * s;
        const float costCy = cardY + 22.f * s;
        const float gemR = 18.5f * s;
        const float ringPad = 5.f * s;
        if (handCostPath) {
            sf::CircleShape costRing(gemR + ringPad);
            costRing.setPosition(sf::Vector2f(costCx - gemR - ringPad, costCy - gemR - ringPad));
            costRing.setFillColor(sf::Color(0, 0, 0, 0));
            costRing.setOutlineColor(curseCard ? sf::Color(210, 195, 225)
                                              : colorlessCard ? sf::Color(222, 222, 228)
                                              : greenCharacterCard ? sf::Color(160, 235, 195)
                                                                  : sf::Color(255, 200, 100));
            costRing.setOutlineThickness(std::max(1.f, 3.f * s));
            window.draw(costRing, states);
        }
        sf::ConvexShape gem;
        gem.setPointCount(8);
        for (int gi = 0; gi < 8; ++gi) {
            const float a = kCardFacePi * 0.125f + static_cast<float>(gi) * (kCardFacePi * 0.25f);
            gem.setPoint(static_cast<std::size_t>(gi),
                         sf::Vector2f(costCx + gemR * std::cos(a), costCy + gemR * std::sin(a)));
        }
        if (curseCard) {
            gem.setFillColor(sf::Color(120, 120, 126));
            gem.setOutlineColor(sf::Color(235, 235, 242));
        } else if (colorlessCard) {
            gem.setFillColor(sf::Color(160, 160, 166));
            gem.setOutlineColor(sf::Color(245, 245, 250));
        } else if (greenCharacterCard) {
            gem.setFillColor(sf::Color(110, 215, 150));
            gem.setOutlineColor(sf::Color(215, 255, 228));
        } else {
            gem.setFillColor(sf::Color(208, 52, 44));
            gem.setOutlineColor(sf::Color(255, 214, 130));
        }
        gem.setOutlineThickness(std::max(1.f, 2.f * s));
        window.draw(gem, states);
        if (displayCost == -1) {
            std::snprintf(buf, sizeof(buf), "X");
        } else {
            std::snprintf(buf, sizeof(buf), "%d", displayCost);
        }
        const unsigned costPt = static_cast<unsigned>(std::max(11.f, std::round(27.f * s)));
        sf::Text costText(fontZh, buf, costPt);
        costText.setFillColor(upgradedStaticFeeLower ? kUpgradeFeeDownGreen : sf::Color::White);
        const sf::FloatRect cb = costText.getLocalBounds();
        costText.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
        costText.setPosition(sf::Vector2f(costCx, costCy));
        window.draw(costText, states);
    }
}

} // namespace tce
