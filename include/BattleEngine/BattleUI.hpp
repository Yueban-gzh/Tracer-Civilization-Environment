/**
 * Battle UI - 严格按照参考图：顶栏(名字/HP/金币/药水/球槽)、遗物行、战场、底栏(能量/抽牌/手牌/结束回合/弃牌)
 * 牌组界面：顶栏+遗物栏不变，中间展示牌堆网格，支持滚轮滚动、返回按钮关闭。
 */
#pragma once                                    // 防止头文件重复包含

#include "BattleUIData.hpp"                     // UI 数据提供者接口、快照结构
#include "BattleStateSnapshot.hpp"              // 完整快照（成员内拷贝，避免 lastSnapshot_ 悬垂）
#include "../CardSystem/CardSystem.hpp"         // 卡牌实例、牌组视图
#include <SFML/Graphics.hpp>                    // 窗口、字体、图形绘制
#include <array>
#include <string>                               // 字符串
#include <unordered_map>                        // 怪物 id→纹理 缓存
#include <unordered_set>                        // 抽/弃牌动画：隐藏飞行中的手牌实例
#include <vector>                               // 动态数组

namespace tce {

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
    /** 加载背景图：path 须含扩展名；支持 png/jpg/jpeg/jfif（与 SFML 一致） */
    bool loadBackground(const std::string& path);
    /** 加载指定战斗的背景图：index 为槽位；path 须含扩展名 */
    bool loadBackgroundForBattle(int index, const std::string& path);
    /** 加载意图图标：key 为 Attack/Block/Strategy/Unknown，path 如 assets/intention/Attack.png */
    bool loadIntentionTexture(const std::string& key, const std::string& path);
    /** 切换当前战斗背景：0/1/2 对应三张地图配置的先秦/汉唐/宋明（由 GameFlow 按当前地图索引设定） */
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

    /** 主地图界面应隐藏右上角「地图」按钮；其它界面显示并可切换「仅浏览地图」浮层 */
    void set_hide_top_right_map_button(bool hide) { hide_top_right_map_button_ = hide; }
    /** 打开全屏地图浏览层时，屏蔽打牌/奖励等，仅保留顶栏与牌组/设置 */
    void set_map_overlay_blocks_world_input(bool block) { map_overlay_blocks_world_input_ = block; }
    /** 轮询：用户点击「地图」按钮，请求切换浏览地图浮层开/关 */
    bool pollMapBrowseToggleRequest();

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

    /** 选牌弹窗：用于卡牌效果中的“从候选牌中选择一张/多张”（当前先支持单选） */
    void set_card_select_active(bool active);
    /** candidate_hand_indices：与 card_ids 同序，表示每张候选在「当前手牌」中的下标；手牌区选牌时优先用此匹配点击，避免 instanceId 与快照不一致导致无法选中 */
    /** hide_played_hand_index：>=0 时该手牌下标视为已打出，扇区与底栏手牌绘制中均不显示（选牌界面用） */
    void set_card_select_data(std::wstring title, std::vector<std::string> card_ids, bool allow_cancel = true, bool use_hand_area = false, std::vector<InstanceId> candidate_instance_ids = {}, int required_pick_count = 1, std::vector<int> candidate_hand_indices = {}, int hide_played_hand_index = -1);
    bool is_card_select_active() const { return card_select_active_; }
    /** 轮询一次选牌结果：-1 取消，0~N-1 选中的下标 */
    bool pollCardSelectPick(int& outCardIndex);
    /** 轮询一次选牌结果（多选版）：outCancelled=true 表示取消。 */
    bool pollCardSelectResult(std::vector<int>& outCardIndices, bool& outCancelled);
    /** 获取选牌列表中指定下标的卡牌 id，越界返回空串 */
    std::string get_card_select_id_at(size_t index) const;

