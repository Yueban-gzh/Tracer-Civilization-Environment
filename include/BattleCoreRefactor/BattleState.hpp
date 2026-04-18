// 整场战斗的数据快照（纯数据，不写具体规则）

#pragma once                                    // 防止头文件重复包含

#include <vector>                               // 用于怪物列表
#include "Types.hpp"                            // PlayerBattleState、MonsterInBattle 等类型

namespace tce {

/** 受击时显示的伤害数字（供 UI 绘制，保留多帧后清除） */
struct DamageDisplayEvent {
    bool is_player = false;                      // true=玩家受击，false=怪物受击
    int  monster_index = 0;                     // 怪物下标（is_player 时忽略）
    int  amount = 0;                             // 伤害数值（显示总伤害，含被格挡部分）
    int  frames_remaining = 180;                 // 剩余显示帧数（3 秒 @60fps）
    bool hit_vfx = false;                        // 是否播放命中序列帧（玩家攻击牌对怪）
};

/** 玩家打出技能牌并增加格挡时触发 UI 序列帧（与 tick 同步倒计时） */
struct PlayerBlockVfxSignal {
    int frames_remaining = 180;
};

/** 对怪物施加中毒时触发 UI 序列帧（锚点与命中特效类似，优先于同帧攻击命中特效） */
struct MonsterPoisonVfxSignal {
    int monster_index      = 0;
    int frames_remaining = 180;
};

/** 玩家或怪物获得力量时：脚下 strength_1 序列（表现层） */
struct StrengthVfxSignal {
    bool is_player       = false;
    int  monster_index   = 0;
    int  frames_remaining = 180;
};

struct BattleState {                            // 整场战斗的完整状态（纯数据，不含规则逻辑）
    PlayerBattleState            player;         // 玩家当前状态（HP、格挡、能量、状态效果等）
    std::vector<MonsterInBattle> monsters;     // 场上怪物列表（含 HP、格挡、状态效果等）
    int                          turnNumber = 0;// 当前回合数（从 1 开始）
    std::vector<DamageDisplayEvent> pendingDamageDisplays;  // 本帧待显示的伤害（消费后清除）
    std::vector<PlayerBlockVfxSignal> pendingPlayerBlockVfx; // 玩家格挡类技能：待播序列帧（表现层）
    std::vector<MonsterPoisonVfxSignal> pendingMonsterPoisonVfx; // 对怪物施加中毒：待播序列帧（表现层）
    std::vector<StrengthVfxSignal>      pendingStrengthVfx;   // 获得力量：脚下待播序列帧

    enum class TurnPhase {                       // 回合阶段枚举
        Idle,                                   // 空闲
        PlayerTurnStart,                        // 玩家回合开始（抽牌、重置能量）
        PlayerTurnEnd,                          // 玩家回合结束（弃牌等）
        EnemyTurnStart,                         // 敌方回合开始（中毒等）
        EnemyTurnActions,                       // 敌方行动（怪物攻击等）
        EnemyTurnEnd                            // 敌方回合结束（格挡清零、duration 递减等）
    } phase = TurnPhase::Idle;                  // 当前所处阶段
};

} // namespace tce