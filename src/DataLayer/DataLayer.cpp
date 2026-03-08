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

#include "DataLayer/DataLayer.h"
#include "DataLayer/JsonParser.h"
#include <algorithm>
#include <fstream>

// -----------------------------------------------------------------------------
// tce 命名空间：B/C 依赖的接口，存储由 load_cards/load_monsters 填充，此处仅做哈希查找 O(1)
// 必须在 namespace DataLayer 之前定义，供 load_cards/load_monsters 写入 s_cards/s_monsters
// -----------------------------------------------------------------------------
namespace tce {

static std::unordered_map<CardId, CardData>    s_cards;
static std::unordered_map<MonsterId, MonsterData> s_monsters;

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

} // namespace tce

namespace DataLayer {

// cardType 直接按 DataLayer.hpp 枚举名解析，无额外映射
static tce::CardType card_type_from_string(const std::string& s) {
    if (s == "Attack") return tce::CardType::Attack;
    if (s == "Skill")  return tce::CardType::Skill;
    if (s == "Power")  return tce::CardType::Power;
    if (s == "Status") return tce::CardType::Status;
    if (s == "Curse")  return tce::CardType::Curse;
    return tce::CardType::Attack;
}
static tce::Rarity rarity_from_string(const std::string& s) {
    if (s == "common")   return tce::Rarity::Common;
    if (s == "uncommon") return tce::Rarity::Uncommon;
    if (s == "rare")     return tce::Rarity::Rare;
    return tce::Rarity::Common;
}

std::string DataLayerImpl::resolve_data_path(const std::string& base, const std::string& filename) const {
    if (base.empty()) return "data/" + filename;
    if (base.size() >= 5 && (base.substr(base.size() - 5) == ".json")) return base;
    std::string b = base;
    if (!b.empty() && b.back() != '/' && b.back() != '\\') b += "/";
    return b + "data/" + filename;
}

// 将 JSON 数组中的每条记录直接插入 tce::s_cards（唯一存储，供 B/C 与 DataLayer 共用）
bool DataLayerImpl::load_cards(const std::string& path_or_base_dir) {
    std::string path = resolve_data_path(path_or_base_dir, "cards.json");
    JsonValue root = parse_json_file(path);
    if (!root.is_array()) return false;
    tce::s_cards.clear();
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        tce::CardData cd;
        if (const JsonValue* p = v.get_key("id")) cd.id = p->as_string();
        if (const JsonValue* p = v.get_key("name")) cd.name = p->as_string();
        if (const JsonValue* p = v.get_key("cardType")) cd.cardType = card_type_from_string(p->as_string());
        if (const JsonValue* p = v.get_key("cost")) cd.cost = p->as_int();
        if (const JsonValue* p = v.get_key("rarity")) cd.rarity = rarity_from_string(p->as_string());
        if (const JsonValue* p = v.get_key("description")) cd.description = p->as_string();
        if (const JsonValue* p = v.get_key("exhaust")) cd.exhaust = p->as_bool();
        if (const JsonValue* p = v.get_key("ethereal")) cd.ethereal = p->as_bool();
        if (const JsonValue* p = v.get_key("innate")) cd.innate = p->as_bool();
        if (const JsonValue* p = v.get_key("retain")) cd.retain = p->as_bool();
        if (const JsonValue* p = v.get_key("unplayable")) cd.unplayable = p->as_bool();
        if (!cd.id.empty()) tce::s_cards[cd.id] = std::move(cd);
    }
    return !tce::s_cards.empty();
}

// 将怪物表每条记录直接插入 tce::s_monsters（唯一存储，供 B 与 DataLayer 共用）
bool DataLayerImpl::load_monsters(const std::string& path_or_base_dir) {
    std::string path = resolve_data_path(path_or_base_dir, "monsters.json");
    JsonValue root = parse_json_file(path);
    if (!root.is_array()) return false;
    tce::s_monsters.clear();
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        tce::MonsterData md;
        if (const JsonValue* p = v.get_key("id")) md.id = p->as_string();
        if (const JsonValue* p = v.get_key("name")) md.name = p->as_string();
        if (const JsonValue* p = v.get_key("maxHp")) md.maxHp = p->as_int();
        if (const JsonValue* p = v.get_key("isBoss")) {
            md.type = p->as_bool() ? tce::MonsterType::Boss : tce::MonsterType::Normal;
        } else if (const JsonValue* p = v.get_key("type")) {
            std::string t = p->as_string();
            if (t == "elite") md.type = tce::MonsterType::Elite;
            else if (t == "boss") md.type = tce::MonsterType::Boss;
            else md.type = tce::MonsterType::Normal;
        }
        if (!md.id.empty()) tce::s_monsters[md.id] = std::move(md);
    }
    return !tce::s_monsters.empty();
}

static EventOption parse_option(const JsonValue& v) {
    EventOption opt;
    if (!v.is_object()) return opt;
    if (const JsonValue* p = v.get_key("text")) opt.text = p->as_string();
    if (const JsonValue* p = v.get_key("next")) opt.next = p->as_string();
    if (const JsonValue* p = v.get_key("result")) {
        if (p->is_object()) {
            if (const JsonValue* pt = p->get_key("type")) opt.result.type = pt->as_string();
            if (const JsonValue* pv = p->get_key("value")) opt.result.value = pv->as_int();
        }
    }
    return opt;
}

// 同上：将事件表每条记录以 id 为 key 插入哈希表；options 为顺序表，逐项 push_back
bool DataLayerImpl::load_events(const std::string& path_or_base_dir) {
    std::string path = resolve_data_path(path_or_base_dir, "events.json");
    JsonValue root = parse_json_file(path);
    if (!root.is_array()) return false;
    events_.clear();
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        Event e;
        if (const JsonValue* p = v.get_key("id")) e.id = p->as_string();
        if (const JsonValue* p = v.get_key("title")) e.title = p->as_string();
        if (const JsonValue* p = v.get_key("description")) e.description = p->as_string();
        if (const JsonValue* p = v.get_key("options")) {
            if (p->is_array())
                for (const JsonValue& o : p->arr) e.options.push_back(parse_option(o));  // 顺序表尾插
        }
        if (!e.id.empty()) events_[e.id] = std::move(e);  // 哈希表插入
    }
    return !events_.empty();
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

int DataLayerImpl::rarity_order(tce::Rarity r) {
    switch (r) {
        case tce::Rarity::Common:   return 0;
        case tce::Rarity::Uncommon: return 1;
        case tce::Rarity::Rare:     return 2;
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
