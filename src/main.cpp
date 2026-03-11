/**
 * 《溯源者：文明环境》课设 - 程序入口
 * 模块：MapEngine(A)、BattleEngine(B)、CardSystem(C)、EventEngine(D)、DataLayer(E)
 *
 * 当前：战斗调试 - 接引擎与 Snapshot，点击结束回合即调用 engine.end_turn()
 */

#include <SFML/Graphics.hpp>
#include <optional>
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#include "BattleEngine/BattleEngine.hpp"
#include "CardSystem/CardSystem.hpp"
#include "DataLayer/DataLayer.hpp"
#include "Effects/CardEffects.hpp"
#include "Effects/StatusEffects.hpp"

static void runBattleUI(sf::RenderWindow& window) {
    using namespace tce;

    CardSystem card_system([](CardId id) { return get_card_by_id(id); });
    BattleEngine engine(
        card_system,
        [](MonsterId id) { return get_monster_by_id(id); },
        [](CardId id) { return get_card_by_id(id); }
    );

    register_all_card_effects(card_system);
    register_all_status_effects(engine);

    PlayerBattleState player;
    player.playerName = "Telys";
    player.character  = "Ironclad";
    player.currentHp  = 80;
    player.maxHp      = 80;
    player.energy     = 3;
    player.maxEnergy  = 3;
    player.gold       = 99;
    player.cardsToDrawPerTurn = 5;

    std::vector<CardId> deck = {"strike", "strike", "strike", "defend", "defend", "bash"};
    std::vector<MonsterId> monsters = {"cultist"};
    // Mock 5 个遗物以便顶栏遗物行显示多格
    std::vector<RelicId> relics = {"burning_blood", "relic_2", "relic_3", "relic_4", "relic_5"};

    engine.start_battle(monsters, player, deck, relics);

    // Mock：玩家初始获得 6 层金属化
    engine.apply_status_to_player("metallicize", 6, -1);

    BattleUI ui(static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));
    // 主字体（英文/数字）
    if (!ui.loadFont("assets/fonts/Sanji.ttf"))
        if (!ui.loadFont("assets/fonts/default.ttf"))
            ui.loadFont("data/font.ttf");
    // 中文字体（显示“结束回合”“玩家”“地图”等），优先系统字体
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf"))  // 若 Sanji 含中文可省去下面
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc"))   // 微软雅黑
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf"))  // 黑体
                ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");   // 宋体

    while (window.isOpen()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>())
                window.close();
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (ui.handleEvent(*ev, mousePos))
                engine.end_turn();   // 点击结束回合按钮即调用
        }
        if (!window.isOpen()) break;

        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        ui.setMousePosition(mousePos);

        // 若 UI 中有“打出牌”的请求，则调用战斗引擎出牌
        int handIndex = -1;
        int targetMonsterIndex = -1;
        if (ui.pollPlayCardRequest(handIndex, targetMonsterIndex)) {
            engine.play_card(handIndex, targetMonsterIndex);
        }

        // 先根据当前阶段获取快照并绘制 UI，再推进到下一个阶段
        BattleStateSnapshot snapshot = engine.get_battle_state();
        SnapshotBattleUIDataProvider adapter(&snapshot);
        window.clear(sf::Color(28, 26, 32));
        ui.draw(window, adapter);
        window.display();

        // 每帧推进一次回合阶段，用于分阶段结算与调试显示
        engine.step_turn_phase();
    }
}

int main() {
    const unsigned int winW = 1920, winH = 1080;
    sf::RenderWindow window(sf::VideoMode({winW, winH}), "Battle Debug - Tracer Civilization");

    runBattleUI(window);
    return 0;
}
/**
 * 《溯源者：文明环境》课设 - 程序入口
 * 模块：MapEngine(A)、BattleEngine(B)、CardSystem(C)、EventEngine(D)、DataLayer(E)
<<<<<<< HEAD
 *
 * 当前：战斗调试 - 接引擎与 Snapshot，点击结束回合即调用 engine.end_turn()
 */

