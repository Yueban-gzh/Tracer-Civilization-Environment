/**
 * 《溯源者：文明环境》课设 - 程序入口
 * 模块：MapEngine(A)、BattleEngine(B)、CardSystem(C)、EventEngine(D)、DataLayer(E)
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
    return 0;
}
