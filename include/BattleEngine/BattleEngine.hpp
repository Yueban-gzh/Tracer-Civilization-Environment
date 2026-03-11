/**
 * B - 战斗引擎：回合、敌我状态、出牌结算、遗物/药水
 */
#pragma once

#include "../Common/Types.hpp"
#include "../DataLayer/DataLayer.hpp"
#include "../CardSystem/CardSystem.hpp"
#include <vector>
#include <string>
#include <functional>

namespace tce {

// --- 充能球槽位 ---
struct OrbSlot {
    std::string orbId;  // 如 "lightning"/"frost"/"dark"
    int         stacks  = 0;
};

// --- 玩家战斗状态 ---
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
    std::vector<PotionId>    potions;
};

// --- 场上怪物 ---
struct MonsterInBattle {
    MonsterId  id;
    int        currentHp = 0;
    int        maxHp     = 0;
    std::string currentIntent;
};

// --- 单条增益/减益 ---
struct StatusInstance {
    StatusId id;
    int      stacks   = 0;
    int      duration = 0;  // -1 表示持续到战斗结束
};

// --- 效果上下文（B 构造，传给 C 的 execute_effect）---
class EffectContext {
public:
    int target_monster_index = -1;
    // 可扩展：玩家/怪物状态指针、施加状态接口等，由 B 在实现中填充
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

    bool use_potion(int slot_index);

private:
    CardSystem*       card_system_ = nullptr;
    GetMonsterByIdFn  get_monster_by_id_;
    GetCardByIdFn     get_card_by_id_;

    std::vector<MonsterInBattle>  monsters_;
    PlayerBattleState             player_state_;
    std::vector<RelicId>          relic_ids_;
    std::vector<StatusInstance>   player_statuses_;
    std::vector<std::vector<StatusInstance>> monster_statuses_;
    int                           turn_number_ = 0;
};

} // namespace tce
