#include "GameFlow/GameFlowController.hpp"

#include <filesystem>
#include <fstream>

#include "DataLayer/JsonParser.h"

namespace tce {

bool GameFlowController::saveRun(const std::string& path) const {
    namespace fs = std::filesystem;
    try {
        fs::path savePath(path);
        fs::path dir = savePath.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            fs::create_directories(dir);
        }

        std::ofstream out(savePath, std::ios::binary);
        if (!out) {
            return false;
        }

        // --- 固定检查点（进入节点瞬间） ---
        // 规则：无论在宝箱/事件/战斗等界面何时触发保存，都只写进入该节点瞬间的状态。
        // 这样房间内的 RNG 消耗不会写入存档，从而避免通过 SL 刷宝箱内容/事件结果。
        const uint64_t rng_state = checkpointValid_ ? checkpointRunRngState_ : runRng_.get_state();
        const int current_layer = checkpointValid_ ? checkpointCurrentLayer_ : mapEngine_.get_current_layer();
        const std::string current_node_id = checkpointValid_ ? checkpointCurrentNodeId_ : "";
        const PlayerBattleState& p = checkpointValid_ ? checkpointPlayerState_ : playerState_;

        out << "{\n";
        out << "  \"schema_version\": 1,\n";
        out << "  \"run_rng_state\": \"" << rng_state << "\",\n";

        // map
        out << "  \"map\": {\n";
        out << "    \"current_layer\": " << current_layer << ",\n";
        out << "    \"current_node_id\": \"" << current_node_id << "\"\n";
        out << "  },\n";

        // player
        out << "  \"player\": {\n";
        out << "    \"name\": \"" << p.playerName << "\",\n";
        out << "    \"character_id\": \"" << p.character << "\",\n";
        out << "    \"current_hp\": " << p.currentHp << ",\n";
        out << "    \"max_hp\": " << p.maxHp << ",\n";
        out << "    \"block\": " << p.block << ",\n";
        out << "    \"gold\": " << p.gold << ",\n";
        out << "    \"max_energy\": " << p.maxEnergy << ",\n";
        out << "    \"potion_slots\": " << p.potionSlotCount << ",\n";

        // potions array
        out << "    \"potions\": [";
        for (size_t i = 0; i < p.potions.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << p.potions[i] << "\"";
        }
        out << "],\n";

        // relics array
        out << "    \"relics\": [";
        for (size_t i = 0; i < p.relics.size(); ++i) {
            if (i > 0) out << ", ";
            out << "\"" << p.relics[i] << "\"";
        }
        out << "],\n";

        // master deck（永久牌组）：同样使用检查点，避免房间内修改导致存档漂移
        out << "    \"master_deck\": [";
        if (checkpointValid_) {
            for (size_t i = 0; i < checkpointMasterDeck_.size(); ++i) {
                if (i > 0) out << ", ";
                out << "\"" << checkpointMasterDeck_[i] << "\"";
            }
        } else {
            const auto& deck = cardSystem_.get_master_deck();
            for (size_t i = 0; i < deck.size(); ++i) {
                if (i > 0) out << ", ";
                out << "\"" << deck[i].id << "\"";
            }
        }
        out << "]\n";
        out << "  },\n";

        // --- 最后所在界面（用于读档后自动跳转）：map / battle / event / shop / rest / treasure ---
        const char* sceneStr = "map";
        switch (lastSceneForSave_) {
        case LastSceneKind::Battle:   sceneStr = "battle";   break;
        case LastSceneKind::Event:    sceneStr = "event";    break;
        case LastSceneKind::Shop:     sceneStr = "shop";     break;
        case LastSceneKind::Rest:     sceneStr = "rest";     break;
        case LastSceneKind::Treasure: sceneStr = "treasure"; break;
        case LastSceneKind::Map:
        default:                      sceneStr = "map";      break;
        }
        out << "  \"last_scene\": \"" << sceneStr << "\"\n";

        out << "}\n";

        return true;
    } catch (...) {
        return false;
    }
}

