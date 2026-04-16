#include "GameFlow/GameFlowController.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

#include "BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.hpp"
#include "BattleEngine/BattleStateSnapshot.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#include "BattleEngine/MonsterBehaviors.hpp"
#include "CardSystem/DeckViewCollection.hpp"
#include "Cheat/CheatEngine.hpp"
#include "Cheat/CheatPanel.hpp"
#include "DataLayer/DataLayer.hpp"
#include "Common/ImagePath.hpp"
#include "Common/UserSettings.hpp"
#include "DataLayer/JsonParser.h"
#include "Effects/CardEffects.hpp"
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include "EventEngine/TreasureRoomUI.hpp"
#include "GameFlow/CharacterSelectScreen.hpp"

namespace tce {

namespace {

const sf::ContextSettings kGameUiContext = [] {
    sf::ContextSettings c;
    c.antiAliasingLevel = 2u;
    return c;
}();

// 旧存档或旧 id 与资源文件名不一致时，用右侧文件名再试一次加载纹理（仍按逻辑 id 缓存）
const std::unordered_map<std::string, std::string> kMonsterImageFileAliases = {
    {"1.2_liejianshush", "1.2_liejianshusheng"},
    {"1.2_liejianshushung", "1.2_liejianshusheng"},
    {"1.9_nongjihuijing", "1.9_nongjihuijin"},
    {"2.7_tongjingsuizhuang", "2.7_tongjingsuizhaung"},
};

int randomIndex(RunRng& rng, int boundExclusive) {
    if (boundExclusive <= 0) return 0;
    return rng.uniform_int(0, boundExclusive - 1);
}

bool is_curse_card_id(const CardId& id) {
    const CardData* cd = get_card_by_id(id);
    if (!cd) return false;
    return (cd->cardType == CardType::Curse || cd->color == CardColor::Curse);
}

std::vector<CardId> event_curse_base_pool() {
    std::vector<CardId> out;
    std::unordered_set<CardId> dedup;
    for (const CardId& id : get_all_card_ids()) {
        if (id.empty() || id.back() == '+') continue;
        if (!is_curse_card_id(id)) continue;
        if (dedup.insert(id).second) out.push_back(id);
    }
    return out;
}

CardId event_pick_random_curse(RunRng& rng) {
    std::vector<CardId> pool = event_curse_base_pool();
    if (pool.empty()) return "parasite"; // 极端兜底：数据缺失时仍可继续流程
    return pool[static_cast<size_t>(randomIndex(rng, static_cast<int>(pool.size())))];
}

bool shop_card_buyable_base(const CardData* cd) {
    if (!cd) return false;
    if (!cd->id.empty() && cd->id.back() == '+') return false;
    if (cd->cardType == CardType::Status || cd->cardType == CardType::Curse) return false;
    if (cd->unplayable) return false;
    if (cd->irremovableFromDeck) return false;
    if (cd->rarity == Rarity::Special) return false;
    return true;
}

CardColor shop_class_color(const std::string& character) {
    if (character == "Silent") return CardColor::Green;
    return CardColor::Red;
}

std::vector<CardId> shop_pool_for_color_rarity(CardColor col, Rarity r) {
    std::vector<CardId> out;
    for (const CardId& id : get_all_card_ids()) {
        const CardData* cd = get_card_by_id(id);
        if (!shop_card_buyable_base(cd)) continue;
        if (cd->color != col) continue;
        if (cd->rarity != r) continue;
        out.push_back(id);
    }
    return out;
}

std::vector<CardId> shop_colorless_pool_rarity(Rarity r) {
    std::vector<CardId> out;
    for (const CardId& id : get_all_card_ids()) {
        const CardData* cd = get_card_by_id(id);
        if (!shop_card_buyable_base(cd)) continue;
        if (cd->color != CardColor::Colorless) continue;
        if (cd->rarity != r) continue;
        out.push_back(id);
    }
    return out;
}

CardId shop_pick_from_pool_excluding(std::vector<CardId> pool, const std::unordered_set<CardId>& used, RunRng& rng) {
    const auto isUsed = [&used](const CardId& id) { return used.count(id) > 0; };
    pool.erase(std::remove_if(pool.begin(), pool.end(), isUsed), pool.end());
    if (pool.empty()) return {};
    return pool[static_cast<size_t>(randomIndex(rng, static_cast<int>(pool.size())))];
}

void shop_degrade_rarity(Rarity& r) {
    if (r == Rarity::Rare)
        r = Rarity::Uncommon;
    else if (r == Rarity::Uncommon)
        r = Rarity::Common;
}

Rarity shop_roll_colored_row_rarity(RunRng& rng) {
    const int x = randomIndex(rng, 100);
    if (x < 52) return Rarity::Common;
    if (x < 87) return Rarity::Uncommon;
    return Rarity::Rare;
}

Rarity shop_roll_colorless_row_rarity(RunRng& rng) {
    const int x = randomIndex(rng, 100);
    if (x < 40) return Rarity::Common;
    if (x < 85) return Rarity::Uncommon;
    return Rarity::Rare;
}

int shop_price_for_card(const CardData& cd, RunRng& rng, int colorlessExtra) {
    const int j = rng.uniform_int(-3, 3);
    int base = 50;
    switch (cd.rarity) {
    case Rarity::Common: base = 49 + j; break;
    case Rarity::Uncommon: base = 74 + j; break;
    case Rarity::Rare: base = 143 + rng.uniform_int(-8, 8); break;
    default: base = 50 + j; break;
    }
    base += colorlessExtra;
    return std::max(25, base);
}

CardId shop_pick_colored_offer(CardColor classColor, std::unordered_set<CardId>& used, RunRng& rng) {
    Rarity r = shop_roll_colored_row_rarity(rng);
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::vector<CardId> pool = shop_pool_for_color_rarity(classColor, r);
        const CardId id = shop_pick_from_pool_excluding(std::move(pool), used, rng);
        if (!id.empty()) return id;
        shop_degrade_rarity(r);
    }
    std::vector<CardId> pool = shop_pool_for_color_rarity(classColor, Rarity::Common);
    return shop_pick_from_pool_excluding(std::move(pool), used, rng);
}

CardId shop_pick_colorless_offer(std::unordered_set<CardId>& used, RunRng& rng) {
    Rarity r = shop_roll_colorless_row_rarity(rng);
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::vector<CardId> pool = shop_colorless_pool_rarity(r);
        const CardId id = shop_pick_from_pool_excluding(std::move(pool), used, rng);
        if (!id.empty()) return id;
        shop_degrade_rarity(r);
    }
    return shop_pick_from_pool_excluding(shop_colorless_pool_rarity(Rarity::Common), used, rng);
}

void fill_random_shop_card_offers(ShopDisplayData& shop, const std::string& character, RunRng& rng) {
    shop.forSale.clear();
    shop.colorlessForSale.clear();
    const CardColor classColor = shop_class_color(character);
    std::unordered_set<CardId> used;
    for (int i = 0; i < 5; ++i) {
        const CardId id = shop_pick_colored_offer(classColor, used, rng);
        if (id.empty()) continue;
        used.insert(id);
        ShopCardOffer c;
        c.id = id;
        const CardData* cd = get_card_by_id(id);
        c.price = cd ? shop_price_for_card(*cd, rng, 0) : 50;
        c.name = cd ? esr_detail::utf8_to_wstring(cd->name) : esr_detail::utf8_to_wstring(id);
        shop.forSale.push_back(std::move(c));
    }
    for (int i = 0; i < 2; ++i) {
        const CardId id = shop_pick_colorless_offer(used, rng);
        if (id.empty()) continue;
        used.insert(id);
        ShopCardOffer c;
        c.id = id;
        const CardData* cd = get_card_by_id(id);
        const int extra = rng.uniform_int(10, 24);
        c.price = cd ? shop_price_for_card(*cd, rng, extra) : (75 + extra);
        c.name = cd ? esr_detail::utf8_to_wstring(cd->name) : esr_detail::utf8_to_wstring(id);
        shop.colorlessForSale.push_back(std::move(c));
    }
}

CardId pick_map_merchant_random_card(const std::string& character, RunRng& rng) {
    const CardColor classColor = shop_class_color(character);
    const int x = randomIndex(rng, 100);
    Rarity r = Rarity::Common;
    if (x >= 72 && x < 94) r = Rarity::Uncommon;
    else if (x >= 94) r = Rarity::Rare;
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::vector<CardId> pool = shop_pool_for_color_rarity(classColor, r);
        if (!pool.empty())
            return pool[static_cast<size_t>(randomIndex(rng, static_cast<int>(pool.size())))];
        shop_degrade_rarity(r);
    }
    std::vector<CardId> fallback = shop_pool_for_color_rarity(classColor, Rarity::Common);
    if (!fallback.empty()) return fallback[0];
    return classColor == CardColor::Green ? CardId{"strike_green"} : CardId{"strike"};
}

std::string join_names_cn(const std::vector<std::string>& names) {
    if (names.empty()) return "无";
    std::string out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out += "、";
        out += names[i];
    }
    return out;
}

std::string potion_name_cn(const std::string& id) {
    if (id == "block_potion") return "砌墙灵液";
    if (id == "strength_potion") return "蛮力灵液";
    if (id == "poison_potion") return "淬毒灵液";
    return id;
}

std::string relic_name_cn(const std::string& id) {
    if (id == "burning_blood") return "燃烧之血";
    if (id == "ring_of_the_snake") return "蛇之戒";
    if (id == "marble_bag") return "弹珠袋";
    if (id == "small_blood_vial") return "小血瓶";
    if (id == "copper_scales") return "铜制鳞片";
    if (id == "centennial_puzzle") return "百年积木";
    if (id == "clockwork_boots") return "发条靴";
    if (id == "happy_flower") return "开心小花";
    if (id == "lantern") return "灯笼";
    if (id == "smooth_stone") return "意外光滑的石头";
    if (id == "orichalcum") return "奥利哈钢";
    if (id == "red_skull") return "红头骨";
    if (id == "snake_skull") return "异蛇头骨";
    if (id == "strawberry") return "草莓";
    if (id == "potion_belt") return "灵液腰带";
    if (id == "vajra") return "金刚杵";
    if (id == "nunchaku") return "双截棍";
    if (id == "ceramic_fish") return "陶瓷小鱼";
    if (id == "hand_drum") return "手摇鼓";
    if (id == "pen_nib") return "钢笔尖";
    if (id == "toy_ornithopter") return "玩具扑翼飞机";
    if (id == "preparation_pack") return "准备背包";
    if (id == "anchor") return "锚";
    if (id == "art_of_war") return "孙子兵法";
    if (id == "relic_strength_plus") return "力量遗物";
    return id;
}

std::string read_debug_event_override_id() {
    namespace fs = std::filesystem;
    const fs::path p("data/debug_event_id.txt");
    if (!fs::exists(p)) return "";
    std::ifstream in(p, std::ios::binary);
    if (!in) return "";
    std::string line;
    std::getline(in, line);
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' || line.back() == '\t')) {
        line.pop_back();
    }
    size_t start = 0;
    while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) ++start;
    return (start < line.size()) ? line.substr(start) : std::string();
}

static std::string gfc_join_path(const std::string& dir, const std::string& rel) {
    if (dir.empty()) return rel;
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) return rel;
    const char last = dir.back();
    if (last == '/' || last == '\\') return dir + rel;
    return dir + "/" + rel;
}

/** 从 data/intent_icon_map.json 预载意图贴图（逻辑键 -> assets/intention/ 下无扩展名文件名）；失败则回退英文文件名。 */
static bool preload_intention_icons_from_data_map(BattleUI& ui) {
    const char* const kFile = "intent_icon_map.json";
    std::vector<std::string> candidates = {
        std::string("data/") + kFile,
        std::string("./data/") + kFile,
        std::string("../data/") + kFile,
        std::string("../../data/") + kFile,
        std::string("../../../data/") + kFile,
    };
    if (const std::string exeDir = tce::get_executable_directory_utf8(); !exeDir.empty()) {
        candidates.push_back(gfc_join_path(exeDir, std::string("data/") + kFile));
        candidates.push_back(gfc_join_path(exeDir, std::string("../data/") + kFile));
        candidates.push_back(gfc_join_path(exeDir, std::string("../../data/") + kFile));
    }
    DataLayer::JsonValue root;
    for (const std::string& p : candidates) {
        root = DataLayer::parse_json_file(p);
        if (root.is_object() && !root.obj.empty())
            break;
        root = DataLayer::JsonValue();
    }
    if (!root.is_object() || root.obj.empty())
        return false;
    int n = 0;
    for (const auto& kv : root.obj) {
        if (kv.first.empty() || kv.first[0] == '_')
            continue;
        if (!kv.second.is_string())
            continue;
        const std::string stem = kv.second.as_string();
        if (stem.empty())
            continue;
        const std::string base = "assets/intention/" + stem;
        if (const std::string path = resolve_image_path(base); !path.empty()) {
            if (ui.loadIntentionTexture(kv.first, path))
                ++n;
        }
    }
    return n > 0;
}

/** 预加载战斗 UI：玩家立绘、遗物/灵液、怪物意图图标（支持 .png / .jpg / .jpeg）。 */
void preload_battle_ui_assets(BattleUI& ui, const std::string& character_id) {
    if (const std::string playerPath = resolve_image_path("assets/player/" + character_id); !playerPath.empty()) {
        ui.loadPlayerTexture(character_id, playerPath);
    }
    if (!preload_intention_icons_from_data_map(ui)) {
        // 与 BattleUI::drawBattleCenter 中 iconLookup 一致；无映射表或解析失败时使用英文主文件名
        static const char* const kIntentionBases[] = {
            "assets/intention/Attack",
            "assets/intention/Block",
            "assets/intention/Strategy",
            "assets/intention/Unknown",
            "assets/intention/AttackBlock",
            "assets/intention/Ritual",
            "assets/intention/Debuff",
            "assets/intention/SmallKnife",
        };
        for (const char* base : kIntentionBases) {
            const std::string baseStr(base);
            if (const std::string path = resolve_image_path(baseStr); !path.empty()) {
                std::string key = baseStr;
                const size_t slash = key.find_last_of("/\\");
                if (slash != std::string::npos) key.erase(0, slash + 1);
                ui.loadIntentionTexture(key, path);
            }
        }
    }
    static const std::vector<std::string> kRelics = {
        "akabeko", "anchor", "art_of_war", "burning_blood", "centennial_puzzle", "ceramic_fish",
        "clockwork_boots", "copper_scales", "data_disk", "hand_drum", "happy_flower", "lantern",
        "marble_bag", "nunchaku", "orichalcum", "pen_nib", "potion_belt", "preparation_pack",
        "red_skull", "small_blood_vial", "smooth_stone", "snake_skull", "strawberry",
        "toy_ornithopter", "vajra", "relic_strength_plus"
    };
    static const std::vector<std::string> kPotions = {
        "attack_potion", "block_potion", "blood_potion", "dexterity_potion", "energy_potion",
        "explosion_potion", "fear_potion", "fire_potion", "focus_potion", "poison_potion",
        "speed_potion", "steroid_potion", "strength_potion", "swift_potion", "weak_potion"
    };
    for (const auto& rid : kRelics) {
        if (const std::string path = resolve_image_path("assets/relics/" + rid); !path.empty()) {
            ui.loadRelicTexture(rid, path);
        }
    }
    for (const auto& pid : kPotions) {
        if (const std::string path = resolve_image_path("assets/potions/" + pid); !path.empty()) {
            ui.loadPotionTexture(pid, path);
        }
    }
}

