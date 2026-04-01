/**
 * DataLayer 实现：加载 data/*.json，哈希表按 id 查找，排序
 *
 * 课设数据结构对应：
 * - 哈希表：unordered_map，以 id 为关键字进行插入与查找；
 * - 排序：对 vector 使用比较排序（按稀有度/分数关键字）。
 *
 * 卡牌/怪物直接填充 tce::s_cards / tce::s_monsters（唯一存储），
 * tce::get_card_by_id / get_monster_by_id 供 B/C 与 DataLayer 共用。
 */

#include "../../include/DataLayer/DataLayer.hpp"
#include "DataLayer/DataLayer.h"
#include "DataLayer/JsonParser.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

// -----------------------------------------------------------------------------
// tce 命名空间：B/C 依赖的接口，存储由 load_cards/load_monsters 填充，此处仅做哈希查找 O(1)
// 必须在 namespace DataLayer 之前定义，供 load_cards/load_monsters 写入 s_cards/s_monsters
// -----------------------------------------------------------------------------
namespace tce {

// 初始内置一组简单卡牌/怪物数据，便于战斗调试；若主流程调用 load_cards/load_monsters 则会覆盖
std::unordered_map<CardId, CardData> s_cards{
    { "strike",  CardData{ "strike",  u8"打击",  CardType::Attack, 1, CardColor::Red, Rarity::Common,   u8"造成6点伤害。", false, false, false, false, false, true, false } },
    { "strike+", CardData{ "strike+", u8"打击+", CardType::Attack, 1, CardColor::Red, Rarity::Common,   u8"造成9点伤害。", false, false, false, false, false, true, false } },
    { "defend",  CardData{ "defend",  u8"防御",  CardType::Skill,  1, CardColor::Red, Rarity::Common,   u8"获得5点格挡。", false, false, false, false, false, false, false } },
    { "defend+", CardData{ "defend+", u8"防御+", CardType::Skill,  1, CardColor::Red, Rarity::Common,   u8"获得8点格挡。", false, false, false, false, false, false, false } },
    { "bash",    CardData{ "bash",    u8"重击",  CardType::Attack, 2, CardColor::Red, Rarity::Uncommon, u8"造成8点伤害，并施加2层易伤", false, false, false, false, false, true, false } },
    { "bash+",   CardData{ "bash+",   u8"重击+", CardType::Attack, 2, CardColor::Red, Rarity::Uncommon, u8"造成10点伤害，并施加3层易伤。", false, false, false, false, false, true, false } },
};
std::unordered_map<MonsterId, MonsterData> s_monsters{
    { "cultist", MonsterData{ "cultist", u8"邪教徒", MonsterType::Normal, 100 } },
};

const CardData* get_card_by_id(CardId id) {
    auto it = s_cards.find(id);
    if (it != s_cards.end()) return &it->second;
    return nullptr;
}

const MonsterData* get_monster_by_id(MonsterId id) {
    auto it = s_monsters.find(id);
    if (it != s_monsters.end()) return &it->second;
    return nullptr;
}

std::vector<CardId> get_all_card_ids() {
    std::vector<CardId> out;
    out.reserve(s_cards.size());
    for (const auto& p : s_cards) out.push_back(p.first);
    return out;
}

} // namespace tce

