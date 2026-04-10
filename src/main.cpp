/**
 * 正式入口流程：
 * 1) 创建窗口：桌面分辨率恰为 1920×1080 时用全屏；否则用 1920×1080 窗口（小于桌面则缩小以免出屏）
 * 2) 初始化 GameFlowController（加载数据、系统依赖、UI 资源）
 * 3) 开始界面（新游戏/继续） <-> 主流程循环（地图/战斗/事件/商店/休息/宝箱）
 */
#include <SFML/Graphics.hpp>
#include <iostream>

#include "Common/ImagePath.hpp"
#include "Common/UserSettings.hpp"
#include "GameFlow/GameFlowController.hpp"

int main() {
    try {
        tce::setup_asset_working_directory();
        tce::UserSettings::instance().load();

        sf::ContextSettings ctx;
        ctx.antiAliasingLevel = 2u;

        sf::RenderWindow window;
        tce::UserSettings::instance().applyVideoModeToWindow(window, ctx);

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
