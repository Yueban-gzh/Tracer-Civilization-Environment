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

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// 可执行文件所在目录（用于从 build/ 双击运行时仍能定位项目根下的 data/）
static std::string get_executable_directory() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return {};
    std::string full(buf, buf + n);
    const size_t pos = full.find_last_of("\\/");
    if (pos == std::string::npos) return {};
    return full.substr(0, pos);
#else
    (void)0;
    return {};
#endif
}

static std::string join_path(const std::string& dir, const std::string& rel) {
    if (dir.empty()) return rel;
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) return rel;
    const char last = dir.back();
    if (last == '/' || last == '\\') return dir + rel;
    return dir + "/" + rel;
}

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
std::unordered_map<MonsterId, MonsterData> s_monsters;

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

namespace {
void apply_builtin_monsters() {
    tce::s_monsters.clear();
    static const tce::MonsterData k_builtin[] = {
        { "1.1_xiuqicanzun",      u8"锈蚀残尊",     tce::MonsterType::Normal, 44 },
        { "1.2_liejianshusheng",  u8"裂简书吏",     tce::MonsterType::Normal, 42 },
        { "1.3_yazhongzhihun",    u8"压冢之魂",     tce::MonsterType::Boss,   200 },
        { "1.4_buguguiqi",        u8"卜骨龟蜮",     tce::MonsterType::Normal, 46 },
        { "1.5_fashengjiangli",   u8"法绳降吏",     tce::MonsterType::Normal, 40 },
        { "1.6_jianaidulian",     u8"简哀毒怜",     tce::MonsterType::Normal, 40 },
        { "1.7_wentiantuhai",     u8"问天涂骸",     tce::MonsterType::Normal, 42 },
        { "1.8_bingyongcanju",    u8"兵俑残驹",     tce::MonsterType::Normal, 45 },
        { "1.9_nongjihuijin",     u8"农籍汇烬",     tce::MonsterType::Normal, 43 },
        { "2.1_huxuankongke",     u8"弧悬空壳",     tce::MonsterType::Normal, 46 },
        { "2.2_canbiyansheng",    u8"残壁衍生",     tce::MonsterType::Normal, 44 },
        { "2.3_gongdiduanchang",  u8"宫地断场",     tce::MonsterType::Normal, 45 },
        { "2.4_fengsuiyapao",     u8"砜碎崖炮",     tce::MonsterType::Normal, 43 },
        { "2.5_zuimocanbi",       u8"罪魔残壁",     tce::MonsterType::Boss,   210 },
        { "2.6_nichangyuyi",      u8"霓裳羽衣",     tce::MonsterType::Normal, 41 },
        { "2.7_tongjingsuizhaung",u8"铜镜碎桩",     tce::MonsterType::Normal, 44 },
        { "2.8_fujielu",          u8"符戒戮",       tce::MonsterType::Normal, 42 },
        { "3.1_youshimohun",     u8"游石魔魂",     tce::MonsterType::Normal, 45 },
        { "3.2_bizhongguyin",     u8"笔冢古音",     tce::MonsterType::Normal, 43 },
        { "3.3_xiusitiou",        u8"修思提欧",     tce::MonsterType::Normal, 40 },
        { "3.4_sancerusheng",     u8"三策入生",     tce::MonsterType::Normal, 44 },
        { "3.5_chenzhanhugong",   u8"沉栈湖宫",     tce::MonsterType::Normal, 46 },
        { "3.6_jiaotongxianmo",   u8"交童衔魔",     tce::MonsterType::Normal, 42 },
        { "3.7_suizhicanmu",      u8"碎枝残木",     tce::MonsterType::Boss,   230 },
    };
    for (const auto& m : k_builtin) tce::s_monsters[m.id] = m;
}
struct BuiltinMonstersInit {
    BuiltinMonstersInit() { apply_builtin_monsters(); }
};
static BuiltinMonstersInit g_builtin_monsters_init;
} // namespace

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

// 尝试多个路径加载 JSON 文件（解决从 x64/Debug、build/ 或项目根目录运行时的路径差异）
static JsonValue try_parse_json_file_multi_path(const std::string& filename) {
    std::vector<std::string> candidates = {
        "data/" + filename,
        "./data/" + filename,
        "../data/" + filename,
        "../../data/" + filename,
        "../../../data/" + filename,
    };
    const std::string exeDir = get_executable_directory();
    if (!exeDir.empty()) {
        candidates.push_back(join_path(exeDir, "data/" + filename));
        candidates.push_back(join_path(exeDir, "../data/" + filename));
        candidates.push_back(join_path(exeDir, "../../data/" + filename));
    }
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
        log_error("load_cards: data/cards.json not found or not a JSON array (tried cwd-relative and exe-relative ../data/)");
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
        log_error("load_monsters: data/monsters.json not found or not a JSON array — using built-in monsters");
        apply_builtin_monsters();
        return true;
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
        log_error("load_monsters: no valid records in \"" + path + "\" — using built-in monsters");
        apply_builtin_monsters();
        return true;
    }
    if (skipped_no_id > 0) {
        log_error("load_monsters: skipped " + std::to_string(skipped_no_id) + " records without id");
    }
    if (duplicated > 0) {
        log_error("load_monsters: ignored " + std::to_string(duplicated) + " duplicated id records");
    }
    return true;
}

