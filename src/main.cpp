/**
 * 《溯源者：文明环境》课设 - 程序入口
 *
 * 正式主流程由 tce::GameFlowController 统一调度（地图 → 战斗 / 事件 / 商店 / 休息 / 宝藏），
 * 实现与细节见 src/GameFlow/GameFlowController.cpp、include/GameFlow/GameFlowController.hpp。
 *
 * 下方保留事件/商店/休息、纯地图 UI 的独立测试函数；调试时可暂时改 main 调用它们。
 */

#include <SFML/Graphics.hpp>
#include <iostream>
#include <optional>
#include <string>

#include "GameFlow/GameFlowController.hpp"

#include "DataLayer/DataLayer.hpp"
#include "EventEngine/EventEngine.hpp"
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUIData.hpp"
#include "MapEngine/MapEngine.hpp"
#include "MapEngine/MapUI.hpp"
#include "MapEngine/MapConfig.hpp"
#include "Common/NodeTypes.hpp"

#include "Treasure/TreasureChest.hpp"
#include "Treasure/TreasureUI.hpp"

static void runEventShopRestUITest(sf::RenderWindow& window);
static void runMapUITest(sf::RenderWindow& window);

// 将事件结果转为展示文案（用于结果页）
static std::string eventResultToSummary(const tce::EventEngine::EventResult& res) {
    if (res.type == "gold") return "获得了 " + std::to_string(res.value) + " 金币。";
    if (res.type == "heal") return "恢复了 " + std::to_string(res.value) + " 点生命。";
    if (res.type == "card_reward") return "获得了一张新卡牌（选牌由主流程处理）。";
    if (res.type == "none") return "无事发生。";
    return "事件结束。";
}

