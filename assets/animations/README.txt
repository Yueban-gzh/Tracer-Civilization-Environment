铁甲战士攻击立绘序列帧（可选资源）
================================

放置位置：
  推荐：工程根目录下 assets/animations/Ironclad_attack/
  程序会从 exe 所在目录向上查找含 assets/ 与 data/ 的工程根，再拼出该路径（与状态图标等资源一致），
  从 build/ 子目录启动也能找到。另会尝试当前工作目录下的 assets/animations/Ironclad_attack。

文件要求：
  - 仅使用 .png（小写扩展名）
  - 每一帧一张图；播放顺序 = 按文件名字符串排序（与 assets/vfx/attack_1 相同）
  - 建议命名：000.png, 001.png, … 或 frame_000.png, frame_001.png …，保证排序稳定

导出方式（任选）：
  - Spine / AE / 剪辑软件：导出 PNG 序列到上述文件夹
  - 单张原画拆帧：自行按时间轴导出为连续编号的 PNG

说明：
  - 仅在「当前角色为 Ironclad」且「成功打出攻击牌（CardType::Attack）」时播放
  - 播放时替换战场中央玩家立绘；基准算法与其它序列 VFX 相同（约 60fps 帧间隔 + 最短整段时长），再乘以 IRONCLAD_ATTACK_ANIM_DURATION_MUL（默认 2 = 播放时长翻倍）
  - 进战斗只扫描文件名，PNG 在后续帧内分批解码上传（首帧即开始、每帧张数较多，尽快就绪；仍避免单帧一次性全载入）
  - 程序会限制每张纹理「最长边」不超过 1024 像素（BattleUI.cpp 中常量 kIroncladAttackAnimMaxTextureSidePx）；更大图会先缩小再上传 GPU；PNG 解码仍按原文件大小，若仍慢可在 Spine 导出时降分辨率
  - 绘制时相对静态立绘占位再放大约 3.45 倍（常量 IRONCLAD_ATTACK_ANIM_BOX_MUL），底边对齐后再下移 IRONCLAD_ATTACK_ANIM_ANCHOR_DOWN_EXTRA_PX，避免 Spine 画布上方留白导致人物飘高
  - 文件夹为空或不存在时，行为与原来一致（只显示静态 Ironclad 立绘）
  - 若出牌时尚未全部载入完成，本次攻击不会播放序列（下一回合再试通常已就绪）

可调常量（均在 BattleUI.cpp，设计分辨率 1920×1080 基准）：
  - IRONCLAD_ATTACK_ANIM_BOX_MUL：相对玩家模型占位框的缩放倍数，越大人物越大（与 MODEL_PLACEHOLDER_W/H 相乘得包围盒）
  - IRONCLAD_ATTACK_ANIM_ANCHOR_DOWN_EXTRA_PX：脚底锚点再向下平移的像素（+Y），解决序列画布上方留白导致「飘高」
  - IRONCLAD_ATTACK_ANIM_DURATION_MUL：在 battle_vfx_clip_duration_sec 结果上再乘的时长系数（越大铁甲中央攻击越久）
  - 铁甲 attack_1：挥剑整段结束后，沿玩家→怪身前依次播放多段剑气（IRONCLAD_SWORD_QI_* 常量，见 BattleUI.cpp）
  - kIroncladAttackAnimMaxTextureSidePx（文件靠前全局常量）：单帧纹理最长边上限，影响清晰度与加载量
  - 通用战场序列 VFX 显示边长（BattleUI.cpp 文件最前匿名命名空间）：kBattleVfxSequenceMaxDimPx（怪物 attack_1 命中）、kBattleVfxPoisonSequenceMaxDimPx（中毒）、kBattleVfxBlockSequenceMaxDimPx（玩家格挡 block_1）；数值越大特效越大
  - 水平位置沿用玩家区 playerCenterX（含前冲位移），一般不必改；若需左右微调可在 drawBattleCenter 里 ironcladAttackAnimPlaying_ 分支对 setPosition 的 x 加减偏移
