#include "GameFlow/GameFlowController.hpp"

#include <algorithm>
#include <ctime>
#include <filesystem>

#include "BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.hpp"
#include "BattleEngine/BattleStateSnapshot.hpp"
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleUISnapshotAdapter.hpp"
#include "BattleEngine/MonsterBehaviors.hpp"
#include "CardSystem/DeckViewCollection.hpp"
#include "Cheat/CheatEngine.hpp"
#include "Cheat/CheatPanel.hpp"
#include "DataLayer/DataLayer.hpp"
#include "Effects/CardEffects.hpp"
#include "EventEngine/EventShopRestUI.hpp"

namespace tce {

namespace {
int randomIndex(std::mt19937& rng, int boundExclusive) {
    if (boundExclusive <= 0) return 0;
    std::uniform_int_distribution<int> dist(0, boundExclusive - 1);
    return dist(rng);
}

void applyBattleTestMock(PlayerBattleState& p) {
    if (std::find(p.relics.begin(), p.relics.end(), "burning_blood") == p.relics.end()) {
        p.relics.push_back("burning_blood");
    }
    if (std::find(p.relics.begin(), p.relics.end(), "marble_bag") == p.relics.end()) {
        p.relics.push_back("marble_bag");
    }

    p.statuses.erase(std::remove_if(p.statuses.begin(), p.statuses.end(),
        [](const StatusInstance& s) { return s.id == "metallicize"; }),
        p.statuses.end());
    p.statuses.push_back(StatusInstance{"metallicize", 6, 3});

    p.potions = {"poison_potion", "block_potion", "strength_potion"};
}
} // namespace

GameFlowController::GameFlowController(sf::RenderWindow& window)
    : window_(window)
    , cardSystem_([](CardId id) { return get_card_by_id(id); })
    , battleEngine_(
        cardSystem_,
        [](MonsterId id) { return get_monster_by_id(id); },
        [](CardId id) { return get_card_by_id(id); },
        execute_monster_behavior)
    , eventEngine_(
        [this](EventEngine::EventId id) { return dataLayer_.get_event_by_id(id); },
        [this](CardId id) { cardSystem_.add_to_master_deck(id); },
        [this](InstanceId id) {
            CardId removed;
            if (!cardSystem_.remove_from_master_deck(id, &removed)) return false;
            if (removed == "parasite" || removed == "parasite+") {
                playerState_.maxHp = std::max(1, playerState_.maxHp - 3);
                if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
            }
            return true;
        },
        [this](InstanceId id) { return cardSystem_.upgrade_card_in_master_deck(id); })
    , rng_(static_cast<unsigned>(std::time(nullptr))) {}

bool GameFlowController::initialize() {
    dataLayer_.load_cards("");
    dataLayer_.load_monsters("");
    dataLayer_.load_events("");
    register_all_card_effects(cardSystem_);

    playerState_.playerName = "Telys";
    playerState_.character = "Ironclad";
    playerState_.currentHp = 80;
    playerState_.maxHp = 80;
    playerState_.energy = 3;
    playerState_.maxEnergy = 3;
    playerState_.gold = 99;
    playerState_.cardsToDrawPerTurn = 5;
    playerState_.relics = { "burning_blood" };

    cardSystem_.init_master_deck({
        "armaments",
        "exhume",
        "burning_pact",
        "true_grit",
        "survivor",
        "acrobatics",
        "flame_barrier",
        "second_wind",
        "spot_weakness",
        "prepared",
        "all_out_attack",
        "calculated_gamble",
        "concentrate",
        "piercing_wail",
        "sneaky_strike",
        "venomology",
        "noxious_fumes",
        "reflex",
        "tactician",
        "tools_of_the_trade",
        "well_laid_plans",
    });

    const MapEngine::MapConfig* config = mapConfigManager_.getCurrentConfig();
    if (!config) return false;
    mapEngine_.init_fixed_map(*config);

    if (!mapUI_.initialize(&window_)) return false;
    mapUI_.loadLegendTexture("assets/images/menu.png");
    mapUI_.setLegendPosition(1600.f, 120.f);
    mapUI_.loadBackgroundTexture("assets/images/background.png");
    mapUI_.setMap(&mapEngine_);
    mapUI_.setCurrentLayer(0);

    if (hudFont_.openFromFile("C:/Windows/Fonts/msyh.ttc") ||
        hudFont_.openFromFile("C:/Windows/Fonts/simhei.ttf")) {
        hudFontLoaded_ = true;
    }

    statusText_ = "选择第一个节点开始爬塔。";
    return true;
}

void GameFlowController::run() {
    while (window_.isOpen()) {
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    window_.close();
                    return;
                }
                if (key->scancode == sf::Keyboard::Scancode::Left) {
                    mapConfigManager_.prevMap();
                    if (const auto* cfg = mapConfigManager_.getCurrentConfig()) {
                        mapEngine_.init_fixed_map(*cfg);
                        mapUI_.setCurrentLayer(0);
                        statusText_ = "已切换到上一张地图。";
                    }
                }
                if (key->scancode == sf::Keyboard::Scancode::Right) {
                    mapConfigManager_.nextMap();
                    if (const auto* cfg = mapConfigManager_.getCurrentConfig()) {
                        mapEngine_.init_fixed_map(*cfg);
                        mapUI_.setCurrentLayer(0);
                        statusText_ = "已切换到下一张地图。";
                    }
                }
            }
            if (gameOver_ || gameCleared_) continue;
            if (const auto* mouse = ev->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouse->button != sf::Mouse::Button::Left) continue;
                const std::string nodeId = mapUI_.handleClick(mouse->position.x, mouse->position.y);
                if (nodeId.empty()) continue;
                tryMoveToNode(nodeId);
            }
        }

        window_.clear(sf::Color(245, 245, 245));
        mapUI_.draw();
        drawHud();
        window_.display();
    }
}

