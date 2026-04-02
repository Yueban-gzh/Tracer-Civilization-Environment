/**
 * 临时入口：仅运行战斗模块（便于怪物/卡牌战斗测试）
 */
#include <SFML/Graphics.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/ContextSettings.hpp>
#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.hpp"
#include "BattleCoreRefactor/BattleEngine.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#include "BattleEngine/MonsterBehaviors.hpp"
#include "CardSystem/CardSystem.hpp"
#include "Common/RunRng.hpp"
#include "DataLayer/DataLayer.h"
#include "DataLayer/DataLayer.hpp"
#include "Effects/CardEffects.hpp"

#ifdef _WIN32
static std::string get_exe_directory() {
    char buf[MAX_PATH]{};
    if (GetModuleFileNameA(nullptr, buf, MAX_PATH) == 0) return {};
    std::string full(buf);
    const size_t pos = full.find_last_of("\\/");
    if (pos == std::string::npos) return {};
    return full.substr(0, pos);
}

/** 从资源管理器双击 build\\xxx.exe 时，当前目录往往是 build\\，导致找不到上一级的 data/、assets/。若 exe 在名为 build 的目录下，则把 cwd 设为上一级（项目根）。 */
static void setup_working_directory_for_assets_and_data() {
    const std::string exeDir = get_exe_directory();
    if (exeDir.empty()) return;
    std::string trim = exeDir;
    while (!trim.empty() && (trim.back() == '\\' || trim.back() == '/')) trim.pop_back();
    const size_t slash = trim.find_last_of("\\/");
    const std::string leaf = (slash == std::string::npos) ? trim : trim.substr(slash + 1);
    if (leaf == "build")
        SetCurrentDirectoryA((exeDir + "\\..").c_str());
    else
        SetCurrentDirectoryA(exeDir.c_str());
}

static std::string startup_log_path() {
    const std::string d = get_exe_directory();
    if (!d.empty()) return d + "\\TracerCE_startup.log";
    return "TracerCE_startup.log";
}

static void init_startup_log() {
    std::ofstream f(startup_log_path(), std::ios::trunc);
    if (f) {
        f << "=== TracerCE startup log ===\nexe_dir=" << get_exe_directory() << "\n";
        f.flush();
    }
}

static void log_startup(const std::string& line) {
    std::ofstream f(startup_log_path(), std::ios::app);
    if (f) {
        f << line << "\n";
        f.flush();
    }
    OutputDebugStringA(("[TracerCE] " + line + "\n").c_str());
}

static void show_error_message(const char* text) {
    MessageBoxA(nullptr, text, "Tracer Civilization", MB_OK | MB_ICONERROR);
}
#else
static void setup_working_directory_for_assets_and_data() {}
static void init_startup_log() {}
static void log_startup(const std::string& line) { std::cerr << line << "\n" << std::flush; }
static void show_error_message(const char* text) { std::cerr << text << "\n"; }
#endif

