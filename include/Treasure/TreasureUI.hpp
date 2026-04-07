#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <optional>
#include "Treasure/TreasureChest.hpp" // 确保 Loot 可见

namespace tce {

class TreasureUI {
public:
    TreasureUI();
    bool initialize(sf::RenderWindow* window);
    void show(TreasureChest* chest, const Loot& loot);
    void hide();
    bool isVisible() const;
    bool handleEvent(const sf::Event& event);
    void draw();

private:
    sf::RenderWindow* window_;
    TreasureChest* currentChest_;
    bool isVisible_;

    // 资源
    sf::Font font_;
    sf::Font chineseFont_;

    // UI 元素
    sf::RectangleShape background_;
    sf::RectangleShape chestPanel_;
    sf::RectangleShape closeButton_;

    // 使用 std::optional 来延迟初始化 Text 对象
    std::optional<sf::Text> titleText_;
    std::optional<sf::Text> chestTypeText_;
    std::optional<sf::Text> relicNameText_;
    std::optional<sf::Text> relicDescriptionText_;
    std::optional<sf::Text> goldText_;
    std::optional<sf::Text> closeButtonText_;

    // 字体加载方法
    bool loadFont(const std::string& fontPath);
    bool loadChineseFont(const std::string& fontPath);
};

} // namespace tce