bool GameFlowController::tryMoveToNode(const std::string& nodeId) {
    MapEngine::MapNode node = mapEngine_.get_node_by_id(nodeId);
    if (node.id.empty()) return false;

    const bool canEnterStart = (!mapEngine_.hasCurrentNode() && node.layer == 0);
    const bool canEnterNext = (mapEngine_.hasCurrentNode() && node.is_reachable);
    if (!canEnterStart && !canEnterNext) {
        statusText_ = "该节点当前不可达。";
        return false;
    }

    mapEngine_.set_current_node(nodeId);
    mapEngine_.set_node_visited(nodeId);
    mapEngine_.update_reachable_nodes();
    mapUI_.setCurrentLayer(node.layer);
    resolveNode(node);
    return true;
}

void GameFlowController::resolveNode(const MapEngine::MapNode& node) {
    switch (node.type) {
    case NodeType::Enemy:
    case NodeType::Elite:
    case NodeType::Boss:
        runBattleScene(node.type);
        break;
    case NodeType::Event:
        if (runEventScene(node.content_id)) {
            statusText_ = "事件结束。";
        } else {
            statusText_ = "事件已中断或数据缺失。";
        }
        break;
    case NodeType::Rest:
        if (runRestScene()) {
            statusText_ = "休息结算完成。";
        } else {
            statusText_ = "休息已中断。";
        }
        break;
    case NodeType::Merchant:
        if (runShopScene()) {
            statusText_ = "商店结算完成。";
        } else {
            statusText_ = "商店已中断或金币不足。";
        }
        break;
    case NodeType::Treasure:
        resolveTreasure();
        break;
    default:
        break;
    }
}