/** 战斗结束全屏遮罩：victory=true 为通关界面，false 为失败界面（视觉与文案不同）。 */
void runPostBattleExitOverlay(sf::RenderWindow& window,
                              BattleUI& ui,
                              const BattleState& state,
                              CardSystem& cardSystem,
                              bool victory,
                              const sf::Font& hudFont,
                              bool hudFontLoaded,
                              int top_bar_map_layer,
                              int top_bar_map_total) {
    constexpr float kFadeInSec            = 1.0f;
    constexpr float kShowButtonAfterSec   = 1.1f;
    constexpr float kAutoReturnSec        = 3.2f;
    sf::Clock clock;

    const float w = static_cast<float>(window.getSize().x);
    const float h = static_cast<float>(window.getSize().y);
    const sf::Vector2f center(w * 0.5f, h * 0.5f);

    const float btnW = 260.f;
    const float btnH = 56.f;
    const sf::FloatRect backRect(
        sf::Vector2f(center.x - btnW * 0.5f, center.y + 120.f),
        sf::Vector2f(btnW, btnH));

    bool leaving = false;
    while (window.isOpen() && !leaving) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window.close();
                leaving = true;
                break;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    leaving = true;
                    break;
                }
            }
            if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (m->button == sf::Mouse::Button::Left) {
                    const sf::Vector2f p = window.mapPixelToCoords(m->position);
                    const float t = clock.getElapsedTime().asSeconds();
                    if (t >= kShowButtonAfterSec && backRect.contains(p)) {
                        leaving = true;
                        break;
                    }
                }
            }
        }

        const float t = clock.getElapsedTime().asSeconds();
        if (t >= kAutoReturnSec) leaving = true;
        const sf::Vector2f mouseWorld = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        const bool hoverBack = (t >= kShowButtonAfterSec) && backRect.contains(mouseWorld);

        window.clear(sf::Color(12, 10, 18));

        BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(state, &cardSystem);
        tce::SnapshotBattleUIDataProvider adapter(&snapshot);
        ui.set_top_bar_map_floor(top_bar_map_layer, top_bar_map_total);
        ui.draw(window, adapter);

        float a01 = (kFadeInSec <= 0.f) ? 1.f : std::min(1.f, std::max(0.f, t / kFadeInSec));
        const std::uint8_t a = static_cast<std::uint8_t>((victory ? 220.f : 196.f) * a01);
        sf::RectangleShape dim(sf::Vector2f(w, h));
        dim.setPosition(sf::Vector2f(0.f, 0.f));
        dim.setFillColor(sf::Color(0, 0, 0, a));
        window.draw(dim);

        if (!victory) {
            // 失败页：弱面板 + 细金线，强调庄重收束而非高亮刺激。
            sf::RectangleShape centerMist(sf::Vector2f(w * 0.44f, h * 0.42f));
            centerMist.setOrigin(sf::Vector2f(centerMist.getSize().x * 0.5f, centerMist.getSize().y * 0.5f));
            centerMist.setPosition(sf::Vector2f(center.x, center.y - 18.f));
            centerMist.setFillColor(sf::Color(14, 16, 24, static_cast<std::uint8_t>(76.f * a01)));
            window.draw(centerMist);

            sf::RectangleShape topLine(sf::Vector2f(250.f, 1.f));
            topLine.setOrigin(sf::Vector2f(topLine.getSize().x * 0.5f, 0.f));
            topLine.setPosition(sf::Vector2f(center.x, center.y - 86.f));
            topLine.setFillColor(sf::Color(176, 144, 92, static_cast<std::uint8_t>(150.f * a01)));
            window.draw(topLine);
        }

        if (hudFontLoaded) {
            if (victory) {
                sf::RectangleShape centerMist(sf::Vector2f(w * 0.44f, h * 0.42f));
                centerMist.setOrigin(sf::Vector2f(centerMist.getSize().x * 0.5f, centerMist.getSize().y * 0.5f));
                centerMist.setPosition(sf::Vector2f(center.x, center.y - 18.f));
                centerMist.setFillColor(sf::Color(14, 16, 24, static_cast<std::uint8_t>(74.f * a01)));
                window.draw(centerMist);

                sf::RectangleShape topLine(sf::Vector2f(250.f, 1.f));
                topLine.setOrigin(sf::Vector2f(topLine.getSize().x * 0.5f, 0.f));
                topLine.setPosition(sf::Vector2f(center.x, center.y - 86.f));
                topLine.setFillColor(sf::Color(176, 150, 96, static_cast<std::uint8_t>(155.f * a01)));
                window.draw(topLine);

                sf::Text title(hudFont, sf::String(L"通关成功"), 78);
                title.setLetterSpacing(1.1f);
                title.setFillColor(sf::Color(236, 220, 178, static_cast<std::uint8_t>(255.f * a01)));
                title.setOutlineColor(sf::Color(28, 24, 16, static_cast<std::uint8_t>(224.f * a01)));
                title.setOutlineThickness(2.f);
                const sf::FloatRect tb = title.getLocalBounds();
                title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
                title.setPosition(sf::Vector2f(center.x, center.y - 106.f));
                window.draw(title);

                sf::Text line2(hudFont, sf::String(L"三图 Boss 已全部击败"), 30);
                line2.setFillColor(sf::Color(212, 204, 178, static_cast<std::uint8_t>(225.f * a01)));
                const sf::FloatRect l2 = line2.getLocalBounds();
                line2.setOrigin(sf::Vector2f(l2.position.x + l2.size.x * 0.5f, l2.position.y + l2.size.y * 0.5f));
                line2.setPosition(sf::Vector2f(center.x, center.y - 34.f));
                window.draw(line2);

                sf::Text sub(hudFont, sf::String(L"按 Esc 或点击下方返回主界面"), 23);
                sub.setFillColor(sf::Color(196, 186, 164, static_cast<std::uint8_t>(206.f * a01)));
                const sf::FloatRect sb = sub.getLocalBounds();
                sub.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
                sub.setPosition(sf::Vector2f(center.x, center.y + 8.f));
                window.draw(sub);
            } else {
                sf::Text title(hudFont, sf::String(L"战斗失败"), 78);
                title.setLetterSpacing(1.12f);
                title.setFillColor(sf::Color(224, 206, 170, static_cast<std::uint8_t>(252.f * a01)));
                title.setOutlineColor(sf::Color(30, 22, 16, static_cast<std::uint8_t>(224.f * a01)));
                title.setOutlineThickness(2.f);
                const sf::FloatRect tb = title.getLocalBounds();
                title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
                title.setPosition(sf::Vector2f(center.x, center.y - 106.f));
                window.draw(title);

                sf::Text line1(hudFont, sf::String(L"此卷暂止，文脉未绝"), 30);
                line1.setFillColor(sf::Color(210, 198, 176, static_cast<std::uint8_t>(224.f * a01)));
                const sf::FloatRect l1 = line1.getLocalBounds();
                line1.setOrigin(sf::Vector2f(l1.position.x + l1.size.x * 0.5f, l1.position.y + l1.size.y * 0.5f));
                line1.setPosition(sf::Vector2f(center.x, center.y - 34.f));
                window.draw(line1);

                sf::Text sub(hudFont, sf::String(L"按 Esc 或点击下方返回主界面"), 23);
                sub.setFillColor(sf::Color(196, 184, 166, static_cast<std::uint8_t>(205.f * a01)));
                const sf::FloatRect sb = sub.getLocalBounds();
                sub.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
                sub.setPosition(sf::Vector2f(center.x, center.y + 8.f));
                window.draw(sub);
            }
        }

        if (t >= kShowButtonAfterSec) {
            sf::RectangleShape btn(sf::Vector2f(backRect.size.x, backRect.size.y));
            btn.setPosition(backRect.position);
            if (victory) {
                btn.setFillColor(hoverBack ? sf::Color(44, 56, 78, 236) : sf::Color(34, 42, 62, 228));
                btn.setOutlineColor(hoverBack ? sf::Color(224, 190, 120, 240) : sf::Color(170, 140, 92, 220));
            } else {
                btn.setFillColor(hoverBack ? sf::Color(42, 52, 76, 235) : sf::Color(34, 42, 62, 228));
                btn.setOutlineColor(hoverBack ? sf::Color(222, 186, 118, 240) : sf::Color(164, 136, 90, 220));
            }
            btn.setOutlineThickness(hoverBack ? 2.6f : 2.1f);
            window.draw(btn);

            {
                sf::RectangleShape hl(sf::Vector2f(backRect.size.x - 18.f, backRect.size.y * 0.44f));
                hl.setOrigin(sf::Vector2f(hl.getSize().x * 0.5f, 0.f));
                hl.setPosition(sf::Vector2f(backRect.position.x + backRect.size.x * 0.5f, backRect.position.y + 4.f));
                hl.setFillColor(sf::Color(255, 255, 255, hoverBack ? 22 : 14));
                window.draw(hl);
            }

            if (hudFontLoaded) {
                sf::Text label(hudFont, sf::String(L"返回主界面"), 26);
                label.setFillColor(hoverBack ? sf::Color(245, 236, 216) : sf::Color(230, 222, 204));
                const sf::FloatRect lb = label.getLocalBounds();
                label.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                label.setPosition(sf::Vector2f(backRect.position.x + backRect.size.x * 0.5f,
                                               backRect.position.y + backRect.size.y * 0.5f));
                window.draw(label);
            }
        }

        window.display();
    }
}
} // namespace

GameFlowController::GameFlowController(sf::RenderWindow& window)
    : window_(window)
    , runRng_(static_cast<uint64_t>(std::time(nullptr)))
    , cardSystem_([](CardId id) { return get_card_by_id(id); }, &runRng_)
    , battleEngine_(
        cardSystem_,
        [](MonsterId id) { return get_monster_by_id(id); },
        [](CardId id) { return get_card_by_id(id); },
        execute_monster_behavior,
        &runRng_)
    , eventEngine_(
        [this](EventEngine::EventId id) { return dataLayer_.get_event_by_id(id); },
        [this](CardId id) { cardSystem_.add_to_master_deck(id); },
        [this](InstanceId id) {
            CardId removed;
            if (!cardSystem_.remove_from_master_deck(id, &removed)) return false;
            if (removed == "parasite" || removed == "parasite+") {
                playerState_.maxHp = std::max(1, playerState_.maxHp - 3);
                if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
            }
            return true;
        },
        [this](InstanceId id) { return cardSystem_.upgrade_card_in_master_deck(id); })
    , hudBattleUi_(static_cast<unsigned>(window.getSize().x),
                   static_cast<unsigned>(window.getSize().y)) {}

void GameFlowController::applyPendingVideoAndHudResize(const sf::ContextSettings& ctx) {
    if (!UserSettings::instance().consumeVideoApplyPending()) return;
    UserSettings::instance().applyVideoModeToWindow(window_, ctx);
    const auto sz = window_.getSize();
    hudBattleUi_.set_window_size(sz.x, sz.y);
}

namespace {

std::vector<CardId> repeat_cards(const CardId& id, int count) {
    std::vector<CardId> out;
    out.reserve(static_cast<size_t>(std::max(0, count)));
    for (int i = 0; i < count; ++i) out.push_back(id);
    return out;
}

} // namespace

bool GameFlowController::initialize() {
    return initialize(CharacterClass::Ironclad);
}

bool GameFlowController::initialize(CharacterClass cc) {
    // 每次新开一局前重置运行级状态
    hasPendingSceneAfterLoad_ = false;
    sceneAfterLoad_           = LastSceneKind::Map;
    exitToStartRequested_     = false;
    seenEventRootsByLayer_.clear();
    hudBattleUi_.set_deck_view_active(false);
    hudBattleUi_.set_pause_menu_active(false);
    hasPendingTreasureOutcome_     = false;
    treasureOutcomeSnapshotValid_ = false;

    dataLayer_.load_cards("");
    dataLayer_.load_monsters("");
    dataLayer_.load_encounters("");
    dataLayer_.load_events("");
    register_all_card_effects(cardSystem_);

    playerState_.playerName = "Telys";
    playerState_.potions.clear();
    playerState_.potionSlotCount = 3;
    if (cc == CharacterClass::Ironclad) {
        playerState_.character = "Ironclad";
        playerState_.currentHp = 80;
        playerState_.maxHp = 80;
        playerState_.relics = { "burning_blood" };

        std::vector<CardId> deck;
        auto s = repeat_cards("strike", 5);
        auto d = repeat_cards("defend", 4);
        deck.insert(deck.end(), s.begin(), s.end());
        deck.insert(deck.end(), d.begin(), d.end());
        deck.push_back("bash");
        cardSystem_.init_master_deck(deck);
    } else {
        playerState_.character = "Silent";
        playerState_.currentHp = 70;
        playerState_.maxHp = 70;
        playerState_.relics = { "ring_of_the_snake" };

        std::vector<CardId> deck;
        auto s = repeat_cards("strike_green", 5);
        auto d = repeat_cards("defend_green", 5);
        deck.insert(deck.end(), s.begin(), s.end());
        deck.insert(deck.end(), d.begin(), d.end());
        deck.push_back("neutralize");
        deck.push_back("survivor");
        cardSystem_.init_master_deck(deck);
    }

    playerState_.energy = 3;
    playerState_.maxEnergy = 3;
    playerState_.gold = 99;
    playerState_.cardsToDrawPerTurn = 5;

    mapEngine_.set_run_rng(&runRng_);
    // 使用新版 12 层随机地图生成（替代旧 fixed_map 5/6 层配置）
    const int mapIndex = mapConfigManager_.getCurrentIndex();
    mapEngine_.init_random_map(mapIndex);

    if (!mapUI_.initialize(&window_)) return false;
    mapUI_.loadLegendTexture("assets/images/menu.png");
    mapUI_.setLegendPosition(1550.f, 550.f);
    mapUI_.setLegendScale(1.0f);
    mapUI_.loadBackgroundTexture("assets/images/background.png");
    mapUI_.setMap(&mapEngine_);
    mapUI_.setCurrentLayer(0);
    mapUI_.set_allow_any_node_click(false);

    // 战斗结果页/全局 HUD 字体：与其它界面统一，优先使用项目内字体。
    if (hudFont_.openFromFile("assets/fonts/Sanji.ttf") ||
        hudFont_.openFromFile("data/font.ttf") ||
        hudFont_.openFromFile("C:/Windows/Fonts/msyh.ttc") ||
        hudFont_.openFromFile("C:/Windows/Fonts/simhei.ttf") ||
        hudFont_.openFromFile("C:/Windows/Fonts/simsun.ttc")) {
        hudFontLoaded_ = true;
    }

    // 顶栏/遗物栏 UI 使用与战斗一致的字体配置
    hudBattleUi_.loadFont("assets/fonts/Sanji.ttf");
    hudBattleUi_.loadChineseFont("assets/fonts/Sanji.ttf");
    preload_battle_ui_assets(hudBattleUi_, playerState_.character);

    statusText_ = "选择第一个节点开始爬塔。";
    // 初始检查点：即使还未进入节点，也允许写入稳定基线存档。
    captureCheckpointForCurrentNode();

    musicManager_.scanAssets();
    musicManager_.playMapMusic();
    return true;
}

