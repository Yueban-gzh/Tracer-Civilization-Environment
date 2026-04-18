/**
 * BattleCoreRefactor → BattleStateSnapshot 适配器实现
 */
 #include "../../include/BattleCoreRefactor/BattleCoreRefactorSnapshotAdapter.hpp"  // 适配器头文件
 #include "../../include/BattleEngine/MonsterBehaviors.hpp"                         // get_monster_intent
 
 namespace tce {
 
 BattleStateSnapshot make_snapshot_from_core_refactor(   // 将 BattleCoreRefactor 的 BattleState 转为 UI 所需的快照
     const BattleState& state,                            // 战斗引擎内部状态
     const CardSystem* card_system) {                     // 卡牌系统（手牌、牌堆等）
 
     BattleStateSnapshot s;                               // 待填充的快照
     const auto& p = state.player;                        // 玩家状态引用
 
     s.playerName   = p.playerName;                       // 玩家名字
     s.character    = p.character;                       // 职业
     s.currentHp    = p.currentHp;                       // 当前生命
     s.maxHp       = p.maxHp;                            // 最大生命
     s.block       = p.block;                            // 当前格挡
     s.gold        = p.gold;                             // 金币
     s.energy      = p.energy;                           // 当前能量
     s.maxEnergy   = p.maxEnergy;                        // 最大能量
     s.cardsPlayedThisTurn = p.cardsPlayedThisTurn;      // 本回合已打出牌数（凡庸等）
     s.stance      = p.stance;                           // 姿态
     s.potionSlotCount = p.potionSlotCount;             // 灵液槽位数
     s.potions     = p.potions;                         // 灵液列表
     s.relics      = p.relics;                           // 遗物列表
     s.playerStatuses = p.statuses;                      // 玩家状态效果（金属化、易伤等）
     s.turnNumber  = state.turnNumber;                   // 当前回合数
 
     if (card_system) {                                  // 若传入卡牌系统，则填充手牌与牌堆信息
         s.hand            = card_system->get_hand();    // 手牌
        s.drawPileSize    = card_system->get_deck_size();   // 抽牌堆张数
        s.discardPileSize = card_system->get_discard_size(); // 弃牌堆张数
        s.exhaustPileSize = card_system->get_exhaust_size(); // 消耗堆张数
        s.discardPileCardIds.clear();
        s.discardPileCardIds.reserve(card_system->get_discard_pile().size());
        for (const auto& c : card_system->get_discard_pile()) s.discardPileCardIds.push_back(c.id);
    }
 
     s.monsters.clear();                                 // 清空怪物列表
     s.monsterStatuses.clear();                          // 清空怪物状态列表
     for (size_t i = 0; i < state.monsters.size(); ++i) {   // 遍历场上怪物
         const auto& m = state.monsters[i];              // 第 i 个怪物
         MonsterInBattle snap;                            // 怪物快照
         snap.id        = m.id;                          // 怪物 ID
         snap.currentHp = m.currentHp;                   // 当前生命
         snap.maxHp     = m.maxHp;                       // 最大生命
         snap.block     = m.block;                       // 格挡
         snap.currentIntent = get_monster_intent(m.id, state.turnNumber, &m.statuses);  // 当前意图（攻击/格挡等）
         for (const auto& st : m.statuses) {             // 复制怪物状态效果
             snap.statuses.push_back(StatusInstance{st.id, st.stacks, st.duration});
         }
         s.monsters.push_back(snap);                     // 加入怪物列表
         s.monsterStatuses.push_back(snap.statuses);     // 加入怪物状态列表（供 UI 展示）
     }
 
     switch (state.phase) {                              // 根据当前阶段设置调试标签
     case BattleState::TurnPhase::PlayerTurnStart:  s.phaseDebugLabel = L"PlayerTurnStart"; break;
     case BattleState::TurnPhase::PlayerTurnEnd:    s.phaseDebugLabel = L"PlayerTurnEnd"; break;
     case BattleState::TurnPhase::EnemyTurnStart:   s.phaseDebugLabel = L"EnemyTurnStart"; break;
     case BattleState::TurnPhase::EnemyTurnActions: s.phaseDebugLabel = L"EnemyTurnActions"; break;
     case BattleState::TurnPhase::EnemyTurnEnd:     s.phaseDebugLabel = L"EnemyTurnEnd"; break;
     case BattleState::TurnPhase::Idle:
     default: s.phaseDebugLabel.clear(); break;         // Idle 或未知阶段清空
     }

     s.pendingDamageDisplays = state.pendingDamageDisplays;  // 受击伤害数字
     s.pendingPlayerBlockVfx = state.pendingPlayerBlockVfx;
     s.pendingMonsterPoisonVfx = state.pendingMonsterPoisonVfx;
     s.pendingStrengthVfx      = state.pendingStrengthVfx;

     return s;                                           // 返回完整快照
 }
 
 } // namespace tce