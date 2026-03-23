#pragma once

#include <memory>
#include <vector>
#include "Types.hpp"
#include "BattleModifier.hpp"

namespace tce {

/** 缓冲 modifier：须在 build_modifiers 中最先注册（MOD_PRIORITY_BUFFER_PRE），保证先于遗物与其它状态扣血判定 */
std::shared_ptr<IBattleModifier> create_buffer_modifier();

/** 渎神惩罚：须在玩家其它回合开始类 modifier 之后注册（MOD_PRIORITY_PLAYER_ST+1） */
std::shared_ptr<IBattleModifier> create_blasphemy_modifier();

std::vector<std::shared_ptr<IBattleModifier>>
create_player_status_modifiers(const PlayerBattleState& player);

std::vector<std::shared_ptr<IBattleModifier>>
create_monster_status_modifiers(const std::vector<MonsterInBattle>& monsters);

} // namespace tce
