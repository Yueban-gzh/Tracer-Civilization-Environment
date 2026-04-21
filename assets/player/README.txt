玩家立绘：文件名与角色 id 一致，扩展名可为 .png / .jpg / .jpeg
示例：
  Ironclad.png — 铁甲战士（战斗中央立绘）
  Silent.png   — 静默猎手

战斗内由 preload_battle_ui_assets / loadPlayerTexture 解析 assets/player/<id>。
角色选择页优先 assets/ui/character_select/<id>，缺省再回退本目录同名图（见 assets/ui/character_select/README.txt）。
