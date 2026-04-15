#include "GameFlow/GameFlowController.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

#include "DataLayer/JsonParser.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace tce {

namespace {

// 将相对路径的存档统一落到“项目根/saves/”：
// 从可执行文件所在目录向上查找包含 src/ 与 include/ 的目录，作为项目根。
static std::filesystem::path get_executable_directory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::filesystem::path p(buf);
    return p.parent_path();
#else
    return std::filesystem::current_path();
#endif
}

static std::filesystem::path guess_project_root_from_exe() {
    namespace fs = std::filesystem;
    fs::path d = get_executable_directory();
    for (int i = 0; i < 6 && !d.empty(); ++i) {
        if (fs::exists(d / "src") && fs::exists(d / "include")) return d;
        d = d.parent_path();
    }
    return {};
}

static std::filesystem::path resolve_save_path(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path p = fs::u8path(path);
    if (p.is_absolute()) return p;
    if (fs::path root = guess_project_root_from_exe(); !root.empty()) {
        return root / p;
    }
    return p; // fallback: 当前工作目录
}

std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '\\' || c == '"')
            o += '\\';
        o += c;
    }
    return o;
}

const char* node_type_to_json(NodeType t) {
    switch (t) {
    case NodeType::Elite: return "Elite";
    case NodeType::Event: return "Event";
    case NodeType::Rest: return "Rest";
    case NodeType::Merchant: return "Merchant";
    case NodeType::Treasure: return "Treasure";
    case NodeType::Boss: return "Boss";
    case NodeType::Enemy:
    default: return "Enemy";
    }
}

NodeType node_type_from_json(const std::string& s) {
    if (s == "Elite") return NodeType::Elite;
    if (s == "Event") return NodeType::Event;
    if (s == "Rest") return NodeType::Rest;
    if (s == "Merchant") return NodeType::Merchant;
    if (s == "Treasure") return NodeType::Treasure;
    if (s == "Boss") return NodeType::Boss;
    return NodeType::Enemy;
}

} // namespace

