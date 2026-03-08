/**
 * 增减益效果实现与注册（回合末 tick 等）
 * 中毒：只对敌人生效，在 B 的 end_turn 中于敌人回合开始时按层数扣血，不在此注册
 */
#include "../../include/Effects/StatusEffects.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

void register_all_status_effects(BattleEngine& engine) {
    (void)engine;
    // 中毒在 BattleEngine::end_turn ②.0 中单独处理：仅怪物、敌人回合开始时扣血
    // 后续在此追加其他需回合末 tick 的状态：engine.register_status_tick("xxx", ...);
}

} // namespace tce
