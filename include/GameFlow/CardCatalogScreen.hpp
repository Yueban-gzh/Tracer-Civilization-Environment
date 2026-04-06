#pragma once

#include <SFML/Graphics.hpp>

namespace tce {

/** 开始界面“卡牌总览”：展示 DataLayer 中定义的全部卡牌（复用 BattleUI 牌组查看界面）。 */
void runCardCatalogScreen(sf::RenderWindow& window);

} // namespace tce

