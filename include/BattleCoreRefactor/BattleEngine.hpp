#pragma once

#include <functional>
#include <vector>
#include <string>
#include <deque>

#include "Types.hpp"
#include "BattleState.hpp"
#include "Damage.hpp"
#include "BattleModifier.hpp"

namespace tce {

struct CardData;
struct MonsterData;
class CardSystem;
class BattleEngine;

class EffectContext {
public:
    int target_monster_index = -1;
    int source_monster_index = -1;
    bool from_attack = false;
    int x_cost_spent = 0;

    void deal_damage_to_player(int amount);
    void deal_damage_to_monster(int monster_index, int amount);
    void deal_damage_to_player_ignoring_block(int amount);
    void deal_damage_to_monster_ignoring_block(int monster_index, int amount);

    void apply_status_to_player(StatusId id, int stacks, int duration);
    void apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration);
    /** 设置怪物某状态层数（替换而非叠加），用于内部追踪（如 red_louse 行动历史） */
    void set_monster_status_stacks(int monster_index, StatusId id, int stacks);

    // --- CardEffects 兼容接口 ---
    void add_block_to_player(int amount);
    void add_block_to_monster(int monster_index, int amount);
    int get_effective_damage_dealt_by_player(int base_damage, int target_monster_index) const;
    int get_effective_block_for_player(int base_block) const;
    void generate_to_discard_pile(CardId id);
    void generate_to_draw_pile(CardId id);
    /** 生成一张临时牌并加入手牌（手牌已满时按 CardSystem 规则落入弃牌堆） */
    void generate_to_hand(CardId id);
    void draw_cards(int n);
    void add_energy_to_player(int amount);
    int get_status_stacks_on_monster(int monster_index, const StatusId& id) const;
    int get_status_stacks_on_player(const StatusId& id) const;
    int get_monster_current_hp(int monster_index) const;
    void apply_status_to_all_monsters(StatusId id, int stacks, int duration);
    void deal_damage_to_all_monsters(int base_damage);
    void add_player_max_hp(int amount);
    int get_player_block() const;
    /** 当前能量（用于如双倍能量等效果） */
    int get_player_energy() const;
    /** 弃牌堆张数（用于如堆栈等效果） */
    int get_discard_pile_size() const;
    /** 抽牌堆张数（用于如汇集等效果） */
    int get_draw_pile_size() const;
    /** 将手牌全部移入消耗堆，返回消耗张数（如恶魔之焰）。 */
    int exhaust_all_hand_cards();
    /** 随机弃掉手牌中的 n 张，返回实际弃牌张数。 */
    int discard_random_hand_cards(int n);
    /** 弃掉全部手牌，返回实际弃牌张数。 */
    int discard_all_hand_cards();
    /** 按预选实例顺序弃牌，最多 n 张；返回实际弃牌张数。 */
    int discard_selected_hand_cards(int n);
    /** 随机消耗手牌中的 n 张，返回实际消耗张数。 */
    int exhaust_random_hand_cards(int n);
    /** 按预选实例顺序消耗手牌，最多 n 张；返回实际消耗张数。 */
    int exhaust_selected_hand_cards(int n);
    /** 消耗手牌中全部非攻击牌，返回实际消耗张数。 */
    int exhaust_non_attack_hand_cards();
    /** 随机升级手牌中的 n 张可升级牌，返回实际升级张数。 */
    int upgrade_random_cards_in_hand(int n);
    /** 升级手牌中全部可升级牌，返回升级张数。 */
    int upgrade_all_cards_in_hand();
    /** 升级当前战斗中的全部牌（手/抽/弃/消耗），返回升级张数（如神化）。 */
    int upgrade_all_cards_in_combat();
    /** 是否存在任意存活怪物，其当前意图为攻击（用于 Spot Weakness）。 */
    bool any_monster_intends_attack() const;
    /** 获得一瓶随机药水（药水槽已满时返回空串）。 */
    PotionId grant_random_potion();

private:
    friend class BattleEngine;
    BattleEngine* engine_ = nullptr;
};

class BattleEngine {
public:
    using GetMonsterByIdFn       = std::function<const MonsterData*(MonsterId)>;
    using GetCardByIdFn          = std::function<const CardData*(CardId)>;
    using ExecuteMonsterActionFn = std::function<void(MonsterId id, int turn_number, EffectContext& ctx, int monster_index)>;

    friend class EffectContext;

    BattleEngine(CardSystem& card_system,
                 GetMonsterByIdFn get_monster,
                 GetCardByIdFn get_card,
                 ExecuteMonsterActionFn execute_monster = nullptr);

    void start_battle(const std::vector<MonsterId>& monster_ids,
                      const PlayerBattleState&      player_state,
                      const std::vector<CardId>&    deck_card_ids,
                      const std::vector<RelicId>&   relic_ids);

    BattleState get_battle_state() const;
    /** 清除本帧伤害显示事件（主循环在绘制后调用，避免重复显示） */
    void clear_pending_damage_displays();
    /** 每帧调用：递减伤害数字显示时长，移除过期项（约 2.5 秒） */
    void tick_damage_displays();

