#pragma once

#include <functional>
#include <vector>
#include <string>

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
    void draw_cards(int n);
    void add_energy_to_player(int amount);
    int get_status_stacks_on_monster(int monster_index, const StatusId& id) const;
    int get_status_stacks_on_player(const StatusId& id) const;
    void apply_status_to_all_monsters(StatusId id, int stacks, int duration);
    void deal_damage_to_all_monsters(int base_damage);
    int get_player_block() const;

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
    void draw_cards_impl(int n);
    void add_energy_to_player_impl(int amount);
    int get_status_stacks_on_monster_impl(int monster_index, const StatusId& id) const;
    int get_status_stacks_on_player_impl(const StatusId& id) const;
    void apply_status_to_player_impl(StatusId id, int stacks, int duration);
    void apply_status_to_monster_impl(int monster_index, StatusId id, int stacks, int duration);
    void set_monster_status_stacks_impl(int monster_index, StatusId id, int stacks);
    void apply_status_to_all_monsters_impl(StatusId id, int stacks, int duration);
    void deal_damage_to_all_monsters_impl(int base_damage);
    int get_player_block_impl() const;

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
