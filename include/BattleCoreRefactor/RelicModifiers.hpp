#pragma once

#include <memory>
#include <vector>
#include "Types.hpp"
#include "BattleModifier.hpp"

namespace tce {

std::vector<std::shared_ptr<IBattleModifier>>
create_relic_modifiers(const std::vector<RelicId>& relics);

} // namespace tce