    /** 在确认选牌并即将打出触发牌前调用：接下来若干张「离手进入弃牌/消耗」的飞牌从屏幕中央选牌区飞出并走弧线（与 main/GameFlow 中 play_card 配对） */
    void set_pending_select_ui_pile_fly(int discard_or_exhaust_count);

    /** 仅绘制战斗顶栏（名字/HP/金币/药水）与遗物栏，用于地图/事件等全局 HUD 复用 */
    void drawGlobalHud(sf::RenderWindow& window, const BattleStateSnapshot& s);
    /** 仅绘制牌组/卡牌网格（不含顶栏/遗物栏），用于开始界面等外部复用 */
    void drawDeckViewOnly(sf::RenderWindow& window, const BattleStateSnapshot& s);

    /** 顶栏显示爬塔层数：current 为地图逻辑层（0 起），total>0 时显示「第 a 层 / b」 */
    void set_top_bar_map_floor(int current_layer_index, int total_layers = 0);

    /** 打开/关闭暂停菜单（设置面板），用于全局 HUD 与战斗界面共用 */
    void set_pause_menu_active(bool active);
    bool is_pause_menu_active() const { return pause_menu_active_; }
    /** 轮询一次暂停菜单选项：1=返回游戏 2=保存并退出 3=设置页面（进入二级设置界面） */
    bool pollPauseMenuSelection(int& outChoice);

private:
    void drawPauseMenuOverlay(sf::RenderWindow& window);  // 暂停菜单/设置界面覆盖层（战斗与全局 HUD 共用）
    void drawDeckView(sf::RenderWindow& window, const BattleStateSnapshot& s);   // 绘制牌组界面（网格+牌）
    void drawDeckViewStandalone_(sf::RenderWindow& window, const BattleStateSnapshot& s); // 不含顶栏的牌组网格（总览等）
    void updateDeckViewDetailLayout_();  // 牌组大图详情：更新卡牌、「查看升级」与左右翻牌箭头命中矩形
    void drawTopBar(sf::RenderWindow& window, const BattleStateSnapshot& s);    // 顶部栏：名字、HP、金币、药水、层数
    void drawRelicsRow(sf::RenderWindow& window, const BattleStateSnapshot& s); // 遗物行
    void drawRewardScreen(sf::RenderWindow& window);  // 奖励界面：胜利、金币、卡牌、继续
    void drawCardSelectScreen(sf::RenderWindow& window);  // 选牌弹窗
    /** 在指定矩形内绘制完整卡牌（牌组/手牌扇形/选牌/飞牌/奖励等共用）。手牌传 handSnap+handInst 显示实效费用与费用外圈；states 用于扇形旋转/缩放后的局部坐标 (x,y) 为牌左上角 */
    void drawDetailedCardAt(sf::RenderWindow& window, const std::string& card_id, float x, float y, float w, float h,
                            const sf::Color& outlineColor, float outlineThickness = 8.f,
                            const sf::RenderStates& states = sf::RenderStates::Default,
                            const BattleStateSnapshot* handSnap = nullptr,
                            const CardInstance* handInst = nullptr);
    void drawBattleCenter(sf::RenderWindow& window, const BattleStateSnapshot& s);  // 战场中心：玩家、怪物、意图
    void drawBottomBar(sf::RenderWindow& window, const BattleStateSnapshot& s); // 底栏：能量、手牌、结束回合、牌堆
    void drawTopRight(sf::RenderWindow& window, const BattleStateSnapshot& s, bool showTurnCounter = true);  // 右上角：地图/牌组/设置 + 可选回合数
    void show_center_tip(std::wstring text, float seconds);  // 内部：设置中央提示（供 showTip 调用）
    void draw_center_tip(sf::RenderWindow& window);          // 内部：绘制中央提示
    void drawRelicPotionTooltip(sf::RenderWindow& window, const BattleStateSnapshot& s);  // 遗物/药水悬停提示
    bool can_pay_selected_card_cost() const;   // 当前选中的牌能量是否足够

