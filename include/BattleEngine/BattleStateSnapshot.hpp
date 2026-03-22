/**
 * 战斗状态快照（供 UI 展示）
 */
#pragma once

#include "../BattleCoreRefactor/Types.hpp"
#include "../BattleCoreRefactor/BattleState.hpp"
#include "../CardSystem/CardSystem.hpp"
#include <string>
#include <vector>

namespace tce {

struct BattleStateSnapshot {
    std::string              playerName;
    std::string              character;
    int                      currentHp;
    int                      maxHp;
    int                      block;
    int                      gold;
    int                      energy;
    int                      maxEnergy;
    /** 本回合已打出牌数（与凡庸等规则对齐） */
    int                      cardsPlayedThisTurn = 0;
    Stance                   stance;
    int                      orbSlotCount;
    std::vector<OrbSlot>     orbSlots;
    int                      potionSlotCount = 3;
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
    std::wstring             phaseDebugLabel;
    std::vector<DamageDisplayEvent> pendingDamageDisplays;  // 受击伤害数字（玩家左、怪物右）
};

} // namespace tce
