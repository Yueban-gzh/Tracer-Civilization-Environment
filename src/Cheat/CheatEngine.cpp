/**
 * 金手指模块实现 - 解析命令并调用 BattleEngine 接口
 */
#include "../../include/Cheat/CheatEngine.hpp"
#include "../../include/BattleCoreRefactor/BattleEngine.hpp"
#include "../../include/CardSystem/CardSystem.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace tce {

CheatEngine::CheatEngine(BattleEngine* engine, CardSystem* card_system)
    : engine_(engine), card_system_(card_system) {}

void CheatEngine::trim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
}

bool CheatEngine::parse_int(const std::string& s, int& out) {
    try {
        out = std::stoi(s);
        return true;
    } catch (...) {
        return false;
    }
}

int CheatEngine::execute_line(const std::string& line) {
    std::string ln = line;
    trim(ln);
    if (ln.empty() || ln[0] == '#') return -1;  // 空行或注释跳过

    std::istringstream iss(ln);
    std::string cmd;
    iss >> cmd;

    if (cmd == "player_hp") {
        int val; if (!(iss >> val)) return 0;
        engine_->cheat_set_player_hp(val);
        return 1;
    }
    if (cmd == "player_max_hp") {
        int val; if (!(iss >> val)) return 0;
        engine_->cheat_set_player_max_hp(val);
        return 1;
    }
    if (cmd == "player_block") {
        int val; if (!(iss >> val)) return 0;
        engine_->cheat_set_player_block(val);
        return 1;
    }
    if (cmd == "player_energy") {
        int val; if (!(iss >> val)) return 0;
        engine_->cheat_set_player_energy(val);
        return 1;
    }
    if (cmd == "player_gold") {
        int val; if (!(iss >> val)) return 0;
        engine_->cheat_set_player_gold(val);
        return 1;
    }
    if (cmd == "player_status") {
        std::string id, s2, s3;
        if (!(iss >> id >> s2 >> s3)) return 0;
        int stacks, duration;
        if (!parse_int(s2, stacks) || !parse_int(s3, duration)) return 0;
        engine_->cheat_set_player_status(id, stacks, duration);
        return 1;
    }
    if (cmd == "player_status_remove") {
        std::string id; std::getline(iss, id); trim(id);
        if (id.empty()) return 0;
        engine_->cheat_remove_player_status(id);
        return 1;
    }
    if (cmd == "monster_hp") {
        int idx, hp;
        if (!(iss >> idx >> hp)) return 0;
        engine_->cheat_set_monster_hp(idx, hp);
        return 1;
    }
    if (cmd == "monster_max_hp") {
        int idx, hp;
        if (!(iss >> idx >> hp)) return 0;
        engine_->cheat_set_monster_max_hp(idx, hp);
        return 1;
    }
    if (cmd == "monster_status") {
        int idx;
        std::string id;
        int stacks, duration;
        if (!(iss >> idx >> id >> stacks >> duration)) return 0;
        engine_->cheat_set_monster_status(idx, id, stacks, duration);
        return 1;
    }
    if (cmd == "monster_status_remove") {
        int idx;
        std::string id;
        if (!(iss >> idx >> id)) return 0;
        engine_->cheat_remove_monster_status(idx, id);
        return 1;
    }
    if (cmd == "monster_kill") {
        int idx;
        if (!(iss >> idx)) return 0;
        engine_->cheat_kill_monster(idx);
        return 1;
    }
    if (cmd == "add_relic") {
        std::string id; std::getline(iss, id); trim(id);
        if (id.empty()) return 0;
        engine_->cheat_add_relic(id);
        return 1;
    }
    if (cmd == "remove_relic") {
        std::string id; std::getline(iss, id); trim(id);
        if (id.empty()) return 0;
        engine_->cheat_remove_relic(id);
        return 1;
    }
    if (cmd == "add_potion") {
        std::string id; std::getline(iss, id); trim(id);
        if (id.empty()) return 0;
        engine_->cheat_add_potion(id);
        return 1;
    }
    if (cmd == "remove_potion") {
        int slot;
        if (!(iss >> slot)) return 0;
        engine_->cheat_remove_potion(slot);
        return 1;
    }
    if (cmd == "add_hand") {
        std::string id; std::getline(iss, id); trim(id);
        if (id.empty()) return 0;
        engine_->cheat_add_hand(id);
        return 1;
    }
    if (cmd == "remove_hand") {
        std::string id; std::getline(iss, id); trim(id);
        if (id.empty()) return 0;
        engine_->cheat_remove_hand(id);
        return 1;
    }
    return 0;
}

} // namespace tce
