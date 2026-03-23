========================================
  金手指使用说明 - F2 打开面板输入命令
========================================

1. 游戏中按 F2 打开金手指面板
2. 在输入框中输入命令，按 Enter 执行
3. Esc 或再次按 F2 关闭面板
4. 执行成功显示 OK，失败显示 FAIL

----------------------------------------
命令列表（id 直接填 data/*.json 中的 id）
----------------------------------------

【玩家】
  player_hp 数值           - 设置当前血量
  player_max_hp 数值       - 设置最大血量
  player_block 数值        - 设置格挡
  player_energy 数值       - 设置能量
  player_gold 数值         - 设置金币
  player_status id 层数 持续 - 设置/覆盖状态（如 strength 10 99）
  player_status_remove id  - 移除该状态

【怪物】怪物下标从 0 开始
  monster_hp 下标 数值     - 设置怪物当前血量
  monster_max_hp 下标 数值 - 设置怪物最大血量
  monster_status 下标 id 层数 持续 - 设置怪物状态
  monster_status_remove 下标 id - 移除怪物状态
  monster_kill 下标        - 直接杀死怪物

【遗物】
  add_relic id             - 添加遗物（如 vajra, burning_blood）
  remove_relic id          - 移除遗物

【药水】
  add_potion id            - 添加药水（如 strength_potion）
  remove_potion 槽位       - 移除指定槽位药水（槽位 0 1 2...）

【手牌】
  add_hand id              - 添加一张牌到手牌（如 strike, iron_wave）
  remove_hand id           - 从手牌移除第一张该 id 的牌

----------------------------------------
常用 id 示例
----------------------------------------
状态：strength, strength+, vulnerable, weak, metallicize
遗物：vajra, burning_blood, marble_bag, centennial_puzzle
药水：strength_potion, block_potion, poison_potion
卡牌：strike, strike+, defend, iron_wave, bludgeon
