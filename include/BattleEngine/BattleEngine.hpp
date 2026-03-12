/**
 * B - 战斗引擎：回合、敌我状态、出牌结算、遗物/药水
 */
#pragma once

#include "../Common/Types.hpp"
#include "../DataLayer/DataLayer.hpp"
#include "MonsterBehaviors.hpp"
#include "../CardSystem/CardSystem.hpp"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace tce {

// --- 充能球槽位 ---
struct OrbSlot {
    std::string orbId;  // 如 "lightning"/"frost"/"dark"
    int         stacks  = 0;
};

// --- 单条增益/减益 ---
struct StatusInstance {
    StatusId id;
    int      stacks   = 0;
    int      duration = 0;  // -1 表示持续到战斗结束
};

// --- 玩家战斗状态（实例：身上效果挂在本实例上）---
struct PlayerBattleState {
    std::string              playerName;
    std::string              character;
    int                      currentHp  = 0;
    int                      maxHp      = 0;
    int                      block      = 0;
    int                      energy     = 0;
    int                      maxEnergy  = 0;
    Stance                   stance     = Stance::Neutral;
    int                      orbSlotCount = 0;
    std::vector<OrbSlot>     orbSlots;
    int                      gold       = 0;
    int                      potionSlotCount = 3;  // 药水槽数量，数据驱动，默认 3
    std::vector<PotionId>    potions;
    int                      cardsToDrawPerTurn = 5;  // 每回合开始时的抽牌数（可由遗物/效果修改）
    std::vector<StatusInstance> statuses;  // 本场战斗内玩家身上的增益/减益
};

// --- 场上怪物（实例：身上效果挂在本实例上）---
struct MonsterInBattle {
    MonsterId  id;
    int        currentHp = 0;
    int        maxHp     = 0;
    int        block     = 0;  // 格挡，受伤时先吸收再扣血，回合末可清空
    MonsterIntent currentIntent;
    std::vector<StatusInstance> statuses;  // 本场战斗内该怪物身上的增益/减益
};

// --- 效果上下文（B 构造，传给 C 的 execute_effect）---
// 卡牌效果应使用「有效数值」接口算出最终数值再传给伤害/格挡，这样会受双方增减益影响
class EffectContext {
public:
    int target_monster_index = -1;

    void add_block_to_player(int amount) const { if (add_block_to_player_) add_block_to_player_(amount); }
    void add_block_to_monster(int monster_index, int amount) const { if (add_block_to_monster_) add_block_to_monster_(monster_index, amount); }
    void deal_damage_to_player(int amount) const { if (deal_damage_to_player_) deal_damage_to_player_(amount); }
    void deal_damage_to_monster(int monster_index, int amount) const { if (deal_damage_to_monster_) deal_damage_to_monster_(monster_index, amount); }
    /** 无视格挡造成伤害（如中毒），直接扣血 */
    void deal_damage_to_player_ignoring_block(int amount) const { if (deal_damage_to_player_ignoring_block_) deal_damage_to_player_ignoring_block_(amount); }
    void deal_damage_to_monster_ignoring_block(int monster_index, int amount) const { if (deal_damage_to_monster_ignoring_block_) deal_damage_to_monster_ignoring_block_(monster_index, amount); }

    /** 玩家对目标怪物造成伤害时的有效数值（受玩家力量/虚弱、目标易伤等影响） */
    int get_effective_damage_dealt_by_player(int base_damage, int target_monster_index) const {
        return get_effective_damage_dealt_by_player_ ? get_effective_damage_dealt_by_player_(base_damage, target_monster_index) : base_damage;
    }
    /** 怪物对玩家造成伤害时的有效数值（受玩家易伤等影响） */
    int get_effective_damage_dealt_to_player(int base_damage, int attacker_monster_index) const {
        return get_effective_damage_dealt_to_player_ ? get_effective_damage_dealt_to_player_(base_damage, attacker_monster_index) : base_damage;
    }
    /** 玩家获得格挡时的有效数值（受玩家敏捷、脆弱等影响） */
    int get_effective_block_for_player(int base_block) const {
        return get_effective_block_for_player_ ? get_effective_block_for_player_(base_block) : base_block;
    }

