/*
 * BattleCoreRefactor 公共类型（扩展 Common/Types.hpp）
 * 与 E 约定一致，B、C、E 共用
 */
#pragma once

#include "../Common/Types.hpp"
#include <string>
#include <vector>

namespace tce {

// buff、debuff 实例（只存"数据"，不存"规则"） // 规则在 StatusModifiers 里
struct StatusInstance {
    StatusId id;      // 状态ID
    int      stacks   = 0;   // 层数
    int      duration = 0;  // 持续回合(-1永久)
};

// 充能球槽位（占位：未来可扩展宝珠/充能系统）
struct OrbSlot {
    std::string orbId;
    int         stacks = 0;
};

// 玩家战斗状态（纯数据）
struct PlayerBattleState {
    std::string playerName;
    std::string character;
    int currentHp      = 0;
    int maxHp          = 0;
    int block          = 0;
    int energy         = 0;
    int maxEnergy      = 0;
    Stance stance      = Stance::Neutral;
    int orbSlotCount   = 0;
    std::vector<OrbSlot>        orbSlots;
    int gold            = 0;
    int potionSlotCount = 3;
    std::vector<PotionId>       potions;
    int cardsToDrawPerTurn      = 5;
    std::vector<StatusInstance> statuses;
    std::vector<RelicId>        relics;
};

// 怪物意图（与 MonsterBehaviors 共用）
enum class MonsterIntentKind {
    Attack, Block, Unknown, Buff, Debuff, Sleep, Stun,
};
struct MonsterIntent {
    MonsterIntentKind kind = MonsterIntentKind::Unknown;
    int value = 0;
};

// 场上怪物（战斗实例）
struct MonsterInBattle {
    MonsterId id;
    int currentHp = 0;
    int maxHp     = 0;
    int block     = 0;
    MonsterIntent currentIntent;
    std::vector<StatusInstance> statuses;
};

} // namespace tce
