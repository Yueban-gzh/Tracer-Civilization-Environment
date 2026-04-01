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
#include "EventEngine/EventShopRestUICommon.hpp"

namespace tce {

namespace {
int randomIndex(RunRng& rng, int boundExclusive) {
    if (boundExclusive <= 0) return 0;
    return rng.uniform_int(0, boundExclusive - 1);
}

std::string join_names_cn(const std::vector<std::string>& names) {
    if (names.empty()) return "无";
    std::string out;
    for (size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out += "、";
        out += names[i];
    }
    return out;
}

std::string potion_name_cn(const std::string& id) {
    if (id == "block_potion") return "砌墙灵液";
    if (id == "strength_potion") return "蛮力灵液";
    if (id == "poison_potion") return "淬毒灵液";
    return id;
}

std::string relic_name_cn(const std::string& id) {
    if (id == "burning_blood") return "燃烧之血";
    if (id == "marble_bag") return "弹珠袋";
    if (id == "small_blood_vial") return "小血瓶";
    if (id == "copper_scales") return "铜制鳞片";
    if (id == "centennial_puzzle") return "百年积木";
    if (id == "clockwork_boots") return "发条靴";
    if (id == "happy_flower") return "开心小花";
    if (id == "lantern") return "灯笼";
    if (id == "smooth_stone") return "意外光滑的石头";
    if (id == "orichalcum") return "奥利哈钢";
    if (id == "red_skull") return "红头骨";
    if (id == "snake_skull") return "异蛇头骨";
    if (id == "strawberry") return "草莓";
    if (id == "potion_belt") return "药水腰带";
    if (id == "vajra") return "金刚杵";
    if (id == "nunchaku") return "双截棍";
    if (id == "ceramic_fish") return "陶瓷小鱼";
    if (id == "hand_drum") return "手摇鼓";
    if (id == "pen_nib") return "钢笔尖";
    if (id == "toy_ornithopter") return "玩具扑翼飞机";
    if (id == "preparation_pack") return "准备背包";
    if (id == "anchor") return "锚";
    if (id == "art_of_war") return "孙子兵法";
    if (id == "relic_strength_plus") return "力量遗物";
    return id;
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
    , runRng_(static_cast<uint64_t>(std::time(nullptr)))
    , cardSystem_([](CardId id) { return get_card_by_id(id); }, &runRng_)
    , battleEngine_(
        cardSystem_,
        [](MonsterId id) { return get_monster_by_id(id); },
        [](CardId id) { return get_card_by_id(id); },
        execute_monster_behavior,
        &runRng_)
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
        [this](InstanceId id) { return cardSystem_.upgrade_card_in_master_deck(id); }) {}

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
    mapEngine_.set_run_rng(&runRng_);
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
        const int count = 1 + randomIndex(runRng_, 3);
        for (int i = 0; i < count; ++i) {
            monsters.push_back(normalPool[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(normalPool.size())))]);
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
            const CardId picked = rewards[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(rewards.size())))];
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
        const int pick = randomIndex(runRng_, static_cast<int>(e->options.size()));
        if (!eventEngine_.choose_option(pick)) break;
        if (++safety > 8) break;
    }

    DataLayer::EventResult result{};
    if (!eventEngine_.get_event_result(result)) {
        statusText_ = "事件结束。";
        return;
    }

    const auto applyOneEffect = [&](const std::string& type, int value) {
        if (type == "gold") {
            playerState_.gold += value;
        } else if (type == "heal") {
            playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + value);
        } else if (type == "max_hp") {
            playerState_.maxHp += value;
            if (playerState_.maxHp < 1) playerState_.maxHp = 1;
            if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
        } else if (type == "card_reward") {
            const int count = std::max(0, value);
            if (count > 0) {
                const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
                std::vector<CardId> cards = battleEngine_.get_reward_cards(count);
                for (const auto& cid : cards) {
                    cardSystem_.add_to_master_deck(cid);
                    if (hasCeramicFish) playerState_.gold += 9; // 陶瓷小鱼：每次加入牌组获得 9 金币
                }
            }
        } else if (type == "card_reward_choose") {
            // 非交互结算路径（自动 resolveEvent）：按 value 选择张数，候选池更大一些
            const int chooseCount = std::max(1, value);
            const int candidateCount = std::max(3, chooseCount + 1);
            const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
            std::vector<CardId> cards = battleEngine_.get_reward_cards(candidateCount);
            for (int i = 0; i < chooseCount; ++i) {
                if (cards.empty()) break;
                const int pick = randomIndex(runRng_, static_cast<int>(cards.size()));
                cardSystem_.add_to_master_deck(cards[static_cast<size_t>(pick)]);
                if (hasCeramicFish) playerState_.gold += 9;
                cards.erase(cards.begin() + pick); // 无放回，避免重复
            }
        } else if (type == "add_curse") {
            const int count = std::max(0, value);
            for (int i = 0; i < count; ++i) {
                cardSystem_.add_to_master_deck("parasite");
            }
        } else if (type == "remove_card") {
            const int count = std::max(0, value);
            for (int i = 0; i < count; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;
                const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                eventEngine_.remove_card_from_master_deck(deck[static_cast<size_t>(idx)].instanceId);
            }
        } else if (type == "remove_card_choose") {
            // 非交互结算路径（自动 resolveEvent）：退化为随机移除
            const int count = std::max(1, value);
            for (int i = 0; i < count; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;
                const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                eventEngine_.remove_card_from_master_deck(deck[static_cast<size_t>(idx)].instanceId);
            }
        } else if (type == "remove_curse") {
            const int count = std::max(0, value);
            for (int i = 0; i < count; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;

                std::vector<InstanceId> candidates;
                for (const auto& inst : deck) {
                    if (inst.id == "parasite" || inst.id == "parasite+") candidates.push_back(inst.instanceId);
                }
                if (candidates.empty()) {
                    const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                    candidates.push_back(deck[static_cast<size_t>(idx)].instanceId);
                }
                const InstanceId picked = candidates[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(candidates.size())))];
                eventEngine_.remove_card_from_master_deck(picked);
            }
        } else if (type == "upgrade_random") {
            const int attempts = std::max(0, value);
            for (int i = 0; i < attempts; ++i) {
                const auto& deck = cardSystem_.get_master_deck();
                if (deck.empty()) break;
                const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                cardSystem_.upgrade_card_in_master_deck(deck[static_cast<size_t>(idx)].instanceId);
            }
        } else if (type == "relic") {
            const int count = std::max(0, value);
            static const std::vector<RelicId> relic_pool = {
                "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle",
                "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull",
                "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum",
                "pen_nib", "toy_ornithopter", "preparation_pack", "anchor", "art_of_war", "relic_strength_plus"
            };
            for (int i = 0; i < count; ++i) {
                std::vector<RelicId> available;
                for (const auto& rid : relic_pool) {
                    if (std::find(playerState_.relics.begin(), playerState_.relics.end(), rid) == playerState_.relics.end())
                        available.push_back(rid);
                }
                if (available.empty()) break;
                const RelicId gained = available[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(available.size())))];
                playerState_.relics.push_back(gained);

                // 拾起即刻效果（目前仅实现事件用到的少数遗物）
                if (gained == "strawberry") {
                    playerState_.maxHp += 7;
                    playerState_.currentHp += 7;
                    if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                } else if (gained == "potion_belt") {
                    playerState_.potionSlotCount += 2;
                }
            }
        }

        if (playerState_.currentHp <= 0) gameOver_ = true;
    };

    const auto applyEventResult = [&](const DataLayer::EventResult& res) {
        if (!res.effects.empty()) {
            for (const auto& eff : res.effects) {
                if (!gameOver_) applyOneEffect(eff.type, eff.value);
            }
            return;
        }
        applyOneEffect(res.type, res.value);
    };

    applyEventResult(result);
    statusText_ = "事件结算完成。";
}

