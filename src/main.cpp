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
#include "CardSystem/DeckViewCollection.hpp"
#include "DataLayer/DataLayer.hpp"
#include "DataLayer/DataLayer.h"
#include "Effects/CardEffects.hpp"
#include "Effects/StatusEffects.hpp"

static void runBattleUI(sf::RenderWindow& window) {
    using namespace tce;

    // 加载卡牌/怪物 JSON 到 tce::s_cards / tce::s_monsters（get_card_by_id/get_monster_by_id 会查这两张表）
    // 默认路径：data/cards.json、data/monsters.json
    DataLayer::DataLayerImpl data;
    data.load_cards("");
    data.load_monsters("");

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

    // Mock：永久牌组（master deck）——4 张防御 + 1 张全身撞击+ + 2 张剑柄打击
    card_system.init_master_deck({
        "defend",
        "defend",
        "defend",
        "defend",
        "body_slam+",
        "pommel_strike",
        "pommel_strike"
    });
    std::vector<MonsterId> monsters = {"cultist"};
    // Mock 5 个遗物以便顶栏遗物行显示多格
    std::vector<RelicId> relics = {"burning_blood", "relic_2", "relic_3", "relic_4", "relic_5"};

    engine.start_battle(monsters, player, card_system.get_master_deck_card_ids(), relics);

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

        // 打开牌组界面：1=整个牌组(右上角牌组)，2=抽牌堆(左下角)，3=弃牌堆(右下角)；空则提示不打开
        int deckViewMode = 0;
        if (ui.pollOpenDeckViewRequest(deckViewMode)) {
            const auto mode = static_cast<DeckViewMode>(deckViewMode);
            std::vector<CardInstance> cards = collect_deck_view_cards(card_system, mode);
            if (cards.empty()) {
                ui.showTip(deck_view_empty_tip(mode));
            } else {
                ui.set_deck_view_cards(std::move(cards));
                ui.set_deck_view_active(true);
            }
        }

        // 先根据当前阶段获取快照并绘制 UI，再推进到下一个阶段
        BattleStateSnapshot snapshot = engine.get_battle_state();
        SnapshotBattleUIDataProvider adapter(&snapshot);
        window.clear(sf::Color(28, 26, 32));
        ui.draw(window, adapter);
        window.display();

        // 牌组界面打开时不推进回合；否则每帧推进一次回合阶段
        if (!ui.is_deck_view_active())
            engine.step_turn_phase();
    }
}

int main() {
    const unsigned int winW = 1920, winH = 1080;
    sf::RenderWindow window(sf::VideoMode({winW, winH}), "Battle Debug - Tracer Civilization");

    runBattleUI(window);
    return 0;
}