namespace {

void pad_groups_nine(std::vector<std::vector<tce::MonsterId>>& g, const tce::MonsterId& fallback) {
    if (g.empty()) g.push_back({fallback});
    while (g.size() < 9) g.push_back(g.back());
    if (g.size() > 9) g.resize(9);
}

std::vector<std::vector<tce::MonsterId>> parse_groups_array(const JsonValue* p) {
    std::vector<std::vector<tce::MonsterId>> out;
    if (!p || !p->is_array()) return out;
    for (const JsonValue& g : p->arr) {
        if (!g.is_array()) continue;
        std::vector<tce::MonsterId> row;
        for (const JsonValue& id : g.arr) {
            if (id.is_string()) row.push_back(id.as_string());
        }
        if (!row.empty()) out.push_back(std::move(row));
    }
    return out;
}

std::vector<tce::MonsterId> parse_boss_array(const JsonValue* p) {
    std::vector<tce::MonsterId> out;
    if (!p || !p->is_array()) return out;
    for (const JsonValue& id : p->arr) {
        if (id.is_string()) out.push_back(id.as_string());
    }
    return out;
}

void apply_default_encounter_pages(std::vector<DataLayer::EncounterPage>& pages) {
    pages.clear();
    pages.resize(3);
    const tce::MonsterId fb = "1.1_xiuqicanzun";
    for (int m = 0; m < 3; ++m) {
        DataLayer::EncounterPage& pg = pages[static_cast<size_t>(m)];
        pg.enemy_groups.clear();
        pg.elite_groups.clear();
        for (int i = 0; i < 9; ++i) {
            pg.enemy_groups.push_back({ fb });
            pg.elite_groups.push_back({ fb });
        }
        pad_groups_nine(pg.enemy_groups, fb);
        pad_groups_nine(pg.elite_groups, fb);
        pg.boss = { "1.3_yazhongzhihun" };
    }
}

} // namespace

bool DataLayerImpl::load_encounters(const std::string& path_or_base_dir) {
    JsonValue root;
    if (path_or_base_dir.empty()) {
        root = try_parse_json_file_multi_path("encounters.json");
    } else {
        const std::string path = resolve_data_path(path_or_base_dir, "encounters.json");
        root = parse_json_file(path);
    }
    encounter_pages_.clear();
    if (!root.is_object()) {
        log_error("load_encounters: data/encounters.json missing or not an object — using built-in 3×9 encounter tables");
        apply_default_encounter_pages(encounter_pages_);
        return true;
    }
    const JsonValue* maps = root.get_key("maps");
    if (!maps || !maps->is_array() || maps->arr.empty()) {
        log_error("load_encounters: \"maps\" missing or empty — using built-in encounter tables");
        apply_default_encounter_pages(encounter_pages_);
        return true;
    }
    for (const JsonValue& pageVal : maps->arr) {
        if (!pageVal.is_object()) continue;
        EncounterPage pg;
        if (const JsonValue* p = pageVal.get_key("enemy_groups")) {
            pg.enemy_groups = parse_groups_array(p);
            pad_groups_nine(pg.enemy_groups, "1.1_xiuqicanzun");
        }
        if (const JsonValue* p = pageVal.get_key("elite_groups")) {
            pg.elite_groups = parse_groups_array(p);
            pad_groups_nine(pg.elite_groups, "1.1_xiuqicanzun");
        }
        if (const JsonValue* p = pageVal.get_key("boss")) {
            pg.boss = parse_boss_array(p);
        }
        if (pg.boss.empty()) pg.boss = { "1.3_yazhongzhihun" };
        encounter_pages_.push_back(std::move(pg));
    }
    if (encounter_pages_.empty()) {
        log_error("load_encounters: no valid \"maps\" entries — using built-in encounter tables");
        apply_default_encounter_pages(encounter_pages_);
        return true;
    }
    while (encounter_pages_.size() < 3) {
        encounter_pages_.push_back(encounter_pages_.back());
    }
    if (encounter_pages_.size() > 3) encounter_pages_.resize(3);
    return true;
}

std::vector<tce::MonsterId> DataLayerImpl::roll_monsters_for_battle(int map_page_index, NodeType node_type,
                                                                    tce::RunRng& rng) const {
    if (encounter_pages_.empty()) {
        return { "1.1_xiuqicanzun" };
    }
    const int clamped = map_page_index < 0 ? 0 : map_page_index;
    size_t pi = static_cast<size_t>(clamped);
    if (pi >= encounter_pages_.size()) pi = encounter_pages_.size() - 1;
    const EncounterPage& p = encounter_pages_[pi];
    switch (node_type) {
    case NodeType::Boss:
        if (!p.boss.empty()) return p.boss;
        return { "1.3_yazhongzhihun" };
    case NodeType::Elite: {
        if (p.elite_groups.empty()) return { "1.1_xiuqicanzun" };
        const int ng = static_cast<int>(p.elite_groups.size());
        const int pick = rng.uniform_int(0, ng - 1);
        return p.elite_groups[static_cast<size_t>(pick)];
    }
    case NodeType::Enemy:
    default: {
        if (p.enemy_groups.empty()) return { "1.1_xiuqicanzun" };
        const int ng = static_cast<int>(p.enemy_groups.size());
        const int pick = rng.uniform_int(0, ng - 1);
        return p.enemy_groups[static_cast<size_t>(pick)];
    }
    }
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
