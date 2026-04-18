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
    int                      potionSlotCount = 3;
    std::vector<PotionId>    potions;
    std::vector<RelicId>     relics;
    std::vector<StatusInstance> playerStatuses;
    std::vector<CardInstance>   hand;
    int                      drawPileSize   = 0;
    int                      discardPileSize = 0;
    int                      exhaustPileSize = 0;
    /** 弃牌堆内卡牌 id 顺序（与 CardSystem::get_discard_pile 一致，用于洗牌飞牌等表现） */
    std::vector<CardId>      discardPileCardIds;
    std::vector<MonsterInBattle> monsters;
    std::vector<std::vector<StatusInstance>> monsterStatuses;
    int                      turnNumber     = 0;
    std::wstring             phaseDebugLabel;
    std::vector<DamageDisplayEvent> pendingDamageDisplays;  // 受击伤害数字（玩家左、怪物右）
    std::vector<PlayerBlockVfxSignal> pendingPlayerBlockVfx; // 玩家格挡技能序列帧触发
    std::vector<MonsterPoisonVfxSignal> pendingMonsterPoisonVfx; // 怪物中毒施加序列帧
    std::vector<StrengthVfxSignal>    pendingStrengthVfx;      // 玩家/怪物获得力量：脚下序列帧
};

} // namespace tce
