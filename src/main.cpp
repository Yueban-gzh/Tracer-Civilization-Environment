/**
 * Tracer: Civilization Environment - entry point
 * Modules: MapEngine(A), BattleEngine(B), CardSystem(C), EventEngine(D), DataLayer(E)
 * This program runs a simple E->C->B->D integration test.
 */

#include <iostream>
#include <optional>
#include "DataLayer/DataLayer.h"
#include "BattleEngine/BattleEngine.hpp"
#include "EventEngine/EventEngine.hpp"

#ifdef TEST_BATTLE_UI
#include <SFML/Graphics.hpp>
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#endif

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
    std::cout << "\n";
    // 排行榜排序
    std::vector<DataLayer::DataLayerImpl::LeaderboardEntry> lb = {
        {"p1", 100, 1}, {"p2", 200, 2}, {"p3", 150, 3}
    };
    lb = data.sort_leaderboard(std::move(lb));
    if (lb.size() != 3 || lb[0].score != 200 || lb[1].score != 150 || lb[2].score != 100) {
        std::cout << "[E] sort_leaderboard unexpected\n";
        return false;
    }
    std::cout << "     sort_leaderboard: top=" << lb[0].playerId << " score=" << lb[0].score << "\n";
    std::cout << "[E] DataLayer OK\n";
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
    player.playerName = "test";
    player.character = "default";
    player.currentHp = player.maxHp = 70;
    player.energy = player.maxEnergy = 3;
    player.block = 0;
    player.gold = 0;
    std::vector<tce::MonsterId> monster_ids = { "monster_001" };
    std::vector<tce::CardId> deck = { "card_001", "card_002", "card_007" };
    engine.start_battle(monster_ids, player, deck, {});
    tce::BattleStateSnapshot snap = engine.get_battle_state();
    if (snap.monsters.empty() || snap.playerName != "test") {
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

static bool test_event_engine(DataLayer::DataLayerImpl& data) {
    auto get_event = [&data](DataLayer::EventId id) { return data.get_event_by_id(id); };
    tce::EventEngine ev(get_event);

    ev.start_event("event_001");
    const DataLayer::Event* cur = ev.get_current_event();
    if (!cur) {
        std::cout << "[D] EventEngine: start_event(event_001) unexpected (null)\n";
        return false;
    }
    std::cout << "[D] EventEngine: start_event -> title=" << cur->title
              << " options=" << cur->options.size() << "\n";

    if (!ev.choose_option(0)) {
        std::cout << "[D] EventEngine: choose_option(0) failed\n";
        return false;
    }
    cur = ev.get_current_event();
    if (!cur) {
        std::cout << "[D] EventEngine: choose_option(0) next unexpected (null)\n";
        return false;
    }
    std::cout << "[D] EventEngine: choose_option(0) -> title=" << cur->title << "\n";

    if (!ev.choose_option(0)) {
        std::cout << "[D] EventEngine: choose_option(0) result failed\n";
        return false;
    }
    DataLayer::EventResult res;
    if (!ev.get_event_result(res) || res.type != "heal" || res.value != 10) {
        std::cout << "[D] EventEngine: get_event_result unexpected\n";
        return false;
    }
    std::cout << "[D] EventEngine: get_event_result -> type=" << res.type
              << " value=" << res.value << "\n";

    // 先序/层序遍历
    int pre_count = 0, level_count = 0;
    ev.traverse_preorder("event_001", [&pre_count](const DataLayer::Event& e) { (void)e; ++pre_count; });
    ev.traverse_level_order("event_001", [&level_count](const DataLayer::Event& e) { (void)e; ++level_count; });
    std::cout << "[D] EventEngine: traverse_preorder(event_001) visited " << pre_count << " nodes, level_order " << level_count << " nodes\n";
    std::cout << "[D] EventEngine OK\n";
    return true;
}

#ifdef TEST_BATTLE_UI
static bool test_battle_ui(DataLayer::DataLayerImpl& data) {
    tce::CardSystem cs(tce::get_card_by_id);
    tce::BattleEngine engine(cs, tce::get_monster_by_id, tce::get_card_by_id);
    tce::PlayerBattleState player;
    player.playerName = "UI Test";
    player.character = "default";
    player.currentHp = player.maxHp = 70;
    player.energy = player.maxEnergy = 3;
    player.block = 0;
    player.gold = 99;
    std::vector<tce::MonsterId> monster_ids = { "monster_001" };
    std::vector<tce::CardId> deck = { "card_001", "card_002", "card_007" };
    engine.start_battle(monster_ids, player, deck, {});
    tce::BattleStateSnapshot snap = engine.get_battle_state();

    sf::RenderWindow window(sf::VideoMode(800, 600), "Battle UI Test", sf::Style::Titlebar | sf::Style::Close);
    tce::BattleUI ui(800, 600);
    tce::SnapshotBattleUIDataProvider provider(&snap);
    ui.draw(window, provider);
    window.display();
    std::cout << "[UI] Battle UI: one frame drawn (window open briefly).\n";
    window.close();
    std::cout << "[UI] Battle UI OK\n";
    return true;
}
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
#endif
    std::cout << "--- Integration test (E->C->B->D) ---\n";
    DataLayer::DataLayerImpl data;
    if (!test_data_layer(data)) return 1;
    if (!test_card_system()) return 2;
    if (!test_battle_engine(data)) return 3;
    if (!test_event_engine(data)) return 4;
#ifdef TEST_BATTLE_UI
    if (!test_battle_ui(data)) return 5;
#endif
    std::cout << "--- Tracer: Civilization Environment - 全部通过 ---\n";
    return 0;
}
