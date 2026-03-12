/**
 * 卡牌效果注册：CardId → 效果函数在此注册到 CardSystem
 */
#pragma once

namespace tce {

class CardSystem;

/** 将所有卡牌效果（如 strike、defend、bash）注册到 card_system 的效果表 */
void register_all_card_effects(CardSystem& card_system);

} // namespace tce
