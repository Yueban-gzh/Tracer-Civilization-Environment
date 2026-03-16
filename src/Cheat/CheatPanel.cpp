/**
 * 金手指面板实现 - 输入框 + 执行
 */
#include "../../include/Cheat/CheatPanel.hpp"
#include "../../include/Cheat/CheatEngine.hpp"
#include <SFML/Window/Keyboard.hpp>

namespace tce {

CheatPanel::CheatPanel(CheatEngine* cheat, unsigned windowWidth, unsigned windowHeight)
    : cheat_(cheat), width_(windowWidth), height_(windowHeight) {}

bool CheatPanel::loadFont(const std::string& path) {
    fontLoaded_ = font_.openFromFile(path);
    return fontLoaded_;
}

void CheatPanel::executeCurrent() {
    if (!cheat_ || inputText_.empty()) return;
    int r = cheat_->execute_line(inputText_);
    if (r == 1) {
        resultText_ = "OK: " + inputText_;
        inputText_.clear();
    } else if (r == 0) {
        resultText_ = "FAIL: " + inputText_;
    } else {
        resultText_ = "(skipped)";
    }
}

// 将 KeyPressed 映射为可输入字符（Windows 上 TextEntered 可能不触发，用此兜底）
static char keyToChar(sf::Keyboard::Key code, bool shift) {
    if (code >= sf::Keyboard::Key::A && code <= sf::Keyboard::Key::Z) {
        return static_cast<char>(shift ? 'A' + (static_cast<int>(code) - static_cast<int>(sf::Keyboard::Key::A))
                                       : 'a' + (static_cast<int>(code) - static_cast<int>(sf::Keyboard::Key::A)));
    }
    if (code >= sf::Keyboard::Key::Num0 && code <= sf::Keyboard::Key::Num9) {
        return static_cast<char>('0' + (static_cast<int>(code) - static_cast<int>(sf::Keyboard::Key::Num0)));
    }
    if (code == sf::Keyboard::Key::Space) return ' ';
    if (code == sf::Keyboard::Key::Hyphen) return shift ? '_' : '-';
    if (code == sf::Keyboard::Key::Period) return '.';
    if (code == sf::Keyboard::Key::Slash) return '/';
    return '\0';
}

bool CheatPanel::handleEvent(const sf::Event& ev) {
    if (ev.is<sf::Event::KeyPressed>()) {
        const auto* kp = ev.getIf<sf::Event::KeyPressed>();
        if (!kp) return false;
        if (kp->code == sf::Keyboard::Key::F2) {
            toggle();
            return true;
        }
        if (!visible_) return false;
        if (kp->code == sf::Keyboard::Key::Escape) {
            visible_ = false;
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Enter) {
            executeCurrent();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Backspace) {
            if (!inputText_.empty()) inputText_.pop_back();
            return true;
        }
        // KeyPressed 映射字符（Windows 上 TextEntered 常不触发）
        char c = keyToChar(kp->code, kp->shift);
        if (c != '\0') {
            inputText_ += c;
            return true;
        }
        return true;  // 其他按键也消费，避免触发游戏操作
    }
    // TextEntered 仅处理非 ASCII（KeyPressed 已覆盖字母数字等，避免重复输入）
    if (visible_ && ev.is<sf::Event::TextEntered>()) {
        const auto* te = ev.getIf<sf::Event::TextEntered>();
        if (te && te->unicode >= 128) {
            // 非 ASCII 需 UTF-8 编码，此处简化：仅支持 ASCII 命令
            return true;
        }
    }
    return false;
}

void CheatPanel::draw(sf::RenderWindow& window) {
    if (!visible_ || !fontLoaded_) return;

    const float panelW = 520.f;
    const float panelH = 200.f;
    const float pad = 12.f;
    float left = (static_cast<float>(width_) - panelW) * 0.5f;
    float top = 60.f;

    sf::RectangleShape bg(sf::Vector2f(panelW, panelH));
    bg.setPosition(sf::Vector2f(left, top));
    bg.setFillColor(sf::Color(30, 28, 35, 248));
    bg.setOutlineColor(sf::Color(180, 100, 80));
    bg.setOutlineThickness(2.f);
    window.draw(bg);

    sf::Text label(font_, "Cheat | F2 close | Enter exec", 15);
    label.setFillColor(sf::Color(200, 180, 160));
    label.setPosition(sf::Vector2f(left + pad, top + 4.f));
    window.draw(label);

    sf::RectangleShape inputBg(sf::Vector2f(panelW - pad * 2, 28.f));
    inputBg.setPosition(sf::Vector2f(left + pad, top + 26.f));
    inputBg.setFillColor(sf::Color(50, 45, 55));
    inputBg.setOutlineColor(sf::Color(120, 110, 100));
    inputBg.setOutlineThickness(1.f);
    window.draw(inputBg);

    std::string display = inputText_.empty() ? ">" : "> " + inputText_;
    sf::Text inputText(font_, display, 16);
    inputText.setFillColor(sf::Color(255, 240, 220));
    inputText.setPosition(sf::Vector2f(left + pad + 4.f, top + 30.f));
    window.draw(inputText);

    float y = top + 62.f;
    if (!resultText_.empty()) {
        sf::Text res(font_, resultText_, 13);
        res.setFillColor(sf::Color(150, 220, 150));
        res.setPosition(sf::Vector2f(left + pad, y));
        window.draw(res);
        y += 18.f;
    }

    // 命令教程（直接附在面板下方）
    static const char* helpLines[] = {
        "player_hp 80 | player_energy 3 | player_block 10 | player_gold 999",
        "player_status strength 10 99 | player_status_remove strength",
        "monster_hp 0 50 | monster_kill 0 | monster_status 0 vulnerable 3 2",
        "add_relic vajra | add_potion strength_potion | add_hand strike"
    };
    sf::Text helpText(font_, "", 11);
    helpText.setFillColor(sf::Color(140, 130, 120));
    for (const char* line : helpLines) {
        helpText.setString(line);
        helpText.setPosition(sf::Vector2f(left + pad, y));
        window.draw(helpText);
        y += 14.f;
    }
}

} // namespace tce
