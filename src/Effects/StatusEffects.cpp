/**
 * 增减益效果：已迁移至 StatusModifiers.cpp
 *
 * 原 metallicize、multi_armor、ritual 等回合末 tick 逻辑
 * 现由 BattleCoreRefactor/StatusModifiers.cpp 中的
 * MetallicizeModifier、MultiArmorModifier、RitualModifier 实现。
 *
 * 本文件保留空实现以兼容可能的旧调用；当前 BattleEngine 不调用此函数。
 */
#include "../../include/Effects/StatusEffects.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

void register_all_status_effects(BattleEngine& /*engine*/) {
    // 逻辑已迁移至 StatusModifiers.cpp
}

} // namespace tce