namespace DataLayer {

// 简单日志工具：仅用于 DataLayer 内部调试，不影响对外接口
static void log_error(const std::string& msg) {
    std::cout << "[E][DataLayer] " << msg << "\n";
}

// 将字符串转为小写，便于大小写不敏感比较
static std::string to_lower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

// cardType 直接按 DataLayer.hpp 枚举名解析，无额外映射
static tce::CardType card_type_from_string(const std::string& s) {
    // 兼容大小写：同时接受 "Attack"/"attack" 等写法
    std::string v = s;
    if (v == "Attack" || v == "Skill" || v == "Power" || v == "Status" || v == "Curse") {
        if (v == "Attack") return tce::CardType::Attack;
        if (v == "Skill")  return tce::CardType::Skill;
        if (v == "Power")  return tce::CardType::Power;
        if (v == "Status") return tce::CardType::Status;
        if (v == "Curse")  return tce::CardType::Curse;
    }
    v = to_lower(v);
    if (v == "attack") return tce::CardType::Attack;
    if (v == "skill")  return tce::CardType::Skill;
    if (v == "power")  return tce::CardType::Power;
    if (v == "status") return tce::CardType::Status;
    if (v == "curse")  return tce::CardType::Curse;
    log_error("Unknown cardType=\"" + s + "\", fallback to Attack");
    return tce::CardType::Attack;
}
static tce::Rarity rarity_from_string(const std::string& s) {
    if (s == "common")   return tce::Rarity::Common;
    if (s == "uncommon") return tce::Rarity::Uncommon;
    if (s == "rare")     return tce::Rarity::Rare;
    if (s == "special")  return tce::Rarity::Special;
    return tce::Rarity::Common;
}

static tce::CardColor color_from_string(const std::string& s) {
    if (s == "red") return tce::CardColor::Red;
    if (s == "blue") return tce::CardColor::Blue;
    if (s == "green") return tce::CardColor::Green;
    if (s == "purple") return tce::CardColor::Purple;
    if (s == "colorless") return tce::CardColor::Colorless;
    if (s == "curse") return tce::CardColor::Curse;
    return tce::CardColor::Colorless;
}

std::string DataLayerImpl::resolve_data_path(const std::string& base, const std::string& filename) const {
    if (base.empty()) return "data/" + filename;
    if (base.size() >= 5 && (base.substr(base.size() - 5) == ".json")) return base;
    std::string b = base;
    if (!b.empty() && b.back() != '/' && b.back() != '\\') b += "/";
    return b + "data/" + filename;
}

// 尝试多个路径加载 JSON 文件（解决从 x64/Debug 或项目根目录运行时的路径差异）
static JsonValue try_parse_json_file_multi_path(const std::string& filename) {
    std::string candidates[] = {
        "data/" + filename,
        "./data/" + filename,
        "../data/" + filename,
        "../../data/" + filename,
    };
    for (const auto& path : candidates) {
        JsonValue root = parse_json_file(path);
        if (root.is_array() || root.is_object()) return root;
    }
    return JsonValue();
}

// 将 JSON 数组中的每条记录直接插入 tce::s_cards（唯一存储，供 B/C 与 DataLayer 共用）
bool DataLayerImpl::load_cards(const std::string& path_or_base_dir) {
    JsonValue root;
    if (path_or_base_dir.empty()) {
        root = try_parse_json_file_multi_path("cards.json");
    } else {
        std::string path = resolve_data_path(path_or_base_dir, "cards.json");
        root = parse_json_file(path);
    }
    if (!root.is_array()) {
        log_error("load_cards: data/cards.json not found or not a JSON array (tried data/, ./data/, ../data/, ../../data/)");
        return false;
    }
    tce::s_cards.clear();
    int loaded = 0;
    int skipped_no_id = 0;
    int duplicated = 0;
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        tce::CardData cd;
        if (const JsonValue* p = v.get_key("id")) {
            cd.id = p->as_string();
        }
        if (cd.id.empty()) {
            ++skipped_no_id;
            continue;
        }
        if (const JsonValue* p = v.get_key("name")) cd.name = p->as_string();
        if (const JsonValue* p = v.get_key("cardType")) cd.cardType = card_type_from_string(p->as_string());
        if (const JsonValue* p = v.get_key("cost")) cd.cost = p->as_int();
        if (const JsonValue* p = v.get_key("color")) cd.color = color_from_string(p->as_string());
        if (const JsonValue* p = v.get_key("rarity")) cd.rarity = rarity_from_string(p->as_string());
        if (const JsonValue* p = v.get_key("description")) cd.description = p->as_string();
        if (const JsonValue* p = v.get_key("exhaust")) cd.exhaust = p->as_bool();
        if (const JsonValue* p = v.get_key("ethereal")) cd.ethereal = p->as_bool();
        if (const JsonValue* p = v.get_key("innate")) cd.innate = p->as_bool();
        if (const JsonValue* p = v.get_key("retain")) cd.retain = p->as_bool();
        if (const JsonValue* p = v.get_key("unplayable")) cd.unplayable = p->as_bool();
        if (const JsonValue* p = v.get_key("requiresTarget")) cd.requiresTarget = p->as_bool();
        if (const JsonValue* p = v.get_key("irremovable")) cd.irremovableFromDeck = p->as_bool();
        if (!cd.id.empty()) tce::s_cards[cd.id] = std::move(cd);
    }
    return !tce::s_cards.empty();
}