// ---------- 事件/商店/休息 UI 测试（真实事件数据：DataLayer + EventEngine）----------
static void runEventShopRestUITest(sf::RenderWindow& window) {
    using namespace tce;
    DataLayer::DataLayerImpl data;
    if (!data.load_events("")) {
        std::cerr << "[EventShopRest] events.json 加载失败，将使用占位事件。\n";
    }
    EventEngine engine(
        [&data](EventEngine::EventId id) { return data.get_event_by_id(id); },
        [](CardId) {},
        [](InstanceId) { return false; },
        [](InstanceId) { return false; }
    );
    constexpr const char* ROOT_EVENT_ID = "event_001";
    if (data.get_event_by_id(ROOT_EVENT_ID))
        engine.start_event(ROOT_EVENT_ID);
    else
        std::cerr << "[EventShopRest] 未找到根事件 \"" << ROOT_EVENT_ID << "\"，请确保 data/events.json 存在且含该 id。\n";

    EventShopRestUI ui(static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") && !ui.loadFont("assets/fonts/default.ttf"))
        ui.loadFont("data/font.ttf");
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf") &&
        !ui.loadChineseFont("assets/fonts/Sanji.ttf") &&
        !ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf") &&
        !ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc") &&
        !ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
    }

    ui.setScreen(EventShopRestScreen::Event);
    if (!engine.get_current_event())
        ui.setEventDataFromUtf8("（未加载事件）", "请确保 data/events.json 存在且含 event_001。", { "离开" }, "");
    bool showingResult = false;
    int screenIndex = 0;
    const EventEngine::Event* lastDisplayedEvent = nullptr;  // 仅当“当前事件”变化时刷新 UI，避免每帧 clear 选项矩形导致点击无效
    while (window.isOpen()) {
        const EventEngine::Event* current = engine.get_current_event();
        if (current && !showingResult && current != lastDisplayedEvent) {
            ui.setEventDataFromEvent(current);
            lastDisplayedEvent = current;
        }
        if (showingResult) lastDisplayedEvent = nullptr;
        if (screenIndex == 0 && !current && !showingResult) lastDisplayedEvent = nullptr;

        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { window.close(); return; }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) return;
                if (key->scancode == sf::Keyboard::Scancode::Num1 || key->scancode == sf::Keyboard::Scancode::Numpad1) {
                    screenIndex = 0;
                    ui.setScreen(EventShopRestScreen::Event);
                    showingResult = false;
                    if (data.get_event_by_id(ROOT_EVENT_ID)) engine.start_event(ROOT_EVENT_ID);
                }
                if (key->scancode == sf::Keyboard::Scancode::Num2 || key->scancode == sf::Keyboard::Scancode::Numpad2) {
                    screenIndex = 1;
                    ui.setScreen(EventShopRestScreen::Shop);
                    ShopDisplayData shop;
                    shop.playerGold = 99;
                    shop.forSale = { { "iron_wave", L"铁斩波", 50 }, { "cleave", L"顺劈斩", 60 } };
                    shop.deckForRemove = { { 1, L"打击" }, { 2, L"防御" } };
                    ui.setShopData(shop);
                }
                if (key->scancode == sf::Keyboard::Scancode::Num3 || key->scancode == sf::Keyboard::Scancode::Numpad3) {
                    screenIndex = 2;
                    ui.setScreen(EventShopRestScreen::Rest);
                    RestDisplayData rest;
                    rest.healAmount = 20;
                    rest.deckForUpgrade = { { 1, L"打击" }, { 2, L"铁斩波" } };
                    ui.setRestData(rest);
                }
            }
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            ui.handleEvent(*ev, mousePos);
        }
        sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        ui.setMousePosition(mousePos);

        int outIndex = -1;
        if (ui.pollEventOption(outIndex)) {
            if (showingResult) {
                if (outIndex == 0) showingResult = false;
            } else if (engine.get_current_event() && engine.choose_option(outIndex)) {
                EventEngine::EventResult res;
                if (engine.get_event_result(res)) {
                    engine.apply_event_result(res, [](int v) { std::cout << "[事件] 获得金币: " << v << "\n"; }, [](int v) { std::cout << "[事件] 恢复生命: " << v << "\n"; });
                    ui.setEventResultFromUtf8(eventResultToSummary(res));
                    showingResult = true;
                }
            }
        }
        CardId outCardId;
        if (ui.pollShopBuyCard(outCardId)) std::cout << "[EventShopRestUI] 购买卡牌: " << outCardId << std::endl;
        InstanceId outInstId;
        if (ui.pollShopRemoveCard(outInstId)) std::cout << "[EventShopRestUI] 删除牌实例: " << outInstId << std::endl;
        if (ui.pollRestHeal()) std::cout << "[EventShopRestUI] 选择休息回血" << std::endl;
        if (ui.pollRestUpgradeCard(outInstId)) std::cout << "[EventShopRestUI] 升级牌实例: " << outInstId << std::endl;

        window.clear(sf::Color(40, 38, 45));
        ui.draw(window);
        window.display();
    }
}

