/**
 * Battle UI data provider interface (for drawing from snapshot or mock).
 */
#pragma once

namespace tce {

struct BattleStateSnapshot;

struct IBattleUIDataProvider {
    virtual const BattleStateSnapshot& get_snapshot() const = 0;
    virtual ~IBattleUIDataProvider() = default;
};

} // namespace tce
