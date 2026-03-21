// 插件接口和广播器（"事件总线"）

#pragma once                                    // 防止头文件重复包含

#include <algorithm>                            // 用于 std::stable_sort
#include <functional>                           // 用于 std::function
#include <memory>                               // 用于 std::shared_ptr
#include <utility>                              // 用于 std::move
#include <vector>                               // 用于 std::vector
#include "BattleState.hpp"                      // 战斗状态（玩家、怪物、回合阶段等）
#include "Damage.hpp"                           // 伤害包 DamagePacket

namespace tce {

struct TurnStartContext {                       // 玩家回合开始时的上下文
    int& draw_count;                            // 本回合抽牌数（可被 modifier 修改）
    int& energy;                                // 本回合能量（可被 modifier 修改）
    std::function<void(int)> deal_damage_to_player_ignoring_block;  // 对玩家造成无视格挡伤害的回调（如中毒扣血）
};

struct PlayerTurnEndContext {                   // 玩家回合结束时的上下文
    std::function<void(int monster_index, int amount)> deal_damage_to_monster_ignoring_block;  // 对怪物造成无视格挡伤害（炸弹等）
};

struct EnemyTurnContext {                       // 敌方回合时的上下文
    std::function<void(int monster_index, int amount)> deal_damage_to_monster_ignoring_block;  // 对指定怪物造成无视格挡伤害的回调
};

struct MonsterDiedContext {                     // 怪物死亡时的上下文
    std::function<void(int monster_index, int amount)> deal_damage_to_monster_ignoring_block;  // 尸体爆炸等效果用
    bool from_corpse_explosion = false;         // 防止尸体爆炸递归触发
};

struct CardPlayContext {                        // 打出卡牌时的上下文
    std::function<void(int monster_index, int amount)> deal_damage_to_monster_ignoring_block;  // 勒脖、点穴等即时伤害用
    bool is_attack = false;                     // 是否为攻击牌（双截棍等用）
};

/** 伤害结算后的上下文：玩家受到怪物攻击时，荆棘等可对攻击者造成反伤；百年积木等可抽牌 */
struct DamageAppliedContext {
    std::function<void(int monster_index, int amount)> deal_damage_to_monster_ignoring_block;  // 对怪物造成无视格挡伤害（荆棘反伤用）
    std::function<void(int n)> draw_cards;                      // 抽 n 张牌（百年积木用）
    bool damage_to_player = false;                               // 本次伤害是否施加给玩家
    int hp_damage_to_player = 0;                                 // 玩家实际扣除的生命值（格挡后）
};

/** 打出卡牌前的上下文：modifier 可设置 blocked=true 阻止打出（如缠身禁止攻击牌） */
struct CanPlayCardContext {
    const BattleState& state;                   // 战斗状态
    CardId card_id;                             // 卡牌 ID
    bool is_attack = false;                     // 是否为攻击牌
    int target_monster_index = -1;              // 目标怪物下标
    bool blocked = false;                       // modifier 设为 true 则禁止打出
};

/** 施加状态前的上下文：modifier 可设置 blocked=true 阻止施加（如人工制品免疫负面效果） */
struct StatusApplyContext {
    enum class Target { Player, Monster };
    Target target = Target::Player;             // 目标：玩家或怪物
    int monster_index = -1;                     // 目标为怪物时有效
    StatusId id;                                // 状态 ID
    int stacks = 0;                             // 层数
    int duration = 0;                           // 持续回合(-1永久)
    bool blocked = false;                       // modifier 设为 true 则取消施加
    BattleState& state;                         // 战斗状态（可读可写，如消耗人工制品层数）
};

class IBattleModifier {                         // 战斗 modifier 接口（遗物、状态效果等）
public:
    virtual ~IBattleModifier() = default;        // 虚析构，支持多态

    virtual void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) {}         // 玩家造成伤害前：力量、易伤等可修改 dmg
    virtual void on_monster_deal_damage(DamagePacket& dmg, const BattleState& state) {}        // 怪物造成伤害前：虚弱、易伤等可修改 dmg
    virtual void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) { (void)ctx; }  // 伤害结算后：荆棘反伤等
    virtual void on_turn_start_player(BattleState& state, TurnStartContext* ctx) { (void)ctx; }  // 玩家回合开始：中毒扣血、抽牌数修改等
    virtual void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) { (void)ctx; }  // 玩家回合结束：金属化、炸弹等
    virtual void on_turn_start_monsters(BattleState& state, EnemyTurnContext* ctx) { (void)ctx; }  // 敌方回合开始：怪物中毒扣血等
    virtual void on_turn_end_monsters(BattleState& state, EnemyTurnContext* ctx) { (void)ctx; }     // 敌方回合结束： shackles 减力量、duration 递减等
    virtual void on_monster_died(int monsterIndex, BattleState& state, MonsterDiedContext* ctx) {}  // 怪物死亡时：尸体爆炸等
    virtual void on_player_died(BattleState& state) {}                                         // 玩家死亡时
    virtual void on_card_played(BattleState& state, CardId card_id, int target_monster_index, CardPlayContext* ctx) { (void)card_id; (void)target_monster_index; (void)ctx; }  // 打出卡牌时：勒脖、点穴等即时伤害
    virtual void on_before_status_applied(StatusApplyContext& ctx) {}  // 施加状态前：人工制品等可设置 ctx.blocked 阻止负面效果
    virtual void on_player_gain_block(int& block, const BattleState& state) { (void)block; (void)state; }  // 玩家从卡牌获得格挡时：脆弱等可修改
    virtual void on_can_play_card(CanPlayCardContext& ctx) { (void)ctx; }  // 打出卡牌前：缠身等可禁止打出攻击牌
    virtual void on_battle_start(BattleState& state) { (void)state; }  // 战斗开始时：弹珠袋给全体敌人易伤等
    virtual void on_potion_used(BattleState& state, PotionId /*id*/) { (void)state; }  // 使用药水后：玩具扑翼飞机回复生命等
};

