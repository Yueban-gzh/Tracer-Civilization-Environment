/**
 * C - 牌组界面：收集要展示的卡牌列表（按查看模式：整个牌组/抽牌堆/弃牌堆）
 */
#pragma once

#include "CardSystem.hpp"
#include <string>
#include <vector>

namespace tce {

enum class DeckViewMode : int {
    Deck       = 1, // 玩家真实牌组（不包含战斗中临时生成牌）
    DrawPile   = 2,
    DiscardPile = 3,
    ExhaustPile = 4,
};

/** 根据牌组界面模式，从 CardSystem 收集要展示的卡牌列表。 */
std::vector<CardInstance> collect_deck_view_cards(const CardSystem& card_system, DeckViewMode mode);

/** 对应模式为空时的提示文案。 */
std::wstring deck_view_empty_tip(DeckViewMode mode);

} // namespace tce

