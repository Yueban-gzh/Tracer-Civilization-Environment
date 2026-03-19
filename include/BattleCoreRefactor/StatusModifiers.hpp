#pragma once

#include <memory>
#include <vector>
#include "Types.hpp"
#include "BattleModifier.hpp"

namespace tce {

std::vector<std::shared_ptr<IBattleModifier>>
create_player_status_modifiers(const PlayerBattleState& player);

std::vector<std::shared_ptr<IBattleModifier>>
create_monster_status_modifiers(const std::vector<MonsterInBattle>& monsters);

} // namespace tce
