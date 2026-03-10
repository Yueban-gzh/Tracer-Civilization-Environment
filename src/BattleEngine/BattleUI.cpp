/**
 * Battle UI - 严格按照参考图布局：无大色块，顶栏一行+遗物行+战场+底栏(能量/抽牌/手牌/结束回合/弃牌)
 */
#include "BattleEngine/BattleUI.hpp"
#include "BattleEngine/BattleEngine.hpp"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace tce {

namespace {
    constexpr float TOP_BAR_BG_H = 72.f;         // 顶栏高度 72
    constexpr float TOP_ROW_Y = 20.f;
    constexpr float TOP_ROW_H = 32.f;
    constexpr float RELICS_ROW_Y = 72.f;
    constexpr float RELICS_ROW_H = 72.f;
    constexpr float RELICS_ICON_SZ = 55.f;       // 遗物图标 55×55
    constexpr float RELICS_ICON_Y_OFFSET = 16.f; // 遗物往下一点
    constexpr float BOTTOM_BAR_Y_RATIO = 0.78f;  // 底栏起始，留出空间给手牌
    constexpr float HAND_AREA_BG_H = 220.f;
    constexpr float BOTTOM_MARGIN = 20.f;        // 底边距，抽牌/弃牌贴底
    constexpr float SIDE_MARGIN = 24.f;

    constexpr float CARD_W = 190.f;
    constexpr float CARD_H = 300.f;
    constexpr float CARD_PREVIEW_W = 280.f;  // 悬停预览尺寸
    constexpr float CARD_PREVIEW_H = 410.f;
    constexpr float CARD_PREVIEW_BOTTOM_ABOVE = 5.f;  // 预览时卡牌下边距屏幕底
    constexpr float HAND_CARD_OVER_BUTTONS = 18.f;
    constexpr float HAND_CARD_DISPLAY_STEP = 145.f; // 展示宽度：只比牌面宽度小一点
    constexpr float HAND_FAN_SPAN_MAX_DEG = 45.f;   // 总幅度上限，防止牌数很多时弧过平
    constexpr float HAND_PIVOT_Y_BELOW = 1900.f;    // 轴心在屏幕下方距离（大一些半径够用，10 张牌 200px 展示不超 60°）
    constexpr float HAND_ARC_TOP_ABOVE_BOTTOM = 220.f;  // 弧顶距屏幕底边 220（卡牌上边不超过）
    constexpr float ENERGY_CENTER_X_BL = 190.f;
    constexpr float ENERGY_CENTER_Y_BL = 190.f;
    constexpr float ENERGY_OUTLINE_THICKNESS = 10.f;  // 边框厚度 10
    constexpr float ENERGY_R = (120.f - 20.f) * 0.5f; // 填充圆半径，总直径 120
    constexpr float PILE_ICON_W = 52.f;
    constexpr float PILE_ICON_H = 72.f;
    constexpr float PILE_CENTER_OFFSET = 36.f;
    constexpr float PILE_NUM_CIRCLE_R = 17.f;    // 牌数小圆背景半径（角上）
    constexpr float END_TURN_W = 180.f;
    constexpr float END_TURN_H = 70.f;
    constexpr float END_TURN_CENTER_X_BR = 280.f;  // 以右下为 (0,0) 时按钮中心 x
    constexpr float END_TURN_CENTER_Y_BR = 210.f;  // 以右下为 (0,0) 时按钮中心 y
    constexpr float HP_BAR_W = 230.f;            // 血条最大长度固定，生命上限不影响
    constexpr float HP_BAR_H = 10.f;             // 血条变细；下方为效果栏
    constexpr float INTENT_ORB_R = 20.f;
    constexpr float MODEL_PLACEHOLDER_W = 150.f;
    constexpr float MODEL_PLACEHOLDER_H = 240.f;  // 下边往下 60（原 180+60）
    constexpr float MODEL_TOP_OFFSET = 90.f;      // 模型上边距中心，固定则上边框位置不变
    constexpr float MODEL_CENTER_Y_RATIO = 0.58f;
    constexpr float BATTLE_MODEL_BLOCK_Y_OFFSET = 60.f;  // 模型+血条+效果栏整体下移

    const sf::Color TOP_BAR_BG_COLOR(42, 38, 48);
    const sf::Color HAND_AREA_BG_COLOR(35, 33, 42);

    // 当前 Demo 中：打击/重击等攻击牌需要敌人目标，其它（防御/能力等）默认自选玩家目标
    inline bool card_targets_enemy(const BattleStateSnapshot& s, size_t handIndex) {
        if (handIndex >= s.hand.size()) return true;
        const auto& id = s.hand[handIndex].id;
        const CardData* cd = get_card_by_id(id);
        // 若数据层已有卡牌信息，则按「需要目标」或「攻击牌」决定
        if (cd) {
            if (cd->requiresTarget) return true;
            if (cd->cardType == CardType::Attack) return true;
            return false;
        }
        // 数据层尚未填充具体卡牌数据时，对部分已知攻击牌（如打击/重击）按攻击牌处理
        if (id == "strike" || id == "bash") return true;
        return false;
    }
}

BattleUI::BattleUI(unsigned width, unsigned height) : width_(width), height_(height) {
    const float btnCenterX = static_cast<float>(width_) - END_TURN_CENTER_X_BR;
    const float btnCenterY = static_cast<float>(height_) - END_TURN_CENTER_Y_BR;
    const float btnX = btnCenterX - END_TURN_W * 0.5f;
    const float btnY = btnCenterY - END_TURN_H * 0.5f;
    endTurnButton_ = sf::FloatRect(sf::Vector2f(btnX, btnY), sf::Vector2f(END_TURN_W, END_TURN_H));
}

bool BattleUI::loadFont(const std::string& path) {
    fontLoaded_ = font_.openFromFile(path);
    return fontLoaded_;
}

bool BattleUI::loadChineseFont(const std::string& path) {
    fontChineseLoaded_ = fontChinese_.openFromFile(path);
    return fontChineseLoaded_;
}

void BattleUI::setMousePosition(sf::Vector2f pos) {
    mousePos_ = pos;
}

bool BattleUI::handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos) {
    if (ev.is<sf::Event::MouseButtonPressed>()) {
        auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
        if (!btn) return false;

        // 左键：先处理打牌逻辑，其次再判断结束回合按钮
        if (btn->button == sf::Mouse::Button::Left) {
            // 若当前正在瞄准牌，根据目标类型决定出牌逻辑
            if (selectedHandIndex_ >= 0 && isAimingCard_) {
                if (selectedCardTargetsEnemy_) {
                    // 敌人目标牌：当鼠标在怪物模型上点击时视为对怪物使用
                    if (monsterModelRect_.contains(mousePos)) {
                        pendingPlayHandIndex_ = selectedHandIndex_;
                        pendingPlayTargetMonsterIndex_ = 0;  // 目前只有一个怪物
                        selectedHandIndex_ = -1;
                        isAimingCard_ = false;
                        return false; // 由主循环调用 engine.play_card
                    }
                } else {
                    // 自选（玩家）目标牌：当鼠标已离开原位矩形（玩家模型出现黄框）时点击，视为对玩家使用
                    if (!selectedCardInsideOriginRect_) {
                        pendingPlayHandIndex_ = selectedHandIndex_;
                        pendingPlayTargetMonsterIndex_ = -1; // -1 表示玩家自身
                        selectedHandIndex_ = -1;
                        isAimingCard_ = false;
                        return false;
                    }
                    // 若仍在原位矩形内且玩家模型未高亮，则视为不使用（本次点击忽略）
                }
            }

            // 若尚未选中牌，则检查是否点中了某张手牌以开始选中/瞄准
            // 在 drawBottomBar 中根据 mousePos_ 计算 hoverIndex，这里简单复用 hand 区域的点击检测：
            // 交给下一帧的 drawBottomBar 使用 selectedHandIndex_ 和 isAimingCard_ 来决定展示。

            // 点击结束回合按钮
            if (endTurnButton_.contains(mousePos)) {
                return true;
            }
        }

        // 右键：取消当前选中的牌/瞄准
        if (btn->button == sf::Mouse::Button::Right) {
            selectedHandIndex_ = -1;
            isAimingCard_ = false;
            return false;
        }
    }
    return false;
}