void GameFlowController::captureCheckpointForCurrentNode() {
    // 固定存档点：进入节点（房间）瞬间的检查点。之后在房间内任意时刻存档都只写这份状态，
    // 以避免通过 SL 改变宝箱/事件等奖励结果（RNG 状态不随房间内操作变化）。
    checkpointValid_       = true;
    checkpointRunRngState_ = runRng_.get_state();
    checkpointPlayerState_ = playerState_;
    checkpointMasterDeck_  = cardSystem_.get_master_deck_card_ids();

    checkpointCurrentLayer_ = mapEngine_.get_current_layer();
    checkpointCurrentNodeId_.clear();
    {
        MapEngine::MapSnapshot snap = mapEngine_.get_map_snapshot();
        for (const auto& n : snap.all_nodes) {
            if (n.is_current) {
                checkpointCurrentNodeId_ = n.id;
                break;
            }
        }
    }
}

void GameFlowController::layout_map_browse_return_button() {
    constexpr float returnW = 180.f;
    constexpr float returnH = 50.f;
    constexpr float returnX = 28.f;
    constexpr float bottomMargin = 200.f;
    const float returnY = static_cast<float>(window_.getSize().y) - bottomMargin - returnH;
    map_browse_return_rect_ = sf::FloatRect(sf::Vector2f(returnX, returnY), sf::Vector2f(returnW, returnH));
}

bool GameFlowController::hit_map_browse_return_button(const sf::Vector2f& p) {
    layout_map_browse_return_button();
    return map_browse_return_rect_.contains(p);
}

void GameFlowController::draw_cheat_mode_hint() {
    if (!map_cheat_free_travel_ || !hudFontLoaded_)
        return;
    const float padX = 14.f;
    const float padY = 10.f;
    const float x = 18.f;
    const float y = 88.f; // 顶栏下方、遗物行附近

    sf::Text line1(hudFont_, sf::String(L"作弊模式 ON"), 22);
    line1.setFillColor(sf::Color(255, 220, 120));
    line1.setOutlineThickness(2);
    line1.setOutlineColor(sf::Color(40, 20, 10, 200));

    sf::Text line2(hudFont_, sf::String(L"F2 关闭  |  J 重生成地图  |  战斗中按 K 秒杀全部怪物"), 17);
    line2.setFillColor(sf::Color(235, 235, 245));
    line2.setOutlineThickness(1);
    line2.setOutlineColor(sf::Color(0, 0, 0, 180));

    const sf::FloatRect b1 = line1.getLocalBounds();
    const sf::FloatRect b2 = line2.getLocalBounds();
    const float boxW = std::max(b1.size.x, b2.size.x) + padX * 2.f;
    const float boxH = b1.size.y + b2.size.y + padY * 3.f;

    sf::RectangleShape box(sf::Vector2f(boxW, boxH));
    box.setPosition(sf::Vector2f(x, y));
    box.setFillColor(sf::Color(20, 12, 18, 200));
    box.setOutlineColor(sf::Color(255, 180, 60, 220));
    box.setOutlineThickness(2.f);
    window_.draw(box);

    line1.setPosition(sf::Vector2f(x + padX, y + padY));
    line2.setPosition(sf::Vector2f(x + padX, y + padY + b1.size.y + 6.f));
    window_.draw(line1);
    window_.draw(line2);
}

void GameFlowController::draw_map_browse_return_button() {
    layout_map_browse_return_button();
    sf::RectangleShape btn(sf::Vector2f(map_browse_return_rect_.size.x, map_browse_return_rect_.size.y));
    btn.setPosition(sf::Vector2f(map_browse_return_rect_.position.x, map_browse_return_rect_.position.y));
    btn.setFillColor(sf::Color(120, 50, 50));
    btn.setOutlineColor(sf::Color(200, 90, 90));
    btn.setOutlineThickness(2.f);
    window_.draw(btn);
    if (hudFontLoaded_) {
        sf::Text label(hudFont_, sf::String(L"返回"), 24);
        label.setFillColor(sf::Color::White);
        const sf::FloatRect lb = label.getLocalBounds();
        label.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
        label.setPosition(sf::Vector2f(map_browse_return_rect_.position.x + map_browse_return_rect_.size.x * 0.5f,
                                       map_browse_return_rect_.position.y + map_browse_return_rect_.size.y * 0.5f));
        window_.draw(label);
    }
}

void GameFlowController::open_map_browse_overlay(BattleUI* battleUiOrNull) {
    map_browse_overlay_active_ = true;
    mapUI_.set_nodes_clickable(false);
    if (battleUiOrNull)
        battleUiOrNull->set_map_overlay_blocks_world_input(true);
    else
        hudBattleUi_.set_map_overlay_blocks_world_input(true);
}

void GameFlowController::close_map_browse_overlay(BattleUI* battleUiOrNull) {
    map_browse_overlay_active_ = false;
    mapUI_.set_nodes_clickable(true);
    hudBattleUi_.set_map_overlay_blocks_world_input(false);
    if (battleUiOrNull)
        battleUiOrNull->set_map_overlay_blocks_world_input(false);
}

void GameFlowController::poll_map_browse_toggle(BattleUI* battleUiOrNull) {
    bool t = hudBattleUi_.pollMapBrowseToggleRequest();
    if (battleUiOrNull && battleUiOrNull->pollMapBrowseToggleRequest())
        t = true;
    if (!t)
        return;
    if (map_browse_overlay_active_)
        close_map_browse_overlay(battleUiOrNull);
    else
        open_map_browse_overlay(battleUiOrNull);
}

void GameFlowController::run() {
    // 若是从读档进入（且读档记录了非地图界面），在正式进入地图循环前先跳转一次对应界面
    if (hasPendingSceneAfterLoad_) {
        hasPendingSceneAfterLoad_ = false;
        if (mapEngine_.hasCurrentNode()) {
            // 通过快照查找当前节点
            MapEngine::MapSnapshot snap = mapEngine_.get_map_snapshot();
            MapEngine::MapNode node{};
            for (const auto& n : snap.all_nodes) {
                if (n.is_current) {
                    node = n;
                    break;
                }
            }
            if (node.id.empty()) {
                // 找不到当前节点，则直接进入地图循环
                sceneAfterLoad_ = LastSceneKind::Map;
            }
            switch (sceneAfterLoad_) {
            case LastSceneKind::Battle:
                if (node.type == NodeType::Enemy ||
                    node.type == NodeType::Elite ||
                    node.type == NodeType::Boss) {
                    runBattleScene(node.type);
                }
                break;
            case LastSceneKind::BattleReward:
                runBattleRewardOnlyScene();
                break;
            case LastSceneKind::Event:
                // 读档后重新进入事件界面（带完整交互），而不是直接结算事件
                runEventScene(node.content_id);
                break;
            case LastSceneKind::Shop:
                runShopScene();
                break;
            case LastSceneKind::Rest:
                runRestScene();
                break;
            case LastSceneKind::Treasure:
                runTreasureScene();
                break;
            case LastSceneKind::Map:
            default:
                break;
            }
        }
    }

    while (window_.isOpen()) {
        applyPendingVideoAndHudResize(kGameUiContext);
        hudBattleUi_.set_hide_top_right_map_button(true);
        if (map_browse_overlay_active_)
            close_map_browse_overlay(nullptr);

        while (const std::optional ev = window_.pollEvent()) {
        if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return;
            }

            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    window_.close();
                    return;
                }
                if (key->scancode == sf::Keyboard::Scancode::F2) {
                    map_cheat_free_travel_ = !map_cheat_free_travel_;
                    mapUI_.set_allow_any_node_click(map_cheat_free_travel_);
                    statusText_ = map_cheat_free_travel_
                        ? "作弊模式 ON：地图任意节点；战斗按 K 秒杀（F2 关闭）"
                        : "作弊模式已关闭";
                }
                if (map_cheat_free_travel_ && key->scancode == sf::Keyboard::Scancode::J) {
                    mapEngine_.init_random_map(mapConfigManager_.getCurrentIndex());
                    seenEventRootsByLayer_.clear();
                    mapUI_.setMap(&mapEngine_);
                    mapUI_.setCurrentLayer(0);
                    captureCheckpointForCurrentNode();
                    statusText_ = "作弊：已重新生成当前地图（J）。";
                }
                if (key->scancode == sf::Keyboard::Scancode::Left) {
                    mapConfigManager_.prevMap();
                    mapEngine_.init_random_map(mapConfigManager_.getCurrentIndex());
                    seenEventRootsByLayer_.clear();
                    mapUI_.setMap(&mapEngine_);
                    mapUI_.setCurrentLayer(0);
                    statusText_ = "已切换到上一张地图。";
                }
                if (key->scancode == sf::Keyboard::Scancode::Right) {
                    mapConfigManager_.nextMap();
                    mapEngine_.init_random_map(mapConfigManager_.getCurrentIndex());
                    seenEventRootsByLayer_.clear();
                    mapUI_.setMap(&mapEngine_);
                    mapUI_.setCurrentLayer(0);
                    statusText_ = "已切换到下一张地图。";
                }
                if (key->scancode == sf::Keyboard::Scancode::S) {
                    lastSceneForSave_ = LastSceneKind::Map;
                    if (saveRun()) {
                        statusText_ = "存档已保存到 saves/run_auto_save.json。";
                    } else {
                        statusText_ = "存档失败：无法写入 saves/run_auto_save.json。";
                    }
                }
            }
            // 先把事件交给全局 HUD（BattleUI 顶栏）处理：用于右上角「牌组」按钮等
            {
                sf::Vector2f mp;
                if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                    mp = window_.mapPixelToCoords(mr->position);
                } else {
                    mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                }
                hudBattleUi_.handleEvent(*ev, mp);
            }
            poll_map_browse_toggle(nullptr);

            if (gameOver_) continue;
            if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                if (wheel->wheel == sf::Mouse::Wheel::Vertical) {
                    mapUI_.scroll(wheel->delta * 40.0f);
                    continue;
                }
            }
            if (const auto* mouse = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button != sf::Mouse::Button::Left) continue;
                const std::string nodeId = mapUI_.handleClick(mouse->position.x, mouse->position.y);
                if (nodeId.empty()) continue;
                tryMoveToNode(nodeId);
            }
        }

        // 更新全局 HUD 鼠标位置（用于悬停提示）
        {
            sf::Vector2f mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            hudBattleUi_.setMousePosition(mp);
        }

        // 若用户点击了右上角「牌组」按钮，则根据 master deck 打开牌组视图
        {
            int deckMode = 0;
            if (hudBattleUi_.pollOpenDeckViewRequest(deckMode)) {
                // 目前在地图/事件界面，仅展示主牌组（master deck）
                std::vector<CardInstance> cards = cardSystem_.get_master_deck();
                hudBattleUi_.set_deck_view_cards(std::move(cards));
                hudBattleUi_.set_deck_view_active(true);
            }
        }

        // 处理全局 HUD 暂停菜单选择（地图界面）
        {
            int pauseChoice = 0;
            if (hudBattleUi_.pollPauseMenuSelection(pauseChoice)) {
                if (pauseChoice == 1) {
                    // 返回游戏：不做其它事
                } else if (pauseChoice >= 41 && pauseChoice <= 43) {
                    // 存档到槽位（1~3）
                    const int slot = pauseChoice - 40;
                    lastSceneForSave_ = LastSceneKind::Map;
                    const std::string path = "saves/slot_" + std::to_string(slot) + ".json";
                    (void)saveRun(path);
                } else if (pauseChoice == 2) {
                    // 保存并退出：写入存档后关闭窗口
                    lastSceneForSave_ = LastSceneKind::Map;
                    saveRun();
                    exitToStartRequested_ = true;  // 请求回到开始界面
                    return;
                } else if (pauseChoice == 3) {
                    // 进入二级设置界面：仅在 HUD 内部处理（显示占位项）
                }
            }
        }

        window_.clear(sf::Color(245, 245, 245));
        mapUI_.draw();
        drawHud();
        draw_cheat_mode_hint();
        window_.display();

        if (exitToStartRequested_) {
            exitToStartRequested_ = false;
            return;  // 结束本次 run，回到 main 逻辑（开始界面）
        }
    }
}

bool GameFlowController::tryMoveToNode(const std::string& nodeId) {
    MapEngine::MapNode node = mapEngine_.get_node_by_id(nodeId);
    if (node.id.empty()) return false;

    if (!map_cheat_free_travel_) {
        const bool canEnterStart = (!mapEngine_.hasCurrentNode() && node.layer == 0);
        const bool canEnterNext = (mapEngine_.hasCurrentNode() && node.is_reachable);
        if (!canEnterStart && !canEnterNext) {
            statusText_ = "该节点当前不可达。";
            return false;
        }
    }

    // 自动保存：进入节点前（仍停留在地图当前节点）
    // 注意：必须在 set_current_node 之前保存，避免存档记录为“已进入新房间”。
    lastSceneForSave_ = LastSceneKind::Map;
    (void)saveRun();

    mapEngine_.set_current_node(nodeId);
    mapEngine_.set_node_visited(nodeId);
    mapEngine_.update_reachable_nodes();
    mapUI_.setCurrentLayer(node.layer);
    // 进入节点瞬间先记录检查点（之后在该节点界面里存档都只写这份）。
    captureCheckpointForCurrentNode();
    resolveNode(node);
    return true;
}

void GameFlowController::resolveNode(const MapEngine::MapNode& node) {
    switch (node.type) {
    case NodeType::Enemy:
    case NodeType::Elite:
    case NodeType::Boss:
        runBattleScene(node.type);
        break;
    case NodeType::Event:
        if (runEventScene(node.content_id)) {
            statusText_ = "事件结束。";
        } else {
            statusText_ = "事件已中断或数据缺失。";
        }
        break;
    case NodeType::Rest:
        if (runRestScene()) {
            statusText_ = "休息结算完成。";
        } else {
            statusText_ = "休息已中断。";
        }
        break;
    case NodeType::Merchant:
        if (runShopScene()) {
            statusText_ = "商店结算完成。";
        } else {
            statusText_ = "商店已中断或金币不足。";
        }
        break;
    case NodeType::Treasure:
        if (runTreasureScene()) {
            // statusText_ 在 runTreasureScene 内写入
        } else {
            statusText_ = "宝箱已中断。";
        }
        break;
    default:
        break;
    }
}