// ---------- 地图 UI 测试 ----------
static void runMapUITest(sf::RenderWindow& window) {
    MapEngine::MapConfigManager configManager;
    MapEngine::MapEngine engine;
    
    // 初始化宝箱系统
    tce::LootFactory lootFactory;
    tce::ChestManager chestManager;
    tce::TreasureUI treasureUI;
    
    engine.setContentIdGenerator([](NodeType type, int layer, int index) -> std::string {
        const char* t[] = { "enemy", "elite", "event", "rest", "merchant", "treasure", "boss" };
        int i = static_cast<int>(type);
        return std::string(i >= 0 && i < 7 ? t[i] : "node") + "_" + std::to_string(layer) + "_" + std::to_string(index);
    });
    engine.setNodeEnterCallback([&chestManager, &lootFactory, &treasureUI](const MapEngine::MapNode& node) {
        std::cout << "[MapUI] 进入节点: " << node.id << " 类型:" << static_cast<int>(node.type) << " content_id:" << node.content_id << std::endl;
        
        // 显示节点类型的文字描述
        std::string typeName;
        switch (node.type) {
            case NodeType::Enemy: typeName = "敌人"; break;
            case NodeType::Elite: typeName = "精英"; break;
            case NodeType::Event: typeName = "事件"; break;
            case NodeType::Rest: typeName = "休息"; break;
            case NodeType::Merchant: typeName = "商人"; break;
            case NodeType::Treasure: typeName = "宝藏"; break;
            case NodeType::Boss: typeName = "Boss"; break;
            default: typeName = "未知"; break;
        }
        std::cout << "节点类型: " << typeName << "，是否可达: " << (node.is_reachable ? "是" : "否") << std::endl;
        
        // 处理宝藏节点
        if (node.type == NodeType::Treasure) {
            std::cout << "[宝箱] 发现宝藏节点！" << std::endl;
            // 检查是否已有宝箱
            if (!chestManager.hasChest(node.id)) {
                // 创建一个随机宝箱
                chestManager.createRandomChest(node.id, lootFactory);
                std::cout << "[宝箱] 在节点 " << node.id << " 创建了一个随机宝箱！" << std::endl;
            }
            
            // 询问是否打开宝箱
            std::cout << "[宝箱] 你发现了一个宝箱，是否打开？(y/n): " << std::flush;
            char input;    std::cin >> input;
            if (input == 'y' || input == 'Y') {
                tce::Loot loot = chestManager.openChest(node.id, lootFactory);
                tce::TreasureChest* chest = chestManager.getChest(node.id);
                if (chest) {
                    treasureUI.show(chest, loot);
                }
            }
        }
    });

    if (!configManager.getCurrentConfig()) { std::cerr << "无地图配置" << std::endl; return; }
    engine.init_fixed_map(*configManager.getCurrentConfig());
    
    // 初始地图的宝藏节点将在进入时自动创建宝箱

    MapEngine::MapUI ui;
    if (!ui.initialize(&window)) { std::cerr << "地图 UI 初始化失败" << std::endl; return; }
    ui.loadLegendTexture("assets/images/menu.png");
    ui.setLegendPosition(1800.f, 900.f);
    ui.loadBackgroundTexture("assets/images/background.png");
    ui.setMap(&engine);
    ui.setCurrentLayer(0);
    
    // 初始化宝箱UI
    if (!treasureUI.initialize(&window)) {
        std::cerr << "宝箱 UI 初始化失败" << std::endl;
    }

    while (window.isOpen()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { window.close(); return; }
            
            // 处理宝箱UI事件
            if (treasureUI.isVisible()) {
                if (treasureUI.handleEvent(*ev)) {
                    continue; // 如果宝箱UI处理了事件，跳过其他处理
                }
            }
            
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) return;
                if (key->scancode == sf::Keyboard::Scancode::Left) {
                    configManager.prevMap();
                    engine.init_fixed_map(*configManager.getCurrentConfig());
                    
                    // 新地图的宝藏节点将在进入时自动创建宝箱
                }
                if (key->scancode == sf::Keyboard::Scancode::Right) {
                    configManager.nextMap();
                    engine.init_fixed_map(*configManager.getCurrentConfig());
                    
                    // 新地图的宝藏节点将在进入时自动创建宝箱
                }
            }
            if (const auto* mouse = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button == sf::Mouse::Button::Left) {
                    sf::Vector2i pos = mouse->position;
                    std::string nodeId = ui.handleClick(pos.x, pos.y);
                    if (!nodeId.empty()) {
                        MapEngine::MapNode node = engine.get_node_by_id(nodeId);
                        if (!engine.hasCurrentNode() && node.layer == 0) {
                            engine.set_current_node(nodeId);
                            engine.update_reachable_nodes();
                        } else if (node.is_reachable) {
                            engine.set_current_node(nodeId);
                            engine.update_reachable_nodes();
                        }
                    }
                }
            }
        }
        window.clear(sf::Color(255, 255, 255));
        ui.draw();
        
        // 绘制宝箱UI
        if (treasureUI.isVisible()) {
            treasureUI.draw();
        }
        
        window.display();
    }
}

int main() {
    const unsigned int winW = 1920, winH = 1080;
    sf::RenderWindow window(sf::VideoMode({ winW, winH }), "Tracer Civilization - 溯源者");
    window.setFramerateLimit(60);

    tce::GameFlowController game(window);
    if (!game.initialize()) {
        std::cerr << "[启动] GameFlowController::initialize 失败（请检查地图配置、资源路径等）。\n";
        return 1;
    }
    game.run();
    return 0;
}

// 调试：可将 main 内改为 runEventShopRestUITest(window) 或 runMapUITest(window)（单独测 UI）。