bool BattleUI::pollPlayCardRequest(int& outHandIndex, int& outTargetMonsterIndex) {
    if (pendingPlayHandIndex_ < 0) return false;
    outHandIndex = pendingPlayHandIndex_;
    outTargetMonsterIndex = pendingPlayTargetMonsterIndex_;
    pendingPlayHandIndex_ = -1;
    pendingPlayTargetMonsterIndex_ = -1;
    return true;
}

void BattleUI::draw(sf::RenderWindow& window, IBattleUIDataProvider& data) {
    const BattleStateSnapshot& s = data.get_snapshot();
    if (!fontLoaded_) return;

    // 顶栏背景色块
    sf::RectangleShape topBg(sf::Vector2f(static_cast<float>(width_), TOP_BAR_BG_H));
    topBg.setPosition(sf::Vector2f(0.f, 0.f));
    topBg.setFillColor(TOP_BAR_BG_COLOR);
    window.draw(topBg);

    drawTopBar(window, s);
    drawRelicsRow(window, s);
    drawBattleCenter(window, s);

    // 手牌区背景色块
    float handBgY = height_ - HAND_AREA_BG_H;
    sf::RectangleShape handBg(sf::Vector2f(static_cast<float>(width_), HAND_AREA_BG_H));
    handBg.setPosition(sf::Vector2f(0.f, handBgY));
    handBg.setFillColor(HAND_AREA_BG_COLOR);
    window.draw(handBg);

    drawBottomBar(window, s);
    drawTopRight(window, s);
}

// 顶栏从左到右：钥匙槽、名字、职业、HP、金币、药水槽、当前层数、当前难度（统一高度、信息间有间隙）
void BattleUI::drawTopBar(sf::RenderWindow& window, const BattleStateSnapshot& s) {
    const float left = 28.f;
    const float rowY = TOP_ROW_Y + 8.f;   // 统一基线高度
    const float itemGap = 20.f;
    const unsigned nameSize = 42;         // 名字变大
    const float nameY = rowY - 10.f;      // 名字上移
    const unsigned hpSize = 28;
    const unsigned restSize = 26;
    float x = left;
    char buf[128];

    // 1. 钥匙槽
    const float keySz = 44.f;
    sf::RectangleShape keySlot(sf::Vector2f(keySz, keySz));
    keySlot.setPosition(sf::Vector2f(x, rowY - 4.f));
    keySlot.setFillColor(sf::Color(60, 55, 65));
    keySlot.setOutlineColor(sf::Color(140, 130, 100));
    keySlot.setOutlineThickness(1.f);
    window.draw(keySlot);
    x += keySz + itemGap;

    // 2. 名字（变大、上移）
    sf::Text nameText(font_, s.playerName, nameSize);
    nameText.setFillColor(sf::Color(240, 240, 240));
    nameText.setPosition(sf::Vector2f(x, nameY));
    window.draw(nameText);
    x += 110.f + 10.f;   // 职业贴着名字，只留小间隙

    // 3. 职业（贴着名字）
    sf::Text charText(font_, s.character, restSize);
    charText.setFillColor(sf::Color(200, 200, 210));
    charText.setPosition(sf::Vector2f(x, rowY));
    window.draw(charText);
    x += 88.f + itemGap + 80.f;   // 生命再往右

    // 4. HP
    std::snprintf(buf, sizeof(buf), "%d/%d", s.currentHp, s.maxHp);
    sf::Text hpText(font_, buf, hpSize);
    hpText.setFillColor(sf::Color(230, 80, 80));
    hpText.setPosition(sf::Vector2f(x, rowY));
    window.draw(hpText);
    x += 96.f + itemGap + 50.f;   // 金钱再往右

    // 5. 金币
    std::snprintf(buf, sizeof(buf), "%d", s.gold);
    sf::Text goldText(font_, buf, restSize);
    goldText.setFillColor(sf::Color(255, 200, 80));
    goldText.setPosition(sf::Vector2f(x, rowY));
    window.draw(goldText);
    x += 44.f + itemGap + 12.f;   // 药水栏再往右，金钱与药水栏间隙缩小

    // 6. 药水栏：药水槽数量由快照数据驱动，默认 3；单槽大小固定，3 个和 5 个的槽尺寸一致
    const int potionSlotCount = std::max(1, std::min(5, s.potionSlotCount));
    const float potionW = 48.f;   // 固定槽宽
    const float potionH = 40.f;   // 固定槽高
    const float potionGap = 14.f;
    const float potionStartX = x;
    for (int i = 0; i < potionSlotCount; ++i) {
        sf::RectangleShape slot(sf::Vector2f(potionW, potionH));
        slot.setPosition(sf::Vector2f(x, rowY - 4.f));
        slot.setFillColor(sf::Color(0, 0, 0, 0));
        slot.setOutlineColor(sf::Color(180, 180, 180, 180));
        slot.setOutlineThickness(1.f);
        window.draw(slot);
        x += potionW + potionGap;
    }
    for (size_t i = 0; i < s.potions.size() && i < static_cast<size_t>(potionSlotCount); ++i) {
        const float slotX = potionStartX + i * (potionW + potionGap);
        sf::RectangleShape fill(sf::Vector2f(potionW - 6.f, potionH - 8.f));
        fill.setPosition(sf::Vector2f(slotX + 3.f, rowY - 2.f));
        fill.setFillColor(sf::Color(180, 140, 255));
        window.draw(fill);
    }
    x += 16.f;

    // 7. 当前层数（占位）：当前暂未接入真实地图层数，这里先显示占位
    const float centerRightX = width_ * 0.62f;
    sf::Text floorText(fontForChinese(), sf::String(L"层 -"), restSize);
    floorText.setFillColor(sf::Color(220, 210, 200));
    floorText.setPosition(sf::Vector2f(centerRightX, rowY));
    window.draw(floorText);

    const float diffX = width_ * 0.74f;
    sf::Text diffText(fontForChinese(), sf::String(L"难度 -"), restSize);
    diffText.setFillColor(sf::Color(180, 180, 190));
    diffText.setPosition(sf::Vector2f(diffX, rowY));
    window.draw(diffText);
}

// 遗物栏：顶栏下方，往下一点、图标变大
void BattleUI::drawRelicsRow(sf::RenderWindow& window, const BattleStateSnapshot& s) {
    float x = 28.f;
    const float y = RELICS_ROW_Y + RELICS_ICON_Y_OFFSET;
    for (size_t i = 0; i < s.relics.size() && i < 12u; ++i) {
        sf::RectangleShape icon(sf::Vector2f(RELICS_ICON_SZ, RELICS_ICON_SZ));
        icon.setPosition(sf::Vector2f(x, y));
        icon.setFillColor(sf::Color(80, 70, 90));
        icon.setOutlineColor(sf::Color(120, 110, 130));
        icon.setOutlineThickness(1.f);
        window.draw(icon);
        x += RELICS_ICON_SZ + 18.f;   // 遗物间距稍大
    }
}