bool GameFlowController::runBattleScene(NodeType nodeType) {
    std::vector<MonsterId> monsters;
    if (nodeType == NodeType::Boss) {
        monsters = { "hexaghost" };
    } else if (nodeType == NodeType::Elite) {
        monsters = { "fat_gremlin", "green_louse" };
    } else {
        static const std::vector<MonsterId> normalPool = { "cultist", "green_louse", "red_louse" };
        const int count = 1 + randomIndex(rng_, 3);
        for (int i = 0; i < count; ++i) {
            monsters.push_back(normalPool[static_cast<size_t>(randomIndex(rng_, static_cast<int>(normalPool.size())))]);
        }
    }

    applyBattleTestMock(playerState_);
    battleEngine_.start_battle(monsters, playerState_, cardSystem_.get_master_deck_card_ids(), playerState_.relics);
    BattleUI ui(static_cast<unsigned>(window_.getSize().x), static_cast<unsigned>(window_.getSize().y));
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

    auto exists = [](const std::string& p) {
        return std::filesystem::exists(std::filesystem::u8path(p));
    };
    if ((exists("assets/backgrounds/bg.png") && ui.loadBackground("assets/backgrounds/bg.png")) ||
        (exists("assets/backgrounds/bg.jpg") && ui.loadBackground("assets/backgrounds/bg.jpg"))) {
        // loaded
    }

    CheatEngine cheat(&battleEngine_, &cardSystem_);
    CheatPanel cheatPanel(&cheat,
                          static_cast<unsigned>(window_.getSize().x),
                          static_cast<unsigned>(window_.getSize().y));
    if (!cheatPanel.loadFont("assets/fonts/Sanji.ttf") &&
        !cheatPanel.loadFont("assets/fonts/default.ttf")) {
        cheatPanel.loadFont("data/font.ttf");
    }

    bool runningBattleScene = true;
    struct PendingCardSelectPlay {
        bool active = false;
        int playHandIndex = -1;
        int playTargetMonsterIndex = -1;
        int requiredCount = 1;
        std::wstring title;
        std::vector<InstanceId> candidateInstanceIds;
        std::vector<InstanceId> selectedInstanceIds;
    } pendingSelectPlay;
    while (window_.isOpen() && runningBattleScene) {
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (cheatPanel.handleEvent(*ev)) {
                continue;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    runningBattleScene = false;
                }
            }

            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mousePos = window_.mapPixelToCoords(mp->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>()) {
                mousePos = window_.mapPixelToCoords(mr->position);
            }
            if (ui.handleEvent(*ev, mousePos)) {
                battleEngine_.end_turn();
            }
        }
        if (!window_.isOpen() || !runningBattleScene) break;

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        int handIndex = -1;
        int targetMonsterIndex = -1;
        if (ui.pollPlayCardRequest(handIndex, targetMonsterIndex)) {
            const auto& handNow = cardSystem_.get_hand();
            if (handIndex >= 0 && static_cast<size_t>(handIndex) < handNow.size()) {
                const CardId& id = handNow[static_cast<size_t>(handIndex)].id;

                if (id == "exhume" || id == "exhume+") {
                    const auto& exPile = cardSystem_.get_exhaust_pile();
                    if (exPile.empty()) {
                        ui.showTip(L"消耗堆为空", 1.5f);
                    } else {
                        std::vector<std::string> candidates;
                        std::vector<InstanceId> candidateIids;
                        for (const auto& c : exPile) {
                            candidates.push_back(c.id);
                            candidateIids.push_back(c.instanceId);
                        }
                        pendingSelectPlay.active = true;
                        pendingSelectPlay.playHandIndex = handIndex;
                        pendingSelectPlay.playTargetMonsterIndex = targetMonsterIndex;
                        pendingSelectPlay.requiredCount = 1;
                        pendingSelectPlay.title = L"选择一张已消耗的牌";
                        pendingSelectPlay.candidateInstanceIds = candidateIids;
                        pendingSelectPlay.selectedInstanceIds.clear();
                        std::vector<int> noHandIdx;
                        ui.set_card_select_data(
                            pendingSelectPlay.title,
                            std::move(candidates),
                            true,
                            false,
                            std::vector<InstanceId>(pendingSelectPlay.candidateInstanceIds),
                            1,
                            std::move(noHandIdx),
                            -1);
                        ui.set_card_select_active(true);
                    }
                } else {
                    bool needSelect = (id == "armaments" ||
                                       id == "survivor" || id == "survivor+" ||
                                       id == "true_grit+" ||
                                       id == "burning_pact" || id == "burning_pact+" ||
                                       id == "concentrate" || id == "concentrate+" ||
                                       id == "all_out_attack" || id == "all_out_attack+" ||
                                       id == "acrobatics" || id == "acrobatics+" ||
                                       id == "prepared" || id == "prepared+");
                    if (needSelect) {
                        std::vector<std::string> candidates;
                        std::vector<InstanceId> candidateIids;
                        std::vector<int> candidateHandIdx;
                        for (size_t i = 0; i < handNow.size(); ++i) {
                            if (static_cast<int>(i) == handIndex) continue;
                            candidates.push_back(handNow[i].id);
                            candidateIids.push_back(handNow[i].instanceId);
                            candidateHandIdx.push_back(static_cast<int>(i));
                        }
                        if (!candidates.empty()) {
                            pendingSelectPlay.active = true;
                            pendingSelectPlay.playHandIndex = handIndex;
                            pendingSelectPlay.playTargetMonsterIndex = targetMonsterIndex;
                            pendingSelectPlay.requiredCount =
                                (id == "concentrate+") ? 2 :
                                (id == "concentrate") ? 3 :
                                (id == "prepared+") ? 2 :
                                (id == "prepared") ? 1 :
                                1;
                            if (pendingSelectPlay.requiredCount > static_cast<int>(candidates.size()))
                                pendingSelectPlay.requiredCount = static_cast<int>(candidates.size());
                            if (pendingSelectPlay.requiredCount <= 0) {
                                battleEngine_.play_card(handIndex, targetMonsterIndex);
                                continue;
                            }
                            pendingSelectPlay.selectedInstanceIds.clear();
                            if (id == "armaments") {
                                pendingSelectPlay.title = L"选择要升级的手牌";
                            } else if (id == "true_grit+" ||
                                id == "burning_pact" || id == "burning_pact+") {
                                pendingSelectPlay.title = L"选择要消耗的手牌";
                            } else {
                                pendingSelectPlay.title = L"选择要丢弃的手牌";
                            }
                            pendingSelectPlay.candidateInstanceIds = candidateIids;
                            ui.set_card_select_data(
                                pendingSelectPlay.title,
                                std::move(candidates), true, true, std::move(candidateIids), pendingSelectPlay.requiredCount, std::move(candidateHandIdx),
                                handIndex);
                            ui.set_card_select_active(true);
                        } else {
                            battleEngine_.play_card(handIndex, targetMonsterIndex);
                        }
                    } else {
                        battleEngine_.play_card(handIndex, targetMonsterIndex);
                    }
                }
            } else {
                battleEngine_.play_card(handIndex, targetMonsterIndex);
            }
        }

        std::vector<int> pickedHandIndices;
        bool cardSelectCancelled = false;
        if (ui.pollCardSelectResult(pickedHandIndices, cardSelectCancelled)) {
            if (pendingSelectPlay.active) {
                if (cardSelectCancelled) {  // 取消：本次不打出牌
                    pendingSelectPlay.active = false;
                    pendingSelectPlay.selectedInstanceIds.clear();
                    pendingSelectPlay.candidateInstanceIds.clear();
                } else {
                    for (int pickedHandIndex : pickedHandIndices) {
                        if (pickedHandIndex >= 0 && static_cast<size_t>(pickedHandIndex) < pendingSelectPlay.candidateInstanceIds.size()) {
                            pendingSelectPlay.selectedInstanceIds.push_back(
                                pendingSelectPlay.candidateInstanceIds[static_cast<size_t>(pickedHandIndex)]);
                        }
                    }
                }
                if (!pendingSelectPlay.active) {
                    // cancelled
                } else if (static_cast<int>(pendingSelectPlay.selectedInstanceIds.size()) >= pendingSelectPlay.requiredCount) {
                    // 只取前 requiredCount 个，避免异常多选
                    if (static_cast<int>(pendingSelectPlay.selectedInstanceIds.size()) > pendingSelectPlay.requiredCount) {
                        pendingSelectPlay.selectedInstanceIds.resize(static_cast<size_t>(pendingSelectPlay.requiredCount));
                    }
                    ui.set_pending_select_ui_pile_fly(static_cast<int>(pendingSelectPlay.selectedInstanceIds.size()));
                    battleEngine_.set_effect_selected_instance_ids(pendingSelectPlay.selectedInstanceIds);
                    battleEngine_.play_card(pendingSelectPlay.playHandIndex, pendingSelectPlay.playTargetMonsterIndex);
                    pendingSelectPlay.active = false;
                    pendingSelectPlay.selectedInstanceIds.clear();
                    pendingSelectPlay.candidateInstanceIds.clear();
                } else {
                    // 理论上不会到这里（UI 已限制“确定”按钮），兜底按取消处理
                    pendingSelectPlay.active = false;
                    pendingSelectPlay.selectedInstanceIds.clear();
                    pendingSelectPlay.candidateInstanceIds.clear();
                }
            }
        }

        int potionSlotIndex = -1;
        int potionTargetIndex = -1;
        if (ui.pollPotionRequest(potionSlotIndex, potionTargetIndex)) {
            battleEngine_.use_potion(potionSlotIndex, potionTargetIndex);
        }

        int deckViewMode = 0;
        if (ui.pollOpenDeckViewRequest(deckViewMode)) {
            const auto mode = static_cast<DeckViewMode>(deckViewMode);
            std::vector<CardInstance> cards = collect_deck_view_cards(cardSystem_, mode);
            if (cards.empty()) {
                ui.showTip(deck_view_empty_tip(mode));
            } else {
                ui.set_deck_view_cards(std::move(cards));
                ui.set_deck_view_active(true);
            }
        }

        if (!ui.is_deck_view_active() && !ui.is_card_select_active() && !battleEngine_.is_battle_over()) {
            battleEngine_.step_turn_phase();
        }

        BattleState state = battleEngine_.get_battle_state();
        BattleStateSnapshot snapshot = make_snapshot_from_core_refactor(state, &cardSystem_);
        tce::SnapshotBattleUIDataProvider adapter(&snapshot);
        window_.clear(sf::Color(28, 26, 32));
        ui.draw(window_, adapter);
        cheatPanel.draw(window_);
        window_.display();
        battleEngine_.tick_damage_displays();

        if (battleEngine_.is_battle_over()) {
            runningBattleScene = false;
        }
    }

    if (battleEngine_.is_battle_over() && battleEngine_.is_victory()) {
        battleEngine_.grant_victory_gold();
        std::vector<CardId> rewards = battleEngine_.get_reward_cards(3);
        if (!rewards.empty()) {
            const CardId picked = rewards[static_cast<size_t>(randomIndex(rng_, static_cast<int>(rewards.size())))];
            battleEngine_.add_card_to_master_deck(picked);
        }
        BattleState state = battleEngine_.get_battle_state();
        playerState_ = state.player;

        if (nodeType == NodeType::Boss) {
            gameCleared_ = true;
            statusText_ = "击败 Boss，通关成功！";
        } else {
            statusText_ = "战斗胜利，已发放金币与卡牌奖励。";
        }
        return true;
    }

    BattleState state = battleEngine_.get_battle_state();
    playerState_ = state.player;
    if (playerState_.currentHp <= 0) {
        gameOver_ = true;
    }
    statusText_ = "战斗失败，爬塔结束。";
    return false;
}

