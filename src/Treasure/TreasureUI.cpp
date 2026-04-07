#include "Treasure/TreasureUI.hpp"
#include "Treasure/TreasureChest.hpp"
#include <iostream>
#include <filesystem>

namespace tce {

TreasureUI::TreasureUI() 
    : window_(nullptr), currentChest_(nullptr), isVisible_(false) {
    // 由于SFML 3.0.2的Text构造函数需要非临时Font引用，
    // 我们在initialize方法中通过setFont设置字体
}

bool TreasureUI::initialize(sf::RenderWindow* window) {
    window_ = window;
    
    // 加载字体 - 使用项目中现有的字体文件
    if (!font_.openFromFile(std::filesystem::path("assets/fonts/Sanji.ttf"))) {
        std::cerr << "[TreasureUI] 无法加载默认字体 Sanji.ttf" << std::endl;
    }
    
    if (!chineseFont_.openFromFile(std::filesystem::path("assets/fonts/simkai.ttf"))) {
        std::cerr << "[TreasureUI] 无法加载中文字体 simkai.ttf" << std::endl;
        // 回退到默认字体
        chineseFont_ = font_;
    }
    
    // 初始化背景
    background_.setSize(sf::Vector2f(static_cast<float>(window_->getSize().x), static_cast<float>(window_->getSize().y)));
    background_.setFillColor(sf::Color(0, 0, 0, 180));
    
    // 初始化宝箱面板
    chestPanel_.setSize(sf::Vector2f(600, 500));
    chestPanel_.setPosition({static_cast<float>(window_->getSize().x) / 2 - 300, static_cast<float>(window_->getSize().y) / 2 - 250});
    chestPanel_.setFillColor(sf::Color(50, 45, 55));
    chestPanel_.setOutlineColor(sf::Color(100, 90, 110));
    chestPanel_.setOutlineThickness(2);
    
    // 初始化标题文本
    titleText_ = sf::Text(chineseFont_, "宝箱", 30);
    titleText_->setFillColor(sf::Color::White);
    titleText_->setPosition({chestPanel_.getPosition().x + 20, chestPanel_.getPosition().y + 20});
    
    // 初始化宝箱类型文本
    chestTypeText_ = sf::Text(chineseFont_, "", 24);
    chestTypeText_->setFillColor(sf::Color(200, 200, 200));
    chestTypeText_->setPosition({chestPanel_.getPosition().x + 20, chestPanel_.getPosition().y + 60});
    
    // 初始化遗物名称文本
    relicNameText_ = sf::Text(chineseFont_, "", 28);
    relicNameText_->setFillColor(sf::Color::Yellow);
    relicNameText_->setPosition({chestPanel_.getPosition().x + 20, chestPanel_.getPosition().y + 100});
    
    // 初始化遗物描述文本
    relicDescriptionText_ = sf::Text(chineseFont_, "", 16);
    relicDescriptionText_->setFillColor(sf::Color::White);
    relicDescriptionText_->setPosition({chestPanel_.getPosition().x + 20, chestPanel_.getPosition().y + 140});
    
    // 初始化金币文本
    goldText_ = sf::Text(chineseFont_, "", 20);
    goldText_->setFillColor(sf::Color::Yellow);
    goldText_->setPosition({chestPanel_.getPosition().x + 20, chestPanel_.getPosition().y + 220});
    
    // 初始化关闭按钮
    closeButton_.setSize(sf::Vector2f(120, 40));
    closeButton_.setPosition({chestPanel_.getPosition().x + chestPanel_.getSize().x - 140, chestPanel_.getPosition().y + chestPanel_.getSize().y - 60});
    closeButton_.setFillColor(sf::Color(80, 70, 90));
    closeButton_.setOutlineColor(sf::Color(120, 110, 130));
    closeButton_.setOutlineThickness(1);
    
    // 初始化关闭按钮文本
    closeButtonText_ = sf::Text(chineseFont_, "关闭", 18);
    closeButtonText_->setFillColor(sf::Color::White);
    closeButtonText_->setPosition({closeButton_.getPosition().x + 30, closeButton_.getPosition().y + 8});
    
    return true;
}

void TreasureUI::show(TreasureChest* chest, const Loot& loot) {
    currentChest_ = chest;
    isVisible_ = true;
    
    // 更新文本
    if (chestTypeText_) {
        chestTypeText_->setString("宝箱类型: " + chest->getChestTypeName());
    }
    
    if (relicNameText_) {
        relicNameText_->setString("获得遗物: " + loot.relic.name);
    }
    
    if (relicDescriptionText_) {
        relicDescriptionText_->setString("描述: " + loot.relic.description);
    }
    
    if (goldText_) {
        if (loot.hasGold) {
            goldText_->setString("获得金币: " + std::to_string(loot.gold));
        } else {
            goldText_->setString("获得金币: 0");
        }
    }
}

void TreasureUI::hide() {
    isVisible_ = false;
    currentChest_ = nullptr;
}

bool TreasureUI::isVisible() const {
    return isVisible_;
}

bool TreasureUI::handleEvent(const sf::Event& event) {
    if (!isVisible_) {
        return false;
    }
    
    if (event.is<sf::Event::MouseButtonPressed>()) {
        const auto* mouseEvent = event.getIf<sf::Event::MouseButtonPressed>();
        if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Left) {
            sf::Vector2i mousePos = sf::Mouse::getPosition(*window_);
            sf::Vector2f mousePosF(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y));
            
            // 检查是否点击了关闭按钮
            if (closeButton_.getGlobalBounds().contains(mousePosF)) {
                hide();
                return true;
            }
            
            // 检查是否点击了背景
            if (!chestPanel_.getGlobalBounds().contains(mousePosF)) {
                hide();
                return true;
            }
        }
    }
    
    if (event.is<sf::Event::KeyPressed>()) {
        const auto* keyEvent = event.getIf<sf::Event::KeyPressed>();
        if (keyEvent && keyEvent->scancode == sf::Keyboard::Scancode::Escape) {
            hide();
            return true;
        }
    }
    
    return false;
}

void TreasureUI::draw() {
    if (!isVisible_ || !window_) {
        return;
    }
    
    window_->draw(background_);
    window_->draw(chestPanel_);
    
    if (titleText_) {
        window_->draw(*titleText_);
    }
    
    if (chestTypeText_) {
        window_->draw(*chestTypeText_);
    }
    
    if (relicNameText_) {
        window_->draw(*relicNameText_);
    }
    
    if (relicDescriptionText_) {
        window_->draw(*relicDescriptionText_);
    }
    
    if (goldText_) {
        window_->draw(*goldText_);
    }
    
    window_->draw(closeButton_);
    
    if (closeButtonText_) {
        window_->draw(*closeButtonText_);
    }
}

bool TreasureUI::loadFont(const std::string& fontPath) {
    return font_.openFromFile(std::filesystem::path(fontPath));
}

bool TreasureUI::loadChineseFont(const std::string& fontPath) {
    return chineseFont_.openFromFile(std::filesystem::path(fontPath));
}

} // namespace tce