// 战场中央：玩家区(背景+模型+血条在下+增益减益)、怪物区(背景+意图在上+模型+血条在下)
void BattleUI::drawBattleCenter(sf::RenderWindow& window, const BattleStateSnapshot& s) {
    const float battleTop = RELICS_ROW_Y + RELICS_ROW_H + 14.f;  // 战场在遗物栏下方
    const float battleH = (height_ * BOTTOM_BAR_Y_RATIO) - battleTop - 16.f;
    const float playerLeft = width_ * 0.06f;
    const float playerW = width_ * 0.36f;
    const float monsterLeft = width_ * 0.58f;
    const float monsterW = width_ * 0.36f;
    const float modelCenterY = battleTop + battleH * MODEL_CENTER_Y_RATIO + BATTLE_MODEL_BLOCK_Y_OFFSET;

    char buf[32];

    // ---------- 玩家区：模型占位 -> 血条在模型下方 -> 增益减益在血条下方 ----------
    float playerCenterX = playerLeft + playerW * 0.5f;
    float modelTop = modelCenterY - MODEL_TOP_OFFSET;  // 上边不变
    sf::RectangleShape playerModel(sf::Vector2f(MODEL_PLACEHOLDER_W, MODEL_PLACEHOLDER_H));
    playerModel.setPosition(sf::Vector2f(playerCenterX - MODEL_PLACEHOLDER_W * 0.5f, modelTop));
    // 记录玩家模型矩形用于高亮与命中检测（如自选目标牌）
    playerModelRect_ = sf::FloatRect(
        sf::Vector2f(playerCenterX - MODEL_PLACEHOLDER_W * 0.5f, modelTop),
        sf::Vector2f(MODEL_PLACEHOLDER_W, MODEL_PLACEHOLDER_H)
    );
    playerModel.setFillColor(sf::Color(60, 55, 70));
    playerModel.setOutlineColor(sf::Color(100, 95, 110));
    playerModel.setOutlineThickness(1.f);
    window.draw(playerModel);
    sf::Text playerLabel(fontForChinese(), sf::String(L"玩家"), 20);
    playerLabel.setFillColor(sf::Color(180, 180, 190));
    playerLabel.setPosition(sf::Vector2f(playerCenterX - 24.f, modelCenterY - 26.f));
    window.draw(playerLabel);

    // 玩家模型左侧显示当前回合阶段（调试用）
    if (!s.phaseDebugLabel.empty()) {
        sf::Text phaseText(fontForChinese(), sf::String(s.phaseDebugLabel), 18);
        phaseText.setFillColor(sf::Color(200, 210, 230));
        const float phaseX = playerCenterX - MODEL_PLACEHOLDER_W * 0.5f - 160.f;
        const float phaseY = modelCenterY - 26.f;
        phaseText.setPosition(sf::Vector2f(phaseX, phaseY));
        window.draw(phaseText);
    }

    float barY = modelTop + MODEL_PLACEHOLDER_H + 12.f;  // 血条在模型下方
    float barX = playerCenterX - HP_BAR_W * 0.5f;
    sf::RectangleShape bgBar(sf::Vector2f(HP_BAR_W, HP_BAR_H));
    bgBar.setPosition(sf::Vector2f(barX, barY));
    bgBar.setFillColor(sf::Color(35, 30, 40));
    // 无格挡：更透明的淡灰外框；有格挡：淡蓝外框
    bgBar.setOutlineColor(s.block > 0 ? sf::Color(170, 210, 255, 255) : sf::Color(200, 200, 210, 140));
    bgBar.setOutlineThickness(2.f);
    window.draw(bgBar);
    float hpRatio = s.maxHp > 0 ? static_cast<float>(s.currentHp) / s.maxHp : 0.f;
    if (hpRatio > 0.f) {
        sf::RectangleShape hpBar(sf::Vector2f(HP_BAR_W * hpRatio, HP_BAR_H));
        hpBar.setPosition(sf::Vector2f(barX, barY));
        // 无格挡为红色血条，有格挡为蓝色血条
        if (s.block > 0)
            hpBar.setFillColor(sf::Color(60, 120, 220));
        else
            hpBar.setFillColor(sf::Color(200, 60, 60));
        window.draw(hpBar);
    }
    if (s.block > 0) {
        // 血条左侧盾牌图标 + 格挡数字（画在血条之上）
        const float shieldR = 18.f;
        sf::CircleShape shield(shieldR, 20);
        float shieldCx = barX - shieldR + 2.f;
        float shieldCy = barY + HP_BAR_H * 0.5f;
        shield.setPosition(sf::Vector2f(shieldCx - shieldR, shieldCy - shieldR));
        shield.setFillColor(sf::Color(80, 150, 210));
        shield.setOutlineColor(sf::Color(230, 250, 255));
        shield.setOutlineThickness(2.f);
        window.draw(shield);

        std::snprintf(buf, sizeof(buf), "%d", s.block);
        sf::Text shieldText(font_, buf, 22);
        shieldText.setStyle(sf::Text::Bold);
        shieldText.setFillColor(sf::Color::White);
        shieldText.setOutlineThickness(2.f);
        shieldText.setOutlineColor(sf::Color(20, 60, 110)); // 深蓝描边
        const sf::FloatRect sb = shieldText.getLocalBounds();
        shieldText.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
        shieldText.setPosition(sf::Vector2f(shieldCx, shieldCy));
        window.draw(shieldText);
    }
    std::snprintf(buf, sizeof(buf), "%d/%d", s.currentHp, s.maxHp);
    sf::Text hpText(font_, buf, 20);
    hpText.setStyle(sf::Text::Bold);
    hpText.setFillColor(sf::Color::White);
    hpText.setOutlineThickness(2.f);
    hpText.setOutlineColor(sf::Color(120, 0, 0));  // 深红色描边
    // 粗一点并略宽于血条：水平居中，稍微向左右溢出
    const sf::FloatRect hpBounds = hpText.getLocalBounds();
    hpText.setOrigin(sf::Vector2f(hpBounds.position.x + hpBounds.size.x * 0.5f, hpBounds.position.y + hpBounds.size.y * 0.5f));
    hpText.setPosition(sf::Vector2f(barX + HP_BAR_W * 0.5f, barY + HP_BAR_H * 0.5f));
    window.draw(hpText);
    const float statusIconSz = 30.f;
    const unsigned statusFontSize = 20u;   // 下标字号
    const float statusGapX = statusIconSz + 14.f;
    const float statusGapY = statusIconSz + 8.f;
    const float stBaseX = barX;
    const float stBaseY = barY + HP_BAR_H + 6.f;
    int hoveredPlayerStatus = -1;
    auto is_positive_status_ui = [](const std::string& id) {
        return id == "strength" || id == "dexterity" || id == "metallicize" || id == "flex";
    };

    for (size_t i = 0; i < s.playerStatuses.size(); ++i) {
        const size_t col = i % 8u;
        const size_t row = i / 8u;
        const float stX = stBaseX + static_cast<float>(col) * statusGapX;
        const float stY = stBaseY + static_cast<float>(row) * statusGapY;
        const auto& st = s.playerStatuses[i];
        sf::RectangleShape icon(sf::Vector2f(statusIconSz, statusIconSz));
        icon.setPosition(sf::Vector2f(stX, stY));
        icon.setFillColor(sf::Color(90, 85, 100));
        icon.setOutlineColor(sf::Color(180, 170, 150));
        icon.setOutlineThickness(1.f);
        window.draw(icon);
        std::snprintf(buf, sizeof(buf), "%d", st.stacks);
        sf::Text stText(font_, buf, statusFontSize);
        // 正面效果：正值为绿色，负值为红色；负面效果下标始终白色
        if (is_positive_status_ui(st.id)) {
            if (st.stacks >= 0)
                stText.setFillColor(sf::Color(120, 230, 120));
            else
                stText.setFillColor(sf::Color(230, 120, 120));
        } else {
            stText.setFillColor(sf::Color::White);
        }
        const sf::FloatRect textBounds = stText.getLocalBounds();
        stText.setOrigin(sf::Vector2f(textBounds.size.x, textBounds.size.y));
        stText.setPosition(sf::Vector2f(stX + statusIconSz, stY + statusIconSz));
        window.draw(stText);

        // 悬停检测
        if (hoveredPlayerStatus < 0 &&
            mousePos_.x >= stX && mousePos_.x <= stX + statusIconSz &&
            mousePos_.y >= stY && mousePos_.y <= stY + statusIconSz) {
            hoveredPlayerStatus = static_cast<int>(i);
        }
    }

    // ---------- 怪物区：意图在模型上方 -> 模型占位 -> 血条在模型下方 ----------
    float monsterCenterX = monsterLeft + monsterW * 0.5f;
    float mModelTop = modelCenterY - MODEL_TOP_OFFSET;  // 上边不变
    const size_t nMonsters = s.monsters.size();

    if (nMonsters > 0) {
        const auto& m = s.monsters[0];
        // 意图：在模型上方
        float intentY = mModelTop - INTENT_ORB_R * 2.f - 8.f;
        sf::CircleShape intentOrb(INTENT_ORB_R);
        intentOrb.setPosition(sf::Vector2f(monsterCenterX - INTENT_ORB_R, intentY));
        intentOrb.setFillColor(sf::Color(80, 120, 200));
        intentOrb.setOutlineColor(sf::Color(120, 160, 255));
        intentOrb.setOutlineThickness(1.f);
        window.draw(intentOrb);
        {
            sf::String intentStr;
            switch (m.currentIntent.kind) {
            case MonsterIntentKind::Attack:
                intentStr = sf::String(L"攻击 ") + sf::String(std::to_wstring(m.currentIntent.value));
                break;
            case MonsterIntentKind::Block:
                intentStr = sf::String(L"防御");
                break;
            case MonsterIntentKind::Buff:
                intentStr = sf::String(L"强化");
                break;
            case MonsterIntentKind::Debuff:
                intentStr = sf::String(L"施加负面效果");
                break;
            case MonsterIntentKind::Sleep:
                intentStr = sf::String(L"睡眠");
                break;
            case MonsterIntentKind::Stun:
                intentStr = sf::String(L"晕眩");
                break;
            case MonsterIntentKind::Unknown:
            default:
                intentStr = sf::String(L"？？？");
                break;
            }
            sf::Text intentText(fontForChinese(), intentStr, 14);
            intentText.setFillColor(sf::Color(200, 220, 255));
            intentText.setPosition(sf::Vector2f(monsterCenterX - 40.f, intentY - 14.f));
            window.draw(intentText);
        }
    }

    sf::RectangleShape monsterModel(sf::Vector2f(MODEL_PLACEHOLDER_W, MODEL_PLACEHOLDER_H));
    monsterModel.setPosition(sf::Vector2f(monsterCenterX - MODEL_PLACEHOLDER_W * 0.5f, mModelTop));
    // 记录怪物模型矩形用于点击检测（目前仅支持单个怪物）
    monsterModelRect_ = sf::FloatRect(
        sf::Vector2f(monsterCenterX - MODEL_PLACEHOLDER_W * 0.5f, mModelTop),
        sf::Vector2f(MODEL_PLACEHOLDER_W, MODEL_PLACEHOLDER_H)
    );
    monsterModel.setFillColor(sf::Color(70, 55, 65));
    monsterModel.setOutlineColor(sf::Color(110, 90, 100));
    monsterModel.setOutlineThickness(1.f);
    window.draw(monsterModel);
    sf::Text monsterLabel(font_, nMonsters > 0 ? s.monsters[0].id.c_str() : "?", 18);
    monsterLabel.setFillColor(sf::Color(200, 180, 190));
    monsterLabel.setPosition(sf::Vector2f(monsterCenterX - 28.f, modelCenterY - 24.f));
    window.draw(monsterLabel);

    float mBarY = mModelTop + MODEL_PLACEHOLDER_H + 12.f;  // 血条在模型下方
    int hoveredMonsterStatus = -1;
    if (nMonsters > 0) {
        const auto& m = s.monsters[0];
        float mBarX = monsterCenterX - HP_BAR_W * 0.5f;
        sf::RectangleShape mbg(sf::Vector2f(HP_BAR_W, HP_BAR_H));
        mbg.setPosition(sf::Vector2f(mBarX, mBarY));
        mbg.setFillColor(sf::Color(35, 30, 40));
        mbg.setOutlineColor(m.block > 0 ? sf::Color(170, 210, 255, 255) : sf::Color(200, 200, 210, 140));
        mbg.setOutlineThickness(2.f);
        window.draw(mbg);
        float mhr = m.maxHp > 0 ? static_cast<float>(m.currentHp) / m.maxHp : 0.f;
        if (mhr > 0.f) {
            sf::RectangleShape mhp(sf::Vector2f(HP_BAR_W * mhr, HP_BAR_H));
            mhp.setPosition(sf::Vector2f(mBarX, mBarY));
            if (m.block > 0)
                mhp.setFillColor(sf::Color(60, 120, 220));
            else
                mhp.setFillColor(sf::Color(200, 60, 60));
            window.draw(mhp);
        }
        if (m.block > 0) {
            const float shieldR = 18.f;
            sf::CircleShape shield(shieldR, 20);
            float shieldCx = mBarX - shieldR + 2.f;
            float shieldCy = mBarY + HP_BAR_H * 0.5f;
            shield.setPosition(sf::Vector2f(shieldCx - shieldR, shieldCy - shieldR));
            shield.setFillColor(sf::Color(80, 150, 210));
            shield.setOutlineColor(sf::Color(230, 250, 255));
            shield.setOutlineThickness(2.f);
            window.draw(shield);

            std::snprintf(buf, sizeof(buf), "%d", m.block);
            sf::Text shieldText(font_, buf, 22);
            shieldText.setStyle(sf::Text::Bold);
            shieldText.setFillColor(sf::Color::White);
            shieldText.setOutlineThickness(2.f);
            shieldText.setOutlineColor(sf::Color(20, 60, 110)); // 深蓝描边
            const sf::FloatRect sb = shieldText.getLocalBounds();
            shieldText.setOrigin(sf::Vector2f(sb.position.x + sb.size.x * 0.5f, sb.position.y + sb.size.y * 0.5f));
            shieldText.setPosition(sf::Vector2f(shieldCx, shieldCy));
            window.draw(shieldText);
        }
        std::snprintf(buf, sizeof(buf), "%d/%d", m.currentHp, m.maxHp);
        sf::Text mhpText(font_, buf, 20);
        mhpText.setStyle(sf::Text::Bold);
        mhpText.setFillColor(sf::Color::White);
        mhpText.setOutlineThickness(2.f);
        mhpText.setOutlineColor(sf::Color(120, 0, 0));  // 深红色描边
        const sf::FloatRect mhpBounds = mhpText.getLocalBounds();
        mhpText.setOrigin(sf::Vector2f(mhpBounds.position.x + mhpBounds.size.x * 0.5f, mhpBounds.position.y + mhpBounds.size.y * 0.5f));
        mhpText.setPosition(sf::Vector2f(mBarX + HP_BAR_W * 0.5f, mBarY + HP_BAR_H * 0.5f));
        window.draw(mhpText);

        // 怪物状态图标：沿用玩家状态样式，每行最多 8 个，超过换行
        const float statusIconSz = 30.f;
        const unsigned statusFontSize = 20u;
        const float statusGapX = statusIconSz + 14.f;
        const float statusGapY = statusIconSz + 8.f;
        const float mstBaseX = mBarX;
        const float mstBaseY = mBarY + HP_BAR_H + 6.f;
        if (!s.monsterStatuses.empty()) {
            const auto& mStatuses = s.monsterStatuses[0];
            for (size_t i = 0; i < mStatuses.size(); ++i) {
                const size_t col = i % 8u;
                const size_t row = i / 8u;
                const float mstX = mstBaseX + static_cast<float>(col) * statusGapX;
                const float mstY = mstBaseY + static_cast<float>(row) * statusGapY;
                const auto& st = mStatuses[i];
                sf::RectangleShape icon(sf::Vector2f(statusIconSz, statusIconSz));
                icon.setPosition(sf::Vector2f(mstX, mstY));
                icon.setFillColor(sf::Color(85, 75, 95));
                icon.setOutlineColor(sf::Color(190, 180, 160));
                icon.setOutlineThickness(1.f);
                window.draw(icon);
                std::snprintf(buf, sizeof(buf), "%d", st.stacks);
                sf::Text stText(font_, buf, statusFontSize);
                // 怪物状态下标颜色规则同玩家
                if (is_positive_status_ui(st.id)) {
                    if (st.stacks >= 0)
                        stText.setFillColor(sf::Color(120, 230, 120));
                    else
                        stText.setFillColor(sf::Color(230, 120, 120));
                } else {
                    stText.setFillColor(sf::Color::White);
                }
                const sf::FloatRect textBounds = stText.getLocalBounds();
                stText.setOrigin(sf::Vector2f(textBounds.size.x, textBounds.size.y));
                stText.setPosition(sf::Vector2f(mstX + statusIconSz, mstY + statusIconSz));
                window.draw(stText);

                // 悬停检测
                if (hoveredMonsterStatus < 0 &&
                    mousePos_.x >= mstX && mousePos_.x <= mstX + statusIconSz &&
                    mousePos_.y >= mstY && mousePos_.y <= mstY + statusIconSz) {
                    hoveredMonsterStatus = static_cast<int>(i);
                }
            }
        }
    }

    // 玩家/怪物状态悬停提示：玩家放在模型右侧，怪物放在模型左侧；
    // 悬停任一图标时，为该单位的每个状态各画一个方框：第一行是名字，第二行是描述（内嵌层数），高度统一，宽度自适应
    auto makeStatusTooltipText = [&](const StatusInstance& st) {
        const std::string& id = st.id;
        int n = st.stacks;

        std::wstring name;
        std::wstring line2;

        if (id == "strength") {
            name = L"力量";
            line2 = L"攻击伤害提升 " + std::to_wstring(n);
        } else if (id == "vulnerable") {
            name = L"易伤";
            line2 = L"从攻击受到的伤害增加 50%，持续时间" + std::to_wstring(st.duration) + L"回合";
        } else if (id == "weak") {
            name = L"虚弱";
            line2 = L"攻击造成的伤害降低 25%，持续时间" + std::to_wstring(st.duration) + L"回合";
        } else if (id == "dexterity") {
            name = L"敏捷";
            line2 = L"从卡牌获得的格挡提升 " + std::to_wstring(n);
        } else if (id == "frail") {
            name = L"脆弱";
            line2 = L"在" + std::to_wstring(st.duration) + L"回合内,从卡牌中获得的格挡减少 25%";
        } else if (id == "entangle") {
            name = L"缠绕";
            line2 = L"在你的回合结束时，受到" + std::to_wstring(n) + L"点伤害";
        } else if (id == "shackles") {
            name = L"镣铐";
            line2 = L"在其回合结束时，回复 " + std::to_wstring(n) + L" 点力量";
        } else if (id == "flex") {
            name = L"活动肌肉";
            line2 = L"在你的回合结束时，失去" + std::to_wstring(n) + L"点力量";
        } else if (id == "dexterity_down") {
            name = L"敏捷下降";
            line2 = L"在你的回合结束时，失去 " + std::to_wstring(n) + L" 点敏捷";
        } else if (id == "draw_reduction") {
            name = L"抽牌减少";
            line2 = L"下 " + std::to_wstring(n) + L" 个回合内，每回合少抽 1 张牌";
        } else if (id == "poison") {
            name = L"中毒";
            line2 = L"在回合开始时，受到 " + std::to_wstring(n) + L" 点伤害，然后中毒层数减少 1";
        } else if (id == "metallicize") {
            name = L"金属化";
            line2 = L"在你的回合结束时，获得 " + std::to_wstring(n) + L" 点格挡";
        } else {
            name = sf::String(id).toWideString();
            line2 = L"效果层数 " + std::to_wstring(n);
        }

        return name + L"\n" + line2;
    };

    if (hoveredPlayerStatus >= 0 && !s.playerStatuses.empty()) {
        const float paddingX = 10.f;
        const float paddingY = 6.f;
        const float boxGapY = 6.f;
        const float boxLeft = playerCenterX + MODEL_PLACEHOLDER_W * 0.5f + 16.f;
        float boxTop = modelTop + 8.f;

        // 先计算统一的高度和最大宽度
        float boxHeight = -1.f;
        float maxBoxWidth = 0.f;
        for (size_t i = 0; i < s.playerStatuses.size(); ++i) {
            const auto& st = s.playerStatuses[i];
            std::wstring desc = makeStatusTooltipText(st);
            sf::Text tip(fontForChinese(), sf::String(desc), 20);
            const sf::FloatRect tb = tip.getLocalBounds();
            if (boxHeight < 0.f)
                boxHeight = tb.size.y + paddingY * 2.f;
            float w = tb.size.x + paddingX * 2.f;
            if (w > maxBoxWidth) maxBoxWidth = w;
        }

        // 再按统一宽度逐个绘制
        for (size_t i = 0; i < s.playerStatuses.size(); ++i) {
            const auto& st = s.playerStatuses[i];
            std::wstring desc = makeStatusTooltipText(st);
            sf::Text tip(fontForChinese(), sf::String(desc), 20);
            tip.setFillColor(sf::Color(235, 230, 220));
            const sf::FloatRect tb = tip.getLocalBounds();
            sf::RectangleShape bg(sf::Vector2f(maxBoxWidth, boxHeight));
            bg.setPosition(sf::Vector2f(boxLeft, boxTop));
            bg.setFillColor(sf::Color(40, 35, 45, 230));
            bg.setOutlineColor(sf::Color(150, 140, 120));
            bg.setOutlineThickness(1.f);
            window.draw(bg);
            tip.setPosition(sf::Vector2f(boxLeft + paddingX - tb.position.x, boxTop + paddingY - tb.position.y));
            window.draw(tip);
            boxTop += boxHeight + boxGapY;
        }
    }

    if (nMonsters > 0 && hoveredMonsterStatus >= 0 &&
        !s.monsterStatuses.empty() && !s.monsterStatuses[0].empty()) {
        const auto& mStatuses = s.monsterStatuses[0];
        const float paddingX = 10.f;
        const float paddingY = 6.f;
        const float boxGapY = 6.f;
        const float boxRight = monsterCenterX - MODEL_PLACEHOLDER_W * 0.5f - 16.f;
        float boxTop = mModelTop + 8.f;

        // 先计算统一的高度和最大宽度
        float boxHeight = -1.f;
        float maxBoxWidth = 0.f;
        for (size_t i = 0; i < mStatuses.size(); ++i) {
            const auto& st = mStatuses[i];
            std::wstring desc = makeStatusTooltipText(st);
            sf::Text tip(fontForChinese(), sf::String(desc), 20);
            const sf::FloatRect tb = tip.getLocalBounds();
            if (boxHeight < 0.f)
                boxHeight = tb.size.y + paddingY * 2.f;
            float w = tb.size.x + paddingX * 2.f;
            if (w > maxBoxWidth) maxBoxWidth = w;
        }

        // 再按统一宽度逐个绘制
        for (size_t i = 0; i < mStatuses.size(); ++i) {
            const auto& st = mStatuses[i];
            std::wstring desc = makeStatusTooltipText(st);
            sf::Text tip(fontForChinese(), sf::String(desc), 20);
            tip.setFillColor(sf::Color(235, 230, 220));
            const sf::FloatRect tb = tip.getLocalBounds();
            const float boxLeft = boxRight - maxBoxWidth;
            sf::RectangleShape bg(sf::Vector2f(maxBoxWidth, boxHeight));
            bg.setPosition(sf::Vector2f(boxLeft, boxTop));
            bg.setFillColor(sf::Color(40, 35, 45, 230));
            bg.setOutlineColor(sf::Color(150, 140, 120));
            bg.setOutlineThickness(1.f);
            window.draw(bg);
            tip.setPosition(sf::Vector2f(boxLeft + paddingX - tb.position.x, boxTop + paddingY - tb.position.y));
            window.draw(tip);
            boxTop += boxHeight + boxGapY;
        }
    }
}

