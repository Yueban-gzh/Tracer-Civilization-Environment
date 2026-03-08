/**
 * DataLayer 实现：加载 data/*.json，哈希表按 id 查找，排序
 *
 * 课设数据结构对应：
 * - 哈希表：unordered_map，以 id 为关键字进行插入与查找；
 * - 排序：对 vector 使用比较排序（按稀有度/分数关键字）。
 */

#include "DataLayer/DataLayer.h"
#include "DataLayer/JsonParser.h"
#include <algorithm>
#include <fstream>

namespace DataLayer {

std::string DataLayerImpl::resolve_data_path(const std::string& base, const std::string& filename) const {
    if (base.empty()) return "data/" + filename;
    if (base.size() >= 5 && (base.substr(base.size() - 5) == ".json")) return base;
    std::string b = base;
    if (!b.empty() && b.back() != '/' && b.back() != '\\') b += "/";
    return b + "data/" + filename;
}

// 将 JSON 数组中的每条记录插入哈希表 cards_，以 id 为 key（散列存储）
bool DataLayerImpl::load_cards(const std::string& path_or_base_dir) {
    std::string path = resolve_data_path(path_or_base_dir, "cards.json");
    JsonValue root = parse_json_file(path);
    if (!root.is_array()) return false;
    cards_.clear();
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        Card c;
        if (const JsonValue* p = v.get_key("id")) c.id = p->as_string();
        if (const JsonValue* p = v.get_key("name")) c.name = p->as_string();
        if (const JsonValue* p = v.get_key("cardType")) c.cardType = p->as_string();
        if (const JsonValue* p = v.get_key("cost")) c.cost = p->as_int();
        if (const JsonValue* p = v.get_key("rarity")) c.rarity = p->as_string();
        if (const JsonValue* p = v.get_key("description")) c.description = p->as_string();
        if (const JsonValue* p = v.get_key("effectType")) c.effectType = p->as_string();
        if (const JsonValue* p = v.get_key("effectValue")) c.effectValue = p->as_int();
        if (!c.id.empty()) cards_[c.id] = std::move(c);  // 哈希表插入：key=c.id
    }
    return !cards_.empty();
}

// 同上：将怪物表每条记录以 id 为 key 插入哈希表 monsters_
bool DataLayerImpl::load_monsters(const std::string& path_or_base_dir) {
    std::string path = resolve_data_path(path_or_base_dir, "monsters.json");
    JsonValue root = parse_json_file(path);
    if (!root.is_array()) return false;
    monsters_.clear();
    for (const JsonValue& v : root.arr) {
        if (!v.is_object()) continue;
        Monster m;
        if (const JsonValue* p = v.get_key("id")) m.id = p->as_string();
        if (const JsonValue* p = v.get_key("name")) m.name = p->as_string();
        if (const JsonValue* p = v.get_key("isBoss")) m.isBoss = p->as_bool();
        if (const JsonValue* p = v.get_key("maxHp")) m.maxHp = p->as_int();
        if (const JsonValue* p = v.get_key("intentPattern")) m.intentPattern = p->as_string();
        if (const JsonValue* p = v.get_key("attackDamage")) m.attackDamage = p->as_int();
        if (const JsonValue* p = v.get_key("blockAmount")) m.blockAmount = p->as_int();
        if (!m.id.empty()) monsters_[m.id] = std::move(m);  // 哈希表插入
    }
    return !monsters_.empty();
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

// 哈希查找：按 key(id) 在哈希表中查找，平均 O(1)
const Card* DataLayerImpl::get_card_by_id(const CardId& id) const {
    auto it = cards_.find(id);
    if (it == cards_.end()) return nullptr;
    return &it->second;
}

const Monster* DataLayerImpl::get_monster_by_id(const MonsterId& id) const {
    auto it = monsters_.find(id);
    if (it == monsters_.end()) return nullptr;
    return &it->second;
}

const Event* DataLayerImpl::get_event_by_id(const EventId& id) const {
    auto it = events_.find(id);
    if (it == events_.end()) return nullptr;
    return &it->second;
}

// 将稀有度映射为可比较的序数（用于排序的关键字）
int DataLayerImpl::rarity_order(const std::string& rarity) const {
    if (rarity == "common")   return 0;
    if (rarity == "uncommon") return 1;
    if (rarity == "rare")     return 2;
    return 0;
}

// 排序：对线性表（vector）按关键字 rarity 升序排序；比较排序，时间复杂度 O(n log n)
std::vector<CardId> DataLayerImpl::sort_cards_by_rarity(const std::vector<CardId>& card_ids) const {
    std::vector<CardId> out = card_ids;
    std::sort(out.begin(), out.end(), [this](const CardId& a, const CardId& b) {
        const Card* ca = get_card_by_id(a);
        const Card* cb = get_card_by_id(b);
        int ra = ca ? rarity_order(ca->rarity) : 0;
        int rb = cb ? rarity_order(cb->rarity) : 0;
        if (ra != rb) return ra < rb;  // 先按稀有度关键字：common < uncommon < rare
        return a < b;                   // 同稀有度按 id 字典序稳定
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