bool GameFlowController::loadRun(const std::string& path) {
    namespace fs = std::filesystem;
    try {
        if (!fs::exists(path)) {
            return false;
        }

        DataLayer::JsonValue root = DataLayer::parse_json_file(path);
        if (!root.is_object()) return false;

        // --- 运行级随机数状态 ---
        if (const auto* rngVal = root.get_key("run_rng_state")) {
            if (rngVal->is_string()) {
                uint64_t s = 0;
                try {
                    s = static_cast<uint64_t>(std::stoull(rngVal->s));
                } catch (...) {
                    s = 0;
                }
                if (s != 0) {
                    runRng_.set_state(s);
                }
            }
        }

        // --- 地图当前层与当前节点 ---
        std::string current_node_id;
        int         current_layer = 0;
        if (const auto* mapVal = root.get_key("map")) {
            if (mapVal->is_object()) {
                if (const auto* layerVal = mapVal->get_key("current_layer")) {
                    if (layerVal->is_int()) current_layer = layerVal->i;
                }
                if (const auto* nodeVal = mapVal->get_key("current_node_id")) {
                    if (nodeVal->is_string()) current_node_id = nodeVal->s;
                }
            }
        }

        // --- 玩家基础状态 ---
        if (const auto* pVal = root.get_key("player")) {
            if (pVal->is_object()) {
                PlayerBattleState p = playerState_;
                if (const auto* v = pVal->get_key("name"); v && v->is_string()) p.playerName = v->s;
                if (const auto* v = pVal->get_key("character_id"); v && v->is_string()) p.character = v->s;
                if (const auto* v = pVal->get_key("current_hp"); v && v->is_int()) p.currentHp = v->i;
                if (const auto* v = pVal->get_key("max_hp"); v && v->is_int()) p.maxHp = v->i;
                if (const auto* v = pVal->get_key("block"); v && v->is_int()) p.block = v->i;
                if (const auto* v = pVal->get_key("gold"); v && v->is_int()) p.gold = v->i;
                if (const auto* v = pVal->get_key("max_energy"); v && v->is_int()) p.maxEnergy = v->i;
                if (const auto* v = pVal->get_key("potion_slots"); v && v->is_int()) p.potionSlotCount = v->i;

                p.potions.clear();
                if (const auto* arr = pVal->get_key("potions"); arr && arr->is_array()) {
                    for (const auto& e : arr->arr) {
                        if (e.is_string()) p.potions.push_back(e.s);
                    }
                }

                p.relics.clear();
                if (const auto* arr = pVal->get_key("relics"); arr && arr->is_array()) {
                    for (const auto& e : arr->arr) {
                        if (e.is_string()) p.relics.push_back(e.s);
                    }
                }

                playerState_ = p;

                // master deck（永久牌组）
                if (const auto* arr = pVal->get_key("master_deck"); arr && arr->is_array()) {
                    std::vector<CardId> ids;
                    ids.reserve(arr->arr.size());
                    for (const auto& e : arr->arr) {
                        if (e.is_string()) ids.push_back(e.s);
                    }
                    if (!ids.empty()) {
                        cardSystem_.init_master_deck(ids);
                    }
                }
            }
        }

        // --- 恢复地图当前位置 ---
        if (!current_node_id.empty()) {
            mapEngine_.set_current_node(current_node_id);
            mapEngine_.set_node_visited(current_node_id);
            mapEngine_.update_reachable_nodes();
            if (current_layer >= 0) {
                mapUI_.setCurrentLayer(current_layer);
            }
        }

        // --- 读出最后所在界面，决定 run() 启动时是否需要先跳转到具体界面 ---
        std::string lastSceneStr = "map";
        if (const auto* v = root.get_key("last_scene")) {
            if (v->is_string()) lastSceneStr = v->s;
        }
        sceneAfterLoad_ = LastSceneKind::Map;
        if (lastSceneStr == "battle")      sceneAfterLoad_ = LastSceneKind::Battle;
        else if (lastSceneStr == "event")  sceneAfterLoad_ = LastSceneKind::Event;
        else if (lastSceneStr == "shop")   sceneAfterLoad_ = LastSceneKind::Shop;
        else if (lastSceneStr == "rest")   sceneAfterLoad_ = LastSceneKind::Rest;
        else if (lastSceneStr == "treasure") sceneAfterLoad_ = LastSceneKind::Treasure;
        hasPendingSceneAfterLoad_ = (sceneAfterLoad_ != LastSceneKind::Map);

        // --- 重置全局 HUD 的临时 UI 状态，避免一进界面就处于“暂停/牌组已打开”的状态 ---
        hudBattleUi_.set_deck_view_active(false);
        hudBattleUi_.set_pause_menu_active(false);

        // --- 读档恢复后，检查点应与存档一致（存档本身即检查点） ---
        captureCheckpointForCurrentNode();

        gameOver_ = false;
        gameCleared_ = false;
        statusText_ = "继续旅程。";
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace tce

