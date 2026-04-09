#pragma once

#include <SFML/Graphics.hpp>
#include <string>

namespace tce {

struct BattleStateSnapshot;
struct CardInstance;

/** 与战斗界面手牌/奖励/选牌相同的完整卡面（圆角框、丝带、立绘、描述、费用球）。 */
void draw_detailed_card_at(sf::RenderWindow& window, const sf::Font& fontZh, const std::string& card_id, float cardX,
    float cardY, float w, float h, const sf::Color& outlineColor, float outlineThickness = 8.f,
    const sf::RenderStates& states = sf::RenderStates::Default, const BattleStateSnapshot* handSnap = nullptr,
    const CardInstance* handInst = nullptr);

} // namespace tce
