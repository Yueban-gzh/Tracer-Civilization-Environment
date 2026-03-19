/**
 * 金手指面板 - 独立 UI，输入框直接输入命令并执行
 * 与主游戏逻辑完全分离，仅通过 CheatEngine 与引擎交互
 */
#pragma once

#include <SFML/Graphics.hpp>
#include <string>
#include <vector>

namespace tce {

class CheatEngine;

class CheatPanel {
public:
    CheatPanel(CheatEngine* cheat, unsigned windowWidth, unsigned windowHeight);
    /** 尝试加载字体，返回是否成功 */
    bool loadFont(const std::string& path);
    /** 处理事件，返回 true 表示已消费（主循环不再传递给其他 UI） */
    bool handleEvent(const sf::Event& ev);
    /** 绘制面板（仅当可见时有效） */
    void draw(sf::RenderWindow& window);
    /** 是否可见 */
    bool isVisible() const { return visible_; }
    /** F2 切换显示/隐藏 */
    void toggle() {
        visible_ = !visible_;
        if (visible_) cursorBlinkClock_.restart();
    }

private:
    CheatEngine* cheat_ = nullptr;
    unsigned width_ = 0;
    unsigned height_ = 0;
    bool visible_ = false;
    std::string inputText_;
    std::string resultText_;
    sf::Font font_;
    bool fontLoaded_ = false;
    sf::Clock cursorBlinkClock_;
    /** Tab 循环补全：上次匹配列表与当前选中的命令/参数，用于连续 Tab 切换 */
    std::vector<std::string> lastTabMatches_;
    std::string lastTabCurrent_;
    size_t lastTabIndex_ = 0;
    /** 本次补全替换的起始位置（0=补全命令首词，>0=补全参数即最后一词） */
    size_t lastReplaceStart_ = 0;

    void executeCurrent();
    /** Tab 补全：根据当前输入的第一个词补全命令 */
    void completeInput();
};

} // namespace tce
