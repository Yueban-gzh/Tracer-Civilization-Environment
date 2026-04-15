/**
 * 事件 / 商店 / 休息 UI 实现（公共逻辑；事件/商店/休息绘制已拆到 _Event/_Shop/_Rest.cpp）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include "DataLayer/DataTypes.h"
#include "DataLayer/DataLayer.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tce {
using namespace esr_detail;

namespace {
bool rest_forge_card_upgradable(const CardId& id) {
    if (id.empty() || id.back() == '+') return false;
    return get_card_by_id(id + "+") != nullptr;
}
} // namespace

EventShopRestUI::EventShopRestUI(unsigned width, unsigned height)
    : width_(width), height_(height) {
    eventBgLoaded_ = try_load_texture(eventBgTexture_, {
        "assets/backgrounds/event_bg_cn.png",
        "assets/backgrounds/event_bg_cn..png",
        "./assets/backgrounds/event_bg_cn.png",
        "./assets/backgrounds/event_bg_cn..png"
    });
    eventBannerLoaded_ = try_load_texture(eventBannerTexture_, {
        "assets/ui/event/banner_scroll_cn.png",
        "./assets/ui/event/banner_scroll_cn.png"
    });
    eventPanelFrameLoaded_ = try_load_texture(eventPanelFrameTexture_, {
        "assets/ui/event/panel_frame_cn.png",
        "./assets/ui/event/panel_frame_cn.png"
    });
    restBgLoaded_ = try_load_texture(restBgTexture_, {
        "assets/backgrounds/rest_bg_cn.png",
        "assets/backgrounds/event_bg_cn.png",
        "./assets/backgrounds/rest_bg_cn.png",
        "./assets/backgrounds/event_bg_cn.png"
    });
    shopBgLoaded_ = try_load_texture(shopBgTexture_, {
        "assets/backgrounds/shop_bg.png",
        "./assets/backgrounds/shop_bg.png"
    });
    shopTitleLoaded_ = try_load_texture(shopTitleTexture_, {
        "assets/ui/shop/shop_title.png",
        "./assets/ui/shop/shop_title.png"
    });
    shopDeleteCardLoaded_ = try_load_texture(shopDeleteCardTexture_, {
        "assets/ui/shop/delete_card.png",
        "./assets/ui/shop/delete_card.png"
    });
    shopDeleteBgLoaded_ = try_load_texture(shopDeleteBgTexture_, {
        "assets/ui/shop/delete_bg.png",
        "./assets/ui/shop/delete_bg.png"
    });
    restHealIconLoaded_ = try_load_texture(restHealIconTexture_, {
        "assets/ui/rest/icon_heal.png",
        "./assets/ui/rest/icon_heal.png"
    });
    restSmithIconLoaded_ = try_load_texture(restSmithIconTexture_, {
        "assets/ui/rest/icon_smith.png",
        "./assets/ui/rest/icon_smith.png"
    });
    restScrollLoaded_ = try_load_texture(restScrollTexture_, {
        "assets/ui/rest/cloud.png",
        "assets/ui/rest/book.png",
        "./assets/ui/rest/cloud.png",
        "./assets/ui/rest/book.png"
    });
}

bool EventShopRestUI::loadFont(const std::string& path) {
    fontLoaded_ = font_.openFromFile(path);
    return fontLoaded_;
}

bool EventShopRestUI::loadChineseFont(const std::string& path) {
    fontChineseLoaded_ = fontChinese_.openFromFile(path);
    return fontChineseLoaded_;
}

void EventShopRestUI::setScreen(EventShopRestScreen screen) {
    screen_ = screen;
    pendingEventOption_ = -1;
    eventOptionPressedIndex_ = -1;
    if (screen == EventShopRestScreen::Event) selectedEventOption_ = 0;
    if (screen == EventShopRestScreen::Rest) restShowUpgradeList_ = false;
    pendingShopBuy_ = false;
    pendingShopPayRemove_ = false;
    pendingShopRelic_ = false;
    pendingShopPotion_ = false;
    pendingShopLeave_ = false;
    pendingShopRemove_ = false;
    shopDeckScrollOffset_ = 0.f;
    shopDeckScrollMax_ = 0.f;
    pendingRestHeal_ = false;
    pendingRestUpgrade_ = false;
    restDeckPickScrollOffset_ = 0.f;
    restDeckPickScrollMax_ = 0.f;
    restForgeViewUpgrade_ = false;
    restForgeScrollbarDragging_ = false;
    restForgeUpgradeConfirmOpen_ = false;
    restForgeConfirmInstanceId_ = 0;
    shopRemoveConfirmOpen_ = false;
    shopRemoveConfirmInstanceId_ = 0;
    shopBuyConfirmOpen_ = false;
    shopBuyConfirmCardId_.clear();
    shopBuyConfirmPrice_ = 0;
}

void EventShopRestUI::setEventData(const EventDisplayData& data) {
    eventData_ = data;
    if (eventData_.optionCardIds.size() > eventData_.optionTexts.size())
        eventData_.optionCardIds.resize(eventData_.optionTexts.size());
    if (eventData_.optionTexts.empty()) {
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
        eventData_.optionEffectTexts.push_back(std::wstring());
    } else if (eventData_.optionEffectTexts.size() != eventData_.optionTexts.size()) {
        // 防止旧数据结构导致 size 不一致
        if (eventData_.optionEffectTexts.size() < eventData_.optionTexts.size())
            eventData_.optionEffectTexts.resize(eventData_.optionTexts.size());
        else
            eventData_.optionEffectTexts.resize(eventData_.optionTexts.size());
    }
    eventOptionRects_.clear();
    pendingEventOption_ = -1;
    eventOptionPressedIndex_ = -1;
    selectedEventOption_ = 0;
    eventCardScrollOffset_ = 0.f;
    eventCardScrollMax_ = 0.f;
    eventCardScrollStep_ = 42.f;
    if (data.imagePath != eventIllustPath_) {
        eventIllustPath_ = data.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
    if (eventIllustLoaded_) syncEventIllustrationSceneBackup(eventIllustPath_);
}

void EventShopRestUI::setEventDataFromUtf8(const std::string& title, const std::string& description,
                                           const std::vector<std::string>& optionTexts,
                                           const std::string& imagePath,
                                           const std::vector<std::string>& optionEffectTexts,
                                           const std::vector<std::string>& optionCardIds) {
    eventData_.title = utf8_to_wstring(title);
    eventData_.description = utf8_to_wstring(description);
    eventData_.optionTexts.clear();
    for (const auto& s : optionTexts)
        eventData_.optionTexts.push_back(utf8_to_wstring(s));
    if (eventData_.optionTexts.empty()) eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventData_.optionEffectTexts.clear();
    eventData_.optionEffectTexts.reserve(eventData_.optionTexts.size());
    for (std::size_t i = 0; i < eventData_.optionTexts.size(); ++i) {
        if (i < optionEffectTexts.size())
            eventData_.optionEffectTexts.push_back(utf8_to_wstring(optionEffectTexts[i]));
        else
            eventData_.optionEffectTexts.push_back(std::wstring());
    }
    eventData_.optionCardIds = optionCardIds;
    if (eventData_.optionCardIds.size() > eventData_.optionTexts.size())
        eventData_.optionCardIds.resize(eventData_.optionTexts.size());
    eventData_.imagePath = imagePath;
    eventOptionRects_.clear();
    pendingEventOption_ = -1;
    eventOptionPressedIndex_ = -1;
    selectedEventOption_ = 0;
    eventCardScrollOffset_ = 0.f;
    eventCardScrollMax_ = 0.f;
    eventCardScrollStep_ = 42.f;
    if (eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
    if (eventIllustLoaded_) syncEventIllustrationSceneBackup(eventIllustPath_);
}

void EventShopRestUI::setEventResultFromUtf8(const std::string& resultSummary, const std::string& imagePath) {
    eventData_.title = std::wstring(L"事件结果");
    eventData_.description = utf8_to_wstring(resultSummary);
    eventData_.optionTexts.assign(1, std::wstring(L"确定"));
    eventData_.optionEffectTexts.assign(1, std::wstring());
    eventData_.optionCardIds.clear();
    eventData_.imagePath = imagePath;
    eventOptionRects_.clear();
    pendingEventOption_ = -1;
    eventOptionPressedIndex_ = -1;
    selectedEventOption_ = 0;
    eventCardScrollOffset_ = 0.f;
    eventCardScrollMax_ = 0.f;
    eventCardScrollStep_ = 42.f;
    // 无卡牌/遗物/灵液等预览路径时保留进入结果页前的事件插图，避免左侧留白
    if (!eventData_.imagePath.empty() && eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        // "__cardid:" / "__cards:" 等走代码绘制，不加载贴图
        if (eventIllustPath_.rfind("__cardid:", 0) != 0 &&
            eventIllustPath_.rfind("__cards:", 0) != 0 &&
            eventIllustPath_.rfind("__cards_relic:", 0) != 0 &&
            eventIllustPath_.rfind("__relic:", 0) != 0 &&
            eventIllustPath_.rfind("__potion:", 0) != 0) {
            eventIllustLoaded_ = eventIllustTexture_.loadFromFile(eventIllustPath_);
        } else {
            eventIllustLoaded_ = false;
        }
        if (eventIllustLoaded_) syncEventIllustrationSceneBackup(eventIllustPath_);
    }
}

void EventShopRestUI::setEventDataFromEvent(const DataLayer::Event* event) {
    if (!event) return;
    eventData_.title = utf8_to_wstring(event->title);
    eventData_.description = utf8_to_wstring(event->description);
    eventData_.optionTexts.clear();
    eventData_.optionEffectTexts.clear();
    eventData_.optionCardIds.clear();
    auto effectToPreview = [](const DataLayer::EventEffect& eff) -> std::string {
        const int v = eff.value;
        if (eff.type == "gold") {
            if (v >= 0) return "+" + std::to_string(v) + "金";
            return "-" + std::to_string(-v) + "金";
        }
        if (eff.type == "heal") {
            if (v >= 0) return "+" + std::to_string(v) + "血";
            return "-" + std::to_string(-v) + "血";
        }
        if (eff.type == "max_hp") {
            if (v >= 0) return "+" + std::to_string(v) + "上限";
            return "-" + std::to_string(-v) + "上限";
        }
        if (eff.type == "card_reward") return "+" + std::to_string(v) + "牌";
        if (eff.type == "card_reward_choose") return "自选+" + std::to_string(std::max(1, v)) + "牌";
        if (eff.type == "remove_card") return "-" + std::to_string(v) + "牌";
        if (eff.type == "remove_card_choose") return "自选删" + std::to_string(std::max(1, v)) + "牌";
        if (eff.type == "upgrade_random") return "升级" + std::to_string(v) + "次";
        if (eff.type == "relic") return "+" + std::to_string(v) + "遗物";
        if (eff.type == "add_curse") return "+" + std::to_string(v) + "诅咒";
        if (eff.type == "remove_curse") return "-" + std::to_string(v) + "诅咒";
        if (eff.type == "none") return {};
        return eff.type;
    };

    for (const auto& opt : event->options) {
        std::string effectPreview;
        if (!opt.result.effects.empty()) {
            for (size_t i = 0; i < opt.result.effects.size(); ++i) {
                const std::string piece = effectToPreview(opt.result.effects[i]);
                if (piece.empty()) continue;
                if (!effectPreview.empty()) effectPreview += "；";
                effectPreview += piece;
            }
        } else {
            DataLayer::EventEffect eff{ opt.result.type, opt.result.value };
            effectPreview = effectToPreview(eff);
        }

        eventData_.optionTexts.push_back(utf8_to_wstring(opt.text));
        eventData_.optionEffectTexts.push_back(utf8_to_wstring(effectPreview));
    }
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    if (eventData_.optionEffectTexts.empty())
        eventData_.optionEffectTexts.assign(eventData_.optionTexts.size(), std::wstring());
    eventData_.imagePath = event->image;
    eventOptionRects_.clear();
    pendingEventOption_ = -1;
    eventOptionPressedIndex_ = -1;
    selectedEventOption_ = 0;
    eventCardScrollOffset_ = 0.f;
    eventCardScrollMax_ = 0.f;
    eventCardScrollStep_ = 42.f;
    if (eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
    if (eventIllustLoaded_) syncEventIllustrationSceneBackup(eventIllustPath_);
}

void EventShopRestUI::syncEventIllustrationSceneBackup(const std::string& path) {
    if (path.empty()) return;
    if (path.rfind("__cardid:", 0) == 0 || path.rfind("__cards:", 0) == 0 ||
        path.rfind("__cards_relic:", 0) == 0 || path.rfind("__relic:", 0) == 0 ||
        path.rfind("__potion:", 0) == 0)
        return;
    if (eventIllustSceneBackupTexture_.loadFromFile(path)) eventIllustSceneBackupLoaded_ = true;
}

void EventShopRestUI::setShopData(const ShopDisplayData& data) {
    shopData_ = data;
    shopBuyRects_.clear();
    shopColorlessRects_.clear();
    shopRelicRects_.clear();
    shopPotionRects_.clear();
    shopRemoveRects_.clear();
    shopDeckScrollOffset_ = 0.f;
    shopDeckScrollMax_ = 0.f;
    shopRemoveConfirmOpen_ = false;
    shopRemoveConfirmInstanceId_ = 0;
    shopBuyConfirmOpen_ = false;
    shopBuyConfirmCardId_.clear();
    shopBuyConfirmPrice_ = 0;
}

void EventShopRestUI::setRestData(const RestDisplayData& data) {
    restData_ = data;
    restUpgradeRects_.clear();
    restDeckPickScrollOffset_ = 0.f;
    restDeckPickScrollMax_ = 0.f;
    restForgeViewUpgrade_ = false;
    restForgeScrollbarDragging_ = false;
    restForgeUpgradeConfirmOpen_ = false;
    restForgeConfirmInstanceId_ = 0;
}

void EventShopRestUI::setMousePosition(sf::Vector2f pos) {
    mousePos_ = pos;
}

bool EventShopRestUI::clickInRect(const sf::Vector2f& pos, const sf::FloatRect& r) const {
    return r.contains(pos);
}

void EventShopRestUI::drawPanel(sf::RenderWindow& window, float centerX, float centerY, float w, float h) {
    sf::RectangleShape rect(sf::Vector2f(w, h));
    rect.setOrigin(sf::Vector2f(w * 0.5f, h * 0.5f));
    rect.setPosition(sf::Vector2f(centerX, centerY));
    rect.setFillColor(PANEL_BG);
    rect.setOutlineThickness(2.f);
    rect.setOutlineColor(PANEL_OUTLINE);
    window.draw(rect);
}

bool EventShopRestUI::handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos) {
    if (screen_ == EventShopRestScreen::None) return false;

    const bool shopRemovePickMode =
        (screen_ == EventShopRestScreen::Shop && shopData_.removeServicePaid && !shopData_.removeServiceSoldOut);

    if (ev.is<sf::Event::MouseMoved>()) {
        if (screen_ == EventShopRestScreen::Rest && restShowUpgradeList_ && restForgeScrollbarDragging_ &&
            !restForgeUpgradeConfirmOpen_) {
            syncRestForgeScrollbarDrag(mousePos);
            return true;
        }
    }

    if (screen_ == EventShopRestScreen::Event) {
        if (auto const* wheel = ev.getIf<sf::Event::MouseWheelScrolled>()) {
            const bool cardPickMode = (eventData_.title == L"择术" || eventData_.title == L"断舍" || eventData_.title == L"精修");
            if (cardPickMode && eventCardScrollMax_ > 0.f) {
                // SFML 鼠标滚轮：上滚 delta>0，偏移应减小（往上看）
                const float step = std::max(12.f, eventCardScrollStep_);
                eventCardScrollOffset_ -= wheel->delta * step;
                if (eventCardScrollOffset_ < 0.f) eventCardScrollOffset_ = 0.f;
                if (eventCardScrollOffset_ > eventCardScrollMax_) eventCardScrollOffset_ = eventCardScrollMax_;
                return true;
            }
        }
    } else if (shopRemovePickMode && !shopRemoveConfirmOpen_) {
        if (auto const* wheel = ev.getIf<sf::Event::MouseWheelScrolled>()) {
            if (shopDeckScrollMax_ > 0.f) {
                const float step = std::max(14.f, eventCardScrollStep_);
                shopDeckScrollOffset_ -= wheel->delta * step;
                if (shopDeckScrollOffset_ < 0.f) shopDeckScrollOffset_ = 0.f;
                if (shopDeckScrollOffset_ > shopDeckScrollMax_) shopDeckScrollOffset_ = shopDeckScrollMax_;
                return true;
            }
        }
    } else if (screen_ == EventShopRestScreen::Rest && restShowUpgradeList_ && !restForgeUpgradeConfirmOpen_) {
        if (auto const* wheel = ev.getIf<sf::Event::MouseWheelScrolled>()) {
            if (restDeckPickScrollMax_ > 0.f) {
                const float step = std::max(14.f, eventCardScrollStep_);
                restDeckPickScrollOffset_ -= wheel->delta * step;
                if (restDeckPickScrollOffset_ < 0.f) restDeckPickScrollOffset_ = 0.f;
                if (restDeckPickScrollOffset_ > restDeckPickScrollMax_) restDeckPickScrollOffset_ = restDeckPickScrollMax_;
                return true;
            }
        }
    }

    if (ev.is<sf::Event::MouseButtonReleased>()) {
        auto const* rel = ev.getIf<sf::Event::MouseButtonReleased>();
        if (rel && rel->button == sf::Mouse::Button::Left) {
            restForgeScrollbarDragging_ = false;
            if (screen_ == EventShopRestScreen::Event && eventOptionPressedIndex_ >= 0 &&
                eventOptionPressedIndex_ < static_cast<int>(eventOptionRects_.size())) {
                if (eventOptionRects_[static_cast<std::size_t>(eventOptionPressedIndex_)].contains(mousePos)) {
                    pendingEventOption_ = eventOptionPressedIndex_;
                }
            }
            eventOptionPressedIndex_ = -1;
        }
        return false;
    }
    if (ev.is<sf::Event::MouseButtonPressed>()) {
        auto const* btn = ev.getIf<sf::Event::MouseButtonPressed>();
        if (!btn || btn->button != sf::Mouse::Button::Left) return false;

        if (screen_ == EventShopRestScreen::Event) {
            for (size_t i = 0; i < eventOptionRects_.size(); ++i) {
                if (eventOptionRects_[i].contains(mousePos)) {
                    eventOptionPressedIndex_ = static_cast<int>(i);
                    return true;
                }
            }
        } else if (screen_ == EventShopRestScreen::Shop) {
            if (!shopRemovePickMode && shopBuyConfirmOpen_) {
                if (shopBuyConfirmOkRect_.contains(mousePos)) {
                    pendingShopBuyCard_ = shopBuyConfirmCardId_;
                    pendingShopBuy_ = true;
                    shopBuyConfirmOpen_ = false;
                    shopBuyConfirmCardId_.clear();
                    shopBuyConfirmPrice_ = 0;
                    return true;
                }
                if (shopBuyConfirmBackRect_.contains(mousePos)) {
                    shopBuyConfirmOpen_ = false;
                    shopBuyConfirmCardId_.clear();
                    shopBuyConfirmPrice_ = 0;
                    return true;
                }
                return true;
            }
            if (shopRemovePickMode && shopRemoveConfirmOpen_) {
                if (shopRemoveConfirmOkRect_.contains(mousePos)) {
                    pendingShopRemoveInstance_ = shopRemoveConfirmInstanceId_;
                    pendingShopRemove_ = true;
                    shopRemoveConfirmOpen_ = false;
                    shopRemoveConfirmInstanceId_ = 0;
                    return true;
                }
                if (shopRemoveConfirmBackRect_.contains(mousePos)) {
                    shopRemoveConfirmOpen_ = false;
                    shopRemoveConfirmInstanceId_ = 0;
                    return true;
                }
                return true;
            }
            if (shopLeaveButtonRect_.contains(mousePos)) {
                pendingShopLeave_ = true;
                return true;
            }
            if (shopRemovePickMode) {
                for (size_t i = 0; i < shopRemoveRects_.size() && i < shopData_.deckForRemove.size(); ++i) {
                    if (shopRemoveRects_[i].contains(mousePos)) {
                        shopRemoveConfirmInstanceId_ = shopData_.deckForRemove[i].instanceId;
                        shopRemoveConfirmOpen_ = true;
                        return true;
                    }
                }
                return false;
            }
            if (shopRemoveServiceRect_.contains(mousePos) &&
                !shopData_.removeServiceSoldOut &&
                !shopData_.removeServicePaid &&
                shopData_.removeServicePrice > 0) {
                pendingShopPayRemove_ = true;
                return true;
            }
            for (size_t i = 0; i < shopBuyRects_.size() && i < shopData_.forSale.size(); ++i) {
                if (shopBuyRects_[i].contains(mousePos)) {
                    shopBuyConfirmCardId_ = shopData_.forSale[i].id;
                    shopBuyConfirmPrice_ = shopData_.forSale[i].price;
                    shopBuyConfirmOpen_ = true;
                    return true;
                }
            }
            for (size_t i = 0; i < shopColorlessRects_.size() && i < shopData_.colorlessForSale.size(); ++i) {
                if (shopColorlessRects_[i].contains(mousePos)) {
                    shopBuyConfirmCardId_ = shopData_.colorlessForSale[i].id;
                    shopBuyConfirmPrice_ = shopData_.colorlessForSale[i].price;
                    shopBuyConfirmOpen_ = true;
                    return true;
                }
            }
            for (size_t i = 0; i < shopRelicRects_.size() && i < shopData_.relicsForSale.size(); ++i) {
                if (shopRelicRects_[i].contains(mousePos)) {
                    pendingShopRelicIndex_ = static_cast<int>(i);
                    pendingShopRelic_ = true;
                    return true;
                }
            }
            for (size_t i = 0; i < shopPotionRects_.size() && i < shopData_.potionsForSale.size(); ++i) {
                if (shopPotionRects_[i].contains(mousePos)) {
                    pendingShopPotionIndex_ = static_cast<int>(i);
                    pendingShopPotion_ = true;
                    return true;
                }
            }
        } else if (screen_ == EventShopRestScreen::Rest) {
            if (restShowUpgradeList_ && restForgeUpgradeConfirmOpen_) {
                if (restForgeConfirmOkRect_.contains(mousePos)) {
                    pendingRestUpgradeInstance_ = restForgeConfirmInstanceId_;
                    pendingRestUpgrade_ = true;
                    restForgeUpgradeConfirmOpen_ = false;
                    restForgeConfirmInstanceId_ = 0;
                    return true;
                }
                if (restBackButton_.contains(mousePos)) {
                    restForgeUpgradeConfirmOpen_ = false;
                    restForgeConfirmInstanceId_ = 0;
                    return true;
                }
                if (restForgeViewUpgradeToggleRect_.contains(mousePos)) {
                    restForgeViewUpgrade_ = !restForgeViewUpgrade_;
                    return true;
                }
                return true;
            }
            if (restShowUpgradeList_) {
                if (restForgeScrollbarThumbRect_.contains(mousePos) && restDeckPickScrollMax_ > 0.f) {
                    restForgeScrollbarDragging_ = true;
                    restForgeScrollbarGrabY_ = mousePos.y - restForgeScrollbarThumbRect_.position.y;
                    return true;
                }
                if (restForgeScrollbarTrackRect_.contains(mousePos) && restDeckPickScrollMax_ > 0.f &&
                    !restForgeScrollbarThumbRect_.contains(mousePos)) {
                    const float trackTop = restForgeScrollbarTrackRect_.position.y;
                    const float trackH = restForgeScrollbarTrackRect_.size.y;
                    const float thumbH = restForgeScrollbarThumbH_;
                    const float maxT = std::max(0.f, trackH - thumbH);
                    if (maxT > 1.f) {
                        float thumbTop = mousePos.y - thumbH * 0.5f;
                        if (thumbTop < trackTop) thumbTop = trackTop;
                        if (thumbTop > trackTop + maxT) thumbTop = trackTop + maxT;
                        restDeckPickScrollOffset_ = ((thumbTop - trackTop) / maxT) * restDeckPickScrollMax_;
                    }
                    return true;
                }
                if (restForgeViewUpgradeToggleRect_.contains(mousePos)) {
                    restForgeViewUpgrade_ = !restForgeViewUpgrade_;
                    return true;
                }
                if (restBackButton_.contains(mousePos)) {
                    restShowUpgradeList_ = false;
                    restForgeScrollbarDragging_ = false;
                    restForgeUpgradeConfirmOpen_ = false;
                    restForgeConfirmInstanceId_ = 0;
                    return true;
                }
                for (size_t i = 0; i < restUpgradeRects_.size() && i < restData_.deckForUpgrade.size(); ++i) {
                    if (restUpgradeRects_[i].contains(mousePos)) {
                        const auto& pick = restData_.deckForUpgrade[i];
                        if (rest_forge_card_upgradable(pick.cardId)) {
                            restForgeConfirmInstanceId_ = pick.instanceId;
                            restForgeUpgradeConfirmOpen_ = true;
                            restForgeScrollbarDragging_ = false;
                        }
                        return true;
                    }
                }
            } else {
                if (restHealButton_.contains(mousePos)) {
                    pendingRestHeal_ = true;
                    return true;
                }
                if (restUpgradeChoiceButton_.contains(mousePos)) {
                    restShowUpgradeList_ = true;
                    restDeckPickScrollOffset_ = 0.f;
                    restForgeViewUpgrade_ = false;
                    restForgeUpgradeConfirmOpen_ = false;
                    restForgeConfirmInstanceId_ = 0;
                    return true;
                }
            }
        }
        return false;
    }

    if (ev.is<sf::Event::KeyPressed>() && screen_ == EventShopRestScreen::Event && !eventOptionRects_.empty()) {
        auto const* key = ev.getIf<sf::Event::KeyPressed>();
        if (!key) return false;
        const int n = static_cast<int>(eventOptionRects_.size());
        if (key->code == sf::Keyboard::Key::Up) {
            selectedEventOption_ = (selectedEventOption_ - 1 + n) % n;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Down) {
            selectedEventOption_ = (selectedEventOption_ + 1) % n;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) {
            pendingEventOption_ = selectedEventOption_;
            return true;
        }
        if (key->code == sf::Keyboard::Key::Escape) {
            selectedEventOption_ = 0;
            return true;
        }
    }
    return false;
}

void EventShopRestUI::draw(sf::RenderWindow& window) {
    if (screen_ == EventShopRestScreen::None) return;
    if (screen_ == EventShopRestScreen::Rest && restBgLoaded_) {
        draw_texture_fit(window, restBgTexture_, sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_))));
    } else if (screen_ == EventShopRestScreen::Shop && shopBgLoaded_) {
        // 商店专用背景，整体压暗一档
        draw_texture_fit(window, shopBgTexture_, sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_))),
            sf::Color(158, 156, 162));
    } else if (eventBgLoaded_) {
        draw_texture_fit(window, eventBgTexture_, sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_))));
    }
    switch (screen_) {
    case EventShopRestScreen::Event:
        drawEventScreen(window);
        break;
    case EventShopRestScreen::Shop:
        drawShopScreen(window);
        break;
    case EventShopRestScreen::Rest:
        drawRestScreen(window);
        break;
    default:
        break;
    }
}

bool EventShopRestUI::pollEventOption(int& outIndex) {
    if (pendingEventOption_ < 0) return false;
    outIndex = pendingEventOption_;
    pendingEventOption_ = -1;
    return true;
}

bool EventShopRestUI::pollShopBuyCard(CardId& outCardId) {
    if (!pendingShopBuy_) return false;
    outCardId = pendingShopBuyCard_;
    pendingShopBuy_ = false;
    return true;
}

bool EventShopRestUI::pollShopPayRemoveService() {
    if (!pendingShopPayRemove_) return false;
    pendingShopPayRemove_ = false;
    return true;
}

bool EventShopRestUI::pollShopBuyRelic(int& outIndex) {
    if (!pendingShopRelic_) return false;
    outIndex = pendingShopRelicIndex_;
    pendingShopRelic_ = false;
    pendingShopRelicIndex_ = -1;
    return true;
}

bool EventShopRestUI::pollShopBuyPotion(int& outIndex) {
    if (!pendingShopPotion_) return false;
    outIndex = pendingShopPotionIndex_;
    pendingShopPotion_ = false;
    pendingShopPotionIndex_ = -1;
    return true;
}

bool EventShopRestUI::pollShopLeave() {
    if (!pendingShopLeave_) return false;
    pendingShopLeave_ = false;
    return true;
}

bool EventShopRestUI::pollShopRemoveCard(InstanceId& outInstanceId) {
    if (!pendingShopRemove_) return false;
    outInstanceId = pendingShopRemoveInstance_;
    pendingShopRemove_ = false;
    return true;
}

bool EventShopRestUI::pollRestHeal() {
    if (!pendingRestHeal_) return false;
    pendingRestHeal_ = false;
    return true;
}

bool EventShopRestUI::pollRestUpgradeCard(InstanceId& outInstanceId) {
    if (!pendingRestUpgrade_) return false;
    outInstanceId = pendingRestUpgradeInstance_;
    pendingRestUpgrade_ = false;
    return true;
}

bool EventShopRestUI::tryDismissRestForgeUpgradeConfirm() {
    if (screen_ != EventShopRestScreen::Rest || !restShowUpgradeList_ || !restForgeUpgradeConfirmOpen_) return false;
    restForgeUpgradeConfirmOpen_ = false;
    restForgeConfirmInstanceId_ = 0;
    return true;
}

bool EventShopRestUI::tryDismissShopRemoveConfirm() {
    const bool inRemovePickMode =
        (screen_ == EventShopRestScreen::Shop && shopData_.removeServicePaid && !shopData_.removeServiceSoldOut);
    if (inRemovePickMode && shopRemoveConfirmOpen_) {
        shopRemoveConfirmOpen_ = false;
        shopRemoveConfirmInstanceId_ = 0;
        return true;
    }
    if (!inRemovePickMode && shopBuyConfirmOpen_) {
        shopBuyConfirmOpen_ = false;
        shopBuyConfirmCardId_.clear();
        shopBuyConfirmPrice_ = 0;
        return true;
    }
    return false;
}

void EventShopRestUI::syncRestForgeScrollbarDrag(const sf::Vector2f& mousePos) {
    if (restForgeUpgradeConfirmOpen_) return;
    if (!restForgeScrollbarDragging_ || screen_ != EventShopRestScreen::Rest || !restShowUpgradeList_) return;
    const float trackTop = restForgeScrollbarTrackRect_.position.y;
    const float trackH = restForgeScrollbarTrackRect_.size.y;
    const float thumbH = restForgeScrollbarThumbH_;
    if (restDeckPickScrollMax_ <= 0.f || trackH <= thumbH) return;
    const float maxT = trackH - thumbH;
    float thumbTop = mousePos.y - restForgeScrollbarGrabY_;
    if (thumbTop < trackTop) thumbTop = trackTop;
    if (thumbTop > trackTop + maxT) thumbTop = trackTop + maxT;
    restDeckPickScrollOffset_ = ((thumbTop - trackTop) / maxT) * restDeckPickScrollMax_;
}

} // namespace tce