bool GameFlowController::runBattleScene(NodeType nodeType) {
    struct BattleMusicScope {
        MusicManager& mm;
        explicit BattleMusicScope(MusicManager& m) : mm(m) { m.playRandomBattleMusic(); }
        ~BattleMusicScope() { mm.playMapMusic(); }
    };
    const BattleMusicScope battleMusicScope(musicManager_);

    const int mapPage = mapConfigManager_.getCurrentIndex();
    std::vector<MonsterId> monsters = dataLayer_.roll_monsters_for_battle(mapPage, nodeType, runRng_);

    battleEngine_.start_battle(monsters, playerState_, cardSystem_.get_master_deck_card_ids(), playerState_.relics);
    BattleUI ui(static_cast<unsigned>(window_.getSize().x), static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf")) {
        if (!ui.loadFont("assets/fonts/default.ttf")) {
            ui.loadFont("data/font.ttf");
        }
    }
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
            }
        }
    }
    preload_battle_ui_assets(ui, playerState_.character);

    ui.set_hide_top_right_map_button(false);
    if (map_browse_overlay_active_)
        close_map_browse_overlay(&ui);

    // 战斗专用三张背景（商店/休息/事件等仍用各自界面资源，不受影响）。
    // resolve_image_path 会依次尝试 .png / .jpg / .jpeg / .jfif（含大写），JPEG 与 PNG 均可。
    {
        static const char* const kBattleBgBases[] = {
            "assets/backgrounds/bg_xianqin",
            "assets/backgrounds/bg_hantang",
            "assets/backgrounds/bg_songming",
        };
        const std::string fallback = resolve_image_path("assets/backgrounds/bg");
        std::string slotPath[3];
        for (int i = 0; i < 3; ++i) {
            slotPath[i] = resolve_image_path(kBattleBgBases[i]);
            if (slotPath[i].empty()) slotPath[i] = fallback;
        }
        std::string firstGood;
        for (int i = 0; i < 3; ++i) {
            if (!slotPath[i].empty()) {
                firstGood = slotPath[i];
                break;
            }
        }
        if (!firstGood.empty()) {
            for (int i = 0; i < 3; ++i) {
                const std::string& p = !slotPath[i].empty() ? slotPath[i] : firstGood;
                ui.loadBackgroundForBattle(i, p);
            }
            // 三张地图配置各对应一张战斗背景（整场不变），与 mapConfigManager 下标一致：0 先秦 / 1 汉唐 / 2 宋明
            int bgIdx = mapPage;
            if (bgIdx < 0) bgIdx = 0;
            if (bgIdx > 2) bgIdx = 2;
            ui.setBattleBackground(bgIdx);
        }
    }

    for (const auto& mid : monsters) {
        std::string path = resolve_image_path("assets/monsters/" + mid);
        if (path.empty()) {
            const auto it = kMonsterImageFileAliases.find(mid);
            if (it != kMonsterImageFileAliases.end())
                path = resolve_image_path("assets/monsters/" + it->second);
        }
        if (!path.empty())
            ui.loadMonsterTexture(mid, path);
    }

    CheatEngine cheat(&battleEngine_, &cardSystem_);
    CheatPanel cheatPanel(&cheat,
                          static_cast<unsigned>(window_.getSize().x),
                          static_cast<unsigned>(window_.getSize().y));
    if (!cheatPanel.loadFont("assets/fonts/Sanji.ttf") &&
        !cheatPanel.loadFont("assets/fonts/default.ttf")) {
        cheatPanel.loadFont("data/font.ttf");
    }

    bool runningBattleScene = true;
    bool reward_phase_started = false;
    int reward_gold = 0;
    std::vector<std::string> reward_card_strs;
    std::vector<std::string> reward_relic_offers;
    std::vector<std::string> reward_potion_offers;
    bool reward_card_picked_for_save = false;
    struct PendingCardSelectPlay {
        bool active = false;
        int playHandIndex = -1;
        int playTargetMonsterIndex = -1;
        int requiredCount = 1;
        std::wstring title;
        std::vector<InstanceId> candidateInstanceIds;
        std::vector<InstanceId> selectedInstanceIds;
    } pendingSelectPlay;
    while (window_.isOpen() && runningBattleScene) {
        applyPendingVideoAndHudResize(kGameUiContext);
        ui.set_window_size(static_cast<unsigned>(window_.getSize().x), static_cast<unsigned>(window_.getSize().y));
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (cheatPanel.handleEvent(*ev)) {
                continue;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    runningBattleScene = false;
                }
                if (map_cheat_free_travel_ && key->scancode == sf::Keyboard::Scancode::K) {
                    if (!battleEngine_.is_battle_over()) {
                        BattleState st = battleEngine_.get_battle_state();
                        for (int i = 0; i < static_cast<int>(st.monsters.size()); ++i) {
                            if (st.monsters[static_cast<size_t>(i)].currentHp > 0)
                                battleEngine_.cheat_kill_monster(i);
                        }
                    }
                    continue;
                }
            }

            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mousePos = window_.mapPixelToCoords(mp->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>()) {
                mousePos = window_.mapPixelToCoords(mr->position);
            }
            if (map_browse_overlay_active_) {
                if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                    if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                        mapUI_.scroll(wheel->delta * 40.f);
                    continue;
                }
                if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    if (m->button == sf::Mouse::Button::Left) {
                        const sf::Vector2f p = window_.mapPixelToCoords(m->position);
                        if (hit_map_browse_return_button(p))
                            close_map_browse_overlay(&ui);
                    }
                }
                (void)ui.handleEvent(*ev, mousePos);
                poll_map_browse_toggle(&ui);
                continue;
            }
            if (ui.handleEvent(*ev, mousePos)) {
                battleEngine_.end_turn();
            }
            poll_map_browse_toggle(&ui);
        }
        if (!window_.isOpen() || !runningBattleScene) break;

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        // 处理战斗内的暂停菜单选择
        {
            int pauseChoice = 0;
            if (ui.pollPauseMenuSelection(pauseChoice)) {
                if (pauseChoice == 1) {
                    // 返回游戏：不做额外逻辑
                } else if (pauseChoice >= 41 && pauseChoice <= 43) {
                    const int slot = pauseChoice - 40;
                    // 若在战斗胜利奖励界面存档：读档应回到奖励界面，不重新战斗
                    if (ui.is_reward_screen_active()) {
                        lastSceneForSave_ = LastSceneKind::BattleReward;
                        savedBattleRewardGold_ = reward_gold;
                        savedBattleRewardCardPicked_ = reward_card_picked_for_save;
                        savedBattleRewardCards_ = reward_card_strs;
                        savedBattleRewardRelicOffers_ = reward_relic_offers;
                        savedBattleRewardPotionOffers_ = reward_potion_offers;
                    } else {
                        lastSceneForSave_ = LastSceneKind::Battle;
                    }
                    const std::string path = "saves/slot_" + std::to_string(slot) + ".json";
                    (void)saveRun(path);
                } else if (pauseChoice == 2) {
                    // 保存并退出：写入存档并关闭窗口
                    if (ui.is_reward_screen_active()) {
                        lastSceneForSave_ = LastSceneKind::BattleReward;
                        savedBattleRewardGold_ = reward_gold;
                        savedBattleRewardCardPicked_ = reward_card_picked_for_save;
                        savedBattleRewardCards_ = reward_card_strs;
                        savedBattleRewardRelicOffers_ = reward_relic_offers;
                        savedBattleRewardPotionOffers_ = reward_potion_offers;
                    } else {
                        lastSceneForSave_ = LastSceneKind::Battle;
                    }
                    saveRun();
                    exitToStartRequested_ = true;  // 请求回到开始界面
                    return false;
                } else if (pauseChoice == 3) {
                    // 进入设置页面：UI 内部已处理为二级界面，占位功能
                }
            }
        }

        int handIndex = -1;
        int targetMonsterIndex = -1;
        if (ui.pollPlayCardRequest(handIndex, targetMonsterIndex)) {
            const auto& handNow = cardSystem_.get_hand();
            if (handIndex >= 0 && static_cast<size_t>(handIndex) < handNow.size()) {
                const CardId& id = handNow[static_cast<size_t>(handIndex)].id;

                if (id == "exhume" || id == "exhume+") {
                    const auto& exPile = cardSystem_.get_exhaust_pile();
                    if (exPile.empty()) {
                        ui.showTip(L"消耗堆为空", 1.5f);
                    } else {
                        std::vector<std::string> candidates;
                        std::vector<InstanceId> candidateIids;
                        for (const auto& c : exPile) {
                            candidates.push_back(c.id);
                            candidateIids.push_back(c.instanceId);
                        }
                        pendingSelectPlay.active = true;
                        pendingSelectPlay.playHandIndex = handIndex;
                        pendingSelectPlay.playTargetMonsterIndex = targetMonsterIndex;
                        pendingSelectPlay.requiredCount = 1;
                        pendingSelectPlay.title = L"选择一张已消耗的牌";
                        pendingSelectPlay.candidateInstanceIds = candidateIids;
                        pendingSelectPlay.selectedInstanceIds.clear();
                        std::vector<int> noHandIdx;
                        ui.set_card_select_data(
                            pendingSelectPlay.title,
                            std::move(candidates),
                            true,
                            false,
                            std::vector<InstanceId>(pendingSelectPlay.candidateInstanceIds),
                            1,
                            std::move(noHandIdx),
                            -1);
                        ui.set_card_select_active(true);
                    }
                } else {
                    bool needSelect = (id == "armaments" ||
                                       id == "survivor" || id == "survivor+" ||
                                       id == "true_grit+" ||
                                       id == "burning_pact" || id == "burning_pact+" ||
                                       id == "concentrate" || id == "concentrate+" ||
                                       id == "all_out_attack" || id == "all_out_attack+" ||
                                       id == "acrobatics" || id == "acrobatics+" ||
                                       id == "prepared" || id == "prepared+");
                    if (needSelect) {
                        std::vector<std::string> candidates;
                        std::vector<InstanceId> candidateIids;
                        std::vector<int> candidateHandIdx;
                        for (size_t i = 0; i < handNow.size(); ++i) {
                            if (static_cast<int>(i) == handIndex) continue;
                            candidates.push_back(handNow[i].id);
                            candidateIids.push_back(handNow[i].instanceId);
                            candidateHandIdx.push_back(static_cast<int>(i));
                        }
                        if (!candidates.empty()) {
                            pendingSelectPlay.active = true;
                            pendingSelectPlay.playHandIndex = handIndex;
                            pendingSelectPlay.playTargetMonsterIndex = targetMonsterIndex;
                            pendingSelectPlay.requiredCount =
                                (id == "concentrate+") ? 2 :
                                (id == "concentrate") ? 3 :
                                (id == "prepared+") ? 2 :
                                (id == "prepared") ? 1 :
                                1;
                            if (pendingSelectPlay.requiredCount > static_cast<int>(candidates.size()))
                                pendingSelectPlay.requiredCount = static_cast<int>(candidates.size());
                            if (pendingSelectPlay.requiredCount <= 0) {
                                battleEngine_.play_card(handIndex, targetMonsterIndex);
                                continue;
                            }
                            pendingSelectPlay.selectedInstanceIds.clear();
                            if (id == "armaments") {
                                pendingSelectPlay.title = L"选择要升级的手牌";
                            } else if (id == "true_grit+" ||
                                id == "burning_pact" || id == "burning_pact+") {
                                pendingSelectPlay.title = L"选择要消耗的手牌";
                            } else {
                                pendingSelectPlay.title = L"选择要丢弃的手牌";
                            }
                            pendingSelectPlay.candidateInstanceIds = candidateIids;
                            ui.set_card_select_data(
                                pendingSelectPlay.title,
                                std::move(candidates), true, true, std::move(candidateIids), pendingSelectPlay.requiredCount, std::move(candidateHandIdx),
                                handIndex);
                            ui.set_card_select_active(true);
                        } else {
                            battleEngine_.play_card(handIndex, targetMonsterIndex);
                        }
                    } else {
                        battleEngine_.play_card(handIndex, targetMonsterIndex);
                    }
                }
            } else {
                battleEngine_.play_card(handIndex, targetMonsterIndex);
            }
        }

        std::vector<int> pickedHandIndices;
        bool cardSelectCancelled = false;
        if (ui.pollCardSelectResult(pickedHandIndices, cardSelectCancelled)) {
            if (pendingSelectPlay.active) {
                if (cardSelectCancelled) {  // 取消：本次不打出牌
                    pendingSelectPlay.active = false;
                    pendingSelectPlay.selectedInstanceIds.clear();
                    pendingSelectPlay.candidateInstanceIds.clear();
                } else {
                    for (int pickedHandIndex : pickedHandIndices) {
                        if (pickedHandIndex >= 0 && static_cast<size_t>(pickedHandIndex) < pendingSelectPlay.candidateInstanceIds.size()) {
                            pendingSelectPlay.selectedInstanceIds.push_back(
                                pendingSelectPlay.candidateInstanceIds[static_cast<size_t>(pickedHandIndex)]);
                        }
                    }
                }
                if (!pendingSelectPlay.active) {
                    // cancelled
                } else if (static_cast<int>(pendingSelectPlay.selectedInstanceIds.size()) >= pendingSelectPlay.requiredCount) {
                    // 只取前 requiredCount 个，避免异常多选
                    if (static_cast<int>(pendingSelectPlay.selectedInstanceIds.size()) > pendingSelectPlay.requiredCount) {
                        pendingSelectPlay.selectedInstanceIds.resize(static_cast<size_t>(pendingSelectPlay.requiredCount));
                    }
                    ui.set_pending_select_ui_pile_fly(static_cast<int>(pendingSelectPlay.selectedInstanceIds.size()));
                    battleEngine_.set_effect_selected_instance_ids(pendingSelectPlay.selectedInstanceIds);
                    battleEngine_.play_card(pendingSelectPlay.playHandIndex, pendingSelectPlay.playTargetMonsterIndex);
                    pendingSelectPlay.active = false;
                    pendingSelectPlay.selectedInstanceIds.clear();
                    pendingSelectPlay.candidateInstanceIds.clear();
                } else {
                    // 理论上不会到这里（UI 已限制“确定”按钮），兜底按取消处理
                    pendingSelectPlay.active = false;
                    pendingSelectPlay.selectedInstanceIds.clear();
                    pendingSelectPlay.candidateInstanceIds.clear();
                }
            }
        }

        int potionSlotIndex = -1;
        int potionTargetIndex = -1;
        if (ui.pollPotionRequest(potionSlotIndex, potionTargetIndex)) {
            battleEngine_.use_potion(potionSlotIndex, potionTargetIndex);
        }

        int deckViewMode = 0;
        if (ui.pollOpenDeckViewRequest(deckViewMode)) {
            const auto mode = static_cast<DeckViewMode>(deckViewMode);
            std::vector<CardInstance> cards = collect_deck_view_cards(cardSystem_, mode);
            if (cards.empty()) {
                ui.showTip(deck_view_empty_tip(mode));
            } else {
                ui.set_deck_view_cards(std::move(cards));
                ui.set_deck_view_active(true);
            }
        }

        // 回合推进：在 PlayerTurnStart / EnemyTurnStart 时先播放中央提示（战斗开始/玩家回合/敌方回合），提示播完后再推进引擎逻辑。
        // 这样可以实现：进入战斗→战斗开始→玩家回合1→发牌；结束回合→敌方回合→敌方行动→玩家回合N→发牌。
        BattleState state = battleEngine_.get_battle_state();
        BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(state, &cardSystem_);
        if (!map_browse_overlay_active_ && !ui.is_deck_view_active() && !ui.is_card_select_active()
            && !ui.is_reward_screen_active() && !battleEngine_.is_battle_over()) {
            const bool blockStep = ui.blocks_battle_engine_step(snapshot);
            if (!blockStep) {
                battleEngine_.step_turn_phase();
                state = battleEngine_.get_battle_state();
                snapshot = make_snapshot_from_core_refactor(state, &cardSystem_);
            }
        }
        tce::SnapshotBattleUIDataProvider adapter(&snapshot);
        ui.set_top_bar_map_floor(mapEngine_.get_current_layer(), mapEngine_.get_total_layers());
        window_.clear(sf::Color(28, 26, 32));
        if (map_browse_overlay_active_) {
            mapUI_.draw();
            ui.drawGlobalHud(window_, snapshot);
            draw_map_browse_return_button();
            draw_cheat_mode_hint();
        } else {
            ui.draw(window_, adapter);
            draw_cheat_mode_hint();
        }
        cheatPanel.draw(window_);
        window_.display();
        battleEngine_.tick_damage_displays();

        if (battleEngine_.is_battle_over()) {
            if (!battleEngine_.is_victory()) {
                runningBattleScene = false;
            } else {
                // 奖励阶段：选项制（点击才获得）
                if (!reward_phase_started) {
                    reward_phase_started = true;
                    reward_gold = battleEngine_.get_victory_gold();
                    battleEngine_.grant_victory_gold(); // 金币直接发放

                    std::vector<CardId> reward_cards = battleEngine_.get_reward_cards(3);
                    reward_card_strs.clear();
                    reward_card_strs.reserve(reward_cards.size());
                    for (const CardId& c : reward_cards) reward_card_strs.push_back(c);

                    reward_relic_offers.clear();
                    reward_potion_offers.clear();
                    // 遗物/灵液按掉落概率“生成候选”，点击才真正领取
                    if (runRng_.uniform_int(0, 99) < 40) {
                        const RelicId r = battleEngine_.roll_reward_relic();
                        if (!r.empty()) reward_relic_offers.push_back(r);
                    }
                    if (runRng_.uniform_int(0, 99) < 40) {
                        const PotionId p = battleEngine_.roll_reward_potion();
                        if (!p.empty()) reward_potion_offers.push_back(p);
                    }

                    ui.set_reward_data(reward_gold, std::vector<std::string>(reward_card_strs),
                                       std::vector<std::string>(reward_relic_offers),
                                       std::vector<std::string>(reward_potion_offers));
                    ui.set_reward_screen_active(true);

                    // 自动保存：战斗胜利进入奖励界面（读档无需重打战斗）
                    lastSceneForSave_ = LastSceneKind::BattleReward;
                    savedBattleRewardGold_ = reward_gold;
                    savedBattleRewardCardPicked_ = reward_card_picked_for_save;
                    savedBattleRewardCards_ = reward_card_strs;
                    savedBattleRewardRelicOffers_ = reward_relic_offers;
                    savedBattleRewardPotionOffers_ = reward_potion_offers;
                    (void)saveRun();
                }
                int reward_pick = -2;
                if (ui.pollRewardCardPick(reward_pick)) {
                    if (reward_pick >= 0) {
                        const std::string cid = ui.get_reward_card_id_at(static_cast<size_t>(reward_pick));
                        if (!cid.empty()) battleEngine_.add_card_to_master_deck(cid);
                    }
                    reward_card_picked_for_save = true;
                }
                {
                    std::string rid;
                    if (ui.pollRewardRelicTake(rid)) {
                        if (battleEngine_.take_reward_relic(rid)) {
                            reward_relic_offers.erase(std::remove(reward_relic_offers.begin(), reward_relic_offers.end(), rid),
                                                      reward_relic_offers.end());
                            ui.set_reward_data(reward_gold, std::vector<std::string>(reward_card_strs),
                                               std::vector<std::string>(reward_relic_offers),
                                               std::vector<std::string>(reward_potion_offers));
                        }
                    }
                    std::string pid;
                    int replaceSlot = -2;
                    if (ui.pollRewardPotionTake(pid, replaceSlot)) {
                        if (replaceSlot >= -1 && battleEngine_.take_reward_potion(pid, replaceSlot)) {
                            reward_potion_offers.erase(std::remove(reward_potion_offers.begin(), reward_potion_offers.end(), pid),
                                                       reward_potion_offers.end());
                            ui.set_reward_data(reward_gold, std::vector<std::string>(reward_card_strs),
                                               std::vector<std::string>(reward_relic_offers),
                                               std::vector<std::string>(reward_potion_offers));
                        }
                    }
                }
                if (ui.pollContinueToNextBattleRequest()) {
                    ui.set_reward_screen_active(false);
                    runningBattleScene = false;
                }
            }
        }
    }

    close_map_browse_overlay(&ui);

    if (battleEngine_.is_battle_over() && battleEngine_.is_victory()) {
        BattleState state = battleEngine_.get_battle_state();
        playerState_ = state.player;

        if (nodeType == NodeType::Boss) {
            const int mapIdx     = mapConfigManager_.getCurrentIndex();
            const int mapCount   = static_cast<int>(mapConfigManager_.getMapCount());
            const int lastMapIdx = mapCount > 0 ? mapCount - 1 : 0;
            if (mapIdx >= lastMapIdx) {
                // 最后一页 Boss：通关界面后返回主菜单
                captureCheckpointForCurrentNode();
                statusText_ = "通关！";
                runPostBattleExitOverlay(window_, ui, state, cardSystem_, true, hudFont_, hudFontLoaded_,
                    mapEngine_.get_current_layer(), mapEngine_.get_total_layers());
                exitToStartRequested_ = true;
                return false;
            } else {
                // 非最后一页：进入下一页地图，重新随机生成，继续 Run
                mapConfigManager_.nextMap();
                mapEngine_.init_random_map(mapConfigManager_.getCurrentIndex());
                seenEventRootsByLayer_.clear();
                mapUI_.setMap(&mapEngine_);
                mapUI_.setCurrentLayer(0);
                captureCheckpointForCurrentNode();
                statusText_ = "Boss 已击败，进入「" + mapConfigManager_.getCurrentMapName() + "」。请从本图起点继续。";
            }
        } else {
            statusText_ = "战斗胜利，已发放金币与卡牌奖励。";
        }
        return true;
    }

    BattleState state = battleEngine_.get_battle_state();
    playerState_ = state.player;
    if (playerState_.currentHp <= 0) gameOver_ = true;
    statusText_ = "战斗失败，爬塔结束。";

    close_map_browse_overlay(&ui);
    runPostBattleExitOverlay(window_, ui, state, cardSystem_, false, hudFont_, hudFontLoaded_,
        mapEngine_.get_current_layer(), mapEngine_.get_total_layers());

    // 退出到开始界面（main 会重新调用 runStartScreen）
    exitToStartRequested_ = true;
    return false;
}

