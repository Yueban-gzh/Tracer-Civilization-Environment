/**
 * 牌组界面：将 CardSystem 各牌堆收集成展示列表
 */

#include "CardSystem/DeckViewCollection.hpp"

namespace tce {

// 收集牌组界面要展示的卡牌列表：用于把不同来源（牌组/抽牌堆/弃牌堆/消耗堆）统一成一份列表交给 UI 绘制
std::vector<CardInstance> collect_deck_view_cards(const CardSystem& card_system, DeckViewMode mode) {
    std::vector<CardInstance> cards;

    switch (mode) {
    case DeckViewMode::Deck:
        // 右上角“牌组”：展示玩家真实牌组（master deck），不受战斗中临时生成牌影响
        for (const auto& c : card_system.get_master_deck()) cards.push_back(c);
        break;
    case DeckViewMode::DrawPile:
        // 左下角抽牌堆：展示当前抽牌堆内容（可能包含战斗中生成的临时牌）
        for (const auto& c : card_system.get_draw_pile()) cards.push_back(c);
        break;
    case DeckViewMode::DiscardPile:
        // 右下角弃牌堆：展示当前弃牌堆内容（可能包含战斗中生成的临时牌）
        for (const auto& c : card_system.get_discard_pile()) cards.push_back(c);
        break;
    case DeckViewMode::ExhaustPile:
        // 消耗堆：展示当前消耗堆内容（本场战斗内移出循环，仅用于展示/统计）
        for (const auto& c : card_system.get_exhaust_pile()) cards.push_back(c);
        break;
    default:
        break;
    }

    return cards;
}

// 各模式为空时的提示文案（UI 用：空就不打开界面，只提示）
std::wstring deck_view_empty_tip(DeckViewMode mode) {
    switch (mode) {
    case DeckViewMode::Deck: return L"牌组为空";
    case DeckViewMode::DrawPile: return L"抽牌堆为空";
    case DeckViewMode::DiscardPile: return L"弃牌堆为空";
    case DeckViewMode::ExhaustPile: return L"消耗堆为空";
    default: return L"牌堆为空";
    }
}

} // namespace tce