#include <SFML/Graphics.hpp>
#include <optional>
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#include "BattleEngine/BattleEngine.hpp"
#include "CardSystem/CardSystem.hpp"
#include "DataLayer/DataLayer.hpp"
#include "Effects/CardEffects.hpp"
#include "Effects/StatusEffects.hpp"

static void runBattleUI(sf::RenderWindow& window) {
    using namespace tce;

    CardSystem card_system([](CardId id) { return get_card_by_id(id); });
    BattleEngine engine(
        card_system,
        [](MonsterId id) { return get_monster_by_id(id); },
        [](CardId id) { return get_card_by_id(id); }
    );

    register_all_card_effects(card_system);
    register_all_status_effects(engine);

    PlayerBattleState player;
    player.playerName = "Telys";
    player.character  = "Ironclad";
    player.currentHp  = 80;
    player.maxHp      = 80;
    player.energy     = 3;
    player.maxEnergy  = 3;
    player.gold       = 99;
    player.cardsToDrawPerTurn = 5;

    std::vector<CardId> deck = {"strike", "strike", "strike", "defend", "defend", "bash"};
    std::vector<MonsterId> monsters = {"cultist"};
    // Mock 5 个遗物以便顶栏遗物行显示多格
    std::vector<RelicId> relics = {"burning_blood", "relic_2", "relic_3", "relic_4", "relic_5"};

    engine.start_battle(monsters, player, deck, relics);

    // Mock：玩家初始获得 6 层金属化
    engine.apply_status_to_player("metallicize", 6, -1);

    BattleUI ui(static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));
    // 主字体（英文/数字）
    if (!ui.loadFont("assets/fonts/Sanji.ttf"))
        if (!ui.loadFont("assets/fonts/default.ttf"))
            ui.loadFont("data/font.ttf");
    // 中文字体（显示“结束回合”“玩家”“地图”等），优先系统字体
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf"))  // 若 Sanji 含中文可省去下面
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc"))   // 微软雅黑
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf"))  // 黑体
                ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");   // 宋体

    while (window.isOpen()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>())
                window.close();
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (ui.handleEvent(*ev, mousePos))
                engine.end_turn();   // 点击结束回合按钮即调用
        }
        if (!window.isOpen()) break;

        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        ui.setMousePosition(mousePos);

        // 若 UI 中有“打出牌”的请求，则调用战斗引擎出牌
        int handIndex = -1;
        int targetMonsterIndex = -1;
        if (ui.pollPlayCardRequest(handIndex, targetMonsterIndex)) {
            engine.play_card(handIndex, targetMonsterIndex);
        }

        // 先根据当前阶段获取快照并绘制 UI，再推进到下一个阶段
        BattleStateSnapshot snapshot = engine.get_battle_state();
        SnapshotBattleUIDataProvider adapter(&snapshot);
        window.clear(sf::Color(28, 26, 32));
        ui.draw(window, adapter);
        window.display();

        // 每帧推进一次回合阶段，用于分阶段结算与调试显示
        engine.step_turn_phase();
    }
}