bool GameFlowController::runBattleRewardOnlyScene() {
    // 读档恢复：不重新战斗，只显示胜利奖励界面并允许继续领取
    battleEngine_.start_battle({}, playerState_, cardSystem_.get_master_deck_card_ids(), playerState_.relics);

    BattleUI ui(static_cast<unsigned>(window_.getSize().x), static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf")) {
        if (!ui.loadFont("assets/fonts/default.ttf")) {
            ui.loadFont("data/font.ttf");
        }
    }
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
            }
        }
    }
    preload_battle_ui_assets(ui, playerState_.character);
    ui.set_hide_top_right_map_button(false);
    if (map_browse_overlay_active_)
        close_map_browse_overlay(&ui);

    // 恢复奖励数据
    ui.set_reward_data(savedBattleRewardGold_,
                       std::vector<std::string>(savedBattleRewardCards_),
                       std::vector<std::string>(savedBattleRewardRelicOffers_),
                       std::vector<std::string>(savedBattleRewardPotionOffers_));
    ui.set_reward_screen_active(true);
    ui.set_reward_card_picked(savedBattleRewardCardPicked_);

    bool running = true;
    while (window_.isOpen() && running) {
        applyPendingVideoAndHudResize(kGameUiContext);
        const sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    // 不允许直接用 Esc 跳过奖励（避免误触），保持与界面按钮一致
                }
            }
            (void)ui.handleEvent(*ev, mousePos);
            poll_map_browse_toggle(&ui);
        }

        int reward_pick = -2;
        if (ui.pollRewardCardPick(reward_pick)) {
            if (reward_pick >= 0) {
                const std::string cid = ui.get_reward_card_id_at(static_cast<size_t>(reward_pick));
                if (!cid.empty()) battleEngine_.add_card_to_master_deck(cid);
            }
            savedBattleRewardCardPicked_ = true;
            ui.set_reward_card_picked(true);
        }
        {
            std::string rid;
            if (ui.pollRewardRelicTake(rid)) {
                if (battleEngine_.take_reward_relic(rid)) {
                    auto& v = savedBattleRewardRelicOffers_;
                    v.erase(std::remove(v.begin(), v.end(), rid), v.end());
                    ui.set_reward_data(savedBattleRewardGold_,
                                       std::vector<std::string>(savedBattleRewardCards_),
                                       std::vector<std::string>(savedBattleRewardRelicOffers_),
                                       std::vector<std::string>(savedBattleRewardPotionOffers_));
                    ui.set_reward_card_picked(savedBattleRewardCardPicked_);
                }
            }
            std::string pid;
            int replaceSlot = -2;
            if (ui.pollRewardPotionTake(pid, replaceSlot)) {
                if (replaceSlot >= -1 && battleEngine_.take_reward_potion(pid, replaceSlot)) {
                    auto& v = savedBattleRewardPotionOffers_;
                    v.erase(std::remove(v.begin(), v.end(), pid), v.end());
                    ui.set_reward_data(savedBattleRewardGold_,
                                       std::vector<std::string>(savedBattleRewardCards_),
                                       std::vector<std::string>(savedBattleRewardRelicOffers_),
                                       std::vector<std::string>(savedBattleRewardPotionOffers_));
                    ui.set_reward_card_picked(savedBattleRewardCardPicked_);
                }
            }
        }
        if (ui.pollContinueToNextBattleRequest()) {
            ui.set_reward_screen_active(false);
            running = false;
        }

        BattleState state = battleEngine_.get_battle_state();
        BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(state, &cardSystem_);
        SnapshotBattleUIDataProvider adapter(&snapshot);
        ui.set_top_bar_map_floor(mapEngine_.get_current_layer(), mapEngine_.get_total_layers());
        window_.clear(sf::Color(28, 26, 32));
        ui.draw(window_, adapter);
        window_.display();
        battleEngine_.tick_damage_displays();
    }

    // 同步玩家状态
    playerState_ = battleEngine_.get_battle_state().player;
    // 读档后的奖励界面结束后回到地图（不需要重战）
    lastSceneForSave_ = LastSceneKind::Map;
    return true;
}

void GameFlowController::resolveEvent(const std::string& contentId) {
    std::string eventId = contentId;
    if (dataLayer_.get_event_by_id(eventId) == nullptr) {
        eventId = "event_001";
    }
    if (dataLayer_.get_event_by_id(eventId) == nullptr) {
        statusText_ = "事件数据缺失，跳过本节点。";
        return;
    }

    eventEngine_.start_event(eventId);
    int safety = 0;
    while (const auto* e = eventEngine_.get_current_event()) {
        if (e->options.empty()) break;
        const int pick = randomIndex(runRng_, static_cast<int>(e->options.size()));
        if (!eventEngine_.choose_option(pick)) break;
        if (++safety > 8) break;
    }

    DataLayer::EventResult result{};
    if (!eventEngine_.get_event_result(result)) {
        statusText_ = "事件结束。";
        return;
    }

    const auto applyOneEffect = [&](const std::string& type, int value) {
        if (type == "gold") {
            playerState_.gold += value;
        } else if (type == "heal") {
            playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + value);
        } else if (type == "max_hp") {
            playerState_.maxHp += value;
            if (playerState_.maxHp < 1) playerState_.maxHp = 1;
            if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
        } else if (type == "card_reward") {
            const int count = std::max(0, value);
            if (count > 0) {
                const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
                std::vector<CardId> cards = battleEngine_.get_reward_cards(count);
                for (const auto& cid : cards) {
                    cardSystem_.add_to_master_deck(cid);
                    if (hasCeramicFish) playerState_.gold += 9; // 陶瓷小鱼：每次加入牌组获得 9 金币
                }
            }
        } else if (type == "card_reward_choose") {
            // 非交互结算路径（自动 resolveEvent）：按 value 选择张数，候选池更大一些
            const int chooseCount = std::max(1, value);
            const int candidateCount = std::max(3, chooseCount + 1);
            const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
            std::vector<CardId> cards = battleEngine_.get_reward_cards(candidateCount);
            for (int i = 0; i < chooseCount; ++i) {
                if (cards.empty()) break;
                const int pick = randomIndex(runRng_, static_cast<int>(cards.size()));
                cardSystem_.add_to_master_deck(cards[static_cast<size_t>(pick)]);
                if (hasCeramicFish) playerState_.gold += 9;
                cards.erase(cards.begin() + pick); // 无放回，避免重复
            }
        } else if (type == "add_curse") {
            const int count = std::max(0, value);
            for (int i = 0; i < count; ++i) {
                cardSystem_.add_to_master_deck(event_pick_random_curse(runRng_));
            }
        } else if (type == "remove_card") {
            const int count = std::max(0, value);
            for (int i = 0; i < count; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;
                const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                eventEngine_.remove_card_from_master_deck(deck[static_cast<size_t>(idx)].instanceId);
            }
        } else if (type == "remove_card_choose") {
            // 非交互结算路径（自动 resolveEvent）：退化为随机移除
            const int count = std::max(1, value);
            for (int i = 0; i < count; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;
                const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                eventEngine_.remove_card_from_master_deck(deck[static_cast<size_t>(idx)].instanceId);
            }
        } else if (type == "remove_curse") {
            const int count = std::max(0, value);
            for (int i = 0; i < count; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;

                std::vector<InstanceId> candidates;
                for (const auto& inst : deck) {
                    if (is_curse_card_id(inst.id)) candidates.push_back(inst.instanceId);
                }
                if (candidates.empty()) break; // 无诅咒则本次效果不移除任何牌
                const InstanceId picked = candidates[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(candidates.size())))];
                eventEngine_.remove_card_from_master_deck(picked);
            }
        } else if (type == "upgrade_random") {
            const int attempts = std::max(0, value);
            for (int i = 0; i < attempts; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;
                const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                cardSystem_.upgrade_card_in_master_deck(deck[static_cast<size_t>(idx)].instanceId);
            }
        } else if (type == "relic") {
            const int count = std::max(0, value);
            static const std::vector<RelicId> relic_pool = {
                "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle",
                "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull",
                "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum",
                "pen_nib", "toy_ornithopter", "preparation_pack", "anchor", "art_of_war", "relic_strength_plus"
            };
            for (int i = 0; i < count; ++i) {
                std::vector<RelicId> available;
                for (const auto& rid : relic_pool) {
                    if (std::find(playerState_.relics.begin(), playerState_.relics.end(), rid) == playerState_.relics.end())
                        available.push_back(rid);
                }
                if (available.empty()) break;
                const RelicId gained = available[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(available.size())))];
                playerState_.relics.push_back(gained);

                // 拾起即刻效果（目前仅实现事件用到的少数遗物）
                if (gained == "strawberry") {
                    playerState_.maxHp += 7;
                    playerState_.currentHp += 7;
                    if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                } else if (gained == "potion_belt") {
                    playerState_.potionSlotCount += 2;
                }
            }
        }

        if (playerState_.currentHp <= 0) gameOver_ = true;
    };

    const auto applyEventResult = [&](const DataLayer::EventResult& res) {
        if (!res.effects.empty()) {
            for (const auto& eff : res.effects) {
                if (!gameOver_) applyOneEffect(eff.type, eff.value);
            }
            return;
        }
        applyOneEffect(res.type, res.value);
    };

    applyEventResult(result);
    statusText_ = "事件结算完成。";
}