// 底栏：抽牌堆左下角(放大) | 能量在其右上方(放大) | 手牌 | 结束回合在弃牌左上方(放大) | 弃牌堆右下角(放大)，卡牌放大
void BattleUI::drawBottomBar(sf::RenderWindow& window, const BattleStateSnapshot& s) {
    const float barY = height_ * BOTTOM_BAR_Y_RATIO;
    char buf[24];

    // 抽牌堆：往左 4px
    const float drawPileX = SIDE_MARGIN + PILE_CENTER_OFFSET - 4.f;
    const float drawPileY = height_ - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
    sf::RectangleShape drawPileIcon(sf::Vector2f(PILE_ICON_W, PILE_ICON_H));
    drawPileIcon.setPosition(sf::Vector2f(drawPileX, drawPileY));
    drawPileIcon.setFillColor(sf::Color(120, 50, 50));
    drawPileIcon.setOutlineColor(sf::Color(180, 80, 80));
    drawPileIcon.setOutlineThickness(2.f);
    window.draw(drawPileIcon);
    const float drawNumCx = drawPileX + PILE_ICON_W;
    const float drawNumCy = drawPileY + PILE_ICON_H;
    sf::CircleShape drawNumBg(PILE_NUM_CIRCLE_R);
    drawNumBg.setPosition(sf::Vector2f(drawNumCx - PILE_NUM_CIRCLE_R, drawNumCy - PILE_NUM_CIRCLE_R));
    drawNumBg.setFillColor(sf::Color(80, 30, 30));
    drawNumBg.setOutlineColor(sf::Color(180, 80, 80));
    drawNumBg.setOutlineThickness(1.f);
    window.draw(drawNumBg);
    std::snprintf(buf, sizeof(buf), "%d", s.drawPileSize);
    sf::Text drawNum(font_, buf, 26);
    drawNum.setFillColor(sf::Color(255, 220, 220));
    const sf::FloatRect drawNumB = drawNum.getLocalBounds();
    drawNum.setOrigin(sf::Vector2f(drawNumB.position.x + drawNumB.size.x * 0.5f, drawNumB.position.y + drawNumB.size.y * 0.5f));
    drawNum.setPosition(sf::Vector2f(drawNumCx, drawNumCy));
    window.draw(drawNum);

    // 能量：以左下为 (0,0)，圆心在 (190, 190)，直径 130 的圆
    const float energyCenterX = ENERGY_CENTER_X_BL;
    const float energyCenterY = static_cast<float>(height_) - ENERGY_CENTER_Y_BL;
    const float energyLeft = energyCenterX - ENERGY_R;
    const float energyTop = energyCenterY - ENERGY_R;
    sf::CircleShape energyCircle(ENERGY_R);
    energyCircle.setPosition(sf::Vector2f(energyLeft, energyTop));
    energyCircle.setFillColor(sf::Color(80, 50, 20));
    energyCircle.setOutlineColor(sf::Color(255, 160, 60));
    energyCircle.setOutlineThickness(ENERGY_OUTLINE_THICKNESS);
    window.draw(energyCircle);
    std::snprintf(buf, sizeof(buf), "%d/%d", s.energy, s.maxEnergy);
    sf::Text energyText(font_, buf, 48);
    energyText.setStyle(sf::Text::Bold);
    energyText.setFillColor(sf::Color(255, 200, 100));
    const sf::FloatRect eb = energyText.getLocalBounds();
    energyText.setOrigin(sf::Vector2f(eb.position.x + eb.size.x * 0.5f, eb.position.y + eb.size.y * 0.5f));
    energyText.setPosition(sf::Vector2f(energyCenterX, energyCenterY));
    window.draw(energyText);

    // 手牌：弧心在屏幕中心，弧顶距底边 220；总角度由「展示宽度」反推，使相邻牌心水平间距约 HAND_CARD_DISPLAY_STEP
    const size_t handCount = s.hand.size();
    constexpr float DEG2RAD = 3.14159265f / 180.f;
    const float pivotX = static_cast<float>(width_) * 0.5f;
    const float pivotY = static_cast<float>(height_) + HAND_PIVOT_Y_BELOW;
    const float arcTopCenterY = static_cast<float>(height_) - HAND_ARC_TOP_ABOVE_BOTTOM + CARD_H * 0.5f;
    const float arcRadius = pivotY - arcTopCenterY;
    float handFanSpanDeg = (handCount > 1)
        ? (static_cast<float>(handCount - 1) * HAND_CARD_DISPLAY_STEP / arcRadius * (180.f / 3.14159265f))
        : 0.f;
    if (handFanSpanDeg > HAND_FAN_SPAN_MAX_DEG) handFanSpanDeg = HAND_FAN_SPAN_MAX_DEG;
    const float angleStepDeg = (handCount > 1) ? (handFanSpanDeg / static_cast<float>(handCount - 1)) : 0.f;
    const float angleStepRad = angleStepDeg * DEG2RAD;

    auto getCardPos = [&](size_t idx, bool addHoverLift, float& out_cx, float& out_cy, float& out_angleDeg) {
        const float angleDeg = handCount > 1 ? (static_cast<float>(idx) - (handCount - 1) * 0.5f) * angleStepDeg : 0.f;
        const float rad = angleDeg * DEG2RAD;
        out_cx = pivotX + arcRadius * std::sin(rad);
        out_cy = pivotY - arcRadius * std::cos(rad);
        if (addHoverLift) out_cy -= 28.f;
        out_angleDeg = angleDeg;
    };

    int hoverIndex = -1;
    // 若当前没有选中牌，则正常根据鼠标位置计算 hover；选中后 hover 不再影响展示
    if (selectedHandIndex_ < 0) {
        for (int i = static_cast<int>(handCount) - 1; i >= 0; --i) {
            float cx_i, cy_i, angleDeg;
            getCardPos(static_cast<size_t>(i), false, cx_i, cy_i, angleDeg);
            const float cardLeft = cx_i - CARD_W * 0.5f;
            const float cardTop = cy_i - CARD_H * 0.5f;
            if (mousePos_.x >= cardLeft && mousePos_.x <= cardLeft + CARD_W && mousePos_.y >= cardTop && mousePos_.y <= cardTop + CARD_H) {
                hoverIndex = i;
                // 左键点击时开始选中该牌（由 handleEvent 设置 selectedHandIndex_）
                break;
            }
        }
        // 若本帧没有显式选中牌且有 hover，按 hover 行为处理
        if (hoverIndex >= 0 && selectedHandIndex_ < 0 && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
            selectedHandIndex_ = hoverIndex;
            // 选中时即进入“出牌瞄准/选择目标”阶段，根据卡牌类型决定是否需要敌人目标
            selectedCardTargetsEnemy_ = card_targets_enemy(s, static_cast<size_t>(hoverIndex));
            isAimingCard_ = true;
            // 以原位下这张牌的默认中心为跟随区域中心
            float cx, cy, ang;
            getCardPos(static_cast<size_t>(hoverIndex), false, cx, cy, ang);
            selectedCardOriginPos_ = sf::Vector2f(cx, cy);
            selectedCardIsFollowing_ = true;
            selectedCardInsideOriginRect_ = true;
        }
    }

    auto drawOneCard = [&](size_t idx, bool isHover, bool isSelected) {
        float cx_i, cy_i, angleDeg;
        getCardPos(idx, isHover, cx_i, cy_i, angleDeg);
        float w, h;
        if (isSelected) {
            // 选中牌：根据是否需要敌人目标决定行为
            const float halfW = CARD_W * 0.5f;
            const float halfH = CARD_H * 0.5f;
            sf::FloatRect followRect(
                sf::Vector2f(selectedCardOriginPos_.x - halfW, selectedCardOriginPos_.y - halfH),
                sf::Vector2f(CARD_W, CARD_H));
            bool inside = followRect.contains(mousePos_);
            selectedCardInsideOriginRect_ = inside;

            if (selectedCardTargetsEnemy_) {
                // 敌人目标牌：在原位矩形内以预览大小跟随鼠标；离开后回到默认大小固定在屏幕中下方，并进入瞄准阶段
                if (inside) {
                    selectedCardIsFollowing_ = true;
                    cx_i = mousePos_.x;
                    cy_i = mousePos_.y;
                    w = CARD_PREVIEW_W;
                    h = CARD_PREVIEW_H;
                } else {
                    selectedCardIsFollowing_ = false;
                    cx_i = static_cast<float>(width_) * 0.5f;
                    cy_i = static_cast<float>(height_) - CARD_PREVIEW_BOTTOM_ABOVE - CARD_H * 0.5f;
                    w = CARD_W;
                    h = CARD_H;
                }
            } else {
                // 自选（玩家）目标牌：始终以预览大小跟随鼠标，仅用原位矩形判断是否高亮玩家
                selectedCardIsFollowing_ = true;
                cx_i = mousePos_.x;
                cy_i = mousePos_.y;
                w = CARD_PREVIEW_W;
                h = CARD_PREVIEW_H;
            }
            angleDeg = 0.f;
        } else if (isHover) {
            w = CARD_PREVIEW_W;
            h = CARD_PREVIEW_H;
            angleDeg = 0.f;  // 预览时旋转归零
            cy_i = static_cast<float>(height_) - CARD_PREVIEW_BOTTOM_ABOVE - h * 0.5f;  // 下边距底 5
        } else {
            w = CARD_W;
            h = CARD_H;
        }

        // 记录选中牌当前中心位置，供箭头使用
        if (isSelected) {
            selectedCardScreenPos_ = sf::Vector2f(cx_i, cy_i);
        }

        sf::Transform tr;
        tr.translate(sf::Vector2f(cx_i, cy_i));
        tr.rotate(sf::degrees(angleDeg));
        tr.translate(sf::Vector2f(-w * 0.5f, -h * 0.5f));
        sf::RenderStates states(tr);

        const float pad = 4.f;
        const float innerL = pad;
        const float innerT = pad;
        const float innerW = w - pad * 2.f;

        sf::RectangleShape cardBg(sf::Vector2f(w, h));
        cardBg.setPosition(sf::Vector2f(0.f, 0.f));
        cardBg.setFillColor(sf::Color(55, 50, 48));
        cardBg.setOutlineColor(sf::Color(180, 50, 45));
        cardBg.setOutlineThickness(isHover ? 12.f : 8.f);
        window.draw(cardBg, states);

        const float titleY = innerT + 24.f;
        const float titleH = 32.f;
        sf::RectangleShape titleBar(sf::Vector2f(innerW - 16.f, titleH));
        titleBar.setPosition(sf::Vector2f(innerL + 8.f, titleY));
        titleBar.setFillColor(sf::Color(72, 68, 65));
        titleBar.setOutlineColor(sf::Color(90, 85, 82));
        titleBar.setOutlineThickness(1.f);
        window.draw(titleBar, states);

        const float artTop = titleY + titleH + 4.f;
        const float artH = 98.f;
        sf::ConvexShape artPanel;
        artPanel.setPointCount(8);
        artPanel.setPoint(0, sf::Vector2f(innerL, artTop));
        artPanel.setPoint(1, sf::Vector2f(innerL + innerW, artTop));
        artPanel.setPoint(2, sf::Vector2f(innerL + innerW, artTop + artH - 12.f));
        artPanel.setPoint(3, sf::Vector2f(innerL + innerW * 0.75f, artTop + artH));
        artPanel.setPoint(4, sf::Vector2f(innerL + innerW * 0.5f, artTop + artH - 10.f));
        artPanel.setPoint(5, sf::Vector2f(innerL + innerW * 0.25f, artTop + artH));
        artPanel.setPoint(6, sf::Vector2f(innerL, artTop + artH - 12.f));
        artPanel.setPoint(7, sf::Vector2f(innerL, artTop));
        artPanel.setFillColor(sf::Color(120, 45, 42));
        artPanel.setOutlineColor(sf::Color(100, 38, 35));
        artPanel.setOutlineThickness(1.f);
        window.draw(artPanel, states);

        const float typeY = artTop + artH + 6.f;
        const float typeH = 26.f;
        sf::RectangleShape typeBar(sf::Vector2f(innerW - 24.f, typeH));
        typeBar.setPosition(sf::Vector2f(innerL + 12.f, typeY));
        typeBar.setFillColor(sf::Color(72, 68, 65));
        typeBar.setOutlineColor(sf::Color(90, 85, 82));
        typeBar.setOutlineThickness(1.f);
        window.draw(typeBar, states);

        const float costR = 22.f;
        const float costCx = innerL + costR - 10.f;
        const float costCy = innerT + costR - 10.f;
        sf::CircleShape costRing(costR + 4.f);
        costRing.setPosition(sf::Vector2f(costCx - costR - 4.f, costCy - costR - 4.f));
        costRing.setFillColor(sf::Color(0, 0, 0, 0));
        costRing.setOutlineColor(sf::Color(255, 190, 90));
        costRing.setOutlineThickness(3.f);
        window.draw(costRing, states);
        sf::CircleShape costCircle(costR);
        costCircle.setPosition(sf::Vector2f(costCx - costR, costCy - costR));
        costCircle.setFillColor(sf::Color(200, 55, 50));
        costCircle.setOutlineColor(sf::Color(255, 190, 90));
        costCircle.setOutlineThickness(2.f);
        window.draw(costCircle, states);

        const CardData* cd = get_card_by_id(s.hand[idx].id);
        int cost = cd ? cd->cost : 1;
        std::snprintf(buf, sizeof(buf), "%d", cost);
        sf::Text costText(font_, buf, 26);
        costText.setFillColor(sf::Color::White);
        const sf::FloatRect cb = costText.getLocalBounds();
        costText.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
        costText.setPosition(sf::Vector2f(costCx, costCy));
        window.draw(costText, states);

        sf::String cardName;
        if (cd && !cd->name.empty()) {
            cardName = sf::String::fromUtf8(cd->name.begin(), cd->name.end());
        } else if (s.hand[idx].id.empty()) {
            cardName = sf::String(L"?");
        } else {
            cardName = sf::String(s.hand[idx].id);
        }
        sf::Text nameText(fontForChinese(), cardName, 20);
        nameText.setFillColor(sf::Color::White);
        const sf::FloatRect nb = nameText.getLocalBounds();
        nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
        nameText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, titleY + titleH * 0.5f));
        window.draw(nameText, states);

        sf::String typeStr = sf::String(L"?");
        if (cd) {
            switch (cd->cardType) {
            case CardType::Attack: typeStr = sf::String(L"攻击"); break;
            case CardType::Skill:  typeStr = sf::String(L"技能"); break;
            case CardType::Power:  typeStr = sf::String(L"能力"); break;
            case CardType::Status: typeStr = sf::String(L"状态"); break;
            case CardType::Curse:  typeStr = sf::String(L"诅咒"); break;
            }
        }
        sf::Text typeText(fontForChinese(), typeStr, 16);
        typeText.setFillColor(sf::Color::White);
        const sf::FloatRect tb = typeText.getLocalBounds();
        typeText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        typeText.setPosition(sf::Vector2f(innerL + innerW * 0.5f, typeY + typeH * 0.5f));
        window.draw(typeText, states);

        sf::String descStr;
        if (cd && !cd->description.empty()) {
            descStr = sf::String::fromUtf8(cd->description.begin(), cd->description.end());
        } else {
            descStr = sf::String(L"");
        }
        sf::Text descText(fontForChinese(), descStr, 15);
        descText.setFillColor(sf::Color(240, 238, 235));
        descText.setPosition(sf::Vector2f(innerL + 12.f, typeY + typeH + 14.f));
        window.draw(descText, states);
    };

    for (size_t i = 0; i < handCount; ++i) {
        if (static_cast<int>(i) == selectedHandIndex_) continue;
        if (hoverIndex >= 0 && static_cast<size_t>(hoverIndex) == i)
            drawOneCard(i, true, false);
        else
            drawOneCard(i, false, false);
    }
    if (selectedHandIndex_ >= 0 && static_cast<size_t>(selectedHandIndex_) < handCount) {
        drawOneCard(static_cast<size_t>(selectedHandIndex_), false, true);
    } else if (hoverIndex >= 0) {
        drawOneCard(static_cast<size_t>(hoverIndex), true, false);
    }

    bool aimingAtMonster = false;

    // 若正处于瞄准状态，且当前牌需要敌人目标，则从选中牌中心到鼠标位置绘制一条弧形箭头
    if (selectedHandIndex_ >= 0 && isAimingCard_ && selectedCardTargetsEnemy_) {
        // 跟随阶段不显示箭头（但仍继续绘制后面的弃牌堆、结束回合按钮等 UI）
        if (selectedCardIsFollowing_) {
            // 仍在原位矩形内跟随鼠标，仅展示预览牌本身
        } else {
        sf::Vector2f start = selectedCardScreenPos_;
        sf::Vector2f end = mousePos_;
        sf::Vector2f mid = (start + end) * 0.5f;
        // 向上抬一点，形成弧形
        mid.y -= 60.f;

        // 根据是否悬停在怪物上决定箭头颜色
        aimingAtMonster = monsterModelRect_.contains(end);
        sf::Color arrowColor = aimingAtMonster ? sf::Color(240, 90, 90) : sf::Color(200, 230, 255);

        // 用多条弧线叠加成更粗的实线箭头（只画到三角形基底，避免出现“两条线”）
        const int segments = 20;

        // 此时已离开跟随区域，可以画完整的线+箭头
        // 箭头三角形，线段不超过三角形基底
        sf::Vector2f dir = end - mid;
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len > 0.001f) {
            dir /= len;
            sf::Vector2f n(-dir.y, dir.x);
            const float headLen = 40.f;   // 箭头更长
            const float headWidth = 26.f; // 箭头更宽
            sf::Vector2f tip = end;
            sf::Vector2f base = end - dir * headLen;

            // 重新绘制一遍主线，让其只到达三角形基底（不伸出三角形外）
            auto drawMainCurveToBase = [&](float offset) {
                sf::VertexArray curve(sf::PrimitiveType::LineStrip, static_cast<size_t>(segments + 1));
                sf::Vector2f dirSB = base - start;
                float lenSB = std::sqrt(dirSB.x * dirSB.x + dirSB.y * dirSB.y);
                sf::Vector2f nSB(0.f, 0.f);
                if (lenSB > 0.001f) {
                    dirSB /= lenSB;
                    nSB = sf::Vector2f(-dirSB.y, dirSB.x);
                }
                for (int i = 0; i <= segments; ++i) {
                    float t = static_cast<float>(i) / static_cast<float>(segments);
                    float u = 1.f - t;
                    sf::Vector2f p = u * u * start + 2.f * u * t * ((start + base) * 0.5f - sf::Vector2f(0.f, 60.f)) + t * t * base;
                    p += nSB * offset;
                    curve[static_cast<size_t>(i)].position = p;
                    curve[static_cast<size_t>(i)].color = arrowColor;
                }
                window.draw(curve);
            };

            // 粗主线（到达基底，整体更粗）
            drawMainCurveToBase(0.f);
            drawMainCurveToBase(3.f);
            drawMainCurveToBase(-3.f);
            drawMainCurveToBase(6.f);
            drawMainCurveToBase(-6.f);

            // 三角形本体
            sf::Vector2f left = base + n * headWidth;
            sf::Vector2f right = base - n * headWidth;
            sf::VertexArray arrow(sf::PrimitiveType::Triangles, 3);
            arrow[0].position = tip;
            arrow[1].position = left;
            arrow[2].position = right;
            arrow[0].color = arrow[1].color = arrow[2].color = arrowColor;
            window.draw(arrow);
        }
        }
        // 若正在瞄准怪物，在怪物模型周围画黄色目标框
        if (aimingAtMonster) {
            sf::RectangleShape targetRect;
            targetRect.setPosition(monsterModelRect_.position);
            targetRect.setSize(monsterModelRect_.size);
            targetRect.setFillColor(sf::Color(0, 0, 0, 0));
            targetRect.setOutlineColor(sf::Color(255, 230, 120));
            targetRect.setOutlineThickness(3.f);
            window.draw(targetRect);
        }
    }

    // 自选（玩家）目标牌：当鼠标已离开原位矩形时，高亮玩家模型，提示“默认选中玩家”
    if (selectedHandIndex_ >= 0 && isAimingCard_ && !selectedCardTargetsEnemy_ && !selectedCardInsideOriginRect_) {
        sf::RectangleShape targetRect;
        targetRect.setPosition(playerModelRect_.position);
        targetRect.setSize(playerModelRect_.size);
        targetRect.setFillColor(sf::Color(0, 0, 0, 0));
        targetRect.setOutlineColor(sf::Color(255, 230, 120));
        targetRect.setOutlineThickness(3.f);
        window.draw(targetRect);
    }

    // 弃牌堆：往右 4px，往上 4px
    const float discardX = width_ - SIDE_MARGIN - PILE_ICON_W - PILE_CENTER_OFFSET + 4.f;
    const float discardY = height_ - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
    if (s.exhaustPileSize > 0) {
        sf::RectangleShape exhaustIcon(sf::Vector2f(PILE_ICON_W - 6.f, 48.f));
        exhaustIcon.setPosition(sf::Vector2f(discardX + 3.f, discardY - 56.f));
        exhaustIcon.setFillColor(sf::Color(70, 70, 70));
        exhaustIcon.setOutlineColor(sf::Color(100, 100, 100));
        exhaustIcon.setOutlineThickness(1.f);
        window.draw(exhaustIcon);
        std::snprintf(buf, sizeof(buf), "%d", s.exhaustPileSize);
        sf::Text exNum(font_, buf, 16);
        exNum.setFillColor(sf::Color(180, 180, 180));
        exNum.setPosition(sf::Vector2f(discardX + 14.f, discardY - 48.f));
        window.draw(exNum);
    }
    sf::RectangleShape discardIcon(sf::Vector2f(PILE_ICON_W, PILE_ICON_H));
    discardIcon.setPosition(sf::Vector2f(discardX, discardY));
    discardIcon.setFillColor(sf::Color(50, 50, 120));
    discardIcon.setOutlineColor(sf::Color(80, 80, 180));
    discardIcon.setOutlineThickness(2.f);
    window.draw(discardIcon);
    const float discardNumCx = discardX;
    const float discardNumCy = discardY + PILE_ICON_H;
    sf::CircleShape discardNumBg(PILE_NUM_CIRCLE_R);
    discardNumBg.setPosition(sf::Vector2f(discardNumCx - PILE_NUM_CIRCLE_R, discardNumCy - PILE_NUM_CIRCLE_R));
    discardNumBg.setFillColor(sf::Color(30, 30, 80));
    discardNumBg.setOutlineColor(sf::Color(80, 80, 180));
    discardNumBg.setOutlineThickness(1.f);
    window.draw(discardNumBg);
    std::snprintf(buf, sizeof(buf), "%d", s.discardPileSize);
    sf::Text discardNum(font_, buf, 26);
    discardNum.setFillColor(sf::Color(220, 220, 255));
    const sf::FloatRect discardNumB = discardNum.getLocalBounds();
    discardNum.setOrigin(sf::Vector2f(discardNumB.position.x + discardNumB.size.x * 0.5f, discardNumB.position.y + discardNumB.size.y * 0.5f));
    discardNum.setPosition(sf::Vector2f(discardNumCx, discardNumCy));
    window.draw(discardNum);

    // 结束回合：以右下为 (0,0) 时中心 (280, 210)，大小 180×75
    sf::RectangleShape btn(endTurnButton_.size);
    btn.setPosition(endTurnButton_.position);
    btn.setFillColor(sf::Color(140, 140, 150));
    btn.setOutlineColor(sf::Color(180, 180, 190));
    btn.setOutlineThickness(2.f);
    window.draw(btn);
    const float btnCx = endTurnButton_.position.x + endTurnButton_.size.x * 0.5f;
    const float btnCy = endTurnButton_.position.y + endTurnButton_.size.y * 0.5f;
    sf::Text btnText(fontForChinese(), sf::String(L"结束回合"), 26);
    btnText.setFillColor(sf::Color::White);
    const sf::FloatRect bt = btnText.getLocalBounds();
    btnText.setOrigin(sf::Vector2f(bt.position.x + bt.size.x * 0.5f, bt.position.y + bt.size.y * 0.5f));
    btnText.setPosition(sf::Vector2f(btnCx, btnCy));
    window.draw(btnText);
}