void GameFlowController::resolveEvent(const std::string& contentId) {
    std::string eventId = contentId;
    if (dataLayer_.get_event_by_id(eventId) == nullptr) {
        eventId = "event_001";
    }
    if (dataLayer_.get_event_by_id(eventId) == nullptr) {
        statusText_ = "事件数据缺失，跳过本节点。";
        return;
    }

    eventEngine_.start_event(eventId);
    int safety = 0;
    while (const auto* e = eventEngine_.get_current_event()) {
        if (e->options.empty()) break;
        const int pick = randomIndex(rng_, static_cast<int>(e->options.size()));
        if (!eventEngine_.choose_option(pick)) break;
        if (++safety > 8) break;
    }

    DataLayer::EventResult result{};
    if (!eventEngine_.get_event_result(result)) {
        statusText_ = "事件结束。";
        return;
    }

    eventEngine_.apply_event_result(
        result,
        [this](int gold) { playerState_.gold += gold; },
        [this](int heal) { playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + heal); });
    statusText_ = "事件结算完成。";
}

bool GameFlowController::runEventScene(const std::string& contentId) {
    std::string eventId = contentId;
    if (dataLayer_.get_event_by_id(eventId) == nullptr) {
        eventId = "event_001";
    }
    if (dataLayer_.get_event_by_id(eventId) == nullptr) {
        return false;
    }

    eventEngine_.start_event(eventId);

    EventShopRestUI ui(static_cast<unsigned>(window_.getSize().x),
                       static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") &&
        !ui.loadFont("assets/fonts/default.ttf")) {
        ui.loadFont("data/font.ttf");
    }
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                    if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
                    }
                }
            }
        }
    }

    ui.setScreen(EventShopRestScreen::Event);
    if (!eventEngine_.get_current_event()) {
        ui.setEventDataFromUtf8("（未加载事件）",
                                "请确保 data/events.json 存在且含 event_001。",
                                { "离开" },
                                "");
    }

    bool inScene = true;
    bool showingResult = false;
    const EventEngine::Event* lastDisplayedEvent = nullptr;

    auto summarizeResult = [](const DataLayer::EventResult& res) {
        if (res.type == "gold") return std::string("获得了 ") + std::to_string(res.value) + " 金币。";
        if (res.type == "heal") return std::string("恢复了 ") + std::to_string(res.value) + " 点生命。";
        if (res.type == "card_reward") return std::string("获得了一张新卡牌（选牌由主流程处理）。");
        if (res.type == "none") return std::string("无事发生。");
        return std::string("事件结束。");
    };

    while (window_.isOpen() && inScene) {
        const EventEngine::Event* current = eventEngine_.get_current_event();
        if (current && !showingResult && current != lastDisplayedEvent) {
            ui.setEventDataFromEvent(current);
            lastDisplayedEvent = current;
        }
        if (showingResult) lastDisplayedEvent = nullptr;

        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    inScene = false;
                }
            }
            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mousePos = window_.mapPixelToCoords(mp->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>()) {
                mousePos = window_.mapPixelToCoords(mr->position);
            }
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        int outIndex = -1;
        if (ui.pollEventOption(outIndex)) {
            if (showingResult) {
                if (outIndex == 0) {
                    showingResult = false;
                    if (!eventEngine_.get_current_event()) {
                        inScene = false;
                    }
                }
            } else if (eventEngine_.get_current_event() && eventEngine_.choose_option(outIndex)) {
                DataLayer::EventResult res{};
                if (eventEngine_.get_event_result(res)) {
                    eventEngine_.apply_event_result(
                        res,
                        [this](int gold) { playerState_.gold += gold; },
                        [this](int heal) {
                            playerState_.currentHp =
                                std::min(playerState_.maxHp, playerState_.currentHp + heal);
                        });
                    ui.setEventResultFromUtf8(summarizeResult(res));
                    showingResult = true;
                } else {
                    inScene = false;
                }
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        ui.draw(window_);
        window_.display();
    }

    return true;
}

