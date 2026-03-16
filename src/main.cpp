// src/main.cpp
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include "../include/MapEngine/MapEngine.hpp"
#include "../include/MapEngine/MapUI.hpp"
#include "../MapConfig.hpp"
#include <iostream>
#include <memory>
#include <string>

using MapEngine::MapNode;

int main() {
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();

    sf::RenderWindow window(desktop,
        "Tracer Civilization - 地图模块",
        sf::State::Fullscreen);
    window.setFramerateLimit(60);

    // 创建地图配置管理器
    MapEngine::MapConfigManager configManager;

    // 创建地图引擎
    MapEngine::MapEngine engine;

    // ==== 添加：设置内容ID生成器（示例）====
    engine.setContentIdGenerator([](NodeType type, int layer, int index) -> std::string {
        // 这里应该调用战斗/事件模块的接口
        // 目前返回示例ID
        std::string typeStr;
        switch (type) {
        case NodeType::Enemy: typeStr = "enemy"; break;
        case NodeType::Elite: typeStr = "elite"; break;
        case NodeType::Event: typeStr = "event"; break;
        case NodeType::Rest: typeStr = "rest"; break;
        case NodeType::Merchant: typeStr = "merchant"; break;
        case NodeType::Treasure: typeStr = "treasure"; break;
        case NodeType::Boss: typeStr = "boss"; break;
        }
        return typeStr + "_" + std::to_string(layer) + "_" + std::to_string(index);
        });
    // ==== 添加结束 ====

    // ==== 添加：设置节点进入回调 ====
    engine.setNodeEnterCallback([](const MapNode& node) {
        std::cout << "\n=== 玩家进入节点 ===" << std::endl;
        std::cout << "节点ID: " << node.id << std::endl;
        std::cout << "节点类型: " << static_cast<int>(node.type) << std::endl;
        std::cout << "内容ID: " << node.content_id << std::endl;
        std::cout << "所在层: " << node.layer << std::endl;
        std::cout << "=== 节点进入处理完成 ===\n" << std::endl;


        });
    // ==== 添加结束 ====

    // 初始化地图
    engine.init_fixed_map(*configManager.getCurrentConfig());

    // 创建UI
    MapEngine::MapUI ui;
    if (!ui.initialize(&window)) {
        std::cerr << "UI初始化失败" << std::endl;
        return -1;
    }

    // 先加载图例（确保图例正常工作）
    ui.loadLegendTexture("assets/images/menu.png");
    ui.setLegendPosition(1800.0f, 900.0f);  // 先用明显的位置
    ui.setLegendScale(1.5f);

    // 再加载背景
    ui.loadBackgroundTexture("assets/images/background.png");

    ui.setMap(&engine);
    ui.setCurrentLayer(0);

    // 加载字体
    sf::Font chineseFont;
    std::vector<std::string> fontPaths = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc"
        "C:/Windows/Fonts/arial.ttf"
    };

    for (const auto& path : fontPaths) {
        if (chineseFont.openFromFile(path)) {
            break;
        }
    }

    sf::Text infoText(chineseFont);
    infoText.setCharacterSize(20);
    infoText.setFillColor(sf::Color::Black);
    infoText.setPosition({ 20, 20 });

    // 主循环
    while (window.isOpen()) {
        while (const std::optional<sf::Event> event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
                if (keyPressed->scancode == sf::Keyboard::Scancode::Escape) {
                    window.close();
                }

                // 左右箭头切换地图
                if (keyPressed->scancode == sf::Keyboard::Scancode::Left) {
                    configManager.prevMap();
                    engine.init_fixed_map(*configManager.getCurrentConfig());
                    std::cout << "切换到: " << configManager.getCurrentMapName() << std::endl;
                }
                else if (keyPressed->scancode == sf::Keyboard::Scancode::Right) {
                    configManager.nextMap();
                    engine.init_fixed_map(*configManager.getCurrentConfig());
                    std::cout << "切换到: " << configManager.getCurrentMapName() << std::endl;
                }
            }

            if (const auto* mousePressed = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mousePressed->button == sf::Mouse::Button::Left) {
                    sf::Vector2i mousePos = mousePressed->position;

                    std::string clickedNodeId = ui.handleClick(mousePos.x, mousePos.y);

                    if (!clickedNodeId.empty()) {
                        MapNode clickedNode = engine.get_node_by_id(clickedNodeId);

                        // 如果还没有当前节点，只能选择第0层的节点作为起点
                        if (!engine.hasCurrentNode()) {
                            if (clickedNode.layer == 0) {
                                engine.set_current_node(clickedNodeId);
                                engine.update_reachable_nodes();
                                std::cout << "游戏开始！选择节点: " << clickedNodeId << std::endl;
                            }
                            else {
                                std::cout << "错误：只能选择最底层（第0层）的节点作为起点！" << std::endl;
                                // 可选：添加声音或视觉提示
                            }
                        }
                        // 如果已经有当前节点，只能选择可达节点
                        else if (clickedNode.is_reachable) {
                            engine.set_current_node(clickedNode.id);
                            engine.update_reachable_nodes();
                        }
                    }
                }
            }
        }



        window.clear(sf::Color(255, 255, 255));

        ui.draw();
        window.draw(infoText);
        window.display();
    }

    return 0;
}