    /** 对玩家施加增益/减益（如力量、虚弱）；duration -1 表示持续到战斗结束或直到被清除 */
    void apply_status_to_player(StatusId id, int stacks, int duration) const {
        if (apply_status_to_player_) apply_status_to_player_(std::move(id), stacks, duration);
    }
    /** 对指定怪物施加增益/减益（如易伤）；duration -1 表示持续到战斗结束或直到被清除 */
    void apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration) const {
        if (apply_status_to_monster_) apply_status_to_monster_(monster_index, std::move(id), stacks, duration);
    }

    /** 战斗中生成一张牌到弃牌堆（用于如“愤怒：在弃牌堆加入一张此牌”一类效果）。 */
    void generate_to_discard_pile(CardId id) const {
        if (generate_to_discard_pile_) generate_to_discard_pile_(std::move(id));
    }

    /** 抽牌（从抽牌堆抽 n 张到手牌，按 CardSystem 的抽牌规则）。 */
    void draw_cards(int n) const { if (draw_cards_) draw_cards_(n); }

    /** 增加玩家能量（用于如“飞踢：若目标易伤则获得1点能量”）。 */
    void add_energy_to_player(int amount) const { if (add_energy_to_player_) add_energy_to_player_(amount); }

    /** 读取怪物身上某状态的层数（如 vulnerable/weak）。 */
    int get_status_stacks_on_monster(int monster_index, const StatusId& id) const {
        return get_status_stacks_on_monster_ ? get_status_stacks_on_monster_(monster_index, id) : 0;
    }

    /** 读取玩家身上某状态的层数（如 strength），用于如「重刃」这类按力量多倍加成的卡牌。 */
    int get_status_stacks_on_player(const StatusId& id) const {
        return get_status_stacks_on_player_ ? get_status_stacks_on_player_(id) : 0;
    }

    /** 对所有存活怪物施加同一状态（如「闪电霹雳：所有敌人获得 1 层易伤」）。 */
    void apply_status_to_all_monsters(StatusId id, int stacks, int duration) const {
        if (apply_status_to_all_monsters_) apply_status_to_all_monsters_(std::move(id), stacks, duration);
    }

    /** 对所有存活怪物造成伤害（会对每个怪物分别计算易伤等影响）。 */
    void deal_damage_to_all_monsters(int base_damage) const {
        if (deal_damage_to_all_monsters_) deal_damage_to_all_monsters_(base_damage);
    }

    /** 读取玩家当前格挡值（用于如「全身撞击：造成你当前格挡值的伤害」一类效果）。 */
    int get_player_block() const {
        return get_player_block_ ? get_player_block_() : 0;
    }

    std::function<void(int)> add_block_to_player_;
    std::function<void(int, int)> add_block_to_monster_;
    std::function<void(int)> deal_damage_to_player_;
    std::function<void(int, int)> deal_damage_to_monster_;
    std::function<void(int)> deal_damage_to_player_ignoring_block_;
    std::function<void(int, int)> deal_damage_to_monster_ignoring_block_;
    std::function<int(int, int)> get_effective_damage_dealt_by_player_;
    std::function<int(int, int)> get_effective_damage_dealt_to_player_;
    std::function<int(int)> get_effective_block_for_player_;
    std::function<void(StatusId, int, int)> apply_status_to_player_;
    std::function<void(int, StatusId, int, int)> apply_status_to_monster_;
    std::function<void(CardId)> generate_to_discard_pile_;
    std::function<void(int)> draw_cards_;
    std::function<void(int)> add_energy_to_player_;
    std::function<int(int, const StatusId&)> get_status_stacks_on_monster_;
    std::function<int(const StatusId&)> get_status_stacks_on_player_;
    std::function<void(StatusId, int, int)> apply_status_to_all_monsters_;
    std::function<void(int)> deal_damage_to_all_monsters_;
    std::function<int()> get_player_block_;
};

// --- 战斗状态快照（供 UI）---
struct BattleStateSnapshot {
    std::string              playerName;
    std::string              character;
    int                      currentHp;
    int                      maxHp;
    int                      block;
    int                      gold;
    int                      energy;
    int                      maxEnergy;
    Stance                   stance;
    int                      orbSlotCount;
    std::vector<OrbSlot>     orbSlots;
    int                      potionSlotCount = 3;  // 药水槽数量，默认 3
    std::vector<PotionId>    potions;
    std::vector<RelicId>     relics;
    std::vector<StatusInstance> playerStatuses;
    std::vector<CardInstance>   hand;
    int                      drawPileSize   = 0;
    int                      discardPileSize = 0;
    int                      exhaustPileSize = 0;
    std::vector<MonsterInBattle> monsters;
    std::vector<std::vector<StatusInstance>> monsterStatuses;
    int                      turnNumber     = 0;
    // 当前回合阶段（供 UI 调试显示，例：L"玩家回合开始"）
    std::wstring             phaseDebugLabel;
};