// 将怪物表每条记录直接插入 tce::s_monsters（唯一存储，供 B 与 DataLayer 共用）
bool DataLayerImpl::load_monsters(const std::string& path_or_base_dir) {
    JsonValue root;
    std::string path;
    if (path_or_base_dir.empty()) {
        root = try_parse_json_file_multi_path("monsters.json");
        path = "data/monsters.json";
    } else {
        path = resolve_data_path(path_or_base_dir, "monsters.json");
        root = parse_json_file(path);
    }
    if (!root.is_array()) {
        log_error("load_monsters: data/monsters.json not found or not a JSON array");
        return false;
    }
    tce::s_monsters.clear();
    int loaded = 0;
    int skipped_no_id = 0;
    int duplicated = 0;
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        tce::MonsterData md;
        if (const JsonValue* p = v.get_key("id")) {
            md.id = p->as_string();
        }
        if (md.id.empty()) {
            ++skipped_no_id;
            continue;
        }
        if (const JsonValue* p = v.get_key("name")) md.name = p->as_string();
        if (const JsonValue* p = v.get_key("maxHp")) md.maxHp = p->as_int();
        if (md.maxHp <= 0) md.maxHp = 10;  // 缺失或无效时默认 10，避免除零
        if (const JsonValue* p = v.get_key("isBoss")) {
            md.type = p->as_bool() ? tce::MonsterType::Boss : tce::MonsterType::Normal;
        } else if (const JsonValue* p = v.get_key("type")) {
            std::string t = p->as_string();
            if (t == "elite") md.type = tce::MonsterType::Elite;
            else if (t == "boss") md.type = tce::MonsterType::Boss;
            else md.type = tce::MonsterType::Normal;
        }
        auto it = tce::s_monsters.find(md.id);
        if (it != tce::s_monsters.end()) {
            ++duplicated;
            log_error("load_monsters: duplicated monster id=\"" + md.id + "\", keeping the first one");
            continue;
        }
        tce::s_monsters[md.id] = std::move(md);
        ++loaded;
    }
    if (loaded == 0) {
        log_error("load_monsters: no valid records loaded from \"" + path + "\"");
        return false;
    }
    if (skipped_no_id > 0) {
        log_error("load_monsters: skipped " + std::to_string(skipped_no_id) + " records without id");
    }
    if (duplicated > 0) {
        log_error("load_monsters: ignored " + std::to_string(duplicated) + " duplicated id records");
    }
    return true;
}

static EventOption parse_option(const JsonValue& v) {
    EventOption opt;
    if (!v.is_object()) return opt;
    if (const JsonValue* p = v.get_key("text")) opt.text = p->as_string();
    if (const JsonValue* p = v.get_key("next")) opt.next = p->as_string();
    if (const JsonValue* p = v.get_key("result")) {
        if (p->is_object()) {
            // 新版：result.effects（可携带多个效果）
            if (const JsonValue* pe = p->get_key("effects")) {
                if (pe->is_array()) {
                    for (const JsonValue& ev : pe->arr) {
                        if (!ev.is_object()) continue;
                        EventEffect eff;
                        if (const JsonValue* et = ev.get_key("type")) eff.type = et->as_string();
                        if (const JsonValue* val = ev.get_key("value")) eff.value = val->as_int();
                        if (!eff.type.empty()) opt.result.effects.push_back(std::move(eff));
                    }
                }
            } else {
                // 旧版：result.type + result.value
                if (const JsonValue* pt = p->get_key("type")) opt.result.type = pt->as_string();
                if (const JsonValue* pv = p->get_key("value")) opt.result.value = pv->as_int();
            }
        }
    }
    return opt;
}

// 同上：将事件表每条记录以 id 为 key 插入哈希表；options 为顺序表，逐项 push_back
bool DataLayerImpl::load_events(const std::string& path_or_base_dir) {
    std::string path = resolve_data_path(path_or_base_dir, "events.json");
    JsonValue root = parse_json_file(path);
    if (!root.is_array()) {
        log_error("load_events: file \"" + path + "\" is not a JSON array or failed to parse");
        return false;
    }
    events_.clear();
    int loaded = 0;
    int skipped_no_id = 0;
    int duplicated = 0;
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        Event e;
        if (const JsonValue* p = v.get_key("id")) {
            e.id = p->as_string();
        }
        if (e.id.empty()) {
            ++skipped_no_id;
            continue;
        }
        if (const JsonValue* p = v.get_key("title")) e.title = p->as_string();
        if (const JsonValue* p = v.get_key("description")) e.description = p->as_string();
        if (const JsonValue* p = v.get_key("image")) e.image = p->as_string();
        if (const JsonValue* p = v.get_key("layer_min")) e.layer_min = p->as_int();
        if (const JsonValue* p = v.get_key("layer_max")) e.layer_max = p->as_int();
        if (const JsonValue* p = v.get_key("weight")) e.weight = p->as_int();
        if (const JsonValue* p = v.get_key("options")) {
            if (p->is_array())
                for (const JsonValue& o : p->arr) e.options.push_back(parse_option(o));  // 顺序表尾插
        }
        auto it = events_.find(e.id);
        if (it != events_.end()) {
            ++duplicated;
            log_error("load_events: duplicated event id=\"" + e.id + "\", keeping the first one");
            continue;
        }
        events_[e.id] = std::move(e);  // 哈希表插入
        ++loaded;
    }
    if (loaded == 0) {
        log_error("load_events: no valid records loaded from \"" + path + "\"");
        return false;
    }
    if (skipped_no_id > 0) {
        log_error("load_events: skipped " + std::to_string(skipped_no_id) + " records without id");
    }
    if (duplicated > 0) {
        log_error("load_events: ignored " + std::to_string(duplicated) + " duplicated id records");
    }
    return true;
}

