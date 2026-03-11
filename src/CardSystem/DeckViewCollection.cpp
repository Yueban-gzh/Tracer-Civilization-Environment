/**
 * 牌组界面：将 CardSystem 各牌堆收集成展示列表
 */

#include "CardSystem/DeckViewCollection.hpp"

namespace tce {

std::vector<CardInstance> collect_deck_view_cards(const CardSystem& card_system, DeckViewMode mode) {
    std::vector<CardInstance> cards;

    switch (mode) {
    case DeckViewMode::FullDeck:
        for (const auto& c : card_system.get_hand()) cards.push_back(c);
        for (const auto& c : card_system.get_draw_pile()) cards.push_back(c);
        for (const auto& c : card_system.get_discard_pile()) cards.push_back(c);
        for (const auto& c : card_system.get_exhaust_pile()) cards.push_back(c);
        break;
    case DeckViewMode::DrawPile:
        for (const auto& c : card_system.get_draw_pile()) cards.push_back(c);
        break;
    case DeckViewMode::DiscardPile:
        for (const auto& c : card_system.get_discard_pile()) cards.push_back(c);
        break;
    default:
        break;
    }

    return cards;
}

std::wstring deck_view_empty_tip(DeckViewMode mode) {
    switch (mode) {
    case DeckViewMode::FullDeck: return L"牌组为空";
    case DeckViewMode::DrawPile: return L"抽牌堆为空";
    case DeckViewMode::DiscardPile: return L"弃牌堆为空";
    default: return L"牌堆为空";
    }
}

} // namespace tce