void GameFlowController::resolveRest() {
    const int heal = std::max(12, playerState_.maxHp / 5);
    playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + heal);
    statusText_ = "在休息点恢复了生命值。";
}

void GameFlowController::resolveMerchant() {
    if (playerState_.gold < 50) {
        statusText_ = "金币不足，离开商店。";
        return;
    }
    static const std::vector<CardId> shopCards = { "iron_wave", "cleave", "shrug_it_off", "quick_slash" };
    const CardId picked = shopCards[static_cast<size_t>(randomIndex(rng_, static_cast<int>(shopCards.size())))];
    cardSystem_.add_to_master_deck(picked);
    playerState_.gold -= 50;
    statusText_ = "商店购买 1 张牌（-50 金币）。";
}

void GameFlowController::resolveTreasure() {
    const int gold = 40 + randomIndex(rng_, 61);
    playerState_.gold += gold;
    statusText_ = "宝箱节点：获得金币。";
}

bool GameFlowController::runShopScene() {
    EventShopRestUI ui(static_cast<unsigned>(window_.getSize().x),
                       static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") &&
        !ui.loadFont("assets/fonts/default.ttf")) {
        ui.loadFont("data/font.ttf");
    }
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                    if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
                    }
                }
            }
        }
    }

    ShopDisplayData shop{};
    shop.playerGold = playerState_.gold;

    // 简单示例：给出几张固定卡牌作为出售列表
    auto pushOffer = [](ShopDisplayData& s, const std::string& id,
                        const std::wstring& name, int price) {
        ShopCardOffer c;
        c.id = id;
        c.name = name;
        c.price = price;
        s.forSale.push_back(c);
    };
    pushOffer(shop, "iron_wave", L"铁斩波", 50);
    pushOffer(shop, "cleave", L"顺劈斩", 60);
    pushOffer(shop, "shrug_it_off", L"耸肩无视", 45);

    // 当前 master deck 用于删牌列表
    for (const auto& inst : cardSystem_.get_master_deck()) {
        MasterDeckCardDisplay d;
        d.instanceId = inst.instanceId;
        const CardData* data = get_card_by_id(inst.id);
        std::string name = data ? data->name : inst.id;
        d.cardName = std::wstring(name.begin(), name.end());
        shop.deckForRemove.push_back(d);
    }

    ui.setShopData(shop);
    ui.setScreen(EventShopRestScreen::Shop);

    bool inScene = true;
    while (window_.isOpen() && inScene) {
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    inScene = false;
                }
            }
            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        // 购买卡牌
        CardId buyId;
        if (ui.pollShopBuyCard(buyId)) {
            const CardData* cardData = get_card_by_id(buyId);
            int price = 0;
            for (const auto& offer : shop.forSale) {
                if (offer.id == buyId) {
                    price = offer.price;
                    break;
                }
            }
            if (cardData && price > 0 && playerState_.gold >= price) {
                playerState_.gold -= price;
                cardSystem_.add_to_master_deck(buyId);
                shop.playerGold = playerState_.gold;
                ui.setShopData(shop);
            }
        }

        // 删除牌
        InstanceId removeId = 0;
        if (ui.pollShopRemoveCard(removeId)) {
            CardId removedId;
            if (removeId > 0 && cardSystem_.remove_from_master_deck(removeId, &removedId)) {
                if (removedId == "parasite" || removedId == "parasite+") {
                    playerState_.maxHp = std::max(1, playerState_.maxHp - 3);
                    if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                }
                auto& list = shop.deckForRemove;
                list.erase(std::remove_if(list.begin(), list.end(),
                                          [removeId](const MasterDeckCardDisplay& d) {
                                              return d.instanceId == removeId;
                                          }),
                           list.end());
                ui.setShopData(shop);
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        ui.draw(window_);
        window_.display();
    }

    return true;
}