const tce::CardData* DataLayerImpl::get_card_by_id(const CardId& id) const {
    return tce::get_card_by_id(id);
}

const tce::MonsterData* DataLayerImpl::get_monster_by_id(const MonsterId& id) const {
    return tce::get_monster_by_id(id);
}

const Event* DataLayerImpl::get_event_by_id(const EventId& id) const {
    auto it = events_.find(id);
    if (it == events_.end()) return nullptr;
    return &it->second;
}

std::vector<EventId> DataLayerImpl::get_root_event_ids() const {
    // 根事件：id 形如 "event_001"（只包含三位数字，不含后缀如 event_001a / event_001b）
    static constexpr const char* kPrefix = "event_";
    constexpr size_t kPrefixLen = 6;

    std::vector<EventId> out;
    out.reserve(events_.size());
    for (const auto& kv : events_) {
        const EventId& id = kv.first;
        if (id.size() != kPrefixLen + 3) continue;
        if (id.rfind(kPrefix, 0) != 0) continue;

        bool ok = true;
        for (size_t i = kPrefixLen; i < id.size(); ++i) {
            const char ch = id[i];
            if (ch < '0' || ch > '9') { ok = false; break; }
        }
        if (ok) out.push_back(id);
    }

    // 为了可复现与 UI 好看：按 id 排序
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<RootEventCandidate> DataLayerImpl::get_root_event_candidates_for_layer(int layer) const {
    // 根事件：id 形如 "event_001"（只包含三位数字，不含后缀如 event_001a / event_001b）
    static constexpr const char* kPrefix = "event_";
    constexpr size_t kPrefixLen = 6;

    std::vector<RootEventCandidate> out;
    out.reserve(events_.size());

    for (const auto& kv : events_) {
        const EventId& id = kv.first;
        if (id.size() != kPrefixLen + 3) continue;
        if (id.rfind(kPrefix, 0) != 0) continue;

        bool ok = true;
        for (size_t i = kPrefixLen; i < id.size(); ++i) {
            const char ch = id[i];
            if (ch < '0' || ch > '9') { ok = false; break; }
        }
        if (!ok) continue;

        const Event& e = kv.second;
        if (layer < e.layer_min || layer > e.layer_max) continue;
        const int w = e.weight <= 0 ? 1 : e.weight;
        out.push_back(RootEventCandidate{ e.id, w });
    }

    // 保证可复现：按 id 排序
    std::sort(out.begin(), out.end(), [](const RootEventCandidate& a, const RootEventCandidate& b) {
        return a.id < b.id;
    });
    return out;
}

int DataLayerImpl::rarity_order(tce::Rarity r) {
    switch (r) {
        case tce::Rarity::Common:   return 0;
        case tce::Rarity::Uncommon: return 1;
        case tce::Rarity::Rare:     return 2;
        case tce::Rarity::Special:  return 3;
        default: return 0;
    }
}

// 排序：对线性表（vector）按关键字 rarity 升序排序；比较排序 O(n log n)
std::vector<CardId> DataLayerImpl::sort_cards_by_rarity(const std::vector<CardId>& card_ids) const {
    std::vector<CardId> out = card_ids;
    std::sort(out.begin(), out.end(), [](const CardId& a, const CardId& b) {
        const tce::CardData* ca = tce::get_card_by_id(a);
        const tce::CardData* cb = tce::get_card_by_id(b);
        int ra = ca ? DataLayerImpl::rarity_order(ca->rarity) : 0;
        int rb = cb ? DataLayerImpl::rarity_order(cb->rarity) : 0;
        if (ra != rb) return ra < rb;
        return a < b;
    });
    return out;
}

// 排序：对排行榜按关键字 score 降序排序；比较排序 O(n log n)
std::vector<DataLayerImpl::LeaderboardEntry> DataLayerImpl::sort_leaderboard(
    std::vector<LeaderboardEntry> entries) const {
    std::sort(entries.begin(), entries.end(),
        [](const LeaderboardEntry& a, const LeaderboardEntry& b) { return a.score > b.score; });
    return entries;
}

} // namespace DataLayer