    /** 根据当前选中的候选下标，计算仍在手牌扇区展示的牌（手牌下标顺序）及每张的中心/角度 */
    void compute_card_select_hand_fan_(const BattleStateSnapshot& s, const std::vector<int>& selected_candidate_indices,
                                       std::vector<int>& out_vis_hand_indices,
                                       std::vector<sf::Vector2f>& out_centers, std::vector<float>& out_angles) const;

    /** 抽牌堆→手牌、手牌→弃牌/消耗 的飞牌动画（与逻辑不同步帧，仅表现层） */
    void tick_pile_card_anims_();
    void detect_pile_card_anims_(const BattleStateSnapshot& s);
    void draw_pile_card_anims_(sf::RenderWindow& window);
    /** 底栏牌堆 / 结束回合、右上角按钮、顶栏药水槽的悬停插值（每帧调用一次） */
    void update_interactive_hover_(bool bottom_bar_interactive, bool potion_slots_interactive);
    sf::Vector2f hand_fan_card_center_(size_t hand_index, size_t hand_count) const;
    void pile_pile_screen_centers_(sf::Vector2f& out_draw, sf::Vector2f& out_discard, sf::Vector2f& out_exhaust) const;
    sf::Vector2f card_select_preview_center_for_fly_() const;

    struct PileCardFlightAnim {
        CardId       card_id;
        sf::Vector2f start{};
        sf::Vector2f end{};
        float        duration_sec = 0.36f;
        sf::Clock    clock{};
        enum Kind { DrawToHand, HandToDiscard, HandToExhaust } kind = DrawToHand;
        InstanceId   instance_id = 0;
        bool         use_arc_path = false;
    };
    std::vector<PileCardFlightAnim>     pile_card_flights_;
    std::unordered_set<InstanceId>      pile_draw_anim_hiding_;       // 抽到手的牌在飞入完成前不画在手牌区
    std::unordered_map<InstanceId, sf::Vector2f> instance_hand_center_cache_; // 上一帧手牌中心，供弃牌起点
    std::vector<CardInstance>           prev_hand_for_pile_anim_;
    int                                 prev_discard_sz_for_anim_ = 0;
    int                                 prev_exhaust_sz_for_anim_ = 0;
    bool                                pile_anim_snapshot_ready_ = false;
    int                                 pending_select_ui_pile_fly_remaining_ = 0;
    bool                                pending_select_ui_force_center_fly_ = false;

    sf::Clock                           ui_hover_anim_clock_{};
    float                               hover_draw_pile_    = 0.f;
    float                               hover_discard_pile_ = 0.f;
    float                               hover_exhaust_pile_ = 0.f;
    float                               hover_end_turn_     = 0.f;
    float                               hover_btn_map_      = 0.f;
    float                               hover_btn_deck_     = 0.f;
    float                               hover_btn_settings_ = 0.f;
    std::array<float, 5>                hover_potion_slot_{};  // 顶栏药水槽悬停 0~1（最多 5 槽）

    int top_bar_map_layer_ = -1;   // 地图当前层（无当前节点时为 -1）
    int top_bar_map_total_  = 0;   // 总层数；0 表示顶栏不显示「/ 总层」

    unsigned width_;                            // 窗口宽度
    unsigned height_;                           // 窗口高度
    /** 布局常量按 1920×1080 设计；与窗口不一致时用于缩放部分固定像素控件 */
    static constexpr float kDesignWidth = 1920.f;
    static constexpr float kDesignHeight = 1080.f;
    float uiScaleX_ = 1.f;
    float uiScaleY_ = 1.f;
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

    // 最近一次绘制：将适配器快照拷贝到本对象，lastSnapshot_ 始终指向 snapshotForEvents_（避免指向 main 栈上已销毁的临时快照）
    BattleStateSnapshot        snapshotForEvents_{};
    const BattleStateSnapshot* lastSnapshot_ = nullptr;

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
    // 卡牌立绘缓存（key -> texture），用于 artPanel 内的插画
    std::unordered_map<std::string, sf::Texture> cardArtTextures_;

