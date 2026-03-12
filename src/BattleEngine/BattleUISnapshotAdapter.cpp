/**
 * SnapshotBattleUIDataProvider implementation.
 */
#include "BattleEngine/BattleUISnapshotAdapter.hpp"

namespace tce {

const BattleStateSnapshot& SnapshotBattleUIDataProvider::get_snapshot() const {
    return *snapshot_;
}

} // namespace tce