    bool play_card(int hand_index, int target_monster_index);
    void end_turn();
    bool is_battle_over() const;
    /** 是否胜利（玩家存活且怪物全灭）；失败时 is_battle_over 也为 true */
    bool is_victory() const;
    /** 战斗胜利后调用，返回 count 张可选奖励卡牌 id（如 3 张三选一） */
    std::vector<CardId> get_reward_cards(int count);
    /** 战斗胜利后调用，根据击败怪物类型计算金币并加入玩家 */
    void grant_victory_gold();
    /** 返回本场胜利将获得的金币数（调用 grant_victory_gold 前可先查询用于 UI 显示） */
    int get_victory_gold() const;
    /** 战斗胜利后调用，随机获得 1 个遗物并加入玩家，返回获得的遗物 id（空串表示未获得） */
    RelicId grant_reward_relic();
    /** 丢弃指定遗物（移除第一个匹配项），并回滚其拾起时的效果；返回是否成功移除 */
    bool remove_relic(RelicId id);
    /** 往牌组加入一张牌（奖励/商店等），并触发陶瓷小鱼等遗物效果 */
    void add_card_to_master_deck(CardId id);
    /** 战斗胜利后调用，若药水栏未满则随机获得 1 瓶药水，返回获得的药水 id（空串表示未获得） */
    PotionId grant_reward_potion();

    /** target_monster_index: 需指定目标的药水（如毒药水）传入怪物下标；无需目标的传 -1 */
    bool use_potion(int slot_index, int target_monster_index = -1);
    void step_turn_phase();

    // --- 金手指接口（CheatEngine 直接修改状态）---
    void cheat_set_player_hp(int hp);
    void cheat_set_player_max_hp(int hp);
    void cheat_set_player_block(int b);
    void cheat_set_player_energy(int e);
    void cheat_set_player_gold(int g);
    void cheat_set_player_status(StatusId id, int stacks, int duration);
    void cheat_remove_player_status(StatusId id);
    void cheat_set_monster_hp(int idx, int hp);
    void cheat_set_monster_max_hp(int idx, int hp);
    void cheat_set_monster_status(int idx, StatusId id, int stacks, int duration);
    void cheat_remove_monster_status(int idx, StatusId id);
    void cheat_kill_monster(int idx);
    void cheat_add_relic(RelicId id);
    void cheat_remove_relic(RelicId id);
    void cheat_add_potion(PotionId id);
    void cheat_remove_potion(int slot);
    void cheat_add_hand(CardId id);
    void cheat_remove_hand(CardId id);

    static int  get_status_stacks(const std::vector<StatusInstance>& list, const StatusId& id);
    static void reduce_status_stacks(std::vector<StatusInstance>& list, const StatusId& id, int amount);

    // --- 供 EffectContext 调用的内部接口（CardEffects 兼容）---
    void add_block_to_player_impl(int amount);
    void add_block_to_monster_impl(int monster_index, int amount);
    int get_effective_damage_dealt_by_player_impl(int base_damage, int target_monster_index) const;
    int get_effective_block_for_player_impl(int base_block) const;
    void generate_to_discard_pile_impl(CardId id);
    void generate_to_draw_pile_impl(CardId id);
    void generate_to_hand_impl(CardId id);
    void draw_cards_impl(int n);
    void add_energy_to_player_impl(int amount);
    int get_status_stacks_on_monster_impl(int monster_index, const StatusId& id) const;
    int get_status_stacks_on_player_impl(const StatusId& id) const;
    int get_monster_current_hp_impl(int monster_index) const;
    void apply_status_to_player_impl(StatusId id, int stacks, int duration);
    void apply_status_to_monster_impl(int monster_index, StatusId id, int stacks, int duration);
    void set_monster_status_stacks_impl(int monster_index, StatusId id, int stacks);
    void apply_status_to_all_monsters_impl(StatusId id, int stacks, int duration);
    void deal_damage_to_all_monsters_impl(int base_damage);
    int get_player_block_impl() const;
    int get_player_energy_impl() const;
    int get_discard_pile_size_impl() const;
    int get_draw_pile_size_impl() const;
    int exhaust_all_hand_cards_impl();
    int discard_random_hand_cards_impl(int n);
    int discard_all_hand_cards_impl();
    int discard_selected_hand_cards_impl(int n);
    int exhaust_random_hand_cards_impl(int n);
    int exhaust_selected_hand_cards_impl(int n);
    int exhaust_non_attack_hand_cards_impl();
    int upgrade_random_cards_in_hand_impl(int n);
    int upgrade_all_cards_in_hand_impl();
    int upgrade_all_cards_in_combat_impl();
    bool any_monster_intends_attack_impl() const;
public:
    /** 为下一次卡牌效果注入“手牌选择结果”（实例 id 队列，按顺序消费）。 */
    void set_effect_selected_instance_ids(const std::vector<InstanceId>& ids);
private:
    std::deque<InstanceId> effect_selected_instance_ids_;
    void add_player_max_hp_impl(int amount);

private:
    BattleState           state_;
    CardSystem*           card_system_ = nullptr;
    GetMonsterByIdFn      get_monster_by_id_;
    GetCardByIdFn         get_card_by_id_;
    ExecuteMonsterActionFn execute_monster_action_;
    mutable ModifierSystem modifiers_;  // mutable：get_effective_damage 需广播但不改逻辑状态

    void apply_damage_to_player(DamagePacket& dmg);
    void apply_damage_to_monster(DamagePacket& dmg);

    void handle_player_turn_start();
    void handle_player_turn_end();
    void handle_enemy_turn_start();
    void handle_enemy_turn_actions();
    void handle_enemy_turn_end();

    void on_monster_just_died(int monsterIndex);
    void on_player_just_died();

    void build_modifiers_from_state();
    void fill_effect_context(EffectContext& ctx);
};

} // namespace tce
