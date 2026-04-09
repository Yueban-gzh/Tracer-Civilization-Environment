/**
 * 数据层（E）：卡牌/怪物静态数据，供 B、C 只读使用
 *
 * - CardData / MonsterData 与 docs/BC模块设计与接口.md 一致，为唯一数据源（无冗余副本）。
 * - 效果不由 E 提供（C 用卡牌 id→效果函数）；意图/伤害/格挡不由 E 提供（B 用怪物 id→行为函数）。
 * - 实现与 load 在 DataLayer.cpp：主流程调用 DataLayerImpl::load_cards/load_monsters 直接填充本命名空间存储，
 *   get_card_by_id / get_monster_by_id 供 B/C 与 DataLayer 共用。
 */
 #pragma once                                    // 防止头文件重复包含

 #include "../Common/Types.hpp"                   // CardId、MonsterId 等基础类型
 #include <string>                               // std::string
 #include <vector>                               // std::vector

 namespace tce {
 
 enum class CardType { Attack, Skill, Power, Status, Curse };  // 卡牌类型
 // 卡牌颜色（对应不同角色/无色/诅咒）
 enum class CardColor { Red, Blue, Green, Purple, Colorless, Curse };  // 卡牌颜色
 enum class Rarity { Common, Uncommon, Rare, Special };         // 稀有度
 enum class MonsterType { Normal, Elite, Boss };                 // 怪物类型
 
 struct CardData {                               // 卡牌静态数据（仅属性，不含效果逻辑）
     CardId     id;                              // 卡牌 ID（如 "strike"、"strike+"）
     std::string name;                           // 显示名称
     CardType   cardType   = CardType::Attack;   // 类型：攻击/技能/能力/状态/诅咒
     int        cost       = 0;                  // 费用
     CardColor  color      = CardColor::Colorless;  // 颜色
     Rarity     rarity     = Rarity::Common;     // 稀有度
    /** 立绘图片路径（相对项目根目录），为空则不绘制立绘 */
    std::string art;                            // 立绘资源路径（如 "assets/cards/strike.png"）
     std::string description;                    // 描述文本
     bool       exhaust    = false;              // 消耗：打出后移入消耗堆
     bool       ethereal   = false;              // 虚无：回合末未打出则消耗
     bool       innate     = false;              // 固有：开局必在手牌
     bool       retain     = false;              // 保留：回合末不弃牌
    bool       unplayable = false;              // 不可打出
    // 是否需要确认目标：true 时需要外部提供具体目标（如敌人）；
    // false 时使用默认目标（通常是玩家自身或全体）
    bool       requiresTarget = false;          // 是否需要选择目标
    /** 不可从永久牌组移除（商店删牌、事件移除等）；进阶之灾、铃铛诅咒、死灵诅咒等 */
    bool       irremovableFromDeck = false;
};
 
 struct MonsterData {                            // 怪物静态数据（仅属性，不含行为逻辑）
     MonsterId  id;                              // 怪物 ID
     std::string name;                           // 显示名称
     MonsterType type  = MonsterType::Normal;     // 类型：普通/精英/Boss
     int         maxHp = 0;                      // 最大生命
 };
 
 // E 对外接口：按 id 获取数据，未就绪时可由 Mock 实现
 const CardData*    get_card_by_id(CardId id);   // 按卡牌 ID 获取卡牌数据
 const MonsterData* get_monster_by_id(MonsterId id);  // 按怪物 ID 获取怪物数据
 /** 返回当前已加载的全部卡牌 ID（用于金手指补全等） */
 std::vector<CardId> get_all_card_ids();
 
 inline const char* to_string(Rarity r) {        // 稀有度转字符串
     switch (r) {
     case Rarity::Common: return "common";
     case Rarity::Uncommon: return "uncommon";
     case Rarity::Rare: return "rare";
     case Rarity::Special: return "special";
     default: return "common";
     }
 }
 inline const char* to_string(CardColor c) {     // 卡牌颜色转字符串
     switch (c) {
     case CardColor::Red: return "red";
     case CardColor::Blue: return "blue";
     case CardColor::Green: return "green";
     case CardColor::Purple: return "purple";
     case CardColor::Colorless: return "colorless";
     case CardColor::Curse: return "curse";
     default: return "colorless";
     }
 }
 inline const char* to_string(MonsterType t) {   // 怪物类型转字符串
     switch (t) { case MonsterType::Normal: return "normal"; case MonsterType::Elite: return "elite"; case MonsterType::Boss: return "boss"; default: return "normal"; }
 }
 
 } // namespace tce