bool GameFlowController::runRestScene() {
    EventShopRestUI ui(static_cast<unsigned>(window_.getSize().x),
                       static_cast<unsigned>(window_.getSize().y));
    if (!ui.loadFont("assets/fonts/Sanji.ttf") &&
        !ui.loadFont("assets/fonts/default.ttf")) {
        ui.loadFont("data/font.ttf");
    }
    if (!ui.loadChineseFont("assets/fonts/simkai.ttf")) {
        if (!ui.loadChineseFont("assets/fonts/Sanji.ttf")) {
            if (!ui.loadChineseFont("C:/Windows/Fonts/simkai.ttf")) {
                if (!ui.loadChineseFont("C:/Windows/Fonts/msyh.ttc")) {
                    if (!ui.loadChineseFont("C:/Windows/Fonts/simhei.ttf")) {
                        ui.loadChineseFont("C:/Windows/Fonts/simsun.ttc");
                    }
                }
            }
        }
    }

    RestDisplayData rest{};
    rest.healAmount = std::max(12, playerState_.maxHp / 5);
    for (const auto& inst : cardSystem_.get_master_deck()) {
        MasterDeckCardDisplay d;
        d.instanceId = inst.instanceId;
        const CardData* data = get_card_by_id(inst.id);
        std::string name = data ? data->name : inst.id;
        d.cardName = std::wstring(name.begin(), name.end());
        rest.deckForUpgrade.push_back(d);
    }

    ui.setRestData(rest);
    ui.setScreen(EventShopRestScreen::Rest);

    bool inScene = true;
    while (window_.isOpen() && inScene) {
        while (const std::optional ev = window_.pollEvent()) {
            if (ev->is<sf::Event::Closed>()) {
                window_.close();
                return false;
            }
            if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                if (key->scancode == sf::Keyboard::Scancode::Escape) {
                    inScene = false;
                }
            }
            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            if (const auto* mp = ev->getIf<sf::Event::MouseButtonPressed>()) {
                mousePos = window_.mapPixelToCoords(mp->position);
            } else if (const auto* mr = ev->getIf<sf::Event::MouseButtonReleased>()) {
                mousePos = window_.mapPixelToCoords(mr->position);
            }
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        if (ui.pollRestHeal()) {
            playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + rest.healAmount);
            inScene = false;
        }

        InstanceId upgradeId = 0;
        if (ui.pollRestUpgradeCard(upgradeId)) {
            if (upgradeId > 0 && cardSystem_.upgrade_card_in_master_deck(upgradeId)) {
                // 刷新升级列表显示名称
                rest.deckForUpgrade.clear();
                for (const auto& inst : cardSystem_.get_master_deck()) {
                    MasterDeckCardDisplay d;
                    d.instanceId = inst.instanceId;
                    const CardData* data = get_card_by_id(inst.id);
                    std::string name = data ? data->name : inst.id;
                    d.cardName = std::wstring(name.begin(), name.end());
                    rest.deckForUpgrade.push_back(d);
                }
                ui.setRestData(rest);
                inScene = false;
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        ui.draw(window_);
        window_.display();
    }

    return true;
}

int GameFlowController::firstAliveMonsterIndex(const BattleState& state) const {
    for (int i = 0; i < static_cast<int>(state.monsters.size()); ++i) {
        if (state.monsters[static_cast<size_t>(i)].currentHp > 0) return i;
    }
    return -1;
}

void GameFlowController::drawHud() {
    if (!hudFontLoaded_) return;

    sf::Text text(hudFont_);
    text.setCharacterSize(24);
    text.setFillColor(sf::Color::Black);
    text.setPosition({ 20.f, 16.f });

    std::string stageText = "进行中";
    if (gameOver_) stageText = "失败";
    if (gameCleared_) stageText = "通关";

    const std::string hud =
        "HP: " + std::to_string(playerState_.currentHp) + "/" + std::to_string(playerState_.maxHp) +
        "   Gold: " + std::to_string(playerState_.gold) +
        "   Deck: " + std::to_string(cardSystem_.get_master_deck().size()) +
        "   状态: " + stageText +
        "\n" + statusText_;
    text.setString(sf::String::fromUtf8(hud.begin(), hud.end()));

    window_.draw(text);
}

std::string GameFlowController::nodeTypeToString(NodeType nodeType) const {
    switch (nodeType) {
    case NodeType::Enemy: return "Enemy";
    case NodeType::Elite: return "Elite";
    case NodeType::Event: return "Event";
    case NodeType::Rest: return "Rest";
    case NodeType::Merchant: return "Merchant";
    case NodeType::Treasure: return "Treasure";
    case NodeType::Boss: return "Boss";
    default: return "Unknown";
    }
}

} // namespace tce