bool GameFlowController::saveRun(const std::string& path) const {
    namespace fs = std::filesystem;
    try {
        fs::path savePath = resolve_save_path(path);
        fs::path dir = savePath.parent_path();
        if (!dir.empty() && !fs::exists(dir)) {
            fs::create_directories(dir);
        }

        std::ofstream out(savePath, std::ios::binary);
        if (!out) {
            return false;
        }

        // --- 固定检查点（进入节点瞬间） ---
        // 默认规则：无论在宝箱/事件/战斗等界面何时触发保存，都只写进入该节点瞬间的状态，
        // 避免通过 SL 刷宝箱内容/事件结果（房间内 RNG 消耗不写入存档）。
        //
        // 例外：若在“战斗胜利奖励界面”存档，则需要能读档回到奖励界面而不重新战斗，
        // 因此此时必须写入当前实时状态（含牌组/灵液等变更与奖励候选）。
        const bool useCheckpoint = checkpointValid_ && lastSceneForSave_ != LastSceneKind::BattleReward;
        const uint64_t rng_state = useCheckpoint ? checkpointRunRngState_ : runRng_.get_state();
        const int current_layer = useCheckpoint ? checkpointCurrentLayer_ : mapEngine_.get_current_layer();
        const std::string current_node_id = useCheckpoint ? checkpointCurrentNodeId_ : "";
        const PlayerBattleState& p = useCheckpoint ? checkpointPlayerState_ : playerState_;

        const MapEngine::MapSnapshot snap = mapEngine_.get_map_snapshot();

        out << "{\n";
        out << "  \"schema_version\": 2,\n";
        out << "  \"run_rng_state\": \"" << rng_state << "\",\n";

        // map：完整节点快照 + 地图配置页索引，读档后可 1:1 复现整张随机地图
        out << "  \"map\": {\n";
        out << "    \"page_index\": " << mapConfigManager_.getCurrentIndex() << ",\n";
        out << "    \"total_layers\": " << snap.total_layers << ",\n";
        out << "    \"current_layer\": " << current_layer << ",\n";
        out << "    \"current_node_id\": \"" << json_escape(current_node_id) << "\",\n";
        out << "    \"nodes\": [\n";
        for (size_t i = 0; i < snap.all_nodes.size(); ++i) {
            const MapEngine::MapNode& n = snap.all_nodes[i];
            if (i > 0) out << ",\n";
            out << "      {";
            out << "\"id\":\"" << json_escape(n.id) << "\",";
            out << "\"type\":\"" << node_type_to_json(n.type) << "\",";
            out << "\"content_id\":\"" << json_escape(n.content_id) << "\",";
            out << "\"layer\":" << n.layer << ",";
            const int px = static_cast<int>(std::lround(n.position.x * 100.0f));
            const int py = static_cast<int>(std::lround(n.position.y * 100.0f));
            out << "\"px\":" << px << ",\"py\":" << py << ",";
            out << "\"prev\":[";
            for (size_t j = 0; j < n.prev_nodes.size(); ++j) {
                if (j > 0) out << ",";
                out << "\"" << json_escape(n.prev_nodes[j]) << "\"";
            }
            out << "],\"next\":[";
            for (size_t j = 0; j < n.next_nodes.size(); ++j) {
                if (j > 0) out << ",";
                out << "\"" << json_escape(n.next_nodes[j]) << "\"";
            }
            out << "],\"is_visited\":" << (n.is_visited ? "true" : "false") << ",";
            out << "\"is_current\":" << (n.is_current ? "true" : "false") << ",";
            out << "\"is_reachable\":" << (n.is_reachable ? "true" : "false") << ",";
            out << "\"is_completed\":" << (n.is_completed ? "true" : "false");
            out << "}";
        }
        out << "\n    ]\n";
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

        // master deck（永久牌组）
        out << "    \"master_deck\": [";
        if (useCheckpoint) {
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
        case LastSceneKind::BattleReward: sceneStr = "battle_reward"; break;
        case LastSceneKind::Event:    sceneStr = "event";    break;
        case LastSceneKind::Shop:     sceneStr = "shop";     break;
        case LastSceneKind::Rest:     sceneStr = "rest";     break;
        case LastSceneKind::Treasure: sceneStr = "treasure"; break;
        case LastSceneKind::Map:
        default:                      sceneStr = "map";      break;
        }
        // 战斗胜利奖励界面：保存奖励候选与进度（点击才获得）
        if (lastSceneForSave_ == LastSceneKind::BattleReward) {
            out << "  \"battle_reward\": {\n";
            out << "    \"gold\": " << savedBattleRewardGold_ << ",\n";
            out << "    \"card_picked\": " << (savedBattleRewardCardPicked_ ? "true" : "false") << ",\n";
            out << "    \"cards\": [";
            for (size_t i = 0; i < savedBattleRewardCards_.size(); ++i) {
                if (i > 0) out << ", ";
                out << "\"" << json_escape(savedBattleRewardCards_[i]) << "\"";
            }
            out << "],\n";
            out << "    \"relic_offers\": [";
            for (size_t i = 0; i < savedBattleRewardRelicOffers_.size(); ++i) {
                if (i > 0) out << ", ";
                out << "\"" << json_escape(savedBattleRewardRelicOffers_[i]) << "\"";
            }
            out << "],\n";
            out << "    \"potion_offers\": [";
            for (size_t i = 0; i < savedBattleRewardPotionOffers_.size(); ++i) {
                if (i > 0) out << ", ";
                out << "\"" << json_escape(savedBattleRewardPotionOffers_[i]) << "\"";
            }
            out << "]\n";
            out << "  },\n";
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
        seenEventRootsByLayer_.clear();
        const fs::path loadPath = resolve_save_path(path);
        if (!fs::exists(loadPath)) {
            return false;
        }

        DataLayer::JsonValue root = DataLayer::parse_json_file(loadPath.u8string());
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

        // --- 地图：完整快照（schema>=2）或仅当前节点（旧档）---
        std::string current_node_id;
        int         current_layer = 0;
        bool        restored_full_map = false;

        if (const auto* mapVal = root.get_key("map")) {
            if (mapVal->is_object()) {
                if (const auto* pi = mapVal->get_key("page_index")) {
                    if (pi->is_int())
                        mapConfigManager_.setCurrentIndex(pi->i);
                }

                if (const auto* layerVal = mapVal->get_key("current_layer")) {
                    if (layerVal->is_int()) current_layer = layerVal->i;
                }
                if (const auto* nodeVal = mapVal->get_key("current_node_id")) {
                    if (nodeVal->is_string()) current_node_id = nodeVal->s;
                }

                MapEngine::MapSnapshot snap;
                if (const auto* tl = mapVal->get_key("total_layers")) {
                    if (tl->is_int()) snap.total_layers = tl->i;
                }

                if (const auto* nodesArr = mapVal->get_key("nodes")) {
                    if (nodesArr->is_array() && !nodesArr->arr.empty()) {
                        snap.all_nodes.clear();
                        snap.all_nodes.reserve(nodesArr->arr.size());
                        for (const auto& nv : nodesArr->arr) {
                            if (!nv.is_object()) continue;
                            MapEngine::MapNode mn;
                            if (const auto* v = nv.get_key("id")) mn.id = v->as_string();
                            if (const auto* v = nv.get_key("type")) mn.type = node_type_from_json(v->as_string());
                            if (const auto* v = nv.get_key("content_id")) mn.content_id = v->as_string();
                            if (const auto* v = nv.get_key("layer")) mn.layer = v->as_int();
                            if (const auto* v = nv.get_key("px")) mn.position.x = static_cast<float>(v->as_int()) * 0.01f;
                            if (const auto* v = nv.get_key("py")) mn.position.y = static_cast<float>(v->as_int()) * 0.01f;
                            if (const auto* parr = nv.get_key("prev"); parr && parr->is_array()) {
                                for (const auto& pe : parr->arr) {
                                    if (pe.is_string()) mn.prev_nodes.push_back(pe.s);
                                }
                            }
                            if (const auto* narr = nv.get_key("next"); narr && narr->is_array()) {
                                for (const auto& ne : narr->arr) {
                                    if (ne.is_string()) mn.next_nodes.push_back(ne.s);
                                }
                            }
                            if (const auto* v = nv.get_key("is_visited")) mn.is_visited = v->as_bool();
                            if (const auto* v = nv.get_key("is_current")) mn.is_current = v->as_bool();
                            if (const auto* v = nv.get_key("is_reachable")) mn.is_reachable = v->as_bool();
                            if (const auto* v = nv.get_key("is_completed")) mn.is_completed = v->as_bool();
                            if (!mn.id.empty()) snap.all_nodes.push_back(std::move(mn));
                        }
                        if (!snap.all_nodes.empty()) {
                            if (snap.total_layers <= 0) {
                                int maxL = 0;
                                for (const auto& n : snap.all_nodes)
                                    maxL = std::max(maxL, n.layer);
                                snap.total_layers = maxL + 1;
                            }
                            mapEngine_.restore_from_snapshot(snap);
                            mapUI_.setMap(&mapEngine_);
                            restored_full_map = true;
                        }
                    }
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

        // --- 旧档或无快照：仅用节点 id 套在当前地图上 ---
        if (!restored_full_map && !current_node_id.empty()) {
            mapEngine_.set_current_node(current_node_id);
            mapEngine_.set_node_visited(current_node_id);
            mapEngine_.update_reachable_nodes();
        }
        if (current_layer >= 0) {
            mapUI_.setCurrentLayer(current_layer);
        }

        // --- 读出最后所在界面，决定 run() 启动时是否需要先跳转到具体界面 ---
        std::string lastSceneStr = "map";
        if (const auto* v = root.get_key("last_scene")) {
            if (v->is_string()) lastSceneStr = v->s;
        }
        sceneAfterLoad_ = LastSceneKind::Map;
        if (lastSceneStr == "battle")      sceneAfterLoad_ = LastSceneKind::Battle;
        else if (lastSceneStr == "battle_reward") sceneAfterLoad_ = LastSceneKind::BattleReward;
        else if (lastSceneStr == "event")  sceneAfterLoad_ = LastSceneKind::Event;
        else if (lastSceneStr == "shop")   sceneAfterLoad_ = LastSceneKind::Shop;
        else if (lastSceneStr == "rest")   sceneAfterLoad_ = LastSceneKind::Rest;
        else if (lastSceneStr == "treasure") sceneAfterLoad_ = LastSceneKind::Treasure;
        hasPendingSceneAfterLoad_ = (sceneAfterLoad_ != LastSceneKind::Map);

        // 战斗胜利奖励界面：读取奖励候选与进度
        savedBattleRewardGold_ = 0;
        savedBattleRewardCardPicked_ = false;
        savedBattleRewardCards_.clear();
        savedBattleRewardRelicOffers_.clear();
        savedBattleRewardPotionOffers_.clear();
        if (sceneAfterLoad_ == LastSceneKind::BattleReward) {
            if (const auto* br = root.get_key("battle_reward"); br && br->is_object()) {
                if (const auto* g = br->get_key("gold"); g && g->is_int()) savedBattleRewardGold_ = g->i;
                if (const auto* cp = br->get_key("card_picked"); cp && cp->is_bool()) savedBattleRewardCardPicked_ = cp->b;
                if (const auto* arr = br->get_key("cards"); arr && arr->is_array()) {
                    for (const auto& e : arr->arr) if (e.is_string()) savedBattleRewardCards_.push_back(e.s);
                }
                if (const auto* arr = br->get_key("relic_offers"); arr && arr->is_array()) {
                    for (const auto& e : arr->arr) if (e.is_string()) savedBattleRewardRelicOffers_.push_back(e.s);
                }
                if (const auto* arr = br->get_key("potion_offers"); arr && arr->is_array()) {
                    for (const auto& e : arr->arr) if (e.is_string()) savedBattleRewardPotionOffers_.push_back(e.s);
                }
            }
        }

        // --- 重置全局 HUD 的临时 UI 状态，避免一进界面就处于“暂停/牌组已打开”的状态 ---
        hudBattleUi_.set_deck_view_active(false);
        hudBattleUi_.set_pause_menu_active(false);

        // --- 读档恢复后，检查点应与存档一致（存档本身即检查点） ---
        captureCheckpointForCurrentNode();

        gameOver_ = false;
        statusText_ = "继续旅程。";

        musicManager_.scanAssets();
        if (sceneAfterLoad_ != LastSceneKind::Battle) musicManager_.playMapMusic();

        return true;
    } catch (...) {
        return false;
    }
}

} // namespace tce
