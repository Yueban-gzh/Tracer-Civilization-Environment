/**
 * 增减益效果注册：回合末 tick 等行为在此注册到 BattleEngine
 */
#pragma once

namespace tce {

class BattleEngine;

/** 将所有状态效果（如中毒 tick）注册到 engine 的 status_tick 表 */
void register_all_status_effects(BattleEngine& engine);

} // namespace tce
