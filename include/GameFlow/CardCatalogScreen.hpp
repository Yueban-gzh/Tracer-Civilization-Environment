#pragma once

#include <SFML/Graphics.hpp>

namespace tce {

/** 开始界面“卡牌总览”：展示 DataLayer 中的卡牌（复用 BattleUI 牌组查看）；可按红/绿/无色/诅咒筛选，默认仅红色。 */
void runCardCatalogScreen(sf::RenderWindow& window);

} // namespace tce

