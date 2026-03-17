/**
 * Battle UI - 严格按照参考图：顶栏(名字/HP/金币/药水/球槽)、遗物行、战场、底栏(能量/抽牌/手牌/结束回合/弃牌)
 * 牌组界面：顶栏+遗物栏不变，中间展示牌堆网格，支持滚轮滚动、返回按钮关闭。
 */
#pragma once                                    // 防止头文件重复包含

#include "BattleUIData.hpp"                     // UI 数据提供者接口、快照结构
#include "../CardSystem/CardSystem.hpp"         // 卡牌实例、牌组视图
#include <SFML/Graphics.hpp>                    // 窗口、字体、图形绘制
#include <string>                               // 字符串
#include <unordered_map>                        // 怪物 id→纹理 缓存
#include <vector>                               // 动态数组

namespace tce {

struct BattleStateSnapshot;                     // 前向声明：战斗状态快照（含手牌、怪物等）

class BattleUI {
public:
    BattleUI(unsigned width, unsigned height);  // 构造：传入窗口宽高

    bool loadFont(const std::string& path);     // 加载主字体（英文/数字）
    /** 加载中文字体（用于显示中文，若未加载则用主字体可能显示为方框） */
    bool loadChineseFont(const std::string& path);
    /** 加载怪物图片：path 如 assets/monsters/cultist.png，会按 id 缓存供绘制使用 */
    bool loadMonsterTexture(const std::string& monster_id, const std::string& path);
    /** 加载遗物图标：path 如 assets/relics/burning_blood.png，按 id 缓存 */
    bool loadRelicTexture(const std::string& relic_id, const std::string& path);
    /** 加载药水图标：path 如 assets/potions/strength_potion.png，按 id 缓存 */
    bool loadPotionTexture(const std::string& potion_id, const std::string& path);
    /** 加载玩家角色图片：path 如 assets/player/Ironclad.png，按 character id 缓存供绘制使用 */
    bool loadPlayerTexture(const std::string& character_id, const std::string& path);
    /** 加载背景图：path 如 assets/backgrounds/bg.png，绘制时置于最底层（单张时用） */
    bool loadBackground(const std::string& path);
    /** 加载指定战斗的背景图：index 对应战斗序号，path 如 assets/backgrounds/bg_0.png */
    bool loadBackgroundForBattle(int index, const std::string& path);
    /** 加载意图图标：key 为 Attack/Block/Strategy/Unknown，path 如 assets/intention/Attack.png */
    bool loadIntentionTexture(const std::string& key, const std::string& path);
    /** 切换当前战斗背景：index 对应战斗序号（0=邪教徒, 1=胖地精, 2=绿虱虫, 3=红虱虫） */
    void setBattleBackground(int index);

    bool handleEvent(const sf::Event& ev, const sf::Vector2f& mousePos);  // 处理事件，返回 true 表示点击了结束回合
    void setMousePosition(sf::Vector2f pos);    // 设置鼠标位置（用于悬停高亮）

    void draw(sf::RenderWindow& window, IBattleUIDataProvider& data);  // 绘制整帧 UI

    /** 轮询一次是否有“打出牌”的请求，若有则返回手牌下标与目标怪物下标 */
    bool pollPlayCardRequest(int& outHandIndex, int& outTargetMonsterIndex);

    /** 轮询一次是否有"使用药水"的请求，若有则返回药水槽下标与目标怪物下标（-1 表示无需目标） */
    bool pollPotionRequest(int& outSlotIndex, int& outTargetMonsterIndex);

    /** 牌组界面：设置要展示的牌列表（手牌+抽牌堆+弃牌堆+消耗堆合并），并打开/关闭牌组界面 */
    void set_deck_view_cards(std::vector<CardInstance> cards);  // 设置牌组视图要展示的牌
    void set_deck_view_active(bool active);    // 打开/关闭牌组界面
    bool is_deck_view_active() const { return deck_view_active_; }  // 牌组界面是否打开中
    /** 轮询一次是否请求打开牌组界面；outMode: 1=牌组(右上角)，2=抽牌堆(左下角)，3=弃牌堆(右下角)，4=消耗堆(弃牌堆上方) */
    bool pollOpenDeckViewRequest(int& outMode);

