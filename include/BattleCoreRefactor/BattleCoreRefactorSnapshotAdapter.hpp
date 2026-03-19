/**
 * BattleCoreRefactor → BattleStateSnapshot 适配器
 */
#pragma once

#include "BattleState.hpp"
#include "../BattleEngine/BattleStateSnapshot.hpp"
#include "../CardSystem/CardSystem.hpp"

namespace tce {

BattleStateSnapshot make_snapshot_from_core_refactor(
    const BattleState& state,
    const CardSystem* card_system);

} // namespace tce
