/**
 * 《溯源者：文明环境》课设 - 程序入口
 * 模块：MapEngine(A)、BattleEngine(B)、CardSystem(C)、EventEngine(D)、DataLayer(E)
 */

#include <iostream>
#include "DataLayer/DataLayer.h"

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    // 控制台使用 UTF-8，否则从 JSON 读出的中文会乱码
    SetConsoleOutputCP(65001);
#endif
    using namespace DataLayer;
    DataLayerImpl data;
    // 从当前工作目录加载（运行时可设为项目根目录，使 data/ 可找到）
    if (!data.load_cards(".")) {
        std::cout << "DataLayer: load_cards failed (check data/cards.json and cwd)\n";
        return 1;
    }
    if (!data.load_monsters(".")) {
        std::cout << "DataLayer: load_monsters failed\n";
        return 1;
    }
    if (!data.load_events(".")) {
        std::cout << "DataLayer: load_events failed\n";
        return 1;
    }
    std::cout << "DataLayer: cards/monsters/events loaded.\n";
    const Card* c = data.get_card_by_id("card_001");
    if (c) {
        std::cout << "get_card_by_id(card_001): " << c->name << " cost=" << c->cost
                  << " " << c->rarity << " - " << c->description.substr(0, 40) << "...\n";
    }
    const Monster* m = data.get_monster_by_id("monster_boss_1");
    if (m) std::cout << "get_monster_by_id(monster_boss_1): " << m->name << " HP=" << m->maxHp << " (Boss)\n";
    const Event* e = data.get_event_by_id("event_001");
    if (e) std::cout << "get_event_by_id(event_001): " << e->title << " options=" << e->options.size() << "\n";
    std::vector<CardId> ids = { "card_004", "card_001", "card_015" };  // rare, common, rare
    ids = data.sort_cards_by_rarity(ids);
    std::cout << "sort_cards_by_rarity: ";
    for (const auto& id : ids) {
        const Card* p = data.get_card_by_id(id);
        if (p) std::cout << p->name << "(" << p->rarity << ") ";
    }
    std::cout << "\nTracer: Civilization Environment - OK\n";
    return 0;
}