    /** 屏幕中央短时提示（如“抽牌堆为空”），seconds 为显示秒数 */
    void showTip(std::wstring text, float seconds = 1.2f);

    /** 奖励界面：战斗胜利后显示（金币、遗物、药水、三选一卡牌、跳过、继续），参考杀戮尖塔 */
    void set_reward_screen_active(bool active);
    void set_reward_data(int gold, std::vector<std::string> card_ids,
                        std::vector<std::string> relic_ids = {},
                        std::vector<std::string> potion_ids = {});
    bool is_reward_screen_active() const { return reward_screen_active_; }
    bool pollContinueToNextBattleRequest();
    bool pollRewardCardPick(int& outCardIndex);
    /** 获取奖励卡牌列表中指定下标的卡牌 id（0~2），越界返回空串 */
    std::string get_reward_card_id_at(size_t index) const;

private:
    void drawDeckView(sf::RenderWindow& window, const BattleStateSnapshot& s);   // 绘制牌组界面（网格+牌）
    void drawTopBar(sf::RenderWindow& window, const BattleStateSnapshot& s);    // 顶部栏：名字、HP、金币、药水
    void drawRelicsRow(sf::RenderWindow& window, const BattleStateSnapshot& s); // 遗物行
    void drawRewardScreen(sf::RenderWindow& window);  // 奖励界面：胜利、金币、卡牌、继续
    void drawBattleCenter(sf::RenderWindow& window, const BattleStateSnapshot& s);  // 战场中心：玩家、怪物、意图
    void drawBottomBar(sf::RenderWindow& window, const BattleStateSnapshot& s); // 底栏：能量、手牌、结束回合、牌堆
    void drawTopRight(sf::RenderWindow& window, const BattleStateSnapshot& s);  // 右上角：牌组、抽牌堆
    void show_center_tip(std::wstring text, float seconds);  // 内部：设置中央提示（供 showTip 调用）
    void draw_center_tip(sf::RenderWindow& window);          // 内部：绘制中央提示
    void drawRelicPotionTooltip(sf::RenderWindow& window, const BattleStateSnapshot& s);  // 遗物/药水悬停提示
    bool can_pay_selected_card_cost() const;   // 当前选中的牌能量是否足够

    unsigned width_;                            // 窗口宽度
    unsigned height_;                           // 窗口高度
    sf::Font font_;                             // 主字体（英文/数字）
    bool fontLoaded_ = false;                   // 主字体是否加载成功
    sf::Font fontChinese_;                     // 中文字体
    bool fontChineseLoaded_ = false;            // 中文字体是否加载成功

    const sf::Font& fontForChinese() const { return fontChineseLoaded_ ? fontChinese_ : font_; }  // 优先用中文，否则主字体
    sf::FloatRect endTurnButton_;               // 结束回合按钮的矩形（用于点击检测）
    sf::Vector2f mousePos_ = {-9999.f, -9999.f};  // 当前鼠标位置（世界坐标）

    // --- 出牌/瞄准相关临时状态 ---
    int  selectedHandIndex_ = -1;              // 当前选中的手牌索引，-1 表示未选中
    bool isAimingCard_ = false;                 // 是否正在瞄准（选择目标）
    bool selectedCardTargetsEnemy_ = false;     // 选中牌是否需要敌人目标，否则为玩家自身

    // 模型矩形（用于点击/高亮）
    sf::FloatRect playerModelRect_;             // 玩家模型矩形（用于点击检测）
    std::vector<sf::FloatRect> monsterModelRects_;   // 每个怪物的模型矩形（用于点击、瞄准）
    std::vector<sf::FloatRect> monsterIntentRects_;   // 每个怪物的意图图标矩形

    // 等待主循环实际调用 engine.play_card 的请求
    int pendingPlayHandIndex_ = -1;            // 待打出的手牌下标
    int pendingPlayTargetMonsterIndex_ = -1;   // 待打出的目标怪物下标，-1 表示玩家自身