    // 顶栏右上角“设置”按钮对应的暂停菜单 / 设置界面状态
    bool pause_menu_active_ = false;         // 一级暂停菜单是否打开
    bool settings_panel_active_ = false;     // 二级“设置”页面是否打开
    int  pending_pause_menu_choice_ = 0;     // 待处理的选择：1=返回游戏 2=保存并退出 3=进入设置
    sf::FloatRect pauseResumeRect_;          // 暂停菜单：返回游戏按钮区域
    sf::FloatRect pauseSaveQuitRect_;        // 暂停菜单：保存并退出按钮区域
    sf::FloatRect pauseSettingsRect_;        // 暂停菜单：设置按钮区域
    sf::FloatRect settingsBackRect_;         // 设置页面：返回按钮区域
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
    // 牌组网格悬停：对“当前悬停卡牌”做平滑插值放大
    sf::Clock                     deck_view_hover_clock_{};
    int                           deck_view_hover_index_ = -1;
    float                         deck_view_hover_blend_ = 0.f; // 0~1
    /** 牌组网格中点击牌后的放大详情（再点牌面或空白关闭；「查看升级」切换原版/升级版预览） */
    bool                          deck_view_detail_active_ = false;
    CardInstance                  deck_view_detail_inst_{};
    bool                          deck_view_detail_show_upgraded_ = false;
    sf::FloatRect                 deck_view_detail_card_rect_{};
    sf::FloatRect                 deck_view_detail_upgrade_btn_rect_{};
    sf::FloatRect                 deck_view_detail_prev_btn_rect_{};
    sf::FloatRect                 deck_view_detail_next_btn_rect_{};
    int                           deck_view_detail_index_ = -1;  // 详情当前牌在 deck_view_cards_ 中的下标
    bool                          hide_top_right_map_button_ = false;
    bool                          map_overlay_blocks_world_input_ = false;
    int                           pending_map_browse_toggle_ = 0;

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

    // 通用选牌弹窗（供效果牌交互复用）
    bool                          card_select_active_ = false;
    std::wstring                  card_select_title_;
    std::vector<std::string>      card_select_ids_;
    std::vector<InstanceId>       card_select_candidate_instance_ids_;
    std::vector<int>              card_select_candidate_hand_indices_;
    std::vector<sf::FloatRect>    card_select_rects_;
    std::vector<sf::FloatRect>    hand_card_rects_;       // 当前帧手牌点击矩形（用于手牌区选牌）
    std::vector<int>              hand_card_rect_indices_; // 与 hand_card_rects_ 对齐的手牌下标
    sf::FloatRect                 card_select_cancel_rect_;
    sf::FloatRect                 card_select_confirm_rect_;
    bool                          card_select_allow_cancel_ = true;
    bool                          card_select_use_hand_area_ = false;   // true=直接在手牌区选牌；false=弹窗网格选牌
    int                           card_select_required_pick_count_ = 1;
    /** 选牌时从扇区隐藏的手牌下标（触发选牌的那张，视作已打出） */
    int                           card_select_hide_hand_index_ = -1;
    std::vector<int>              card_select_selected_indices_;
    std::vector<int>              pending_card_select_indices_;
    bool                          pending_card_select_cancelled_ = false;
    int                           pending_card_select_index_ = -2;   // -2 无，-1 取消，0~N-1 选中
    /** 手牌区多选：已选牌从扇区飞到中央的插值动画（按候选列表下标 k，与 instanceId 无关） */
    struct CardSelectPullAnim {
        int          candidateIndex = -1;
        sf::Vector2f startCenter{0.f, 0.f};
        sf::Vector2f targetCenter{0.f, 0.f};
        float        durationSec = 0.22f;
        sf::Clock    clock{};
    };
    std::vector<CardSelectPullAnim> card_select_pull_anims_;
    sf::Clock                       card_select_confirm_pulse_clock_{};
};

} // namespace tce
