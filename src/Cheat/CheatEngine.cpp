/**
 * 金手指模块实现 - 解析命令并调用 BattleEngine 接口
 */
#include "../../include/Cheat/CheatEngine.hpp"
#include "../../include/BattleCoreRefactor/BattleEngine.hpp"
#include "../../include/CardSystem/CardSystem.hpp"
#include "../../include/DataLayer/DataLayer.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace tce {

// 状态效果 ID 列表，供 player_status / monster_status 等命令补全
static const char* const STATUS_IDS[] = {
    "weak", "vulnerable", "strength", "poison", "draw_up", "cannot_draw", "cannot_block", "energy_up", "block_up",
    "metallicize", "poison_cloud", "double_damage", "double_tap", "demon_form", "heat_sink", "establishment", "equilibrium", "blasphemy", "hello", "unstoppable", "free_attack", "dexterity", "dexterity_down", "artifact", "strength_down", "frail",
    "slow", "daze", "void", "flight", "indestructible", "death_rhythm", "curiosity", "anger", "curl_up", "vanish"
};
static const size_t STATUS_IDS_COUNT = sizeof(STATUS_IDS) / sizeof(STATUS_IDS[0]);

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

std::vector<std::string> CheatEngine::get_completion_candidates(const std::string& command, int arg_index, const std::string& prefix) const {
    std::vector<std::string> out;
    if (prefix.empty()) return out;

    if (arg_index == 1) {
        if (command == "add_hand" || command == "remove_hand") {
            for (const CardId& id : get_all_card_ids()) {
                if (id.size() >= prefix.size() && id.compare(0, prefix.size(), prefix) == 0)
                    out.push_back(id);
            }
            return out;
        }
        if (command == "player_status" || command == "player_status_remove") {
            for (size_t i = 0; i < STATUS_IDS_COUNT; ++i) {
                std::string s(STATUS_IDS[i]);
                if (s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0)
                    out.push_back(s);
            }
            return out;
        }
    }
    if (arg_index == 2 && (command == "monster_status" || command == "monster_status_remove")) {
        for (size_t i = 0; i < STATUS_IDS_COUNT; ++i) {
            std::string s(STATUS_IDS[i]);
            if (s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0)
                out.push_back(s);
        }
        return out;
    }
    return out;
}

std::string CheatEngine::get_command_usage(const std::string& command) const {
    if (command == "player_hp") return "player_hp <value>";
    if (command == "player_max_hp") return "player_max_hp <value>";
    if (command == "player_block") return "player_block <value>";
    if (command == "player_energy") return "player_energy <value>";
    if (command == "player_gold") return "player_gold <value>";
    if (command == "player_status") return "player_status <status_id> <stacks> <duration>";
    if (command == "player_status_remove") return "player_status_remove <status_id>";
    if (command == "monster_hp") return "monster_hp <index> <hp>";
    if (command == "monster_max_hp") return "monster_max_hp <index> <hp>";
    if (command == "monster_status") return "monster_status <index> <status_id> <stacks> <duration>";
    if (command == "monster_status_remove") return "monster_status_remove <index> <status_id>";
    if (command == "monster_kill") return "monster_kill <index>";
    if (command == "add_relic") return "add_relic <relic_id>";
    if (command == "remove_relic") return "remove_relic <relic_id>";
    if (command == "add_potion") return "add_potion <potion_id>";
    if (command == "remove_potion") return "remove_potion <slot>";
    if (command == "add_hand") return "add_hand <card_id>";
    if (command == "remove_hand") return "remove_hand <card_id>";
    return "";
}

} // namespace tce
