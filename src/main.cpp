/**
 * 《溯源者：文明环境》课设 - 程序入口
 * 模块：MapEngine(A)、BattleEngine(B)、CardSystem(C)、EventEngine(D)、DataLayer(E)
 *
 * 主菜单：1/F1=战斗  2/F2=事件/商店/休息 UI  3/F3=地图  Esc=退出
 * 用于测试所有 UI 与重要功能是否正常运行。
 */

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <random>
#include <optional>
#include <string>

#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#include "BattleEngine/BattleEngine.hpp"
#include "BattleEngine/BattleStateSnapshot.hpp"
#include "BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.hpp"
#include "BattleEngine/MonsterBehaviors.hpp"
#include "CardSystem/CardSystem.hpp"
#include "CardSystem/DeckViewCollection.hpp"
#include "Cheat/CheatEngine.hpp"
#include "Cheat/CheatPanel.hpp"
#include "DataLayer/DataLayer.hpp"
#include "DataLayer/DataLayer.h"
#include "Effects/CardEffects.hpp"
#include "EventEngine/EventEngine.hpp"
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUIData.hpp"
#include "MapEngine/MapEngine.hpp"
#include "MapEngine/MapUI.hpp"
#include "MapEngine/MapConfig.hpp"
#include "Common/NodeTypes.hpp"

