怪物图片放置说明
================

1. 将图片放在本目录，文件名必须与「怪物 id + 扩展名」一致。
2. 怪物 id 与 data/encounters.json、data/monster_behaviors.json 中一致（不含扩展名）。
3. 支持的扩展名（与玩家/遗物/药水一致，按顺序尝试）：.png、.jpg、.jpeg（及常见大写扩展名）
4. 战斗 UI 中模型区域为 1:1 正方形，图片会等比缩放填入。

示例：
  1.4_buguguiqi.png
  1.2_liejianshusheng.png
  1.9_nongjihuijin.png
  2.7_tongjingsuizhaung.png

请将图片放在本目录（或工程内 assets/monsters），勿仅放在其它盘符路径；程序从「工程根目录/assets/monsters/」按 id 加载。

当前设计共 24 只怪，请对照 data 目录下 JSON 中的 id 命名。若与资源文件名不一致，请重命名资源或改 JSON。
