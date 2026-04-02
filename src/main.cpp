/**
 * 正式入口流程：
 * 1) 创建窗口（按桌面尺寸自适应 1920x1080 基准）
 * 2) 初始化 GameFlowController（加载数据、系统依赖、UI 资源）
 * 3) 开始界面（新游戏/继续） <-> 主流程循环（地图/战斗/事件/商店/休息/宝箱）
 */
#include <SFML/Graphics.hpp>
#include <iostream>

#include "GameFlow/GameFlowController.hpp"

int main() {
    try {
        constexpr unsigned kDesignW = 1920u;
        constexpr unsigned kDesignH = 1080u;
        const sf::VideoMode desktop = sf::VideoMode::getDesktopMode();

        unsigned winW = kDesignW;
        unsigned winH = kDesignH;
        if (desktop.size.x > 0u && static_cast<unsigned>(desktop.size.x) < winW)
            winW = static_cast<unsigned>(desktop.size.x);
        if (desktop.size.y > 0u && static_cast<unsigned>(desktop.size.y) < winH)
            winH = static_cast<unsigned>(desktop.size.y);

        sf::ContextSettings ctx;
        ctx.antiAliasingLevel = 2u;

        sf::RenderWindow window(
            sf::VideoMode({ winW, winH }),
            "Tracer Civilization",
            sf::Style::Close | sf::Style::Titlebar,
            sf::State::Windowed,
            ctx);
        window.setFramerateLimit(60);

        tce::GameFlowController game(window);
        if (!game.initialize()) {
            std::cerr << "[Main] initialize failed\n";
            return 1;
        }

        while (window.isOpen()) {
            tce::runStartScreen(window, game);
            if (!window.isOpen()) break;
            game.run();
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[Main] exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "[Main] unknown exception\n";
        return 1;
    }
}
 