bool GameFlowController::runEventScene(const std::string& contentId) {
    // 问号房：按“当前地图层 index”从事件池做加权随机抽取（模仿杀戮尖塔问号房高波动事件池）。
    // 本项目 MapEngine 的默认 content_id 形如：content_{typeInt}_{layer-index}
    int layer = 0;
    {
        const size_t pos = contentId.rfind('_');
        if (pos != std::string::npos) {
            const std::string tail = contentId.substr(pos + 1); // "layer-index"
            const size_t dash = tail.find('-');
            if (dash != std::string::npos) {
                try { layer = std::stoi(tail.substr(0, dash)); }
                catch (...) { layer = 0; }
            }
        }
    }

    std::vector<DataLayer::RootEventCandidate> candidates =
        dataLayer_.get_root_event_candidates_for_layer(layer);

    std::string eventId;
    if (!candidates.empty()) {
        int totalWeight = 0;
        for (const auto& c : candidates) totalWeight += std::max(1, c.weight);
        if (totalWeight <= 0) totalWeight = 1;

        int pick = randomIndex(runRng_, totalWeight);
        for (const auto& c : candidates) {
            const int w = std::max(1, c.weight);
            if (pick < w) { eventId = c.id; break; }
            pick -= w;
        }
    }

    if (eventId.empty()) eventId = "event_001";
    if (dataLayer_.get_event_by_id(eventId) == nullptr) return false;

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
                                "请确保 data/events.json 存在且含根事件（如 event_001）。",
                                { "离开" },
                                "");
    }

    bool inScene = true;
    bool showingResult = false;
    const EventEngine::Event* lastDisplayedEvent = nullptr;
    std::vector<std::string> resultCardPreviewIds;

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
        if (eff.type == "add_curse") return std::string("获得了 ") + std::to_string(v) + " 张诅咒（寄生）牌。";
        if (eff.type == "remove_card") return std::string("移除了牌组中的 ") + std::to_string(v) + " 张卡牌。";
        if (eff.type == "remove_card_choose") return std::string("自选移除牌组中的 ") + std::to_string(std::max(1, v)) + " 张卡牌。";
        if (eff.type == "remove_curse") return std::string("移除了 ") + std::to_string(v) + " 张诅咒（寄生）牌。";
        if (eff.type == "upgrade_random") return std::string("升级了 ") + std::to_string(v) + " 张卡牌。";
        if (eff.type == "relic") return std::string("获得了 ") + std::to_string(v) + " 个遗物。";
        if (eff.type == "none") return std::string("无事发生。");
        return std::string("事件结算：")+ eff.type + "（x" + std::to_string(v) + "）。";
    };

    auto summarizeResult = [&](const DataLayer::EventResult& res) {
        std::string out;
        if (!res.effects.empty()) {
            for (size_t i = 0; i < res.effects.size(); ++i) {
                if (i > 0) out += " ";
                out += effectToSummary(res.effects[i]);
            }
            return out.empty() ? std::string("无事发生。") : out;
        }
        // 兼容旧数据：退化到 type/value
        DataLayer::EventEffect eff{ res.type, res.value };
        return effectToSummary(eff);
    };

    // 事件中的“自选”交互：复用事件面板按钮，返回被选中的索引（取消/关闭返回 -1）
    auto pickFromEventOptions = [&](const std::string& title,
                                    const std::string& desc,
                                    const std::vector<std::string>& options,
                                    const std::vector<std::string>& optionEffects = std::vector<std::string>{}) -> int {
        if (options.empty()) return -1;
        ui.setEventDataFromUtf8(title, desc, options, "", optionEffects);
        while (window_.isOpen()) {
            while (const std::optional ev = window_.pollEvent()) {
                if (ev->is<sf::Event::Closed>()) {
                    window_.close();
                    return -1;
                }
                if (const auto* key = ev->getIf<sf::Event::KeyPressed>()) {
                    if (key->scancode == sf::Keyboard::Scancode::Escape) return -1;
                }
                sf::Vector2f mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
                if (const auto* m1 = ev->getIf<sf::Event::MouseButtonPressed>()) {
                    mp = window_.mapPixelToCoords(m1->position);
                } else if (const auto* m2 = ev->getIf<sf::Event::MouseButtonReleased>()) {
                    mp = window_.mapPixelToCoords(m2->position);
                }
                ui.handleEvent(*ev, mp);
            }

            sf::Vector2f mp = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            ui.setMousePosition(mp);
            int picked = -1;
            if (ui.pollEventOption(picked)) return picked;

            window_.clear(sf::Color(40, 38, 45));
            ui.draw(window_);
            window_.display();
        }
        return -1;
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
                        std::vector<std::string> detailMessages;
                        const auto pushCardPreview = [&](const std::string& cid) {
                            if (cid.empty()) return;
                            if (resultCardPreviewIds.size() < 4) resultCardPreviewIds.push_back(cid);
                        };
                        const auto pushDetail = [&](const std::string& msg) {
                            if (!msg.empty()) detailMessages.push_back(msg);
                        };
                        const auto applyOneEffect = [&](const std::string& type, int value) {
                            if (type == "gold") {
                                playerState_.gold += value;
                            } else if (type == "heal") {
                                playerState_.currentHp = std::min(playerState_.maxHp, playerState_.currentHp + value);
                            } else if (type == "max_hp") {
                                playerState_.maxHp += value;
                                if (playerState_.maxHp < 1) playerState_.maxHp = 1;
                                if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                            } else if (type == "card_reward") {
                                const int count = std::min(4, std::max(0, value));
                                if (count > 0) {
                                    const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
                                    std::vector<CardId> cards = battleEngine_.get_reward_cards(count);
                                    std::vector<std::string> gained;
                                    for (const auto& cid : cards) {
                                        cardSystem_.add_to_master_deck(cid);
                                        pushCardPreview(cid);
                                        const CardData* cd = get_card_by_id(cid);
                                        gained.push_back(cd ? cd->name : cid);
                                        if (hasCeramicFish) playerState_.gold += 9; // 陶瓷小鱼：每次加入牌组获得 9 金币
                                    }
                                    pushDetail("获得卡牌：" + join_names_cn(gained));
                                }
                            } else if (type == "card_reward_choose") {
                                const int chooseCount = std::min(4, std::max(1, value));
                                const int candidateCount = std::max(3, chooseCount + 1);
                                const bool hasCeramicFish = std::find(playerState_.relics.begin(), playerState_.relics.end(), "ceramic_fish") != playerState_.relics.end();
                                std::vector<CardId> cards = battleEngine_.get_reward_cards(candidateCount);
                                std::vector<std::string> chosenNames;
                                for (int chooseIdx = 0; chooseIdx < chooseCount; ++chooseIdx) {
                                    if (cards.empty()) break;
                                    std::vector<std::string> optionTexts;
                                    std::vector<std::string> optionEffects;
                                    optionTexts.reserve(cards.size());
                                    optionEffects.reserve(cards.size());
                                    for (const auto& cid : cards) {
                                        const CardData* cd = get_card_by_id(cid);
                                        optionTexts.push_back(cd ? cd->name : cid);
                                        if (cd) {
                                            optionEffects.push_back("费" + std::to_string(std::max(0, cd->cost)) + "｜" + cd->description);
                                        } else {
                                            optionEffects.push_back("未知卡牌效果");
                                        }
                                    }
                                    const int pick = pickFromEventOptions(
                                        "择术",
                                        "请选择 1 张要加入牌组的卡牌",
                                        optionTexts,
                                        optionEffects);
                                    if (pick < 0 || pick >= static_cast<int>(cards.size())) break;
                                    const CardId chosen = cards[static_cast<size_t>(pick)];
                                    cardSystem_.add_to_master_deck(chosen);
                                    pushCardPreview(chosen);
                                    const CardData* chosenCd = get_card_by_id(chosen);
                                    chosenNames.push_back(chosenCd ? chosenCd->name : chosen);
                                    if (hasCeramicFish) playerState_.gold += 9;
                                    cards.erase(cards.begin() + pick); // 无放回，避免重复
                                }
                                pushDetail("自选获得卡牌：" + join_names_cn(chosenNames));
                            } else if (type == "add_curse") {
                                const int count = std::min(4, std::max(0, value));
                                for (int i = 0; i < count; ++i) {
                                    cardSystem_.add_to_master_deck("parasite");
                                    pushCardPreview("parasite");
                                }
                                pushDetail("获得诅咒牌：寄生 x" + std::to_string(count));
                            } else if (type == "remove_card") {
                                const int count = std::min(4, std::max(0, value));
                                std::vector<std::string> removedNames;
                                for (int i = 0; i < count; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;
                                    const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                                    const auto& inst = deck[static_cast<size_t>(idx)];
                                    pushCardPreview(inst.id);
                                    const CardData* cd = get_card_by_id(inst.id);
                                    removedNames.push_back(cd ? cd->name : inst.id);
                                    eventEngine_.remove_card_from_master_deck(inst.instanceId);
                                }
                                pushDetail("移除卡牌：" + join_names_cn(removedNames));
                            } else if (type == "remove_card_choose") {
                                const int count = std::min(4, std::max(1, value));
                                std::vector<std::string> removedNames;
                                for (int i = 0; i < count; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;
                                    std::vector<InstanceId> ids;
                                    std::vector<std::string> names;
                                    std::vector<std::string> effects;
                                    ids.reserve(deck.size());
                                    names.reserve(deck.size());
                                    effects.reserve(deck.size());
                                    for (const auto& inst : deck) {
                                        ids.push_back(inst.instanceId);
                                        const CardData* cd = get_card_by_id(inst.id);
                                        names.push_back(cd ? cd->name : inst.id);
                                        if (cd) {
                                            effects.push_back("费" + std::to_string(std::max(0, cd->cost)) + "｜" + cd->description);
                                        } else {
                                            effects.push_back("未知卡牌效果");
                                        }
                                    }
                                    const int pick = pickFromEventOptions(
                                        "断舍",
                                        "请选择 1 张要移除的卡牌",
                                        names,
                                        effects);
                                    if (pick < 0 || pick >= static_cast<int>(ids.size())) break;
                                    const InstanceId chosenInstId = ids[static_cast<size_t>(pick)];
                                    for (const auto& inst : deck) {
                                        if (inst.instanceId == chosenInstId) {
                                            pushCardPreview(inst.id);
                                            const CardData* cd = get_card_by_id(inst.id);
                                            removedNames.push_back(cd ? cd->name : inst.id);
                                            break;
                                        }
                                    }
                                    eventEngine_.remove_card_from_master_deck(chosenInstId);
                                }
                                pushDetail("自选移除卡牌：" + join_names_cn(removedNames));
                            } else if (type == "remove_curse") {
                                const int count = std::min(4, std::max(0, value));
                                std::vector<std::string> removedNames;
                                for (int i = 0; i < count; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;

                                    std::vector<InstanceId> candidates;
                                    for (const auto& inst : deck) {
                                        if (inst.id == "parasite" || inst.id == "parasite+") candidates.push_back(inst.instanceId);
                                    }
                                    if (candidates.empty()) {
                                        const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                                        candidates.push_back(deck[static_cast<size_t>(idx)].instanceId);
                                    }
                                    const InstanceId picked = candidates[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(candidates.size())))];
                                    for (const auto& inst : deck) {
                                        if (inst.instanceId == picked) {
                                            pushCardPreview(inst.id);
                                            const CardData* cd = get_card_by_id(inst.id);
                                            removedNames.push_back(cd ? cd->name : inst.id);
                                            break;
                                        }
                                    }
                                    eventEngine_.remove_card_from_master_deck(picked);
                                }
                                pushDetail("移除诅咒相关卡牌：" + join_names_cn(removedNames));
                            } else if (type == "upgrade_random") {
                                const int attempts = std::min(4, std::max(0, value));
                                std::vector<std::string> upgradedNames;
                                for (int i = 0; i < attempts; ++i) {
                                    const auto& deck = cardSystem_.get_master_deck();
                                    if (deck.empty()) break;
                                    bool useChooseUpgrade = (randomIndex(runRng_, 100) < 45); // 一定比率使用自选升级
                                    if (useChooseUpgrade) {
                                        std::vector<InstanceId> ids;
                                        std::vector<std::string> names;
                                        std::vector<std::string> effects;
                                        ids.reserve(deck.size());
                                        names.reserve(deck.size());
                                        effects.reserve(deck.size());
                                        for (const auto& inst : deck) {
                                            ids.push_back(inst.instanceId);
                                            const CardData* cd = get_card_by_id(inst.id);
                                            names.push_back(cd ? cd->name : inst.id);
                                            if (cd) effects.push_back("费" + std::to_string(std::max(0, cd->cost)) + "｜" + cd->description);
                                            else effects.push_back("未知卡牌效果");
                                        }
                                        const int pick = pickFromEventOptions(
                                            "精修",
                                            "请选择 1 张要升级的卡牌",
                                            names,
                                            effects);
                                        if (pick >= 0 && pick < static_cast<int>(ids.size())) {
                                            const InstanceId iid = ids[static_cast<size_t>(pick)];
                                            for (const auto& inst : deck) {
                                                if (inst.instanceId == iid) {
                                                    pushCardPreview(inst.id);
                                                    const CardData* cd = get_card_by_id(inst.id);
                                                    upgradedNames.push_back(cd ? cd->name : inst.id);
                                                    break;
                                                }
                                            }
                                            cardSystem_.upgrade_card_in_master_deck(iid);
                                            continue;
                                        }
                                    }
                                    const int idx = randomIndex(runRng_, static_cast<int>(deck.size()));
                                    const auto& inst = deck[static_cast<size_t>(idx)];
                                    pushCardPreview(inst.id);
                                    const CardData* cd = get_card_by_id(inst.id);
                                    upgradedNames.push_back(cd ? cd->name : inst.id);
                                    cardSystem_.upgrade_card_in_master_deck(inst.instanceId);
                                }
                                pushDetail("升级卡牌：" + join_names_cn(upgradedNames));
                            } else if (type == "relic") {
                                const int count = std::max(0, value);
                                static const std::vector<RelicId> relic_pool = {
                                    "burning_blood", "marble_bag", "small_blood_vial", "copper_scales", "centennial_puzzle",
                                    "clockwork_boots", "happy_flower", "lantern", "smooth_stone", "orichalcum", "red_skull",
                                    "snake_skull", "strawberry", "potion_belt", "vajra", "nunchaku", "ceramic_fish", "hand_drum",
                                    "pen_nib", "toy_ornithopter", "preparation_pack", "anchor", "art_of_war", "relic_strength_plus"
                                };
                                std::vector<std::string> gainedRelics;
                                for (int i = 0; i < count; ++i) {
                                    std::vector<RelicId> available;
                                    for (const auto& rid : relic_pool) {
                                        if (std::find(playerState_.relics.begin(), playerState_.relics.end(), rid) == playerState_.relics.end())
                                            available.push_back(rid);
                                    }
                                    if (available.empty()) break;
                                    const RelicId gained = available[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(available.size())))];
                                    playerState_.relics.push_back(gained);
                                    gainedRelics.push_back(relic_name_cn(gained));

                                    if (gained == "strawberry") {
                                        playerState_.maxHp += 7;
                                        playerState_.currentHp += 7;
                                        if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                                    } else if (gained == "potion_belt") {
                                        playerState_.potionSlotCount += 2;
                                    }
                                }
                                pushDetail("获得遗物：" + join_names_cn(gainedRelics));
                            }
                            if (playerState_.currentHp <= 0) gameOver_ = true;
                        };

                        const auto applyEventResult = [&](const DataLayer::EventResult& r) {
                            if (!r.effects.empty()) {
                                for (const auto& eff : r.effects) {
                                    if (gameOver_) break;
                                    applyOneEffect(eff.type, eff.value);
                                }
                                return;
                            }
                            applyOneEffect(r.type, r.value);
                        };

                        resultCardPreviewIds.clear();
                        applyEventResult(res);
                        // 结果区最多展示 4 张：两列网格
                        std::vector<std::string> uniqueIds;
                        uniqueIds.reserve(resultCardPreviewIds.size());
                        for (const auto& id : resultCardPreviewIds) {
                            if (id.empty()) continue;
                            if (std::find(uniqueIds.begin(), uniqueIds.end(), id) == uniqueIds.end()) {
                                uniqueIds.push_back(id);
                            }
                            if (uniqueIds.size() >= 4) break;
                        }
                        std::string resultImage;
                        if (!uniqueIds.empty()) {
                            resultImage = "__cards:";
                            for (size_t i = 0; i < uniqueIds.size(); ++i) {
                                if (i > 0) resultImage += "|";
                                resultImage += uniqueIds[i];
                            }
                        }
                        std::string finalSummary = summarizeResult(res);
                        if (!detailMessages.empty()) {
                            finalSummary += "\n";
                            for (size_t i = 0; i < detailMessages.size(); ++i) {
                                if (i > 0) finalSummary += "\n";
                                finalSummary += "· " + detailMessages[i];
                            }
                        }
                    ui.setEventResultFromUtf8(finalSummary, resultImage);
                    showingResult = true;
                        if (gameOver_) inScene = false;
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
    const CardId picked = shopCards[static_cast<size_t>(randomIndex(runRng_, static_cast<int>(shopCards.size())))];
    cardSystem_.add_to_master_deck(picked);
    playerState_.gold -= 50;
    statusText_ = "商店购买 1 张牌（-50 金币）。";
}