int main() {
    setup_working_directory_for_assets_and_data();
    init_startup_log();
    log_startup("working directory adjusted (if exe in build\\)");

    try {
    using namespace tce;

    // 先加载数据再创建窗口：若 cards.json 找不到会 return，避免先弹出游戏窗口再立刻退出（看起来像「闪退」）
    DataLayer::DataLayerImpl data;
    if (!data.load_cards("") || !data.load_monsters("") || !data.load_encounters("")) {
        const char* msg =
            "Data load failed (data/cards.json).\n"
            "See TracerCE_startup.log next to the exe.\n"
            "Or run from project root / copy data folder beside exe.";
        log_startup("ERROR: load_cards or load_monsters returned false");
        std::cerr << "[BattleOnly] " << msg << "\n";
        show_error_message(msg);
        return 1;
    }
    log_startup("load_cards + load_monsters + load_encounters OK");

    // 客户端设计分辨率 1920×1080（BattleUI 内像素常量按此基准）；若桌面更小则缩小以完整显示
    constexpr unsigned kDesignW = 1920u;
    constexpr unsigned kDesignH = 1080u;
    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    unsigned winW = kDesignW;
    unsigned winH = kDesignH;
    if (desktop.size.x > 0u && static_cast<unsigned>(desktop.size.x) < winW)
        winW = static_cast<unsigned>(desktop.size.x);
    if (desktop.size.y > 0u && static_cast<unsigned>(desktop.size.y) < winH)
        winH = static_cast<unsigned>(desktop.size.y);
    log_startup("VideoMode request " + std::to_string(winW) + "x" + std::to_string(winH) +
                " (design " + std::to_string(kDesignW) + "x" + std::to_string(kDesignH) +
                ", desktop " + std::to_string(static_cast<unsigned>(desktop.size.x)) + "x" +
                std::to_string(static_cast<unsigned>(desktop.size.y)) + ")");

    sf::ContextSettings gl;
    gl.depthBits = 0;
    gl.stencilBits = 0;
    gl.antiAliasingLevel = 0;

    sf::RenderWindow window(
        sf::VideoMode({winW, winH}),
        "Tracer Civilization - Battle Only",
        sf::Style::Default,
        sf::State::Windowed,
        gl);
    // 与 setFramerateLimit 二选一；垂直同步可降低 GPU 空转与驱动超时风险
    window.setVerticalSyncEnabled(true);
    window.setFramerateLimit(0);
    log_startup("RenderWindow created (vsync on, framerateLimit 0, GL depth 0)");
    // 窗口刚显示时 Windows 可能注入合成鼠标事件；若仍用全局 Mouse::getPosition 做命中测试，易误点「结束回合」并瞬间走完多段敌方阶段。
    sf::Clock bootClock;
    bootClock.restart();

    RunRng runRng(0x20260401ULL);
    CardSystem cardSystem(get_card_by_id, &runRng);
    register_all_card_effects(cardSystem);
    log_startup("CardSystem + register_all_card_effects OK");

    PlayerBattleState player;
    player.playerName = "Telys";
    player.character = "Ironclad";
    player.currentHp = 80;
    player.maxHp = 80;
    player.energy = 3;
    player.maxEnergy = 3;
    player.gold = 99;
    player.cardsToDrawPerTurn = 5;
    player.relics = {"burning_blood"};

    // 战斗专用简牌组：避免依赖“需要额外选牌”的复杂卡，先稳定测核心战斗循环。
    cardSystem.init_master_deck({
        "strike", "strike", "strike", "strike", "strike",
        "defend", "defend", "defend", "defend",
        "bash"
    });
    log_startup("init_master_deck OK");

    BattleEngine battleEngine(
        cardSystem,
        get_monster_by_id,
        get_card_by_id,
        execute_monster_behavior,
        &runRng);

    // 使用新怪物进行默认战斗调试（与 encounters.json / monster_behaviors.json 保持一致）
    battleEngine.start_battle(
        {"buguguiqi", "dading"},
        player,
        cardSystem.get_master_deck_card_ids(),
        player.relics);
    log_startup("start_battle OK, monsters=" + std::to_string(battleEngine.get_battle_state().monsters.size()) +
                " is_battle_over=" + (battleEngine.is_battle_over() ? std::string("1") : std::string("0")));

    BattleUI ui(static_cast<unsigned>(window.getSize().x), static_cast<unsigned>(window.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf")) {
        if (!ui.loadFont("assets/fonts/default.ttf")) {
            ui.loadFont("data/font.ttf");
        }
    }
    if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
        if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
            }
        }
    }
    log_startup("BattleUI fonts attempted");

    // 为本场战斗的怪物加载贴图：约定文件名为 assets/monsters/<monster_id>.png 或 .jpg
    auto exists = [](const std::string& p) {
        return std::filesystem::exists(std::filesystem::u8path(p));
    };
    const BattleState& initState = battleEngine.get_battle_state();
    for (const auto& m : initState.monsters) {
        const std::string basePng = "assets/monsters/" + m.id + ".png";
        const std::string baseJpg = "assets/monsters/" + m.id + ".jpg";
        if (exists(basePng)) {
            ui.loadMonsterTexture(m.id, basePng);
        } else if (exists(baseJpg)) {
            ui.loadMonsterTexture(m.id, baseJpg);
        }
    }

    bool battleOverLogged = false;
    bool reward_phase_started = false;
    static unsigned loop_frame = 0;
    sf::Clock loopWallClock;
    loopWallClock.restart();
    log_startup(
        "entering main loop: first 16 frames log each tick; then every 60 frames + elapsed seconds "
        "(gaps are normal, not crash)");
    while (window.isOpen()) {
        const unsigned fi = loop_frame++;
        const float tsec = loopWallClock.getElapsedTime().asSeconds();
        if (fi < 16u || (fi % 60u) == 0u) {
            char buf[96];
            std::snprintf(buf, sizeof(buf), "loop frame %u t=%.2fs", fi, static_cast<double>(tsec));
            log_startup(buf);
        }
        while (const std::optional ev = window.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                log_startup("Event::Closed -> window.close()");
                window.close();
                break;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Escape) {
                    if (bootClock.getElapsedTime().asSeconds() < 0.5f) {
                        log_startup("Key::Escape ignored (startup debounce)");
                        continue;
                    }
                    log_startup("Key::Escape -> window.close()");
                    window.close();
                    break;
                }
            }
            // 忽略启动后极短时间内的左键按下，避免误触「结束回合」
            if (const auto* mb0 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mb0->button == sf::Mouse::Button::Left && bootClock.getElapsedTime().asSeconds() < 0.40f) {
                    log_startup("MouseButtonPressed Left ignored (startup debounce)");
                    continue;
                }
            }
            sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (const auto* mb = ev->getIf<sf::Event::MouseButtonPressed>())
                mousePos = window.mapPixelToCoords(sf::Vector2i(mb->position));
            else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>())
                mousePos = window.mapPixelToCoords(sf::Vector2i(mr->position));
            else if (const auto* mm = ev->getIf<sf::Event::MouseMoved>())
                mousePos = window.mapPixelToCoords(sf::Vector2i(mm->position));
            else if (const auto* mw = ev->getIf<sf::Event::MouseWheelScrolled>())
                mousePos = window.mapPixelToCoords(sf::Vector2i(mw->position));

            if (ui.handleEvent(*ev, mousePos)) {
                log_startup("handleEvent returned true -> end_turn()");
                battleEngine.end_turn();
            }
        }
        if (!window.isOpen()) break;

        const sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
        ui.setMousePosition(mousePos);
        ui.syncRestForgeScrollbarDrag(mousePos);

        int handIndex = -1;
        int targetMonsterIndex = -1;
        if (ui.pollPlayCardRequest(handIndex, targetMonsterIndex)) {
            battleEngine.play_card(handIndex, targetMonsterIndex);
        }

        int potionSlotIndex = -1;
        int potionTargetIndex = -1;
        if (ui.pollPotionRequest(potionSlotIndex, potionTargetIndex)) {
            battleEngine.use_potion(potionSlotIndex, potionTargetIndex);
        }

        if (!ui.is_reward_screen_active() && !battleEngine.is_battle_over()) {
            battleEngine.step_turn_phase();
        }

        if (battleEngine.is_battle_over()) {
            if (battleEngine.is_victory()) {
                if (!reward_phase_started) {
                    reward_phase_started = true;
                    const int gold_reward = battleEngine.get_victory_gold();
                    battleEngine.grant_victory_gold();
                    std::vector<tce::CardId> reward_cards = battleEngine.get_reward_cards(3);
                    std::vector<std::string> reward_strs;
                    reward_strs.reserve(reward_cards.size());
                    for (const auto& c : reward_cards) reward_strs.push_back(c);
                    std::vector<std::string> relic_ids;
                    std::vector<std::string> potion_ids;
                    const tce::RelicId r = battleEngine.grant_reward_relic();
                    if (!r.empty()) relic_ids.push_back(r);
                    const tce::PotionId p = battleEngine.grant_reward_potion();
                    if (!p.empty()) potion_ids.push_back(p);
                    ui.set_reward_data(gold_reward, std::move(reward_strs), std::move(relic_ids), std::move(potion_ids));
                    ui.set_reward_screen_active(true);
                    std::cout << "[BattleOnly] 战斗胜利 — 请选择奖励卡牌或跳过，然后点继续。\n";
                }
                int reward_pick = -2;
                if (ui.pollRewardCardPick(reward_pick)) {
                    if (reward_pick >= 0) {
                        const std::string cid = ui.get_reward_card_id_at(static_cast<size_t>(reward_pick));
                        if (!cid.empty()) battleEngine.add_card_to_master_deck(cid);
                    }
                }
                if (ui.pollContinueToNextBattleRequest()) {
                    ui.set_reward_screen_active(false);
                    reward_phase_started = false;
                    battleOverLogged = false;
                    const PlayerBattleState nextPlayer = battleEngine.get_battle_state().player;
                    battleEngine.start_battle(
                        {"cultist", "fat_gremlin"},
                        nextPlayer,
                        cardSystem.get_master_deck_card_ids(),
                        nextPlayer.relics);
                    std::cout << "[BattleOnly] 奖励已确认，已进入新战斗。\n";
                }
            } else if (!battleOverLogged) {
                battleOverLogged = true;
                std::cout << "[BattleOnly] 战斗失败\n";
            }
        }

        if (fi == 0u) log_startup("frame0: before snapshot");
        BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(battleEngine.get_battle_state(), &cardSystem);
        if (fi == 0u) log_startup("frame0: before draw");
        SnapshotBattleUIDataProvider adapter(&snapshot);
        window.clear(sf::Color(28, 26, 32));
        ui.draw(window, adapter);
        if (fi == 0u) log_startup("frame0: before display");
        window.display();
        if (fi == 0u) log_startup("frame0: after display OK");

        battleEngine.tick_damage_displays();
    }

    log_startup("main loop exited normally");
    return 0;

    } catch (const std::exception& e) {
        const std::string what = e.what();
        log_startup(std::string("std::exception: ") + what);
        std::string box = "Fatal error (exception). See TracerCE_startup.log next to exe.\n\n";
        box += what;
        show_error_message(box.c_str());
        return 1;
    } catch (...) {
        log_startup("unknown non-std exception");
        show_error_message("Fatal error (unknown). See TracerCE_startup.log next to the exe.");
        return 1;
    }

    // 循环：开始界面 → 一次游戏流程 → 回到开始界面，直到窗口被关闭
    while (window.isOpen()) {
        // 开始界面：提供“开始新游戏 / 继续游戏”选项
        tce::runStartScreen(window, game);
        if (!window.isOpen()) break;

        game.run();  // 可能在地图/战斗/事件等界面的“保存并退出”中提前 return，回到本循环顶部
    }

    return 0;
}
