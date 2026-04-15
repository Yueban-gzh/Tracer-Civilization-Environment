/**
 * Battle UI - 严格按照参考图布局：无大色块，顶栏一行+遗物行+战场+底栏(能量/抽牌/手牌/结束回合/弃牌)
 */
#include "BattleEngine/BattleUI.hpp"               // BattleUI 类声明
#include "BattleEngine/BattleStateSnapshot.hpp"    // 战斗状态快照结构
#include "CardSystem/CardSystem.hpp"               // CardInstance（本场减费展示）
#include "BattleCoreRefactor/PotionEffects.hpp"    // 灵液需目标判断
#include "DataLayer/DataLayer.hpp"                 // 卡牌/怪物数据查询
#include "Common/ImagePath.hpp"
#include "Common/UserSettings.hpp"
#include <SFML/Graphics.hpp>                       // 图形绘制
#include <algorithm>                               // std::min/max 等
#include <array>
#include <map>                                     // std::map（按目标统计伤害序号，用于错位）
#include <cmath>                                   // std::sqrt/floor
#include <cctype>                                  // std::toupper
#include <cstdio>                                  // std::snprintf
#include <filesystem>                              // u8path：含空格/中文的状态图标路径
#include <cstdint>                                 // std::uint8_t
#include <cstring>                                 // std::strlen
#include <iostream>                                // 选牌调试输出（控制台）
#include <string>                                  // std::string
#include <unordered_map>
#include <unordered_set>                           // 抽/弃牌动画 diff
#include <vector>                                  // 牌面圆角轮廓点

namespace {

inline float ui_hover_lerp(float current, float target, float dt, float speed = 16.f) {
    if (dt <= 0.f) return target;
    const float k = 1.f - std::exp(-speed * dt);
    return current + (target - current) * k;
}

inline std::uint8_t ui_hover_lighten_byte(std::uint8_t v, float hover01, int maxDelta = 42) {
    const int add = static_cast<int>(static_cast<float>(maxDelta) * hover01);
    return static_cast<std::uint8_t>(std::min(255, static_cast<int>(v) + add));
}

// 将 0~1 的时间映射为“前冲再回弹”的位移系数
inline float ui_attack_lunge01(float t01) {
    if (t01 <= 0.f) return 0.f;
    if (t01 >= 1.f) return 0.f;
    // 更“沉”的前冲：先加速后减速（峰值略靠前），回弹更柔
    const float t = t01;
    const float s = std::sin(3.1415926f * t);
    const float w = 0.88f + 0.12f * (1.f - t); // 前段略更强，后段略收
    return s * w;
}

} // namespace

namespace tce {

/** SFML::loadFromFile(string) 在 MSVC 上易与系统编码不一致；含空格/中文路径用 u8path。 */
static bool load_sf_texture_utf8(sf::Texture& tex, const std::string& utf8Path) {
    if (utf8Path.empty())
        return false;
    namespace fs = std::filesystem;
    const fs::path p = fs::u8path(utf8Path);
    if (tex.loadFromFile(p))
        return true;
    if (p.is_absolute())
        return false;
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (ec)
        return false;
    return tex.loadFromFile(cwd / p);
}

/** 仅处理 ASCII A–Z，用于匹配磁盘上的 Icon 文件名（与 wiki 主名一致）。 */
static std::string ascii_lower_ascii(std::string s) {
    for (char& c : s) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc >= 'A' && uc <= 'Z')
            c = static_cast<char>(uc - 'A' + 'a');
    }
    return s;
}

/** 意图贴图查找：先精确键，再 ASCII 不区分大小写（与预载键 Attack 与 JSON 里 attack 对齐） */
static const sf::Texture* lookup_intention_texture(const std::unordered_map<std::string, sf::Texture>& m, const std::string& id) {
    if (id.empty()) return nullptr;
    auto it = m.find(id);
    if (it != m.end()) return &it->second;
    const std::string want = ascii_lower_ascii(id);
    for (const auto& kv : m) {
        if (ascii_lower_ascii(kv.first) == want)
            return &kv.second;
    }
    return nullptr;
}

/** 在意图图标上叠画伤害数字（按 kind，与悬停说明一致） */
static bool monster_intent_shows_attack_value_on_icon(MonsterIntentKind k) {
    switch (k) {
    case MonsterIntentKind::Attack:
    case MonsterIntentKind::Mul_Attack:
    case MonsterIntentKind::Attack_And_Weak:
    case MonsterIntentKind::Attack_And_Vulnerable:
        return true;
    default:
        return false;
    }
}

/** 扫描 assets/status，用绝对 path 建立主名(小写)→路径（含 Icon *.png 扫出的主名键，作次要回退）。 */
static std::unordered_map<std::string, std::filesystem::path> g_statusWikiLowerToAbsPath;
static bool                                                 g_statusIconDirIndexed = false;

static void merge_one_status_icon_file(const std::filesystem::path& fp) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_regular_file(fp, ec) || ec)
        return;
    std::string fname = fp.filename().u8string();
    const std::string lower = ascii_lower_ascii(fname);
    if (lower.size() < 5u || lower.compare(lower.size() - 4u, 4u, ".png") != 0)
        return;
    const fs::path absP = fs::absolute(fp, ec);
    if (ec || absP.empty())
        return;
    std::string key;
    if (lower.size() >= 9u && lower.compare(0u, 5u, "icon ") == 0) {
        const std::string titlePart = fname.substr(5u, fname.size() - 5u - 4u);
        key = ascii_lower_ascii(titlePart);
    } else {
        key = ascii_lower_ascii(fname.substr(0u, fname.size() - 4u));
    }
    if (g_statusWikiLowerToAbsPath.find(key) == g_statusWikiLowerToAbsPath.end())
        g_statusWikiLowerToAbsPath[std::move(key)] = absP;
}

static void scan_status_dir_merge(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec))
        return;
    for (const auto& ent : fs::directory_iterator(dir, ec)) {
        if (ec)
            break;
        merge_one_status_icon_file(ent.path());
    }
}

static void ensure_status_effect_icon_dir_index() {
    if (g_statusIconDirIndexed)
        return;
    g_statusIconDirIndexed = true;
    namespace fs = std::filesystem;
    std::vector<fs::path> roots;
    append_status_effect_icon_scan_roots(roots);
    for (const fs::path& d : roots)
        scan_status_dir_merge(d);
}

static bool battle_ui_status_show_stack_number(const std::string& id) {
    return id != "barricade" && id != "blasphemy";
}

/** 不在效果栏占位：内部计数、*_history、自燃伤害拆分数等（与怪物侧跳过 *_history 一致）。 */
static bool battle_ui_status_hide_from_icon_row(const std::string& id) {
    if (id.size() >= 8u && id.compare(id.size() - 8u, 8u, "_history") == 0)
        return true;
    return id == "discarded_this_turn" || id == "attacks_played_this_turn" || id == "combust_damage";
}

static bool battle_ui_status_positive_stack_color(const std::string& id) {
    return id == "strength"
        || id == "dexterity"
        || id == "metallicize"
        || id == "flex"
        || id == "ritual"
        || id == "multi_armor"
        || id == "barricade"
        || id == "accuracy"
        || id == "panache"
        || id == "artifact"
        || id == "poison_cloud"
        || id == "double_damage"
        || id == "double_tap"
        || id == "demon_form"
        || id == "heat_sink"
        || id == "hello"
        || id == "unstoppable"
        || id == "establishment"
        || id == "equilibrium"
        || id == "free_attack"
        || id == "wraith_form"
        || id == "buffer"
        || id == "draw_up"
        || id == "energy_up"
        || id == "block_up"
        || id == "vigor"
        || id == "combust"
        || id == "rupture"
        || id == "flight"
        || id == "indestructible"
        || id == "curiosity"
        || id == "anger"
        || id == "curl_up";
}

