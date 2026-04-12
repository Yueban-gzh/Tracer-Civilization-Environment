#include "GameFlow/CardCatalogScreen.hpp"

#include <algorithm>
#include <array>

#include "BattleEngine/BattleStateSnapshot.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "DataLayer/DataLayer.hpp"

namespace tce {

namespace {

bool load_battle_ui_fonts_for_catalog(BattleUI& ui) {
    bool ok = false;
    // 主字体（英文/数字）兜底：assets -> data -> Windows 字体
    ok = ui.loadFont("assets/fonts/Sanji.ttf") || ok;
    ok = ui.loadFont("assets/fonts/default.ttf") || ok;
    ok = ui.loadFont("data/font.ttf") || ok;
    ok = ui.loadFont("C:/Windows/Fonts/arial.ttf") || ok;

    // 中文字体兜底：assets -> Windows
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
        (void)ui.loadChineseFont("assets/fonts/simkai.ttf");
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                (void)ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
            }
        }
    }
    return ok;
}

bool load_ui_font_for_catalog(sf::Font& font) {
    if (font.openFromFile("assets/fonts/Sanji.ttf")) return true;
    if (font.openFromFile("data/font.ttf")) return true;
    if (font.openFromFile("C:/Windows/Fonts/msyh.ttc")) return true;
    if (font.openFromFile("C:/Windows/Fonts/simhei.ttf")) return true;
    if (font.openFromFile("C:/Windows/Fonts/simsun.ttc")) return true;
    return false;
}

} // namespace