bool GameFlowController::runEventScene(const std::string& contentId) {
    // 问号房：按“当前地图层 index”从事件池做加权随机抽取（模仿杀戮尖塔问号房高波动事件池）。
    // 本项目 MapEngine 的默认 content_id 形如：content_{typeInt}_{layer-index}
    int layer = 0;
    {
        const size_t pos = contentId.rfind('_');
        if (pos != std::string::npos) {
            const std::string tail = contentId.substr(pos + 1); // "layer-index"
            const size_t dash = tail.find('-');
            if (dash != std::string::npos) {
                try { layer = std::stoi(tail.substr(0, dash)); }
                catch (...) { layer = 0; }
            }
        }
    }

    std::string eventId;
    const std::string debugEventId = read_debug_event_override_id();
    if (!debugEventId.empty() && dataLayer_.get_event_by_id(debugEventId) != nullptr) {
        eventId = debugEventId;
        statusText_ = "调试事件覆盖已启用：" + debugEventId;
    } else {
        std::vector<DataLayer::RootEventCandidate> candidates =
            dataLayer_.get_root_event_candidates_for_layer(layer);
    if (!candidates.empty()) {
        auto& seenInLayer = seenEventRootsByLayer_[layer];
        std::vector<DataLayer::RootEventCandidate> freshCandidates;
        freshCandidates.reserve(candidates.size());
        for (const auto& c : candidates) {
            if (seenInLayer.find(c.id) == seenInLayer.end()) freshCandidates.push_back(c);
        }
        // 本层已出现过池中每一个根事件时，清空记录再开一轮，避免立刻按权重放回全池造成高频重复。
        if (freshCandidates.empty() && !candidates.empty()) {
            seenInLayer.clear();
            freshCandidates = candidates;
        }
        const auto& pickPool = freshCandidates;

        int totalWeight = 0;
        for (const auto& c : pickPool) totalWeight += std::max(1, c.weight);
        if (totalWeight <= 0) totalWeight = 1;

        int pick = randomIndex(runRng_, totalWeight);
        for (const auto& c : pickPool) {
            const int w = std::max(1, c.weight);
            if (pick < w) { eventId = c.id; break; }
            pick -= w;
        }
    }
    }

    if (eventId.empty()) eventId = "event_001";
    if (dataLayer_.get_event_by_id(eventId) == nullptr) return false;
    seenEventRootsByLayer_[layer].insert(eventId);

    eventEngine_.start_event(eventId);

    EventShopRestUI ui(static_cast<unsigned>(window_.getSize().x),
                       static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") &&
        !ui.loadFont("assets/fonts/default.ttf")) {
        ui.loadFont("data/font.ttf");
    }
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                    if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
                    }
                }
            }
        }
    }

    ui.setScreen(EventShopRestScreen::Event);
    if (!eventEngine_.get_current_event()) {
        ui.setEventDataFromUtf8("（未加载事件）",
                                "请确保 data/events.json 存在且含根事件（如 event_001）。",
                                { "离开" },
                                "");
    }

    bool inScene = true;
    bool showingResult = false;
    const EventEngine::Event* lastDisplayedEvent = nullptr;
    std::vector<std::string> resultCardPreviewIds;

    hudBattleUi_.set_hide_top_right_map_button(false);
    if (map_browse_overlay_active_)
        close_map_browse_overlay(nullptr);

    auto effectToSummary = [](const DataLayer::EventEffect& eff) {
        const int v = eff.value;
        if (eff.type == "gold") {
            if (v >= 0) return std::string("获得了 ") + std::to_string(v) + " 金币。";
            return std::string("失去了 ") + std::to_string(-v) + " 金币。";
        }
        if (eff.type == "heal") {
            if (v >= 0) return std::string("恢复了 ") + std::to_string(v) + " 点生命。";
            return std::string("失去了 ") + std::to_string(-v) + " 点生命。";
        }
        if (eff.type == "max_hp") {
            if (v >= 0) return std::string("最大生命值提升了 ") + std::to_string(v) + "。";
            return std::string("最大生命值降低了 ") + std::to_string(-v) + "。";
        }
        if (eff.type == "card_reward") return std::string("获得了 ") + std::to_string(v) + " 张新卡牌。";
        if (eff.type == "card_reward_choose") return std::string("可选获得 ") + std::to_string(std::max(1, v)) + " 张卡牌。";
        if (eff.type == "add_curse") return std::string("获得了 ") + std::to_string(v) + " 张诅咒牌。";
        if (eff.type == "remove_card") return std::string("移除了牌组中的 ") + std::to_string(v) + " 张卡牌。";
        if (eff.type == "remove_card_choose") return std::string("自选移除牌组中的 ") + std::to_string(std::max(1, v)) + " 张卡牌。";
        if (eff.type == "remove_curse") return std::string("移除了 ") + std::to_string(v) + " 张诅咒牌。";
        if (eff.type == "upgrade_random") return std::string("升级了 ") + std::to_string(v) + " 张卡牌。";
        if (eff.type == "relic") return std::string("获得了 ") + std::to_string(v) + " 个遗物。";
        if (eff.type == "none") return std::string("无事发生。");
        return std::string("事件结算：")+ eff.type + "（x" + std::to_string(v) + "）。";
    };

    auto summarizeResult = [&](const DataLayer::EventResult& res) {
        std::string out;
        if (!res.effects.empty()) {
            for (size_t i = 0; i < res.effects.size(); ++i) {
                if (i > 0) out += " ";
                out += effectToSummary(res.effects[i]);
            }
            return out.empty() ? std::string("无事发生。") : out;
        }
        // 兼容旧数据：退化到 type/value
        DataLayer::EventEffect eff{ res.type, res.value };
        return effectToSummary(eff);
    };

    // 事件中的“自选”交互：复用事件面板按钮，返回被选中的索引（取消/关闭返回 -1）
    auto pickFromEventOptions = [&](const std::string& title,
                                    const std::string& desc,
                                    const std::vector<std::string>& options,
                                    const std::vector<std::string>& optionEffects = std::vector<std::string>{},
                                    const std::vector<std::string>& optionCardIds = std::vector<std::string>{}) -> int {
        if (options.empty()) return -1;
        ui.setEventDataFromUtf8(title, desc, options, "", optionEffects, optionCardIds);
        while (window_.isOpen()) {
            this->applyPendingVideoAndHudResize(kGameUiContext);
            while (const std::optional ev = window_.pollEvent()) {
                if (ev->is<sf::Event::Closed>()) {
                    window_.close();
                    return -1;
                }
                if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                    if (key->scancode == sf::Keyboard::Scancode::Escape) return -1;
                }
                sf::Vector2f mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                if (const auto* m1 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m1->position);
                } else if (const auto* m2 = ev->getIf<sf::Event::MouseButtonReleased>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                }
                ui.handleEvent(*ev, mp);
            }

            sf::Vector2f mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            ui.setMousePosition(mp);
            int picked = -1;
            if (ui.pollEventOption(picked)) return picked;

            window_.clear(sf::Color(40, 38, 45));
            ui.draw(window_);
            window_.display();
        }
        return -1;
    };

    while (window_.isOpen() && inScene) {
        applyPendingVideoAndHudResize(kGameUiContext);
        const EventEngine::Event* current = eventEngine_.get_current_event();
        if (current && !showingResult && current != lastDisplayedEvent) {
            ui.setEventDataFromEvent(current);
            lastDisplayedEvent = current;
        }
        if (showingResult) lastDisplayedEvent = nullptr;

        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    inScene = false;
                }
            }

            // 先交给全局 HUD（右上角按钮）处理；地图浏览浮层时只滚地图/关浮层，不传递给事件 UI
            {
                sf::Vector2f mp;
                if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                    mp = window_.mapPixelToCoords(mr->position);
                } else {
                    mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                }
                if (map_browse_overlay_active_) {
                    if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                        if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                            mapUI_.scroll(wheel->delta * 40.f);
                        continue;
                    }
                    if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                        if (m->button == sf::Mouse::Button::Left) {
                            const sf::Vector2f p = window_.mapPixelToCoords(m->position);
                            if (hit_map_browse_return_button(p))
                                close_map_browse_overlay(nullptr);
                        }
                    }
                    hudBattleUi_.handleEvent(*ev, mp);
                    poll_map_browse_toggle(nullptr);
                    continue;
                }
                hudBattleUi_.handleEvent(*ev, mp);
            }
            poll_map_browse_toggle(nullptr);

            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mousePos = window_.mapPixelToCoords(mp->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>()) {
                mousePos = window_.mapPixelToCoords(mr->position);
            }
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        // HUD 悬停位置
        hudBattleUi_.setMousePosition(mousePos);

        // 处理 HUD 牌组按钮
        {
            int deckMode = 0;
            if (hudBattleUi_.pollOpenDeckViewRequest(deckMode)) {
                std::vector<CardInstance> cards = cardSystem_.get_master_deck();
                hudBattleUi_.set_deck_view_cards(std::move(cards));
                hudBattleUi_.set_deck_view_active(true);
            }
        }

        // 处理 HUD 暂停菜单（事件界面）
        {
            int pauseChoice = 0;
            if (hudBattleUi_.pollPauseMenuSelection(pauseChoice)) {
                if (pauseChoice >= 41 && pauseChoice <= 43) {
                    const int slot = pauseChoice - 40;
                    lastSceneForSave_ = LastSceneKind::Event;
                    const std::string path = "saves/slot_" + std::to_string(slot) + ".json";
                    (void)saveRun(path);
                } else if (pauseChoice == 2) {
                    lastSceneForSave_ = LastSceneKind::Event;
                    saveRun();
                    exitToStartRequested_ = true;  // 请求回到开始界面
                    return false;
                }
            }
        }

        int outIndex = -1;
        if (ui.pollEventOption(outIndex)) {
            if (showingResult) {
                if (outIndex == 0) {
                    showingResult = false;
                    if (!eventEngine_.get_current_event()) {
                        inScene = false;
                    }
                }
            } else if (const EventEngine::Event* curEv = eventEngine_.get_current_event()) {
                if (outIndex >= 0 && static_cast<size_t>(outIndex) < curEv->options.size()) {
                    const std::string chosenOptionText = curEv->options[static_cast<size_t>(outIndex)].text;
                    if (eventEngine_.choose_option(outIndex)) {
                        DataLayer::EventResult res{};
                        if (eventEngine_.get_event_result(res)) {
                        std::vector<std::string> detailMessages;
                        std::vector<std::string> gainedRelicIds;
                        const auto pushCardPreview = [&](const std::string& cid) {
                            if (cid.empty()) return;
                            if (resultCardPreviewIds.size() < 4) resultCardPreviewIds.push_back(cid);
                        };
                        const auto pushDetail = [&](const std::string& msg) {
                            if (!msg.empty()) detailMessages.push_back(msg);
                        };
                        const auto applyOneEffect = [&](const std::string& type, int value) {
                            if (type == "gold") {
                                playerState_.gold += value;
                            } else if (type == "heal") {
                                playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + value);
                            } else if (type == "max_hp") {
                                playerState_.maxHp += value;
                                if (playerState_.maxHp < 1) playerState_.maxHp = 1;
                                if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                            } else if (type == "card_reward") {
                                const int count = std::min(4, std::max(0, value));
                                if (count > 0) {
                                    const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
                                    std::vector<CardId> cards = battleEngine_.get_reward_cards(count);
                                    std::vector<std::string> gained;
                                    for (const auto& cid : cards) {
                                        cardSystem_.add_to_master_deck(cid);
                                        pushCardPreview(cid);
                                        const CardData* cd = get_card_by_id(cid);
                                        gained.push_back(cd ? cd->name : cid);
                                        if (hasCeramicFish) playerState_.gold += 9; // 陶瓷小鱼：每次加入牌组获得 9 金币
                                    }
                                    pushDetail("获得卡牌：" + join_names_cn(gained));
                                }
                            } else if (type == "card_reward_choose") {
                                const int chooseCount = std::min(4, std::max(1, value));
                                const int candidateCount = std::max(3, chooseCount + 1);
                                const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
                                std::vector<CardId> cards = battleEngine_.get_reward_cards(candidateCount);
                                std::vector<std::string> chosenNames;
                                for (int chooseIdx = 0; chooseIdx < chooseCount; ++chooseIdx) {
                                    if (cards.empty()) break;
                                    std::vector<std::string> optionTexts;
                                    std::vector<std::string> optionEffects;
                                    optionTexts.reserve(cards.size());
                                    optionEffects.reserve(cards.size());
                                    for (const auto& cid : cards) {
                                        const CardData* cd = get_card_by_id(cid);
                                        optionTexts.push_back(cd ? cd->name : cid);
                                        if (cd) {
                                            optionEffects.push_back("费" + std::to_string(std::max(0, cd->cost)) + "｜" + cd->description);
                                        } else {
                                            optionEffects.push_back("未知卡牌效果");
                                        }
                                    }
                                    const int pick = pickFromEventOptions(
                                        "择术",
                                        "请选择 1 张要加入牌组的卡牌",
                                        optionTexts,
                                        optionEffects,
                                        cards);
                                    if (pick < 0 || pick >= static_cast<int>(cards.size())) break;
                                    const CardId chosen = cards[static_cast<size_t>(pick)];
                                    cardSystem_.add_to_master_deck(chosen);
                                    pushCardPreview(chosen);
                                    const CardData* chosenCd = get_card_by_id(chosen);
                                    chosenNames.push_back(chosenCd ? chosenCd->name : chosen);
                                    if (hasCeramicFish) playerState_.gold += 9;
                                    cards.erase(cards.begin() + pick); // 无放回，避免重复
                                }
                                pushDetail("自选获得卡牌：" + join_names_cn(chosenNames));
                            } else if (type == "add_curse") {
                                const int count = std::min(4, std::max(0, value));
                                std::vector<std::string> gainedNames;
                                for (int i = 0; i < count; ++i) {
                                    const CardId gainedCurse = event_pick_random_curse(runRng_);
                                    cardSystem_.add_to_master_deck(gainedCurse);
                                    pushCardPreview(gainedCurse);
                                    const CardData* cd = get_card_by_id(gainedCurse);
                                    gainedNames.push_back(cd ? cd->name : gainedCurse);
                                }
                                pushDetail("获得诅咒牌：" + join_names_cn(gainedNames));
                            } else if (type == "remove_card") {
                                const int count = std::min(4, std::max(0, value));
                                std::vector<std::string> removedNames;
                                for (int i = 0; i < count; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;
                                    const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                                    const auto& inst = deck[static_cast<size_t>(idx)];
                                    pushCardPreview(inst.id);
                                    const CardData* cd = get_card_by_id(inst.id);
                                    removedNames.push_back(cd ? cd->name : inst.id);
                                    eventEngine_.remove_card_from_master_deck(inst.instanceId);
                                }
                                pushDetail("移除卡牌：" + join_names_cn(removedNames));
                            } else if (type == "remove_card_choose") {
                                const int count = std::min(4, std::max(1, value));
                                std::vector<std::string> removedNames;
                                for (int i = 0; i < count; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;
                                    std::vector<InstanceId> ids;
                                    std::vector<std::string> names;
                                    std::vector<std::string> effects;
                                    ids.reserve(deck.size());
                                    names.reserve(deck.size());
                                    effects.reserve(deck.size());
                                    for (const auto& inst : deck) {
                                        ids.push_back(inst.instanceId);
                                        const CardData* cd = get_card_by_id(inst.id);
                                        names.push_back(cd ? cd->name : inst.id);
                                        if (cd) {
                                            effects.push_back("费" + std::to_string(std::max(0, cd->cost)) + "｜" + cd->description);
                                        } else {
                                            effects.push_back("未知卡牌效果");
                                        }
                                    }
                                    std::vector<std::string> idStrs;
                                    idStrs.reserve(deck.size());
                                    for (const auto& inst : deck) idStrs.push_back(inst.id);
                                    const int pick = pickFromEventOptions(
                                        "断舍",
                                        "请选择 1 张要移除的卡牌",
                                        names,
                                        effects,
                                        idStrs);
                                    if (pick < 0 || pick >= static_cast<int>(ids.size())) break;
                                    const InstanceId chosenInstId = ids[static_cast<size_t>(pick)];
                                    for (const auto& inst : deck) {
                                        if (inst.instanceId == chosenInstId) {
                                            pushCardPreview(inst.id);
                                            const CardData* cd = get_card_by_id(inst.id);
                                            removedNames.push_back(cd ? cd->name : inst.id);
                                            break;
                                        }
                                    }
                                    eventEngine_.remove_card_from_master_deck(chosenInstId);
                                }
                                pushDetail("自选移除卡牌：" + join_names_cn(removedNames));
                            } else if (type == "remove_curse") {
                                const int count = std::min(4, std::max(0, value));
                                std::vector<std::string> removedNames;
                                for (int i = 0; i < count; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;

                                    std::vector<InstanceId> candidates;
                                    for (const auto& inst : deck) {
                                        if (is_curse_card_id(inst.id)) candidates.push_back(inst.instanceId);
                                    }
                                    if (candidates.empty()) break; // 无诅咒则本次效果不移除任何牌
                                    const InstanceId picked = candidates[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(candidates.size())))];
                                    for (const auto& inst : deck) {
                                        if (inst.instanceId == picked) {
                                            pushCardPreview(inst.id);
                                            const CardData* cd = get_card_by_id(inst.id);
                                            removedNames.push_back(cd ? cd->name : inst.id);
                                            break;
                                        }
                                    }
                                    eventEngine_.remove_card_from_master_deck(picked);
                                }
                                if (removedNames.empty())
                                    pushDetail("未找到诅咒牌，未移除任何卡牌。");
                                else
                                    pushDetail("移除诅咒牌：" + join_names_cn(removedNames));
                            } else if (type == "upgrade_random") {
                                const int attempts = std::min(4, std::max(0, value));
                                std::vector<std::string> upgradedNames;
                                for (int i = 0; i < attempts; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;
                                    bool useChooseUpgrade = (randomIndex(runRng_, 100) < 45); // 一定比率使用自选升级
                                    if (useChooseUpgrade) {
                                        std::vector<InstanceId> ids;
                                        std::vector<std::string> names;
                                        std::vector<std::string> effects;
                                        ids.reserve(deck.size());
                                        names.reserve(deck.size());
                                        effects.reserve(deck.size());
                                        for (const auto& inst : deck) {
                                            ids.push_back(inst.instanceId);
                                            const CardData* cd = get_card_by_id(inst.id);
                                            names.push_back(cd ? cd->name : inst.id);
                                            if (cd) effects.push_back("费" + std::to_string(std::max(0, cd->cost)) + "｜" + cd->description);
                                            else effects.push_back("未知卡牌效果");
                                        }
                                        std::vector<std::string> upgradeIds;
                                        upgradeIds.reserve(deck.size());
                                        for (const auto& inst : deck) upgradeIds.push_back(inst.id);
                                        const int pick = pickFromEventOptions(
                                            "精修",
                                            "请选择 1 张要升级的卡牌",
                                            names,
                                            effects,
                                            upgradeIds);
                                        if (pick >= 0 && pick < static_cast<int>(ids.size())) {
                                            const InstanceId iid = ids[static_cast<size_t>(pick)];
                                            for (const auto& inst : deck) {
                                                if (inst.instanceId == iid) {
                                                    pushCardPreview(inst.id);
                                                    const CardData* cd = get_card_by_id(inst.id);
                                                    upgradedNames.push_back(cd ? cd->name : inst.id);
                                                    break;
                                                }
                                            }
                                            cardSystem_.upgrade_card_in_master_deck(iid);
                                            continue;
                                        }
                                    }
                                    const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                                    const auto& inst = deck[static_cast<size_t>(idx)];
                                    pushCardPreview(inst.id);
                                    const CardData* cd = get_card_by_id(inst.id);
                                    upgradedNames.push_back(cd ? cd->name : inst.id);
                                    cardSystem_.upgrade_card_in_master_deck(inst.instanceId);
                                }
                                pushDetail("升级卡牌：" + join_names_cn(upgradedNames));
                            } else if (type == "relic") {
                                const int count = std::max(0, value);
                                static const std::vector<RelicId> relic_pool = {
                                    "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle",
                                    "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull",
                                    "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum",
                                    "pen_nib", "toy_ornithopter", "preparation_pack", "anchor", "art_of_war", "relic_strength_plus"
                                };
                                std::vector<std::string> gainedRelics;
                                for (int i = 0; i < count; ++i) {
                                    std::vector<RelicId> available;
                                    for (const auto& rid : relic_pool) {
                                        if (std::find(playerState_.relics.begin(), playerState_.relics.end(), rid) == playerState_.relics.end())
                                            available.push_back(rid);
                                    }
                                    if (available.empty()) break;
                                    const RelicId gained = available[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(available.size())))];
                                    playerState_.relics.push_back(gained);
                                    gainedRelicIds.push_back(gained);
                                    gainedRelics.push_back(relic_name_cn(gained));

                                    if (gained == "strawberry") {
                                        playerState_.maxHp += 7;
                                        playerState_.currentHp += 7;
                                        if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                                    } else if (gained == "potion_belt") {
                                        playerState_.potionSlotCount += 2;
                                    }
                                }
                                pushDetail("获得遗物：" + join_names_cn(gainedRelics));
                            }
                            if (playerState_.currentHp <= 0) gameOver_ = true;
                        };

                        const auto applyEventResult = [&](const DataLayer::EventResult& r) {
                            if (!r.effects.empty()) {
                                for (const auto& eff : r.effects) {
                                    if (gameOver_) break;
                                    applyOneEffect(eff.type, eff.value);
                                }
                                return;
                            }
                            applyOneEffect(r.type, r.value);
                        };

                        resultCardPreviewIds.clear();
                        applyEventResult(res);
                        // 结果区最多展示 4 张：两列网格
                        std::vector<std::string> uniqueIds;
                        uniqueIds.reserve(resultCardPreviewIds.size());
                        for (const auto& id : resultCardPreviewIds) {
                            if (id.empty()) continue;
                            if (std::find(uniqueIds.begin(), uniqueIds.end(), id) == uniqueIds.end()) {
                                uniqueIds.push_back(id);
                            }
                            if (uniqueIds.size() >= 4) break;
                        }
                        std::string resultImage;
                        if (!uniqueIds.empty()) {
                            if (!gainedRelicIds.empty()) {
                                resultImage = "__cards_relic:";
                            } else {
                                resultImage = "__cards:";
                            }
                            for (size_t i = 0; i < uniqueIds.size(); ++i) {
                                if (i > 0) resultImage += "|";
                                resultImage += uniqueIds[i];
                            }
                            if (!gainedRelicIds.empty()) {
                                resultImage += "#";
                                resultImage += gainedRelicIds.front();
                            }
                        } else if (!gainedRelicIds.empty()) {
                            resultImage = "__relic:" + gainedRelicIds.front();
                        }
                        std::string body;
                        if (!detailMessages.empty()) {
                            for (size_t i = 0; i < detailMessages.size(); ++i) {
                                if (i > 0) body += "\n";
                                body += "· " + detailMessages[i];
                            }
                        } else {
                            body = summarizeResult(res);
                        }
                        std::string finalSummary;
                        if (!chosenOptionText.empty()) {
                            finalSummary = "本次选择：「" + chosenOptionText + "」";
                            if (!body.empty()) finalSummary += "\n\n" + body;
                        } else {
                            finalSummary = body;
                        }
                    ui.setEventResultFromUtf8(finalSummary, resultImage);
                    showingResult = true;
                        if (gameOver_) inScene = false;
                } else {
                    // 这里通常是选择了带 next 的选项（进入二层事件），不是事件结束。
                    // 保持在事件场景，等待下一帧按新的 current_event_ 刷新 UI。
                    showingResult = false;
                    lastDisplayedEvent = nullptr;
                }
                    }
                }
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        if (map_browse_overlay_active_) {
            mapUI_.draw();
            drawHud();
            draw_map_browse_return_button();
        } else {
            ui.draw(window_);
            drawHud();  // 事件界面上方叠加全局顶栏 + 遗物栏
        }
        window_.display();
    }

    close_map_browse_overlay(nullptr);
    return true;
}

