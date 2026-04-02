// 伤害包（战斗里所有"扣血事件"的统一描述）

#pragma once                                    // 防止头文件重复包含

#include "Types.hpp"                            // CardId 等基础类型

namespace tce {

struct DamagePacket {                           // 伤害包：描述一次伤害的完整信息，供 Modifier 修改
    int  raw_amount      = 0;                    // 原始伤害值（未经过力量/易伤/虚弱等修正）
    int  modified_amount = 0;                   // 修正后伤害值（Modifier 修改后用于实际扣血）
    bool ignore_block    = false;                // 是否无视格挡（如中毒、勒脖等直接扣血）
    bool can_be_reduced  = true;                 // 是否可被减伤（如 intangible 等）

    enum class SourceType {                     // 伤害来源类型
        Player,                                 // 玩家（卡牌、遗物等）
        Monster,                                // 怪物（攻击等）
        Relic,                                  // 遗物
        Status,                                 // 状态效果（毒、勒脖、尸体爆炸等）
        Potion                                  // 药水
    } source_type = SourceType::Player;         // 当前伤害来源类型

    int source_monster_index = -1;              // 造成伤害的怪物下标（怪物攻击时用）
    int target_monster_index = -1;              // 承受伤害的怪物下标（玩家打怪时用）
    bool from_attack = false;                    // 是否来自攻击（易伤、虚弱等仅对攻击生效）
};

} // namespace tce