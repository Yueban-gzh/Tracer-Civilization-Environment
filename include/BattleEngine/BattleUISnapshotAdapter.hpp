/**
 * Adapts BattleStateSnapshot for BattleUI (IBattleUIDataProvider).
 */
#pragma once

#include "BattleEngine.hpp"
#include "BattleUIData.hpp"

namespace tce {

class SnapshotBattleUIDataProvider : public IBattleUIDataProvider {
public:
    explicit SnapshotBattleUIDataProvider(const BattleStateSnapshot* snapshot) : snapshot_(snapshot) {}
    const BattleStateSnapshot& get_snapshot() const override;

private:
    const BattleStateSnapshot* snapshot_;
};

} // namespace tce