static void runBattleUI(sf::RenderWindow& window);
static void runEventShopRestUITest(sf::RenderWindow& window);
static void runMapUITest(sf::RenderWindow& window);
 
 static void runBattleUI(sf::RenderWindow& window) {       // 运行战斗 UI 主循环
     using namespace tce;                                   // 使用 tce 命名空间
 
    // 加载卡牌/怪物 JSON 到 tce::s_cards / tce::s_monsters（get_card_by_id/get_monster_by_id 会查这两张表）
    // 默认路径：data/cards.json、data/monsters.json（会尝试 data/、./data/、../data/、../../data/）
    DataLayer::DataLayerImpl data;                         // 数据层实现
    if (!data.load_cards("")) {                            // 加载卡牌
        std::cerr << "[启动] 警告：cards.json 加载失败，UI 将显示卡牌 ID 而非名称。请确保 data/cards.json 存在。\n";
    }
    if (!data.load_monsters("")) {                         // 加载怪物
        std::cerr << "[启动] 警告：monsters.json 加载失败。\n";
    }
 
     CardSystem card_system([](CardId id) { return get_card_by_id(id); });  // 卡牌系统，注入按 id 查卡回调
     BattleEngine engine(                                   // 战斗引擎
         card_system,                                       // 卡牌系统引用
         [](MonsterId id) { return get_monster_by_id(id); }, // 按 id 查怪物
         [](CardId id) { return get_card_by_id(id); },      // 按 id 查卡牌
         execute_monster_behavior                           // 怪物行为执行函数
     );
 
     register_all_card_effects(card_system);                 // 注册所有卡牌效果（打击、防御等）
 
     PlayerBattleState player;                              // 玩家战斗初始状态
     player.playerName = "Telys";                            // 玩家名
     player.character  = "Ironclad";                        // 职业
     player.currentHp  = 80;                                // 当前生命
     player.maxHp      = 80;                                // 最大生命
     player.energy     = 3;                                 // 当前能量
     player.maxEnergy  = 3;                                 // 最大能量
     player.gold       = 99;                                // 金币
     player.cardsToDrawPerTurn = 5;                         // 每回合抽牌数
 
    // 初始牌组：包含已实现效果、便于测试的若干卡牌（每种一张基础版 + 一张升级版）
    card_system.init_master_deck({
      
        "neutralize", "neutralize+",
            // 绿色：防御
            "defend_green", "defend_green+",
        // 绿色/蓝色：打击 & 蓝防御（基础牌）
        "strike_green", "strike_green+",
        "strike_blue", "strike_blue+",
        "defend_blue", "defend_blue+",
       
        "dash", "dash+",
        "predator", "predator+",
        "flying_knee", "flying_knee+",
        "impervious", "impervious+",
        "intimidate", "intimidate+",
        "seeing_red", "seeing_red+",
        "bloodletting", "bloodletting+",
        "leg_sweep", "leg_sweep+",
        "terror", "terror+",
        "shockwave", "shockwave+",
        "ghostly_armor", "ghostly_armor+",
        "entrench", "entrench+",
        "offering", "offering+",
        "riddle_with_holes", "riddle_with_holes+",
        "wild_strike", "wild_strike+",
        "reckless_charge", "reckless_charge+",
    });
     // 普通关 1-3 只怪随机，从邪教徒池中抽取
     static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
     int monsterCount = 1 + static_cast<int>(rng() % 3);   // 1~3 只
     static const std::vector<MonsterId> normalPool = {"cultist", "fat_gremlin", "green_louse", "red_louse"};
     std::vector<MonsterId> monsters;
     for (int i = 0; i < monsterCount; ++i)
         monsters.push_back(normalPool[static_cast<size_t>(rng() % normalPool.size())]);
     // 遗物：燃烧之血为铁甲战士角色初始遗物，其余为测试用
     std::vector<RelicId> relics = {"burning_blood", "marble_bag"};  // 燃烧之血+弹珠袋
 
     // Mock：玩家初始获得 6 层金属化（在开战前加入状态）
     player.statuses.push_back(StatusInstance{"metallicize", 6, 3});  // 金属化：回合末加格挡，持续 3 回合
 
     // Mock：药水栏（含毒药水，需点击怪物指定目标）
     player.potions = {"poison_potion", "block_potion", "strength_potion"};  // 毒药水、格挡药水、力量药水
 
     engine.start_battle(monsters, player, card_system.get_master_deck_card_ids(), relics);  // 开始战斗

    CheatEngine cheat(&engine, &card_system);  // 金手指引擎（独立于主逻辑）
    CheatPanel cheat_panel(&cheat, static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));
    if (!cheat_panel.loadFont("assets/fonts/Sanji.ttf") && !cheat_panel.loadFont("assets/fonts/default.ttf"))
        cheat_panel.loadFont("data/font.ttf");

     BattleUI ui(static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));  // 创建战斗 UI
     // 主字体（英文/数字）
     if (!ui.loadFont("assets/fonts/Sanji.ttf"))            // 尝试加载主字体
         if (!ui.loadFont("assets/fonts/default.ttf"))
             ui.loadFont("data/font.ttf");
    // 中文字体（显示"结束回合""玩家""地图"等），优先系统字体
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf"))     // 尝试加载中文字体
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc"))   // 微软雅黑
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf"))  // 黑体
                ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");   // 宋体

    // 背景图：只有文件存在才尝试加载，避免控制台刷屏
    auto exists = [](const std::string& p) {
        return std::filesystem::exists(std::filesystem::u8path(p));
    };
    auto tryLoadBg = [&ui, &exists](int i, const std::string& ext) {
        std::string name = "bg_" + std::to_string(i) + ext;
        std::string p1 = "assets/backgrounds/" + name;
        std::string p2 = "./assets/backgrounds/" + name;
        if (exists(p1)) return ui.loadBackgroundForBattle(i, p1);
        if (exists(p2)) return ui.loadBackgroundForBattle(i, p2);
        return false;
    };
    for (int i = 0; i < 4; ++i) {
        tryLoadBg(i, ".png") || tryLoadBg(i, ".jpg");
    }
    // 默认背景：优先 bg.png，其次 bg.jpg
    if ((exists("assets/backgrounds/bg.png") && ui.loadBackground("assets/backgrounds/bg.png"))
        || (exists("./assets/backgrounds/bg.png") && ui.loadBackground("./assets/backgrounds/bg.png"))
        || (exists("assets/backgrounds/bg.jpg") && ui.loadBackground("assets/backgrounds/bg.jpg"))
        || (exists("./assets/backgrounds/bg.jpg") && ui.loadBackground("./assets/backgrounds/bg.jpg"))) {
        // loaded
    }
    ui.setBattleBackground(0);  // 首场战斗（邪教徒）

    // 怪物图片：放入 assets/monsters/{怪物id}.png 即可显示，支持 Title_Case.png（如 Fat_Gremlin.png）
    auto tryLoadMonster = [&ui](const std::string& mid) {
        auto tryPath = [&](const std::string& base, const std::string& name) {
            return ui.loadMonsterTexture(mid, base + name + ".png");
        };
        if (tryPath("assets/monsters/", mid) || tryPath("./assets/monsters/", mid)) return;
        // 尝试 Title_Case：fat_gremlin -> Fat_Gremlin
        std::string tc = mid;
        bool cap = true;
        for (char& c : tc) {
            if (c == '_') { cap = true; continue; }
            if (cap) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); cap = false; }
            else if (c >= 'A' && c <= 'Z') c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        tryPath("assets/monsters/", tc) || tryPath("./assets/monsters/", tc);
    };
    for (const auto& mid : monsters) tryLoadMonster(mid);

    // 玩家角色图片：放入 assets/player/{职业id}.png 即可显示，支持 Title_Case（如 Ironclad.png）
    auto tryLoadPlayer = [&ui](const std::string& cid) {
        auto tryPath = [&](const std::string& base, const std::string& name) {
            return ui.loadPlayerTexture(cid, base + name + ".png");
        };
        if (tryPath("assets/player/", cid) || tryPath("./assets/player/", cid)) return;
        // 尝试 Title_Case：ironclad -> Ironclad
        std::string tc = cid;
        bool cap = true;
        for (char& c : tc) {
            if (c == '_') { cap = true; continue; }
            if (cap) { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); cap = false; }
            else if (c >= 'A' && c <= 'Z') c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        tryPath("assets/player/", tc) || tryPath("./assets/player/", tc);
    };
    tryLoadPlayer(player.character);

    // 遗物图标：放入 assets/relics/{遗物id}.png 即可显示（药水在顶栏，遗物在左上）
    static const std::vector<std::string> relicIds = {
        "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle", "data_disk",
        "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull", "snake_skull",
        "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum", "pen_nib", "toy_ornithopter",
        "preparation_pack", "anchor", "art_of_war", "akabeko", "relic_strength_plus"
    };
    auto tryLoadRelic = [&ui](const std::string& rid) {
        auto tryPath = [&](const std::string& base) { return ui.loadRelicTexture(rid, base + rid + ".png"); };
        if (tryPath("assets/relics/") || tryPath("./assets/relics/")) return;
    };
    for (const auto& rid : relicIds) tryLoadRelic(rid);

    // 药水图标：放入 assets/potions/{药水id}.png 即可显示（顶栏药水槽）
    static const std::vector<std::string> potionIds = {
        "strength_potion", "block_potion", "energy_potion", "poison_potion", "weak_potion", "fear_potion",
        "explosion_potion", "swift_potion", "blood_potion", "fire_potion",
        "attack_potion", "dexterity_potion", "focus_potion", "speed_potion", "steroid_potion"
    };
    auto tryLoadPotion = [&ui](const std::string& pid) {
        auto tryPath = [&](const std::string& base) { return ui.loadPotionTexture(pid, base + pid + ".png"); };
        if (tryPath("assets/potions/") || tryPath("./assets/potions/")) return;
    };
    for (const auto& pid : potionIds) tryLoadPotion(pid);

    // 意图图标：assets/intention/Attack.png, Block.png, Strategy.png, Unknown.png
    auto tryLoadIntention = [&ui](const std::string& key) {
        return ui.loadIntentionTexture(key, "assets/intention/" + key + ".png")
            || ui.loadIntentionTexture(key, "./assets/intention/" + key + ".png");
    };
    tryLoadIntention("Attack");
    tryLoadIntention("Block");
    tryLoadIntention("Strategy");
    tryLoadIntention("Unknown");

    // 背景音乐：放入 assets/music/bgm.ogg 即可播放（支持 ogg/wav/flac）
    sf::Music bgm;
    if ((exists("assets/music/bgm.ogg") && bgm.openFromFile("assets/music/bgm.ogg"))
        || (exists("./assets/music/bgm.ogg") && bgm.openFromFile("./assets/music/bgm.ogg"))) {
        bgm.setLooping(true);
        bgm.setVolume(60.f);
        bgm.play();
    }

    while (window.isOpen()) {                              // 主循环：窗口未关闭则持续
         while (const std::optional ev = window.pollEvent()) {  // 处理所有待处理事件
             if (ev->is<sf::Event::Closed>())               // 关闭窗口事件
                 window.close();
             if (cheat_panel.handleEvent(*ev))             // 金手指面板优先（F2/输入/Enter）
                 continue;
             sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));  // 鼠标坐标转世界坐标
             if (ui.handleEvent(*ev, mousePos))              // 将事件交给 UI 处理
                 engine.end_turn();   // 点击结束回合按钮即调用
         }
         if (!window.isOpen()) break;                       // 若已关闭则退出循环
 
         sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));  // 当前鼠标位置
         ui.setMousePosition(mousePos);                     // 传给 UI（用于悬停等）
 
         // 若 UI 中有"打出牌"的请求，则调用战斗引擎出牌
         int handIndex = -1;                                // 手牌下标
         int targetMonsterIndex = -1;                       // 目标怪物下标
         if (ui.pollPlayCardRequest(handIndex, targetMonsterIndex)) {  // 轮询是否有出牌请求
             engine.play_card(handIndex, targetMonsterIndex);  // 执行出牌
         }
 
         int potionSlotIndex = -1;                          // 药水槽下标
         int potionTargetIndex = -1;                        // 药水目标怪物下标（-1 表示无需目标）
         if (ui.pollPotionRequest(potionSlotIndex, potionTargetIndex)) {  // 轮询是否有使用药水请求
             engine.use_potion(potionSlotIndex, potionTargetIndex);  // 执行使用药水
         }
 
         // 打开牌组界面：1=整个牌组(右上角牌组)，2=抽牌堆(左下角)，3=弃牌堆(右下角)；空则提示不打开
         int deckViewMode = 0;                              // 牌组视图模式
         if (ui.pollOpenDeckViewRequest(deckViewMode)) {    // 轮询是否请求打开牌组界面
             const auto mode = static_cast<DeckViewMode>(deckViewMode);  // 转为枚举
             std::vector<CardInstance> cards = collect_deck_view_cards(card_system, mode);  // 收集要展示的牌
             if (cards.empty()) {                           // 若为空
                 ui.showTip(deck_view_empty_tip(mode));     // 显示提示
             } else {
                 ui.set_deck_view_cards(std::move(cards));  // 设置牌组视图的牌列表
                 ui.set_deck_view_active(true);             // 打开牌组界面
             }
         }
 
         // 战斗结束检测：胜利则进入奖励界面，失败则显示提示并等待继续（可扩展为重启）
         if (engine.is_battle_over()) {
             if (engine.is_victory()) {
                if (!ui.is_reward_screen_active()) {
                    // 选牌必出；金币、遗物、药水各 40% 概率，互不冲突
                    constexpr int REWARD_CHANCE = 40;  // 40%
                    int victoryGold = 0;
                    if ((rng() % 100) < REWARD_CHANCE) {
                        engine.grant_victory_gold();
                        victoryGold = engine.get_victory_gold();
                    }
                    std::string relic_id;
                    if ((rng() % 100) < REWARD_CHANCE)
                        relic_id = engine.grant_reward_relic();
                    std::string potion_id;
                    if ((rng() % 100) < REWARD_CHANCE)
                        potion_id = engine.grant_reward_potion();
                    auto reward_cards = engine.get_reward_cards(3);  // 三选一（必出）
                    std::vector<std::string> card_ids(reward_cards.begin(), reward_cards.end());
                    std::vector<std::string> relic_ids;
                    if (!relic_id.empty()) relic_ids.push_back(relic_id);
                    std::vector<std::string> potion_ids;
                    if (!potion_id.empty()) potion_ids.push_back(potion_id);
                    ui.set_reward_data(victoryGold, std::move(card_ids), std::move(relic_ids), std::move(potion_ids));
                    ui.set_reward_screen_active(true);
                }
             } else {
                 ui.showTip(L"游戏结束！", 3.f);            // 失败提示（可扩展为重启界面）
             }
         }

         // 奖励界面：处理选牌与继续
         if (ui.is_reward_screen_active()) {
             int cardIndex = -2;
             if (ui.pollRewardCardPick(cardIndex)) {
                 if (cardIndex >= 0) {
                     std::string pickedId = ui.get_reward_card_id_at(static_cast<size_t>(cardIndex));
                     if (!pickedId.empty())
                         engine.add_card_to_master_deck(pickedId);  // 经引擎加入（触发陶瓷小鱼等遗物）
                 }
             }
             if (ui.pollContinueToNextBattleRequest()) {
                 ui.set_reward_screen_active(false);
                 BattleState curState = engine.get_battle_state();  // 获取最新状态（含胜利金币）
                 // 进入下一场战斗：轮换主题，每场随机 1-3 只怪
                 static int battleIndex = 0;
                 battleIndex = (battleIndex + 1) % 4;
                 static const std::vector<std::vector<MonsterId>> battlePool = {
                     {"cultist"},                                    // 邪教徒
                     {"fat_gremlin", "green_louse"},                 // 地精/虱虫
                     {"green_louse", "red_louse"},                   // 虱虫
                     {"red_louse", "fat_gremlin", "green_louse"}     // 混合
                 };
                 const auto& pool = battlePool[static_cast<size_t>(battleIndex)];
                 int nextCount = 1 + static_cast<int>(rng() % 3);
                 std::vector<MonsterId> nextMonsters;
                 for (int i = 0; i < nextCount; ++i)
                     nextMonsters.push_back(pool[static_cast<size_t>(rng() % pool.size())]);
                 player.gold = curState.player.gold;           // 继承金币
                 player.currentHp = curState.player.currentHp;  // 继承血量（可改为满血）
                 player.maxHp = curState.player.maxHp;
                 player.potions = curState.player.potions;
                 player.statuses = curState.player.statuses;
                 player.relics = curState.player.relics;
                 engine.start_battle(nextMonsters, player, card_system.get_master_deck_card_ids(), player.relics);
                 ui.setBattleBackground(battleIndex);  // 切换本场战斗背景
                 for (const auto& mid : nextMonsters) tryLoadMonster(mid);
             }
         }

         // 先推进回合阶段（含玩家回合开始时的能量/抽牌），再取快照绘制，使能量与 UI 当帧一致
         if (!ui.is_deck_view_active() && !ui.is_reward_screen_active())
             engine.step_turn_phase();                      // 推进回合阶段（下回合加能量在此生效后快照才取到）

         BattleState state = engine.get_battle_state();     // 获取当前战斗状态（已含本回合加能量、下回合加能量后的值）
         BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(state, &card_system);  // 转为 UI 快照
         SnapshotBattleUIDataProvider adapter(&snapshot);  // 快照适配器
         window.clear(sf::Color(28, 26, 32));               // 清屏（深色背景）
        ui.draw(window, adapter);                          // 绘制 UI
        cheat_panel.draw(window);                          // 金手指面板（F2 打开时）
        window.display();                                  // 显示到窗口
        engine.tick_damage_displays();                     // 递减伤害数字显示时长，移除过期项（3 秒）
     }
 }

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
     if (!ui.loadChineseFont("assets/fonts/simkai.ttf"))
     if (!ui.loadChineseFont("assets/fonts/Sanji.ttf"))
         if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf"))
             if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc"))
                 if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf"))
                     ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");

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
     sf::RenderWindow window(sf::VideoMode({ winW, winH }), "Tracer Civilization - 全功能测试");
     window.setFramerateLimit(60);

     sf::Font menuFont;
     if (!menuFont.openFromFile("C:/Windows/Fonts/msyh.ttc"))
         menuFont.openFromFile("C:/Windows/Fonts/simhei.ttf");
     sf::Text menuText(menuFont);
     menuText.setCharacterSize(28);
     menuText.setFillColor(sf::Color::White);
     menuText.setPosition({ 80.f, 80.f });

     while (window.isOpen()) {
         while (const std::optional ev = window.pollEvent()) {
             if (ev->is<sf::Event::Closed>()) { window.close(); return 0; }
             if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                 if (key->scancode == sf::Keyboard::Scancode::Escape) { window.close(); return 0; }
                 if (key->scancode == sf::Keyboard::Scancode::Num1 || key->scancode == sf::Keyboard::Scancode::Numpad1 ||
                     key->scancode == sf::Keyboard::Scancode::F1) {
                     runBattleUI(window);
                     if (!window.isOpen()) return 0;
                     continue;
                 }
                 if (key->scancode == sf::Keyboard::Scancode::Num2 || key->scancode == sf::Keyboard::Scancode::Numpad2 ||
                     key->scancode == sf::Keyboard::Scancode::F2) {
                     runEventShopRestUITest(window);
                     if (!window.isOpen()) return 0;
                     continue;
                 }
                 if (key->scancode == sf::Keyboard::Scancode::Num3 || key->scancode == sf::Keyboard::Scancode::Numpad3 ||
                     key->scancode == sf::Keyboard::Scancode::F3) {
                     runMapUITest(window);
                     if (!window.isOpen()) return 0;
                     continue;
                 }
             }
         }
         menuText.setString(
             L"【全功能测试】\n\n"
             L"  1 / F1  战斗 UI（含金手指 F2）\n"
             L"  2 / F2  事件 / 商店 / 休息 UI\n"
             L"  3 / F3  地图 UI\n\n"
             L"  Esc  退出"
         );
         window.clear(sf::Color(32, 30, 36));
         window.draw(menuText);
         window.display();
     }
     return 0;
 }