    // 药水使用：选中槽位、瞄准状态、待执行的请求
    int  selectedPotionSlotIndex_ = -1;         // 当前选中的药水槽下标
    bool isAimingPotion_ = false;               // 是否正在瞄准药水目标（需目标的药水）
    int  pendingPotionSlotIndex_ = -1;         // 待使用的药水槽下标
    int  pendingPotionTargetIndex_ = -1;       // 药水目标怪物下标，-1 表示无需目标
    std::vector<sf::FloatRect> potionSlotRects_;  // 药水槽矩形列表（用于点击检测）
    std::vector<sf::FloatRect> relicSlotRects_;   // 遗物槽矩形列表（用于悬停提示）

    // 选中牌“原位矩形”与跟随状态
    sf::Vector2f selectedCardOriginPos_{0.f, 0.f};  // 选中牌在 hand 中的原始位置
    bool         selectedCardIsFollowing_ = false;  // 牌是否跟随鼠标移动
    bool         selectedCardInsideOriginRect_ = true;  // 牌是否仍在原位矩形内（用于自选目标判断）

    // 选中牌当前屏幕中心位置（用于绘制瞄准箭头）
    sf::Vector2f selectedCardScreenPos_{0.f, 0.f};  // 牌跟随鼠标时的屏幕坐标

    // 最近一次绘制使用的快照（用于在事件处理阶段校验能量等 UI 侧逻辑）
    const BattleStateSnapshot* lastSnapshot_ = nullptr;  // 绘制时传入，事件处理时读取

    // 屏幕中间提示（如“能量不足”）
    sf::Clock    centerTipClock_;              // 提示计时器
    float        centerTipSeconds_ = 0.f;       // 提示剩余显示秒数
    std::wstring centerTipText_;               // 提示文本

    // 怪物图片缓存（monster_id -> texture），无图时用灰色占位矩形
    std::unordered_map<std::string, sf::Texture> monsterTextures_;
    // 遗物图标缓存（relic_id -> texture），无图时用灰色占位矩形
    std::unordered_map<std::string, sf::Texture> relicTextures_;
    // 药水图标缓存（potion_id -> texture），无图时用紫色占位矩形
    std::unordered_map<std::string, sf::Texture> potionTextures_;
    // 玩家角色图片缓存（character_id -> texture），无图时用灰色占位矩形
    std::unordered_map<std::string, sf::Texture> playerTextures_;
    // 背景图（置于最底层）：多张按战斗序号索引，无图时用 clear 色
    std::vector<sf::Texture> backgroundTextures_;
    int                     currentBackgroundIndex_ = 0;
    // 意图图标（Attack/Block/Strategy/Unknown），无图时用灰色圆球占位
    std::unordered_map<std::string, sf::Texture> intentionTextures_;

    // 牌组界面
    bool                          deck_view_active_ = false;   // 牌组界面是否打开
    std::vector<CardInstance>     deck_view_cards_;             // 牌组视图要展示的牌列表
    float                         deck_view_scroll_y_ = 0.f;   // 牌组视图纵向滚动偏移
    int                           pending_deck_view_mode_ = 0;  // 0 无，1 整个牌组，2 抽牌堆，3 弃牌堆
    sf::FloatRect                 deckViewReturnButton_;       // 牌组界面返回按钮矩形

    // 奖励界面（战斗胜利后）
    bool                          reward_screen_active_ = false;
    int                           reward_gold_ = 0;
    std::vector<std::string>      reward_card_ids_;             // 三张可选卡牌 id
    std::vector<std::string>      reward_relic_ids_;             // 本场获得的遗物 id
    std::vector<std::string>      reward_potion_ids_;            // 本场获得的药水 id
    bool                          reward_card_picked_ = false;  // 已选一张或跳过
    std::vector<sf::FloatRect>   reward_card_rects_;           // 卡牌点击区域
    sf::FloatRect                 reward_skip_rect_;           // 跳过按钮
    sf::FloatRect                 reward_continue_rect_;       // 继续按钮
    bool                          pending_continue_to_next_battle_ = false;
    int                           pending_reward_card_index_ = -2;  // -2 无，-1 跳过，0~2 选中的卡
};

} // namespace tce
