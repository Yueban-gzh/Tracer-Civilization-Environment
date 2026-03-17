#include "../../include/BattleCoreRefactor/PotionEffects.hpp"  // 药水效果头文件
#include "../../include/CardSystem/CardSystem.hpp"             // 抽牌需 CardSystem

namespace tce {

void apply_potion_effect(const PotionId& id, BattleState& state, int target_monster_index, CardSystem* card_system) {
    if (id == "strength_potion") {                                  // 力量药水
        StatusInstance s;                                           // 创建状态实例
        s.id       = "strength";                                    // 状态 ID：力量
        s.stacks   = 2;                                             // 层数：2
        s.duration = -1;                                            // 持续回合：-1 表示永久
        state.player.statuses.push_back(s);                         // 加入玩家状态列表
        return;
    }
    // 格挡药水：获得 12 点格挡
    if (id == "block_potion") {
        state.player.block += 12;
        return;
    }

    // 能量药水：获得 2 点能量
    if (id == "energy_potion") {
        state.player.energy += 2;                                   // 增加 2 点能量
        if (state.player.energy > state.player.maxEnergy)           // 若超过上限
            state.player.energy = state.player.maxEnergy;           // 截断到最大能量
        return;
    }

    // 虚弱药水：对指定怪物施加 3 层虚弱（需传入 target_monster_index）
    if (id == "weak_potion") {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state.monsters.size())
            return;                                                 // 目标无效则直接返回
        auto& m = state.monsters[static_cast<size_t>(target_monster_index)];
        if (m.currentHp <= 0) return;                              // 怪物已死则跳过
        bool found = false;
        for (auto& st : m.statuses) {
            if (st.id == "weak") {
                st.stacks += 3;                                     // 已有虚弱则叠加 3 层
                if (st.duration > 0) st.duration = 3;               // 刷新持续回合
                found = true;
                break;
            }
        }
        if (!found) {
            m.statuses.push_back(StatusInstance{"weak", 3, 3});     // 新建虚弱：3 层，持续 3 回合
        }
        return;
    }

    // 毒药水：对指定怪物施加 6 层中毒（需传入 target_monster_index）
    if (id == "poison_potion") {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state.monsters.size())
            return;
        auto& m = state.monsters[static_cast<size_t>(target_monster_index)];
        if (m.currentHp <= 0) return;
        bool found = false;
        for (auto& st : m.statuses) {
            if (st.id == "poison") {
                st.stacks += 6;
                found = true;
                break;
            }
        }
        if (!found) {
            m.statuses.push_back(StatusInstance{"poison", 6, -1});
        }
        return;
    }

    // 恐惧药水：对指定怪物施加 3 层易伤（需传入 target_monster_index）
    if (id == "fear_potion") {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state.monsters.size())
            return;                                                 // 目标无效则直接返回
        auto& m = state.monsters[static_cast<size_t>(target_monster_index)];
        if (m.currentHp <= 0) return;                              // 怪物已死则跳过
        bool found = false;
        for (auto& st : m.statuses) {
            if (st.id == "vulnerable") {
                st.stacks += 3;                                     // 已有易伤则叠加 3 层
                if (st.duration > 0) st.duration = 3;               // 刷新持续回合
                found = true;
                break;
            }
        }
        if (!found) {
            m.statuses.push_back(StatusInstance{"vulnerable", 3, 3});  // 新建易伤：3 层，持续 3 回合
        }
        return;
    }
    
    // 爆炸药水：对所有存活敌人造成 10 点伤害（先扣格挡，再扣生命）
    if (id == "explosion_potion") {
        const int damage = 10;                                      // 固定 10 点伤害
        for (auto& m : state.monsters) {
            if (m.currentHp <= 0) continue;                         // 已死怪物跳过
            int remaining = damage;                                 // 剩余伤害
            if (m.block >= remaining) {                             // 格挡足够吸收
                m.block -= remaining;
                remaining = 0;
            } else {
                remaining -= m.block;                              // 先扣格挡
                m.block = 0;
            }
            m.currentHp -= remaining;                               // 剩余伤害扣生命
            if (m.currentHp < 0) m.currentHp = 0;                  // 不低于 0
        }
        return;
    }

    // 迅捷药水：抽 3 张牌（需传入 card_system）
    if (id == "swift_potion") {
        if (card_system) {                                     // 若有卡牌系统
            card_system->draw_cards(3);                       // 抽 3 张牌
        }
        return;
    }

    // 鲜血药水：回复最大生命值的 20%
    if (id == "blood_potion") {
        int heal = state.player.maxHp * 20 / 100;           // 回复量 = 最大生命 × 20%
        if (heal > 0) {                                    // 避免除零或无效回复
            state.player.currentHp += heal;                // 增加当前生命
            if (state.player.currentHp > state.player.maxHp)  // 若超过上限
                state.player.currentHp = state.player.maxHp;  // 截断到最大生命
        }
        return;
    }

    // 火焰药水：对指定怪物造成 20 点伤害（需传入 target_monster_index）
    if (id == "fire_potion") {
        if (target_monster_index < 0 || static_cast<size_t>(target_monster_index) >= state.monsters.size())
            return;                                                 // 目标无效则直接返回
        auto& m = state.monsters[static_cast<size_t>(target_monster_index)];
        if (m.currentHp <= 0) return;                              // 怪物已死则跳过
        int remaining = 20;                                         // 固定 20 点伤害
        if (m.block >= remaining) {                                 // 格挡足够吸收
            m.block -= remaining;
        } else {
            remaining -= m.block;                                   // 先扣格挡
            m.block = 0;
            m.currentHp -= remaining;                               // 剩余伤害扣生命
            if (m.currentHp < 0) m.currentHp = 0;                  // 不低于 0
        }
        return;
    }

}

bool potion_requires_monster_target(const PotionId& id) {
    return id == "poison_potion" || id == "weak_potion" || id == "fear_potion" || id == "fire_potion";
}

} // namespace tce