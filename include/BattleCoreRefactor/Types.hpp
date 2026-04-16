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
    int gold            = 0;
    int potionSlotCount = 3;
    std::vector<PotionId>       potions;
    int cardsToDrawPerTurn      = 5;
    /** 本回合已成功打出的牌张数（凡庸：手牌中有凡庸时最多再打 3 张）。回合开始在战斗引擎中清零。 */
    int cardsPlayedThisTurn     = 0;
    /** 仪式匕首：本局游戏中所有仪式匕首基础伤害累计加值（斩杀后永久 +3/+5，跨战斗保留）。 */
    int ritualDaggerRunBonus    = 0;
    std::vector<StatusInstance> statuses;
    std::vector<RelicId>        relics;
};

// 怪物意图（与 MonsterBehaviors 共用）
enum class MonsterIntentKind {
    Attack,
    Mul_Attack,
    Block,
    Attack_And_Block,
    Attack_And_Weak,
    Attack_And_Vulnerable,
    Ritual,
    Strength_And_Player_Weak,
    Strength_And_Block,
    Strength_And_Player_Frail,
    Player_Dexterity_Down,
    Unknown,
    Buff,
    Debuff,
    Sleep,
    Stun,
};
struct MonsterIntent {
    MonsterIntentKind kind = MonsterIntentKind::Unknown;
    int value = 0;
    /** 攻击类意图的单段基础值；Mul_Attack 时 value 仍可保留总值供旧逻辑兼容。 */
    int baseValue = 0;
    /** Mul_Attack 的连击次数；非连击时为 1。 */
    int times = 1;
    /** 非空时悬停优先显示该别名（UTF-8）；JSON 可填 ui_label 或 display_name */
    std::string ui_label;
    /** 非空时覆盖意图图标的纹理查找 key；否则由 kind 决定默认 key */
    std::string ui_icon_key;
};

// 场上怪物（战斗实例）
struct MonsterInBattle {
    MonsterId id;
    int currentHp = 0;
    int maxHp     = 0;
    int block     = 0;
    MonsterIntent currentIntent;
    std::vector<StatusInstance> statuses;
    /** 当前玩家回合内已结算的玩家攻击伤害次数（用于飞行：层数 X = 本回合内第 X 次此类伤害后移除） */
    int flightAttackHitsThisTurn = 0;
    /** 从上次玩家回合开始起，该怪物已累计失去的生命值（用于坚不可摧：层数 X = 本回合最多再扣多少 HP） */
    int indestructibleDamageTakenThisTurn = 0;
    /** 蜷身：本场战斗是否已因受玩家攻击伤害触发过（每场战斗仅一次） */
    bool curlUpUsedThisBattle = false;
};

} // namespace tce