void GameFlowController::resolveRest() {
    const int heal = std::max(12, playerState_.maxHp / 5);
    playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + heal);
    statusText_ = "在休息点恢复了生命值。";
}

void GameFlowController::resolveMerchant() {
    if (playerState_.gold < 50) {
        statusText_ = "金币不足，离开商店。";
        return;
    }
    const CardId picked = pick_map_merchant_random_card(playerState_.character, runRng_);
    cardSystem_.add_to_master_deck(picked);
    playerState_.gold -= 50;
    statusText_ = "商店购买 1 张牌（-50 金币）。";
}

bool GameFlowController::runTreasureScene() {
    struct TreasureSnapshotClearOnExit {
        GameFlowController* self;
        explicit TreasureSnapshotClearOnExit(GameFlowController* s) : self(s) {}
        ~TreasureSnapshotClearOnExit() { self->treasureOutcomeSnapshotValid_ = false; }
    } snapClear{this};

    TreasureRoomOutcome tr{};
    if (hasPendingTreasureOutcome_) {
        tr                         = pendingTreasureOutcome_;
        hasPendingTreasureOutcome_ = false;
    } else {
        tr = roll_and_resolve_treasure_room(runRng_, playerState_.relics);
    }
    treasureOutcomeSnapshot_       = tr;
    treasureOutcomeSnapshotValid_  = true;

    std::string relicIconPath;
    if (!tr.relic_id.empty()) {
        relicIconPath = resolve_image_path("assets/relics/" + tr.relic_id);
    }

    TreasureRoomDisplayData dd{};
    dd.chest_kind = tr.chest_kind;
    dd.title_line = esr_detail::utf8_to_wstring(std::string(treasure_chest_kind_label_cn(tr.chest_kind)));
    dd.has_gold = tr.grants_gold;
    dd.gold_amount = tr.gold_amount;
    dd.has_relic = !tr.relic_id.empty();
    if (dd.has_relic)
        dd.relic_line = esr_detail::utf8_to_wstring(std::string("遗物「") + relic_name_cn(tr.relic_id) + "」");
    dd.relic_icon_path = relicIconPath;

    TreasureRoomUI ui(static_cast<unsigned>(window_.getSize().x), static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") && !ui.loadFont("assets/fonts/default.ttf"))
        ui.loadFont("data/font.ttf");
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf"))
                    ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
            }
        }
    }
    ui.setup(dd);

    bool inScene = true;
    hudBattleUi_.set_hide_top_right_map_button(false);
    if (map_browse_overlay_active_)
        close_map_browse_overlay(nullptr);

    while (window_.isOpen() && inScene) {
        applyPendingVideoAndHudResize(kGameUiContext);
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }

            // HUD 右上角按钮（牌组 / 设置 / 地图浏览）
            {
                sf::Vector2f mp;
                if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                    mp = window_.mapPixelToCoords(mr->position);
                } else {
                    mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                }
                if (map_browse_overlay_active_) {
                    if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                        if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                            mapUI_.scroll(wheel->delta * 40.f);
                        continue;
                    }
                    if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                        if (m->button == sf::Mouse::Button::Left) {
                            const sf::Vector2f p = window_.mapPixelToCoords(m->position);
                            if (hit_map_browse_return_button(p))
                                close_map_browse_overlay(nullptr);
                        }
                    }
                    hudBattleUi_.handleEvent(*ev, mp);
                    poll_map_browse_toggle(nullptr);
                    continue;
                }
                hudBattleUi_.handleEvent(*ev, mp);
            }
            poll_map_browse_toggle(nullptr);

            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        // HUD 悬停
        hudBattleUi_.setMousePosition(mousePos);

        // HUD 牌组
        {
            int deckMode = 0;
            if (hudBattleUi_.pollOpenDeckViewRequest(deckMode)) {
                std::vector<CardInstance> cards = cardSystem_.get_master_deck();
                hudBattleUi_.set_deck_view_cards(std::move(cards));
                hudBattleUi_.set_deck_view_active(true);
            }
        }

        // HUD 暂停
        {
            int pauseChoice = 0;
            if (hudBattleUi_.pollPauseMenuSelection(pauseChoice)) {
                if (pauseChoice >= 41 && pauseChoice <= 43) {
                    const int slot = pauseChoice - 40;
                    lastSceneForSave_ = LastSceneKind::Treasure;
                    const std::string path = "saves/slot_" + std::to_string(slot) + ".json";
                    (void)saveRun(path);
                } else if (pauseChoice == 2) {
                    lastSceneForSave_ = LastSceneKind::Treasure;
                    saveRun();
                    exitToStartRequested_ = true;
                    return false;
                }
            }
        }

        if (ui.pollLeave()) {
            std::string detail = std::string(treasure_chest_kind_label_cn(tr.chest_kind)) + "：";
            if (tr.grants_gold) {
                playerState_.gold += tr.gold_amount;
                detail += "金币 +" + std::to_string(tr.gold_amount) + "；";
            }
            if (!tr.relic_id.empty()) {
                playerState_.relics.push_back(tr.relic_id);
                if (tr.relic_id == "strawberry") {
                    playerState_.maxHp += 7;
                    playerState_.currentHp += 7;
                    if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                } else if (tr.relic_id == "potion_belt") {
                    playerState_.potionSlotCount += 2;
                }
                detail += "遗物「" + relic_name_cn(tr.relic_id) + "」；";
            } else {
                detail += "遗物池已空；";
            }
            if (!detail.empty() && detail.back() == '；') detail.pop_back();
            statusText_ = detail;
            inScene = false;
        }

        window_.clear(sf::Color(12, 10, 18));
        if (map_browse_overlay_active_) {
            mapUI_.draw();
            drawHud();
            draw_map_browse_return_button();
        } else {
            ui.draw(window_);
            drawHud();  // 宝箱界面上方叠加全局顶栏 + 遗物栏 + 顶部按钮
        }
        window_.display();
    }

    close_map_browse_overlay(nullptr);
    return window_.isOpen();
}

