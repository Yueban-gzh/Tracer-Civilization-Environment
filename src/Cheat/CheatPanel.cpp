/**
 * 金手指面板实现 - 输入框 + 执行
 */
#include "../../include/Cheat/CheatPanel.hpp"
#include "../../include/Cheat/CheatEngine.hpp"
#include <SFML/Window/Keyboard.hpp>
#include <algorithm>
#include <sstream>
#include <vector>

namespace tce {

// 与 CheatEngine::execute_line 中的命令一致，用于 Tab 补全
static const char* const CHEAT_COMMANDS[] = {
    "player_hp", "player_max_hp", "player_block", "player_energy", "player_gold",
    "player_status", "player_status_remove",
    "monster_hp", "monster_max_hp", "monster_status", "monster_status_remove", "monster_kill",
    "add_relic", "remove_relic", "add_potion", "remove_potion", "add_hand", "remove_hand"
};
static const size_t CHEAT_COMMANDS_COUNT = sizeof(CHEAT_COMMANDS) / sizeof(CHEAT_COMMANDS[0]);

CheatPanel::CheatPanel(CheatEngine* cheat, unsigned windowWidth, unsigned windowHeight)
    : cheat_(cheat), width_(windowWidth), height_(windowHeight) {}

bool CheatPanel::loadFont(const std::string& path) {
    fontLoaded_ = font_.openFromFile(path);
    return fontLoaded_;
}

// 按空格拆分为词，并返回最后一个词的起始下标
static void tokenizeAndLastWordStart(const std::string& in, std::vector<std::string>& words, size_t& lastWordStart) {
    words.clear();
    lastWordStart = 0;
    size_t i = 0;
    while (i < in.size()) {
        while (i < in.size() && in[i] == ' ') ++i;
        if (i >= in.size()) break;
        size_t start = i;
        while (i < in.size() && in[i] != ' ') ++i;
        words.push_back(in.substr(start, i - start));
        lastWordStart = start;
    }
}

// 判断是否为完整命令名（与 CHEAT_COMMANDS 完全匹配）
static bool isFullCommand(const std::string& cmd) {
    for (size_t c = 0; c < CHEAT_COMMANDS_COUNT; ++c)
        if (cmd == CHEAT_COMMANDS[c]) return true;
    return false;
}

void CheatPanel::completeInput() {
    std::string in = inputText_;
    std::vector<std::string> words;
    size_t lastWordStart = 0;
    tokenizeAndLastWordStart(in, words, lastWordStart);

    if (words.empty()) return;

    std::string prefix = words.front();
    size_t firstWordStart = in.find_first_not_of(' ');
    if (firstWordStart == std::string::npos) firstWordStart = 0;
    size_t firstWordEnd = firstWordStart + prefix.size();
    std::string restAfterFirst = (firstWordEnd < in.size()) ? in.substr(firstWordEnd) : std::string();
    while (!restAfterFirst.empty() && restAfterFirst[0] == ' ') restAfterFirst = restAfterFirst.substr(1);

    // 已处于循环补全：当前被补全的词等于上次选中则切换到下一项
    if (!lastTabMatches_.empty()) {
        if (lastReplaceStart_ == 0 && prefix == lastTabCurrent_) {
            lastTabIndex_ = (lastTabIndex_ + 1) % lastTabMatches_.size();
            lastTabCurrent_ = lastTabMatches_[lastTabIndex_];
            inputText_ = lastTabCurrent_ + (restAfterFirst.empty() ? " " : " " + restAfterFirst);
            return;
        }
        if (lastReplaceStart_ > 0) {
            std::string lastWord = in.substr(lastReplaceStart_);
            if (lastWord == lastTabCurrent_) {
                lastTabIndex_ = (lastTabIndex_ + 1) % lastTabMatches_.size();
                lastTabCurrent_ = lastTabMatches_[lastTabIndex_];
                inputText_ = in.substr(0, lastReplaceStart_) + lastTabCurrent_ + " ";
                return;
            }
        }
    }

    // 仅一个词：按命令补全
    if (words.size() == 1) {
        std::vector<std::string> matches;
        for (size_t c = 0; c < CHEAT_COMMANDS_COUNT; ++c) {
            std::string cmd(CHEAT_COMMANDS[c]);
            if (cmd.size() >= prefix.size() && cmd.compare(0, prefix.size(), prefix) == 0)
                matches.push_back(cmd);
        }
        if (matches.empty()) { lastTabMatches_.clear(); lastTabCurrent_.clear(); return; }
        if (matches.size() == 1) {
            inputText_ = matches[0] + (restAfterFirst.empty() ? " " : " " + restAfterFirst);
            lastTabMatches_.clear();
            lastTabCurrent_.clear();
            return;
        }
        lastTabMatches_ = matches;
        lastTabIndex_ = 0;
        lastTabCurrent_ = matches[0];
        lastReplaceStart_ = 0;
        inputText_ = lastTabCurrent_ + (restAfterFirst.empty() ? " " : " " + restAfterFirst);
        return;
    }

    // 多词：首词为完整命令时，对最后一个词做参数（卡牌/状态 ID）补全
    if (!cheat_ || !isFullCommand(words[0])) {
        // 首词不是完整命令，仍按命令补全首词
        std::vector<std::string> matches;
        for (size_t c = 0; c < CHEAT_COMMANDS_COUNT; ++c) {
            std::string cmd(CHEAT_COMMANDS[c]);
            if (cmd.size() >= prefix.size() && cmd.compare(0, prefix.size(), prefix) == 0)
                matches.push_back(cmd);
        }
        if (!matches.empty()) {
            lastTabMatches_ = matches;
            lastTabIndex_ = 0;
            lastTabCurrent_ = matches[0];
            lastReplaceStart_ = 0;
            inputText_ = lastTabCurrent_ + (restAfterFirst.empty() ? " " : " " + restAfterFirst);
        }
        return;
    }

    int argIndex = static_cast<int>(words.size()) - 1;
    std::string argPrefix = words.back();
    std::vector<std::string> idCandidates = cheat_->get_completion_candidates(words[0], argIndex, argPrefix);

    if (idCandidates.empty()) return;

    if (idCandidates.size() == 1) {
        inputText_ = in.substr(0, lastWordStart) + idCandidates[0] + " ";
        lastTabMatches_.clear();
        lastTabCurrent_.clear();
        return;
    }
    lastTabMatches_ = idCandidates;
    lastTabIndex_ = 0;
    lastTabCurrent_ = idCandidates[0];
    lastReplaceStart_ = lastWordStart;
    inputText_ = in.substr(0, lastWordStart) + idCandidates[0] + " ";
}

void CheatPanel::executeCurrent() {
    if (!cheat_ || inputText_.empty()) return;
    int r = cheat_->execute_line(inputText_);
    if (r == 1) {
        resultText_ = "OK: " + inputText_;
        inputText_.clear();
    } else if (r == 0) {
        std::string cmd;
        std::istringstream iss(inputText_);
        if (iss >> cmd) {
            std::string usage = cheat_->get_command_usage(cmd);
            resultText_ = "FAIL: " + inputText_;
            if (!usage.empty()) resultText_ += "\nUsage: " + usage;
        } else {
            resultText_ = "FAIL: " + inputText_;
        }
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
            lastTabMatches_.clear();
            lastTabCurrent_.clear();
            return true;
        }
        if (kp->code == sf::Keyboard::Key::Tab) {
            completeInput();
            return true;
        }
        // KeyPressed 映射字符（Windows 上 TextEntered 常不触发）
        char c = keyToChar(kp->code, kp->shift);
        if (c != '\0') {
            inputText_ += c;
            lastTabMatches_.clear();
            lastTabCurrent_.clear();
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

    sf::Text label(font_, "Cheat | F2 close | Enter exec | Tab complete", 15);
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

    // 光标：在文本末尾绘制竖线，每 0.6s 闪烁（打开面板时已重置时钟）
    float textOriginX = left + pad + 4.f;
    float cursorX = textOriginX;
    float cursorY = top + 28.f;
    if (!display.empty()) {
        sf::Vector2f pos = inputText.findCharacterPos(display.size());
        // SFML 的 findCharacterPos 返回渲染后的全局坐标，直接使用
        cursorX = pos.x;
        cursorY = pos.y;
    }
    float blink = cursorBlinkClock_.getElapsedTime().asSeconds();
    bool showCaret = (static_cast<int>(blink / 0.6f) % 2 == 0);
    if (showCaret) {
        sf::RectangleShape caret(sf::Vector2f(3.f, 20.f));
        caret.setPosition(sf::Vector2f(cursorX + 3.f, cursorY));
        caret.setFillColor(sf::Color(200, 190, 175));
        window.draw(caret);
    }

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
