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
    std::vector<RelicId> relics = {"burning_blood"};

    engine.start_battle(monsters, player, deck, relics);

    BattleUI ui(static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf"))
        if (!ui.loadFont("assets/fonts/default.ttf"))
            ui.loadFont("data/font.ttf");

    while (window.isOpen()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>())
                window.close();
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (ui.handleEvent(*ev, mousePos))
                engine.end_turn();   // 点击结束回合按钮即调用
        }
        if (!window.isOpen()) break;

        BattleStateSnapshot snapshot = engine.get_battle_state();
        SnapshotBattleUIDataProvider adapter(&snapshot);
        window.clear(sf::Color(28, 26, 32));
        ui.draw(window, adapter);
        window.display();
    }
}

int main() {
    const unsigned int winW = 1920, winH = 1080;
    sf::RenderWindow window(sf::VideoMode({winW, winH}), "Battle Debug - Tracer Civilization");

    runBattleUI(window);
    return 0;
}