void GameFlowController::resolveTreasure() {
    const int gold = 40 + randomIndex(runRng_, 61);
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
    shop.playerCurrentHp = playerState_.currentHp;
    shop.playerMaxHp = playerState_.maxHp;
    shop.potionSlotsMax = playerState_.potionSlotCount;
    shop.potionSlotsUsed = static_cast<int>(playerState_.potions.size());
    shop.chapterLine = L"先秦溯源 · 行旅";
    shop.removeServicePrice = 75;
    if (!playerState_.playerName.empty())
        shop.playerTitle = esr_detail::utf8_to_wstring(playerState_.playerName);

    auto refreshDeckList = [&]() {
        shop.deckForRemove.clear();
        for (const auto& inst : cardSystem_.get_master_deck()) {
            MasterDeckCardDisplay d;
            d.instanceId = inst.instanceId;
            d.cardId = inst.id;
            const CardData* data = get_card_by_id(inst.id);
            d.cardName = data ? esr_detail::utf8_to_wstring(data->name) : esr_detail::utf8_to_wstring(inst.id);
            shop.deckForRemove.push_back(d);
        }
    };

    static const std::vector<std::pair<std::string, int>> kShopCards = {
        {"iron_wave", 50}, {"cleave", 60}, {"shrug_it_off", 45}, {"quick_slash", 55}, {"strike", 40},
    };
    for (const auto& [cid, price] : kShopCards) {
        ShopCardOffer c;
        c.id = cid;
        c.price = price;
        const CardData* cd = get_card_by_id(cid);
        c.name = cd ? esr_detail::utf8_to_wstring(cd->name) : esr_detail::utf8_to_wstring(cid);
        shop.forSale.push_back(c);
    }

    static const std::vector<std::pair<std::string, int>> kShopColorless = {
        {"card_001", 85}, {"card_007", 55},
    };
    for (const auto& [cid, price] : kShopColorless) {
        ShopCardOffer c;
        c.id = cid;
        c.price = price;
        const CardData* cd = get_card_by_id(cid);
        c.name = cd ? esr_detail::utf8_to_wstring(cd->name) : esr_detail::utf8_to_wstring(cid);
        shop.colorlessForSale.push_back(c);
    }

    static const std::vector<std::pair<std::string, int>> kShopRelics = {
        {"vajra", 180}, {"anchor", 160}, {"strawberry", 120},
    };
    for (const auto& [rid, price] : kShopRelics) {
        if (std::find(playerState_.relics.begin(), playerState_.relics.end(), rid) != playerState_.relics.end())
            continue;
        ShopRelicOffer r;
        r.id = rid;
        r.price = price;
        r.name = esr_detail::utf8_to_wstring(relic_name_cn(rid));
        shop.relicsForSale.push_back(r);
    }

    static const std::vector<std::pair<std::string, int>> kShopPotions = {
        {"block_potion", 40}, {"strength_potion", 45}, {"poison_potion", 50},
    };
    for (const auto& [pid, price] : kShopPotions) {
        ShopPotionOffer p;
        p.id = pid;
        p.price = price;
        p.name = esr_detail::utf8_to_wstring(potion_name_cn(pid));
        shop.potionsForSale.push_back(p);
    }

    refreshDeckList();

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
                    if (!ui.tryDismissShopRemoveConfirm()) inScene = false;
                }
            }
            sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
            ui.handleEvent(*ev, mousePos);
        }

        sf::Vector2f mousePos = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        ui.setMousePosition(mousePos);

        if (ui.pollShopLeave()) {
            inScene = false;
        }

        if (ui.pollShopPayRemoveService()) {
            if (!shop.removeServiceSoldOut && !shop.removeServicePaid &&
                playerState_.gold >= shop.removeServicePrice) {
                // 只要付得起净简服务，就进入“选择要删除的牌”界面；
                // 若牌组为空，界面会显示提示但仍能正确跳转。
                playerState_.gold -= shop.removeServicePrice;
                shop.removeServicePaid = true;
                shop.playerGold = playerState_.gold;
                statusText_ = "请选择一张牌进行净简移除。";
                ui.setShopData(shop);
            }
        }

        CardId buyId;
        if (ui.pollShopBuyCard(buyId)) {
            int price = 0;
            for (const auto& offer : shop.forSale) {
                if (offer.id == buyId) {
                    price = offer.price;
                    break;
                }
            }
            if (price == 0) {
                for (const auto& offer : shop.colorlessForSale) {
                    if (offer.id == buyId) {
                        price = offer.price;
                        break;
                    }
                }
            }
            const CardData* cardData = get_card_by_id(buyId);
            if (cardData && price > 0 && playerState_.gold >= price) {
                playerState_.gold -= price;
                cardSystem_.add_to_master_deck(buyId);
                shop.playerGold = playerState_.gold;
                shop.forSale.erase(
                    std::remove_if(shop.forSale.begin(), shop.forSale.end(),
                                   [&buyId](const ShopCardOffer& o) { return o.id == buyId; }),
                    shop.forSale.end());
                shop.colorlessForSale.erase(
                    std::remove_if(shop.colorlessForSale.begin(), shop.colorlessForSale.end(),
                                   [&buyId](const ShopCardOffer& o) { return o.id == buyId; }),
                    shop.colorlessForSale.end());
                refreshDeckList();
                ui.setShopData(shop);
            }
        }

        int relicIdx = -1;
        if (ui.pollShopBuyRelic(relicIdx)) {
            if (relicIdx >= 0 && relicIdx < static_cast<int>(shop.relicsForSale.size())) {
                const ShopRelicOffer& o = shop.relicsForSale[static_cast<size_t>(relicIdx)];
                if (playerState_.gold >= o.price &&
                    std::find(playerState_.relics.begin(), playerState_.relics.end(), o.id) ==
                        playerState_.relics.end()) {
                    playerState_.gold -= o.price;
                    playerState_.relics.push_back(o.id);
                    if (o.id == "potion_belt")
                        playerState_.potionSlotCount += 2;
                    shop.relicsForSale.erase(shop.relicsForSale.begin() + relicIdx);
                    shop.playerGold = playerState_.gold;
                    shop.potionSlotsMax = playerState_.potionSlotCount;
                    ui.setShopData(shop);
                }
            }
        }

        int potIdx = -1;
        if (ui.pollShopBuyPotion(potIdx)) {
            if (potIdx >= 0 && potIdx < static_cast<int>(shop.potionsForSale.size())) {
                const ShopPotionOffer& o = shop.potionsForSale[static_cast<size_t>(potIdx)];
                if (playerState_.gold >= o.price &&
                    static_cast<int>(playerState_.potions.size()) < playerState_.potionSlotCount) {
                    playerState_.gold -= o.price;
                    playerState_.potions.push_back(o.id);
                    shop.potionsForSale.erase(shop.potionsForSale.begin() + potIdx);
                    shop.playerGold = playerState_.gold;
                    shop.potionSlotsUsed = static_cast<int>(playerState_.potions.size());
                    ui.setShopData(shop);
                }
            }
        }

        InstanceId removeId = 0;
        if (ui.pollShopRemoveCard(removeId)) {
            CardId removedId;
            if (removeId > 0 && cardSystem_.remove_from_master_deck(removeId, &removedId)) {
                if (removedId == "parasite" || removedId == "parasite+") {
                    playerState_.maxHp = std::max(1, playerState_.maxHp - 3);
                    if (playerState_.currentHp > playerState_.maxHp) playerState_.currentHp = playerState_.maxHp;
                }
                shop.removeServicePaid = false;
                shop.removeServiceSoldOut = true;
                refreshDeckList();
                ui.setShopData(shop);
            }
        }

        window_.clear(sf::Color(40, 38, 45));
        ui.draw(window_);
        window_.display();
    }

    // 商店内已无选牌删牌界面：若曾付净简费但未完成移除，离开时退还
    if (shop.removeServicePaid && !shop.removeServiceSoldOut) {
        playerState_.gold += shop.removeServicePrice;
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
        d.cardId = inst.id;
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
                    if (!ui.tryDismissRestForgeUpgradeConfirm()) inScene = false;
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
        ui.syncRestForgeScrollbarDrag(mousePos);

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
                    d.cardId = inst.id;
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
