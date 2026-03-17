/**
 * 《溯源者：文明环境》课设 - 程序入口
 * 模块：MapEngine(A)、BattleEngine(B)、CardSystem(C)、EventEngine(D)、DataLayer(E)
 *
 * 当前：战斗调试 - 接引擎与 Snapshot，点击结束回合即调用 engine.end_turn()
 */

 #include <SFML/Graphics.hpp>                              // SFML 图形库
 #include <SFML/Audio.hpp>                                 // 背景音乐
 #include <cctype>                                          // std::toupper, std::tolower
 #include <ctime>                                           // std::time
 #include <iostream>                                        // std::cerr
 #include <random>                                          // std::mt19937
 #include <optional>                                       // std::optional
#include "BattleEngine/BattleUI.hpp"                       // 战斗 UI
#include "BattleEngine/BattleUISnapshotAdapter.hpp"       // UI 快照适配器
#include "BattleEngine/BattleEngine.hpp"                  // 战斗引擎
#include "Cheat/CheatEngine.hpp"                           // 金手指引擎
#include "Cheat/CheatPanel.hpp"                            // 金手指面板
 #include "BattleEngine/BattleStateSnapshot.hpp"           // 战斗状态快照
 #include "BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.hpp"  // 核心重构快照适配器
 #include "BattleEngine/MonsterBehaviors.hpp"              // 怪物行为
 #include "CardSystem/CardSystem.hpp"                       // 卡牌系统
 #include "CardSystem/DeckViewCollection.hpp"              // 牌组视图
 #include "DataLayer/DataLayer.hpp"                         // 数据层
 #include "DataLayer/DataLayer.h"                           // 数据层 C 接口
 #include "Effects/CardEffects.hpp"                         // 卡牌效果注册
 
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
 
    // 初始牌组：包含所有已实现效果的卡牌（CardEffects 中注册的非空效果），每种只加入一张
    card_system.init_master_deck({
      


        // 铁斩波 / 金刚臂 / 顺劈斩 / 飞踢 / 残杀 / 双重打击 / 全身撞击 / 剑柄打击 / 连续拳 / 御血术 / 重刃 / 闪电霹雳 / 重锤
        "iron_wave", "iron_wave+",
        "clothesline", "clothesline+",
        "cleave", "cleave+",
        "dropkick", "dropkick+",
        "carnage", "carnage+",
        "twin_strike", "twin_strike+",
        "body_slam", "body_slam+",
        "pommel_strike", "pommel_strike+",
        "pummel", "pummel+",
        "hemokinesis", "hemokinesis+",
        "heavy_blade", "heavy_blade+",
        "thunderclap", "thunderclap+",
        "bludgeon", "bludgeon+",

    
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

    // 背景图：每场战斗一张，bg_0.png~bg_3.png 对应邪教徒/胖地精/绿虱虫/红虱虫，无则用 bg.png
    auto tryLoadBg = [&ui](int i, const std::string& ext) {
        std::string name = "bg_" + std::to_string(i) + ext;
        return ui.loadBackgroundForBattle(i, "assets/backgrounds/" + name)
            || ui.loadBackgroundForBattle(i, "./assets/backgrounds/" + name);
    };
    for (int i = 0; i < 4; ++i) {
        tryLoadBg(i, ".png") || tryLoadBg(i, ".jpg");
    }
    if (!ui.loadBackground("assets/backgrounds/bg.png") && !ui.loadBackground("./assets/backgrounds/bg.png"))
        ui.loadBackground("assets/backgrounds/bg.jpg") || ui.loadBackground("./assets/backgrounds/bg.jpg");
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
    if (bgm.openFromFile("assets/music/bgm.ogg") || bgm.openFromFile("./assets/music/bgm.ogg")) {
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

         // 先根据当前阶段获取快照并绘制 UI，再推进到下一个阶段
         BattleState state = engine.get_battle_state();     // 获取当前战斗状态
         BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(state, &card_system);  // 转为 UI 快照
         SnapshotBattleUIDataProvider adapter(&snapshot);  // 快照适配器
         window.clear(sf::Color(28, 26, 32));               // 清屏（深色背景）
        ui.draw(window, adapter);                          // 绘制 UI
        cheat_panel.draw(window);                          // 金手指面板（F2 打开时）
        window.display();                                  // 显示到窗口
        engine.tick_damage_displays();                     // 递减伤害数字显示时长，移除过期项（3 秒）

        // 牌组界面或奖励界面打开时不推进回合；否则每帧推进一次回合阶段
         if (!ui.is_deck_view_active() && !ui.is_reward_screen_active())
             engine.step_turn_phase();                      // 推进回合阶段（抽牌/敌方行动等）
     }
 }
 
 int main() {                                               // 程序入口
     const unsigned int winW = 1920, winH = 1080;          // 窗口宽高
     sf::RenderWindow window(sf::VideoMode({winW, winH}), "Battle Debug - Tracer Civilization");  // 创建窗口
     window.setFramerateLimit(60);                          // 限制 60fps，使伤害数字 180 帧 = 3 秒

     runBattleUI(window);                                  // 运行战斗 UI
     return 0;
 }