void runCardCatalogScreen(sf::RenderWindow& window) {
    const sf::Vector2u sz = window.getSize();
    BattleUI ui(sz.x, sz.y);
    load_battle_ui_fonts_for_catalog(ui);
    ui.set_hide_top_right_map_button(true);

    // 基础卡池：不显示升级版（id 以 '+' 结尾）
    std::vector<CardId> baseIds = get_all_card_ids();
    baseIds.erase(std::remove_if(baseIds.begin(), baseIds.end(),
                    [](const CardId& id) { return !id.empty() && id.back() == '+'; }),
                 baseIds.end());

    enum class SortKey { Rarity, Type, Cost };
    SortKey sortKey = SortKey::Rarity;
    bool sortAsc = true;

    enum class CatalogColorFilter { Red, Green, Colorless, Curse };
    CatalogColorFilter colorFilter = CatalogColorFilter::Red;

    sf::Font uiFont;
    (void)load_ui_font_for_catalog(uiFont);

    auto cardColorMatches = [](CardColor c, CatalogColorFilter f) -> bool {
        switch (f) {
        case CatalogColorFilter::Red: return c == CardColor::Red;
        case CatalogColorFilter::Green: return c == CardColor::Green;
        case CatalogColorFilter::Colorless: return c == CardColor::Colorless;
        case CatalogColorFilter::Curse: return c == CardColor::Curse;
        }
        return false;
    };

    constexpr float kTopPad = 16.f;
    constexpr float kBtnH = 44.f;
    constexpr float kSortBtnW = 170.f;
    constexpr float kGap = 14.f;
    constexpr float kFilterRowGap = 10.f;
    constexpr float kFilterBtnW = 132.f;
    constexpr float kFilterGap = 10.f;
    const float kFilterRowY = kTopPad + kBtnH + kFilterRowGap;
    /** 第二行筛选条占用高度：与 BattleUI standalone 网格下移量一致，避免压住牌格 */
    constexpr float kCatalogStandaloneNudge = kBtnH + kFilterRowGap;

    auto rebuildCards = [&]() {
        std::vector<CardId> ids = baseIds;
        ids.erase(std::remove_if(ids.begin(), ids.end(), [&](const CardId& id) {
            const CardData* cd = get_card_by_id(id);
            if (!cd) return true;
            return !cardColorMatches(cd->color, colorFilter);
        }), ids.end());
        auto rarityRank = [](Rarity r) {
            switch (r) {
            case Rarity::Common: return 0;
            case Rarity::Uncommon: return 1;
            case Rarity::Rare: return 2;
            case Rarity::Special: return 3;
            default: return 0;
            }
        };
        auto typeRank = [](CardType t) {
            switch (t) {
            case CardType::Attack: return 0;
            case CardType::Skill:  return 1;
            case CardType::Power:  return 2;
            case CardType::Status: return 3;
            case CardType::Curse:  return 4;
            default: return 5;
            }
        };
        auto costRank = [](int c) {
            if (c == -2) return 999; // 不可打出放最后
            if (c < 0) return 99;    // X 等
            return c;
        };
        std::stable_sort(ids.begin(), ids.end(), [&](const CardId& a, const CardId& b) {
            const CardData* ca = get_card_by_id(a);
            const CardData* cb = get_card_by_id(b);
            if (!ca || !cb) return a < b;
            int ka = 0, kb = 0;
            switch (sortKey) {
            case SortKey::Rarity:
                ka = rarityRank(ca->rarity); kb = rarityRank(cb->rarity); break;
            case SortKey::Type:
                ka = typeRank(ca->cardType); kb = typeRank(cb->cardType); break;
            case SortKey::Cost:
                ka = costRank(ca->cost); kb = costRank(cb->cost); break;
            }
            if (ka != kb) return sortAsc ? (ka < kb) : (ka > kb);
            return a < b;
        });

        std::vector<CardInstance> cards;
        cards.reserve(ids.size());
        InstanceId next = 1;
        for (const auto& id : ids) {
            CardInstance c;
            c.instanceId = next++;
            c.id = id;
            cards.push_back(std::move(c));
        }
        ui.set_deck_view_cards(std::move(cards));
    };

    rebuildCards();
    ui.set_deck_view_standalone_grid_layout(true); // 与 drawDeckViewOnly 同几何，否则点击/滚轮仍按战斗牌组布局会整体偏下
    ui.set_deck_view_standalone_vertical_nudge(kCatalogStandaloneNudge);
    ui.set_deck_view_active(true);

    BattleStateSnapshot snap{};
    snap.currentHp = 1;
    snap.maxHp = 1;
    snap.stance = Stance::Neutral;
    snap.potionSlotCount = 0;
    snap.turnNumber = 0;

    while (window.isOpen() && ui.is_deck_view_active()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                return;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    ui.set_deck_view_active(false);
                    break;
                }
            }

            sf::Vector2f mp;
            if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mp = window.mapPixelToCoords(m2->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                mp = window.mapPixelToCoords(mr->position);
            } else {
                mp = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            }
            ui.setMousePosition(mp);

            const sf::FloatRect rBtn(sf::Vector2f(20.f, kTopPad), sf::Vector2f(kSortBtnW, kBtnH));
            const sf::FloatRect tBtn(sf::Vector2f(20.f + (kSortBtnW + kGap), kTopPad), sf::Vector2f(kSortBtnW, kBtnH));
            const sf::FloatRect cBtn(sf::Vector2f(20.f + 2.f * (kSortBtnW + kGap), kTopPad), sf::Vector2f(kSortBtnW, kBtnH));
            const std::array<sf::FloatRect, 4> filterBtns = {
                sf::FloatRect(sf::Vector2f(20.f + 0.f * (kFilterBtnW + kFilterGap), kFilterRowY),
                              sf::Vector2f(kFilterBtnW, kBtnH)),
                sf::FloatRect(sf::Vector2f(20.f + 1.f * (kFilterBtnW + kFilterGap), kFilterRowY),
                              sf::Vector2f(kFilterBtnW, kBtnH)),
                sf::FloatRect(sf::Vector2f(20.f + 2.f * (kFilterBtnW + kFilterGap), kFilterRowY),
                              sf::Vector2f(kFilterBtnW, kBtnH)),
                sf::FloatRect(sf::Vector2f(20.f + 3.f * (kFilterBtnW + kFilterGap), kFilterRowY),
                              sf::Vector2f(kFilterBtnW, kBtnH)),
            };

            if (const auto* mpress = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mpress->button == sf::Mouse::Button::Left) {
                    auto toggleSort = [&](SortKey k) {
                        if (sortKey == k) sortAsc = !sortAsc;
                        else { sortKey = k; sortAsc = true; }
                        rebuildCards();
                    };
                    if (rBtn.contains(mp)) { toggleSort(SortKey::Rarity); continue; }
                    if (tBtn.contains(mp)) { toggleSort(SortKey::Type); continue; }
                    if (cBtn.contains(mp)) { toggleSort(SortKey::Cost); continue; }
                    if (filterBtns[0].contains(mp)) {
                        colorFilter = CatalogColorFilter::Red;
                        rebuildCards();
                        continue;
                    }
                    if (filterBtns[1].contains(mp)) {
                        colorFilter = CatalogColorFilter::Green;
                        rebuildCards();
                        continue;
                    }
                    if (filterBtns[2].contains(mp)) {
                        colorFilter = CatalogColorFilter::Colorless;
                        rebuildCards();
                        continue;
                    }
                    if (filterBtns[3].contains(mp)) {
                        colorFilter = CatalogColorFilter::Curse;
                        rebuildCards();
                        continue;
                    }
                }
            }

            ui.handleEvent(*ev, mp);
        }

        sf::Vector2f mp = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        ui.setMousePosition(mp);

        window.clear(sf::Color(18, 16, 24));

        auto drawSortBtn = [&](float x, const std::wstring& label, bool active) {
            sf::RectangleShape b(sf::Vector2f(kSortBtnW, kBtnH));
            b.setPosition(sf::Vector2f(x, kTopPad));
            b.setFillColor(active ? sf::Color(92, 86, 110) : sf::Color(58, 54, 72));
            b.setOutlineColor(sf::Color(200, 190, 150));
            b.setOutlineThickness(active ? 2.5f : 2.f);
            window.draw(b);
            sf::Text t(uiFont, sf::String(label), 20);
            t.setFillColor(sf::Color(245, 240, 235));
            const sf::FloatRect tb = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
            t.setPosition(sf::Vector2f(x + kSortBtnW * 0.5f, kTopPad + kBtnH * 0.5f));
            window.draw(t);
        };
        auto drawFilterBtn = [&](float x, const std::wstring& label, bool active) {
            sf::RectangleShape b(sf::Vector2f(kFilterBtnW, kBtnH));
            b.setPosition(sf::Vector2f(x, kFilterRowY));
            b.setFillColor(active ? sf::Color(110, 78, 72) : sf::Color(52, 48, 62));
            b.setOutlineColor(active ? sf::Color(230, 175, 120) : sf::Color(160, 150, 130));
            b.setOutlineThickness(active ? 2.5f : 2.f);
            window.draw(b);
            sf::Text t(uiFont, sf::String(label), 18);
            t.setFillColor(sf::Color(248, 242, 235));
            const sf::FloatRect tb = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
            t.setPosition(sf::Vector2f(x + kFilterBtnW * 0.5f, kFilterRowY + kBtnH * 0.5f));
            window.draw(t);
        };
        const auto arrow = sortAsc ? L"↑" : L"↓";
        drawSortBtn(20.f, std::wstring(L"稀有度") + arrow, sortKey == SortKey::Rarity);
        drawSortBtn(20.f + (kSortBtnW + kGap), std::wstring(L"类型") + arrow, sortKey == SortKey::Type);
        drawSortBtn(20.f + 2.f * (kSortBtnW + kGap), std::wstring(L"耗费") + arrow, sortKey == SortKey::Cost);

        drawFilterBtn(20.f + 0.f * (kFilterBtnW + kFilterGap), L"红色牌", colorFilter == CatalogColorFilter::Red);
        drawFilterBtn(20.f + 1.f * (kFilterBtnW + kFilterGap), L"绿色牌", colorFilter == CatalogColorFilter::Green);
        drawFilterBtn(20.f + 2.f * (kFilterBtnW + kFilterGap), L"无色牌", colorFilter == CatalogColorFilter::Colorless);
        drawFilterBtn(20.f + 3.f * (kFilterBtnW + kFilterGap), L"诅咒牌", colorFilter == CatalogColorFilter::Curse);

        ui.drawDeckViewOnly(window, snap);
        window.display();
    }
}

} // namespace tce