int main() {
    const unsigned int winW = 1920, winH = 1080;
    sf::RenderWindow window(sf::VideoMode({winW, winH}), "Battle Debug - Tracer Civilization");

    runBattleUI(window);
=======
 * 运行本程序可一次验证 E→C→B 数据加载与联调是否正常。
 */

#include <iostream>
#include "DataLayer/DataLayer.h"
#include "BattleEngine/BattleEngine.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

static bool test_data_layer(DataLayer::DataLayerImpl& data) {
    if (!data.load_cards(".")) {
        std::cout << "[E] load_cards failed (check data/cards.json and cwd)\n";
        return false;
    }
    if (!data.load_monsters(".")) {
        std::cout << "[E] load_monsters failed\n";
        return false;
    }
    if (!data.load_events(".")) {
        std::cout << "[E] load_events failed\n";
        return false;
    }
    std::cout << "[E] DataLayer: cards/monsters/events loaded.\n";
    const tce::CardData* c = data.get_card_by_id("card_001");
    if (c)
        std::cout << "     get_card_by_id(card_001): " << c->name << " cost=" << c->cost << " " << tce::to_string(c->rarity) << "\n";
    const tce::MonsterData* m = data.get_monster_by_id("monster_boss_1");
    if (m) std::cout << "     get_monster_by_id(monster_boss_1): " << m->name << " HP=" << m->maxHp << " (" << tce::to_string(m->type) << ")\n";
    const DataLayer::Event* e = data.get_event_by_id("event_001");
    if (e) std::cout << "     get_event_by_id(event_001): " << e->title << " options=" << e->options.size() << "\n";
    std::vector<DataLayer::CardId> ids = { "card_004", "card_001", "card_015" };
    ids = data.sort_cards_by_rarity(ids);
    std::cout << "     sort_cards_by_rarity: ";
    for (const auto& id : ids) {
        const tce::CardData* p = data.get_card_by_id(id);
        if (p) std::cout << p->name << "(" << tce::to_string(p->rarity) << ") ";
    }
    std::cout << "\n[E] DataLayer OK\n";
    return true;
}

static bool test_card_system() {
    tce::CardSystem cs(tce::get_card_by_id);
    std::vector<tce::CardId> deck = { "card_001", "card_002", "card_007", "card_008", "card_013" };
    cs.init_deck(deck);
    cs.draw_cards(5);
    int hand = static_cast<int>(cs.get_hand().size());
    int draw = cs.get_deck_size();
    if (hand <= 0 || (hand + draw) != 5) {
        std::cout << "[C] CardSystem: init_deck/draw_cards unexpected (hand=" << hand << " draw=" << draw << ")\n";
        return false;
    }
    std::cout << "[C] CardSystem: init_deck + draw_cards(5) -> hand=" << hand << " drawPile=" << draw << "\n";
    std::cout << "[C] CardSystem OK\n";
    return true;
}

static bool test_battle_engine(DataLayer::DataLayerImpl& data) {
    tce::CardSystem cs(tce::get_card_by_id);
    tce::BattleEngine engine(cs, tce::get_monster_by_id, tce::get_card_by_id);
    tce::PlayerBattleState player;
    player.playerName = "测试";
    player.character = "default";
    player.currentHp = player.maxHp = 70;
    player.energy = player.maxEnergy = 3;
    player.block = 0;
    player.gold = 0;
    std::vector<tce::MonsterId> monster_ids = { "monster_001" };
    std::vector<tce::CardId> deck = { "card_001", "card_002", "card_007" };
    engine.start_battle(monster_ids, player, deck, {});
    tce::BattleStateSnapshot snap = engine.get_battle_state();
    if (snap.monsters.empty() || snap.playerName != "测试") {
        std::cout << "[B] BattleEngine: start_battle/get_battle_state unexpected\n";
        return false;
    }
    std::cout << "[B] BattleEngine: start_battle -> player " << snap.playerName << " vs " << snap.monsters.size() << " monster(s)\n";
    // 出一张牌、结束回合，确保不崩
    if (!snap.hand.empty() && snap.monsters[0].currentHp > 0)
        engine.play_card(0, 0);
    engine.end_turn();
    bool over = engine.is_battle_over();
    std::cout << "[B] play_card + end_turn done, is_battle_over=" << (over ? "true" : "false") << "\n";
    std::cout << "[B] BattleEngine OK\n";
    return true;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    std::cout << "--- 模块联调测试 (E→C→B) ---\n";
    DataLayer::DataLayerImpl data;
    if (!test_data_layer(data)) return 1;
    if (!test_card_system()) return 2;
    if (!test_battle_engine(data)) return 3;
    std::cout << "--- Tracer: Civilization Environment - 全部通过 ---\n";
>>>>>>> origin/main
    return 0;
}
