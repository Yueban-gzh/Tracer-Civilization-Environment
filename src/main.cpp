/**
 * 《溯源者：文明环境》课设 - 程序入口
 *
 * 正式主流程由 tce::GameFlowController 统一调度（地图 → 战斗 / 事件 / 商店 / 休息 / 宝藏），
 * 实现与细节见 src/GameFlow/GameFlowController.cpp、include/GameFlow/GameFlowController.hpp。
 *
 * 下方保留事件/商店/休息、纯地图 UI 的独立测试函数；调试时可暂时改 main 调用它们。
 */

#include <SFML/Graphics.hpp>
#include <algorithm>
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

static void runEventShopRestUITest(sf::RenderWindow& window);
static void runMapUITest(sf::RenderWindow& window);

// 将事件结果转为展示文案（用于结果页）
static std::string eventResultToSummary(const tce::EventEngine::EventResult& res) {
    auto effectToSummary = [](const DataLayer::EventEffect& eff) {
        const int v = eff.value;
        if (eff.type == "gold") {
            if (v >= 0) return std::string("获得了 ") + std::to_string(v) + " 金币。";
            return std::string("失去了 ") + std::to_string(-v) + " 金币。";
        }
        if (eff.type == "heal") {
            if (v >= 0) return std::string("恢复了 ") + std::to_string(v) + " 点生命。";
            return std::string("失去了 ") + std::to_string(-v) + " 点生命。";
        }
        if (eff.type == "max_hp") {
            if (v >= 0) return std::string("最大生命值提升了 ") + std::to_string(v) + "。";
            return std::string("最大生命值降低了 ") + std::to_string(-v) + "。";
        }
        if (eff.type == "card_reward") return std::string("获得了 ") + std::to_string(v) + " 张新卡牌。";
        if (eff.type == "card_reward_choose") return std::string("可选获得 ") + std::to_string(std::max(1, v)) + " 张卡牌。";
        if (eff.type == "remove_card") return std::string("移除了牌组中的 ") + std::to_string(v) + " 张卡牌。";
        if (eff.type == "remove_card_choose") return std::string("自选移除牌组中的 ") + std::to_string(std::max(1, v)) + " 张卡牌。";
        if (eff.type == "remove_curse") return std::string("移除了 ") + std::to_string(v) + " 张诅咒（寄生）牌。";
        if (eff.type == "add_curse") return std::string("获得了 ") + std::to_string(v) + " 张诅咒（寄生）牌。";
        if (eff.type == "upgrade_random") return std::string("升级了 ") + std::to_string(v) + " 张卡牌。";
        if (eff.type == "relic") return std::string("获得了 ") + std::to_string(v) + " 个遗物。";
        if (eff.type == "none") return std::string("无事发生。");
        return std::string("事件结算：") + eff.type;
    };

    if (!res.effects.empty()) {
        std::string out;
        for (size_t i = 0; i < res.effects.size(); ++i) {
            if (i) out += " ";
            out += effectToSummary(res.effects[i]);
        }
        return out.empty() ? std::string("无事发生。") : out;
    }

    DataLayer::EventEffect eff{ res.type, res.value };
    return effectToSummary(eff);
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
                    shop.playerCurrentHp = 72;
                    shop.playerMaxHp = 80;
                    shop.potionSlotsMax = 3;
                    shop.potionSlotsUsed = 1;
                    shop.playerTitle = L"稷下学子";
                    shop.chapterLine = L"先秦溯源 · 第 6 层";
                    shop.removeServicePrice = 75;
                    shop.forSale = {
                        { "iron_wave", L"铁斩波", 50 }, { "cleave", L"顺劈斩", 60 }, { "shrug_it_off", L"耸肩无视", 45 },
                        { "quick_slash", L"快斩", 55 }, { "strike", L"打击", 40 },
                    };
                    shop.colorlessForSale = {
                        { "card_001", L"与子同袍", 85 }, { "card_007", L"雨雪霏霏", 55 },
                    };
                    shop.relicsForSale = {
                        { "vajra", L"金刚杵", 180 }, { "anchor", L"船锚", 160 }, { "strawberry", L"草莓", 120 },
                    };
                    shop.potionsForSale = {
                        { "block_potion", L"砌墙灵液", 40 }, { "strength_potion", L"蛮力灵液", 45 },
                        { "poison_potion", L"淬毒灵液", 50 },
                    };
                    shop.deckForRemove = { { 1, "strike", L"打击" }, { 2, "defend", L"防御" } };
                    ui.setShopData(shop);
                }
                if (key->scancode == sf::Keyboard::Scancode::Num3 || key->scancode == sf::Keyboard::Scancode::Numpad3) {
                    screenIndex = 2;
                    ui.setScreen(EventShopRestScreen::Rest);
                    RestDisplayData rest;
                    rest.healAmount = 20;
                    rest.deckForUpgrade = { { 1, "strike", L"打击" }, { 2, "cleave", L"铁斩波" } };
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
        if (ui.pollShopPayRemoveService()) std::cout << "[EventShopRestUI] 净简付费" << std::endl;
        int ri = -1, pi = -1;
        if (ui.pollShopBuyRelic(ri)) std::cout << "[EventShopRestUI] 购买遗物: " << ri << std::endl;
        if (ui.pollShopBuyPotion(pi)) std::cout << "[EventShopRestUI] 购买灵液: " << pi << std::endl;
        if (ui.pollShopLeave()) std::cout << "[EventShopRestUI] 离开商店" << std::endl;
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
    engine.setContentIdGenerator([](NodeType type, int layer, int index) -> std::string {
        const char* t[] = { "enemy", "elite", "event", "rest", "merchant", "treasure", "boss" };
        int i = static_cast<int>(type);
        return std::string(i >= 0 && i < 7 ? t[i] : "node") + "_" + std::to_string(layer) + "_" + std::to_string(index);
    });
    engine.setNodeEnterCallback([](const MapEngine::MapNode& node) {
        std::cout << "[MapUI] 进入节点: " << node.id << " 类型:" << static_cast<int>(node.type) << " content_id:" << node.content_id << std::endl;
    });

    if (!configManager.getCurrentConfig()) { std::cerr << "无地图配置" << std::endl; return; }
    engine.init_fixed_map(*configManager.getCurrentConfig());

    MapEngine::MapUI ui;
    if (!ui.initialize(&window)) { std::cerr << "地图 UI 初始化失败" << std::endl; return; }
    ui.loadLegendTexture("assets/images/menu.png");
    ui.setLegendPosition(1800.f, 900.f);
    ui.loadBackgroundTexture("assets/images/background.png");
    ui.setMap(&engine);
    ui.setCurrentLayer(0);

    while (window.isOpen()) {
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) { window.close(); return; }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) return;
                if (key->scancode == sf::Keyboard::Scancode::Left) {
                    configManager.prevMap();
                    engine.init_fixed_map(*configManager.getCurrentConfig());
                }
                if (key->scancode == sf::Keyboard::Scancode::Right) {
                    configManager.nextMap();
                    engine.init_fixed_map(*configManager.getCurrentConfig());
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
