/**
 * DataLayer 实现：加载 data/*.json，哈希表按 id 查找，排序
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
        if (!c.id.empty()) cards_[c.id] = std::move(c);
    }
    return !cards_.empty();
}

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
        if (!m.id.empty()) monsters_[m.id] = std::move(m);
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
                for (const JsonValue& o : p->arr) e.options.push_back(parse_option(o));
        }
        if (!e.id.empty()) events_[e.id] = std::move(e);
    }
    return !events_.empty();
}

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

int DataLayerImpl::rarity_order(const std::string& rarity) const {
    if (rarity == "common")   return 0;
    if (rarity == "uncommon") return 1;
    if (rarity == "rare")     return 2;
    return 0;
}

std::vector<CardId> DataLayerImpl::sort_cards_by_rarity(const std::vector<CardId>& card_ids) const {
    std::vector<CardId> out = card_ids;
    std::sort(out.begin(), out.end(), [this](const CardId& a, const CardId& b) {
        const Card* ca = get_card_by_id(a);
        const Card* cb = get_card_by_id(b);
        int ra = ca ? rarity_order(ca->rarity) : 0;
        int rb = cb ? rarity_order(cb->rarity) : 0;
        if (ra != rb) return ra < rb;  // common < uncommon < rare
        return a < b;
    });
    return out;
}

std::vector<DataLayerImpl::LeaderboardEntry> DataLayerImpl::sort_leaderboard(
    std::vector<LeaderboardEntry> entries) const {
    std::sort(entries.begin(), entries.end(),
        [](const LeaderboardEntry& a, const LeaderboardEntry& b) { return a.score > b.score; });
    return entries;
}

} // namespace DataLayer