// 右上角：地图、牌组、设置按钮 + 回合数（顶栏下方）
void BattleUI::drawTopRight(sf::RenderWindow& window, const BattleStateSnapshot& s) {
    const float btnW = 58.f;
    const float btnH = 48.f;
    const float gap = 18.f;
    const float right = width_ - 28.f;
    const float rowY = TOP_ROW_Y;  // 按钮整体上移一点

    float x = right - (btnW * 3.f + gap * 2.f);

    auto drawBtn = [&](const std::wstring& label) {
        sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
        btn.setPosition(sf::Vector2f(x, rowY));
        btn.setFillColor(sf::Color(70, 65, 75));
        btn.setOutlineColor(sf::Color(120, 115, 125));
        btn.setOutlineThickness(1.f);
        window.draw(btn);
        sf::Text t(fontForChinese(), sf::String(label), 18);
        t.setFillColor(sf::Color(220, 215, 225));
        t.setPosition(sf::Vector2f(x + 10.f, rowY + 12.f));
        window.draw(t);
        x += btnW + gap;
    };

    drawBtn(L"地图");
    drawBtn(L"牌组");
    drawBtn(L"设置");

    // 顶栏下方显示当前回合数，居中对齐在这三个按钮下方
    const float groupWidth = btnW * 3.f + gap * 2.f;
    const float turnCenterX = right - groupWidth * 0.5f;
    const float turnRowY = rowY + btnH + 18.f;  // 回合文本再往下
    int turn = std::max(1, s.turnNumber);
    std::wstring turnStr = L"回合 " + std::to_wstring(turn);
    sf::Text turnText(fontForChinese(), sf::String(turnStr), 38);  // 字体变大
    turnText.setFillColor(sf::Color(220, 210, 200));
    const sf::FloatRect tb = turnText.getLocalBounds();
    turnText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, 0.f));
    turnText.setPosition(sf::Vector2f(turnCenterX, turnRowY));
    window.draw(turnText);
}

} // namespace tce