class BattleEngine {
public:
    using GetMonsterByIdFn = std::function<const MonsterData*(MonsterId)>;
    using GetCardByIdFn    = std::function<const CardData*(CardId)>;

    BattleEngine(CardSystem& card_system, GetMonsterByIdFn get_monster, GetCardByIdFn get_card);

    void start_battle(const std::vector<MonsterId>& monster_ids,
                      const PlayerBattleState&      player_state,
                      const std::vector<CardId>&    deck_card_ids,
                      const std::vector<RelicId>&   relic_ids);

    BattleStateSnapshot get_battle_state() const;

    bool play_card(int hand_index, int target_monster_index);

    void end_turn();

    bool is_battle_over() const;

    std::vector<CardId> get_reward_cards(int count);

    void apply_status_to_player(StatusId id, int stacks, int duration);
    void apply_status_to_monster(int monster_index, StatusId id, int stacks, int duration);

    void add_block_to_player(int amount);
    void add_block_to_monster(int monster_index, int amount);
    void deal_damage_to_player(int amount);
    void deal_damage_to_monster(int monster_index, int amount);
    /** 无视格挡造成伤害（如中毒），直接扣血 */
    void deal_damage_to_player_ignoring_block(int amount);
    void deal_damage_to_monster_ignoring_block(int monster_index, int amount);

    bool use_potion(int slot_index);

    /** 回合末状态 tick：stacks=层数，target_is_player=是否玩家，monster_index=怪物下标（玩家时为 -1），ctx 可用来造成无视格挡伤害等 */
    using StatusTickFn = std::function<void(int stacks, bool target_is_player, int monster_index, EffectContext& ctx)>;
    void register_status_tick(StatusId id, StatusTickFn fn);

    /** 数值计算：受双方增减益影响后的伤害/格挡（卡牌效果应通过 EffectContext 的 get_effective_* 调用，此处供 B 内部绑定） */
    int get_effective_damage_dealt_by_player(int base_damage, int target_monster_index) const;
    int get_effective_damage_dealt_to_player(int base_damage, int attacker_monster_index) const;
    int get_effective_block_for_player(int base_block) const;

    /** 按阶段推进一小步，用于配合 UI 动画的逐帧结算 */
    void step_turn_phase();

private:
    /** 回合流程阶段枚举 */
    enum class TurnPhase {
        Idle,
        PlayerTurnEnd,
        EnemyTurnStart,
        EnemyTurnActions,
        EnemyTurnEnd,
        PlayerTurnStart,
    };

    /** 各阶段内部处理函数 */
    void phase_player_turn_end(EffectContext& ctx);
    void phase_enemy_turn_start(EffectContext& ctx);
    void phase_enemy_turn_actions(EffectContext& ctx);
    void phase_enemy_turn_end(EffectContext& ctx);
    void phase_player_turn_start(EffectContext& ctx);

    void fill_effect_context(EffectContext& ctx);
    static int get_status_stacks(const std::vector<StatusInstance>& list, const StatusId& id);
    /** 将 list 中 id 对应状态的层数减少 amount（最少为 0），若为 0 则移除该条 */
    static void reduce_status_stacks(std::vector<StatusInstance>& list, const StatusId& id, int amount);

    CardSystem*       card_system_ = nullptr;
    GetMonsterByIdFn  get_monster_by_id_;
    GetCardByIdFn     get_card_by_id_;

    std::vector<MonsterInBattle>  monsters_;
    PlayerBattleState             player_state_;
    std::vector<RelicId>          relic_ids_;
    int                           turn_number_ = 0;
    std::unordered_map<StatusId, StatusTickFn> status_tick_registry_;

    // 回合阶段驱动
    TurnPhase                     turn_phase_ = TurnPhase::Idle;
    int                           turn_phase_hold_frames_ = 0;

    // 用于防止荆棘反伤递归
    bool                          in_thorns_recoil_ = false;
    // 标记当前是否正在结算一张牌的效果（用于区分「从牌中失去生命」与其它来源）
    bool                          in_card_effect_ = false;
};

} // namespace tce