/**
 * 执行优先级：数值越小越先执行。按杀戮尖塔约定：
 * - 遗物(0-99)：按获得顺序
 * - 玩家状态(100-199)：力量/易伤/虚弱等
 * - 怪物状态(200-299)：毒/勒脖等
 * - 系统级(300+)：格挡清零、duration 递减
 */
constexpr int MOD_PRIORITY_RELIC      = 0;      // 遗物优先级
constexpr int MOD_PRIORITY_PLAYER_ST  = 100;    // 玩家状态优先级
constexpr int MOD_PRIORITY_MONSTER_ST = 200;    // 怪物状态优先级
constexpr int MOD_PRIORITY_SYSTEM     = 300;    // 系统级优先级

class ModifierSystem {                          // modifier 广播器：按优先级依次调用所有 modifier
public:
    void clear() { modifiers_.clear(); }         // 清空所有 modifier（如新战斗重建时）

    void add_modifier(std::shared_ptr<IBattleModifier> mod, int priority = MOD_PRIORITY_SYSTEM) {
        modifiers_.emplace_back(priority, std::move(mod));   // 加入 modifier 并记录优先级
        std::stable_sort(modifiers_.begin(), modifiers_.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });  // 按优先级升序排序
    }

    void on_player_deal_damage(DamagePacket& dmg, const BattleState& state) {
        for (auto& p : modifiers_) p.second->on_player_deal_damage(dmg, state);  // 广播：玩家造成伤害前
    }
    void on_monster_deal_damage(DamagePacket& dmg, const BattleState& state) {
        for (auto& p : modifiers_) p.second->on_monster_deal_damage(dmg, state);  // 广播：怪物造成伤害前
    }
    void on_damage_applied(const DamagePacket& dmg, BattleState& state, DamageAppliedContext* ctx) {
        for (auto& p : modifiers_) p.second->on_damage_applied(dmg, state, ctx);  // 广播：伤害结算后（荆棘反伤等）
    }
    void on_turn_start_player(BattleState& state, TurnStartContext* ctx) {
        for (auto& p : modifiers_) p.second->on_turn_start_player(state, ctx);  // 广播：玩家回合开始
    }
    void on_turn_end_player(BattleState& state, PlayerTurnEndContext* ctx) {
        for (auto& p : modifiers_) p.second->on_turn_end_player(state, ctx);  // 广播：玩家回合结束
    }
    void on_turn_start_monsters(BattleState& state, EnemyTurnContext* ctx) {
        for (auto& p : modifiers_) p.second->on_turn_start_monsters(state, ctx);  // 广播：敌方回合开始
    }
    void on_turn_end_monsters(BattleState& state, EnemyTurnContext* ctx) {
        for (auto& p : modifiers_) p.second->on_turn_end_monsters(state, ctx);  // 广播：敌方回合结束
    }
    void on_monster_died(int monsterIndex, BattleState& state, MonsterDiedContext* ctx) {
        for (auto& p : modifiers_) p.second->on_monster_died(monsterIndex, state, ctx);  // 广播：怪物死亡
    }
    void on_card_played(BattleState& state, CardId card_id, int target_monster_index, CardPlayContext* ctx) {
        for (auto& p : modifiers_) p.second->on_card_played(state, card_id, target_monster_index, ctx);  // 广播：打出卡牌
    }
    void on_player_died(BattleState& state) {
        for (auto& p : modifiers_) p.second->on_player_died(state);  // 广播：玩家死亡
    }
    void on_before_status_applied(StatusApplyContext& ctx) {
        for (auto& p : modifiers_) p.second->on_before_status_applied(ctx);  // 广播：施加状态前（人工制品可阻止）
    }
    void on_player_gain_block(int& block, const BattleState& state) {
        for (auto& p : modifiers_) p.second->on_player_gain_block(block, state);  // 广播：玩家获得格挡时（脆弱减 25%）
    }
    void on_can_play_card(CanPlayCardContext& ctx) {
        for (auto& p : modifiers_) p.second->on_can_play_card(ctx);  // 广播：打出卡牌前（缠身禁止攻击牌）
    }
    void on_battle_start(BattleState& state) {
        for (auto& p : modifiers_) p.second->on_battle_start(state);  // 广播：战斗开始时（弹珠袋等）
    }
    void on_potion_used(BattleState& state, PotionId id) {
        for (auto& p : modifiers_) p.second->on_potion_used(state, id);  // 广播：使用药水后（玩具扑翼飞机等）
    }

private:
    std::vector<std::pair<int, std::shared_ptr<IBattleModifier>>> modifiers_;  // (优先级, modifier) 列表
};

} // namespace tce