namespace {

bool deck_view_card_has_upgrade_definition(const CardId& id) {
    if (id.empty() || id.back() == '+') return false;
    return get_card_by_id(id + "+") != nullptr;
}

std::string deck_view_detail_resolve_display_id(const CardInstance& inst, bool showing_upgraded) {
    const CardId& base = inst.id;
    if (base.empty()) return base;
    if (base.back() == '+') return base;
    if (showing_upgraded && get_card_by_id(base + "+")) return base + "+";
    return base;
}

} // namespace

    /** 快照中玩家某状态总层数（用于 UI 与引擎规则对齐，如腐化） */
    inline int snapshot_player_status_stacks(const BattleStateSnapshot& s, const std::string& statusId) {
        int t = 0;
        for (const auto& st : s.playerStatuses) {
            if (st.id == statusId) t += st.stacks;
        }
        return t;
    }

    /** 手牌/战斗 UI：基础费与实例本场减费、免费攻击、腐化/结茧/内脏切除等与引擎一致 */
    inline int effective_hand_card_energy_cost(const CardData* cd, const CardInstance& inst, const BattleStateSnapshot* snap_opt = nullptr) {
        if (!cd) return 0;
        int c = cd->cost;
        if (c < 0) return c;
        if (snap_opt) {
            if (cd->cardType == CardType::Skill && snapshot_player_status_stacks(*snap_opt, "corruption") > 0) return 0;
            if (cd->cardType == CardType::Skill && inst.combat_cost_zero) return 0;
            if (cd->cardType == CardType::Attack && snapshot_player_status_stacks(*snap_opt, "free_attack") > 0) return 0;
            const std::string& hid = inst.id;
            if (hid == "eviscerate" || hid == "eviscerate+") {
                const int disc = snapshot_player_status_stacks(*snap_opt, "discarded_this_turn");
                return std::max(0, cd->cost - disc - inst.combatCostDiscount);
            }
        }
        int e = c - inst.combatCostDiscount;
        return e < 0 ? 0 : e;
    }

    /** 当前能量与数据层标记下，该手牌下标是否可「打出」（普通出牌；选牌弃牌/消耗不限） */
    inline bool hand_index_playable_now(const BattleStateSnapshot& s, int handIdx) {
        if (handIdx < 0 || static_cast<size_t>(handIdx) >= s.hand.size()) return false;
        const CardData* cd = get_card_by_id(s.hand[static_cast<size_t>(handIdx)].id);
        if (!cd) return true;
        if (cd->unplayable) return false;
        if (cd->cost == -2) return false;
        if (cd->cost == -1) return true; // X 费：0 能量也可打出（效果层按支付 0 处理）
        // 凡庸：手牌中有凡庸时本回合最多打出 3 张牌
        {
            bool has_normality = false;
            for (const auto& hc : s.hand) {
                if (hc.id == "normality" || hc.id == "normality+") {
                    has_normality = true;
                    break;
                }
            }
            if (has_normality && s.cardsPlayedThisTurn >= 3)
                return false;
        }
        // 华丽收场：仅当抽牌堆为空（与 BattleEngine::play_card 一致）
        {
            const std::string& hid = s.hand[static_cast<size_t>(handIdx)].id;
            if (hid == "grand_finale" || hid == "grand_finale+") {
                if (s.drawPileSize > 0) return false;
            }
        }
        const CardInstance& inst = s.hand[static_cast<size_t>(handIdx)];
        return s.energy >= effective_hand_card_energy_cost(cd, inst, &s);
    }

    namespace { // 文件内匿名命名空间；像素常量按 1920×1080 设计（与 BattleUI::kDesignWidth/Height 一致）
        constexpr float TOP_BAR_BG_H = 72.f;       // 顶栏高度 72 像素
        constexpr float TOP_ROW_Y = 20.f;            // 顶栏内容起始 Y 坐标
        constexpr float TOP_ROW_H = 32.f;            // 顶栏内容行高
        constexpr float RELICS_ROW_Y = 72.f;         // 遗物行起始 Y（紧接顶栏下方）
        constexpr float RELICS_ROW_H = 72.f;         // 遗物行高度
        constexpr float RELICS_ICON_SZ = 55.f;       // 遗物图标 55×55 像素
        constexpr float RELICS_ICON_Y_OFFSET = 16.f;  // 遗物图标下移偏移量
        constexpr float BOTTOM_BAR_Y_RATIO = 0.78f;   // 底栏起始比例（与手牌区背景下沿一致）
        // 手牌区条带高度 = height - handBgY（与战场下沿对齐），不再用固定 220，避免压住血条
        constexpr float BOTTOM_MARGIN = 24.f;        // 底边距，保证抽牌/弃牌/消耗图标完整露出
        // 牌组视图滚到最底时，最后一行牌底边与窗口底之间多留的像素（再往上推一截）
        constexpr float DECK_VIEW_SCROLL_BOTTOM_INSET = 48.f;
        constexpr float SIDE_MARGIN = 24.f;          // 左右边距

        constexpr float CARD_W = 206.f;              // 手牌默认宽度
        constexpr float CARD_H = 300.f;              // 手牌默认高度
        constexpr float CARD_PREVIEW_W = 304.f;  // 悬停/选中时预览卡牌宽度（与 CARD_W 同比）
        constexpr float CARD_PREVIEW_H = 410.f;     // 悬停/选中时预览卡牌高度
        constexpr float CARD_PREVIEW_BOTTOM_ABOVE = 5.f;  // 预览时卡牌下边距屏幕底 5 像素
        constexpr float HAND_CARD_OVER_BUTTONS = 18.f;   // 手牌与按钮区域重叠量
        constexpr float HAND_CARD_DISPLAY_STEP = 154.f; // 相邻手牌中心水平间距（扇形展示）
        constexpr float HAND_FAN_SPAN_MAX_DEG = 45.f;   // 手牌扇形总角度上限（度），防止牌多时弧过平
        constexpr float HAND_PIVOT_Y_BELOW = 1900.f;    // 手牌弧心在屏幕下方距离（大半径使 10 张牌不超 60°）
        constexpr float HAND_ARC_TOP_ABOVE_BOTTOM = 220.f;  // 弧顶距屏幕底边 220 像素（卡牌上边不超过）
        constexpr float ENERGY_CENTER_X_BL = 190.f;  // 能量圆圆心 x（以左下为原点）
        constexpr float ENERGY_CENTER_Y_BL = 190.f;  // 能量圆圆心 y（距底边）
        constexpr float ENERGY_OUTLINE_THICKNESS = 10.f;  // 能量圆边框厚度
        constexpr float ENERGY_R = (120.f - 20.f) * 0.5f; // 能量圆填充半径（总直径约 120）
        constexpr float PILE_ICON_W = 52.f;         // 抽牌/弃牌堆图标宽度
        constexpr float PILE_ICON_H = 72.f;          // 抽牌/弃牌堆图标高度
        constexpr float PILE_CENTER_OFFSET = 36.f;   // 牌堆图标中心偏移
        constexpr float PILE_NUM_CIRCLE_R = 17.f;    // 牌数小圆背景半径（图标顶部居中显示张数）
        constexpr float END_TURN_W = 180.f;          // 结束回合按钮宽度
        constexpr float END_TURN_H = 70.f;           // 结束回合按钮高度
        constexpr float END_TURN_CENTER_X_BR = 280.f;  // 以右下为 (0,0) 时按钮中心 x
        constexpr float END_TURN_CENTER_Y_BR = 210.f;  // 以右下为 (0,0) 时按钮中心 y
        constexpr float HP_BAR_W = 230.f;            // 血条最大长度（固定，与生命上限无关）
        constexpr float HP_BAR_H = 10.f;             // 血条高度（变细）；下方为增益减益栏
        constexpr float BATTLE_STATUS_ICON_SZ = 40.f; // 血条下状态效果图标边长（玩家/怪物共用）
        constexpr unsigned BATTLE_STATUS_STACK_FONT_SZ = 22u; // 状态层数角标字号
        constexpr float INTENT_ORB_R = 34.f;              // 怪物意图图标半径（方形贴图边长约 2R）
        constexpr float INTENT_FLOAT_AMP_PX = 6.f;        // 意图图标上下浮动振幅（1920 设计坐标，再乘 uiScaleX_）
        constexpr float INTENT_FLOAT_SPEED  = 2.35f;      // 角速度（rad/s），约 2π/2.7s 一周期
        constexpr float INTENT_FLOAT_PHASE  = 0.58f;      // 相邻怪物相位差（rad），避免完全同相
        /** 怪物立绘呼吸：相对缩放的半振幅（如 0.02 → 约 0.98~1.02），脚底贴地 */
        constexpr float MONSTER_BREATH_SCALE_AMP = 0.022f;
        constexpr float MONSTER_BREATH_SPEED     = 1.65f;  // rad/s，略慢于意图球
        constexpr float MONSTER_BREATH_PHASE     = 0.47f;  // 相邻怪相位差
        constexpr float MODEL_PLACEHOLDER_W = 380.f;      // 玩家/怪物模型占位（1:1）
        constexpr float MODEL_PLACEHOLDER_H = 380.f;
        constexpr float MODEL_TOP_OFFSET = 190.f;      // 约为高度一半，使模型中心对齐 modelCenterY
        constexpr float MODEL_CENTER_Y_RATIO = 0.50f; // 略上移，避免大模型压住下方手牌区与底栏图标
        constexpr float BATTLE_MODEL_BLOCK_Y_OFFSET = 32.f;  // 整体略上移，与 MODEL_CENTER_Y_RATIO 配合

        const sf::Color TOP_BAR_BG_COLOR(42, 38, 48);   // 顶栏背景色（深灰紫）

        constexpr float kCardFacePi = 3.14159265f;

        /** 圆角矩形边界点（顺时针、凸多边形），用于 ConvexShape 填充/描边 */
        inline void build_round_rect_poly(std::vector<sf::Vector2f>& out, float x, float y, float rw, float rh, float r, int seg) {
            out.clear();
            r = std::max(0.f, std::min(r, std::min(rw, rh) * 0.5f - 0.5f));
            if (r < 0.5f) {
                out.push_back(sf::Vector2f(x, y));
                out.push_back(sf::Vector2f(x + rw, y));
                out.push_back(sf::Vector2f(x + rw, y + rh));
                out.push_back(sf::Vector2f(x, y + rh));
                return;
            }
            auto add_arc = [&](float cx, float cy, float a0, float a1) {
                for (int i = 1; i <= seg; ++i) {
                    const float t = static_cast<float>(i) / static_cast<float>(seg);
                    const float a = a0 + (a1 - a0) * t;
                    out.push_back(sf::Vector2f(cx + r * std::cos(a), cy + r * std::sin(a)));
                }
            };
            out.push_back(sf::Vector2f(x + r, y));
            out.push_back(sf::Vector2f(x + rw - r, y));
            add_arc(x + rw - r, y + r, -kCardFacePi * 0.5f, 0.f);
            out.push_back(sf::Vector2f(x + rw, y + rh - r));
            add_arc(x + rw - r, y + rh - r, 0.f, kCardFacePi * 0.5f);
            out.push_back(sf::Vector2f(x + r, y + rh));
            add_arc(x + r, y + rh - r, kCardFacePi * 0.5f, kCardFacePi);
            out.push_back(sf::Vector2f(x, y + r));
            add_arc(x + r, y + r, kCardFacePi, kCardFacePi * 1.5f);
        }

        inline void draw_convex_poly(sf::RenderWindow& window, const std::vector<sf::Vector2f>& pts, const sf::Color& fill,
                                     const sf::Color& stroke, float strokeTh, const sf::RenderStates& rs) {
            if (pts.size() < 3) return;
            sf::ConvexShape poly;
            poly.setPointCount(pts.size());
            for (std::size_t i = 0; i < pts.size(); ++i)
                poly.setPoint(i, pts[i]);
            poly.setFillColor(fill);
            poly.setOutlineColor(stroke);
            poly.setOutlineThickness(strokeTh);
            window.draw(poly, rs);
        }

        // 遗物名称与效果（悬停提示用）：返回 (名称, 描述) 或 (未知遗物, 空)
        inline std::pair<std::wstring, std::wstring> get_relic_display_info(const std::string& id) {
            static const std::unordered_map<std::string, std::pair<std::wstring, std::wstring>> m = {
                {"burning_blood", {L"燃烧之血", L"战斗胜利时回复 6 点生命"}},
                {"ring_of_the_snake", {L"蛇之戒", L"（起始遗物）暂未实现效果"}},
                {"marble_bag", {L"弹珠袋", L"战斗开始时给所有敌人 1 层易伤"}},
                {"small_blood_vial", {L"小血瓶", L"战斗开始时回复 2 点生命"}},
                {"copper_scales", {L"铜制鳞片", L"（无战斗效果）"}},
                {"smooth_stone", {L"意外光滑的石头", L"战斗开始时获得 1 点敏捷"}},
                {"lantern", {L"灯笼", L"每场战斗第一回合获得 1 点能量"}},
                {"happy_flower", {L"开心小花", L"每 3 回合获得 1 点能量"}},
                {"clockwork_boots", {L"发条靴", L"攻击伤害≤5且未被格挡时提升为 5"}},
                {"centennial_puzzle", {L"百年积木", L"第一次损伤生命时抽 3 张牌"}},
                {"orichalcum", {L"奥利哈钢", L"回合结束时若没有格挡则获得 6 点格挡"}},
                {"red_skull", {L"红头骨", L"生命≤50%时攻击伤害+3"}},
                {"snake_skull", {L"异蛇头骨", L"给予敌人中毒时额外+1层"}},
                {"strawberry", {L"草莓", L"拾起时最大生命+7"}},
                {"potion_belt", {L"灵液腰带", L"拾起时灵液槽位+2"}},
                {"vajra", {L"金刚杵", L"战斗开始时获得 1 点力量"}},
                {"nunchaku", {L"双截棍", L"每打出10张攻击牌获得1点能量"}},
                {"ceramic_fish", {L"陶瓷小鱼", L"每次往牌组加牌时获得9金币"}},
                {"hand_drum", {L"手摇鼓", L"回合开始时获得1层真言"}},
                {"pen_nib", {L"钢笔尖", L"每打出的第10张攻击牌造成双倍伤害"}},
                {"toy_ornithopter", {L"玩具扑翼飞机", L"每使用一瓶灵液回复5点生命"}},
                {"preparation_pack", {L"准备背包", L"战斗开始时额外抽2张牌"}},
                {"anchor", {L"锚", L"战斗开始时获得10点格挡"}},
                {"art_of_war", {L"孙子兵法", L"回合中未打出攻击牌时，下一回合获得1点能量"}},
                {"relic_strength_plus", {L"力量遗物", L"攻击伤害+2"}},
            };
            auto it = m.find(id);
            if (it != m.end()) return it->second;
            return {L"未知遗物", L""};
        }
        // 灵液名称与效果（悬停提示用）：返回 (名称, 描述) 或 (未知灵液, 空)
        inline std::pair<std::wstring, std::wstring> get_potion_display_info(const std::string& id) {
            static const std::unordered_map<std::string, std::pair<std::wstring, std::wstring>> m = {
                {"strength_potion", {L"力量灵液", L"获得 2 层力量"}},
                {"block_potion", {L"格挡灵液", L"获得 12 点格挡"}},
                {"energy_potion", {L"能量灵液", L"获得 2 点能量"}},
                {"poison_potion", {L"毒灵液", L"对目标施加 6 层中毒"}},
                {"weak_potion", {L"虚弱灵液", L"对目标施加 3 层虚弱"}},
                {"fear_potion", {L"恐惧灵液", L"对目标施加 3 层易伤"}},
                {"explosion_potion", {L"爆炸灵液", L"对所有敌人造成 10 点伤害"}},
                {"swift_potion", {L"迅捷灵液", L"抽 3 张牌"}},
                {"blood_potion", {L"鲜血灵液", L"回复最大生命值 20%"}},
                {"fire_potion", {L"火焰灵液", L"对目标造成 20 点伤害"}},
            };
            auto it = m.find(id);
            if (it != m.end()) return it->second;
            return {L"未知灵液", L""};
        }

        // 判断该手牌是否需要敌人目标：打击/重击等攻击牌需要，防御/能力等默认自选玩家
        inline bool card_targets_enemy(const BattleStateSnapshot& s, size_t handIndex) {
            if (handIndex >= s.hand.size()) return true;  // 越界默认需要目标（安全）
            const auto& id = s.hand[handIndex].id;
            const CardData* cd = get_card_by_id(id);
            // 仅按 requiresTarget 决定：群伤（顺劈斩/闪电霹雳/匕首雨等）为 false，不需选目标
            if (cd) return cd->requiresTarget;
            // 数据层尚未填充时，对部分已知需目标牌按需目标处理
            if (id == "strike" || id == "bash") return true;
            return false;
        }

        // 按像素宽度自动换行，超出行数用省略号截断，避免绘制出卡牌区域
    inline void draw_wrapped_text(  // 按像素宽度自动换行，超出行数用省略号截断
            sf::RenderTarget& target,
            const sf::Font& font,
            const sf::String& text,
            unsigned int charSize,
            sf::Vector2f pos,
            float maxWidth,
            float maxHeight,
            sf::Color color,
            const sf::RenderStates& states = sf::RenderStates::Default
        ) {
        if (maxWidth <= 1.f || maxHeight <= 1.f || text.isEmpty()) return;

        sf::Text measure(font, sf::String(L""), charSize);  // 用于测量单行字符宽度
            measure.setFillColor(color);

        const float lineH = font.getLineSpacing(charSize);
        const int maxLines = static_cast<int>(std::floor(maxHeight / lineH));  // 最大行数（由高度决定）
            if (maxLines <= 0) return;

            std::vector<sf::String> lines;
            lines.reserve(static_cast<size_t>(maxLines));

            sf::String current;
            auto flush_line = [&]() {                  // 将当前行写入 lines 并清空 current
                lines.push_back(current);
                current.clear();
            };

            for (std::size_t i = 0; i < text.getSize(); ++i) {  // 逐字符处理
                const char32_t ch = text[i]; // SFML3: operator[] 返回 char32_t（UTF-32）
                if (ch == U'\r') continue;   // 忽略回车
                if (ch == U'\n') {
                    flush_line();
                    if (static_cast<int>(lines.size()) >= maxLines) break;
                    continue;
                }

                sf::String candidate = current;
                candidate += ch;
                measure.setString(candidate);
                const float w = measure.getLocalBounds().size.x;

                if (!current.isEmpty() && w > maxWidth) {  // 超宽则换行，当前字符加入新行
                    flush_line();
                    if (static_cast<int>(lines.size()) >= maxLines) break;
                    current += ch;
                } else {
                    current = std::move(candidate);
                }
            }
            if (!current.isEmpty() && static_cast<int>(lines.size()) < maxLines) {
                lines.push_back(current);
            }
            if (lines.empty()) return;

            // 若文本被截断（行数已满且还有剩余字符），给最后一行加省略号，并保证不超宽
            std::size_t joinedLen = 0;
            for (const auto& l : lines) joinedLen += l.getSize();
            if (static_cast<int>(lines.size()) == maxLines && joinedLen + 1 < text.getSize()) {
                sf::String& last = lines.back();
                const sf::String ell = sf::String(L"…");
                while (true) {
                    measure.setString(last + ell);
                    if (measure.getLocalBounds().size.x <= maxWidth) break;
                    if (last.isEmpty()) break;
                    last.erase(last.getSize() - 1, 1);
                }
                last += ell;
            }

            for (std::size_t li = 0; li < lines.size(); ++li) {
                sf::Text t(font, lines[li], charSize);
                t.setFillColor(color);
                t.setPosition(sf::Vector2f(pos.x, pos.y + static_cast<float>(li) * lineH));
                target.draw(t, states);
            }
        }

        struct CardDescGlossaryEntry {
            sf::String key;
            std::wstring title;
            std::wstring body;
        };

        inline const std::vector<CardDescGlossaryEntry>& card_desc_glossary_sorted() {
            static const std::vector<CardDescGlossaryEntry> kTable = [] {
                std::vector<CardDescGlossaryEntry> v;
                auto add = [&](const char* utf8, const wchar_t* title, const wchar_t* body) {
                    const char* end = utf8 + std::strlen(utf8);
                    v.push_back({sf::String::fromUtf8(utf8, end), title, body});
                };
                add(u8"护体罡气", L"护体罡气", L"持续到下回合开始前，抵挡将要受到的伤害。");
                add(u8"弃牌堆", L"弃牌堆", L"已打出或弃掉的牌进入此处；洗牌后可能回到抽牌流程。");
                add(u8"参悟堆", L"参悟堆", L"与抽牌相关的牌堆；部分效果会将弃牌堆洗入此处再抽取。");
                add(u8"人工制品", L"人工制品", L"抵消下一次受到的负面效果（层数各算一次）。");
                add(u8"无实体", L"无实体", L"免疫若干次受到的伤害（按层数消耗）。");
                add(u8"破绽", L"破绽", L"有破绽者受到的攻击伤害提高50%。");
                add(u8"涣散", L"涣散", L"涣散者造成的攻击伤害降低25%。");
                add(u8"蛊毒", L"蛊毒", L"回合结束时失去相应层数气血，层数每回合下降1。");
                add(u8"内伤", L"内伤", L"状态/诅咒类效果中使用的伤害或惩罚标记。");
                add(u8"升级", L"升级", L"升级后卡牌会变得更强大，每张卡牌只能升级一次。");
                add(u8"焚毁", L"焚毁", L"将牌移入消耗堆，本场战斗通常不再回到抽牌堆。");
                add(u8"虚无", L"虚无", L"在手牌中于回合结束时被消耗；否则会进入消耗堆。");
                add(u8"劲力", L"劲力", L"劲力会让武学获得额外的伤害。");
                add(u8"本命", L"本命", L"在每场战斗开始时，这张牌会出现在你的手牌中。");
                add(u8"留存", L"留存", L"留存的牌不会在回合结束时被放入弃牌堆。");
                add(u8"参悟", L"参悟", L"从抽牌堆抽取若干张牌加入手牌。");
                add(u8"折损", L"折损", L"直接失去生命，通常无视护体罡气。");
                add(u8"气血", L"气血", L"当前生命；最大气血为生命上限。");
                add(u8"武学", L"武学", L"敌人的攻击类意图；与「受到武学」等描述对应。");
                add(u8"劲力", L"劲力", L"提高攻击牌造成的伤害。");
                add(u8"灵动", L"灵动", L"增加你从行动中获得的护体罡气。");
                add(u8"真气", L"真气", L"你需要真气来行动。");
                add(u8"脆弱", L"脆弱", L"你可获得的护体罡气减少25%。");
                add(u8"斩杀", L"斩杀", L"指本次伤害使目标生命值降至 0 或以下。");
                add(u8"抽牌", L"抽牌", L"从抽牌堆顶将牌加入手牌。");
                add(u8"能力牌", L"能力牌", L"打出时生效的一类牌，与武学牌等区分。");
                add(u8"武学牌", L"武学牌", L"以武学方式结算的攻击类牌。");
                add(u8"保留", L"保留", L"回合结束时仍可留在手牌的特性；与部分减耗效果联动。");
                add(u8"攻击伤害", L"攻击伤害", L"由攻击类武学或效果造成的伤害。");
                std::sort(v.begin(), v.end(), [](const CardDescGlossaryEntry& a, const CardDescGlossaryEntry& b) {
                    return a.key.getSize() > b.key.getSize();
                });
                return v;
            }();
            return kTable;
        }

        inline const CardDescGlossaryEntry* card_desc_match_keyword_at(const sf::String& full, std::size_t pos,
                                                                       const std::vector<CardDescGlossaryEntry>& tab) {
            for (const auto& e : tab) {
                const std::size_t n = e.key.getSize();
                if (pos + n > full.getSize()) continue;
                bool ok = true;
                for (std::size_t k = 0; k < n; ++k) {
                    if (full[pos + k] != e.key[k]) {
                        ok = false;
                        break;
                    }
                }
                if (ok) return &e;
            }
            return nullptr;
        }

        inline bool card_desc_any_keyword_starts_at(const sf::String& full, std::size_t pos,
                                                    const std::vector<CardDescGlossaryEntry>& tab) {
            return card_desc_match_keyword_at(full, pos, tab) != nullptr;
        }

        /** 供牌面描述关键词金色：关键词整块；普通段仅在宽度超过 maxWidth 时按字切开以便换行 */
        struct CardDescLayoutAtom {
            sf::String s;
            bool kw{};
            bool lineBreak{};
            std::wstring title;
            std::wstring body;
        };

        /** 将过宽的普通 atom 拆成多段（关键词与强制换行不变），避免整段宽于描述框却独占一行导致画出牌面 */
        inline void card_desc_expand_oversized_plain_atoms(const sf::Font& font, unsigned int charSize,
                                                         float maxWidth, std::vector<CardDescLayoutAtom>& atoms) {
            if (maxWidth <= 1.f) return;
            sf::Text measure(font, sf::String(L""), charSize);
            std::vector<CardDescLayoutAtom> out;
            out.reserve(atoms.size() + 8u);

            for (const auto& a : atoms) {
                if (a.lineBreak || a.kw) {
                    out.push_back(a);
                    continue;
                }
                measure.setString(a.s);
                if (measure.getLocalBounds().size.x <= maxWidth + 0.5f) {
                    out.push_back(a);
                    continue;
                }
                sf::String current;
                for (std::size_t i = 0; i < a.s.getSize(); ++i) {
                    const char32_t ch = a.s[i];
                    if (ch == U'\r') continue;
                    sf::String candidate = current;
                    candidate += ch;
                    measure.setString(candidate);
                    const float w = measure.getLocalBounds().size.x;
                    if (!current.isEmpty() && w > maxWidth) {
                        out.push_back({current, false, false, {}, {}});
                        current.clear();
                        current += ch;
                    } else {
                        current = std::move(candidate);
                    }
                }
                if (!current.isEmpty())
                    out.push_back({current, false, false, {}, {}});
            }
            atoms = std::move(out);
        }

        inline void card_desc_build_layout_atoms_from_text(const sf::String& text,
                                                           std::vector<CardDescLayoutAtom>& atoms) {
            atoms.clear();
            atoms.reserve(text.getSize());
            const auto& glossary = card_desc_glossary_sorted();
            std::size_t i = 0;
            const std::size_t n = text.getSize();
            while (i < n) {
                const CardDescGlossaryEntry* hit = card_desc_match_keyword_at(text, i, glossary);
                if (hit) {
                    atoms.push_back({hit->key, true, false, hit->title, hit->body});
                    i += hit->key.getSize();
                    continue;
                }
                std::size_t j = i + 1;
                while (j < n && !card_desc_any_keyword_starts_at(text, j, glossary)) ++j;
                sf::String chunk;
                for (std::size_t k = i; k < j; ++k) {
                    const char32_t ch = text[k];
                    if (ch == U'\r') continue;
                    if (ch == U'\n') {
                        if (!chunk.isEmpty()) {
                            atoms.push_back({chunk, false, false, {}, {}});
                            chunk.clear();
                        }
                        atoms.push_back({sf::String(), false, true, {}, {}});
                        continue;
                    }
                    chunk += ch;
                }
                if (!chunk.isEmpty())
                    atoms.push_back({chunk, false, false, {}, {}});
                i = j;
            }
        }

        /** 换行测量（不限制行数），用于牌侧说明框按内容收紧宽高 */
        inline void measure_wrapped_lines_unlimited(const sf::Font& font, const sf::String& text, unsigned int charSize,
                                                    float maxWidth, float& outMaxLineW, int& outLineCount) {
            outMaxLineW = 0.f;
            outLineCount = 0;
            if (text.isEmpty() || maxWidth <= 1.f) return;

            sf::Text measure(font, sf::String(L""), charSize);
            std::vector<sf::String> lines;
            sf::String current;
            auto flush_line = [&]() {
                lines.push_back(current);
                current.clear();
            };
            for (std::size_t i = 0; i < text.getSize(); ++i) {
                const char32_t ch = text[i];
                if (ch == U'\r') continue;
                if (ch == U'\n') {
                    flush_line();
                    continue;
                }
                sf::String candidate = current;
                candidate += ch;
                measure.setString(candidate);
                const float w = measure.getLocalBounds().size.x;
                if (!current.isEmpty() && w > maxWidth) {
                    flush_line();
                    current += ch;
                } else {
                    current = std::move(candidate);
                }
            }
            if (!current.isEmpty()) lines.push_back(current);
            outLineCount = static_cast<int>(lines.size());
            for (const auto& l : lines) {
                measure.setString(l);
                outMaxLineW = std::max(outMaxLineW, measure.getLocalBounds().size.x);
            }
        }

        /** 关键词金色 + 按出现顺序收集去重词条（供牌侧说明） */
        inline void draw_wrapped_card_description_with_keywords(
            sf::RenderTarget& target,
            const sf::Font& font,
            const sf::String& text,
            unsigned int charSize,
            sf::Vector2f pos,
            float maxWidth,
            float maxHeight,
            sf::Color plainColor,
            sf::Color keywordColor,
            const sf::RenderStates& states,
            std::vector<BattleUI::CardGlossaryEntry>* glossaryOut) {
            if (maxWidth <= 1.f || maxHeight <= 1.f || text.isEmpty()) return;

            std::unordered_set<std::wstring> glossarySeen;
            std::vector<CardDescLayoutAtom> atoms;
            card_desc_build_layout_atoms_from_text(text, atoms);
            card_desc_expand_oversized_plain_atoms(font, charSize, maxWidth, atoms);

            sf::Text measure(font, sf::String(L""), charSize);
            measure.setFillColor(plainColor);
            // 金色关键词与两侧普通字块留空；前侧略大于后侧，避免贴住上一段文字
            const float kwPadAfter =
                std::max(2.5f, static_cast<float>(charSize) * 0.09f);
            const float kwPadBefore =
                std::max(3.4f, static_cast<float>(charSize) * 0.12f);
            std::vector<float> atomW;
            atomW.reserve(atoms.size());
            for (const auto& a : atoms) {
                if (a.lineBreak) {
                    atomW.push_back(0.f);
                    continue;
                }
                measure.setString(a.s);
                float w = measure.getLocalBounds().size.x;
                if (a.kw) w += kwPadBefore + kwPadAfter;
                atomW.push_back(w);
            }

            const float lineH = font.getLineSpacing(charSize);
            const int maxLines = static_cast<int>(std::floor(maxHeight / lineH));
            if (maxLines <= 0) return;

            std::vector<std::vector<std::size_t>> lines;
            lines.reserve(static_cast<std::size_t>(maxLines));
            std::vector<std::size_t> cur;
            float curW = 0.f;

            auto push_line = [&]() {
                lines.push_back(std::move(cur));
                cur.clear();
                curW = 0.f;
            };

            std::size_t atomIdx = 0;
            while (atomIdx < atoms.size()) {
                if (atoms[atomIdx].lineBreak) {
                    if (static_cast<int>(lines.size()) >= maxLines) break;
                    push_line();
                    ++atomIdx;
                    continue;
                }
                const float w = atomW[atomIdx];
                if (curW + w > maxWidth && curW > 0.5f) {
                    if (static_cast<int>(lines.size()) >= maxLines) break;
                    push_line();
                }
                if (static_cast<int>(lines.size()) >= maxLines) break;
                if (curW + w > maxWidth && curW <= 0.5f) {
                    cur.push_back(atomIdx);
                    curW += w;
                    ++atomIdx;
                    if (static_cast<int>(lines.size()) < maxLines) push_line();
                    else break;
                    continue;
                }
                cur.push_back(atomIdx);
                curW += w;
                ++atomIdx;
            }
            if (!cur.empty() && static_cast<int>(lines.size()) < maxLines) push_line();

            const bool truncated = atomIdx < atoms.size();
            if (truncated && !lines.empty()) {
                atoms.push_back({sf::String(L"…"), false, false, {}, {}});
                measure.setString(atoms.back().s);
                const float ew = measure.getLocalBounds().size.x;
                atomW.push_back(ew);
                const std::size_t ellIdx = atoms.size() - 1;
                std::vector<std::size_t>& last = lines.back();
                float lineW = 0.f;
                for (std::size_t id : last) lineW += atomW[id];
                while (!last.empty() && lineW - atomW[last.back()] + ew > maxWidth) {
                    lineW -= atomW[last.back()];
                    last.pop_back();
                }
                last.push_back(ellIdx);
            }

            float y = pos.y;
            for (const auto& line : lines) {
                float x = pos.x;
                for (std::size_t li : line) {
                    const CardDescLayoutAtom& a = atoms[li];
                    sf::Text t(font, a.s, charSize);
                    t.setFillColor(a.kw ? keywordColor : plainColor);
                    const float drawX = x + (a.kw ? kwPadBefore : 0.f);
                    t.setPosition(sf::Vector2f(drawX, y));
                    target.draw(t, states);
                    if (glossaryOut && a.kw && glossarySeen.insert(a.title).second)
                        glossaryOut->push_back({a.title, a.body});
                    x += atomW[li];
                }
                y += lineH;
            }
        }

        /** 牌侧名词解释（与牌面分离）；与 drawDetailedCardAt 相同参照高 410，随卡牌屏幕高度等比缩放 */
        inline void draw_card_glossary_beside_preview_impl(
            sf::RenderWindow& window,
            const sf::FloatRect& cardBb,
            const std::vector<BattleUI::CardGlossaryEntry>& entries,
            const sf::Font& cn,
            unsigned winW,
            unsigned winH) {
            if (entries.empty()) return;
            constexpr float kCardLayoutRefH = 410.f;
            const float s =
                std::clamp(cardBb.size.y / kCardLayoutRefH, 0.28f, 3.2f);
            const unsigned titlePt =
                static_cast<unsigned>(std::max(10.f, std::round(22.f * s)));
            const unsigned bodyPt =
                static_cast<unsigned>(std::max(10.f, std::round(21.f * s)));
            const float kPadX = 10.f * s;
            const float kPadY = 8.f * s;
            const float kTitleBodyGap = 5.f * s;
            const float kPanelGap = 7.f * s;
            const float kFromCard = 10.f * s;
            const float margin = std::max(4.f, 6.f * s);
            const float cornerR = std::max(3.f, 7.f * s);
            const float outlineTh = std::max(0.75f, 1.1f * s);
            // 与 410 参照下栏宽成比例；再用窗口宽度设上限，避免窄窗下侧栏比屏还宽
            float innerW = 258.f * s;
            const float capFromWin = static_cast<float>(winW) * 0.34f - 2.f * kPadX;
            if (capFromWin > 52.f) innerW = std::min(innerW, capFromWin);
            innerW = std::max(innerW, 52.f);
            const float pw = innerW + kPadX * 2.f;

            struct PanelSz {
                float h;
                float titleBlockH;
                float bodyBlockH;
            };
            std::vector<PanelSz> sizes;
            sizes.reserve(entries.size());
            const float bodyLineH = cn.getLineSpacing(bodyPt);
            const float titleLineH = cn.getLineSpacing(titlePt);

            for (const auto& e : entries) {
                float titleLineMaxW = 0.f;
                int titleLines = 0;
                measure_wrapped_lines_unlimited(cn, sf::String(e.title), titlePt, innerW, titleLineMaxW, titleLines);
                float bodyLineMaxW = 0.f;
                int bodyLines = 0;
                measure_wrapped_lines_unlimited(cn, sf::String(e.body), bodyPt, innerW, bodyLineMaxW, bodyLines);
                const float titleBlockH = static_cast<float>(std::max(0, titleLines)) * titleLineH;
                const float bodyBlockH = static_cast<float>(std::max(0, bodyLines)) * bodyLineH;
                const float ph = kPadY + titleBlockH + kTitleBodyGap + bodyBlockH + kPadY;
                sizes.push_back({ph, titleBlockH, bodyBlockH});
            }

            float stackH = 0.f;
            for (std::size_t i = 0; i < sizes.size(); ++i) {
                stackH += sizes[i].h;
                if (i + 1 < sizes.size()) stackH += kPanelGap;
            }

            float startY = cardBb.position.y;
            if (startY + stackH > static_cast<float>(winH) - margin)
                startY = std::max(margin, static_cast<float>(winH) - margin - stackH);

            float panelX = cardBb.position.x + cardBb.size.x + kFromCard;
            if (panelX + pw > static_cast<float>(winW) - margin)
                panelX = cardBb.position.x - kFromCard - pw;
            if (panelX < margin) panelX = margin;

            std::vector<sf::Vector2f> rr;
            float y = startY;
            for (std::size_t ei = 0; ei < entries.size(); ++ei) {
                const PanelSz& sz = sizes[ei];
                const float bx = panelX;
                const float by = y;
                build_round_rect_poly(rr, bx, by, pw, sz.h, cornerR, 3);
                draw_convex_poly(window, rr, sf::Color(28, 26, 32, 238), sf::Color(168, 163, 152), outlineTh,
                                 sf::RenderStates::Default);

                const float titleMaxH = sz.titleBlockH + titleLineH * 0.5f;
                draw_wrapped_text(window, cn, sf::String(entries[ei].title), titlePt,
                                  sf::Vector2f(bx + kPadX, by + kPadY), innerW, titleMaxH,
                                  sf::Color(235, 205, 115), sf::RenderStates::Default);

                const float bodyTop = by + kPadY + sz.titleBlockH + kTitleBodyGap;
                draw_wrapped_text(window, cn, sf::String(entries[ei].body), bodyPt,
                                  sf::Vector2f(bx + kPadX, bodyTop), innerW, sz.bodyBlockH + bodyLineH * 0.5f,
                                  sf::Color(235, 235, 240), sf::RenderStates::Default);
                y += sz.h + kPanelGap;
            }
        }
    }

    BattleUI::BattleUI(unsigned width, unsigned height) : width_(width), height_(height) {
        uiScaleX_ = static_cast<float>(width_) / kDesignWidth;
        uiScaleY_ = static_cast<float>(height_) / kDesignHeight;
        // 结束回合按钮：常量按 1920×1080，随 uiScale 缩放，避免非设计分辨率下命中框与绘制错位
        const float btnCenterX = static_cast<float>(width_) - END_TURN_CENTER_X_BR * uiScaleX_;
        const float btnCenterY = static_cast<float>(height_) - END_TURN_CENTER_Y_BR * uiScaleY_;
        const float btnX = btnCenterX - (END_TURN_W * uiScaleX_) * 0.5f;
        const float btnY = btnCenterY - (END_TURN_H * uiScaleY_) * 0.5f;
        endTurnButton_ = sf::FloatRect(
            sf::Vector2f(btnX, btnY),
            sf::Vector2f(END_TURN_W * uiScaleX_, END_TURN_H * uiScaleY_));
    }

    void BattleUI::set_window_size(unsigned width, unsigned height) {
        width_  = width;
        height_ = height;
        uiScaleX_ = static_cast<float>(width_) / kDesignWidth;
        uiScaleY_ = static_cast<float>(height_) / kDesignHeight;
        const float btnCenterX = static_cast<float>(width_) - END_TURN_CENTER_X_BR * uiScaleX_;
        const float btnCenterY = static_cast<float>(height_) - END_TURN_CENTER_Y_BR * uiScaleY_;
        const float btnX = btnCenterX - (END_TURN_W * uiScaleX_) * 0.5f;
        const float btnY = btnCenterY - (END_TURN_H * uiScaleY_) * 0.5f;
        endTurnButton_ = sf::FloatRect(
            sf::Vector2f(btnX, btnY),
            sf::Vector2f(END_TURN_W * uiScaleX_, END_TURN_H * uiScaleY_));
    }

    void BattleUI::set_top_bar_map_floor(int current_layer_index, int total_layers) {
        top_bar_map_layer_ = current_layer_index;
        top_bar_map_total_ = total_layers > 0 ? total_layers : 0;
    }

    void BattleUI::update_interactive_hover_(bool bottom_bar_interactive, bool potion_slots_interactive) {
        float dt = ui_hover_anim_clock_.restart().asSeconds();
        if (dt > 0.08f) dt = 0.08f;
        if (dt < 0.f) dt = 0.f;

        const sf::Vector2f mp = mousePos_;

        for (size_t i = 0; i < hover_potion_slot_.size(); ++i) {
            bool over = false;
            if (potion_slots_interactive && i < potionSlotRects_.size())
                over = potionSlotRects_[i].contains(mp);
            hover_potion_slot_[i] = ui_hover_lerp(hover_potion_slot_[i], over ? 1.f : 0.f, dt);
        }

        const float right = static_cast<float>(width_) - 28.f;
        const float btnW = 58.f;
        const float btnH = 48.f;
        const float gap  = 18.f;
        const int topBtnCount = hide_top_right_map_button_ ? 2 : 3;
        float rowLeft = right - (static_cast<float>(topBtnCount) * btnW + static_cast<float>(topBtnCount - 1) * gap);

        bool over_map = false;
        bool over_deck = false;
        if (!hide_top_right_map_button_) {
            over_map = mp.x >= rowLeft && mp.x <= rowLeft + btnW && mp.y >= TOP_ROW_Y && mp.y <= TOP_ROW_Y + btnH;
            rowLeft += btnW + gap;
        }
        over_deck = mp.x >= rowLeft && mp.x <= rowLeft + btnW && mp.y >= TOP_ROW_Y && mp.y <= TOP_ROW_Y + btnH;
        const float settingsBtnLeft = right - btnW;
        const bool over_settings =
            mp.x >= settingsBtnLeft && mp.x <= settingsBtnLeft + btnW && mp.y >= TOP_ROW_Y && mp.y <= TOP_ROW_Y + btnH;

        hover_btn_map_      = ui_hover_lerp(hover_btn_map_, over_map ? 1.f : 0.f, dt);
        hover_btn_deck_     = ui_hover_lerp(hover_btn_deck_, over_deck ? 1.f : 0.f, dt);
        hover_btn_settings_ = ui_hover_lerp(hover_btn_settings_, over_settings ? 1.f : 0.f, dt);

        if (!bottom_bar_interactive) {
            hover_draw_pile_    = ui_hover_lerp(hover_draw_pile_, 0.f, dt);
            hover_discard_pile_ = ui_hover_lerp(hover_discard_pile_, 0.f, dt);
            hover_exhaust_pile_ = ui_hover_lerp(hover_exhaust_pile_, 0.f, dt);
            hover_end_turn_     = ui_hover_lerp(hover_end_turn_, 0.f, dt);
            return;  // 灵液悬停已在上方更新
        }

        const float drawPileX = SIDE_MARGIN + PILE_CENTER_OFFSET - 4.f;
        const float drawPileY = static_cast<float>(height_) - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
        const bool over_draw =
            mp.x >= drawPileX && mp.x <= drawPileX + PILE_ICON_W && mp.y >= drawPileY && mp.y <= drawPileY + PILE_ICON_H;

        const float discardX = static_cast<float>(width_) - SIDE_MARGIN - PILE_ICON_W - PILE_CENTER_OFFSET + 4.f;
        const float discardY = static_cast<float>(height_) - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
        const bool over_disc =
            mp.x >= discardX && mp.x <= discardX + PILE_ICON_W && mp.y >= discardY && mp.y <= discardY + PILE_ICON_H;

        const float exhaustX = discardX + 3.f;
        const float exhaustY = discardY - 56.f;
        const float exhaustW = PILE_ICON_W - 6.f;
        const float exhaustH = 48.f;
        const bool over_ex =
            mp.x >= exhaustX && mp.x <= exhaustX + exhaustW && mp.y >= exhaustY && mp.y <= exhaustY + exhaustH;

        const bool over_et = endTurnButton_.contains(mp);

        hover_draw_pile_    = ui_hover_lerp(hover_draw_pile_, over_draw ? 1.f : 0.f, dt);
        hover_discard_pile_ = ui_hover_lerp(hover_discard_pile_, over_disc ? 1.f : 0.f, dt);
        hover_exhaust_pile_ = ui_hover_lerp(hover_exhaust_pile_, over_ex ? 1.f : 0.f, dt);
        hover_end_turn_     = ui_hover_lerp(hover_end_turn_, over_et ? 1.f : 0.f, dt);
    }

    bool BattleUI::loadFont(const std::string& path) {
        fontLoaded_ = font_.openFromFile(path);     // SFML 3: openFromFile，加载西文字体
        return fontLoaded_;
    }

    bool BattleUI::loadChineseFont(const std::string& path) {
        fontChineseLoaded_ = fontChinese_.openFromFile(path);  // 加载中文字体（卡牌名、描述等）
        return fontChineseLoaded_;
    }

    bool BattleUI::loadMonsterTexture(const std::string& monster_id, const std::string& path) {
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return false;  // 加载失败返回 false
        tex.setSmooth(true);  // 缩放时线性过滤，立绘放大后观感更柔和
        monsterTextures_[monster_id] = std::move(tex);  // 按 monster_id 缓存纹理
        return true;
    }

    bool BattleUI::loadRelicTexture(const std::string& relic_id, const std::string& path) {
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return false;
        relicTextures_[relic_id] = std::move(tex);  // 按 relic_id 缓存
        return true;
    }

    bool BattleUI::loadPotionTexture(const std::string& potion_id, const std::string& path) {
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return false;
        tex.setSmooth(true);
        potionTextures_[potion_id] = std::move(tex);  // 按 potion_id 缓存
        return true;
    }

    bool BattleUI::loadPlayerTexture(const std::string& character_id, const std::string& path) {
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return false;
        tex.setSmooth(true);
        playerTextures_[character_id] = std::move(tex);  // 按角色 id 缓存（如 Ironclad）
        return true;
    }

    bool BattleUI::loadBackground(const std::string& path) {
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return false;
        if (backgroundTextures_.empty())
            backgroundTextures_.push_back(std::move(tex));
        else
            backgroundTextures_[0] = std::move(tex);  // 替换默认背景
        return true;
    }

    bool BattleUI::loadBackgroundForBattle(int index, const std::string& path) {
        sf::Texture tex;
        if (!tex.loadFromFile(path)) return false;
        if (index < 0) return false;
        if (static_cast<size_t>(index) >= backgroundTextures_.size())
            backgroundTextures_.resize(static_cast<size_t>(index) + 1);  // 扩展容器
        backgroundTextures_[static_cast<size_t>(index)] = std::move(tex);
        return true;
    }

    void BattleUI::setBattleBackground(int index) {
        currentBackgroundIndex_ = index;  // 切换当前使用的背景图索引
    }

    bool BattleUI::loadIntentionTexture(const std::string& key, const std::string& path) {
        sf::Texture tex;
        if (!load_sf_texture_utf8(tex, path)) return false;
        tex.setSmooth(true);
        intentionTextures_[key] = std::move(tex);
        return true;
    }

    bool BattleUI::loadStatusEffectTexture(const std::string& status_id, const std::string& path) {
        sf::Texture tex;
        if (!load_sf_texture_utf8(tex, path)) return false;
        tex.setSmooth(true);
        statusEffectTextures_[status_id] = std::move(tex);
        statusEffectMissing_.erase(status_id);
        return true;
    }

    const sf::Texture* BattleUI::textureForStatusEffect_(const std::string& id) {
        const auto cached = statusEffectTextures_.find(id);
        if (cached != statusEffectTextures_.end())
            return &cached->second;
        if (statusEffectMissing_.count(id))
            return nullptr;
        if (id.empty()) {
            statusEffectMissing_.insert(id);
            return nullptr;
        }
        ensure_status_effect_icon_dir_index();

        sf::Texture tex;
        bool loaded = false;
        // 1) 主路径：assets/status/<id>.png（与引擎状态 id 一致，无 Icon 前缀）
        {
            const std::string flatBase = std::string("assets/status/") + id;
            if (const std::string pFlat = resolve_image_path(flatBase); !pFlat.empty())
                loaded = load_sf_texture_utf8(tex, pFlat);
        }
        // 2) 目录扫描索引（例如盘上仅有旧 Icon 图时，键可能为 wiki 主名小写而非 id）
        if (!loaded) {
            const auto it = g_statusWikiLowerToAbsPath.find(id);
            if (it != g_statusWikiLowerToAbsPath.end())
                loaded = tex.loadFromFile(it->second);
        }
        if (!loaded) {
            statusEffectMissing_.insert(id);
            return nullptr;
        }
        tex.setSmooth(true);
        const auto ins = statusEffectTextures_.emplace(id, std::move(tex));
        return &ins.first->second;
    }

    void BattleUI::setMousePosition(sf::Vector2f pos) {
        mousePos_ = pos;                            // 每帧由主循环更新，供悬停检测、瞄准箭头等
    }

    void BattleUI::show_center_tip(std::wstring text, float seconds) {
        show_center_tip_(std::move(text), seconds, 40u, false);
    }

    void BattleUI::show_center_tip_(std::wstring text, float seconds, unsigned font_size, bool pop_anim) {
        centerTipText_ = std::move(text);           // 设置中央提示文本（如"能量不足"）
        centerTipSeconds_ = seconds;                // 显示时长（秒）
        centerTipFontSize_ = (font_size > 0u) ? font_size : 40u;
        centerTipPopAnim_ = pop_anim;
        centerTipClock_.restart();                  // 重置计时器，开始计时
    }

    bool BattleUI::center_tip_active_() const {
        if (centerTipText_.empty() || centerTipSeconds_ <= 0.f) return false;
        const float t = centerTipClock_.getElapsedTime().asSeconds();
        return t <= centerTipSeconds_;
    }

    void BattleUI::enqueue_center_tip_(std::wstring text, float seconds) {
        if (text.empty() || seconds <= 0.f) return;
        CenterTipItem it;
        it.text = std::move(text);
        it.seconds = seconds;
        // 默认：队列里的提示一般是“回合/战斗开始”，用更醒目的样式
        it.font_size = 64u;
        it.pop_anim  = true;
        centerTipQueue_.push_back(std::move(it));
    }

    void BattleUI::tick_center_tip_queue_() {
        if (center_tip_active_()) return;
        if (centerTipQueue_.empty()) return;
        CenterTipItem it = std::move(centerTipQueue_.front());
        centerTipQueue_.erase(centerTipQueue_.begin());
        show_center_tip_(std::move(it.text), it.seconds, it.font_size, it.pop_anim);
    }

    bool BattleUI::blocks_battle_engine_step(const BattleStateSnapshot& s) {
        // 先推进队列（上一条提示播完后开始下一条）
        tick_center_tip_queue_();

        // 仅在“回合开始”阶段阻塞推进，确保提示先播放，随后再让主循环推进引擎逻辑（抽牌/怪物行动等）。
        const std::wstring& phase = s.phaseDebugLabel;
        const bool phase_changed = (phase != lastPhaseForTurnTips_);

        if (phase == L"PlayerTurnStart") {
            if (phase_changed) {
                if (s.turnNumber == 1 && !battleStartTipShown_) {
                    enqueue_center_tip_(L"战斗开始", 0.95f);
                    battleStartTipShown_ = true;
                }
                {
                    std::wstring msg = L"玩家回合 ";
                    msg += std::to_wstring(std::max(1, s.turnNumber));
                    enqueue_center_tip_(std::move(msg), 0.95f);
                }
            }
            lastPhaseForTurnTips_ = phase;
            tick_center_tip_queue_();
            return center_tip_active_() || !centerTipQueue_.empty();
        }
        if (phase == L"EnemyTurnStart") {
            if (phase_changed) {
                enqueue_center_tip_(L"敌方回合", 0.95f);
            }
            lastPhaseForTurnTips_ = phase;
            tick_center_tip_queue_();
            return center_tip_active_() || !centerTipQueue_.empty();
        }

        // 其它阶段不阻塞；同时刷新 lastPhase，避免从其它阶段切回时漏触发。
        lastPhaseForTurnTips_ = phase;
        return false;
    }

    bool BattleUI::can_pay_selected_card_cost() const {
        if (!lastSnapshot_) return true;
        if (selectedHandIndex_ < 0 || static_cast<size_t>(selectedHandIndex_) >= lastSnapshot_->hand.size())
            return true;
        return hand_index_playable_now(*lastSnapshot_, selectedHandIndex_);
    }

    void BattleUI::draw_center_tip(sf::RenderWindow& window) {
        if (centerTipText_.empty() || centerTipSeconds_ <= 0.f) return;  // 无内容或已过期
        float t = centerTipClock_.getElapsedTime().asSeconds();
        if (t > centerTipSeconds_) return;         // 超时不再绘制

        // 入场动画：轻微“弹出缩放 + 上移”（回合提示），普通提示则保持稳定
        float popScale   = 1.f;
        float popYOffset = 0.f;
        float inAlpha01  = 1.f;
        if (centerTipPopAnim_) {
            const float inDur = 0.18f;
            float u = t / inDur;
            if (u < 0.f) u = 0.f;
            if (u > 1.f) u = 1.f;
            const float e = 1.f - (1.f - u) * (1.f - u); // easeOutQuad
            // 0.86 -> 1.06 -> 1.00（轻微回弹）
            const float overshoot = (u < 0.65f)
                ? (0.86f + (1.06f - 0.86f) * (e / 0.65f))
                : (1.06f + (1.00f - 1.06f) * ((e - 0.65f) / 0.35f));
            popScale = overshoot;
            popYOffset = (1.f - e) * 18.f;
            inAlpha01 = 0.35f + 0.65f * e;
        }

        // 淡出：最后 0.25 秒逐渐透明
        float alpha01 = 1.f;
        const float fade = 0.25f;
        if (centerTipSeconds_ > 0.f && t > centerTipSeconds_ - fade) {
            alpha01 = (centerTipSeconds_ - t) / fade;
            if (alpha01 < 0.f) alpha01 = 0.f;
        }
        alpha01 *= inAlpha01;
        auto a = static_cast<std::uint8_t>(200.f * alpha01);  // 描边透明度

        sf::Text tip(fontForChinese(), sf::String(centerTipText_), centerTipFontSize_);
        tip.setFillColor(sf::Color(255, 240, 220, static_cast<std::uint8_t>(255.f * alpha01)));
        tip.setOutlineThickness(3.f);
        tip.setOutlineColor(sf::Color(20, 10, 10, a));
        const sf::FloatRect tb = tip.getLocalBounds();
        tip.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        const sf::Vector2f center(width_ * 0.5f, height_ * 0.5f - popYOffset);
        tip.setPosition(center);
        tip.setScale(sf::Vector2f(popScale, popScale));

        // 半透明背景条，居中
        const float padX = 26.f;
        const float padY = 14.f;
        sf::RectangleShape bg(sf::Vector2f(tb.size.x + padX * 2.f, tb.size.y + padY * 2.f));
        bg.setOrigin(sf::Vector2f(bg.getSize().x * 0.5f, bg.getSize().y * 0.5f));
        bg.setPosition(center);
        bg.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(120.f * alpha01)));
        bg.setOutlineThickness(2.f);
        bg.setOutlineColor(sf::Color(255, 220, 180, static_cast<std::uint8_t>(90.f * alpha01)));
        bg.setScale(sf::Vector2f(popScale, popScale));
        window.draw(bg);
        window.draw(tip);
    }

    bool BattleUI::handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos) {
        // 通用选牌弹窗：优先级高于普通战斗交互
        if (card_select_active_) {
            if (ev.is<sf::Event::MouseButtonPressed>()) {
                auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
                if (btn && btn->button == sf::Mouse::Button::Left) {
                    std::cerr << "[BattleUI][card_select] 左键 mouse=(" << mousePos.x << "," << mousePos.y << ") "
                              << "use_hand=" << (card_select_use_hand_area_ ? 1 : 0) << "\n";
                    // 先处理「确定」：不可放在手牌命中失败分支之后，否则 handIndex<0 时永远点不到确定
                    if (card_select_confirm_rect_.contains(mousePos)) {
                        std::cerr << "[BattleUI][card_select] 区域: 确定按钮 rect=("
                                  << card_select_confirm_rect_.position.x << "," << card_select_confirm_rect_.position.y << ") size("
                                  << card_select_confirm_rect_.size.x << "x" << card_select_confirm_rect_.size.y << ") 已选="
                                  << card_select_selected_indices_.size() << "/" << card_select_required_pick_count_ << "\n";
                        if (static_cast<int>(card_select_selected_indices_.size()) >= card_select_required_pick_count_) {
                            pending_card_select_indices_ = card_select_selected_indices_;
                            pending_card_select_cancelled_ = false;
                            set_card_select_active(false);
                            std::cerr << "[BattleUI][card_select] -> 确认提交\n";
                        } else {
                            std::cerr << "[BattleUI][card_select] -> 选够张数前确定无效\n";
                        }
                        return false;
                    }
                    if (card_select_use_hand_area_) {
                        if (!lastSnapshot_) {
                            std::cerr << "[BattleUI][card_select] 错误: lastSnapshot_ 为空（本帧 draw 未写入快照）\n";
                            return false;
                        }
                        int handIndex = -1;
                        int hitVia = -1; // 0=缓存矩形 1=扇形兜底
                        for (size_t i = 0; i < hand_card_rects_.size() && i < hand_card_rect_indices_.size(); ++i) {
                            if (!hand_card_rects_[i].contains(mousePos)) continue;
                            handIndex = hand_card_rect_indices_[i];
                            hitVia = 0;
                            break;
                        }
                        if (handIndex < 0) {
                            std::vector<int> vis;
                            std::vector<sf::Vector2f> centers;
                            std::vector<float> angTmp;
                            compute_card_select_hand_fan_(*lastSnapshot_, card_select_selected_indices_, vis, centers, angTmp);
                            std::cerr << "[BattleUI][card_select] 手牌扇形兜底: 可见张数=" << vis.size()
                                      << " 缓存矩形数=" << hand_card_rects_.size() << "\n";
                            for (int j = static_cast<int>(vis.size()) - 1; j >= 0; --j) {
                                const float cx = centers[static_cast<size_t>(j)].x;
                                const float cy = centers[static_cast<size_t>(j)].y;
                                const sf::FloatRect r(sf::Vector2f(cx - CARD_W * 0.5f, cy - CARD_H * 0.5f), sf::Vector2f(CARD_W, CARD_H));
                                if (r.contains(mousePos)) {
                                    handIndex = vis[static_cast<size_t>(j)];
                                    hitVia = 1;
                                    break;
                                }
                            }
                        } else {
                            std::cerr << "[BattleUI][card_select] 命中缓存矩形 handIndex=" << handIndex << "\n";
                        }
                        if (handIndex < 0 || static_cast<size_t>(handIndex) >= lastSnapshot_->hand.size()) {
                            std::cerr << "[BattleUI][card_select] 未命中任何手牌区卡片 handIndex=" << handIndex
                                      << " handSize=" << lastSnapshot_->hand.size() << " hitVia=" << hitVia << "\n";
                            return false;
                        }
                        if (hitVia == 1) {
                            std::cerr << "[BattleUI][card_select] 扇形兜底命中 handIndex=" << handIndex << "\n";
                        }
                        const auto& clicked = lastSnapshot_->hand[static_cast<size_t>(handIndex)];
                        const auto erasePullAnimForCandidate = [this](int candIdx) {
                            card_select_pull_anims_.erase(
                                std::remove_if(card_select_pull_anims_.begin(), card_select_pull_anims_.end(),
                                               [candIdx](const CardSelectPullAnim& a) { return a.candidateIndex == candIdx; }),
                                card_select_pull_anims_.end());
                        };
                        auto toggleCandidateIndex = [&](int k) {
                            auto it = std::find(card_select_selected_indices_.begin(), card_select_selected_indices_.end(), k);
                            if (it != card_select_selected_indices_.end()) {
                                card_select_selected_indices_.erase(it);
                                erasePullAnimForCandidate(k);
                                std::cerr << "[BattleUI][card_select] -> 取消候选 k=" << k
                                          << " 当前已选数=" << card_select_selected_indices_.size() << "\n";
                                return;
                            }
                            const std::vector<int> selBefore = card_select_selected_indices_;
                            std::vector<int> vis;
                            std::vector<sf::Vector2f> centers;
                            std::vector<float> angTmp;
                            compute_card_select_hand_fan_(*lastSnapshot_, selBefore, vis, centers, angTmp);
                            sf::Vector2f startC = mousePos;
                            for (size_t j = 0; j < vis.size(); ++j) {
                                if (vis[j] == handIndex) {
                                    startC = centers[j];
                                    break;
                                }
                            }
                            const int cap = card_select_required_pick_count_;
                            // 已达上限：不能再增加张数，点击另一张候选则替换「最后选中的那一项」
                            if (cap > 0 && static_cast<int>(card_select_selected_indices_.size()) >= cap) {
                                const int oldLast = card_select_selected_indices_.back();
                                erasePullAnimForCandidate(oldLast);
                                card_select_selected_indices_.back() = k;
                                erasePullAnimForCandidate(k);
                                CardSelectPullAnim anim;
                                anim.candidateIndex = k;
                                anim.startCenter = startC;
                                anim.targetCenter = startC;
                                anim.durationSec = 0.22f;
                                anim.clock.restart();
                                card_select_pull_anims_.push_back(std::move(anim));
                                std::cerr << "[BattleUI][card_select] -> 已满(" << cap << ") 替换末选 old=" << oldLast
                                          << " -> k=" << k << "\n";
                                return;
                            }
                            erasePullAnimForCandidate(k);
                            CardSelectPullAnim anim;
                            anim.candidateIndex = k;
                            anim.startCenter = startC;
                            anim.targetCenter = startC;
                            anim.durationSec = 0.22f;
                            anim.clock.restart();
                            card_select_pull_anims_.push_back(std::move(anim));
                            card_select_selected_indices_.push_back(k);
                            std::cerr << "[BattleUI][card_select] -> 选中候选 k=" << k
                                      << " 当前已选数=" << card_select_selected_indices_.size() << "\n";
                        };
                        // 优先：主流程传入的「手牌下标」与点击下标一致（不依赖快照/牌库 instanceId 同步）
                        if (!card_select_candidate_hand_indices_.empty() &&
                            card_select_candidate_hand_indices_.size() == card_select_ids_.size()) {
                            for (size_t k = 0; k < card_select_candidate_hand_indices_.size(); ++k) {
                                if (card_select_candidate_hand_indices_[k] != handIndex) continue;
                                std::cerr << "[BattleUI][card_select] 匹配候选(手牌下标) k=" << k
                                          << " cardId=" << card_select_ids_[k] << " handIndex=" << handIndex << "\n";
                                toggleCandidateIndex(static_cast<int>(k));
                                return false;
                            }
                            std::cerr << "[BattleUI][card_select] 点击在手牌上但不在候选内 handIndex=" << handIndex
                                      << "（可能点在要打出的那张上）\n";
                            return false;
                        }
                        if (!card_select_candidate_instance_ids_.empty() &&
                            card_select_candidate_instance_ids_.size() == card_select_ids_.size()) {
                            for (size_t k = 0; k < card_select_candidate_instance_ids_.size(); ++k) {
                                if (card_select_candidate_instance_ids_[k] != clicked.instanceId) continue;
                                std::cerr << "[BattleUI][card_select] 匹配候选(instanceId) k=" << k << "\n";
                                toggleCandidateIndex(static_cast<int>(k));
                                return false;
                            }
                            std::cerr << "[BattleUI][card_select] instanceId 未匹配任何候选\n";
                            return false;
                        }
                        const CardId& clickedId = clicked.id;
                        for (size_t k = 0; k < card_select_ids_.size(); ++k) {
                            if (card_select_ids_[k] != clickedId) continue;
                            std::cerr << "[BattleUI][card_select] 匹配候选(仅 cardId) k=" << k << "\n";
                            toggleCandidateIndex(static_cast<int>(k));
                            return false;
                        }
                        std::cerr << "[BattleUI][card_select] cardId 未匹配任何候选 clickedId=" << clickedId << "\n";
                        return false;
                    }
                    for (size_t i = 0; i < card_select_rects_.size() && i < card_select_ids_.size(); ++i) {
                        if (card_select_rects_[i].contains(mousePos)) {
                            std::cerr << "[BattleUI][card_select] 区域: 弹窗网格槽 i=" << i
                                      << " rect=(" << card_select_rects_[i].position.x << ","
                                      << card_select_rects_[i].position.y << ")\n";
                            auto it = std::find(card_select_selected_indices_.begin(), card_select_selected_indices_.end(), static_cast<int>(i));
                            if (it != card_select_selected_indices_.end()) {
                                card_select_selected_indices_.erase(it);
                            } else {
                                const int cap = card_select_required_pick_count_;
                                if (cap > 0 && static_cast<int>(card_select_selected_indices_.size()) >= cap)
                                    card_select_selected_indices_.back() = static_cast<int>(i);
                                else
                                    card_select_selected_indices_.push_back(static_cast<int>(i));
                            }
                            return false;
                        }
                    }
                    std::cerr << "[BattleUI][card_select] 未命中确定/手牌/网格任一区域\n";
                }
            }
            return false;
        }

        // 若暂停菜单 / 设置界面打开：仅处理暂停相关点击，屏蔽其它交互
        if (pause_menu_active_ || settings_panel_active_) {
            if (ev.is<sf::Event::MouseButtonPressed>()) {
                auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
                if (btn && btn->button == sf::Mouse::Button::Left) {
                    // 再次点击顶栏「设置」：关闭暂停菜单 / 设置二级页（与再点一次退出一致）
                    {
                        const float right = width_ - 28.f;
                        const float btnW = 58.f, btnH = 48.f, gap = 18.f;
                        const float settingsBtnLeft = right - btnW;
                        if (mousePos.x >= settingsBtnLeft && mousePos.x <= settingsBtnLeft + btnW &&
                            mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                            pause_menu_active_    = false;
                            settings_panel_active_ = false;
                            pending_pause_menu_choice_ = 0;
                            return false;
                        }
                    }
                    if (!settings_panel_active_) {
                        if (pauseResumeRect_.contains(mousePos)) {
                            pending_pause_menu_choice_ = 1;   // 返回游戏
                            pause_menu_active_ = false;
                            return false;
                        }
                        if (pauseSaveQuitRect_.contains(mousePos)) {
                            pending_pause_menu_choice_ = 2;   // 保存并退出
                            // 保持菜单打开直到上层处理完关闭窗口
                            return false;
                        }
                        if (pauseSettingsRect_.contains(mousePos)) {
                            pending_pause_menu_choice_ = 3;   // 进入设置页面
                            settings_panel_active_ = true;
                            pause_save_slot_panel_active_ = false;
                            settings_video_preset_preview_ = UserSettings::instance().videoPreset();
                            return false;
                        }
                    } else {
                        const float panelW = 760.f;
                        const float panelH = 520.f;
                        const float panelX = (static_cast<float>(width_) - panelW) * 0.5f;
                        const float panelY = (static_cast<float>(height_) - panelH) * 0.5f;
                        layout_pause_settings_controls_(panelX, panelY, panelW, panelH);
                        if (settingsVolDownRect_.contains(mousePos)) {
                            UserSettings::instance().setMasterVolume(UserSettings::instance().masterVolume() - 5.f);
                            return false;
                        }
                        if (settingsVolUpRect_.contains(mousePos)) {
                            UserSettings::instance().setMasterVolume(UserSettings::instance().masterVolume() + 5.f);
                            return false;
                        }
                        if (settingsResPrevRect_.contains(mousePos)) {
                            settings_video_preset_preview_ =
                                (settings_video_preset_preview_ - 1 + UserSettings::kVideoModeCount) %
                                UserSettings::kVideoModeCount;
                            return false;
                        }
                        if (settingsResNextRect_.contains(mousePos)) {
                            settings_video_preset_preview_ =
                                (settings_video_preset_preview_ + 1) % UserSettings::kVideoModeCount;
                            return false;
                        }
                        if (settingsApplyVideoRect_.contains(mousePos)) {
                            UserSettings::instance().setVideoPreset(settings_video_preset_preview_);
                            UserSettings::instance().markVideoApplyPending();
                            UserSettings::instance().save();
                            return false;
                        }
                        if (settingsBackRect_.contains(mousePos)) {
                            settings_panel_active_ = false; // 返回到一级暂停菜单
                            return false;
                        }
                    }
                }
            }
            return false;
        }

        // 牌组界面：滚轮滚动网格；返回/顶栏牌组关闭；点牌进入大图详情（点牌面或空白退出，「查看升级」切换预览）
        if (deck_view_active_) {
            if (deck_view_detail_active_) {
                if (ev.is<sf::Event::MouseWheelScrolled>())
                    return false;
                if (ev.is<sf::Event::MouseButtonPressed>()) {
                    auto const& mb = ev.getIf<sf::Event::MouseButtonPressed>();
                    if (mb && mb->button == sf::Mouse::Button::Left) {
                        updateDeckViewDetailLayout_();
                        {
                            const float right = width_ - 28.f;
                            const float btnW = 58.f, btnH = 48.f, gap = 18.f;
                            const int topBtnCount = hide_top_right_map_button_ ? 2 : 3;
                            float rowLeft = right - (static_cast<float>(topBtnCount) * btnW + static_cast<float>(topBtnCount - 1) * gap);
                            if (!hide_top_right_map_button_)
                                rowLeft += btnW + gap;
                            if (mousePos.x >= rowLeft && mousePos.x <= rowLeft + btnW && mousePos.y >= TOP_ROW_Y &&
                                mousePos.y <= TOP_ROW_Y + btnH) {
                                deck_view_detail_active_        = false;
                                deck_view_detail_show_upgraded_ = false;
                                return false;
                            }
                        }
                        if (deckViewReturnButton_.contains(mousePos)) {
                            deck_view_detail_active_        = false;
                            deck_view_detail_show_upgraded_ = false;
                            return false;
                        }
                        if (deck_view_detail_upgrade_btn_rect_.contains(mousePos)) {
                            if (deck_view_card_has_upgrade_definition(deck_view_detail_inst_.id))
                                deck_view_detail_show_upgraded_ = !deck_view_detail_show_upgraded_;
                            return false;
                        }
                        if (deck_view_detail_card_rect_.contains(mousePos)) {
                            deck_view_detail_active_        = false;
                            deck_view_detail_show_upgraded_ = false;
                            return false;
                        }
                        deck_view_detail_active_        = false;
                        deck_view_detail_show_upgraded_ = false;
                    }
                    return false;
                }
                return false;
            }

            if (ev.is<sf::Event::MouseWheelScrolled>()) {
                auto const& wheel = ev.getIf<sf::Event::MouseWheelScrolled>();
                if (wheel) {
                    constexpr float CARD_H = 300.f;
                    constexpr float ROW_CENTER_TO_CENTER = 360.f;
                    float contentTop = 0.f;
                    float firstRowCenterY = 0.f;
                    float viewTopUnused = 0.f;
                    deck_view_grid_layout_(contentTop, firstRowCenterY, viewTopUnused);
                    const int numRows = (static_cast<int>(deck_view_cards_.size()) + 4) / 5;
                    const float firstRowTop = firstRowCenterY - CARD_H * 0.5f;
                    const float padTop = firstRowTop - contentTop;
                    const float lastCardBottomY = numRows > 0
                        ? contentTop + padTop + (numRows - 1) * ROW_CENTER_TO_CENTER + CARD_H
                        : contentTop + padTop;
                    const float maxScroll = (numRows > 0)
                        ? std::max(0.f, lastCardBottomY - static_cast<float>(height_) + DECK_VIEW_SCROLL_BOTTOM_INSET)
                        : 0.f;
                    const float step = 80.f;
                    deck_view_scroll_y_ -= wheel->delta * step;
                    if (maxScroll <= 0.f) deck_view_scroll_y_ = 0.f;
                    else {
                        if (deck_view_scroll_y_ < 0.f) deck_view_scroll_y_ = 0.f;
                        if (deck_view_scroll_y_ > maxScroll) deck_view_scroll_y_ = maxScroll;
                    }
                }
                return false;
            }
            if (ev.is<sf::Event::MouseButtonPressed>()) {
                auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
                if (btn && btn->button == sf::Mouse::Button::Left) {
                    {
                        const float right = width_ - 28.f;
                        const float btnW = 58.f, btnH = 48.f, gap = 18.f;
                        const int topBtnCount = hide_top_right_map_button_ ? 2 : 3;
                        float rowLeft = right - (static_cast<float>(topBtnCount) * btnW + static_cast<float>(topBtnCount - 1) * gap);
                        if (!hide_top_right_map_button_)
                            rowLeft += btnW + gap;
                        if (mousePos.x >= rowLeft && mousePos.x <= rowLeft + btnW && mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                            deck_view_active_               = false;
                            deck_view_scroll_y_             = 0.f;
                            deck_view_detail_active_        = false;
                            deck_view_detail_show_upgraded_ = false;
                            deck_view_glossary_entries_.clear();
                            deck_view_glossary_active_ = false;
                            return false;
                        }
                    }
                    if (deckViewReturnButton_.contains(mousePos)) {
                        deck_view_active_               = false;
                        deck_view_scroll_y_             = 0.f;
                        deck_view_detail_active_        = false;
                        deck_view_detail_show_upgraded_ = false;
                        deck_view_glossary_entries_.clear();
                        deck_view_glossary_active_ = false;
                        return false;
                    }
                    constexpr float DECK_CARD_W = 206.f;
                    constexpr float DECK_CARD_H = 300.f;
                    constexpr float COL_CENTER_TO_CENTER = 276.f;
                    constexpr float ROW_CENTER_TO_CENTER = 360.f;
                    constexpr int COLS = 5;
                    float contentTop = 0.f;
                    float firstRowCenterY = 0.f;
                    float viewTop = 0.f;
                    deck_view_grid_layout_(contentTop, firstRowCenterY, viewTop);
                    const float viewBottom = static_cast<float>(height_);
                    const size_t cardCount = deck_view_cards_.size();
                    const float firstRowTop = firstRowCenterY - DECK_CARD_H * 0.5f;
                    const float padTop = firstRowTop - contentTop;
                    const float totalContentW = (COLS - 1) * COL_CENTER_TO_CENTER + DECK_CARD_W;
                    const float contentLeft = (static_cast<float>(width_) - totalContentW) * 0.5f;
                    for (size_t i = 0; i < cardCount; ++i) {
                        const int row = static_cast<int>(i) / COLS;
                        const int col = static_cast<int>(i) % COLS;
                        const float cardX = contentLeft + col * COL_CENTER_TO_CENTER;
                        const float cardY = contentTop + padTop + row * ROW_CENTER_TO_CENTER - deck_view_scroll_y_;
                        if (cardY + DECK_CARD_H < viewTop || cardY > viewBottom) continue;
                        if (mousePos.x >= cardX && mousePos.x <= cardX + DECK_CARD_W && mousePos.y >= cardY &&
                            mousePos.y <= cardY + DECK_CARD_H) {
                            deck_view_detail_inst_           = deck_view_cards_[i];
                            deck_view_detail_show_upgraded_  = false;
                            deck_view_detail_active_         = true;
                            return false;
                        }
                    }
                }
            }
            return false;
        }

        // 全屏地图浏览浮层：仅响应顶栏「地图 / 牌组 / 设置」（不设牌堆快捷入口）
        if (map_overlay_blocks_world_input_) {
            if (ev.is<sf::Event::MouseButtonPressed>()) {
                auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
                if (btn && btn->button == sf::Mouse::Button::Left) {
                    const float right = width_ - 28.f;
                    const float btnW = 58.f, btnH = 48.f, gap = 18.f;
                    const int topBtnCount = hide_top_right_map_button_ ? 2 : 3;
                    float rowLeft = right - (static_cast<float>(topBtnCount) * btnW + static_cast<float>(topBtnCount - 1) * gap);
                    if (!hide_top_right_map_button_) {
                        if (mousePos.x >= rowLeft && mousePos.x <= rowLeft + btnW && mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                            pending_map_browse_toggle_ = 1;
                            return false;
                        }
                        rowLeft += btnW + gap;
                    }
                    if (mousePos.x >= rowLeft && mousePos.x <= rowLeft + btnW && mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                        pending_deck_view_mode_ = 1;
                        return false;
                    }
                    const float settingsBtnLeft = right - btnW;
                    if (mousePos.x >= settingsBtnLeft && mousePos.x <= settingsBtnLeft + btnW &&
                        mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                        if (pause_menu_active_ || settings_panel_active_) {
                            pause_menu_active_    = false;
                            settings_panel_active_ = false;
                            pending_pause_menu_choice_ = 0;
                        } else {
                            pause_menu_active_ = true;
                            settings_panel_active_ = false;
                        }
                        return false;
                    }
                }
            }
            return false;
        }

        // 非牌组界面：点击顶栏「地图」「牌组」「设置」、左下抽牌堆、右下弃牌堆、弃牌堆上方消耗堆 → 打开对应视图
        if (ev.is<sf::Event::MouseButtonPressed>()) {
            auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
            if (btn && btn->button == sf::Mouse::Button::Left) {
                const float right = width_ - 28.f;
                const float btnW = 58.f, btnH = 48.f, gap = 18.f;
                const int topBtnCount = hide_top_right_map_button_ ? 2 : 3;
                float rowLeft = right - (static_cast<float>(topBtnCount) * btnW + static_cast<float>(topBtnCount - 1) * gap);
                if (!hide_top_right_map_button_) {
                    if (mousePos.x >= rowLeft && mousePos.x <= rowLeft + btnW && mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                        pending_map_browse_toggle_ = 1;
                        return false;
                    }
                    rowLeft += btnW + gap;
                }
                if (mousePos.x >= rowLeft && mousePos.x <= rowLeft + btnW && mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                    pending_deck_view_mode_ = 1;  // 牌组（主牌组）
                    return false;
                }
                // 顶栏右上角「设置」：打开暂停菜单；再点一次关闭
                const float settingsBtnLeft = right - btnW;  // 最右侧按钮
                if (mousePos.x >= settingsBtnLeft && mousePos.x <= settingsBtnLeft + btnW &&
                    mousePos.y >= TOP_ROW_Y && mousePos.y <= TOP_ROW_Y + btnH) {
                    if (pause_menu_active_ || settings_panel_active_) {
                        pause_menu_active_       = false;
                        settings_panel_active_   = false;
                        pending_pause_menu_choice_ = 0;
                    } else {
                        pause_menu_active_ = true;
                        settings_panel_active_ = false;
                    }
                    return false;
                }
                const float drawPileX = SIDE_MARGIN + PILE_CENTER_OFFSET - 4.f;
                const float drawPileY = height_ - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
                if (mousePos.x >= drawPileX && mousePos.x <= drawPileX + PILE_ICON_W && mousePos.y >= drawPileY && mousePos.y <= drawPileY + PILE_ICON_H) {
                    pending_deck_view_mode_ = 2;  // 抽牌堆
                    return false;
                }
                const float discardX = width_ - SIDE_MARGIN - PILE_ICON_W - PILE_CENTER_OFFSET + 4.f;
                const float discardY = height_ - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
                if (mousePos.x >= discardX && mousePos.x <= discardX + PILE_ICON_W && mousePos.y >= discardY && mousePos.y <= discardY + PILE_ICON_H) {
                    pending_deck_view_mode_ = 3;  // 弃牌堆
                    return false;
                }
                // 消耗堆：弃牌堆上方的小图标区域（与绘制位置一致）
                const float exhaustX = discardX + 3.f;
                const float exhaustY = discardY - 56.f;
                const float exhaustW = PILE_ICON_W - 6.f;
                const float exhaustH = 48.f;
                if (mousePos.x >= exhaustX && mousePos.x <= exhaustX + exhaustW && mousePos.y >= exhaustY && mousePos.y <= exhaustY + exhaustH) {
                    pending_deck_view_mode_ = 4;  // 消耗堆
                    return false;
                }
            }
        }

        // 奖励界面：处理卡牌选择、跳过、继续（在顶栏之后，以便战斗中仍可打开地图/牌组浏览）
        if (reward_screen_active_) {
            if (ev.is<sf::Event::MouseButtonPressed>()) {
                auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
                if (btn && btn->button == sf::Mouse::Button::Left) {
                    // 槽满领取灵液：弹窗中选择要丢弃的旧灵液
                    if (reward_potion_replace_active_) {
                        if (reward_potion_replace_cancel_rect_.contains(mousePos)) {
                            reward_potion_replace_active_ = false;
                            reward_potion_replace_slot_rects_.clear();
                            // 取消替换：不触发领取
                            pending_reward_potion_id_.clear();
                            pending_reward_potion_replace_slot_ = -2;
                            return false;
                        }
                        for (size_t i = 0; i < reward_potion_replace_slot_rects_.size(); ++i) {
                            if (reward_potion_replace_slot_rects_[i].contains(mousePos)) {
                                // 用 i 槽的旧灵液替换为 pending_reward_potion_id_
                                if (!pending_reward_potion_id_.empty()) {
                                    pending_reward_potion_replace_slot_ = static_cast<int>(i);
                                }
                                reward_potion_replace_active_ = false;
                                reward_potion_replace_slot_rects_.clear();
                                return false;
                            }
                        }
                        return false;
                    }

                    // 遗物/灵液：点击领取（选项制）——不依赖是否已选卡牌
                    for (size_t i = 0; i < reward_relic_rects_.size() && i < reward_relic_ids_.size(); ++i) {
                        if (reward_relic_rects_[i].contains(mousePos)) {
                            pending_reward_relic_id_ = reward_relic_ids_[i];
                            return false;
                        }
                    }
                    for (size_t i = 0; i < reward_potion_rects_.size() && i < reward_potion_ids_.size(); ++i) {
                        if (reward_potion_rects_[i].contains(mousePos)) {
                            // 若槽满：进入替换选择模式（让玩家选丢弃哪瓶）
                            if (lastSnapshot_) {
                                const int slots = std::max(0, lastSnapshot_->potionSlotCount);
                                const int have  = static_cast<int>(lastSnapshot_->potions.size());
                                if (slots > 0 && have >= slots) {
                                        // 槽满：先进入替换选择态，不要立刻触发领取（避免被主循环提前消费）
                                        pending_reward_potion_id_ = reward_potion_ids_[i];
                                        pending_reward_potion_replace_slot_ = -2; // 等待用户选槽位
                                    reward_potion_replace_active_ = true;
                                    reward_potion_replace_slot_rects_.clear();
                                    const float W = static_cast<float>(width_);
                                    const float H = static_cast<float>(height_);
                                    const float boxW = 620.f;
                                    const float boxH = 210.f;
                                    const float boxX = (W - boxW) * 0.5f;
                                    const float boxY = (H - boxH) * 0.5f;
                                    const float icon = 56.f;
                                    const float gap  = 16.f;
                                    float startX = boxX + 56.f;
                                    const float y = boxY + 86.f;
                                    for (int s = 0; s < slots; ++s) {
                                        reward_potion_replace_slot_rects_.push_back(
                                            sf::FloatRect(sf::Vector2f(startX + s * (icon + gap), y),
                                                          sf::Vector2f(icon, icon)));
                                    }
                                    reward_potion_replace_cancel_rect_ = sf::FloatRect(
                                        sf::Vector2f(boxX + boxW - 160.f, boxY + boxH - 60.f),
                                        sf::Vector2f(120.f, 40.f));
                                }
                                    // 有空槽：直接领取（追加）
                                    else {
                                        pending_reward_potion_id_ = reward_potion_ids_[i];
                                        pending_reward_potion_replace_slot_ = -1;
                                    }
                            }
                                else {
                                    // 没有快照时退化为追加领取
                                    pending_reward_potion_id_ = reward_potion_ids_[i];
                                    pending_reward_potion_replace_slot_ = -1;
                                }
                            return false;
                        }
                    }

                    if (reward_card_picked_) {
                        if (reward_continue_rect_.contains(mousePos)) {
                            pending_continue_to_next_battle_ = true;
                            return false;
                        }
                    } else {
                        for (size_t i = 0; i < reward_card_rects_.size() && i < reward_card_ids_.size(); ++i) {
                            if (reward_card_rects_[i].contains(mousePos)) {
                                pending_reward_card_index_ = static_cast<int>(i);
                                reward_card_picked_ = true;
                                return false;
                            }
                        }
                        if (reward_skip_rect_.contains(mousePos)) {
                            pending_reward_card_index_ = -1;
                            reward_card_picked_ = true;
                            return false;
                        }
                    }
                }
            }
            return false;
        }

        if (ev.is<sf::Event::MouseButtonPressed>()) {
            auto const& btn = ev.getIf<sf::Event::MouseButtonPressed>();
            if (!btn) return false;

            // 左键：先处理打牌逻辑，其次灵液逻辑，再判断结束回合按钮
            if (btn->button == sf::Mouse::Button::Left) {
                // 若当前正在瞄准灵液，点击怪物则确认使用
                if (selectedPotionSlotIndex_ >= 0 && isAimingPotion_) {
                    for (size_t i = 0; i < monsterModelRects_.size(); ++i) {
                        if (monsterModelRects_[i].contains(mousePos) && lastSnapshot_ && i < lastSnapshot_->monsters.size()) {
                            pendingPotionSlotIndex_ = selectedPotionSlotIndex_;
                            pendingPotionTargetIndex_ = static_cast<int>(i);
                            selectedPotionSlotIndex_ = -1;
                            isAimingPotion_ = false;
                            return false;
                        }
                    }
                }
                // 若当前正在瞄准牌，根据目标类型决定出牌逻辑
                else if (selectedHandIndex_ >= 0 && isAimingCard_) {
                    if (selectedCardTargetsEnemy_) {
                        for (size_t i = 0; i < monsterModelRects_.size(); ++i) {
                            if (monsterModelRects_[i].contains(mousePos)) {
                                if (!can_pay_selected_card_cost()) {
                                    show_center_tip(L"能量不足", 1.2f);
                                    return false;
                                }
                                pendingPlayHandIndex_ = selectedHandIndex_;
                                pendingPlayTargetMonsterIndex_ = static_cast<int>(i);
                                selectedHandIndex_ = -1;
                                isAimingCard_ = false;
                                return false;
                            }
                        }
                    }
                    else {
                        // 自选（玩家）目标牌：鼠标离开原位矩形且玩家模型黄框时点击，视为对玩家使用
                        if (!selectedCardInsideOriginRect_) {
                            if (!can_pay_selected_card_cost()) {
                                show_center_tip(L"能量不足", 1.2f);
                                return false;
                            }
                            pendingPlayHandIndex_ = selectedHandIndex_;
                            pendingPlayTargetMonsterIndex_ = -1; // -1 表示玩家自身
                            selectedHandIndex_ = -1;
                            isAimingCard_ = false;
                            return false;
                        }
                        // 若仍在原位矩形内且玩家模型未高亮，则视为不使用（本次点击忽略）
                    }
                }

                // 若尚未选中牌，则检查是否点中了某张手牌以开始选中/瞄准（在 drawBottomBar 中处理左键按下）
                // 若未选中牌，检查是否点击灵液槽
                if (selectedHandIndex_ < 0 && lastSnapshot_) {
                    for (size_t i = 0; i < potionSlotRects_.size() && i < lastSnapshot_->potions.size(); ++i) {
                        if (potionSlotRects_[i].contains(mousePos)) {
                            const PotionId& pid = lastSnapshot_->potions[i];
                            if (potion_requires_monster_target(pid)) {
                                selectedPotionSlotIndex_ = static_cast<int>(i);
                                isAimingPotion_ = true;
                            } else {
                                pendingPotionSlotIndex_ = static_cast<int>(i);
                                pendingPotionTargetIndex_ = -1;
                            }
                            return false;
                        }
                    }
                }

                // 点击结束回合按钮
                if (endTurnButton_.contains(mousePos)) {
                    return true;                     // 返回 true 表示主循环应调用 engine.end_turn() 结束回合
                }
            }

            // 右键：取消当前选中的牌/灵液或瞄准状态
            if (btn->button == sf::Mouse::Button::Right) {
                selectedHandIndex_ = -1;
                isAimingCard_ = false;
                selectedPotionSlotIndex_ = -1;
                isAimingPotion_ = false;
                return false;
            }
        }
        return false;
    }

    bool BattleUI::pollPlayCardRequest(int& outHandIndex, int& outTargetMonsterIndex) {
        if (pendingPlayHandIndex_ < 0) return false;
        outHandIndex = pendingPlayHandIndex_;
        outTargetMonsterIndex = pendingPlayTargetMonsterIndex_;  // -1 表示玩家自身（自选目标牌）
        pendingPlayHandIndex_ = -1;
        pendingPlayTargetMonsterIndex_ = -1;
        return true;
    }

    bool BattleUI::pollPotionRequest(int& outSlotIndex, int& outTargetMonsterIndex) {
        if (pendingPotionSlotIndex_ < 0) return false;
        outSlotIndex = pendingPotionSlotIndex_;
        outTargetMonsterIndex = pendingPotionTargetIndex_;
        pendingPotionSlotIndex_ = -1;
        pendingPotionTargetIndex_ = -1;
        return true;
    }

    void BattleUI::set_deck_view_cards(std::vector<CardInstance> cards) {
        deck_view_cards_ = std::move(cards);
        deck_view_scroll_y_             = 0.f;
        deck_view_detail_active_        = false;
        deck_view_detail_show_upgraded_ = false;
        deck_view_glossary_entries_.clear();
        deck_view_glossary_active_ = false;
    }

    void BattleUI::deck_view_grid_layout_(float& outContentTop, float& outFirstRowCenterY, float& outViewTop) const {
        if (deck_view_standalone_grid_layout_) {
            const float n = deck_view_standalone_vertical_nudge_;
            outContentTop        = 72.f + n;
            outFirstRowCenterY   = 260.f + n;
            outViewTop           = 0.f;
        } else {
            outContentTop        = RELICS_ROW_Y + RELICS_ROW_H + 14.f;
            outFirstRowCenterY   = 430.f;
            outViewTop           = TOP_BAR_BG_H;
        }
    }

    void BattleUI::set_deck_view_active(bool active) {
        deck_view_active_ = active;
        if (active) {
            pile_anim_snapshot_ready_ = false;
            prev_discard_ids_for_anim_.clear();
            pile_card_flights_.clear();
            pile_draw_anim_hiding_.clear();
            pending_select_ui_pile_fly_remaining_ = 0;
            deck_view_detail_active_         = false;
            deck_view_detail_show_upgraded_  = false;
            deck_view_detail_inst_           = CardInstance{};
            deck_view_hover_index_ = -1;
            deck_view_hover_blend_ = 0.f;
            deck_view_hover_clock_.restart();
        }
        if (!active) {
            deck_view_scroll_y_ = 0.f;
            deck_view_standalone_grid_layout_ = false;
            deck_view_standalone_vertical_nudge_ = 0.f;
            deck_view_detail_active_         = false;
            deck_view_detail_show_upgraded_  = false;
            deck_view_detail_inst_           = CardInstance{};
            deck_view_hover_index_ = -1;
            deck_view_hover_blend_ = 0.f;
            deck_view_glossary_entries_.clear();
            deck_view_glossary_active_ = false;
            pile_anim_snapshot_ready_ = false;
            prev_discard_ids_for_anim_.clear();
            pending_select_ui_pile_fly_remaining_ = 0;
            return;
        }
        // 默认滚动到第二行可见、并露出第三行一部分（3 行及以上时）
        constexpr float ROW_CENTER_TO_CENTER = 360.f;
        constexpr float DECK_CARD_H = 300.f;
        float contentTop = 0.f;
        float firstRowCenterY = 0.f;
        float viewTopUnused = 0.f;
        deck_view_grid_layout_(contentTop, firstRowCenterY, viewTopUnused);
        const float firstRowTop = firstRowCenterY - DECK_CARD_H * 0.5f;
        const float padTop = firstRowTop - contentTop;
        const int numRows = (static_cast<int>(deck_view_cards_.size()) + 4) / 5;
        const float lastCardBottomY = numRows > 0
            ? contentTop + padTop + (numRows - 1) * ROW_CENTER_TO_CENTER + DECK_CARD_H
            : contentTop + padTop;
        const float maxScroll = (numRows > 0)
            ? std::max(0.f, lastCardBottomY - static_cast<float>(height_) + DECK_VIEW_SCROLL_BOTTOM_INSET)
            : 0.f;
        if (numRows >= 3)
            deck_view_scroll_y_ = 40.f; // 第二行顶对齐视口顶
        else
            deck_view_scroll_y_ = 0.f;
    }

    void BattleUI::showTip(std::wstring text, float seconds) {
        show_center_tip(std::move(text), seconds);
    }

    bool BattleUI::pollOpenDeckViewRequest(int& outMode) {  // outMode: 1=牌组 2=抽牌堆 3=弃牌堆 4=消耗堆
        if (pending_deck_view_mode_ == 0) return false;
        outMode = pending_deck_view_mode_;
        pending_deck_view_mode_ = 0;
        return true;
    }

    bool BattleUI::pollMapBrowseToggleRequest() {
        if (pending_map_browse_toggle_ == 0) return false;
        pending_map_browse_toggle_ = 0;
        return true;
    }

    void BattleUI::set_pause_menu_active(bool active) {
        pause_menu_active_ = active;
        if (!active) {
            settings_panel_active_ = false;
            pending_pause_menu_choice_ = 0;
        }
    }

    bool BattleUI::pollPauseMenuSelection(int& outChoice) {
        if (pending_pause_menu_choice_ == 0) return false;
        outChoice = pending_pause_menu_choice_;
        pending_pause_menu_choice_ = 0;
        return true;
    }

    void BattleUI::set_reward_screen_active(bool active) {
        reward_screen_active_ = active;
        pile_anim_snapshot_ready_ = false;
        prev_discard_ids_for_anim_.clear();
        pile_card_flights_.clear();
        pile_draw_anim_hiding_.clear();
        pending_select_ui_pile_fly_remaining_ = 0;
        if (!active) {
            reward_card_picked_ = false;
            pending_continue_to_next_battle_ = false;
            pending_reward_card_index_ = -2;
            pending_reward_relic_id_.clear();
            pending_reward_potion_id_.clear();
            pending_reward_potion_replace_slot_ = -2;
            reward_potion_replace_active_ = false;
            reward_potion_replace_slot_rects_.clear();
            hover_reward_card_.clear();
            hover_reward_relic_.clear();
            hover_reward_potion_.clear();
        }
    }

    void BattleUI::set_reward_data(int gold, std::vector<std::string> card_ids,
                                   std::vector<std::string> relic_ids,
                                   std::vector<std::string> potion_ids) {
        reward_gold_ = gold;
        reward_card_ids_ = std::move(card_ids);
        reward_relic_ids_ = std::move(relic_ids);
        reward_potion_ids_ = std::move(potion_ids);
        // 布局矩形不要只在 set_reward_data 里算死：奖励面板会随窗口尺寸变化，
        // 且 drawRewardScreen 内的 panelY/cardY 是动态的。
        // 这里先清空，具体布局在 drawRewardScreen 里按同一套公式每帧刷新。
        reward_card_rects_.clear();
        constexpr float REWARD_CARD_W = 206.f;
        constexpr float REWARD_CARD_H = 280.f;
        constexpr float CARD_GAP = 40.f;
        const float totalCardsW = static_cast<float>(reward_card_ids_.size()) * REWARD_CARD_W
            + (reward_card_ids_.size() > 1 ? (reward_card_ids_.size() - 1) * CARD_GAP : 0.f);
        float cardStartX = (static_cast<float>(width_) - totalCardsW) * 0.5f;
        (void)cardStartX;
        constexpr float SKIP_BTN_W = 140.f;
        constexpr float SKIP_BTN_H = 50.f;
        reward_skip_rect_ = sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(SKIP_BTN_W, SKIP_BTN_H));
        constexpr float CONTINUE_BTN_W = 200.f;
        constexpr float CONTINUE_BTN_H = 56.f;
        reward_continue_rect_ = sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(CONTINUE_BTN_W, CONTINUE_BTN_H));

        // 遗物/灵液选项点击区域也由 drawRewardScreen 统一刷新
        reward_relic_rects_.clear();
        reward_potion_rects_.clear();
    }

    void BattleUI::set_reward_card_picked(bool picked) {
        reward_card_picked_ = picked;
        if (!picked) {
            pending_reward_card_index_ = -2;
            pending_continue_to_next_battle_ = false;
        }
    }

    bool BattleUI::pollContinueToNextBattleRequest() {
        if (!pending_continue_to_next_battle_) return false;
        pending_continue_to_next_battle_ = false;
        return true;
    }

    bool BattleUI::pollRewardCardPick(int& outCardIndex) {
        if (pending_reward_card_index_ < -1) return false;
        outCardIndex = pending_reward_card_index_;
        pending_reward_card_index_ = -2;
        return true;
    }

    bool BattleUI::pollRewardRelicTake(std::string& outRelicId) {
        if (pending_reward_relic_id_.empty()) return false;
        outRelicId = pending_reward_relic_id_;
        pending_reward_relic_id_.clear();
        return true;
    }

    bool BattleUI::pollRewardPotionTake(std::string& outPotionId, int& outReplaceSlot) {
        // 若正在“槽满替换选择”中，不要把领取请求提前吐给主循环
        if (reward_potion_replace_active_) return false;
        if (pending_reward_potion_id_.empty()) return false;
        outPotionId = pending_reward_potion_id_;
        outReplaceSlot = pending_reward_potion_replace_slot_;
        if (outReplaceSlot < -1) return false; // -2=等待选择槽位
        pending_reward_potion_id_.clear();
        pending_reward_potion_replace_slot_ = -2;
        return true;
    }

    std::string BattleUI::get_reward_card_id_at(size_t index) const {
        if (index >= reward_card_ids_.size()) return "";
        return reward_card_ids_[index];
    }

    void BattleUI::set_card_select_active(bool active) {
        card_select_active_ = active;
        if (!active) {
            card_select_rects_.clear();
            card_select_selected_indices_.clear();
            card_select_candidate_hand_indices_.clear();
            card_select_pull_anims_.clear();
            card_select_hide_hand_index_ = -1;
        }
    }

    void BattleUI::compute_card_select_hand_fan_(const BattleStateSnapshot& s, const std::vector<int>& selected_candidate_indices,
                                                 std::vector<int>& out_vis_hand_indices,
                                                 std::vector<sf::Vector2f>& out_centers, std::vector<float>& out_angles) const {
        out_vis_hand_indices.clear();
        out_centers.clear();
        out_angles.clear();
        const size_t handCount = s.hand.size();
        for (size_t i = 0; i < handCount; ++i) {
            const int hi = static_cast<int>(i);
            // 触发选牌界面的那张牌视作已打出，不参与扇面排布
            if (card_select_hide_hand_index_ >= 0 && hi == card_select_hide_hand_index_)
                continue;
            bool pulled = false;
            if (!card_select_candidate_hand_indices_.empty() &&
                card_select_candidate_hand_indices_.size() == card_select_ids_.size()) {
                for (int si : selected_candidate_indices) {
                    if (si < 0 || static_cast<size_t>(si) >= card_select_candidate_hand_indices_.size()) continue;
                    if (card_select_candidate_hand_indices_[static_cast<size_t>(si)] == hi) {
                        pulled = true;
                        break;
                    }
                }
            } else if (card_select_candidate_instance_ids_.size() == card_select_ids_.size()) {
                const InstanceId iid = s.hand[i].instanceId;
                for (int si : selected_candidate_indices) {
                    if (si < 0 || static_cast<size_t>(si) >= card_select_candidate_instance_ids_.size()) continue;
                    if (card_select_candidate_instance_ids_[static_cast<size_t>(si)] == iid) {
                        pulled = true;
                        break;
                    }
                }
            }
            if (!pulled) out_vis_hand_indices.push_back(hi);
        }
        const size_t n = out_vis_hand_indices.size();
        constexpr float DEG2RAD = 3.14159265f / 180.f;
        const float pivotX = static_cast<float>(width_) * 0.5f;
        const float pivotY = static_cast<float>(height_) + HAND_PIVOT_Y_BELOW;
        const float arcTopCenterY = static_cast<float>(height_) - HAND_ARC_TOP_ABOVE_BOTTOM + CARD_H * 0.5f;
        const float arcRadius = pivotY - arcTopCenterY;
        float handFanSpanDeg = (n > 1)
            ? (static_cast<float>(n - 1) * HAND_CARD_DISPLAY_STEP / arcRadius * (180.f / 3.14159265f))
            : 0.f;
        if (handFanSpanDeg > HAND_FAN_SPAN_MAX_DEG) handFanSpanDeg = HAND_FAN_SPAN_MAX_DEG;
        const float angleStepDeg = (n > 1) ? (handFanSpanDeg / static_cast<float>(n - 1)) : 0.f;
        out_centers.resize(n);
        out_angles.resize(n);
        for (size_t j = 0; j < n; ++j) {
            const float angleDeg = n > 1 ? (static_cast<float>(j) - static_cast<float>(n - 1) * 0.5f) * angleStepDeg : 0.f;
            const float rad = angleDeg * DEG2RAD;
            out_centers[j].x = pivotX + arcRadius * std::sin(rad);
            out_centers[j].y = pivotY - arcRadius * std::cos(rad);
            out_angles[j] = angleDeg;
        }
    }

    void BattleUI::set_card_select_data(std::wstring title, std::vector<std::string> card_ids, bool allow_cancel, bool use_hand_area, std::vector<InstanceId> candidate_instance_ids, int required_pick_count, std::vector<int> candidate_hand_indices, int hide_played_hand_index) {
        card_select_title_ = std::move(title);
        card_select_ids_ = std::move(card_ids);
        card_select_candidate_instance_ids_ = std::move(candidate_instance_ids);
        card_select_candidate_hand_indices_ = std::move(candidate_hand_indices);
        card_select_allow_cancel_ = allow_cancel;
        card_select_use_hand_area_ = use_hand_area;
        card_select_required_pick_count_ = std::max(1, required_pick_count);
        card_select_hide_hand_index_ = (use_hand_area && hide_played_hand_index >= 0) ? hide_played_hand_index : -1;
        card_select_rects_.clear();
        card_select_selected_indices_.clear();
        card_select_pull_anims_.clear();
        pending_card_select_indices_.clear();
        pending_card_select_cancelled_ = false;
        pending_card_select_index_ = -2;
        card_select_confirm_pulse_clock_.restart();

        if (use_hand_area) {
            selectedHandIndex_ = -1;
            isAimingCard_ = false;
            selectedCardIsFollowing_ = false;
        }

        if (!card_select_use_hand_area_) {
            constexpr float CARD_W = 206.f;
            constexpr float CARD_H = 280.f;
            constexpr float GAP = 28.f;
            const size_t n = card_select_ids_.size();
            const float totalW = static_cast<float>(n) * CARD_W + (n > 1 ? static_cast<float>(n - 1) * GAP : 0.f);
            const float startX = (static_cast<float>(width_) - totalW) * 0.5f;
            const float y = 320.f;
            for (size_t i = 0; i < n; ++i) {
                const float x = startX + static_cast<float>(i) * (CARD_W + GAP);
                card_select_rects_.emplace_back(sf::Vector2f(x, y), sf::Vector2f(CARD_W, CARD_H));
            }
            constexpr float BTN_W = 140.f;
            constexpr float BTN_H = 50.f;
            card_select_confirm_rect_ = sf::FloatRect(
                sf::Vector2f(width_ * 0.5f - BTN_W * 0.5f, y + CARD_H + 52.f),
                sf::Vector2f(BTN_W, BTN_H));
        } else {
            constexpr float BTN_W = 140.f;
            constexpr float BTN_H = 50.f;
            // 手牌选择模式：确定键上移，整体位于底栏手牌扇区之上（避免与手牌重叠）
            const float btnCenterX = width_ * 0.5f - BTN_W * 0.5f;
            const float btnY = static_cast<float>(height_) - 420.f;
            card_select_confirm_rect_ = sf::FloatRect(
                sf::Vector2f(btnCenterX, btnY),
                sf::Vector2f(BTN_W, BTN_H));
        }
    }

    bool BattleUI::pollCardSelectPick(int& outCardIndex) {
        if (pending_card_select_index_ < -1) return false;
        outCardIndex = pending_card_select_index_;
        pending_card_select_index_ = -2;
        return true;
    }

    bool BattleUI::pollCardSelectResult(std::vector<int>& outCardIndices, bool& outCancelled) {
        if (!pending_card_select_cancelled_ && pending_card_select_indices_.empty()) return false;
        outCancelled = pending_card_select_cancelled_;
        outCardIndices = pending_card_select_indices_;
        pending_card_select_cancelled_ = false;
        pending_card_select_indices_.clear();
        return true;
    }

    std::string BattleUI::get_card_select_id_at(size_t index) const {
        if (index >= card_select_ids_.size()) return "";
        return card_select_ids_[index];
    }

    void BattleUI::tick_pile_card_anims_() {
        for (auto it = pile_card_flights_.begin(); it != pile_card_flights_.end();) {
            const float t = it->clock.getElapsedTime().asSeconds() / it->duration_sec;
            if (t >= 1.f) {
                if (it->kind == PileCardFlightAnim::DrawToHand)
                    pile_draw_anim_hiding_.erase(it->instance_id);
                it = pile_card_flights_.erase(it);
            } else
                ++it;
        }
    }

    sf::Vector2f BattleUI::hand_fan_card_center_(size_t hand_index, size_t hand_count) const {
        if (hand_count == 0) return sf::Vector2f(static_cast<float>(width_) * 0.5f, static_cast<float>(height_) * 0.82f);
        constexpr float DEG2RAD = 3.14159265f / 180.f;
        constexpr float HAND_PIVOT_Y_BELOW = 1900.f;
        constexpr float HAND_ARC_TOP_ABOVE_BOTTOM = 220.f;
        constexpr float HAND_CARD_DISPLAY_STEP = 154.f;
        constexpr float HAND_FAN_SPAN_MAX_DEG = 45.f;
        constexpr float CARD_H_LOC = 300.f;
        const float pivotX = static_cast<float>(width_) * 0.5f;
        const float pivotY = static_cast<float>(height_) + HAND_PIVOT_Y_BELOW;
        const float arcTopCenterY = static_cast<float>(height_) - HAND_ARC_TOP_ABOVE_BOTTOM + CARD_H_LOC * 0.5f;
        const float arcRadius = pivotY - arcTopCenterY;
        float handFanSpanDeg = (hand_count > 1)
            ? (static_cast<float>(hand_count - 1) * HAND_CARD_DISPLAY_STEP / arcRadius * (180.f / 3.14159265f))
            : 0.f;
        if (handFanSpanDeg > HAND_FAN_SPAN_MAX_DEG) handFanSpanDeg = HAND_FAN_SPAN_MAX_DEG;
        const float angleStepDeg = (hand_count > 1) ? (handFanSpanDeg / static_cast<float>(hand_count - 1)) : 0.f;
        const float angleDeg = hand_count > 1 ? (static_cast<float>(hand_index) - (hand_count - 1) * 0.5f) * angleStepDeg : 0.f;
        const float rad = angleDeg * DEG2RAD;
        const float cx = pivotX + arcRadius * std::sin(rad);
        const float cy = pivotY - arcRadius * std::cos(rad);
        return sf::Vector2f(cx, cy);
    }

    void BattleUI::set_pending_select_ui_pile_fly(int discard_or_exhaust_count) {
        pending_select_ui_pile_fly_remaining_ = std::max(0, discard_or_exhaust_count);
        // 选牌确认后打出：本帧（下一次 detect_pile_card_anims_）里所有 Hand→Discard/Exhaust 飞牌起点强制用中央预览区，
        // 避免 “选牌丢弃数 > UI 选中数” 时出现部分从抽牌堆/手牌位置起飞。
        pending_select_ui_force_center_fly_ = (pending_select_ui_pile_fly_remaining_ > 0);
    }

    sf::Vector2f BattleUI::card_select_preview_center_for_fly_() const {
        // 与 drawCardSelectScreen 手牌区模式中间预览条一致：SEL_H=300，顶 y=268
        constexpr float SEL_H = 300.f;
        constexpr float previewTopY = 268.f;
        const float cx = static_cast<float>(width_) * 0.5f;
        const float cy = previewTopY + SEL_H * 0.5f;
        return sf::Vector2f(cx, cy);
    }

    void BattleUI::pile_pile_screen_centers_(sf::Vector2f& out_draw, sf::Vector2f& out_discard, sf::Vector2f& out_exhaust) const {
        constexpr float PILE_ICON_W = 52.f;
        constexpr float PILE_ICON_H = 72.f;
        constexpr float PILE_CENTER_OFFSET = 36.f;
        constexpr float SIDE_MARGIN = 24.f;
        constexpr float BOTTOM_MARGIN = 20.f;
        const float drawPileX = SIDE_MARGIN + PILE_CENTER_OFFSET - 4.f;
        const float drawPileY = static_cast<float>(height_) - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
        out_draw = sf::Vector2f(drawPileX + PILE_ICON_W * 0.5f, drawPileY + PILE_ICON_H * 0.5f);
        const float discardX = static_cast<float>(width_) - SIDE_MARGIN - PILE_ICON_W - PILE_CENTER_OFFSET + 4.f;
        const float discardY = static_cast<float>(height_) - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
        out_discard = sf::Vector2f(discardX + PILE_ICON_W * 0.5f, discardY + PILE_ICON_H * 0.5f);
        constexpr float exW = PILE_ICON_W - 6.f;
        constexpr float exH = 48.f;
        out_exhaust = sf::Vector2f(discardX + 3.f + exW * 0.5f, discardY - 56.f + exH * 0.5f);
    }

    void BattleUI::detect_pile_card_anims_(const BattleStateSnapshot& s) {
        if (deck_view_active_) return;

        if (!pile_anim_snapshot_ready_) {
            prev_hand_for_pile_anim_ = s.hand;
            prev_draw_sz_for_anim_ = s.drawPileSize;
            prev_discard_sz_for_anim_ = s.discardPileSize;
            prev_exhaust_sz_for_anim_ = s.exhaustPileSize;
            prev_discard_ids_for_anim_ = s.discardPileCardIds;
            pile_anim_snapshot_ready_ = true;
            return;
        }

        std::unordered_set<InstanceId> prev_ids;
        prev_ids.reserve(prev_hand_for_pile_anim_.size() + 4);
        for (const auto& c : prev_hand_for_pile_anim_) prev_ids.insert(c.instanceId);
        std::unordered_set<InstanceId> cur_ids;
        cur_ids.reserve(s.hand.size() + 4);
        for (const auto& c : s.hand) cur_ids.insert(c.instanceId);

        sf::Vector2f drawC, discardC, exhaustC;
        pile_pile_screen_centers_(drawC, discardC, exhaustC);

        constexpr size_t kMaxNewAnims = 16;

        std::vector<CardInstance> removed;
        removed.reserve(8);
        for (const auto& c : prev_hand_for_pile_anim_) {
            if (!cur_ids.count(c.instanceId)) removed.push_back(c);
        }

        int discard_delta = s.discardPileSize - prev_discard_sz_for_anim_;
        int exhaust_delta = s.exhaustPileSize - prev_exhaust_sz_for_anim_;
        const int draw_delta = s.drawPileSize - prev_draw_sz_for_anim_;

        // 弃牌堆洗回抽牌堆：表现层做“弃牌堆 -> 抽牌堆”的飞牌动画
        // 判定：上一帧弃牌堆有牌，本帧弃牌堆变小且抽牌堆变大（常见为弃牌清空、抽牌增加）
        if (prev_discard_sz_for_anim_ > 0 && s.discardPileSize < prev_discard_sz_for_anim_ && draw_delta > 0) {
            const int moved = std::min(prev_discard_sz_for_anim_, draw_delta);
            const int kMax = 10;
            const int nAnim = std::min(moved, kMax);
            // 先播放一小段“洗牌”停顿，再开始飞牌（避免洗牌与飞牌同时进行）
            const float shuffleLead = 0.42f;
            for (int i = 0; i < nAnim && pile_card_flights_.size() < kMaxNewAnims + 8; ++i) {
                PileCardFlightAnim a;
                a.card_id = "strike";
                const auto& pd = prev_discard_ids_for_anim_;
                if (!pd.empty()) {
                    const size_t k = pd.size();
                    // 弃牌堆为 push_back 顺序，视觉上常把后入者视作靠近顶牌，从尾部向里取不同 id
                    a.card_id = pd[k - 1 - (static_cast<size_t>(i) % k)];
                }
                a.start = discardC;
                a.end = drawC;
                a.kind = PileCardFlightAnim::DiscardToDraw;
                a.start_delay_sec = shuffleLead + 0.06f * static_cast<float>(i);
                a.duration_sec = 0.42f;
                a.use_arc_path = true;
                a.clock.restart();
                pile_card_flights_.push_back(std::move(a));
            }
        }

        // 新入手牌：从抽牌堆飞向扇区目标位
        for (size_t i = 0; i < s.hand.size() && pile_card_flights_.size() < kMaxNewAnims + 8; ++i) {
            const auto& c = s.hand[i];
            if (prev_ids.count(c.instanceId)) continue;
            PileCardFlightAnim a;
            a.card_id     = c.id;
            a.start       = drawC;
            a.end         = hand_fan_card_center_(i, s.hand.size());
            a.kind        = PileCardFlightAnim::DrawToHand;
            a.instance_id = c.instanceId;
            a.duration_sec = 0.38f;
            a.use_arc_path = true;
            a.clock.restart();
            pile_card_flights_.push_back(std::move(a));
            pile_draw_anim_hiding_.insert(c.instanceId);
        }

        bool used_force_center_fly = false;
        for (const auto& c : removed) {
            if (pile_card_flights_.size() >= kMaxNewAnims + 8) break;
            sf::Vector2f start = drawC;
            auto it = instance_hand_center_cache_.find(c.instanceId);
            if (it != instance_hand_center_cache_.end()) start = it->second;

            PileCardFlightAnim a;
            a.card_id     = c.id;
            a.start       = start;
            a.instance_id = c.instanceId;
            a.duration_sec = 0.38f;
            a.clock.restart();
            if (exhaust_delta > 0) {
                a.end = exhaustC;
                a.kind = PileCardFlightAnim::HandToExhaust;
                --exhaust_delta;
            } else if (discard_delta > 0) {
                a.end = discardC;
                a.kind = PileCardFlightAnim::HandToDiscard;
                --discard_delta;
            } else
                continue;
            // 手牌→弃牌/消耗：统一走二次贝塞尔弧线；选牌界面确认时起点改为中央预览区
            a.use_arc_path = true;
            if (pending_select_ui_force_center_fly_) {
                a.start = card_select_preview_center_for_fly_();
                a.duration_sec = 0.42f;
                used_force_center_fly = true;
            } else if (pending_select_ui_pile_fly_remaining_ > 0) {
                // 旧行为兼容：若仍用 remaining 驱动（例如未来别处复用），仅覆盖前 N 张
                a.start = card_select_preview_center_for_fly_();
                a.duration_sec = 0.42f;
                --pending_select_ui_pile_fly_remaining_;
            }
            pile_card_flights_.push_back(std::move(a));
        }
        if (used_force_center_fly) {
            pending_select_ui_force_center_fly_ = false;
            pending_select_ui_pile_fly_remaining_ = 0;
        }

        prev_hand_for_pile_anim_ = s.hand;
        prev_draw_sz_for_anim_ = s.drawPileSize;
        prev_discard_sz_for_anim_ = s.discardPileSize;
        prev_exhaust_sz_for_anim_ = s.exhaustPileSize;
        prev_discard_ids_for_anim_ = s.discardPileCardIds;
    }

    void BattleUI::draw_pile_card_anims_(sf::RenderWindow& window) {
        if (!fontLoaded_ || pile_card_flights_.empty()) return;
        constexpr float FW = 130.f;
        constexpr float FH = 189.f;
        for (const auto& a : pile_card_flights_) {
            float t = a.clock.getElapsedTime().asSeconds() / a.duration_sec;
            if (t > 1.f) t = 1.f;
            const float te = 1.f - (1.f - t) * (1.f - t);
            sf::Vector2f pos;
            if (a.use_arc_path) {
                const sf::Vector2f& p0 = a.start;
                const sf::Vector2f& p2 = a.end;
                sf::Vector2f p1 = (p0 + p2) * 0.5f;
                const float dx = p2.x - p0.x;
                const float bow = std::min(220.f, std::abs(dx) * 0.45f + 90.f);
                p1.y -= bow;
                const float u = 1.f - te;
                pos = p0 * (u * u) + p1 * (2.f * u * te) + p2 * (te * te);
            } else {
                pos = a.start * (1.f - te) + a.end * te;
            }
            sf::Color outline(210, 190, 120);
            if (a.kind == PileCardFlightAnim::HandToDiscard) outline = sf::Color(130, 130, 220);
            else if (a.kind == PileCardFlightAnim::HandToExhaust) outline = sf::Color(150, 150, 150);
            // DiscardToDraw 与 DrawToHand 共用默认描边，卡面 id 已与真实弃牌一致

            // 缩放：整体更小，且飞行过程略“放大到位”，让运动更自然
            float scale = 0.74f;
            if (a.kind == PileCardFlightAnim::DrawToHand) scale = 0.70f + 0.10f * te;
            else if (a.kind == PileCardFlightAnim::DiscardToDraw) scale = 0.66f + 0.08f * te;
            else scale = 0.70f + 0.08f * te;
            const float w = FW * scale;
            const float h = FH * scale;
            const float th = 2.25f + 0.6f * te;
            drawDetailedCardAt(window, a.card_id, pos.x - w * 0.5f, pos.y - h * 0.5f, w, h, outline, th);
        }
    }

    void BattleUI::draw(sf::RenderWindow& window, IBattleUIDataProvider& data) {
        const BattleStateSnapshot& s = data.get_snapshot();  // 从适配器取战斗状态快照
        // 拷贝到成员：lastSnapshot_ 若指向适配器/调用方栈上的临时引用，事件在下一帧处理时会悬垂，表现为 hand.size()==0
        snapshotForEvents_ = s;
        lastSnapshot_      = &snapshotForEvents_;
        if (!fontLoaded_) return;                   // 字体未加载则不绘制（避免崩溃）

        pendingBattleStatusIcons_.clear();          // 由 drawBattleCenter 入队，随后在 drawBottomBar 之前 flush（手牌盖住效果栏）

        const bool bottomHudInteractive = !deck_view_active_ && !pause_menu_active_ && !settings_panel_active_
            && !map_overlay_blocks_world_input_ && !card_select_active_ && !reward_screen_active_;
        const bool potionSlotsInteractive = !deck_view_active_ && !pause_menu_active_ && !settings_panel_active_
            && !map_overlay_blocks_world_input_ && !reward_screen_active_ && !card_select_active_;
        update_interactive_hover_(bottomHudInteractive, potionSlotsInteractive);

        tick_pile_card_anims_();
        detect_pile_card_anims_(snapshotForEvents_);

        // 背景图（最底层，铺满窗口）
        int idx = currentBackgroundIndex_;
        if (idx < 0) idx = 0;
        if (static_cast<size_t>(idx) >= backgroundTextures_.size() && !backgroundTextures_.empty())
            idx = 0;  // 无对应图时用第 0 张
        if (static_cast<size_t>(idx) < backgroundTextures_.size()) {
            const sf::Texture& tex = backgroundTextures_[static_cast<size_t>(idx)];
            sf::Sprite bgSprite(tex);
            bgSprite.setPosition(sf::Vector2f(0.f, 0.f));
            float scaleX = static_cast<float>(width_) / static_cast<float>(tex.getSize().x);
            float scaleY = static_cast<float>(height_) / static_cast<float>(tex.getSize().y);
            bgSprite.setScale(sf::Vector2f(scaleX, scaleY));
            window.draw(bgSprite);
        }

        // 顶栏背景色块
        sf::RectangleShape topBg(sf::Vector2f(static_cast<float>(width_), TOP_BAR_BG_H));
        topBg.setPosition(sf::Vector2f(0.f, 0.f));
        topBg.setFillColor(TOP_BAR_BG_COLOR);
        window.draw(topBg);

        drawTopBar(window, s);                      // 顶栏：钥匙槽、名字、职业、HP、金币、灵液、层数
        drawRelicsRow(window, s);                   // 遗物行：顶栏下方，最多 12 个遗物图标

        if (deck_view_active_) {                    // 牌组界面打开时只画牌组网格+返回按钮
            drawDeckView(window, s);
            drawTopRight(window, s);
            drawRelicPotionTooltip(window, s);      // 顶栏遗物/灵液仍可悬停查看
            draw_center_tip(window);
            return;
        }

        if (reward_screen_active_) {                 // 奖励界面：半透明遮罩+胜利标题+金币+遗物/灵液+三选一卡牌+跳过/继续
            drawBattleCenter(window, s);             // 底层仍画战场（模糊背景感）
            flushPendingBattleStatusIcons_(window);
            drawBottomBar(window, s);
            draw_pile_card_anims_(window);
            drawTopRight(window, s);
            drawRewardScreen(window);
            draw_center_tip(window);
            return;
        }

        if (card_select_active_) {                   // 选牌弹窗：底层战场 + 弹窗
            drawBattleCenter(window, s);
            flushPendingBattleStatusIcons_(window);
            drawBottomBar(window, s);
            draw_pile_card_anims_(window);
            drawTopRight(window, s);
            drawCardSelectScreen(window);
            draw_center_tip(window);
            return;
        }

        drawBattleCenter(window, s);                 // 战场中央：玩家区（模型+血条+状态）、怪物区（意图+模型+血条+状态）、伤害数字

        drawRelicPotionTooltip(window, s);           // 遗物/灵液悬停提示（顶栏灵液槽、遗物行图标）

        // 不再铺手牌区整宽纯色底条，避免挡住战斗背景；手牌与底栏控件直接叠在背景上。

        flushPendingBattleStatusIcons_(window);      // 血条下状态图标在手牌/飞牌之下
        drawBottomBar(window, s);                   // 底栏：抽牌堆、能量、手牌（扇形）、结束回合、弃牌堆、消耗堆
        draw_pile_card_anims_(window);
        drawTopRight(window, s, true);              // 右上角：地图/牌组/设置按钮、回合数（战斗界面显示回合数）
        draw_center_tip(window);                    // 中央提示（如"能量不足"），带淡出

        // 战斗界面中的暂停菜单 / 设置界面（与全局 HUD 版本保持一致）
        if (pause_menu_active_ || settings_panel_active_) {
            drawPauseMenuOverlay(window);
        }
    }

    void BattleUI::drawGlobalHud(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        if (!fontLoaded_) return;
        const bool potionSlotsInteractive = !pause_menu_active_ && !settings_panel_active_
            && !map_overlay_blocks_world_input_ && !deck_view_active_;
        update_interactive_hover_(false, potionSlotsInteractive);  // 无底栏牌堆；灵液与右上角可悬停
        // 顶栏背景色块与战斗界面保持一致
        sf::RectangleShape topBg(sf::Vector2f(static_cast<float>(width_), TOP_BAR_BG_H));
        topBg.setPosition(sf::Vector2f(0.f, 0.f));
        topBg.setFillColor(TOP_BAR_BG_COLOR);
        window.draw(topBg);

        // 顶栏 + 遗物行 + 右上角三个按钮（非战斗界面不显示回合数）
        drawTopBar(window, s);
        drawRelicsRow(window, s);
        drawTopRight(window, s, false);

        // 在地图/事件/商店/休息/宝箱等界面上，同样支持顶栏遗物/灵液的悬停提示
        drawRelicPotionTooltip(window, s);

        // 在地图/事件/商店/休息等界面上叠加牌组视图（含遮罩）
        if (deck_view_active_) {
            drawDeckView(window, s);
        }

        // 在地图/事件/商店/休息等界面上叠加暂停菜单 / 设置界面（布局与战斗界面保持一致）
        if (pause_menu_active_ || settings_panel_active_) {
            drawPauseMenuOverlay(window);
        }

        // 与战斗界面一致：中央提示（如地图/商店内 showTip）
        draw_center_tip(window);
    }

    void BattleUI::drawDeckViewOnly(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        if (!fontLoaded_) return;
        if (!deck_view_active_) return;
        drawDeckViewStandalone_(window, s);
    }

    void BattleUI::layout_pause_settings_controls_(float panelX, float panelY, float panelW, float panelH) {
        const float cx = panelX + panelW * 0.5f;
        float       y  = panelY + 96.f;
        const float rowH = 44.f;
        settingsVolDownRect_ = sf::FloatRect(sf::Vector2f(cx - 210.f, y), sf::Vector2f(52.f, rowH));
        settingsVolUpRect_   = sf::FloatRect(sf::Vector2f(cx + 158.f, y), sf::Vector2f(52.f, rowH));
        y += 72.f;
        // 与音量按钮对齐：同列、同宽，避免视觉跳动
        settingsResPrevRect_ = sf::FloatRect(sf::Vector2f(cx - 210.f, y), sf::Vector2f(52.f, rowH));
        settingsResNextRect_ = sf::FloatRect(sf::Vector2f(cx + 158.f, y), sf::Vector2f(52.f, rowH));
        y += 76.f;
        const float appW = 300.f;
        const float appH = 48.f;
        settingsApplyVideoRect_ = sf::FloatRect(sf::Vector2f(cx - appW * 0.5f, y), sf::Vector2f(appW, appH));
        const float btnW = 240.f;
        const float btnH = 56.f;
        const float bx   = panelX + (panelW - btnW) * 0.5f;
        const float by   = panelY + panelH - btnH - 36.f;
        settingsBackRect_ = sf::FloatRect(sf::Vector2f(bx, by), sf::Vector2f(btnW, btnH));
    }

    void BattleUI::drawPauseMenuOverlay(sf::RenderWindow& window) {
        const float viewTop    = TOP_BAR_BG_H;
        const float viewHeight = static_cast<float>(height_) - viewTop;
        sf::RectangleShape dimBg(sf::Vector2f(static_cast<float>(width_), viewHeight));
        dimBg.setPosition(sf::Vector2f(0.f, viewTop));
        dimBg.setFillColor(sf::Color(10, 10, 16, 220));
        window.draw(dimBg);

        const float panelW = 760.f;
        float panelH = settings_panel_active_ ? 520.f : 420.f;
        if (!settings_panel_active_ && pause_save_slot_panel_active_) panelH += 86.f;
        const float panelX = (static_cast<float>(width_) - panelW) * 0.5f;
        const float panelY = (static_cast<float>(height_) - panelH) * 0.5f;

        sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
        panel.setPosition(sf::Vector2f(panelX, panelY));
        panel.setFillColor(sf::Color(32, 30, 40, 255));
        panel.setOutlineColor(sf::Color(200, 190, 150));
        panel.setOutlineThickness(2.f);
        window.draw(panel);

        const float titleY = panelY + 36.f;
        sf::Text title(fontForChinese(), settings_panel_active_ ? sf::String(L"设置") : sf::String(L"暂停"), 42);
        title.setFillColor(sf::Color(245, 240, 225));
        const sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        title.setPosition(sf::Vector2f(panelX + panelW * 0.5f, titleY));
        window.draw(title);

        if (!settings_panel_active_) {
            const float btnW = 320.f;
            const float btnH = 60.f;
            const float firstY = panelY + 120.f;
            const float gap = 24.f;
            const float centerX = panelX + panelW * 0.5f;

            auto drawPauseBtn = [&](const std::wstring& label, float y, sf::FloatRect& outRect) {
                const float x = centerX - btnW * 0.5f;
                outRect = sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(btnW, btnH));
                sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
                btn.setPosition(sf::Vector2f(x, y));
                btn.setFillColor(sf::Color(70, 65, 75));
                btn.setOutlineColor(sf::Color(170, 160, 130));
                btn.setOutlineThickness(2.f);
                window.draw(btn);

                sf::Text t(fontForChinese(), sf::String(label), 26);
                t.setFillColor(sf::Color(235, 230, 220));
                const sf::FloatRect lb = t.getLocalBounds();
                t.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                t.setPosition(sf::Vector2f(x + btnW * 0.5f, y + btnH * 0.5f));
                window.draw(t);
            };

            drawPauseBtn(L"返回游戏", firstY, pauseResumeRect_);
            drawPauseBtn(L"保存并退出", firstY + (btnH + gap), pauseSaveQuitRect_);
            drawPauseBtn(L"设置", firstY + 2.f * (btnH + gap), pauseSettingsRect_);
        } else {
            layout_pause_settings_controls_(panelX, panelY, panelW, panelH);

            auto drawSmallBtn = [&](const sf::FloatRect& r, const std::wstring& label) {
                const bool hover = r.contains(mousePos_);
                sf::RectangleShape b(r.size);
                b.setPosition(r.position);
                b.setFillColor(hover ? sf::Color(86, 80, 104) : sf::Color(62, 58, 78));
                b.setOutlineColor(hover ? sf::Color(220, 205, 155) : sf::Color(170, 160, 130));
                b.setOutlineThickness(hover ? 2.5f : 2.f);
                window.draw(b);
                sf::Text tx(fontForChinese(), sf::String(label), 22);
                tx.setFillColor(sf::Color(235, 230, 220));
                const sf::FloatRect lb = tx.getLocalBounds();
                tx.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                tx.setPosition(sf::Vector2f(r.position.x + r.size.x * 0.5f, r.position.y + r.size.y * 0.5f));
                window.draw(tx);
            };

            sf::Text labVol(fontForChinese(), sf::String(L"主音量"), 24);
            labVol.setFillColor(sf::Color(210, 205, 195));
            labVol.setPosition(sf::Vector2f(panelX + 48.f, panelY + 100.f));
            window.draw(labVol);

            drawSmallBtn(settingsVolDownRect_, L"－");
            drawSmallBtn(settingsVolUpRect_, L"＋");
            {
                const int v = static_cast<int>(UserSettings::instance().masterVolume() + 0.5f);
                sf::Text tv(fontForChinese(), sf::String(std::to_wstring(v) + L" %"), 24);
                tv.setFillColor(sf::Color(245, 235, 220));
                const sf::FloatRect lb = tv.getLocalBounds();
                tv.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                const float cx = panelX + panelW * 0.5f;
                const float cy = settingsVolDownRect_.position.y + settingsVolDownRect_.size.y * 0.5f;
                tv.setPosition(sf::Vector2f(cx, cy));
                window.draw(tv);
            }

            sf::Text labRes(fontForChinese(), sf::String(L"分辨率"), 24);
            labRes.setFillColor(sf::Color(210, 205, 195));
            labRes.setPosition(sf::Vector2f(panelX + 48.f, settingsResPrevRect_.position.y + 4.f));
            window.draw(labRes);

            // 用 ASCII 箭头，避免字体缺字导致的方块/乱码
            drawSmallBtn(settingsResPrevRect_, L"<");
            drawSmallBtn(settingsResNextRect_, L">");
            {
                const std::wstring label =
                    UserSettings::instance().videoPresetLabelForIndex(settings_video_preset_preview_);
                sf::Text tr(fontForChinese(), sf::String(label), 22);
                tr.setFillColor(sf::Color(245, 235, 220));
                const sf::FloatRect lb = tr.getLocalBounds();
                tr.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                tr.setPosition(sf::Vector2f(panelX + panelW * 0.5f,
                                            settingsResPrevRect_.position.y + settingsResPrevRect_.size.y * 0.5f));
                window.draw(tr);
            }

            {
                const bool hover = settingsApplyVideoRect_.contains(mousePos_);
                sf::RectangleShape ab(settingsApplyVideoRect_.size);
                ab.setPosition(settingsApplyVideoRect_.position);
                ab.setFillColor(hover ? sf::Color(72, 110, 86) : sf::Color(52, 82, 64));
                ab.setOutlineColor(hover ? sf::Color(190, 230, 170) : sf::Color(140, 180, 130));
                ab.setOutlineThickness(hover ? 2.5f : 2.f);
                window.draw(ab);
                sf::Text ta(fontForChinese(), sf::String(L"应用分辨率"), 24);
                ta.setFillColor(sf::Color(235, 245, 230));
                const sf::FloatRect lb = ta.getLocalBounds();
                ta.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                ta.setPosition(sf::Vector2f(settingsApplyVideoRect_.position.x + settingsApplyVideoRect_.size.x * 0.5f,
                                            settingsApplyVideoRect_.position.y + settingsApplyVideoRect_.size.y * 0.5f));
                window.draw(ta);
            }

            const float bx = settingsBackRect_.position.x;
            const float by = settingsBackRect_.position.y;
            const float btnW = settingsBackRect_.size.x;
            const float btnH = settingsBackRect_.size.y;
            sf::RectangleShape btn(sf::Vector2f(btnW, btnH));
            btn.setPosition(sf::Vector2f(bx, by));
            const bool hoverBack = settingsBackRect_.contains(mousePos_);
            btn.setFillColor(hoverBack ? sf::Color(86, 80, 104) : sf::Color(62, 58, 78));
            btn.setOutlineColor(hoverBack ? sf::Color(220, 205, 155) : sf::Color(170, 160, 130));
            btn.setOutlineThickness(hoverBack ? 2.5f : 2.f);
            window.draw(btn);

            sf::Text t(fontForChinese(), sf::String(L"返回"), 26);
            t.setFillColor(sf::Color(235, 230, 220));
            const sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
            t.setPosition(sf::Vector2f(bx + btnW * 0.5f, by + btnH * 0.5f));
            window.draw(t);
        }
    }

    // 顶栏从左到右：钥匙槽、名字、职业、HP、金币、灵液槽（1~5 槽）、当前层数（由 set_top_bar_map_floor 驱动）
    void BattleUI::drawTopBar(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        const float left = 28.f;              // 左侧起始
        const float rowY = TOP_ROW_Y + 8.f;   // 统一基线高度
        const float itemGap = 20.f;            // 元素间距
        const unsigned nameSize = 42;          // 名字字号
        const float nameY = rowY - 10.f;      // 名字上移
        const unsigned hpSize = 28;            // HP 字号
        const unsigned restSize = 26;          // 其它文字字号
        float x = left;                       // 当前绘制 x 位置
        char buf[128];

        // 顶栏小图标（懒加载）
        auto ensure_topbar_icon = [&](const char* base, sf::Texture& tex, bool& loadedFlag) {
            if (loadedFlag) return;
            const std::string p = resolve_image_path(base);
            if (!p.empty() && load_sf_texture_utf8(tex, p)) {
                tex.setSmooth(true);
                loadedFlag = true;
            }
        };

        // 1. 钥匙槽
        const float keySz = 44.f;
        sf::RectangleShape keySlot(sf::Vector2f(keySz, keySz));
        keySlot.setPosition(sf::Vector2f(x, rowY - 4.f));
        keySlot.setFillColor(sf::Color(60, 55, 65));
        keySlot.setOutlineColor(sf::Color(140, 130, 100));
        keySlot.setOutlineThickness(1.f);
        window.draw(keySlot);
        x += keySz + itemGap;                  // 右移为下一项留位

        // 2. 名字（变大、上移）
        sf::Text nameText(font_, s.playerName, nameSize);
        nameText.setFillColor(sf::Color(240, 240, 240));
        nameText.setPosition(sf::Vector2f(x, nameY));
        window.draw(nameText);
        x += 110.f + 10.f;                    // 职业贴着名字，只留小间隙

        // 3. 职业（贴着名字）
        sf::Text charText(font_, s.character, restSize);
        charText.setFillColor(sf::Color(200, 200, 210));
        charText.setPosition(sf::Vector2f(x, rowY));
        window.draw(charText);
        x += 88.f + itemGap + 80.f;           // 生命再往右

        // 4. HP
        // 资源目录为 assets/images（注意复数）
        ensure_topbar_icon("assets/images/heart", topbarHeartIconTex_, topbarHeartIconLoaded_);
        const float hpIconSz = 34.f;
        if (topbarHeartIconLoaded_) {
            sf::Sprite heart(topbarHeartIconTex_);
            const sf::FloatRect tr = heart.getLocalBounds();
            const float sx = (tr.size.x > 0.f) ? (hpIconSz / tr.size.x) : 1.f;
            const float sy = (tr.size.y > 0.f) ? (hpIconSz / tr.size.y) : 1.f;
            heart.setScale(sf::Vector2f(sx, sy));
            heart.setPosition(sf::Vector2f(x, rowY + 3.f));
            window.draw(heart);
        }
        const float hpTextX = x + hpIconSz + 6.f;
        std::snprintf(buf, sizeof(buf), "%d/%d", s.currentHp, s.maxHp);
        sf::Text hpText(font_, buf, hpSize);
        hpText.setFillColor(sf::Color(230, 80, 80));
        hpText.setPosition(sf::Vector2f(hpTextX, rowY));
        window.draw(hpText);
        x += 96.f + itemGap + 50.f;           // 金币再往右

        // 5. 金币
        ensure_topbar_icon("assets/images/gold", topbarGoldIconTex_, topbarGoldIconLoaded_);
        const float goldIconSz = 32.f;
        const float goldShiftLeft = 18.f; // 让金币与图标整体更靠左
        const float goldX = x - goldShiftLeft;
        if (topbarGoldIconLoaded_) {
            sf::Sprite gold(topbarGoldIconTex_);
            const sf::FloatRect tr = gold.getLocalBounds();
            const float sx = (tr.size.x > 0.f) ? (goldIconSz / tr.size.x) : 1.f;
            const float sy = (tr.size.y > 0.f) ? (goldIconSz / tr.size.y) : 1.f;
            gold.setScale(sf::Vector2f(sx, sy));
            gold.setPosition(sf::Vector2f(goldX, rowY + 5.f));
            window.draw(gold);
        }
        const float goldTextX = goldX + goldIconSz + 6.f;
        std::snprintf(buf, sizeof(buf), "%d", s.gold);
        sf::Text goldText(font_, buf, restSize);
        goldText.setFillColor(sf::Color(255, 200, 80));
        goldText.setPosition(sf::Vector2f(goldTextX, rowY));
        window.draw(goldText);
        x += 44.f + itemGap + 12.f;          // 灵液栏再往右，金币与灵液栏间隙缩小

        // 6. 灵液栏：槽位矩形保持未动画（供点击/提示命中）；绘制时按 hover_potion_slot_ 插值放大上移
        const int potionSlotCount = std::max(1, std::min(5, s.potionSlotCount));  // 1~5 槽
        const float potionW = 48.f;
        const float potionH = 40.f;
        const float potionGap = 14.f;
        const float potionStartX = x;
        potionSlotRects_.clear();

        auto potion_slot_anim_box = [](float slotX, float baseY, float bw, float bh, float hov) -> std::array<float, 4> {
            const float sc = 1.f + 0.10f * hov;
            const float nw = bw * sc;
            const float nh = bh * sc;
            const float nx = slotX + bw * 0.5f - nw * 0.5f;
            const float ny = baseY + bh * 0.5f - nh * 0.5f - 5.f * hov;
            return {nx, ny, nw, nh};
        };

        for (int i = 0; i < potionSlotCount; ++i) {
            const float slotX = potionStartX + i * (potionW + potionGap);
            const float baseY = rowY - 4.f;
            potionSlotRects_.push_back(sf::FloatRect(sf::Vector2f(slotX, baseY), sf::Vector2f(potionW, potionH)));
            const float hov = hover_potion_slot_[static_cast<size_t>(i)];
            const auto g = potion_slot_anim_box(slotX, baseY, potionW, potionH, hov);
            sf::RectangleShape slot(sf::Vector2f(g[2], g[3]));
            slot.setPosition(sf::Vector2f(g[0], g[1]));
            slot.setFillColor(sf::Color(0, 0, 0, 0));
            const std::uint8_t oa = static_cast<std::uint8_t>(std::min(255, 180 + static_cast<int>(75.f * hov)));
            slot.setOutlineColor(sf::Color(
                ui_hover_lighten_byte(180, hov, 35),
                ui_hover_lighten_byte(180, hov, 35),
                ui_hover_lighten_byte(180, hov, 35),
                oa));
            slot.setOutlineThickness(1.f);
            window.draw(slot);
            x = slotX + potionW + potionGap;
        }
        for (size_t i = 0; i < s.potions.size() && i < static_cast<size_t>(potionSlotCount); ++i) {
            const float slotX = potionStartX + static_cast<float>(i) * (potionW + potionGap);
            const float baseY = rowY - 4.f;
            const float hov = hover_potion_slot_[i];
            const auto g = potion_slot_anim_box(slotX, baseY, potionW, potionH, hov);
            const float icx = g[0] + g[2] * 0.5f;
            const float icy = g[1] + g[3] * 0.5f;
            const std::string& pid = s.potions[i];
            auto it = potionTextures_.find(pid);
            if (it != potionTextures_.end()) {
                sf::Sprite spr(it->second);
                const float iconMaxW = potionW - 6.f;
                const float iconMaxH = potionH - 8.f;
                float scaleX = iconMaxW / std::max(1.f, static_cast<float>(it->second.getSize().x));
                float scaleY = iconMaxH / std::max(1.f, static_cast<float>(it->second.getSize().y));
                const float scale = std::min(scaleX, scaleY) * (1.f + 0.07f * hov);
                spr.setScale(sf::Vector2f(scale, scale));
                const sf::FloatRect tb = spr.getLocalBounds();
                spr.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
                spr.setPosition(sf::Vector2f(icx, icy));
                window.draw(spr);
            } else {
                const float fillW = (potionW - 6.f) * (1.f + 0.06f * hov);
                const float fillH = (potionH - 8.f) * (1.f + 0.06f * hov);
                sf::RectangleShape fill(sf::Vector2f(fillW, fillH));
                fill.setOrigin(sf::Vector2f(fillW * 0.5f, fillH * 0.5f));
                fill.setPosition(sf::Vector2f(icx, icy));
                fill.setFillColor(sf::Color(
                    ui_hover_lighten_byte(180, hov, 25),
                    ui_hover_lighten_byte(140, hov, 25),
                    ui_hover_lighten_byte(255, hov, 15)));
                window.draw(fill);
            }
        }
        x += 16.f;                              // 灵液栏后留空

        // 7. 当前层数（与地图当前节点层同步，由 GameFlow 每帧/进入节点时刷新）
        const float floorX = width_ * 0.62f;
        std::wstring floorStr;
        if (top_bar_map_layer_ < 0) {
            floorStr = L"未选线路";
        } else {
            floorStr = L"第 " + std::to_wstring(top_bar_map_layer_ + 1) + L" 层";
            if (top_bar_map_total_ > 0)
                floorStr += L" / " + std::to_wstring(top_bar_map_total_);
        }
        sf::Text floorText(fontForChinese(), sf::String(floorStr), restSize);
        floorText.setFillColor(sf::Color(220, 210, 200));
        floorText.setPosition(sf::Vector2f(floorX, rowY));
        window.draw(floorText);
    }

    // 遗物栏：顶栏下方，往下一点、图标变大；有图则显示图标，无图则灰色占位（发放时已去重，每类遗物唯一）
    void BattleUI::drawRelicsRow(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        float x = 28.f;                         // 左侧起始
        const float y = RELICS_ROW_Y + RELICS_ICON_Y_OFFSET;  // 下移一点
        relicSlotRects_.clear();
        for (size_t i = 0; i < s.relics.size() && i < 12u; ++i) {  // 最多显示 12 个遗物
            relicSlotRects_.push_back(sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(RELICS_ICON_SZ, RELICS_ICON_SZ)));  // 供悬停检测
            const std::string& rid = s.relics[i];
            auto it = relicTextures_.find(rid);
            if (it != relicTextures_.end()) {    // 有遗物图标则绘制
                sf::Sprite spr(it->second);
                spr.setPosition(sf::Vector2f(x, y));
                float scale = RELICS_ICON_SZ / std::max(1.f, static_cast<float>(it->second.getSize().x));
                spr.setScale(sf::Vector2f(scale, scale));
                window.draw(spr);
            } else {                             // 无图则灰色占位
                sf::RectangleShape icon(sf::Vector2f(RELICS_ICON_SZ, RELICS_ICON_SZ));
                icon.setPosition(sf::Vector2f(x, y));
                icon.setFillColor(sf::Color(80, 70, 90));
                icon.setOutlineColor(sf::Color(120, 110, 130));
                icon.setOutlineThickness(1.f);
                window.draw(icon);
            }
            x += RELICS_ICON_SZ + 18.f;          // 遗物间距稍大
        }
    }

    void BattleUI::drawRelicPotionTooltip(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        const float paddingX = 12.f;
        const float paddingY = 8.f;
        const unsigned fontSize = 20;
        const float maxTooltipW = 320.f;

        // 优先检测遗物（遗物行在左上，先于灵液）
        for (size_t i = 0; i < relicSlotRects_.size() && i < s.relics.size(); ++i) {
            if (!relicSlotRects_[i].contains(mousePos_)) continue;
            auto [name, desc] = get_relic_display_info(s.relics[i]);
            std::wstring text = name;
            if (!desc.empty()) text += L"\n" + desc;

            sf::Text tip(fontForChinese(), sf::String(text), fontSize);
            tip.setFillColor(sf::Color(235, 230, 220));
            const sf::FloatRect tb = tip.getLocalBounds();
            float boxW = std::min(tb.size.x + paddingX * 2.f, maxTooltipW);
            float boxH = tb.size.y + paddingY * 2.f;

            float boxLeft = relicSlotRects_[i].position.x + relicSlotRects_[i].size.x + 8.f;
            float boxTop = relicSlotRects_[i].position.y;
            if (boxLeft + boxW > static_cast<float>(width_)) boxLeft = relicSlotRects_[i].position.x - boxW - 8.f;
            if (boxLeft < 0.f) boxLeft = 8.f;
            if (boxTop + boxH > static_cast<float>(height_)) boxTop = static_cast<float>(height_) - boxH - 8.f;
            if (boxTop < 0.f) boxTop = 8.f;

            sf::RectangleShape bg(sf::Vector2f(boxW, boxH));
            bg.setPosition(sf::Vector2f(boxLeft, boxTop));
            bg.setFillColor(sf::Color(40, 35, 45, 240));
            bg.setOutlineColor(sf::Color(150, 140, 120));
            bg.setOutlineThickness(1.f);
            window.draw(bg);
            tip.setPosition(sf::Vector2f(boxLeft + paddingX - tb.position.x, boxTop + paddingY - tb.position.y));
            window.draw(tip);
            return;  // 只显示一个提示框，避免重叠
        }

        // 再检测灵液（顶栏灵液槽）
        for (size_t i = 0; i < potionSlotRects_.size() && i < s.potions.size(); ++i) {
            if (!potionSlotRects_[i].contains(mousePos_)) continue;
            auto [name, desc] = get_potion_display_info(s.potions[i]);
            std::wstring text = name;
            if (!desc.empty()) text += L"\n" + desc;

            sf::Text tip(fontForChinese(), sf::String(text), fontSize);
            tip.setFillColor(sf::Color(235, 230, 220));
            const sf::FloatRect tb = tip.getLocalBounds();
            float boxW = std::min(tb.size.x + paddingX * 2.f, maxTooltipW);
            float boxH = tb.size.y + paddingY * 2.f;

            float boxLeft = potionSlotRects_[i].position.x + potionSlotRects_[i].size.x + 8.f;
            float boxTop = potionSlotRects_[i].position.y;
            if (boxLeft + boxW > static_cast<float>(width_)) boxLeft = potionSlotRects_[i].position.x - boxW - 8.f;
            if (boxLeft < 0.f) boxLeft = 8.f;
            if (boxTop + boxH > static_cast<float>(height_)) boxTop = static_cast<float>(height_) - boxH - 8.f;
            if (boxTop < 0.f) boxTop = 8.f;

            sf::RectangleShape bg(sf::Vector2f(boxW, boxH));
            bg.setPosition(sf::Vector2f(boxLeft, boxTop));
            bg.setFillColor(sf::Color(40, 35, 45, 240));
            bg.setOutlineColor(sf::Color(150, 140, 120));
            bg.setOutlineThickness(1.f);
            window.draw(bg);
            tip.setPosition(sf::Vector2f(boxLeft + paddingX - tb.position.x, boxTop + paddingY - tb.position.y));
            window.draw(tip);
            return;
        }
    }

    // 奖励界面：半透明遮罩 + 胜利标题 + 金币 + 三选一卡牌 + 跳过/继续按钮，参考杀戮尖塔
    void BattleUI::drawRewardScreen(sf::RenderWindow& window) {
        constexpr float REWARD_CARD_W = 206.f;
        constexpr float REWARD_CARD_H = 280.f;
        constexpr float CARD_GAP = 40.f;
        constexpr float SKIP_BTN_W = 140.f;
        constexpr float SKIP_BTN_H = 50.f;
        constexpr float CONTINUE_BTN_W = 200.f;
        constexpr float CONTINUE_BTN_H = 56.f;

        // 半透明遮罩从遗物行下方开始，保留顶栏和遗物行可见
        constexpr float OVERLAY_TOP = RELICS_ROW_Y + RELICS_ROW_H;
        const float overlayH = static_cast<float>(height_) - OVERLAY_TOP;
        sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(width_), overlayH));
        overlay.setPosition(sf::Vector2f(0.f, OVERLAY_TOP));
        overlay.setFillColor(sf::Color(0, 0, 0, 160));
        window.draw(overlay);

        // 悬停插值（遗物/灵液奖励）
        float dtHover = reward_hover_clock_.restart().asSeconds();
        if (dtHover > 0.08f) dtHover = 0.08f;
        if (dtHover < 0.f) dtHover = 0.f;

        // 中央面板（奖励区）
        const float W = static_cast<float>(width_);
        const float H = static_cast<float>(height_);
        const float panelW = std::min(980.f, W - 72.f);
        const float panelH = std::min(720.f, overlayH - 32.f);
        const float panelX = (W - panelW) * 0.5f;
        const float panelY = OVERLAY_TOP + (overlayH - panelH) * 0.5f;
        {
            sf::RectangleShape shadow(sf::Vector2f(panelW, panelH));
            shadow.setPosition(sf::Vector2f(panelX + 7.f, panelY + 9.f));
            shadow.setFillColor(sf::Color(0, 0, 0, 95));
            window.draw(shadow);
        }
        sf::RectangleShape panel(sf::Vector2f(panelW, panelH));
        panel.setPosition(sf::Vector2f(panelX, panelY));
        panel.setFillColor(sf::Color(28, 26, 36, 246));
        panel.setOutlineColor(sf::Color(205, 192, 150));
        panel.setOutlineThickness(2.25f);
        window.draw(panel);

        sf::Text title(fontForChinese(), sf::String(L"胜利"), 56);
        title.setFillColor(sf::Color(255, 225, 130));
        const sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        title.setPosition(sf::Vector2f(panelX + panelW * 0.5f, panelY + 66.f));
        window.draw(title);

        float rewardRowY = panelY + 120.f;
        if (reward_gold_ > 0) {                             // 有金币奖励时显示（40% 概率）
            char buf[64];
            std::snprintf(buf, sizeof(buf), "+%d 金币", reward_gold_);
            sf::Text goldText(fontForChinese(), sf::String::fromUtf8(buf, buf + std::strlen(buf)), 32);
            goldText.setFillColor(sf::Color(255, 200, 80));
            const sf::FloatRect gb = goldText.getLocalBounds();
            goldText.setOrigin(sf::Vector2f(gb.position.x + gb.size.x * 0.5f, gb.position.y + gb.size.y * 0.5f));
            goldText.setPosition(sf::Vector2f(panelX + panelW * 0.5f, rewardRowY + 18.f));
            window.draw(goldText);
            rewardRowY += 54.f;
        }

        // 遗物与灵液奖励（显示在金币下方，各 40% 概率，互不冲突）
        rewardRowY = (reward_gold_ > 0) ? (panelY + 168.f) : (panelY + 120.f);
        if (!reward_relic_ids_.empty() || !reward_potion_ids_.empty()) {
            constexpr float REWARD_ICON_SZ = 48.f;
            constexpr float REWARD_ICON_GAP = 12.f;
            const float total = static_cast<float>(reward_relic_ids_.size() + reward_potion_ids_.size());
            float x = panelX + panelW * 0.5f - (total
                * (REWARD_ICON_SZ + REWARD_ICON_GAP) - REWARD_ICON_GAP) * 0.5f;

            // 刷新点击矩形 + hover 容器
            reward_relic_rects_.clear();
            reward_potion_rects_.clear();
            hover_reward_relic_.resize(reward_relic_ids_.size(), 0.f);
            hover_reward_potion_.resize(reward_potion_ids_.size(), 0.f);

            for (const auto& rid : reward_relic_ids_) {
                const size_t idx = reward_relic_rects_.size();
                const sf::FloatRect rect(sf::Vector2f(x, rewardRowY), sf::Vector2f(REWARD_ICON_SZ, REWARD_ICON_SZ));
                reward_relic_rects_.push_back(rect);
                const bool over = rect.contains(mousePos_);
                hover_reward_relic_[idx] = ui_hover_lerp(hover_reward_relic_[idx], over ? 1.f : 0.f, dtHover);
                const float h01 = hover_reward_relic_[idx];
                const float grow = 3.f * h01;

                // 图标底板
                {
                    sf::RectangleShape back(sf::Vector2f(REWARD_ICON_SZ + 10.f + grow * 2.f, REWARD_ICON_SZ + 10.f + grow * 2.f));
                    back.setPosition(sf::Vector2f(x - 5.f - grow, rewardRowY - 5.f - grow));
                    back.setFillColor(sf::Color(18, 18, 26, static_cast<std::uint8_t>(150 + 30.f * h01)));
                    back.setOutlineColor(over ? sf::Color(230, 210, 150, 245) : sf::Color(160, 140, 110, 220));
                    back.setOutlineThickness(over ? 2.25f : 1.5f);
                    window.draw(back);
                }
                auto rit = relicTextures_.find(rid);
                if (rit != relicTextures_.end()) {
                    sf::Sprite spr(rit->second);
                    spr.setPosition(sf::Vector2f(x - grow * 0.25f, rewardRowY - grow * 0.25f));
                    float scale = REWARD_ICON_SZ / std::max(1.f, static_cast<float>(rit->second.getSize().x));
                    spr.setScale(sf::Vector2f(scale * (1.f + 0.07f * h01), scale * (1.f + 0.07f * h01)));
                    window.draw(spr);
                } else {
                    sf::RectangleShape icon(sf::Vector2f(REWARD_ICON_SZ, REWARD_ICON_SZ));
                    icon.setPosition(sf::Vector2f(x, rewardRowY));
                    icon.setFillColor(sf::Color(120, 80, 100));
                    icon.setOutlineColor(sf::Color(180, 100, 120));
                    icon.setOutlineThickness(2.f);
                    window.draw(icon);
                }
                sf::Text label(fontForChinese(), sf::String(L"遗物"), 16);
                label.setFillColor(sf::Color(220, 200, 210));
                const sf::FloatRect lb = label.getLocalBounds();
                label.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, 0.f));
                label.setPosition(sf::Vector2f(x + REWARD_ICON_SZ * 0.5f, rewardRowY + REWARD_ICON_SZ + 4.f));
                window.draw(label);
                x += REWARD_ICON_SZ + REWARD_ICON_GAP;
            }
            for (const auto& pid : reward_potion_ids_) {
                const size_t idx = reward_potion_rects_.size();
                const sf::FloatRect rect(sf::Vector2f(x, rewardRowY), sf::Vector2f(REWARD_ICON_SZ, REWARD_ICON_SZ));
                reward_potion_rects_.push_back(rect);
                const bool over = rect.contains(mousePos_);
                hover_reward_potion_[idx] = ui_hover_lerp(hover_reward_potion_[idx], over ? 1.f : 0.f, dtHover);
                const float h01 = hover_reward_potion_[idx];
                const float grow = 3.f * h01;
                {
                    sf::RectangleShape back(sf::Vector2f(REWARD_ICON_SZ + 10.f + grow * 2.f, REWARD_ICON_SZ + 10.f + grow * 2.f));
                    back.setPosition(sf::Vector2f(x - 5.f - grow, rewardRowY - 5.f - grow));
                    back.setFillColor(sf::Color(18, 18, 26, static_cast<std::uint8_t>(150 + 30.f * h01)));
                    back.setOutlineColor(over ? sf::Color(190, 220, 255, 245) : sf::Color(110, 140, 180, 220));
                    back.setOutlineThickness(over ? 2.25f : 1.5f);
                    window.draw(back);
                }
                auto pit = potionTextures_.find(pid);
                if (pit != potionTextures_.end()) {
                    sf::Sprite spr(pit->second);
                    spr.setPosition(sf::Vector2f(x - grow * 0.25f, rewardRowY - grow * 0.25f));
                    float scale = REWARD_ICON_SZ / std::max(1.f, static_cast<float>(pit->second.getSize().x));
                    spr.setScale(sf::Vector2f(scale * (1.f + 0.07f * h01), scale * (1.f + 0.07f * h01)));
                    window.draw(spr);
                } else {
                    sf::RectangleShape icon(sf::Vector2f(REWARD_ICON_SZ, REWARD_ICON_SZ));
                    icon.setPosition(sf::Vector2f(x, rewardRowY));
                    icon.setFillColor(sf::Color(80, 100, 140));
                    icon.setOutlineColor(sf::Color(120, 150, 200));
                    icon.setOutlineThickness(2.f);
                    window.draw(icon);
                }
                sf::Text label(fontForChinese(), sf::String(L"灵液"), 16);
                label.setFillColor(sf::Color(200, 210, 230));
                const sf::FloatRect lb = label.getLocalBounds();
                label.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, 0.f));
                label.setPosition(sf::Vector2f(x + REWARD_ICON_SZ * 0.5f, rewardRowY + REWARD_ICON_SZ + 4.f));
                window.draw(label);
                x += REWARD_ICON_SZ + REWARD_ICON_GAP;
            }
            rewardRowY += REWARD_ICON_SZ + 28.f;
        }

        // 奖励区整体下移：避免卡牌/提示与遗物/灵液行发生重叠
        const bool hasIcons = !(reward_relic_ids_.empty() && reward_potion_ids_.empty());
        const float rewardDown = hasIcons ? 58.f : 26.f;
        const float cardY = !hasIcons
            ? (panelY + 210.f + rewardDown)
            : (panelY + 250.f + rewardDown);
        const float totalCardsW = static_cast<float>(reward_card_ids_.size()) * REWARD_CARD_W
            + (reward_card_ids_.size() > 1 ? (reward_card_ids_.size() - 1) * CARD_GAP : 0.f);
        float cardStartX = (static_cast<float>(width_) - totalCardsW) * 0.5f;

        // 刷新卡牌点击区域（修复“点击区域偏上”）
        reward_card_rects_.clear();
        for (size_t i = 0; i < reward_card_ids_.size(); ++i) {
            const float cardX = cardStartX + static_cast<float>(i) * (REWARD_CARD_W + CARD_GAP);
            reward_card_rects_.emplace_back(sf::Vector2f(cardX, cardY), sf::Vector2f(REWARD_CARD_W, REWARD_CARD_H));
        }
        // 刷新按钮点击区域（与 draw 一致）
        // 按钮整体下移一些，避免与卡牌区过近
        const float btnDown = 28.f;
        reward_skip_rect_.position = sf::Vector2f(width_ * 0.5f - SKIP_BTN_W * 0.5f,
                                                  cardY + REWARD_CARD_H + 26.f + btnDown);
        reward_continue_rect_.position = sf::Vector2f(width_ * 0.5f - CONTINUE_BTN_W * 0.5f,
                                                      reward_skip_rect_.position.y);

        if (!reward_card_picked_) {
            // 三选一提示（更明显）
            {
                // 避免与遗物/灵液行重叠：有图标时提示再下移一些
                const bool hasIcons = !(reward_relic_ids_.empty() && reward_potion_ids_.empty());
                // 提示条靠近卡牌区上沿，比“图标行”更靠下，避免重叠
                const float hintY = cardY - (hasIcons ? 35.f : 50.f);
                sf::RectangleShape bar(sf::Vector2f(420.f, 42.f));
                bar.setPosition(sf::Vector2f(panelX + panelW * 0.5f - 210.f, hintY - 21.f));
                bar.setFillColor(sf::Color(16, 16, 24, 190));
                bar.setOutlineColor(sf::Color(230, 210, 150, 230));
                bar.setOutlineThickness(2.f);
                window.draw(bar);

                sf::Text hint(fontForChinese(), sf::String(L"选择一张卡牌或跳过"), 22);
                hint.setFillColor(sf::Color(245, 238, 220));
                const sf::FloatRect hb = hint.getLocalBounds();
                hint.setOrigin(sf::Vector2f(hb.position.x + hb.size.x * 0.5f, hb.position.y + hb.size.y * 0.5f));
                hint.setPosition(sf::Vector2f(panelX + panelW * 0.5f, hintY));
                window.draw(hint);
            }

            // 悬停插值：卡牌平滑放大
            hover_reward_card_.resize(reward_card_rects_.size(), 0.f);
            for (size_t i = 0; i < reward_card_rects_.size(); ++i) {
                const bool over = reward_card_rects_[i].contains(mousePos_);
                hover_reward_card_[i] = ui_hover_lerp(hover_reward_card_[i], over ? 1.f : 0.f, dtHover);
            }

            for (size_t i = 0; i < reward_card_ids_.size(); ++i) {
                const float cx = cardStartX + REWARD_CARD_W * 0.5f + i * (REWARD_CARD_W + CARD_GAP);
                const float cardX = cx - REWARD_CARD_W * 0.5f;
                const float h01 = (i < hover_reward_card_.size()) ? hover_reward_card_[i] : 0.f;
                const bool hover = h01 > 0.001f;
                const sf::Color outline = hover ? sf::Color(255, 230, 110) : sf::Color(180, 50, 45);
                const float thick = hover ? (6.f + 2.f * h01) : 4.f;
                const float scale = 1.f + 0.06f * h01;

                sf::RenderStates st = sf::RenderStates::Default;
                const float cy = cardY + REWARD_CARD_H * 0.5f;
                st.transform.translate(sf::Vector2f(cx, cy));
                st.transform.scale(sf::Vector2f(scale, scale));
                st.transform.translate(sf::Vector2f(-cx, -cy));
                // 悬停黄色框：更明显的反馈（不依赖卡面描边细节）
                if (hover) {
                    // 外层光晕
                    {
                        const float pad = 10.f;
                        sf::RectangleShape glow(sf::Vector2f(REWARD_CARD_W + pad * 2.f, REWARD_CARD_H + pad * 2.f));
                        glow.setPosition(sf::Vector2f(cardX - pad, cardY - pad));
                        glow.setFillColor(sf::Color(255, 230, 110, 38));
                        glow.setOutlineColor(sf::Color(255, 230, 110, 170));
                        glow.setOutlineThickness(10.f);
                        window.draw(glow, st);
                    }
                    // 内层高亮框
                    {
                        sf::RectangleShape frame(sf::Vector2f(REWARD_CARD_W, REWARD_CARD_H));
                        frame.setPosition(sf::Vector2f(cardX, cardY));
                        frame.setFillColor(sf::Color::Transparent);
                        frame.setOutlineColor(sf::Color(255, 245, 170, 255));
                        frame.setOutlineThickness(6.f);
                        window.draw(frame, st);
                    }
                }
                drawDetailedCardAt(window, reward_card_ids_[i], cardX, cardY, REWARD_CARD_W, REWARD_CARD_H, outline, thick, st);
            }

            sf::RectangleShape skipBtn(sf::Vector2f(SKIP_BTN_W, SKIP_BTN_H));
            skipBtn.setPosition(sf::Vector2f(reward_skip_rect_.position));
            const bool skipHover = reward_skip_rect_.contains(mousePos_);
            skipBtn.setFillColor(skipHover ? sf::Color(92, 86, 102) : sf::Color(72, 68, 82));
            skipBtn.setOutlineColor(skipHover ? sf::Color(220, 205, 155) : sf::Color(150, 142, 128));
            skipBtn.setOutlineThickness(skipHover ? 2.5f : 2.f);
            window.draw(skipBtn);
            sf::Text skipLabel(fontForChinese(), sf::String(L"跳过"), 24);
            skipLabel.setFillColor(sf::Color::White);
            const sf::FloatRect slb = skipLabel.getLocalBounds();
            skipLabel.setOrigin(sf::Vector2f(slb.position.x + slb.size.x * 0.5f, slb.position.y + slb.size.y * 0.5f));
            skipLabel.setPosition(sf::Vector2f(reward_skip_rect_.position.x + SKIP_BTN_W * 0.5f, reward_skip_rect_.position.y + SKIP_BTN_H * 0.5f));
            window.draw(skipLabel);
        } else {
            sf::RectangleShape contBtn(sf::Vector2f(CONTINUE_BTN_W, CONTINUE_BTN_H));
            contBtn.setPosition(sf::Vector2f(reward_continue_rect_.position));
            const bool contHover = reward_continue_rect_.contains(mousePos_);
            contBtn.setFillColor(contHover ? sf::Color(76, 122, 92) : sf::Color(56, 92, 72));
            contBtn.setOutlineColor(contHover ? sf::Color(190, 230, 170) : sf::Color(140, 180, 130));
            contBtn.setOutlineThickness(contHover ? 2.5f : 2.f);
            window.draw(contBtn);
            sf::Text contLabel(fontForChinese(), sf::String(L"前进"), 28);
            contLabel.setFillColor(sf::Color::White);
            const sf::FloatRect clb = contLabel.getLocalBounds();
            contLabel.setOrigin(sf::Vector2f(clb.position.x + clb.size.x * 0.5f, clb.position.y + clb.size.y * 0.5f));
            contLabel.setPosition(sf::Vector2f(reward_continue_rect_.position.x + CONTINUE_BTN_W * 0.5f, reward_continue_rect_.position.y + CONTINUE_BTN_H * 0.5f));
            window.draw(contLabel);
        }

        // 悬停提示（遗物/灵液）
        if (!reward_potion_replace_active_) {
            auto drawTip = [&](const std::wstring& text) {
                if (text.empty()) return;
                constexpr float paddingX = 12.f;
                constexpr float paddingY = 10.f;
                constexpr float maxW = 520.f;
                const int fontSize = 20;
                sf::Text tip(fontForChinese(), sf::String(text), fontSize);
                tip.setFillColor(sf::Color(235, 230, 220));
                const sf::FloatRect tbx = tip.getLocalBounds();
                float boxW = std::min(tbx.size.x + paddingX * 2.f, maxW);
                float boxH = tbx.size.y + paddingY * 2.f;

                float boxLeft = mousePos_.x + 18.f;
                float boxTop  = mousePos_.y + 10.f;
                if (boxLeft + boxW > W) boxLeft = mousePos_.x - boxW - 18.f;
                if (boxLeft < panelX + 8.f) boxLeft = panelX + 8.f;
                if (boxTop + boxH > panelY + panelH - 8.f) boxTop = panelY + panelH - boxH - 8.f;
                if (boxTop < panelY + 8.f) boxTop = panelY + 8.f;

                sf::RectangleShape bg(sf::Vector2f(boxW, boxH));
                bg.setPosition(sf::Vector2f(boxLeft, boxTop));
                bg.setFillColor(sf::Color(40, 35, 45, 240));
                bg.setOutlineColor(sf::Color(150, 140, 120));
                bg.setOutlineThickness(1.f);
                window.draw(bg);
                tip.setPosition(sf::Vector2f(boxLeft + paddingX - tbx.position.x, boxTop + paddingY - tbx.position.y));
                window.draw(tip);
            };

            for (size_t i = 0; i < reward_relic_rects_.size() && i < reward_relic_ids_.size(); ++i) {
                if (!reward_relic_rects_[i].contains(mousePos_)) continue;
                auto [name, desc] = get_relic_display_info(reward_relic_ids_[i]);
                std::wstring t = name;
                if (!desc.empty()) t += L"\n" + desc;
                drawTip(t);
                break;
            }
            for (size_t i = 0; i < reward_potion_rects_.size() && i < reward_potion_ids_.size(); ++i) {
                if (!reward_potion_rects_[i].contains(mousePos_)) continue;
                auto [name, desc] = get_potion_display_info(reward_potion_ids_[i]);
                std::wstring t = name;
                if (!desc.empty()) t += L"\n" + desc;
                drawTip(t);
                break;
            }
        }

        // 槽满领取灵液：选择要丢弃的旧灵液（弹窗）
        if (reward_potion_replace_active_ && lastSnapshot_) {
            const float W = static_cast<float>(width_);
            const float H = static_cast<float>(height_);
            const float boxW = 620.f;
            const float boxH = 210.f;
            const float boxX = (W - boxW) * 0.5f;
            const float boxY = (H - boxH) * 0.5f;

            sf::RectangleShape shadow(sf::Vector2f(boxW, boxH));
            shadow.setPosition(sf::Vector2f(boxX + 6.f, boxY + 8.f));
            shadow.setFillColor(sf::Color(0, 0, 0, 95));
            window.draw(shadow);

            sf::RectangleShape box(sf::Vector2f(boxW, boxH));
            box.setPosition(sf::Vector2f(boxX, boxY));
            box.setFillColor(sf::Color(28, 26, 36, 248));
            box.setOutlineColor(sf::Color(205, 192, 150));
            box.setOutlineThickness(2.25f);
            window.draw(box);

            sf::Text t(fontForChinese(), sf::String(L"灵液槽已满：选择要丢弃的灵液"), 24);
            t.setFillColor(sf::Color(235, 230, 220));
            const sf::FloatRect tb2 = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(tb2.position.x + tb2.size.x * 0.5f, tb2.position.y + tb2.size.y * 0.5f));
            t.setPosition(sf::Vector2f(boxX + boxW * 0.5f, boxY + 40.f));
            window.draw(t);

            // 当前灵液槽
            for (size_t i = 0; i < reward_potion_replace_slot_rects_.size()
                 && i < lastSnapshot_->potions.size(); ++i) {
                const sf::FloatRect r = reward_potion_replace_slot_rects_[i];
                const bool hover = r.contains(mousePos_);
                sf::RectangleShape back(sf::Vector2f(r.size.x, r.size.y));
                back.setPosition(r.position);
                back.setFillColor(hover ? sf::Color(62, 58, 78) : sf::Color(40, 38, 52));
                back.setOutlineColor(hover ? sf::Color(220, 205, 155) : sf::Color(150, 142, 128));
                back.setOutlineThickness(hover ? 2.5f : 2.f);
                window.draw(back);

                const std::string pid = lastSnapshot_->potions[i];
                auto pit = potionTextures_.find(pid);
                if (pit != potionTextures_.end()) {
                    sf::Sprite spr(pit->second);
                    spr.setPosition(r.position);
                    float scale = r.size.x / std::max(1.f, static_cast<float>(pit->second.getSize().x));
                    spr.setScale(sf::Vector2f(scale, scale));
                    window.draw(spr);
                } else {
                    sf::RectangleShape icon(sf::Vector2f(r.size.x, r.size.y));
                    icon.setPosition(r.position);
                    icon.setFillColor(sf::Color(80, 100, 140));
                    window.draw(icon);
                }
            }

            // 取消
            {
                const bool hv = reward_potion_replace_cancel_rect_.contains(mousePos_);
                sf::RectangleShape b(reward_potion_replace_cancel_rect_.size);
                b.setPosition(reward_potion_replace_cancel_rect_.position);
                b.setFillColor(hv ? sf::Color(92, 86, 102) : sf::Color(72, 68, 82));
                b.setOutlineColor(hv ? sf::Color(220, 205, 155) : sf::Color(150, 142, 128));
                b.setOutlineThickness(hv ? 2.5f : 2.f);
                window.draw(b);
                sf::Text lbl(fontForChinese(), sf::String(L"取消"), 22);
                lbl.setFillColor(sf::Color(235, 230, 220));
                const sf::FloatRect lb = lbl.getLocalBounds();
                lbl.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
                lbl.setPosition(sf::Vector2f(
                    reward_potion_replace_cancel_rect_.position.x + reward_potion_replace_cancel_rect_.size.x * 0.5f,
                    reward_potion_replace_cancel_rect_.position.y + reward_potion_replace_cancel_rect_.size.y * 0.5f));
                window.draw(lbl);
            }
        }
    }

    void BattleUI::drawDetailedCardAt(sf::RenderWindow& window, const std::string& card_id, float cardX, float cardY, float w, float h,
                                      const sf::Color& outlineColor, float outlineThickness,
                                      const sf::RenderStates& states,
                                      const BattleStateSnapshot* handSnap,
                                      const CardInstance* handInst,
                                      std::vector<CardGlossaryEntry>* card_glossary_out) {
        auto base_art_key = [](const std::string& id) -> std::string {
            // 目标：让 strike/strike+/strike_green/strike_green+ 都映射到 "strike"
            std::string k = id;
            if (!k.empty() && k.back() == '+') k.pop_back();
            const std::string suffixes[] = {"_green", "_blue", "_red", "_purple"};
            for (const auto& sfx : suffixes) {
                if (k.size() >= sfx.size() && k.compare(k.size() - sfx.size(), sfx.size(), sfx) == 0) {
                    k.erase(k.size() - sfx.size());
                    break;
                }
            }
            return k;
        };

        const CardData* cd = get_card_by_id(card_id);
        const bool isUpgradedCardId = !card_id.empty() && card_id.back() == '+';
        const CardData* baseCdForUpgrade = nullptr;
        if (isUpgradedCardId && card_id.size() > 1u)
            baseCdForUpgrade = get_card_by_id(card_id.substr(0, card_id.size() - 1u));
        // 升级版：只在“静态费用低于原版”时把费用数字染绿；牌名不变色
        constexpr sf::Color kUpgradeFeeDownGreen(200, 255, 225);
        const bool upgradedStaticFeeLower =
            isUpgradedCardId && cd && baseCdForUpgrade && baseCdForUpgrade->cost >= 0 && cd->cost >= 0 &&
            cd->cost < baseCdForUpgrade->cost;
        char buf[32];
        std::vector<sf::Vector2f> rr;

        // 以预览牌高度 410 为参考，默认牌与预览同一套比例，仅整体缩放
        constexpr float kCardLayoutRefH = 410.f;
        const float s = h / kCardLayoutRefH;

        const bool greenCharacterCard = cd && cd->color == CardColor::Green;
        const bool colorlessCard      = cd && cd->color == CardColor::Colorless;
        const bool curseCard          = cd && cd->color == CardColor::Curse;
        const float thickOutline = std::max(1.f, outlineThickness * s);
        const bool cardOutlineEmphasis = thickOutline > 9.5f;
        sf::Color outerOutlineUse = outlineColor;
        if (greenCharacterCard)
            outerOutlineUse = cardOutlineEmphasis ? sf::Color(140, 250, 175) : sf::Color(72, 175, 108);
        else if (colorlessCard)
            outerOutlineUse = cardOutlineEmphasis ? sf::Color(240, 240, 242) : sf::Color(170, 170, 174);
        else if (curseCard)
            outerOutlineUse = cardOutlineEmphasis ? sf::Color(185, 165, 205) : sf::Color(110, 100, 125);

        const float outerR = 11.f * s;
        const float frameInset = 7.f * s;
        const float innerR = 6.f * s;

        // 投影（略偏移的圆角矩形）
        build_round_rect_poly(rr, cardX + 4.f * s, cardY + 5.f * s, w - 2.f * s, h - 2.f * s, std::max(4.f, outerR - 1.f * s), 3);
        draw_convex_poly(window, rr, sf::Color(0, 0, 0, 72), sf::Color::Transparent, 0.f, states);
        // 外框：红/绿/无色/诅咒按卡色分皮肤
        build_round_rect_poly(rr, cardX, cardY, w, h, outerR, 3);
        draw_convex_poly(window, rr,
                         curseCard ? sf::Color(46, 44, 52)
                                  : colorlessCard ? sf::Color(128, 128, 132)
                                  : greenCharacterCard ? sf::Color(42, 138, 78)
                                                      : sf::Color(128, 72, 54),
                         outerOutlineUse, thickOutline, states);

        const float ix = cardX + frameInset;
        const float iy = cardY + frameInset;
        const float iw = w - frameInset * 2.f;
        const float ih = h - frameInset * 2.f;
        build_round_rect_poly(rr, ix, iy, iw, ih, innerR, 3);
        draw_convex_poly(window, rr,
                         curseCard ? sf::Color(22, 20, 28)
                                  : colorlessCard ? sf::Color(56, 56, 58)
                                  : greenCharacterCard ? sf::Color(34, 46, 38)
                                                      : sf::Color(46, 42, 40),
                         curseCard ? sf::Color(10, 10, 14)
                                  : colorlessCard ? sf::Color(28, 28, 30)
                                  : greenCharacterCard ? sf::Color(22, 32, 26)
                                                      : sf::Color(28, 24, 22),
                         std::max(0.5f, 1.f * s), states);

        const float padIn = 6.f * s;
        const float ribbonX = ix + padIn;
        const float ribbonW = iw - padIn * 2.f;
        const float titleY = iy + 9.f * s;
        const float titleH = 30.f * s;
        const float ribTop = titleY + 2.f * s;
        const float ribBot = titleY + titleH;
        constexpr int kRibbonArchSeg = 8;
        sf::ConvexShape ribbon;
        ribbon.setPointCount(static_cast<std::size_t>(2 + kRibbonArchSeg + 1));
        ribbon.setPoint(0, sf::Vector2f(ribbonX, ribBot - 1.f * s));
        ribbon.setPoint(1, sf::Vector2f(ribbonX + ribbonW, ribBot - 1.f * s));
        for (int i = 0; i <= kRibbonArchSeg; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kRibbonArchSeg);
            const float omt = 1.f - t;
            const float px = omt * omt * (ribbonX + ribbonW) + 2.f * omt * t * (ribbonX + ribbonW * 0.5f) + t * t * ribbonX;
            const float py = omt * omt * (ribBot - 7.f * s) + 2.f * omt * t * ribTop + t * t * (ribBot - 7.f * s);
            ribbon.setPoint(static_cast<std::size_t>(2 + i), sf::Vector2f(px, py));
        }
        const Rarity cardRarity = cd ? cd->rarity : Rarity::Common;
        sf::Color ribFill;
        sf::Color ribOutline;
        float ribOutlineTh = 2.f * s;
        sf::Color ribGloss(255, 255, 255, 42);
        if (curseCard) {
            switch (cardRarity) {
            case Rarity::Common:
                ribFill = sf::Color(110, 108, 118);
                ribOutline = sf::Color(52, 50, 60);
                ribGloss = sf::Color(255, 255, 255, 38);
                break;
            case Rarity::Uncommon:
                ribFill = sf::Color(72, 118, 190);
                ribOutline = sf::Color(18, 62, 148);
                ribGloss = sf::Color(205, 235, 255, 52);
                ribOutlineTh = 2.5f * s;
                break;
            case Rarity::Rare:
            case Rarity::Special:
                ribFill = sf::Color(224, 182, 72);
                ribOutline = sf::Color(118, 78, 28);
                ribGloss = sf::Color(255, 248, 200, 72);
                ribOutlineTh = 3.f * s;
                break;
            }
        } else if (colorlessCard) {
            switch (cardRarity) {
            case Rarity::Common:
                ribFill = sf::Color(190, 190, 194);
                ribOutline = sf::Color(92, 92, 98);
                ribGloss = sf::Color(255, 255, 255, 46);
                break;
            case Rarity::Uncommon:
                ribFill = sf::Color(40, 108, 215);
                ribOutline = sf::Color(14, 52, 142);
                ribGloss = sf::Color(185, 225, 255, 62);
                ribOutlineTh = 2.5f * s;
                break;
            case Rarity::Rare:
            case Rarity::Special:
                ribFill = sf::Color(224, 182, 72);
                ribOutline = sf::Color(118, 78, 28);
                ribGloss = sf::Color(255, 248, 200, 72);
                ribOutlineTh = 3.f * s;
                break;
            }
        } else if (greenCharacterCard) {
            switch (cardRarity) {
            case Rarity::Common:
                ribFill = sf::Color(158, 162, 166);
                ribOutline = sf::Color(88, 94, 90);
                ribGloss = sf::Color(255, 255, 255, 50);
                break;
            case Rarity::Uncommon:
                ribFill = sf::Color(48, 118, 210);
                ribOutline = sf::Color(18, 62, 148);
                ribGloss = sf::Color(210, 235, 255, 62);
                ribOutlineTh = 2.5f * s;
                break;
            case Rarity::Rare:
            case Rarity::Special:
                ribFill = sf::Color(224, 182, 72);
                ribOutline = sf::Color(118, 78, 28);
                ribGloss = sf::Color(255, 248, 200, 72);
                ribOutlineTh = 3.f * s;
                break;
            }
        } else {
            switch (cardRarity) {
            case Rarity::Common:
                ribFill = sf::Color(122, 116, 112);
                ribOutline = sf::Color(52, 48, 46);
                ribGloss = sf::Color(255, 255, 255, 45);
                break;
            case Rarity::Uncommon:
                ribFill = sf::Color(40, 108, 215);
                ribOutline = sf::Color(14, 52, 142);
                ribGloss = sf::Color(185, 225, 255, 62);
                ribOutlineTh = 2.5f * s;
                break;
            case Rarity::Rare:
            case Rarity::Special:
                ribFill = sf::Color(224, 182, 72);
                ribOutline = sf::Color(118, 78, 28);
                ribGloss = sf::Color(255, 248, 200, 72);
                ribOutlineTh = 3.f * s;
                break;
            }
        }
        ribbon.setFillColor(ribFill);
        ribbon.setOutlineColor(ribOutline);
        ribbon.setOutlineThickness(ribOutlineTh);
        window.draw(ribbon, states);
        {
            const sf::Vector2f ribC(ribbonX + ribbonW * 0.5f, ribTop + (ribBot - ribTop) * 0.52f);
            sf::ConvexShape gloss;
            const std::size_t npt = ribbon.getPointCount();
            gloss.setPointCount(npt);
            for (std::size_t i = 0; i < npt; ++i) {
                const sf::Vector2f p = ribbon.getPoint(i);
                gloss.setPoint(i, ribC + (p - ribC) * 0.84f);
            }
            gloss.setFillColor(ribGloss);
            gloss.setOutlineColor(sf::Color::Transparent);
            gloss.setOutlineThickness(0.f);
            window.draw(gloss, states);
        }

        const float ax = ix + padIn;
        const float aw = iw - padIn * 2.f;
        const float artTop = ribBot + 5.f * s;
        const CardType artKind = (cd && cd->cardType == CardType::Attack) ? CardType::Attack
            : (cd && cd->cardType == CardType::Power) ? CardType::Power
            : CardType::Skill;
        float artH = h * 0.42f;
        if (artKind == CardType::Skill)
            artH = std::max(h * 0.30f, artH - 15.f * s);
        else if (artKind == CardType::Power)
            artH += 10.f * s;  // 能力牌立绘区略长于攻击牌，底弧位置适中
        sf::ConvexShape artPanel;
        if (artKind == CardType::Skill) {
            artPanel.setPointCount(4);
            artPanel.setPoint(0, sf::Vector2f(ax, artTop));
            artPanel.setPoint(1, sf::Vector2f(ax + aw, artTop));
            artPanel.setPoint(2, sf::Vector2f(ax + aw, artTop + artH));
            artPanel.setPoint(3, sf::Vector2f(ax, artTop + artH));
        } else if (artKind == CardType::Attack) {
            const float shoulder = artH * 0.22f;
            const float tipY = artTop + artH;
            const float shoulderY = tipY - shoulder;
            artPanel.setPointCount(5);
            artPanel.setPoint(0, sf::Vector2f(ax, artTop));
            artPanel.setPoint(1, sf::Vector2f(ax + aw, artTop));
            artPanel.setPoint(2, sf::Vector2f(ax + aw, shoulderY));
            artPanel.setPoint(3, sf::Vector2f(ax + aw * 0.5f, tipY));
            artPanel.setPoint(4, sf::Vector2f(ax, shoulderY));
        } else {
            constexpr int kArcSeg = 22;
            // 能力牌：底边弧线（arcInset 控制弧垂，越大底越鼓）
            const float arcInset = std::max(20.f * s, artH * 0.22f);
            // 弧线与左右竖边相接处略下移；数值越小弧线整体越靠上
            const float arcStartLower = 6.f * s;
            const float yMid = artTop + artH;
            const float ySide = std::min(yMid - 2.f * s, artTop + artH - arcInset + arcStartLower);
            artPanel.setPointCount(static_cast<std::size_t>(2 + kArcSeg + 1));
            artPanel.setPoint(0, sf::Vector2f(ax, artTop));
            artPanel.setPoint(1, sf::Vector2f(ax + aw, artTop));
            for (int i = 0; i <= kArcSeg; ++i) {
                const float t = static_cast<float>(kArcSeg - i) / static_cast<float>(kArcSeg);
                const float sn = std::sin(kCardFacePi * t);
                artPanel.setPoint(static_cast<std::size_t>(2 + i),
                                  sf::Vector2f(ax + aw * t, ySide + (yMid - ySide) * sn));
            }
        }
        artPanel.setFillColor(curseCard ? sf::Color(58, 22, 62)
                                        : colorlessCard ? sf::Color(92, 92, 96)
                                        : greenCharacterCard ? sf::Color(52, 78, 62)
                                                            : sf::Color(88, 32, 34));
        artPanel.setOutlineColor(curseCard ? sf::Color(200, 180, 220)
                                           : colorlessCard ? sf::Color(210, 210, 214)
                                           : greenCharacterCard ? sf::Color(175, 188, 178)
                                                               : sf::Color(198, 202, 210));
        artPanel.setOutlineThickness(std::max(2.f, 4.5f * s));
        window.draw(artPanel, states);

        // 立绘：根据card_id从assets/cards目录加载图片
        {
            const std::string key = base_art_key(card_id);
            std::string loadKey = key;
            std::string path;
            bool loaded = false;

            // 尝试不同的命名格式
            std::vector<std::string> nameFormats;
            
            // 1. 保持原始格式
            nameFormats.push_back(key);
            
            // 2. 全小写
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
            if (lowerKey != key) nameFormats.push_back(lowerKey);
            
            // 3. 首字母大写，其余小写
            std::string capitalizedKey = lowerKey;
            if (!capitalizedKey.empty()) {
                capitalizedKey[0] = std::toupper(capitalizedKey[0]);
                if (capitalizedKey != key && capitalizedKey != lowerKey) nameFormats.push_back(capitalizedKey);
            }

            // 尝试每种命名格式
            for (const auto& format : nameFormats) {
                path = "assets/cards/" + format + ".png";
                auto it = cardArtTextures_.find(format);
                if (it == cardArtTextures_.end()) {
                    sf::Texture tex;
                    if (tex.loadFromFile(path)) {
                        it = cardArtTextures_.emplace(format, std::move(tex)).first;
                        loaded = true;
                        loadKey = format;
                        break;
                    }
                } else {
                    loaded = true;
                    loadKey = format;
                    break;
                }
            }

            if (loaded) {
                auto it = cardArtTextures_.find(loadKey);
                if (it != cardArtTextures_.end()) {
                    const sf::Vector2u tsz = it->second.getSize();
                    if (tsz.x > 0u && tsz.y > 0u) {
                        // 用与 artPanel 相同的凸多边形承载纹理：天然裁剪，图片不会超出形状
                        sf::ConvexShape artImg = artPanel;
                        artImg.setFillColor(sf::Color::White);
                        artImg.setOutlineThickness(0.f);
                        artImg.setTexture(&it->second);

                        // textureRect 采用 cover（填满立绘区，必要时裁切纹理边缘）
                        const sf::FloatRect ab = artImg.getLocalBounds();
                        const float panelAspect = (ab.size.y > 0.f) ? (ab.size.x / ab.size.y) : 1.f;
                        const float texAspect = static_cast<float>(tsz.x) / static_cast<float>(tsz.y);

                        int rx = 0, ry = 0;
                        int rw = static_cast<int>(tsz.x);
                        int rh = static_cast<int>(tsz.y);
                        if (texAspect > panelAspect) {
                            // 纹理更“宽” -> 裁左右
                            rw = static_cast<int>(static_cast<float>(tsz.y) * panelAspect);
                            rx = (static_cast<int>(tsz.x) - rw) / 2;
                        } else if (texAspect < panelAspect) {
                            // 纹理更“高” -> 裁上下
                            rh = static_cast<int>(static_cast<float>(tsz.x) / panelAspect);
                            ry = (static_cast<int>(tsz.y) - rh) / 2;
                        }
                        artImg.setTextureRect(sf::IntRect({rx, ry}, {rw, rh}));
                        window.draw(artImg, states);
                    }
                }
            }
        }

        sf::String cardName;
        if (cd && !cd->name.empty())
            cardName = sf::String::fromUtf8(cd->name.begin(), cd->name.end());
        else if (card_id.empty())
            cardName = sf::String(L"?");
        else
            cardName = sf::String(card_id);
        const unsigned namePt = static_cast<unsigned>(std::max(15.f, std::round(31.f * s)));
        sf::Text nameText(fontForChinese(), cardName, namePt);
        const sf::FloatRect nb = nameText.getLocalBounds();
        nameText.setOrigin(sf::Vector2f(nb.position.x + nb.size.x * 0.5f, nb.position.y + nb.size.y * 0.5f));
        const sf::Vector2f namePos(ribbonX + ribbonW * 0.5f, (ribTop + ribBot) * 0.5f + 2.f * s);
        nameText.setPosition(namePos);
        nameText.setFillColor(sf::Color::White);
        window.draw(nameText, states);

        sf::String typeStr = sf::String(L"?");
        if (cd) {
            switch (cd->cardType) {
            case CardType::Attack: typeStr = sf::String(L"武学"); break;
            case CardType::Skill:  typeStr = sf::String(L"秘籍"); break;
            case CardType::Power:  typeStr = sf::String(L"能力"); break;
            case CardType::Status: typeStr = sf::String(L"状态"); break;
            case CardType::Curse:  typeStr = sf::String(L"诅咒"); break;
            }
        }
        const unsigned typePt = static_cast<unsigned>(std::max(10.f, std::round(17.f * s)));
        sf::Text typeText(fontForChinese(), typeStr, typePt);
        typeText.setFillColor(curseCard ? sf::Color(30, 28, 36)
                                        : colorlessCard ? sf::Color(48, 48, 52)
                                        : greenCharacterCard ? sf::Color(48, 52, 50)
                                                            : sf::Color(198, 194, 188));
        const float tagCx = ax + aw * 0.5f;
        // 类型条中心压在立绘区底边（平底 / 三角尖端 / 弧底最低点均为 artTop + artH）
        const float artBottomY = artTop + artH;
        const sf::FloatRect tbPre = typeText.getLocalBounds();
        const float pillW = std::max(48.f * s, tbPre.size.x + 20.f * s);
        const float pillH = 22.f * s;
        const float tagCy = artBottomY;
        sf::RectangleShape typePill(sf::Vector2f(pillW, pillH));
        typePill.setOrigin(sf::Vector2f(pillW * 0.5f, pillH * 0.5f));
        typePill.setPosition(sf::Vector2f(tagCx, tagCy));
        typePill.setFillColor(curseCard ? sf::Color(150, 150, 154)
                                        : colorlessCard ? sf::Color(175, 175, 178)
                                        : greenCharacterCard ? sf::Color(118, 124, 118)
                                                            : sf::Color(94, 90, 86));
        typePill.setOutlineColor(curseCard ? sf::Color(90, 90, 98)
                                           : colorlessCard ? sf::Color(110, 110, 118)
                                           : greenCharacterCard ? sf::Color(78, 84, 80)
                                                               : sf::Color(62, 58, 54));
        typePill.setOutlineThickness(std::max(0.5f, 1.f * s));
        window.draw(typePill, states);
        const sf::FloatRect tb = typeText.getLocalBounds();
        typeText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, tb.position.y + tb.size.y * 0.5f));
        typeText.setPosition(sf::Vector2f(tagCx, tagCy));
        window.draw(typeText, states);

        const float descBoxTop = tagCy + pillH * 0.5f + 7.f * s;
        const float descBoxH = (iy + ih) - descBoxTop - 8.f * s;
        if (descBoxH > 18.f * s) {
            const float dPad = 5.f * s;
            build_round_rect_poly(rr, ix + dPad, descBoxTop, iw - dPad * 2.f, descBoxH, std::max(3.f, 5.f * s), 2);
            draw_convex_poly(window, rr,
                             curseCard ? sf::Color(16, 14, 18)
                                      : colorlessCard ? sf::Color(26, 26, 28)
                                      : greenCharacterCard ? sf::Color(24, 34, 28)
                                                          : sf::Color(22, 20, 24),
                             curseCard ? sf::Color(56, 52, 62)
                                      : colorlessCard ? sf::Color(60, 60, 66)
                                      : greenCharacterCard ? sf::Color(48, 62, 52)
                                                          : sf::Color(40, 36, 38),
                             std::max(0.5f, 1.f * s), states);
        }
        sf::String descStr;
        if (cd && !cd->description.empty())
            descStr = sf::String::fromUtf8(cd->description.begin(), cd->description.end());
        const float descX = ix + 11.f * s;
        const float descY = descBoxTop + 8.f * s;
        const float descMaxW = iw - 22.f * s;
        const float descMaxH = std::max(8.f, (iy + ih) - descY - 10.f * s);
        const unsigned descPt = static_cast<unsigned>(std::max(11.f, std::round(23.f * s)));
        const sf::Color descPlain = curseCard ? sf::Color(240, 240, 245)
            : colorlessCard ? sf::Color(236, 236, 236)
            : greenCharacterCard ? sf::Color(232, 238, 232)
                                 : sf::Color(218, 212, 204);
        constexpr sf::Color kDescKeywordGold(235, 205, 115);
        if (card_glossary_out) {
            card_glossary_out->clear();
            draw_wrapped_card_description_with_keywords(window, fontForChinese(), descStr, descPt,
                                                        sf::Vector2f(descX, descY), descMaxW, descMaxH,
                                                        descPlain, kDescKeywordGold, states, card_glossary_out);
        } else {
            draw_wrapped_text(window, fontForChinese(), descStr, descPt,
                              sf::Vector2f(descX, descY), descMaxW, descMaxH, descPlain, states);
        }

        const bool handCostPath = (handSnap != nullptr && handInst != nullptr);
        const bool showCostCircle = cd && cd->cost != -2;
        int displayCost = 0;
        if (showCostCircle) {
            displayCost = handCostPath ? effective_hand_card_energy_cost(cd, *handInst, handSnap) : cd->cost;
            // 费用球保持默认红/金；数字仅当「升级版且静态费用低于原版」时为亮绿
            const float costCx = cardX + 22.f * s;
            const float costCy = cardY + 22.f * s;
            const float gemR = 18.5f * s;
            const float ringPad = 5.f * s;
            if (handCostPath) {
                sf::CircleShape costRing(gemR + ringPad);
                costRing.setPosition(sf::Vector2f(costCx - gemR - ringPad, costCy - gemR - ringPad));
                costRing.setFillColor(sf::Color(0, 0, 0, 0));
                costRing.setOutlineColor(curseCard ? sf::Color(210, 195, 225)
                                                  : colorlessCard ? sf::Color(222, 222, 228)
                                                  : greenCharacterCard ? sf::Color(160, 235, 195)
                                                                      : sf::Color(255, 200, 100));
                costRing.setOutlineThickness(std::max(1.f, 3.f * s));
                window.draw(costRing, states);
            }
            sf::ConvexShape gem;
            gem.setPointCount(8);
            for (int gi = 0; gi < 8; ++gi) {
                const float a = kCardFacePi * 0.125f + static_cast<float>(gi) * (kCardFacePi * 0.25f);
                gem.setPoint(static_cast<std::size_t>(gi),
                             sf::Vector2f(costCx + gemR * std::cos(a), costCy + gemR * std::sin(a)));
            }
            if (curseCard) {
                gem.setFillColor(sf::Color(120, 120, 126));
                gem.setOutlineColor(sf::Color(235, 235, 242));
            } else if (colorlessCard) {
                gem.setFillColor(sf::Color(160, 160, 166));
                gem.setOutlineColor(sf::Color(245, 245, 250));
            } else if (greenCharacterCard) {
                gem.setFillColor(sf::Color(110, 215, 150));
                gem.setOutlineColor(sf::Color(215, 255, 228));
            } else {
                gem.setFillColor(sf::Color(208, 52, 44));
                gem.setOutlineColor(sf::Color(255, 214, 130));
            }
            gem.setOutlineThickness(std::max(1.f, 2.f * s));
            window.draw(gem, states);
            if (displayCost == -1) {
                std::snprintf(buf, sizeof(buf), "X");
            } else {
                std::snprintf(buf, sizeof(buf), "%d", displayCost);
            }
            const unsigned costPt = static_cast<unsigned>(std::max(11.f, std::round(27.f * s)));
            sf::Text costText(font_, buf, costPt);
            costText.setFillColor(upgradedStaticFeeLower ? kUpgradeFeeDownGreen : sf::Color::White);
            const sf::FloatRect cb = costText.getLocalBounds();
            costText.setOrigin(sf::Vector2f(cb.position.x + cb.size.x * 0.5f, cb.position.y + cb.size.y * 0.5f));
            costText.setPosition(sf::Vector2f(costCx, costCy));
            window.draw(costText, states);
        }
    }

    void BattleUI::drawCardSelectScreen(sf::RenderWindow& window) {
        constexpr float CARD_W = 206.f;
        constexpr float CARD_H = 280.f;
        constexpr float BTN_W = 140.f;
        constexpr float BTN_H = 50.f;
        constexpr float OVERLAY_TOP = RELICS_ROW_Y + RELICS_ROW_H;
        const float W = static_cast<float>(width_);
        const float H = static_cast<float>(height_);
        // 与 drawBottomBar 手牌扇形一致（勿用本函数内 CARD_H=280 的网格牌尺寸）
        constexpr float FAN_CARD_W = 206.f;
        constexpr float FAN_CARD_H = 300.f;
        const sf::Color dimColor(18, 18, 26, static_cast<std::uint8_t>(card_select_use_hand_area_ ? 178 : 165));
        auto drawDimRect = [&](float x, float y, float rw, float rh) {
            if (rw <= 0.f || rh <= 0.f) return;
            sf::RectangleShape r(sf::Vector2f(rw, rh));
            r.setPosition(sf::Vector2f(x, y));
            r.setFillColor(dimColor);
            window.draw(r);
        };
        if (card_select_use_hand_area_ && lastSnapshot_) {
            std::vector<int> vis;
            std::vector<sf::Vector2f> centers;
            std::vector<float> angles;
            compute_card_select_hand_fan_(*lastSnapshot_, card_select_selected_indices_, vis, centers, angles);
            if (!vis.empty()) {
                constexpr float DEG2RAD = 3.14159265f / 180.f;
                constexpr float pad = 14.f;
                const float hw = FAN_CARD_W * 0.5f;
                const float hh = FAN_CARD_H * 0.5f;
                float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
                for (size_t j = 0; j < vis.size(); ++j) {
                    const float cx = centers[j].x;
                    const float cy = centers[j].y;
                    const float rad = angles[j] * DEG2RAD;
                    const float c = std::cos(rad), s = std::sin(rad);
                    const float lx[4] = {-hw, hw, hw, -hw};
                    const float ly[4] = {-hh, -hh, hh, hh};
                    for (int k = 0; k < 4; ++k) {
                        const float wx = cx + lx[k] * c - ly[k] * s;
                        const float wy = cy + lx[k] * s + ly[k] * c;
                        minX = std::min(minX, wx);
                        maxX = std::max(maxX, wx);
                        minY = std::min(minY, wy);
                        maxY = std::max(maxY, wy);
                    }
                }
                minX -= pad;
                maxX += pad;
                minY -= pad;
                maxY += pad;
                minX = std::max(0.f, minX);
                maxX = std::min(W, maxX);
                minY = std::max(OVERLAY_TOP, minY);
                maxY = std::min(H, maxY);
                if (minX < maxX && minY < maxY) {
                    if (minY > OVERLAY_TOP + 0.5f)
                        drawDimRect(0.f, OVERLAY_TOP, W, minY - OVERLAY_TOP);
                    if (minX > 0.5f)
                        drawDimRect(0.f, minY, minX, H - minY);
                    if (maxX < W - 0.5f)
                        drawDimRect(maxX, minY, W - maxX, H - minY);
                    if (maxY < H - 0.5f)
                        drawDimRect(minX, maxY, maxX - minX, H - maxY);
                } else {
                    drawDimRect(0.f, OVERLAY_TOP, W, H - OVERLAY_TOP);
                }
            } else {
                drawDimRect(0.f, OVERLAY_TOP, W, H - OVERLAY_TOP);
            }
        } else {
            drawDimRect(0.f, OVERLAY_TOP, W, H - OVERLAY_TOP);
        }

        sf::Text title(fontForChinese(), sf::String(card_select_title_.empty() ? L"选择一张牌" : card_select_title_), 42);
        title.setFillColor(sf::Color(255, 230, 170));
        const sf::FloatRect tb = title.getLocalBounds();
        title.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, 0.f));
        title.setPosition(sf::Vector2f(width_ * 0.5f, 180.f));
        window.draw(title);

        if (!card_select_use_hand_area_) {
            // 弹窗网格：与奖励/牌组视图一致，绘制完整卡牌（用于消耗堆/弃牌等选牌）
            for (size_t i = 0; i < card_select_rects_.size() && i < card_select_ids_.size(); ++i) {
                const auto& r = card_select_rects_[i];
                const bool selected =
                    std::find(card_select_selected_indices_.begin(), card_select_selected_indices_.end(), static_cast<int>(i)) !=
                    card_select_selected_indices_.end();
                const sf::Color outline =
                    selected ? sf::Color(100, 220, 120)
                             : (r.contains(mousePos_) ? sf::Color(230, 200, 120) : sf::Color(180, 50, 45));
                drawDetailedCardAt(window, card_select_ids_[i], r.position.x, r.position.y, CARD_W, CARD_H, outline, 4.f);
            }
        } else {
            // 与手牌/牌组视图同规格；从手牌扇区插值飞到中央，已选牌不再画在底部扇形里
            constexpr float SEL_W = 206.f;
            constexpr float SEL_H = 300.f;
            constexpr float GAP = 22.f;
            const size_t n = card_select_selected_indices_.size();
            const float totalW = static_cast<float>(n) * SEL_W + (n > 1 ? static_cast<float>(n - 1) * GAP : 0.f);
            const float startX = width_ * 0.5f - totalW * 0.5f;
            const float y = 268.f;
            for (size_t i = 0; i < n; ++i) {
                int idx = card_select_selected_indices_[i];
                if (idx < 0 || static_cast<size_t>(idx) >= card_select_ids_.size()) continue;
                const CardId& cid = card_select_ids_[static_cast<size_t>(idx)];
                const float tcx = startX + static_cast<float>(i) * (SEL_W + GAP) + SEL_W * 0.5f;
                const float tcy = y + SEL_H * 0.5f;
                sf::Vector2f drawCenter(tcx, tcy);
                if (!card_select_pull_anims_.empty()) {
                    for (auto& anim : card_select_pull_anims_) {
                        if (anim.candidateIndex != idx) continue;
                        anim.targetCenter = sf::Vector2f(tcx, tcy);
                        const float t = anim.durationSec > 0.0001f
                            ? std::min(1.f, anim.clock.getElapsedTime().asSeconds() / anim.durationSec)
                            : 1.f;
                        const float te = 1.f - (1.f - t) * (1.f - t);
                        drawCenter.x = anim.startCenter.x * (1.f - te) + anim.targetCenter.x * te;
                        drawCenter.y = anim.startCenter.y * (1.f - te) + anim.targetCenter.y * te;
                        break;
                    }
                }
                drawDetailedCardAt(window, cid, drawCenter.x - SEL_W * 0.5f, drawCenter.y - SEL_H * 0.5f, SEL_W, SEL_H,
                                   sf::Color(100, 220, 120), 4.f);
            }

            std::wstring prog = L"已选 " + std::to_wstring(static_cast<int>(n)) + L" / "
                + std::to_wstring(card_select_required_pick_count_) + L" 张 · 点手牌加入或移回";
            sf::Text progText(fontForChinese(), sf::String(prog), 22);
            progText.setFillColor(sf::Color(255, 245, 200));
            const sf::FloatRect pb = progText.getLocalBounds();
            progText.setOrigin(sf::Vector2f(pb.position.x + pb.size.x * 0.5f, 0.f));
            progText.setPosition(sf::Vector2f(width_ * 0.5f, 228.f));
            window.draw(progText);
        }

        const bool canConfirm = static_cast<int>(card_select_selected_indices_.size()) >= card_select_required_pick_count_;
        sf::RectangleShape confirmBtn(sf::Vector2f(BTN_W, BTN_H));
        confirmBtn.setPosition(sf::Vector2f(card_select_confirm_rect_.position));
        if (canConfirm && card_select_use_hand_area_) {
            const float pulse = 0.5f + 0.5f * std::sin(card_select_confirm_pulse_clock_.getElapsedTime().asSeconds() * 7.f);
            const auto gFill = static_cast<std::uint8_t>(125 + static_cast<int>(55.f * pulse));
            const auto gLine = static_cast<std::uint8_t>(185 + static_cast<int>(50.f * pulse));
            confirmBtn.setFillColor(sf::Color(95, gFill, 75));
            confirmBtn.setOutlineColor(sf::Color(255, gLine, 100));
            confirmBtn.setOutlineThickness(3.f + 3.f * pulse);
        } else {
            confirmBtn.setFillColor(canConfirm ? sf::Color(100, 140, 80) : sf::Color(70, 70, 75));
            confirmBtn.setOutlineColor(canConfirm ? sf::Color(150, 200, 120) : sf::Color(120, 120, 130));
            confirmBtn.setOutlineThickness(2.f);
        }
        window.draw(confirmBtn);
        sf::Text confirmText(fontForChinese(), sf::String(L"确定"), 24);
        confirmText.setFillColor(sf::Color::White);
        const sf::FloatRect cfb = confirmText.getLocalBounds();
        confirmText.setOrigin(sf::Vector2f(cfb.position.x + cfb.size.x * 0.5f, cfb.position.y + cfb.size.y * 0.5f));
        confirmText.setPosition(sf::Vector2f(card_select_confirm_rect_.position.x + BTN_W * 0.5f, card_select_confirm_rect_.position.y + BTN_H * 0.5f));
        window.draw(confirmText);

        // 选牌流程不支持取消按钮
    }

    void BattleUI::updateDeckViewDetailLayout_() {
        constexpr float kAspect = 304.f / 410.f;
        float detailH = std::min(static_cast<float>(height_) * 0.62f, 660.f);
        float detailW = detailH * kAspect;
        const float maxW = static_cast<float>(width_) * 0.90f;
        if (detailW > maxW) {
            detailW = maxW;
            detailH = detailW / kAspect;
        }
        const float cx = static_cast<float>(width_) * 0.5f;
        const float cardCenterY = static_cast<float>(height_) * 0.37f + 78.f;  // 牌组详情大图再略下移
        const float left = cx - detailW * 0.5f;
        const float top = cardCenterY - detailH * 0.5f;
        deck_view_detail_card_rect_ = sf::FloatRect(sf::Vector2f(left, top), sf::Vector2f(detailW, detailH));
        constexpr float btnW = 236.f;
        constexpr float btnH = 52.f;
        constexpr float cardToBtnGap = 44.f;  // 牌底与「查看升级」按钮之间的空隙
        const float btnX = cx - btnW * 0.5f;
        const float btnY = top + detailH + cardToBtnGap;
        deck_view_detail_upgrade_btn_rect_ = sf::FloatRect(sf::Vector2f(btnX, btnY), sf::Vector2f(btnW, btnH));
    }

    // 牌组界面：顶栏与遗物栏不变，中间为牌堆网格（一行最多 5 张），可滚轮滚动；返回按钮左下角距底 200
    void BattleUI::drawDeckView(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        deck_view_glossary_active_ = false;
        // 半透明遮罩：覆盖顶栏下方整个区域，让下层内容变暗
        {
            const float viewTop    = TOP_BAR_BG_H;
            const float viewHeight = static_cast<float>(height_) - viewTop;
            sf::RectangleShape dimBg(sf::Vector2f(static_cast<float>(width_), viewHeight));
            dimBg.setPosition(sf::Vector2f(0.f, viewTop));
            dimBg.setFillColor(sf::Color(10, 10, 16, 210)); // 深灰略透明
            window.draw(dimBg);
        }

        constexpr float DECK_CARD_W = 206.f;           // 牌组界面卡牌宽
        constexpr float DECK_CARD_H = 300.f;          // 牌组界面卡牌高
        constexpr float COL_CENTER_TO_CENTER = 276.f; // 单行内相邻牌中心间距
        constexpr float ROW_CENTER_TO_CENTER = 360.f; // 行与行牌中心间距
        constexpr int COLS = 5;                       // 每行 5 张
        const float contentTop = RELICS_ROW_Y + RELICS_ROW_H + 14.f;  // 内容区顶部
        constexpr float returnBtnBottomMargin = 200.f; // 返回按钮距底
        constexpr float returnBtnH = 50.f;            // 返回按钮高度
        // 可见区域：顶栏下沿到屏幕底部；卡牌仅当 下端超出顶栏上沿 或 上端超出屏幕底部 时不渲染
        const float viewTop = TOP_BAR_BG_H;           // 视口顶部（顶栏下沿）
        const float viewBottom = static_cast<float>(height_);  // 视口底部
        const size_t cardCount = deck_view_cards_.size();
        constexpr float FIRST_ROW_CENTER_Y = 430.f;  // 第一行牌中心距屏幕顶部 430
        const float firstRowTop = FIRST_ROW_CENTER_Y - DECK_CARD_H * 0.5f;  // 第一行牌顶 280
        const float padTop = firstRowTop - contentTop;  // 内容区顶部留白
        const float totalContentW = (COLS - 1) * COL_CENTER_TO_CENTER + DECK_CARD_W;  // 总内容宽度
        const float contentLeft = (static_cast<float>(width_) - totalContentW) * 0.5f;  // 水平居中

        // 牌组网格悬停插值（仅在非详情层时启用）
        int hoverIdx = -1;
        if (!deck_view_detail_active_) {
            for (size_t i = 0; i < cardCount; ++i) {
                const int row = static_cast<int>(i) / COLS;
                const int col = static_cast<int>(i) % COLS;
                const float cardX = contentLeft + col * COL_CENTER_TO_CENTER;
                const float cardY = contentTop + padTop + row * ROW_CENTER_TO_CENTER - deck_view_scroll_y_;
                if (cardY + DECK_CARD_H < viewTop || cardY > viewBottom) continue;
                if (mousePos_.x >= cardX && mousePos_.x <= cardX + DECK_CARD_W &&
                    mousePos_.y >= cardY && mousePos_.y <= cardY + DECK_CARD_H) {
                    hoverIdx = static_cast<int>(i);
                    break;
                }
            }
        }
        float dtHover = deck_view_hover_clock_.restart().asSeconds();
        if (dtHover > 0.08f) dtHover = 0.08f;
        if (dtHover < 0.f) dtHover = 0.f;
        if (hoverIdx != deck_view_hover_index_) {
            deck_view_hover_index_ = hoverIdx;
            deck_view_hover_blend_ = 0.f;
        }
        deck_view_hover_blend_ = ui_hover_lerp(deck_view_hover_blend_, (hoverIdx >= 0) ? 1.f : 0.f, dtHover);

        for (size_t i = 0; i < cardCount; ++i) {
            const int row = static_cast<int>(i) / COLS;
            const int col = static_cast<int>(i) % COLS;
            const float cardX = contentLeft + col * COL_CENTER_TO_CENTER;
            const float cardY = contentTop + padTop + row * ROW_CENTER_TO_CENTER - deck_view_scroll_y_;  // 减去滚动偏移
            if (cardY + DECK_CARD_H < viewTop || cardY > viewBottom) continue;  // 视口外不绘制（裁剪优化）
            const CardInstance& inst = deck_view_cards_[i];
            float w = DECK_CARD_W, h = DECK_CARD_H;
            float x = cardX, y = cardY;
            float outlineTh = 8.f;
            sf::Color outline = sf::Color(180, 50, 45);
            if (!deck_view_detail_active_ && deck_view_hover_index_ == static_cast<int>(i)) {
                const float pb = deck_view_hover_blend_;
                const float scale = 1.f + 0.16f * pb;          // 轻微放大
                w = DECK_CARD_W * scale;
                h = DECK_CARD_H * scale;
                x = cardX - (w - DECK_CARD_W) * 0.5f;
                y = cardY - (h - DECK_CARD_H) * 0.5f - 18.f * pb; // 略上浮
                outlineTh = 8.f + 3.f * pb;
                // 悬停时仅做插值放大/加粗，描边颜色保持与原来一致（避免出现金边）
                outline = sf::Color(180, 50, 45);
            }
            std::vector<CardGlossaryEntry>* gv = nullptr;
            if (!deck_view_detail_active_ && deck_view_hover_index_ == static_cast<int>(i)) {
                gv = &deck_view_glossary_entries_;
                deck_view_glossary_card_screen_ = sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(w, h));
                deck_view_glossary_active_ = true;
            }
            drawDetailedCardAt(window, inst.id, x, y, w, h, outline, outlineTh, sf::RenderStates::Default,
                               nullptr, nullptr, gv);
        }

        const float returnW = 180.f, returnH = 50.f;
        const float returnX = 28.f;                  // 左下角
        const float returnY = height_ - returnBtnBottomMargin - returnH;  // 距屏幕底部 200
        deckViewReturnButton_ = sf::FloatRect(sf::Vector2f(returnX, returnY), sf::Vector2f(returnW, returnH));
        sf::RectangleShape returnBtn(sf::Vector2f(returnW, returnH));
        returnBtn.setPosition(sf::Vector2f(returnX, returnY));
        returnBtn.setFillColor(sf::Color(120, 50, 50));
        returnBtn.setOutlineColor(sf::Color(200, 90, 90));
        returnBtn.setOutlineThickness(2.f);
        window.draw(returnBtn);
        sf::Text returnLabel(fontForChinese(), sf::String(L"返回"), 24);
        returnLabel.setFillColor(sf::Color::White);
        const sf::FloatRect rlb = returnLabel.getLocalBounds();
        returnLabel.setOrigin(sf::Vector2f(rlb.position.x + rlb.size.x * 0.5f, rlb.position.y + rlb.size.y * 0.5f));
        returnLabel.setPosition(sf::Vector2f(returnX + returnW * 0.5f, returnY + returnH * 0.5f));
        window.draw(returnLabel);

        if (deck_view_detail_active_) {
            updateDeckViewDetailLayout_();
            const float ovTop = TOP_BAR_BG_H;
            sf::RectangleShape dimDetail(sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_) - ovTop));
            dimDetail.setPosition(sf::Vector2f(0.f, ovTop));
            dimDetail.setFillColor(sf::Color(6, 6, 10, 215));
            window.draw(dimDetail);
            const std::string disp =
                deck_view_detail_resolve_display_id(deck_view_detail_inst_, deck_view_detail_show_upgraded_);
            const sf::FloatRect& cr = deck_view_detail_card_rect_;
            drawDetailedCardAt(window, disp, cr.position.x, cr.position.y, cr.size.x, cr.size.y,
                             sf::Color(205, 75, 58), 11.f, sf::RenderStates::Default, nullptr, nullptr,
                             &deck_view_glossary_entries_);
            deck_view_glossary_card_screen_ = cr;
            deck_view_glossary_active_ = true;
            const sf::FloatRect& br = deck_view_detail_upgrade_btn_rect_;
            const bool alreadyUp =
                !deck_view_detail_inst_.id.empty() && deck_view_detail_inst_.id.back() == '+';
            const bool baseUpgradable = deck_view_card_has_upgrade_definition(deck_view_detail_inst_.id);
            sf::RectangleShape upBtn(sf::Vector2f(br.size.x, br.size.y));
            upBtn.setPosition(br.position);
            if (alreadyUp || !baseUpgradable) {
                upBtn.setFillColor(sf::Color(68, 66, 74));
                upBtn.setOutlineColor(sf::Color(105, 103, 112));
            } else {
                upBtn.setFillColor(sf::Color(88, 128, 78));
                upBtn.setOutlineColor(sf::Color(138, 188, 125));
            }
            upBtn.setOutlineThickness(2.f);
            window.draw(upBtn);
            sf::String btnStr = alreadyUp                      ? sf::String(L"已是升级版")
                                : (!baseUpgradable)          ? sf::String(L"不可升级")
                                : deck_view_detail_show_upgraded_ ? sf::String(L"查看原版")
                                                                 : sf::String(L"查看升级");
            sf::Text upLabel(fontForChinese(), btnStr, 22);
            upLabel.setFillColor(sf::Color(235, 232, 228));
            const sf::FloatRect ulb = upLabel.getLocalBounds();
            upLabel.setOrigin(sf::Vector2f(ulb.position.x + ulb.size.x * 0.5f, ulb.position.y + ulb.size.y * 0.5f));
            upLabel.setPosition(sf::Vector2f(br.position.x + br.size.x * 0.5f, br.position.y + br.size.y * 0.5f));
            window.draw(upLabel);
        }
        if (deck_view_glossary_active_ && !deck_view_glossary_entries_.empty())
            draw_card_glossary_beside_preview_(window, deck_view_glossary_card_screen_, deck_view_glossary_entries_);
    }

    void BattleUI::drawDeckViewStandalone_(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        deck_view_glossary_active_ = false;
        // 与 drawDeckView 基本一致，但不依赖顶栏/遗物栏（用于开始界面等）。
        // 注意：外部界面（如卡牌总览）通常已经自行 clear 了暗底色，这里不再叠加遮罩，避免压暗顶部按钮。

        constexpr float DECK_CARD_W = 206.f;
        constexpr float DECK_CARD_H = 300.f;
        constexpr float COL_CENTER_TO_CENTER = 276.f;
        constexpr float ROW_CENTER_TO_CENTER = 360.f;
        constexpr int COLS = 5;
        float contentTop = 0.f;
        float firstRowCenterY = 0.f;
        float viewTop = 0.f;
        deck_view_grid_layout_(contentTop, firstRowCenterY, viewTop);
        constexpr float returnBtnBottomMargin = 120.f;
        const float viewBottom = static_cast<float>(height_);
        const size_t cardCount = deck_view_cards_.size();
        const float firstRowTop = firstRowCenterY - DECK_CARD_H * 0.5f;
        const float padTop = firstRowTop - contentTop;
        const float totalContentW = (COLS - 1) * COL_CENTER_TO_CENTER + DECK_CARD_W;
        const float contentLeft = (static_cast<float>(width_) - totalContentW) * 0.5f;

        int hoverIdx = -1;
        if (!deck_view_detail_active_) {
            for (size_t i = 0; i < cardCount; ++i) {
                const int row = static_cast<int>(i) / COLS;
                const int col = static_cast<int>(i) % COLS;
                const float cardX = contentLeft + col * COL_CENTER_TO_CENTER;
                const float cardY = contentTop + padTop + row * ROW_CENTER_TO_CENTER - deck_view_scroll_y_;
                if (cardY + DECK_CARD_H < viewTop || cardY > viewBottom) continue;
                if (mousePos_.x >= cardX && mousePos_.x <= cardX + DECK_CARD_W &&
                    mousePos_.y >= cardY && mousePos_.y <= cardY + DECK_CARD_H) {
                    hoverIdx = static_cast<int>(i);
                    break;
                }
            }
        }
        float dtHover = deck_view_hover_clock_.restart().asSeconds();
        if (dtHover > 0.08f) dtHover = 0.08f;
        if (dtHover < 0.f) dtHover = 0.f;
        if (hoverIdx != deck_view_hover_index_) {
            deck_view_hover_index_ = hoverIdx;
            deck_view_hover_blend_ = 0.f;
        }
        deck_view_hover_blend_ = ui_hover_lerp(deck_view_hover_blend_, (hoverIdx >= 0) ? 1.f : 0.f, dtHover);

        for (size_t i = 0; i < cardCount; ++i) {
            const int row = static_cast<int>(i) / COLS;
            const int col = static_cast<int>(i) % COLS;
            const float cardX = contentLeft + col * COL_CENTER_TO_CENTER;
            const float cardY = contentTop + padTop + row * ROW_CENTER_TO_CENTER - deck_view_scroll_y_;
            if (cardY + DECK_CARD_H < viewTop || cardY > viewBottom) continue;
            const CardInstance& inst = deck_view_cards_[i];
            float w = DECK_CARD_W, h = DECK_CARD_H;
            float x = cardX, y = cardY;
            float outlineTh = 8.f;
            sf::Color outline = sf::Color(180, 50, 45);
            if (!deck_view_detail_active_ && deck_view_hover_index_ == static_cast<int>(i)) {
                const float pb = deck_view_hover_blend_;
                const float scale = 1.f + 0.16f * pb;
                w = DECK_CARD_W * scale;
                h = DECK_CARD_H * scale;
                x = cardX - (w - DECK_CARD_W) * 0.5f;
                y = cardY - (h - DECK_CARD_H) * 0.5f - 18.f * pb;
                outlineTh = 8.f + 3.f * pb;
            }
            std::vector<CardGlossaryEntry>* gv = nullptr;
            if (!deck_view_detail_active_ && deck_view_hover_index_ == static_cast<int>(i)) {
                gv = &deck_view_glossary_entries_;
                deck_view_glossary_card_screen_ = sf::FloatRect(sf::Vector2f(x, y), sf::Vector2f(w, h));
                deck_view_glossary_active_ = true;
            }
            drawDetailedCardAt(window, inst.id, x, y, w, h, outline, outlineTh, sf::RenderStates::Default,
                               nullptr, nullptr, gv);
        }

        const float returnW = 180.f, returnH = 50.f;
        const float returnX = 28.f;
        const float returnY = height_ - returnBtnBottomMargin - returnH;
        deckViewReturnButton_ = sf::FloatRect(sf::Vector2f(returnX, returnY), sf::Vector2f(returnW, returnH));
        sf::RectangleShape returnBtn(sf::Vector2f(returnW, returnH));
        returnBtn.setPosition(sf::Vector2f(returnX, returnY));
        returnBtn.setFillColor(sf::Color(120, 50, 50));
        returnBtn.setOutlineColor(sf::Color(200, 90, 90));
        returnBtn.setOutlineThickness(2.f);
        window.draw(returnBtn);
        sf::Text returnLabel(fontForChinese(), sf::String(L"返回"), 24);
        returnLabel.setFillColor(sf::Color::White);
        const sf::FloatRect rlb = returnLabel.getLocalBounds();
        returnLabel.setOrigin(sf::Vector2f(rlb.position.x + rlb.size.x * 0.5f, rlb.position.y + rlb.size.y * 0.5f));
        returnLabel.setPosition(sf::Vector2f(returnX + returnW * 0.5f, returnY + returnH * 0.5f));
        window.draw(returnLabel);

        if (deck_view_detail_active_) {
            updateDeckViewDetailLayout_();
            const float ovTop = 0.f;
            sf::RectangleShape dimDetail(sf::Vector2f(static_cast<float>(width_), static_cast<float>(height_) - ovTop));
            dimDetail.setPosition(sf::Vector2f(0.f, ovTop));
            dimDetail.setFillColor(sf::Color(6, 6, 10, 190));
            window.draw(dimDetail);
            const std::string disp =
                deck_view_detail_resolve_display_id(deck_view_detail_inst_, deck_view_detail_show_upgraded_);
            const sf::FloatRect& cr = deck_view_detail_card_rect_;
            drawDetailedCardAt(window, disp, cr.position.x, cr.position.y, cr.size.x, cr.size.y,
                             sf::Color(205, 75, 58), 11.f, sf::RenderStates::Default, nullptr, nullptr,
                             &deck_view_glossary_entries_);
            deck_view_glossary_card_screen_ = cr;
            deck_view_glossary_active_ = true;
            const sf::FloatRect& br = deck_view_detail_upgrade_btn_rect_;
            const bool alreadyUp =
                !deck_view_detail_inst_.id.empty() && deck_view_detail_inst_.id.back() == '+';
            const bool baseUpgradable = deck_view_card_has_upgrade_definition(deck_view_detail_inst_.id);
            sf::RectangleShape upBtn(sf::Vector2f(br.size.x, br.size.y));
            upBtn.setPosition(br.position);
            if (alreadyUp || !baseUpgradable) {
                upBtn.setFillColor(sf::Color(68, 66, 74));
                upBtn.setOutlineColor(sf::Color(105, 103, 112));
            } else {
                upBtn.setFillColor(sf::Color(88, 128, 78));
                upBtn.setOutlineColor(sf::Color(138, 188, 125));
            }
            upBtn.setOutlineThickness(2.f);
            window.draw(upBtn);
            sf::String btnStr = alreadyUp                      ? sf::String(L"已是升级版")
                                : (!baseUpgradable)          ? sf::String(L"不可升级")
                                : deck_view_detail_show_upgraded_ ? sf::String(L"查看原版")
                                                                 : sf::String(L"查看升级");
            sf::Text upLabel(fontForChinese(), btnStr, 22);
            upLabel.setFillColor(sf::Color(235, 232, 228));
            const sf::FloatRect ulb = upLabel.getLocalBounds();
            upLabel.setOrigin(sf::Vector2f(ulb.position.x + ulb.size.x * 0.5f, ulb.position.y + ulb.size.y * 0.5f));
            upLabel.setPosition(sf::Vector2f(br.position.x + br.size.x * 0.5f, br.position.y + br.size.y * 0.5f));
            window.draw(upLabel);
        }
        if (deck_view_glossary_active_ && !deck_view_glossary_entries_.empty())
            draw_card_glossary_beside_preview_(window, deck_view_glossary_card_screen_, deck_view_glossary_entries_);
    }

    // 战场中央：玩家区(背景+模型+血条在下+增益减益)、怪物区(背景+意图在上+模型+血条在下)
    void BattleUI::drawBattleCenter(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        // 根据“新伤害飘字事件”触发攻击位移动画（纯表现层，尽量不侵入规则层）
        {
            int fresh = 0;
            for (const auto& ev : s.pendingDamageDisplays) {
                if (ev.frames_remaining >= 179) ++fresh;  // BattleEngine 推入时为 180
            }
            if (fresh != prevFreshDamageEventCount_) {
                // 新增事件：触发对应攻击者的前冲动画
                for (const auto& ev : s.pendingDamageDisplays) {
                    if (ev.frames_remaining < 179) continue;
                    if (!ev.is_player) {
                        // 怪物受击：近似为玩家攻击
                        playerAttackClock_.restart();
                    } else {
                        // 玩家受击：近似为怪物攻击（选一只存活怪物前冲）
                        int chosen = -1;
                        const int n = static_cast<int>(s.monsters.size());
                        if (n == 1) chosen = 0;
                        if (chosen < 0) {
                            for (int i = 0; i < n; ++i) {
                                if (s.monsters[static_cast<size_t>(i)].currentHp <= 0) continue;
                                if (monster_intent_shows_attack_value_on_icon(s.monsters[static_cast<size_t>(i)].currentIntent.kind)) {
                                    chosen = i;
                                    break;
                                }
                            }
                        }
                        if (chosen < 0) {
                            for (int i = 0; i < n; ++i) {
                                if (s.monsters[static_cast<size_t>(i)].currentHp > 0) { chosen = i; break; }
                            }
                        }
                        if (chosen >= 0) {
                            if (static_cast<size_t>(chosen) >= monsterAttackClocks_.size())
                                monsterAttackClocks_.resize(static_cast<size_t>(chosen) + 1);
                            monsterAttackClocks_[static_cast<size_t>(chosen)].restart();
                        }
                    }
                }
                prevFreshDamageEventCount_ = fresh;
            }
        }

        const float battleTop = RELICS_ROW_Y + RELICS_ROW_H + 14.f;  // 战场在遗物栏下方
        const float battleH = (height_ * BOTTOM_BAR_Y_RATIO) - battleTop - 16.f;  // 战场高度
        const float playerLeft = width_ * 0.06f;      // 玩家区左边界
        const float playerW = width_ * 0.36f;        // 玩家区宽度
        const float monsterLeft = width_ * 0.58f;     // 怪物区左边界
        const float monsterW = width_ * 0.36f;       // 怪物区宽度
        const float modelCenterY = battleTop + battleH * MODEL_CENTER_Y_RATIO + BATTLE_MODEL_BLOCK_Y_OFFSET;  // 模型中心 Y

        char buf[32];

        // ---------- 玩家区：模型占位 -> 血条在模型下方 -> 增益减益在血条下方 ----------
        // playerModelRect_ 用于自选目标牌高亮与点击命中检测
        float playerCenterX = playerLeft + playerW * 0.5f;  // 玩家区中心 x
        // 玩家攻击位移：向右前冲（朝怪物区）
        {
            const float dur = 0.22f;
            const float t = playerAttackClock_.getElapsedTime().asSeconds();
            if (t >= 0.f && t <= dur) {
                const float t01 = t / dur;
                playerCenterX += ui_attack_lunge01(t01) * 54.f;
            }
        }
        float modelTop = modelCenterY - MODEL_TOP_OFFSET;    // 模型上边，固定不变
        playerModelRect_ = sf::FloatRect(
            sf::Vector2f(playerCenterX - MODEL_PLACEHOLDER_W * 0.5f, modelTop),
            sf::Vector2f(MODEL_PLACEHOLDER_W, MODEL_PLACEHOLDER_H)
        );
        auto pit = playerTextures_.find(s.character);
        if (pit != playerTextures_.end()) {          // 有玩家纹理则用 Sprite 绘制
            sf::Sprite playerSprite(pit->second);
            const sf::FloatRect texRect = playerSprite.getLocalBounds();
            const float scaleX = (texRect.size.x > 0.f) ? (MODEL_PLACEHOLDER_W / texRect.size.x) : 1.f;
            const float scaleY = (texRect.size.y > 0.f) ? (MODEL_PLACEHOLDER_H / texRect.size.y) : 1.f;
            playerSprite.setScale(sf::Vector2f(scaleX, scaleY));
            playerSprite.setPosition(sf::Vector2f(playerCenterX - MODEL_PLACEHOLDER_W * 0.5f, modelTop));
            window.draw(playerSprite);
        } else {                                    // 无纹理则用灰色占位矩形
            sf::RectangleShape playerModel(sf::Vector2f(MODEL_PLACEHOLDER_W, MODEL_PLACEHOLDER_H));
            playerModel.setPosition(sf::Vector2f(playerCenterX - MODEL_PLACEHOLDER_W * 0.5f, modelTop));
            playerModel.setFillColor(sf::Color(60, 55, 70));
            playerModel.setOutlineColor(sf::Color(100, 95, 110));
            playerModel.setOutlineThickness(1.f);
            window.draw(playerModel);
        }
        sf::Text playerLabel(fontForChinese(), sf::String(L"玩家"), 20);  // 玩家标签
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

        float barY = modelTop + MODEL_PLACEHOLDER_H + 12.f;  // 血条在模型下方 12px
        float barX = playerCenterX - HP_BAR_W * 0.5f;
        sf::RectangleShape bgBar(sf::Vector2f(HP_BAR_W, HP_BAR_H));
        bgBar.setPosition(sf::Vector2f(barX, barY));
        bgBar.setFillColor(sf::Color(35, 30, 40));
        // 无格挡：更透明的淡灰外框；有格挡：淡蓝外框
        bgBar.setOutlineColor(s.block > 0 ? sf::Color(170, 210, 255, 255) : sf::Color(200, 200, 210, 140));  // 格挡时蓝框
        bgBar.setOutlineThickness(2.f);
        window.draw(bgBar);
        float hpRatio = s.maxHp > 0 ? static_cast<float>(s.currentHp) / s.maxHp : 0.f;  // 血量比例
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
        const float statusIconSz = BATTLE_STATUS_ICON_SZ;
        const float statusGapX = statusIconSz + 14.f;  // 横向间距
        const float statusGapY = statusIconSz + 8.f;   // 纵向间距
        const float stBaseX = barX;
        const float stBaseY = barY + HP_BAR_H + 6.f;
        int hoveredPlayerStatus = -1;
        int playerStatusDisplayIdx = 0;
        for (size_t i = 0; i < s.playerStatuses.size(); ++i) {
            const auto& st = s.playerStatuses[i];
            if (battle_ui_status_hide_from_icon_row(st.id))
                continue;
            const size_t col = static_cast<size_t>(playerStatusDisplayIdx) % 8u;
            const size_t row = static_cast<size_t>(playerStatusDisplayIdx) / 8u;
            const float stX = stBaseX + static_cast<float>(col) * statusGapX;
            const float stY = stBaseY + static_cast<float>(row) * statusGapY;
            PendingBattleStatusIcon pbd;
            pbd.x = stX;
            pbd.y = stY;
            pbd.iconSz = statusIconSz;
            pbd.stacks = st.stacks;
            pbd.id = st.id;
            pbd.monsterPalette = false;
            pendingBattleStatusIcons_.push_back(std::move(pbd));

            if (hoveredPlayerStatus < 0 &&
                mousePos_.x >= stX && mousePos_.x <= stX + statusIconSz &&
                mousePos_.y >= stY && mousePos_.y <= stY + statusIconSz) {
                hoveredPlayerStatus = playerStatusDisplayIdx;
            }
            ++playerStatusDisplayIdx;
        }

        // ---------- 怪物区：1~3 只怪横向排列，意图在模型上方 -> 模型 -> 血条在下方 ----------
        // monsterModelRects_、monsterIntentRects_ 供出牌瞄准与点击检测
        constexpr float MONSTER_GAP = 88.f;
        const size_t nMonsters = s.monsters.size();
        float monModelW = MODEL_PLACEHOLDER_W;
        float monModelH = MODEL_PLACEHOLDER_H;
        float monTopOff = MODEL_TOP_OFFSET;
        if (nMonsters > 0) {
            const float rawTotal = static_cast<float>(nMonsters) * MODEL_PLACEHOLDER_W
                + static_cast<float>(nMonsters - 1) * MONSTER_GAP;
            if (rawTotal > monsterW * 0.96f) {
                const float slotW = (monsterW * 0.96f - static_cast<float>(nMonsters - 1) * MONSTER_GAP)
                    / static_cast<float>(nMonsters);
                const float s = slotW / MODEL_PLACEHOLDER_W;
                monModelW = MODEL_PLACEHOLDER_W * s;
                monModelH = MODEL_PLACEHOLDER_H * s;
                monTopOff = MODEL_TOP_OFFSET * s;
            }
        }
        const float mModelTop = modelCenterY - monTopOff;
        monsterModelRects_.clear();
        monsterIntentRects_.clear();
        std::vector<float> monsterCenterXs;
        const float totalMonsterW = nMonsters > 0
            ? static_cast<float>(nMonsters) * monModelW + static_cast<float>(nMonsters - 1) * MONSTER_GAP
            : 0.f;
        const float monsterStartX = monsterLeft + (monsterW - totalMonsterW) * 0.5f + monModelW * 0.5f;

        for (size_t i = 0; i < nMonsters; ++i) {
            const float monsterCenterX = monsterStartX + static_cast<float>(i) * (monModelW + MONSTER_GAP);
            monsterCenterXs.push_back(monsterCenterX);
            const auto& m = s.monsters[i];
            float intentY = mModelTop - INTENT_ORB_R * 2.f - 6.f;
            const float intentLeft = monsterCenterX - INTENT_ORB_R;
            const float intentTopBase = intentY - INTENT_ORB_R;
            const float intentSz = INTENT_ORB_R * 2.f;
            const float intentFloatY =
                std::sin(intent_float_clock_.getElapsedTime().asSeconds() * INTENT_FLOAT_SPEED
                         + static_cast<float>(i) * INTENT_FLOAT_PHASE)
                * (INTENT_FLOAT_AMP_PX * uiScaleX_);
            const float intentTop = intentTopBase + intentFloatY;
            sf::FloatRect intentRect(sf::Vector2f(intentLeft, intentTop), sf::Vector2f(intentSz, intentSz));
            monsterIntentRects_.push_back(intentRect);
            // 意图图标：优先用图片，无图则灰色圆球占位
            std::string iconLookup;
            if (!m.currentIntent.ui_icon_key.empty())
                iconLookup = m.currentIntent.ui_icon_key;
            else if (m.currentIntent.kind == MonsterIntentKind::Attack) iconLookup = "Attack";
            else if (m.currentIntent.kind == MonsterIntentKind::Mul_Attack) iconLookup = "Attack";
            else if (m.currentIntent.kind == MonsterIntentKind::Block) iconLookup = "Block";
            else if (m.currentIntent.kind == MonsterIntentKind::Attack_And_Block) iconLookup = "AttackBlock";
            else if (m.currentIntent.kind == MonsterIntentKind::Ritual) iconLookup = "Ritual";
            else if (m.currentIntent.kind == MonsterIntentKind::Strength_And_Player_Weak) iconLookup = "Strategy";
            else if (m.currentIntent.kind == MonsterIntentKind::Strength_And_Block) iconLookup = "AttackBlock";
            else if (m.currentIntent.kind == MonsterIntentKind::Strength_And_Player_Frail) iconLookup = "Strategy";
            else if (m.currentIntent.kind == MonsterIntentKind::Player_Dexterity_Down) iconLookup = "Debuff";
            else if (m.currentIntent.kind == MonsterIntentKind::Attack_And_Weak) iconLookup = "SmallKnife"; // 兼容旧资源名，可用 ui_icon 覆盖
            else if (m.currentIntent.kind == MonsterIntentKind::Attack_And_Vulnerable) iconLookup = "AttackVulnerable";
            else if (m.currentIntent.kind == MonsterIntentKind::Buff) iconLookup = "Strategy";
            else if (m.currentIntent.kind == MonsterIntentKind::Debuff) iconLookup = "Debuff";
            else if (m.currentIntent.kind == MonsterIntentKind::Sleep) iconLookup = "Sleep";
            else if (m.currentIntent.kind == MonsterIntentKind::Stun) iconLookup = "Stun";
            else if (m.currentIntent.kind == MonsterIntentKind::Unknown) iconLookup = "Unknown";
            const sf::Texture* intentTex = lookup_intention_texture(intentionTextures_, iconLookup);
            if (!intentTex && !iconLookup.empty()) {
                if (const std::string p = resolve_image_path("assets/intention/" + iconLookup); !p.empty()) {
                    if (loadIntentionTexture(iconLookup, p))
                        intentTex = lookup_intention_texture(intentionTextures_, iconLookup);
                }
            }
            if (intentTex) {
                sf::Sprite intentSprite(*intentTex);
                const sf::FloatRect tr = intentSprite.getLocalBounds();
                const float sx = (tr.size.x > 0.f) ? (intentSz / tr.size.x) : 1.f;
                const float sy = (tr.size.y > 0.f) ? (intentSz / tr.size.y) : 1.f;
                intentSprite.setScale(sf::Vector2f(sx, sy));
                intentSprite.setPosition(sf::Vector2f(intentLeft, intentTop));
                window.draw(intentSprite);
            } else {
                sf::CircleShape intentOrb(INTENT_ORB_R);
                intentOrb.setPosition(sf::Vector2f(intentLeft, intentTop));
                intentOrb.setFillColor(sf::Color(100, 100, 110));
                intentOrb.setOutlineColor(sf::Color(140, 140, 150));
                intentOrb.setOutlineThickness(1.f);
                window.draw(intentOrb);
            }
            // 攻击类意图：在图标中心略偏左下叠画伤害数字（连击总伤 / 单段攻击等共用 value）
            if (monster_intent_shows_attack_value_on_icon(m.currentIntent.kind) && m.currentIntent.value != 0 && fontLoaded_) {
                const float cx = intentLeft + intentSz * 0.5f;
                const float cy = intentTop + intentSz * 0.5f;
                const unsigned valFontSz = static_cast<unsigned>(std::lround(std::clamp(intentSz * 0.30f, 15.f, 22.f)));
                char vbuf[32];
                std::snprintf(vbuf, sizeof(vbuf), "%d", m.currentIntent.value);
                sf::Text valText(font_, vbuf, valFontSz);
                valText.setStyle(sf::Text::Bold);
                valText.setFillColor(sf::Color(255, 252, 245));
                valText.setOutlineThickness(2.4f);
                valText.setOutlineColor(sf::Color(25, 18, 28));
                const sf::FloatRect vb = valText.getLocalBounds();
                valText.setOrigin(sf::Vector2f(vb.position.x + vb.size.x * 0.5f, vb.position.y + vb.size.y * 0.5f));
                // 相对中心更靠左下（略小于字号，避免抢图标主体）
                valText.setPosition(sf::Vector2f(cx - intentSz * 0.34f, cy + intentSz * 0.34f));
                window.draw(valText);
            }
            if (intentRect.contains(mousePos_)) {
                sf::String intentStr;
                if (!m.currentIntent.ui_label.empty()) {
                    intentStr = sf::String::fromUtf8(m.currentIntent.ui_label.begin(), m.currentIntent.ui_label.end());
                    const bool appendVal =
                        (m.currentIntent.kind == MonsterIntentKind::Attack
                            || m.currentIntent.kind == MonsterIntentKind::Mul_Attack
                            || m.currentIntent.kind == MonsterIntentKind::Attack_And_Weak
                            || m.currentIntent.kind == MonsterIntentKind::Attack_And_Vulnerable
                            || (m.currentIntent.kind == MonsterIntentKind::Ritual && m.currentIntent.value > 0)
                            || (m.currentIntent.kind == MonsterIntentKind::Debuff && m.currentIntent.value > 0))
                        && m.currentIntent.value != 0;
                    if (appendVal)
                        intentStr += sf::String(L" ") + sf::String(std::to_wstring(m.currentIntent.value));
                } else {
                    switch (m.currentIntent.kind) {
                    case MonsterIntentKind::Attack:
                        intentStr = sf::String(L"攻击 ") + sf::String(std::to_wstring(m.currentIntent.value));
                        break;
                    case MonsterIntentKind::Mul_Attack:
                        intentStr = sf::String(L"连击总伤害 ") + sf::String(std::to_wstring(m.currentIntent.value));
                        break;
                    case MonsterIntentKind::Block:
                        intentStr = sf::String(L"防御");
                        break;
                    case MonsterIntentKind::Attack_And_Block:
                        intentStr = sf::String(L"攻击并防御");
                        break;
                    case MonsterIntentKind::Attack_And_Weak:
                        intentStr = sf::String(L"小刀 ") + sf::String(std::to_wstring(m.currentIntent.value));
                        break;
                    case MonsterIntentKind::Attack_And_Vulnerable:
                        intentStr = sf::String(L"攻击+易伤 ") + sf::String(std::to_wstring(m.currentIntent.value));
                        break;
                    case MonsterIntentKind::Ritual:
                        intentStr = (m.currentIntent.value > 0)
                            ? (sf::String(L"仪式 +") + sf::String(std::to_wstring(m.currentIntent.value)))
                            : sf::String(L"仪式");
                        break;
                    case MonsterIntentKind::Strength_And_Player_Weak:
                        intentStr = sf::String(L"力量+虚弱");
                        break;
                    case MonsterIntentKind::Strength_And_Block:
                        intentStr = sf::String(L"力量+格挡");
                        break;
                    case MonsterIntentKind::Strength_And_Player_Frail:
                        intentStr = sf::String(L"力量+脆弱");
                        break;
                    case MonsterIntentKind::Player_Dexterity_Down:
                        intentStr = sf::String(L"敏捷下降");
                        break;
                    case MonsterIntentKind::Buff:
                        intentStr = sf::String(L"强化");
                        break;
                    case MonsterIntentKind::Debuff:
                        intentStr = (m.currentIntent.value > 0)
                            ? (sf::String(L"易伤 ") + sf::String(std::to_wstring(m.currentIntent.value)))
                            : sf::String(L"施加负面效果");
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
                }
                const float tipPad = 8.f;
                sf::Text tipText(fontForChinese(), intentStr, 18);
                tipText.setFillColor(sf::Color(240, 235, 220));
                const sf::FloatRect tb = tipText.getLocalBounds();
                const float tipW = tb.size.x + tipPad * 2.f;
                const float tipH = tb.size.y + tipPad * 2.f;
                float tipX = monsterCenterX - tipW * 0.5f;
                float tipY = intentTop - tipH - 4.f;
                if (tipX < 10.f) tipX = 10.f;
                if (tipX + tipW > width_ - 10.f) tipX = width_ - tipW - 10.f;
                if (tipY < 10.f) tipY = intentTop + INTENT_ORB_R * 2.f + 4.f;
                sf::RectangleShape tipBg(sf::Vector2f(tipW, tipH));
                tipBg.setPosition(sf::Vector2f(tipX, tipY));
                tipBg.setFillColor(sf::Color(30, 28, 35, 240));
                tipBg.setOutlineColor(sf::Color(160, 155, 140));
                tipBg.setOutlineThickness(1.f);
                window.draw(tipBg);
                tipText.setPosition(sf::Vector2f(tipX + tipPad - tb.position.x, tipY + tipPad - tb.position.y));
                window.draw(tipText);
            }
        }

        const float breathT = intent_float_clock_.getElapsedTime().asSeconds();
        const float modelFootY = mModelTop + monModelH;
        for (size_t i = 0; i < nMonsters; ++i) {
            float monsterCenterX = monsterCenterXs[i];
            // 怪物攻击位移：向左前冲（朝玩家区）
            if (i < monsterAttackClocks_.size()) {
                const float dur = 0.22f;
                const float t = monsterAttackClocks_[i].getElapsedTime().asSeconds();
                if (t >= 0.f && t <= dur) {
                    const float t01 = t / dur;
                    monsterCenterX += -ui_attack_lunge01(t01) * 44.f;
                }
            }
            const float breath =
                1.f
                + MONSTER_BREATH_SCALE_AMP
                      * std::sin(breathT * MONSTER_BREATH_SPEED + static_cast<float>(i) * MONSTER_BREATH_PHASE);
            const float drawW = monModelW * breath;
            const float drawH = monModelH * breath;
            const float modelLeft = monsterCenterX - drawW * 0.5f;
            const float modelTop = modelFootY - drawH;
            sf::FloatRect modelRect(sf::Vector2f(modelLeft, modelTop), sf::Vector2f(drawW, drawH));
            monsterModelRects_.push_back(modelRect);
            const std::string& monsterId = s.monsters[i].id;
            auto it = monsterTextures_.find(monsterId);
            if (it != monsterTextures_.end()) {
                sf::Sprite monsterSprite(it->second);
                const sf::FloatRect texRect = monsterSprite.getLocalBounds();
                const float baseSx = (texRect.size.x > 0.f) ? (monModelW / texRect.size.x) : 1.f;
                const float baseSy = (texRect.size.y > 0.f) ? (monModelH / texRect.size.y) : 1.f;
                const float ox = texRect.position.x + texRect.size.x * 0.5f;
                const float oy = texRect.position.y + texRect.size.y;
                monsterSprite.setOrigin(sf::Vector2f(ox, oy));
                monsterSprite.setScale(sf::Vector2f(baseSx * breath, baseSy * breath));
                monsterSprite.setPosition(sf::Vector2f(monsterCenterX, modelFootY));
                window.draw(monsterSprite);
            } else {
                sf::RectangleShape monsterModel(sf::Vector2f(monModelW, monModelH));
                monsterModel.setOrigin(sf::Vector2f(monModelW * 0.5f, monModelH));
                monsterModel.setScale(sf::Vector2f(breath, breath));
                monsterModel.setPosition(sf::Vector2f(monsterCenterX, modelFootY));
                monsterModel.setFillColor(sf::Color(70, 55, 65));
                monsterModel.setOutlineColor(sf::Color(110, 90, 100));
                monsterModel.setOutlineThickness(1.f);
                window.draw(monsterModel);
            }
        }

        // 悬停怪物模型时显示中文名称（默认不绘制怪物 id）
        for (size_t i = 0; i < nMonsters; ++i) {
            if (i >= monsterModelRects_.size()) break;
            if (!monsterModelRects_[i].contains(mousePos_)) continue;
            const float monsterCenterX = monsterCenterXs[i];
            const std::string& mid = s.monsters[i].id;
            const MonsterData* md = get_monster_by_id(mid);
            sf::String tipStr;
            if (md && !md->name.empty())
                tipStr = sf::String::fromUtf8(md->name.begin(), md->name.end());
            else
                tipStr = sf::String(mid);
            const float tipPad = 8.f;
            sf::Text tipText(fontForChinese(), tipStr, 18);
            tipText.setFillColor(sf::Color(240, 235, 220));
            const sf::FloatRect tb = tipText.getLocalBounds();
            const float tipW = tb.size.x + tipPad * 2.f;
            const float tipH = tb.size.y + tipPad * 2.f;
            float tipX = monsterCenterX - tipW * 0.5f;
            float tipY = mModelTop - tipH - 6.f;
            if (tipX < 10.f) tipX = 10.f;
            if (tipX + tipW > static_cast<float>(width_) - 10.f) tipX = static_cast<float>(width_) - tipW - 10.f;
            if (tipY < 10.f) tipY = mModelTop + monModelH + 6.f;
            sf::RectangleShape tipBg(sf::Vector2f(tipW, tipH));
            tipBg.setPosition(sf::Vector2f(tipX, tipY));
            tipBg.setFillColor(sf::Color(30, 28, 35, 240));
            tipBg.setOutlineColor(sf::Color(160, 155, 140));
            tipBg.setOutlineThickness(1.f);
            window.draw(tipBg);
            tipText.setPosition(sf::Vector2f(tipX + tipPad - tb.position.x, tipY + tipPad - tb.position.y));
            window.draw(tipText);
            break;
        }

        float mBarY = mModelTop + monModelH + 12.f;
        int hoveredMonsterIndex = -1;
        int hoveredMonsterStatus = -1;
        for (size_t i = 0; i < nMonsters; ++i) {
            const auto& m = s.monsters[i];
            const float monsterCenterX = monsterCenterXs[i];
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
            const float statusIconSz = BATTLE_STATUS_ICON_SZ;
            const float statusGapX = statusIconSz + 14.f;
            const float statusGapY = statusIconSz + 8.f;
            const float mstBaseX = mBarX;
            const float mstBaseY = mBarY + HP_BAR_H + 6.f;
            if (i < s.monsterStatuses.size()) {
                const auto& mStatuses = s.monsterStatuses[i];
                int displayIdx = 0;
                for (size_t si = 0; si < mStatuses.size(); ++si) {
                    const auto& st = mStatuses[si];
                    if (battle_ui_status_hide_from_icon_row(st.id))
                        continue;
                    const size_t col = static_cast<size_t>(displayIdx) % 8u;
                    const size_t row = static_cast<size_t>(displayIdx) / 8u;
                    const float mstX = mstBaseX + static_cast<float>(col) * statusGapX;
                    const float mstY = mstBaseY + static_cast<float>(row) * statusGapY;
                    PendingBattleStatusIcon mbd;
                    mbd.x = mstX;
                    mbd.y = mstY;
                    mbd.iconSz = statusIconSz;
                    mbd.stacks = st.stacks;
                    mbd.id = st.id;
                    mbd.monsterPalette = true;
                    pendingBattleStatusIcons_.push_back(std::move(mbd));

                    // 悬停检测
                    if (hoveredMonsterStatus < 0 &&
                        mousePos_.x >= mstX && mousePos_.x <= mstX + statusIconSz &&
                        mousePos_.y >= mstY && mousePos_.y <= mstY + statusIconSz) {
                        hoveredMonsterIndex = static_cast<int>(i);
                        hoveredMonsterStatus = static_cast<int>(displayIdx);
                    }
                    ++displayIdx;
                }
            }
        }

        // 玩家/怪物状态悬停提示：玩家放在模型右侧，怪物放在模型左侧；悬停任一图标时显示该单位所有状态的提示框（文案与卡牌名词一致）
        auto makeStatusTooltipText = [&](const StatusInstance& st) {
            const std::string& id = st.id;
            int n = st.stacks;

            std::wstring name;
            std::wstring line2;

            if (id == "strength") {
                name = L"劲力";
                line2 = L"攻击伤害提升 " + std::to_wstring(n);
            }
            else if (id == "vulnerable") {
                name = L"破绽";
                line2 = L"从武学受到的伤害增加 50%，持续时间" + std::to_wstring(st.duration) + L"回合";
            }
            else if (id == "weak") {
                name = L"涣散";
                line2 = L"武学造成的伤害降低 25%，持续时间" + std::to_wstring(st.duration) + L"回合";
            }
            else if (id == "dexterity") {
                name = L"灵动";
                line2 = L"从卡牌获得的护体罡气提升 " + std::to_wstring(n);
            }
            else if (id == "frail") {
                name = L"脆弱";
                line2 = L"在" + std::to_wstring(st.duration) + L"回合内，从卡牌中获得的护体罡气减少 25%";
            }
            else if (id == "blasphemy") {
                name = L"渎神";
                line2 = L"在你的回合开始时立即死亡";
            }
            else if (id == "entangle") {
                name = L"缠绕";
                line2 = L"本回合无法打出武学牌；在你的回合结束时，受到 " + std::to_wstring(n) + L" 点伤害（受护体罡气影响）";
            }
            else if (id == "shackles") {
                name = L"镣铐";
                line2 = L"在其回合结束时，回复 " + std::to_wstring(n) + L" 点劲力";
            }
            else if (id == "flex") {
                name = L"活动肌肉";
                line2 = L"在你的回合结束时，失去 " + std::to_wstring(n) + L" 点劲力";
            }
            else if (id == "dexterity_down") {
                name = L"灵动下降";
                line2 = L"在你的回合结束时，失去 " + std::to_wstring(n) + L" 点灵动（可减至负数）";
            }
            else if (id == "draw_reduction") {
                name = L"抽牌减少";
                line2 = L"下 " + std::to_wstring(n) + L" 个回合内，每回合少抽 1 张牌";
            }
            else if (id == "cannot_draw") {
                name = L"不能抽牌";
                line2 = L"本回合内无法从抽牌堆抽牌；持续 " + std::to_wstring(st.duration) + L" 回合";
            }
            else if (id == "cannot_block") {
                name = L"罡气断绝";
                line2 = (st.duration < 0)
                    ? L"你无法从卡牌中获得护体罡气（本场战斗）"
                    : L"你无法从卡牌中获得护体罡气，持续 " + std::to_wstring(st.duration) + L" 回合";
            }
            else if (id == "draw_up") {
                name = L"抽牌增加";
                line2 = L"下回合多抽 " + std::to_wstring(n) + L" 张牌";
            }
            else if (id == "energy_up") {
                name = L"真气充盈";
                line2 = L"下回合额外获得 " + std::to_wstring(n) + L" 点真气";
            }
            else if (id == "block_up") {
                name = L"下回合护体罡气";
                line2 = L"在你的下回合开始时，获得 " + std::to_wstring(n) + L" 点护体罡气";
            }
            else if (id == "vigor") {
                name = L"活力";
                line2 = L"你的下一次武学造成 " + std::to_wstring(n) + L" 点额外伤害";
            }
            else if (id == "poison") {
                name = L"蛊毒";
                line2 = L"在回合开始时，受到 " + std::to_wstring(n) + L" 点伤害，然后蛊毒层数减少 1";
            }
            else if (id == "death_rhythm") {
                name = L"死亡律动";
                line2 = L"你每打出一张牌，受到等同于场上所有存活敌人该效果层数之和的伤害；本图标为这名敌人提供的 " + std::to_wstring(n) + L" 层";
            }
            else if (id == "curiosity") {
                name = L"好奇";
                line2 = L"你每打出一张能力牌，该敌人获得 " + std::to_wstring(n) + L" 点劲力";
            }
            else if (id == "anger") {
                name = L"生气";
                line2 = L"每当受到武学伤害，获得 " + std::to_wstring(n) + L" 点劲力";
            }
            else if (id == "curl_up") {
                name = L"蜷身";
                line2 = L"当受到武学伤害时，获得 " + std::to_wstring(n) + L" 点护体罡气；每场战斗仅触发一次";
            }
            else if (id == "vanish") {
                name = L"消逝";
                line2 = L"层数每回合减少 1（敌方回合结束时）；降至 0 时立即死亡。当前剩余 " + std::to_wstring(n) + L" 回合";
            }
            else if (id == "metallicize") {
                name = L"金属化";
                line2 = L"在你的回合结束时，获得 " + std::to_wstring(n) + L" 点护体罡气";
            }
            else if (id == "poison_cloud") {
                name = L"毒雾";
                line2 = L"在你的回合开始时，给予所有敌人 " + std::to_wstring(n) + L" 层蛊毒";
            }
            else if (id == "double_damage") {
                name = L"双倍伤害";
                line2 = L"武学造成的伤害翻倍；剩余 " + std::to_wstring(n) + L" 回合";
            }
            else if (id == "double_tap") {
                name = L"双发";
                line2 = L"本回合再打出的下 " + std::to_wstring(n) + L" 张武学牌会打出两次";
            }
            else if (id == "demon_form") {
                name = L"恶魔形态";
                line2 = L"在你的回合开始时，获得 " + std::to_wstring(n) + L" 点劲力";
            }
            else if (id == "heat_sink") {
                name = L"散热";
                line2 = L"你每打出一张能力牌，抽 " + std::to_wstring(n) + L" 张牌";
            }
            else if (id == "hello") {
                name = L"你好";
                line2 = L"在你的回合开始时，将 " + std::to_wstring(n) + L" 张随机普通稀有度的牌置入手牌（本场临时牌）";
            }
            else if (id == "unstoppable") {
                name = L"势不可挡";
                line2 = L"每当你获得护体罡气时，对一名随机敌人造成 " + std::to_wstring(n) + L" 点伤害";
            }
            else if (id == "establishment") {
                name = L"确立基础";
                line2 = L"每当有卡牌因「保留」留在手牌或打出后回到手牌，该牌本场耗能额外减少 " + std::to_wstring(n) + L" 点";
            }
            else if (id == "equilibrium") {
                name = L"均衡";
                line2 = L"接下来 " + std::to_wstring(n) + L" 个你的回合结束时，手牌保留在手中（虚无牌仍会消耗）";
            }
            else if (id == "free_attack") {
                name = L"免费武学";
                line2 = L"再打出的下 " + std::to_wstring(n) + L" 张武学牌耗能为 0";
            }
            else if (id == "ritual") {
                name = L"仪式";
                line2 = L"在其回合结束时，获得 " + std::to_wstring(n) + L" 点劲力";
            }
            else if (id == "multi_armor") {
                name = L"多层护甲";
                line2 = L"在其回合结束时，获得 " + std::to_wstring(n) + L" 点护体罡气；\n该单位受到武学伤害而失去气血时，层数减少 1";
            }
            else if (id == "barricade") {
                name = L"壁垒";
                line2 = L"护体罡气不会在其回合开始时消失";
            }
            else if (id == "accuracy") {
                name = L"精准";
                line2 = L"小刀造成的伤害增加 " + std::to_wstring(n);
            }
            else if (id == "panache") {
                name = L"神气制胜";
                line2 = L"每打出 5 张牌，对所有敌人造成 " + std::to_wstring(n) + L" 点伤害";
            }
            else if (id == "panache_counter") {
                name = L"神气进度";
                line2 = L"本回合已打出牌数计数（接近 5 时触发神气制胜）";
            }
            else if (id == "intangible") {
                name = L"无实体";
                line2 = L"在本回合内，将该单位每次受到的伤害和生命减少效果降为 1";
            }
            else if (id == "flight") {
                name = L"飞行";
                line2 = L"从武学受到的伤害减半；在本玩家回合内累计受到 " + std::to_wstring(n) + L" 次武学伤害后失去该状态";
            }
            else if (id == "indestructible") {
                name = L"坚不可摧";
                line2 = L"从本次玩家回合开始，累计最多再失去 " + std::to_wstring(n) + L" 点气血（仅统计实际扣血，护体罡气吸收不计入上限）";
            }
            else if (id == "artifact") {
                name = L"人工制品";
                line2 = L"免疫 " + std::to_wstring(n) + L" 次负面效果";
            }
            else if (id == "fasting") {
                name = L"斋戒";
                line2 = L"在你的回合开始时，失去 " + std::to_wstring(n) + L" 点真气";
            }
            else if (id == "wraith_form") {
                name = L"幽魂形态";
                line2 = L"在你的回合开始时，失去 " + std::to_wstring(n) + L" 点灵动";
            }
            else if (id == "buffer") {
                name = L"缓冲";
                line2 = L"阻止下 " + std::to_wstring(n) + L" 次你受到的气血损伤";
            }
            else if (id == "combust") {
                name = L"自燃";
                // Y 为自燃层数（本状态 st.stacks），X 为由牌提供的总伤害数值：来自状态 \"combust_damage\" 的层数
                int y = n;
                int x = 0;
                // 在玩家身上的状态列表中查找 combust_damage
                for (const auto& st2 : s.playerStatuses) {
                    if (st2.id == "combust_damage") {
                        x = st2.stacks;
                        break;
                    }
                }
                if (x <= 0) {
                    // 若尚未有 combust_damage，则描述中仅展示气血损失部分
                    line2 = L"在你的回合结束时，失去 " + std::to_wstring(y) + L" 点气血";
                }
                else {
                    line2 = L"在你的回合结束时，失去 " + std::to_wstring(y)
                        + L" 点气血，并对所有敌人造成 " + std::to_wstring(x) + L" 点伤害";
                }
            }
            else if (id == "rupture") {
                name = L"撕裂";
                line2 = L"当你从一张牌中失去气血时，获得 " + std::to_wstring(n) + L" 点劲力";
            }
            else {
                name = sf::String(id).toWideString();
                line2 = L"效果层数 " + std::to_wstring(n);
            }

            return name + L"\n" + line2;
            };

        constexpr sf::Color kStatusTooltipPlain(235, 230, 220);
        constexpr sf::Color kStatusTooltipTitleGold(235, 205, 115);
        constexpr unsigned kStatusTooltipCharPt = 20;
        constexpr float kStatusTooltipTitleBodyGap = 4.f;

        auto split_status_tooltip_desc = [](const std::wstring& desc, sf::String& outTitle, sf::String& outBody) {
            const std::size_t br = desc.find(L'\n');
            if (br == std::wstring::npos) {
                outTitle = sf::String(desc);
                outBody = sf::String();
                return;
            }
            outTitle = sf::String(desc.substr(0, br));
            outBody = (br + 1 < desc.size()) ? sf::String(desc.substr(br + 1)) : sf::String();
        };

        if (hoveredPlayerStatus >= 0 && !s.playerStatuses.empty()) {  // 悬停玩家状态图标时
            const float paddingX = 10.f;
            const float paddingY = 6.f;
            const float boxGapY = 6.f;
            const float boxLeft = playerCenterX + MODEL_PLACEHOLDER_W * 0.5f + 16.f;  // 玩家模型右侧
            float boxTop = modelTop + 8.f;

            const sf::Font& cn = fontForChinese();
            const float innerW =
                std::clamp(static_cast<float>(width_) * 0.26f, 220.f, 380.f);
            const float lineH = cn.getLineSpacing(kStatusTooltipCharPt);

            int maxBodyLines = 0;
            float maxPixelLineW = 0.f;
            for (size_t i = 0; i < s.playerStatuses.size(); ++i) {
                const auto& st = s.playerStatuses[i];
                if (battle_ui_status_hide_from_icon_row(st.id))
                    continue;
                std::wstring desc = makeStatusTooltipText(st);
                sf::String titleStr;
                sf::String bodyStr;
                split_status_tooltip_desc(desc, titleStr, bodyStr);
                sf::Text titleMeasure(cn, titleStr, kStatusTooltipCharPt);
                const float titleW = titleMeasure.getLocalBounds().size.x;
                float bodyMaxLineW = 0.f;
                int bodyLines = 0;
                if (!bodyStr.isEmpty()) {
                    measure_wrapped_lines_unlimited(cn, bodyStr, kStatusTooltipCharPt, innerW, bodyMaxLineW,
                                                    bodyLines);
                }
                maxBodyLines = std::max(maxBodyLines, bodyLines);
                maxPixelLineW = std::max(maxPixelLineW, std::max(titleW, bodyMaxLineW));
            }
            const float maxBoxWidth = std::max(48.f, maxPixelLineW + paddingX * 2.f);
            const float titleBodyGapCol = (maxBodyLines > 0) ? kStatusTooltipTitleBodyGap : 0.f;
            const float boxHeight = paddingY + lineH + titleBodyGapCol +
                                    static_cast<float>(std::max(0, maxBodyLines)) * lineH + paddingY;
            const float bodyInnerH =
                static_cast<float>(std::max(1, maxBodyLines)) * lineH + lineH * 0.35f;

            for (size_t i = 0; i < s.playerStatuses.size(); ++i) {
                const auto& st = s.playerStatuses[i];
                if (battle_ui_status_hide_from_icon_row(st.id))
                    continue;
                std::wstring desc = makeStatusTooltipText(st);
                sf::String titleStr;
                sf::String bodyStr;
                split_status_tooltip_desc(desc, titleStr, bodyStr);
                sf::RectangleShape bg(sf::Vector2f(maxBoxWidth, boxHeight));
                bg.setPosition(sf::Vector2f(boxLeft, boxTop));
                bg.setFillColor(sf::Color(40, 35, 45, 230));
                bg.setOutlineColor(sf::Color(150, 140, 120));
                bg.setOutlineThickness(1.f);
                window.draw(bg);
                const float textLeft = boxLeft + paddingX;
                float y = boxTop + paddingY;
                if (!titleStr.isEmpty()) {
                    sf::Text titleT(cn, titleStr, kStatusTooltipCharPt);
                    titleT.setFillColor(kStatusTooltipTitleGold);
                    const sf::FloatRect tb = titleT.getLocalBounds();
                    titleT.setPosition(sf::Vector2f(textLeft - tb.position.x, y - tb.position.y));
                    window.draw(titleT);
                }
                y += lineH;
                if (!bodyStr.isEmpty()) {
                    y += kStatusTooltipTitleBodyGap;
                    draw_wrapped_text(window, cn, bodyStr, kStatusTooltipCharPt,
                                      sf::Vector2f(textLeft, y), innerW, bodyInnerH, kStatusTooltipPlain);
                }
                boxTop += boxHeight + boxGapY;
            }
        }

        if (nMonsters > 0 && hoveredMonsterIndex >= 0 && static_cast<size_t>(hoveredMonsterIndex) < s.monsterStatuses.size() &&
            !s.monsterStatuses[static_cast<size_t>(hoveredMonsterIndex)].empty()) {  // 悬停怪物状态图标时
            const auto& mStatuses = s.monsterStatuses[static_cast<size_t>(hoveredMonsterIndex)];
            const float boxRight = monsterCenterXs[static_cast<size_t>(hoveredMonsterIndex)] - monModelW * 0.5f - 16.f;  // 怪物模型左侧
            const float paddingX = 10.f;
            const float paddingY = 6.f;
            const float boxGapY = 6.f;
            float boxTop = mModelTop + 8.f;

            const sf::Font& cn = fontForChinese();
            const float innerW =
                std::clamp(static_cast<float>(width_) * 0.26f, 220.f, 380.f);
            const float lineH = cn.getLineSpacing(kStatusTooltipCharPt);

            int maxBodyLines = 0;
            float maxPixelLineW = 0.f;
            for (size_t i = 0; i < mStatuses.size(); ++i) {
                const auto& st = mStatuses[i];
                if (battle_ui_status_hide_from_icon_row(st.id))
                    continue;
                std::wstring desc = makeStatusTooltipText(st);
                sf::String titleStr;
                sf::String bodyStr;
                split_status_tooltip_desc(desc, titleStr, bodyStr);
                sf::Text titleMeasure(cn, titleStr, kStatusTooltipCharPt);
                const float titleW = titleMeasure.getLocalBounds().size.x;
                float bodyMaxLineW = 0.f;
                int bodyLines = 0;
                if (!bodyStr.isEmpty()) {
                    measure_wrapped_lines_unlimited(cn, bodyStr, kStatusTooltipCharPt, innerW, bodyMaxLineW,
                                                    bodyLines);
                }
                maxBodyLines = std::max(maxBodyLines, bodyLines);
                maxPixelLineW = std::max(maxPixelLineW, std::max(titleW, bodyMaxLineW));
            }
            const float maxBoxWidth = std::max(48.f, maxPixelLineW + paddingX * 2.f);
            const float titleBodyGapCol = (maxBodyLines > 0) ? kStatusTooltipTitleBodyGap : 0.f;
            const float boxHeight = paddingY + lineH + titleBodyGapCol +
                                    static_cast<float>(std::max(0, maxBodyLines)) * lineH + paddingY;
            const float bodyInnerH =
                static_cast<float>(std::max(1, maxBodyLines)) * lineH + lineH * 0.35f;

            for (size_t i = 0; i < mStatuses.size(); ++i) {
                const auto& st = mStatuses[i];
                if (battle_ui_status_hide_from_icon_row(st.id))
                    continue;
                std::wstring desc = makeStatusTooltipText(st);
                sf::String titleStr;
                sf::String bodyStr;
                split_status_tooltip_desc(desc, titleStr, bodyStr);
                const float boxLeft = boxRight - maxBoxWidth;
                sf::RectangleShape bg(sf::Vector2f(maxBoxWidth, boxHeight));
                bg.setPosition(sf::Vector2f(boxLeft, boxTop));
                bg.setFillColor(sf::Color(40, 35, 45, 230));
                bg.setOutlineColor(sf::Color(150, 140, 120));
                bg.setOutlineThickness(1.f);
                window.draw(bg);
                const float textLeft = boxLeft + paddingX;
                float y = boxTop + paddingY;
                if (!titleStr.isEmpty()) {
                    sf::Text titleT(cn, titleStr, kStatusTooltipCharPt);
                    titleT.setFillColor(kStatusTooltipTitleGold);
                    const sf::FloatRect tb = titleT.getLocalBounds();
                    titleT.setPosition(sf::Vector2f(textLeft - tb.position.x, y - tb.position.y));
                    window.draw(titleT);
                }
                y += lineH;
                if (!bodyStr.isEmpty()) {
                    y += kStatusTooltipTitleBodyGap;
                    draw_wrapped_text(window, cn, bodyStr, kStatusTooltipCharPt,
                                      sf::Vector2f(textLeft, y), innerW, bodyInnerH, kStatusTooltipPlain);
                }
                boxTop += boxHeight + boxGapY;
            }
        }

        // 受击伤害数字：
        // - 玩家受击：仍显示在玩家左侧
        // - 怪物受击：从怪物模型处“蹦出”红色数字，随后缓缓变小并淡出
        // 同一目标多个伤害错开出现：用 frames_remaining 推断“入队时长”，第 i 个需等待 i*12 帧后才显示（纯 UI 逻辑，不改结构体）
        constexpr int DAMAGE_DISPLAY_DURATION = 180;  // 与 BattleEngine 一致
        constexpr int STAGGER_FRAMES = 12;            // 同目标相邻数字间隔约 0.2 秒
        const float damageNumSize = 42.f;  // 伤害数字的字号（字体大小）
        const float playerDamageX = playerCenterX - MODEL_PLACEHOLDER_W * 0.5f - 50.f;  // 玩家受伤数字显示在玩家模型左侧一点
        const float damageY = modelCenterY - 20.f;  // 伤害数字的垂直位置（接近模型中心略偏上）
        const float offsetStepX = 16.f, offsetStepY = 12.f;  // 每个后续伤害的错位步长（玩家左、怪物右；都略向下）
        std::map<std::pair<bool, int>, int> targetIndex;  // 按目标统计当前是第几个（用于错位与延迟）
        for (const auto& ev : s.pendingDamageDisplays) {  // 遍历当前所有待显示的伤害事件
        const auto key = std::make_pair(ev.is_player, ev.monster_index);
        const int idx = targetIndex[key]++;  // 该目标下第几个（0,1,2...）
        const int age = DAMAGE_DISPLAY_DURATION - ev.frames_remaining;  // 入队已过帧数
        const int ageDisplay = age - idx * STAGGER_FRAMES;
        if (ageDisplay < 0) continue;  // 未到显示时机，跳过（错开出现）
        const int displayFrames = std::max(1, DAMAGE_DISPLAY_DURATION - idx * STAGGER_FRAMES);
        float t01 = static_cast<float>(ageDisplay) / static_cast<float>(displayFrames);
        if (t01 < 0.f) t01 = 0.f;
        if (t01 > 1.f) t01 = 1.f;
        float dx = 0.f, dy = 0.f;
        if (idx > 0) {  // 第 2 个及以后：玩家往左下方叠，怪物往右下方叠
            if (ev.is_player) { dx = -idx * offsetStepX; dy = idx * offsetStepY; }
            else { dx = idx * offsetStepX; dy = idx * offsetStepY; }
        }
        char buf[16];  // 存放数值字符串的缓冲区
        std::snprintf(buf, sizeof(buf), "%d", ev.amount);  // 把伤害数值格式化为字符串
        sf::Text dmgText(font_, buf, static_cast<unsigned>(damageNumSize));  // 创建 SFML 文本对象显示伤害数字
        dmgText.setStyle(sf::Text::Bold);  // 使用粗体，让数字更醒目
        dmgText.setFillColor(sf::Color(255, 70, 60));  // 数字填充颜色：偏红
        dmgText.setOutlineThickness(3.f);  // 数字描边厚度：3 像素
        dmgText.setOutlineColor(sf::Color(70, 10, 10));  // 数字描边颜色：深红，增强对比度
        const sf::FloatRect db = dmgText.getLocalBounds();  // 获取文本本地边界，用于计算中心点
        dmgText.setOrigin(sf::Vector2f(db.position.x + db.size.x * 0.5f, db.position.y + db.size.y * 0.5f));  // 将原点设置为文本中心，便于居中放置
        if (ev.is_player) {  // 如果是玩家受伤事件
            dmgText.setPosition(sf::Vector2f(playerDamageX + dx, damageY + dy));  // 在玩家左侧，多数字往左下方叠
            window.draw(dmgText);  // 绘制玩家受伤数字
        } else if (ev.monster_index >= 0 && static_cast<size_t>(ev.monster_index) < monsterModelRects_.size()) {  // 否则是怪物受伤，且索引有效
            const sf::FloatRect& mr = monsterModelRects_[static_cast<size_t>(ev.monster_index)];
            const float mx = mr.position.x + mr.size.x * 0.5f;
            const float my = mr.position.y + mr.size.y * 0.35f; // 略靠上：更像“从身上蹦出来”

            // 弧线蹦出→下坠：让时间更快走完，从而更快消失
            float tf = t01 * 1.55f; // >1 表示更快结束
            if (tf > 1.f) tf = 1.f;
            const float e = 1.f - (1.f - tf) * (1.f - tf); // easeOutQuad

            // 轨迹：x 方向轻微弧线偏移，y 方向先上后下（抛物线）
            const float side = (idx % 2 == 0) ? 1.f : -1.f;
            const float arcX = side * (10.f + 16.f * e) + side * 6.f * std::sin(3.1415926f * e);
            // 注意：最高点高度约为 up^2/(4*fall)。如果 up 与 fall 等比例增大，肉眼提升会不明显。
            // 这里用更高的 up + 相对较小的 fall，让“蹦起高度”更直观。
            const float up = 260.f;    // 上冲强度（越大越高）
            const float fall = 180.f;  // 下坠强度（越大越快落）
            const float yArc = -(up * tf) + (fall * tf * tf); // tf≈0.28 附近到达最高点，之后下坠

            // 缩放与透明：更快淡出/缩小
            const float scale = 1.42f - 0.92f * e;
            float alpha01 = 1.f - tf;
            alpha01 = alpha01 * alpha01 * alpha01; // 更快收尾
            const auto a = static_cast<std::uint8_t>(std::clamp(alpha01, 0.f, 1.f) * 255.f);
            dmgText.setFillColor(sf::Color(255, 60, 60, a));
            dmgText.setOutlineColor(sf::Color(70, 10, 10, a));
            dmgText.setScale(sf::Vector2f(scale, scale));
            dmgText.setPosition(sf::Vector2f(mx + dx + arcX, my + dy + yArc));
            window.draw(dmgText);  // 绘制怪物受伤数字
        }
    }
}

    void BattleUI::flushPendingBattleStatusIcons_(sf::RenderWindow& window) {
        if (pendingBattleStatusIcons_.empty() || !fontLoaded_)
            return;
        char buf[32];
        for (const PendingBattleStatusIcon& p : pendingBattleStatusIcons_) {
            const sf::Texture* const ptex = textureForStatusEffect_(p.id);
            if (ptex) {
                sf::Sprite sp(*ptex);
                const sf::Vector2u ts = ptex->getSize();
                const float sx = ts.x ? (p.iconSz / static_cast<float>(ts.x)) : 1.f;
                const float sy = ts.y ? (p.iconSz / static_cast<float>(ts.y)) : 1.f;
                sp.setScale(sf::Vector2f(sx, sy));
                sp.setPosition(sf::Vector2f(p.x, p.y));
                window.draw(sp);
            } else {
                sf::RectangleShape icon(sf::Vector2f(p.iconSz, p.iconSz));
                icon.setPosition(sf::Vector2f(p.x, p.y));
                if (p.monsterPalette) {
                    icon.setFillColor(sf::Color(85, 75, 95));
                    icon.setOutlineColor(sf::Color(190, 180, 160));
                } else {
                    icon.setFillColor(sf::Color(90, 85, 100));
                    icon.setOutlineColor(sf::Color(180, 170, 150));
                }
                icon.setOutlineThickness(1.f);
                window.draw(icon);
            }
            if (battle_ui_status_show_stack_number(p.id)) {
                std::snprintf(buf, sizeof(buf), "%d", p.stacks);
                sf::Text stText(font_, buf, BATTLE_STATUS_STACK_FONT_SZ);
                if (battle_ui_status_positive_stack_color(p.id)) {
                    if (p.stacks >= 0)
                        stText.setFillColor(sf::Color(120, 230, 120));
                    else
                        stText.setFillColor(sf::Color(230, 120, 120));
                } else {
                    stText.setFillColor(sf::Color::White);
                }
                const sf::FloatRect textBounds = stText.getLocalBounds();
                stText.setOrigin(sf::Vector2f(textBounds.size.x, textBounds.size.y));
                stText.setPosition(sf::Vector2f(p.x + p.iconSz, p.y + p.iconSz));
                window.draw(stText);
            }
        }
    }

    // 底栏布局：抽牌堆左下 | 能量圆其右上方 | 手牌扇形居中 | 结束回合 | 弃牌堆右下 | 消耗堆在弃牌上方
    void BattleUI::drawBottomBar(sf::RenderWindow& window, const BattleStateSnapshot& s) {
        const float barY = height_ * BOTTOM_BAR_Y_RATIO;  // 底栏起始 Y（未直接使用）
        char buf[24];

        auto draw_pile_hover_rect = [&](float bx, float by, float bw, float bh, float hov,
                                        sf::Color fill, sf::Color outline, float th) -> std::array<float, 4> {
            constexpr float kLift = 7.f;
            const float sc = 1.f + 0.11f * hov;
            const float nw = bw * sc;
            const float nh = bh * sc;
            const float nx = bx + bw * 0.5f - nw * 0.5f;
            const float ny = by + bh * 0.5f - nh * 0.5f - kLift * hov;
            sf::RectangleShape r(sf::Vector2f(nw, nh));
            r.setPosition(sf::Vector2f(nx, ny));
            r.setFillColor(sf::Color(
                ui_hover_lighten_byte(fill.r, hov),
                ui_hover_lighten_byte(fill.g, hov),
                ui_hover_lighten_byte(fill.b, hov),
                fill.a));
            r.setOutlineColor(sf::Color(
                ui_hover_lighten_byte(outline.r, hov),
                ui_hover_lighten_byte(outline.g, hov),
                ui_hover_lighten_byte(outline.b, hov),
                outline.a));
            r.setOutlineThickness(th);
            window.draw(r);
            return {nx, ny, nw, nh};
        };

        // 抽牌堆：往左 4px
        const float drawPileX = SIDE_MARGIN + PILE_CENTER_OFFSET - 4.f;  // 左下角
        const float drawPileY = height_ - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
        const auto drawPileGeom =
            draw_pile_hover_rect(drawPileX, drawPileY, PILE_ICON_W, PILE_ICON_H, hover_draw_pile_,
                                 sf::Color(120, 50, 50), sf::Color(180, 80, 80), 2.f);
        const float drawNumCx = drawPileGeom[0] + drawPileGeom[2];
        const float drawNumCy = drawPileGeom[1] + drawPileGeom[3];
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
        const float energyCenterY = static_cast<float>(height_) - ENERGY_CENTER_Y_BL;  // 从底往上算
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

        // 手牌：弧心在屏幕中心下方，弧顶距底边 220；总角度由展示宽度反推，相邻牌心间距约 145
        const size_t handCount = s.hand.size();
        constexpr float DEG2RAD = 3.14159265f / 180.f;
        const float pivotX = static_cast<float>(width_) * 0.5f;  // 弧心 x（屏幕中心）
        const float pivotY = static_cast<float>(height_) + HAND_PIVOT_Y_BELOW;  // 弧心 y（屏幕下方）
        const float arcTopCenterY = static_cast<float>(height_) - HAND_ARC_TOP_ABOVE_BOTTOM + CARD_H * 0.5f;  // 弧顶中心 Y
        const float arcRadius = pivotY - arcTopCenterY;  // 弧半径
        float handFanSpanDeg = (handCount > 1)
            ? (static_cast<float>(handCount - 1) * HAND_CARD_DISPLAY_STEP / arcRadius * (180.f / 3.14159265f))  // 根据展示宽度反推角度
            : 0.f;
        if (handFanSpanDeg > HAND_FAN_SPAN_MAX_DEG) handFanSpanDeg = HAND_FAN_SPAN_MAX_DEG;  // 限制最大弧度
        const float angleStepDeg = (handCount > 1) ? (handFanSpanDeg / static_cast<float>(handCount - 1)) : 0.f;
        hand_card_rects_.clear();
        hand_card_rect_indices_.clear();
        hand_preview_glossary_entries_.clear();
        hand_preview_glossary_active_ = false;
        hand_card_rects_.reserve(handCount);
        hand_card_rect_indices_.reserve(handCount);

        const bool handSelectReshuffleFan = card_select_active_ && card_select_use_hand_area_;
        std::vector<int> selectFanVis;
        std::vector<sf::Vector2f> selectFanCenters;
        std::vector<float> selectFanAngles;
        if (handSelectReshuffleFan)
            compute_card_select_hand_fan_(s, card_select_selected_indices_, selectFanVis, selectFanCenters, selectFanAngles);

        auto getCardPos = [&](size_t idx, bool addHoverLift, float& out_cx, float& out_cy, float& out_angleDeg) {
            const float angleDeg = handCount > 1 ? (static_cast<float>(idx) - (handCount - 1) * 0.5f) * angleStepDeg : 0.f;  // 居中分布
            const float rad = angleDeg * DEG2RAD;
            out_cx = pivotX + arcRadius * std::sin(rad);
            out_cy = pivotY - arcRadius * std::cos(rad);
            if (addHoverLift) out_cy -= 28.f;       // 悬停时上浮 28 像素
            out_angleDeg = angleDeg;
            };

        int hoverIndex = -1;                        // 当前悬停的手牌下标（-1 表示无）
        // 若当前没有选中牌，则根据鼠标位置计算 hover；选中后 hover 不再影响展示
        if (selectedHandIndex_ < 0) {
            if (handSelectReshuffleFan) {
                for (int j = static_cast<int>(selectFanVis.size()) - 1; j >= 0; --j) {
                    const float cx_i = selectFanCenters[static_cast<size_t>(j)].x;
                    const float cy_i = selectFanCenters[static_cast<size_t>(j)].y;
                    const float cardLeft = cx_i - CARD_W * 0.5f;
                    const float cardTop = cy_i - CARD_H * 0.5f;
                    if (mousePos_.x >= cardLeft && mousePos_.x <= cardLeft + CARD_W && mousePos_.y >= cardTop && mousePos_.y <= cardTop + CARD_H) {
                        hoverIndex = selectFanVis[static_cast<size_t>(j)];
                        break;
                    }
                }
            } else {
                for (int i = static_cast<int>(handCount) - 1; i >= 0; --i) {
                    float cx_i, cy_i, angleDeg;
                    getCardPos(static_cast<size_t>(i), false, cx_i, cy_i, angleDeg);
                    const float cardLeft = cx_i - CARD_W * 0.5f;
                    const float cardTop = cy_i - CARD_H * 0.5f;
                    if (mousePos_.x >= cardLeft && mousePos_.x <= cardLeft + CARD_W && mousePos_.y >= cardTop && mousePos_.y <= cardTop + CARD_H) {
                        // 普通出牌：所有手牌都可以悬停预览；是否可打出在点击时判断
                        hoverIndex = i;
                        // 左键点击时开始选中该牌（由 handleEvent 设置 selectedHandIndex_）
                        break;
                    }
                }
            }
            // 若本帧没有显式选中牌且有 hover，按 hover 行为处理
            if (!card_select_active_ && hoverIndex >= 0 && selectedHandIndex_ < 0 && sf::Mouse::isButtonPressed(sf::Mouse::Button::Left)) {
                if (!hand_index_playable_now(s, hoverIndex)) {
                    const CardData* cd = get_card_by_id(s.hand[static_cast<size_t>(hoverIndex)].id);
                    if (cd && (cd->unplayable || cd->cost == -2))
                        show_center_tip(L"无法打出", 1.2f);
                    else
                        show_center_tip(L"能量不足", 1.2f);
                    return;
                }
                selectedHandIndex_ = hoverIndex;
                // 选中时即进入“出牌瞄准/选择目标”阶段，根据卡牌类型决定是否需要敌人目标
                selectedCardTargetsEnemy_ = card_targets_enemy(s, static_cast<size_t>(hoverIndex));  // 攻击牌/需目标牌=true
                isAimingCard_ = true;
                // 以原位下这张牌的默认中心为跟随区域中心
                float cx, cy, ang;
                getCardPos(static_cast<size_t>(hoverIndex), false, cx, cy, ang);
                selectedCardOriginPos_ = sf::Vector2f(cx, cy);
                selectedCardIsFollowing_ = true;
                selectedCardInsideOriginRect_ = true;
            }
        }

        auto drawOneCard = [&](size_t idx, bool isHover, bool isSelected, bool useFanOverride, float fanCx, float fanCy, float fanAng) {
            float cx_i, cy_i, angleDeg;
            if (useFanOverride && !isSelected) {
                cx_i = fanCx;
                cy_i = fanCy;
                angleDeg = fanAng;
            } else {
                getCardPos(idx, isHover && !isSelected, cx_i, cy_i, angleDeg);
            }
            float w, h;
            if (isSelected) {
                // 选中牌：根据是否需要敌人目标决定行为（敌人目标：原位内跟随/离开后固定；自选：始终跟随）
                const float halfW = CARD_W * 0.5f;
                const float halfH = CARD_H * 0.5f;
                sf::FloatRect followRect(           // 原位矩形，用于判断是否"离开"
                    sf::Vector2f(selectedCardOriginPos_.x - halfW, selectedCardOriginPos_.y - halfH),
                    sf::Vector2f(CARD_W, CARD_H));
                bool inside = followRect.contains(mousePos_);
                selectedCardInsideOriginRect_ = inside;  // 供自选目标牌判断是否高亮玩家

                if (selectedCardTargetsEnemy_) {    // 敌人目标牌
                    // 敌人目标牌：在原位矩形内以预览大小跟随鼠标；离开后回到默认大小固定在屏幕中下方，并进入瞄准阶段
                    if (inside) {                  // 仍在原位：预览大小跟随鼠标
                        selectedCardIsFollowing_ = true;
                        cx_i = mousePos_.x;
                        cy_i = mousePos_.y;
                        w = CARD_PREVIEW_W;
                        h = CARD_PREVIEW_H;
                    }
                    else {                         // 离开原位：固定大小在屏幕中下方，进入瞄准
                        selectedCardIsFollowing_ = false;
                        cx_i = static_cast<float>(width_) * 0.5f;
                        cy_i = static_cast<float>(height_) - CARD_PREVIEW_BOTTOM_ABOVE - CARD_H * 0.5f;
                        w = CARD_W;
                        h = CARD_H;
                    }
                }
                else {                             // 自选（玩家）目标牌
                    selectedCardIsFollowing_ = true;
                    cx_i = mousePos_.x;
                    cy_i = mousePos_.y;
                    w = CARD_PREVIEW_W;
                    h = CARD_PREVIEW_H;
                }
                angleDeg = 0.f;
            }
            else if (isHover) {                     // 仅悬停：预览大小、下边距底（手牌区选牌时保留在扇形上，避免误判）
                if (useFanOverride && !isSelected) {
                    w = CARD_W;
                    h = CARD_H;
                } else {
                    w = CARD_PREVIEW_W;
                    h = CARD_PREVIEW_H;
                    angleDeg = 0.f;                     // 预览时旋转归零
                    cy_i = static_cast<float>(height_) - CARD_PREVIEW_BOTTOM_ABOVE - h * 0.5f;
                }
            }
            else {
                w = CARD_W;
                h = CARD_H;
            }

            // 记录选中牌当前中心位置，供瞄准箭头起点使用
            if (isSelected) {
                selectedCardScreenPos_ = sf::Vector2f(cx_i, cy_i);
            }

            sf::Transform tr;
            tr.translate(sf::Vector2f(cx_i, cy_i));
            tr.rotate(sf::degrees(angleDeg));
            tr.translate(sf::Vector2f(-w * 0.5f, -h * 0.5f));
            sf::RenderStates states(tr);

            const CardInstance& inst = s.hand[idx];
            const bool fanSmallHover = useFanOverride && !isSelected && isHover;
            const bool wantGlossarySidebar =
                (!isSelected && isHover && !fanSmallHover) || (isSelected && w >= CARD_PREVIEW_W * 0.92f);
            std::vector<CardGlossaryEntry>* glossOut =
                wantGlossarySidebar ? &hand_preview_glossary_entries_ : nullptr;
            drawDetailedCardAt(window, inst.id, 0.f, 0.f, w, h,
                               sf::Color(180, 50, 45), isHover ? 12.f : 8.f, states, &s, &inst, glossOut);
            if (glossOut && !hand_preview_glossary_entries_.empty()) {
                const sf::Transform& ct = states.transform;
                auto tp = [&](float x, float y) { return ct.transformPoint(sf::Vector2f(x, y)); };
                const sf::Vector2f q0 = tp(0.f, 0.f);
                const sf::Vector2f q1 = tp(w, 0.f);
                const sf::Vector2f q2 = tp(w, h);
                const sf::Vector2f q3 = tp(0.f, h);
                const float minx = std::min(std::min(q0.x, q1.x), std::min(q2.x, q3.x));
                const float maxx = std::max(std::max(q0.x, q1.x), std::max(q2.x, q3.x));
                const float miny = std::min(std::min(q0.y, q1.y), std::min(q2.y, q3.y));
                const float maxy = std::max(std::max(q0.y, q1.y), std::max(q2.y, q3.y));
                hand_preview_glossary_card_screen_ =
                    sf::FloatRect(sf::Vector2f(minx, miny), sf::Vector2f(maxx - minx, maxy - miny));
                hand_preview_glossary_active_ = true;
            }
            };

        if (handSelectReshuffleFan) {
            for (size_t j = 0; j < selectFanVis.size(); ++j) {
                const int hi = selectFanVis[j];
                if (pile_draw_anim_hiding_.count(s.hand[static_cast<size_t>(hi)].instanceId)) continue;
                const float cx_i = selectFanCenters[j].x;
                const float cy_i = selectFanCenters[j].y;
                hand_card_rects_.emplace_back(
                    sf::Vector2f(cx_i - CARD_W * 0.5f, cy_i - CARD_H * 0.5f),
                    sf::Vector2f(CARD_W, CARD_H));
                hand_card_rect_indices_.push_back(hi);
            }
            for (size_t j = 0; j < selectFanVis.size(); ++j) {
                const int hi = selectFanVis[j];
                if (pile_draw_anim_hiding_.count(s.hand[static_cast<size_t>(hi)].instanceId)) continue;
                if (hi == selectedHandIndex_) continue;
                const bool isHov = hoverIndex >= 0 && hoverIndex == hi;
                float cx = selectFanCenters[j].x;
                float cy = selectFanCenters[j].y;
                const float ang = selectFanAngles[j];
                if (isHov) cy -= 28.f;
                drawOneCard(static_cast<size_t>(hi), isHov, false, true, cx, cy, ang);
            }
        } else {
            for (size_t i = 0; i < handCount; ++i) {
                if (pile_draw_anim_hiding_.count(s.hand[i].instanceId)) continue;
                float cx_i, cy_i, angleDeg;
                getCardPos(i, false, cx_i, cy_i, angleDeg);
                hand_card_rects_.emplace_back(
                    sf::Vector2f(cx_i - CARD_W * 0.5f, cy_i - CARD_H * 0.5f),
                    sf::Vector2f(CARD_W, CARD_H));
                hand_card_rect_indices_.push_back(static_cast<int>(i));
                if (static_cast<int>(i) == selectedHandIndex_) continue;  // 选中牌最后单独绘制（置顶）
                if (hoverIndex >= 0 && static_cast<size_t>(hoverIndex) == i)
                    drawOneCard(i, true, false, false, 0.f, 0.f, 0.f);
                else
                    drawOneCard(i, false, false, false, 0.f, 0.f, 0.f);
            }
        }
        if (selectedHandIndex_ >= 0 && static_cast<size_t>(selectedHandIndex_) < handCount) {
            if (!pile_draw_anim_hiding_.count(s.hand[static_cast<size_t>(selectedHandIndex_)].instanceId))
                drawOneCard(static_cast<size_t>(selectedHandIndex_), false, true, false, 0.f, 0.f, 0.f);  // 选中牌最后画，置顶
        }
        else if (hoverIndex >= 0 && !handSelectReshuffleFan) {
            if (!pile_draw_anim_hiding_.count(s.hand[static_cast<size_t>(hoverIndex)].instanceId))
                drawOneCard(static_cast<size_t>(hoverIndex), true, false, false, 0.f, 0.f, 0.f);
        }

        // 供下一帧弃牌/消耗飞牌起点（本帧手牌中心）
        instance_hand_center_cache_.clear();
        if (handSelectReshuffleFan) {
            for (size_t j = 0; j < selectFanVis.size(); ++j) {
                const int hi = selectFanVis[j];
                float cx = selectFanCenters[j].x;
                float cy = selectFanCenters[j].y;
                if (hoverIndex >= 0 && hoverIndex == hi) cy -= 28.f;
                instance_hand_center_cache_[s.hand[static_cast<size_t>(hi)].instanceId] = sf::Vector2f(cx, cy);
            }
        } else {
            for (size_t i = 0; i < handCount; ++i) {
                float cx_i, cy_i, angleDeg;
                getCardPos(i, hoverIndex >= 0 && static_cast<int>(i) == hoverIndex, cx_i, cy_i, angleDeg);
                instance_hand_center_cache_[s.hand[i].instanceId] = sf::Vector2f(cx_i, cy_i);
            }
        }

        bool aimingAtMonster = false;               // 鼠标是否在怪物上（决定箭头颜色：红=可攻击，蓝=不可）

        // 瞄准状态且需要敌人目标：从选中牌中心到鼠标绘制弧形箭头；跟随阶段不显示
        if (selectedHandIndex_ >= 0 && isAimingCard_ && selectedCardTargetsEnemy_) {
            // 跟随阶段不显示箭头（仍在原位矩形内）
            if (selectedCardIsFollowing_) {
                // 仍在原位矩形内跟随鼠标，仅展示预览牌本身
            }
            else {
                sf::Vector2f start = selectedCardScreenPos_;
                sf::Vector2f end = mousePos_;
                sf::Vector2f mid = (start + end) * 0.5f;
                // 向上抬一点，形成弧形
                mid.y -= 60.f;

                for (const auto& r : monsterModelRects_) { if (r.contains(end)) { aimingAtMonster = true; break; } }
                sf::Color arrowColor = aimingAtMonster ? sf::Color(240, 90, 90) : sf::Color(200, 230, 255);

                // 用多条弧线叠加成更粗的实线箭头（只画到三角形基底，避免出现“两条线”）
                const int segments = 20;

                // 箭头三角形，线段不超过三角形基底
                sf::Vector2f dir = end - mid;
                float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
                if (len > 0.001f) {
                    dir /= len;
                    sf::Vector2f n(-dir.y, dir.x);
                    const float headLen = 40.f;     // 箭头长度
                    const float headWidth = 26.f;   // 箭头宽度
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
            if (aimingAtMonster) {                 // 瞄准怪物时画黄色目标框
                for (const auto& r : monsterModelRects_) {
                    if (r.contains(mousePos_)) {
                        sf::RectangleShape targetRect;
                        targetRect.setPosition(r.position);
                        targetRect.setSize(r.size);
                        targetRect.setFillColor(sf::Color(0, 0, 0, 0));
                        targetRect.setOutlineColor(sf::Color(255, 230, 120));
                        targetRect.setOutlineThickness(3.f);
                        window.draw(targetRect);
                        break;
                    }
                }
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
        const float discardX = width_ - SIDE_MARGIN - PILE_ICON_W - PILE_CENTER_OFFSET + 4.f;  // 右下角
        const float discardY = height_ - BOTTOM_MARGIN - PILE_ICON_H - 4.f;
        // 消耗堆：弃牌堆上方小图标（为空也显示，便于点击查看消耗牌）
        {
            const float exBx = discardX + 3.f;
            const float exBy = discardY - 56.f;
            const float exBw = PILE_ICON_W - 6.f;
            const float exBh = 48.f;
            const sf::Color exFill = s.exhaustPileSize > 0 ? sf::Color(70, 70, 70) : sf::Color(55, 55, 55);
            const sf::Color exOut  = s.exhaustPileSize > 0 ? sf::Color(120, 120, 120) : sf::Color(85, 85, 85);
            const auto exG = draw_pile_hover_rect(exBx, exBy, exBw, exBh, hover_exhaust_pile_, exFill, exOut, 1.f);

            std::snprintf(buf, sizeof(buf), "%d", s.exhaustPileSize);
            sf::Text exNum(font_, buf, 16);
            exNum.setFillColor(s.exhaustPileSize > 0 ? sf::Color(200, 200, 200) : sf::Color(160, 160, 160));
            exNum.setPosition(sf::Vector2f(exG[0] + 11.f, exG[1] + 8.f));
            window.draw(exNum);
        }
        const auto discardGeom = draw_pile_hover_rect(
            discardX, discardY, PILE_ICON_W, PILE_ICON_H, hover_discard_pile_,
            sf::Color(50, 50, 120), sf::Color(80, 80, 180), 2.f);
        const float discardNumCx = discardGeom[0];
        const float discardNumCy = discardGeom[1] + discardGeom[3];
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

        // 结束回合按钮：以右下为原点时中心 (280, 210)，大小 180×70
        const auto endGeom = draw_pile_hover_rect(
            endTurnButton_.position.x, endTurnButton_.position.y,
            endTurnButton_.size.x, endTurnButton_.size.y, hover_end_turn_,
            sf::Color(140, 140, 150), sf::Color(180, 180, 190), 2.f);
        const float btnCx = endGeom[0] + endGeom[2] * 0.5f;
        const float btnCy = endGeom[1] + endGeom[3] * 0.5f;
        sf::Text btnText(fontForChinese(), sf::String(L"结束回合"), 26);
        btnText.setFillColor(sf::Color::White);
        const sf::FloatRect bt = btnText.getLocalBounds();
        btnText.setOrigin(sf::Vector2f(bt.position.x + bt.size.x * 0.5f, bt.position.y + bt.size.y * 0.5f));
        btnText.setPosition(sf::Vector2f(btnCx, btnCy));
        window.draw(btnText);

        if (hand_preview_glossary_active_ && !hand_preview_glossary_entries_.empty())
            draw_card_glossary_beside_preview_(window, hand_preview_glossary_card_screen_, hand_preview_glossary_entries_);
    }

    void BattleUI::draw_card_glossary_beside_preview_(sf::RenderWindow& window, const sf::FloatRect& cardBb,
                                                      const std::vector<CardGlossaryEntry>& entries) {
        draw_card_glossary_beside_preview_impl(window, cardBb, entries, fontForChinese(), width_, height_);
    }

    // 右上角：地图、牌组、设置三个按钮；可选在下方显示“回合 N”
    void BattleUI::drawTopRight(sf::RenderWindow& window, const BattleStateSnapshot& s, bool showTurnCounter) {
        const float btnW = 58.f;
        const float btnH = 48.f;
        const float gap = 18.f;
        const float right = width_ - 28.f;          // 右边界
        const float rowY = TOP_ROW_Y;               // 按钮整体上移一点

        const int topBtnCount = hide_top_right_map_button_ ? 2 : 3;
        float x = right - (static_cast<float>(topBtnCount) * btnW + static_cast<float>(topBtnCount - 1) * gap);

        auto drawBtn = [&](const std::wstring& label, float hoverT) {
            constexpr float kLift = 5.f;
            const float sc = 1.f + 0.09f * hoverT;
            const float nw = btnW * sc;
            const float nh = btnH * sc;
            const float nx = x + btnW * 0.5f - nw * 0.5f;
            const float ny = rowY + btnH * 0.5f - nh * 0.5f - kLift * hoverT;
            sf::RectangleShape btn(sf::Vector2f(nw, nh));
            btn.setPosition(sf::Vector2f(nx, ny));
            btn.setFillColor(sf::Color(
                ui_hover_lighten_byte(70, hoverT, 38),
                ui_hover_lighten_byte(65, hoverT, 38),
                ui_hover_lighten_byte(75, hoverT, 38)));
            btn.setOutlineColor(sf::Color(
                ui_hover_lighten_byte(120, hoverT, 45),
                ui_hover_lighten_byte(115, hoverT, 45),
                ui_hover_lighten_byte(125, hoverT, 45)));
            btn.setOutlineThickness(1.f);
            window.draw(btn);
            sf::Text t(fontForChinese(), sf::String(label), 18);
            t.setFillColor(sf::Color(
                ui_hover_lighten_byte(220, hoverT, 30),
                ui_hover_lighten_byte(215, hoverT, 30),
                ui_hover_lighten_byte(225, hoverT, 30)));
            const sf::FloatRect lb = t.getLocalBounds();
            t.setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
            t.setPosition(sf::Vector2f(nx + nw * 0.5f, ny + nh * 0.5f));
            window.draw(t);
            x += btnW + gap;
        };

        if (!hide_top_right_map_button_)
            drawBtn(L"地图", hover_btn_map_);
        drawBtn(L"牌组", hover_btn_deck_);
        drawBtn(L"设置", hover_btn_settings_);

        if (showTurnCounter) {
            // 顶栏下方显示当前回合数，居中对齐在这组按钮下方
            const float groupWidth = btnW * static_cast<float>(topBtnCount) + gap * static_cast<float>(topBtnCount - 1);  // 按钮组总宽
            const float turnCenterX = right - groupWidth * 0.5f;
            const float turnRowY = rowY + btnH + 18.f;  // 回合文本再往下
            int turn = std::max(1, s.turnNumber);
            std::wstring turnStr = L"回合 " + std::to_wstring(turn);
            sf::Text turnText(fontForChinese(), sf::String(turnStr), 38);  // 字体变大
            turnText.setFillColor(sf::Color(220, 210, 200));
            const sf::FloatRect tb = turnText.getLocalBounds();
            turnText.setOrigin(sf::Vector2f(tb.position.x + tb.size.x * 0.5f, 0.f));
            turnText.setPosition(sf::Vector2f(turnCenterX, turnRowY));
            window.draw(turnText);                      // 回合数（如 "回合 3"），至少显示 1
        }
    }

std::vector<std::string> ui_get_all_known_potion_ids() {
    // 与 GameFlowController::preload_battle_ui_assets 中 kPotions 一致，便于总览与资源预载对齐
    static const std::vector<std::string> kIds = {
        "attack_potion", "block_potion", "blood_potion", "dexterity_potion", "energy_potion",
        "explosion_potion", "fear_potion", "fire_potion", "focus_potion", "poison_potion",
        "speed_potion", "steroid_potion", "strength_potion", "swift_potion", "weak_potion"};
    return kIds;
}

std::vector<std::string> ui_get_all_known_relic_ids() {
    static const std::vector<std::string> kIds = {
        "akabeko", "anchor", "art_of_war", "burning_blood", "centennial_puzzle", "ceramic_fish",
        "clockwork_boots", "copper_scales", "data_disk", "hand_drum", "happy_flower", "lantern",
        "marble_bag", "nunchaku", "orichalcum", "pen_nib", "potion_belt", "preparation_pack",
        "red_skull", "small_blood_vial", "smooth_stone", "snake_skull", "strawberry",
        "toy_ornithopter", "vajra", "relic_strength_plus"};
    return kIds;
}

std::pair<std::wstring, std::wstring> ui_get_potion_display_info(const std::string& id) {
    return get_potion_display_info(id);
}

std::pair<std::wstring, std::wstring> ui_get_relic_display_info(const std::string& id) {
    return get_relic_display_info(id);
}

} // namespace tce
