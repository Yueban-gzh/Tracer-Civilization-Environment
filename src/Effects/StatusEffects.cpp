/**
 * 增减益效果实现与注册（回合末 tick 等）
 * 中毒：只对敌人生效，在 B 的 end_turn 中于敌人回合开始时按层数扣血，不在此注册
 */
#include "../../include/Effects/StatusEffects.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"

namespace tce {

void register_all_status_effects(BattleEngine& engine) {
    // 中毒在 BattleEngine::end_turn ②.0 中单独处理：仅怪物、敌人回合开始时扣血
    // 在此注册需要在回合末 tick 的增减益

    // 金属化：在你的回合结束时，获得 N 点格挡
    engine.register_status_tick("metallicize",
        [](int stacks, bool target_is_player, int /*monster_index*/, EffectContext& ctx) {
            if (!target_is_player) return;
            if (stacks <= 0) return;
            ctx.add_block_to_player(stacks);
        });
}

} // namespace tce