bool GameFlowController::runShopScene() {
    EventShopRestUI ui(static_cast<unsigned>(window_.getSize().x),
                       static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") &&
        !ui.loadFont("assets/fonts/default.ttf")) {
        ui.loadFont("data/font.ttf");
    }
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                    if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
                    }
                }
            }
        }
    }

    ShopDisplayData shop{};
    shop.playerGold = playerState_.gold;
    shop.playerCurrentHp = playerState_.currentHp;
    shop.playerMaxHp = playerState_.maxHp;
    shop.potionSlotsMax = playerState_.potionSlotCount;
    shop.potionSlotsUsed = static_cast<int>(playerState_.potions.size());
    shop.chapterLine = L"先秦溯源 · 行旅";
    shop.removeServicePrice = 75;
    if (!playerState_.playerName.empty())
        shop.playerTitle = esr_detail::utf8_to_wstring(playerState_.playerName);

    auto refreshDeckList = [&]() {
        shop.deckForRemove.clear();
        for (const auto& inst : cardSystem_.get_master_deck()) {
            MasterDeckCardDisplay d;
            d.instanceId = inst.instanceId;
            d.cardId = inst.id;
            const CardData* data = get_card_by_id(inst.id);
            d.cardName = data ? esr_detail::utf8_to_wstring(data->name) : esr_detail::utf8_to_wstring(inst.id);
            shop.deckForRemove.push_back(d);
        }
    };

    fill_random_shop_card_offers(shop, playerState_.character, runRng_);

    static const std::vector<std::pair<std::string, int>> kShopRelics = {
        {"vajra", 180}, {"anchor", 160}, {"strawberry", 120},
    };
    for (const auto& [rid, price] : kShopRelics) {
        if (std::find(playerState_.relics.begin(), playerState_.relics.end(), rid) != playerState_.relics.end())
            continue;
        ShopRelicOffer r;
        r.id = rid;
        r.price = price;
        r.name = esr_detail::utf8_to_wstring(relic_name_cn(rid));
        shop.relicsForSale.push_back(r);
    }

    static const std::vector<std::pair<std::string, int>> kShopPotions = {
        {"block_potion", 40}, {"strength_potion", 45}, {"poison_potion", 50},
    };
    for (const auto& [pid, price] : kShopPotions) {
        ShopPotionOffer p;
        p.id = pid;
        p.price = price;
        p.name = esr_detail::utf8_to_wstring(potion_name_cn(pid));
        shop.potionsForSale.push_back(p);
    }

    refreshDeckList();

    ui.setShopData(shop);
    ui.setScreen(EventShopRestScreen::Shop);

    bool inScene = true;
    hudBattleUi_.set_hide_top_right_map_button(false);
    if (map_browse_overlay_active_)
        close_map_browse_overlay(nullptr);

    while (window_.isOpen() && inScene) {
        applyPendingVideoAndHudResize(kGameUiContext);
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    if (!ui.tryDismissShopRemoveConfirm()) inScene = false;
                }
            }

            // HUD 右上角按钮
            {
                sf::Vector2f mp;
                if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                    mp = window_.mapPixelToCoords(mr->position);
                } else {
                    mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                }
                if (map_browse_overlay_active_) {
                    if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                        if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                            mapUI_.scroll(wheel->delta * 40.f);
                        continue;
                    }
                    if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                        if (m->button == sf::Mouse::Button::Left) {
                            const sf::Vector2f p = window_.mapPixelToCoords(m->position);
                            if (hit_map_browse_return_button(p))
                                close_map_browse_overlay(nullptr);
                        }
                    }
                    hudBattleUi_.handleEvent(*ev, mp);
                    poll_map_browse_toggle(nullptr);
                    continue;
                }
                hudBattleUi_.handleEvent(*ev, mp);
            }
            poll_map_browse_toggle(nullptr);

            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        // HUD 悬停
        hudBattleUi_.setMousePosition(mousePos);

        // HUD 牌组
        {
            int deckMode = 0;
            if (hudBattleUi_.pollOpenDeckViewRequest(deckMode)) {
                std::vector<CardInstance> cards = cardSystem_.get_master_deck();
                hudBattleUi_.set_deck_view_cards(std::move(cards));
                hudBattleUi_.set_deck_view_active(true);
            }
        }

        // HUD 暂停（商店界面）
        {
            int pauseChoice = 0;
            if (hudBattleUi_.pollPauseMenuSelection(pauseChoice)) {
                if (pauseChoice >= 41 && pauseChoice <= 43) {
                    const int slot = pauseChoice - 40;
                    lastSceneForSave_ = LastSceneKind::Shop;
                    const std::string path = "saves/slot_" + std::to_string(slot) + ".json";
                    (void)saveRun(path);
                } else if (pauseChoice == 2) {
                    lastSceneForSave_ = LastSceneKind::Shop;
                    saveRun();
                    exitToStartRequested_ = true;  // 请求回到开始界面
                    return false;
                }
            }
        }

        if (ui.pollShopLeave()) {
            inScene = false;
        }

        if (ui.pollShopPayRemoveService()) {
            if (!shop.removeServiceSoldOut && !shop.removeServicePaid &&
                playerState_.gold >= shop.removeServicePrice) {
                // 只要付得起净简服务，就进入“选择要删除的牌”界面；
                // 若牌组为空，界面会显示提示但仍能正确跳转。
                playerState_.gold -= shop.removeServicePrice;
                shop.removeServicePaid = true;
                shop.playerGold = playerState_.gold;
                statusText_ = "请选择一张牌进行净简移除。";
                ui.setShopData(shop);
            } else if (!shop.removeServiceSoldOut && !shop.removeServicePaid && shop.removeServicePrice > 0 &&
                       playerState_.gold < shop.removeServicePrice) {
                statusText_ = "金币不足，无法购买净简服务。";
                hudBattleUi_.showTip(L"金币不足", 1.8f);
            }
        }

        CardId buyId;
        if (ui.pollShopBuyCard(buyId)) {
            int price = 0;
            for (const auto& offer : shop.forSale) {
                if (offer.id == buyId) {
                    price = offer.price;
                    break;
                }
            }
            if (price == 0) {
                for (const auto& offer : shop.colorlessForSale) {
                    if (offer.id == buyId) {
                        price = offer.price;
                        break;
                    }
                }
            }
            const CardData* cardData = get_card_by_id(buyId);
            if (cardData && price > 0 && playerState_.gold < price) {
                statusText_ = "金币不足，无法购买该牌。";
                hudBattleUi_.showTip(L"金币不足", 1.8f);
            } else if (cardData && price > 0 && playerState_.gold >= price) {
                playerState_.gold -= price;
                cardSystem_.add_to_master_deck(buyId);
                shop.playerGold = playerState_.gold;
                shop.forSale.erase(
                    std::remove_if(shop.forSale.begin(), shop.forSale.end(),
                                   [&buyId](const ShopCardOffer& o) { return o.id == buyId; }),
                    shop.forSale.end());
                shop.colorlessForSale.erase(
                    std::remove_if(shop.colorlessForSale.begin(), shop.colorlessForSale.end(),
                                   [&buyId](const ShopCardOffer& o) { return o.id == buyId; }),
                    shop.colorlessForSale.end());
                refreshDeckList();
                ui.setShopData(shop);
            }
        }

        int relicIdx = -1;
        if (ui.pollShopBuyRelic(relicIdx)) {
            if (relicIdx >= 0 && relicIdx < static_cast<int>(shop.relicsForSale.size())) {
                const ShopRelicOffer& o = shop.relicsForSale[static_cast<size_t>(relicIdx)];
                const bool alreadyOwned =
                    std::find(playerState_.relics.begin(), playerState_.relics.end(), o.id) !=
                    playerState_.relics.end();
                if (playerState_.gold < o.price) {
                    statusText_ = "金币不足，无法购买遗物。";
                    hudBattleUi_.showTip(L"金币不足", 1.8f);
                } else if (alreadyOwned) {
                    statusText_ = "已拥有该遗物。";
                    hudBattleUi_.showTip(L"已拥有该遗物", 1.8f);
                } else {
                    playerState_.gold -= o.price;
                    playerState_.relics.push_back(o.id);
                    if (o.id == "potion_belt")
                        playerState_.potionSlotCount += 2;
                    shop.relicsForSale.erase(shop.relicsForSale.begin() + relicIdx);
                    shop.playerGold = playerState_.gold;
                    shop.potionSlotsMax = playerState_.potionSlotCount;
                    ui.setShopData(shop);
                }
            }
        }

        int potIdx = -1;
        if (ui.pollShopBuyPotion(potIdx)) {
            if (potIdx >= 0 && potIdx < static_cast<int>(shop.potionsForSale.size())) {
                const ShopPotionOffer& o = shop.potionsForSale[static_cast<size_t>(potIdx)];
                if (playerState_.gold < o.price) {
                    statusText_ = "金币不足，无法购买灵液。";
                    hudBattleUi_.showTip(L"金币不足", 1.8f);
                } else if (static_cast<int>(playerState_.potions.size()) >= playerState_.potionSlotCount) {
                    statusText_ = "灵液槽已满，请先使用或丢弃灵液后再购买。";
                    hudBattleUi_.showTip(L"灵液槽已满", 2.0f);
                } else {
                    playerState_.gold -= o.price;
                    playerState_.potions.push_back(o.id);
                    shop.potionsForSale.erase(shop.potionsForSale.begin() + potIdx);
                    shop.playerGold = playerState_.gold;
                    shop.potionSlotsUsed = static_cast<int>(playerState_.potions.size());
                    ui.setShopData(shop);
                }
            }
        }

        InstanceId removeId = 0;
        if (ui.pollShopRemoveCard(removeId)) {
            CardId removedId;
            if (removeId > 0 && cardSystem_.remove_from_master_deck(removeId, &removedId)) {
                if (removedId == "parasite" || removedId == "parasite+") {
                    playerState_.maxHp = std::max(1, playerState_.maxHp - 3);
                    if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                }
                shop.removeServicePaid = false;
                shop.removeServiceSoldOut = true;
                refreshDeckList();
                ui.setShopData(shop);
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        if (map_browse_overlay_active_) {
            mapUI_.draw();
            drawHud();
            draw_map_browse_return_button();
        } else {
            ui.draw(window_);
            drawHud();  // 商店界面上方叠加全局顶栏 + 遗物栏
        }
        window_.display();
    }

    close_map_browse_overlay(nullptr);
    // 商店内已无选牌删牌界面：若曾付净简费但未完成移除，离开时退还
    if (shop.removeServicePaid && !shop.removeServiceSoldOut) {
        playerState_.gold += shop.removeServicePrice;
    }

    return true;
}

bool GameFlowController::runRestScene() {
    EventShopRestUI ui(static_cast<unsigned>(window_.getSize().x),
                       static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") &&
        !ui.loadFont("assets/fonts/default.ttf")) {
        ui.loadFont("data/font.ttf");
    }
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                    if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
                    }
                }
            }
        }
    }

    RestDisplayData rest{};
    rest.healAmount = std::max(12, playerState_.maxHp / 5);
    for (const auto& inst : cardSystem_.get_master_deck()) {
        MasterDeckCardDisplay d;
        d.instanceId = inst.instanceId;
        d.cardId = inst.id;
        const CardData* data = get_card_by_id(inst.id);
        std::string name = data ? data->name : inst.id;
        d.cardName = std::wstring(name.begin(), name.end());
        rest.deckForUpgrade.push_back(d);
    }

    ui.setRestData(rest);
    ui.setScreen(EventShopRestScreen::Rest);

    bool inScene = true;
    hudBattleUi_.set_hide_top_right_map_button(false);
    if (map_browse_overlay_active_)
        close_map_browse_overlay(nullptr);

    while (window_.isOpen() && inScene) {
        applyPendingVideoAndHudResize(kGameUiContext);
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    if (!ui.tryDismissRestForgeUpgradeConfirm()) inScene = false;
                }
            }

            // HUD 右上角按钮
            {
                sf::Vector2f mp;
                if (const auto* m2 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                } else if (const auto* mr = ev->getIf<sf::Event::MouseMoved>()) {
                    mp = window_.mapPixelToCoords(mr->position);
                } else {
                    mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                }
                if (map_browse_overlay_active_) {
                    if (const auto* wheel = ev->getIf<sf::Event::MouseWheelScrolled>()) {
                        if (wheel->wheel == sf::Mouse::Wheel::Vertical)
                            mapUI_.scroll(wheel->delta * 40.f);
                        continue;
                    }
                    if (const auto* m = ev->getIf<sf::Event::MouseButtonPressed>()) {
                        if (m->button == sf::Mouse::Button::Left) {
                            const sf::Vector2f p = window_.mapPixelToCoords(m->position);
                            if (hit_map_browse_return_button(p))
                                close_map_browse_overlay(nullptr);
                        }
                    }
                    hudBattleUi_.handleEvent(*ev, mp);
                    poll_map_browse_toggle(nullptr);
                    continue;
                }
                hudBattleUi_.handleEvent(*ev, mp);
            }
            poll_map_browse_toggle(nullptr);

            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mousePos = window_.mapPixelToCoords(mp->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>()) {
                mousePos = window_.mapPixelToCoords(mr->position);
            }
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);
        ui.syncRestForgeScrollbarDrag(mousePos);

        // HUD 悬停
        hudBattleUi_.setMousePosition(mousePos);

        // HUD 牌组
        {
            int deckMode = 0;
            if (hudBattleUi_.pollOpenDeckViewRequest(deckMode)) {
                std::vector<CardInstance> cards = cardSystem_.get_master_deck();
                hudBattleUi_.set_deck_view_cards(std::move(cards));
                hudBattleUi_.set_deck_view_active(true);
            }
        }

        // HUD 暂停（休息界面）
        {
            int pauseChoice = 0;
            if (hudBattleUi_.pollPauseMenuSelection(pauseChoice)) {
                if (pauseChoice >= 41 && pauseChoice <= 43) {
                    const int slot = pauseChoice - 40;
                    lastSceneForSave_ = LastSceneKind::Rest;
                    const std::string path = "saves/slot_" + std::to_string(slot) + ".json";
                    (void)saveRun(path);
                } else if (pauseChoice == 2) {
                    lastSceneForSave_ = LastSceneKind::Rest;
                    saveRun();
                    exitToStartRequested_ = true;  // 请求回到开始界面
                    return false;
                }
            }
        }

        if (ui.pollRestHeal()) {
            playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + rest.healAmount);
            inScene = false;
        }

        InstanceId upgradeId = 0;
        if (ui.pollRestUpgradeCard(upgradeId)) {
            if (upgradeId > 0 && cardSystem_.upgrade_card_in_master_deck(upgradeId)) {
                // 刷新升级列表显示名称
                rest.deckForUpgrade.clear();
                for (const auto& inst : cardSystem_.get_master_deck()) {
                    MasterDeckCardDisplay d;
                    d.instanceId = inst.instanceId;
                    d.cardId = inst.id;
                    const CardData* data = get_card_by_id(inst.id);
                    std::string name = data ? data->name : inst.id;
                    d.cardName = std::wstring(name.begin(), name.end());
                    rest.deckForUpgrade.push_back(d);
                }
                ui.setRestData(rest);
                inScene = false;
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        if (map_browse_overlay_active_) {
            mapUI_.draw();
            drawHud();
            draw_map_browse_return_button();
        } else {
            ui.draw(window_);
            drawHud();  // 休息界面上方叠加全局顶栏 + 遗物栏
        }
        window_.display();
    }

    close_map_browse_overlay(nullptr);
    return true;
}

int GameFlowController::firstAliveMonsterIndex(const BattleState& state) const {
    for (int i = 0; i < static_cast<int>(state.monsters.size()); ++i) {
        if (state.monsters[static_cast<size_t>(i)].currentHp > 0) return i;
    }
    return -1;
}

void GameFlowController::drawHud() {
    // 利用 BattleUI 的顶栏与遗物栏绘制逻辑，构造一个最小 BattleStateSnapshot
    BattleStateSnapshot snap{};
    snap.playerName = playerState_.playerName;
    snap.character  = playerState_.character;
    snap.currentHp  = playerState_.currentHp;
    snap.maxHp      = playerState_.maxHp;
    snap.gold       = playerState_.gold;
    snap.potionSlotCount = playerState_.potionSlotCount;
    snap.potions    = playerState_.potions;
    snap.relics     = playerState_.relics;

    hudBattleUi_.set_top_bar_map_floor(mapEngine_.get_current_layer(), mapEngine_.get_total_layers());
    hudBattleUi_.drawGlobalHud(window_, snap);
}

std::string GameFlowController::nodeTypeToString(NodeType nodeType) const {
    switch (nodeType) {
    case NodeType::Enemy: return "Enemy";
    case NodeType::Elite: return "Elite";
    case NodeType::Event: return "Event";
    case NodeType::Rest: return "Rest";
    case NodeType::Merchant: return "Merchant";
    case NodeType::Treasure: return "Treasure";
    case NodeType::Boss: return "Boss";
    default: return "Unknown";
    }
}

} // namespace tce
