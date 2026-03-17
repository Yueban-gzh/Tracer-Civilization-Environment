/**
 * 事件 / 商店 / 休息 UI 实现（公共逻辑；事件/商店/休息绘制已拆到 _Event/_Shop/_Rest.cpp）
 */
#include "EventEngine/EventShopRestUI.hpp"
#include "EventEngine/EventShopRestUICommon.hpp"
#include "DataLayer/DataTypes.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tce {
using namespace esr_detail;

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
    pendingShopRemove_ = false;
    pendingRestHeal_ = false;
    pendingRestUpgrade_ = false;
}

void EventShopRestUI::setEventData(const EventDisplayData& data) {
    eventData_ = data;
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventOptionRects_.clear();
    if (data.imagePath != eventIllustPath_) {
        eventIllustPath_ = data.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
}

void EventShopRestUI::setEventDataFromUtf8(const std::string& title, const std::string& description,
                                           const std::vector<std::string>& optionTexts,
                                           const std::string& imagePath) {
    eventData_.title = utf8_to_wstring(title);
    eventData_.description = utf8_to_wstring(description);
    eventData_.optionTexts.clear();
    for (const auto& s : optionTexts)
        eventData_.optionTexts.push_back(utf8_to_wstring(s));
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventData_.imagePath = imagePath;
    eventOptionRects_.clear();
    if (eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
}

void EventShopRestUI::setEventResultFromUtf8(const std::string& resultSummary) {
    eventData_.title = std::wstring(L"事件结果");
    eventData_.description = utf8_to_wstring(resultSummary);
    eventData_.optionTexts.assign(1, std::wstring(L"确定"));
    eventData_.imagePath.clear();
    eventOptionRects_.clear();
    if (!eventIllustPath_.empty()) {
        eventIllustPath_.clear();
        eventIllustLoaded_ = false;
    }
}

void EventShopRestUI::setEventDataFromEvent(const DataLayer::Event* event) {
    if (!event) return;
    eventData_.title = utf8_to_wstring(event->title);
    eventData_.description = utf8_to_wstring(event->description);
    eventData_.optionTexts.clear();
    for (const auto& opt : event->options)
        eventData_.optionTexts.push_back(utf8_to_wstring(opt.text));
    if (eventData_.optionTexts.empty())
        eventData_.optionTexts.push_back(std::wstring(L"离开"));
    eventData_.imagePath = event->image;
    eventOptionRects_.clear();
    if (eventData_.imagePath != eventIllustPath_) {
        eventIllustPath_ = eventData_.imagePath;
        eventIllustLoaded_ = !eventIllustPath_.empty() && eventIllustTexture_.loadFromFile(eventIllustPath_);
    }
}

void EventShopRestUI::setShopData(const ShopDisplayData& data) {
    shopData_ = data;
    shopBuyRects_.clear();
    shopRemoveRects_.clear();
}

void EventShopRestUI::setRestData(const RestDisplayData& data) {
    restData_ = data;
    restUpgradeRects_.clear();
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

    if (ev.is<sf::Event::MouseButtonReleased>()) {
        auto const* rel = ev.getIf<sf::Event::MouseButtonReleased>();
        if (rel && rel->button == sf::Mouse::Button::Left)
            eventOptionPressedIndex_ = -1;
        return false;
    }
    if (ev.is<sf::Event::MouseButtonPressed>()) {
        auto const* btn = ev.getIf<sf::Event::MouseButtonPressed>();
        if (!btn || btn->button != sf::Mouse::Button::Left) return false;

        if (screen_ == EventShopRestScreen::Event) {
            for (size_t i = 0; i < eventOptionRects_.size(); ++i) {
                if (eventOptionRects_[i].contains(mousePos)) {
                    eventOptionPressedIndex_ = static_cast<int>(i);
                    pendingEventOption_ = static_cast<int>(i);
                    return true;
                }
            }
        } else if (screen_ == EventShopRestScreen::Shop) {
            for (size_t i = 0; i < shopBuyRects_.size() && i < shopData_.forSale.size(); ++i) {
                if (shopBuyRects_[i].contains(mousePos)) {
                    pendingShopBuyCard_ = shopData_.forSale[i].id;
                    pendingShopBuy_ = true;
                    return true;
                }
            }
            for (size_t i = 0; i < shopRemoveRects_.size() && i < shopData_.deckForRemove.size(); ++i) {
                if (shopRemoveRects_[i].contains(mousePos)) {
                    pendingShopRemoveInstance_ = shopData_.deckForRemove[i].instanceId;
                    pendingShopRemove_ = true;
                    return true;
                }
            }
        } else if (screen_ == EventShopRestScreen::Rest) {
            if (restShowUpgradeList_) {
                if (restBackButton_.contains(mousePos)) {
                    restShowUpgradeList_ = false;
                    return true;
                }
                for (size_t i = 0; i < restUpgradeRects_.size() && i < restData_.deckForUpgrade.size(); ++i) {
                    if (restUpgradeRects_[i].contains(mousePos)) {
                        pendingRestUpgradeInstance_ = restData_.deckForUpgrade[i].instanceId;
                        pendingRestUpgrade_ = true;
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

} // namespace tce
