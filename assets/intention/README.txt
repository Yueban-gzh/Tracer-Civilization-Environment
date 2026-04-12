怪物意图图标
-----------
优先使用 data/intent_icon_map.json：逻辑键（与 BattleUI 中 iconLookup 一致）→ 本目录下「无扩展名」主文件名（建议纯英文，避免部分环境路径编码问题）。
预载入口：GameFlowController::preload_battle_ui_assets；若映射表缺失或无效，则回退为下列英文主文件名：
  Attack.png / Block.png / Strategy.png / Unknown.png / AttackBlock.png / Ritual.png / Debuff.png / SmallKnife.png

怪物 JSON 里 intent.ui_icon 可填逻辑键或与本目录某文件主名一致（再通过懒加载解析）。

当前默认映射（可在 intent_icon_map.json 中改）：
  Attack / Mul_Attack → longsword    Block → guard    AttackBlock → longsword_guard
  Ritual / Strategy(Buff 等) → empower    Debuff → debuff    SmallKnife → small_knife
  AttackVulnerable → longsword_debuff    Unknown → heavy_debuff
  Sleep → sleep    Stun → stun

其它已归一化为英文主名的变体图（可在映射表或 ui_icon 中按需引用）：
  scythe / scythe_guard    cleaver / cleaver_debuff    axe / axe_debuff
  chopper / chopper_debuff / chopper_guard    shortsword / shortsword_debuff / shortsword_guard
  longsword_empower    empower_guard    guard_debuff    flee

从灰机维基批量拉取意图图标:
  1. 浏览器打开 https://sts.huijiwiki.com/wiki/%E6%84%8F%E5%9B%BE 并通过 Cloudflare。
  2. 将开发者工具里该站的 Cookie（整行）保存为 assets/intention/cookie.txt（勿提交，已 .gitignore）。
  3. pip install -r scripts/requirements-intent-scrape.txt
  4. python scripts/scrape_huiji_intent_icons.py --cloudscraper
     （推荐加 --cloudscraper：绕过维基 CF，且图片 CDN 需与同一会话下载，否则易 HTTP 567）
  可选: --dry-run 仅列出 URL；--html-file 已保存的页面.html 离线解析。

维基 CDN 常见为 WebP，若保存成 .png 扩展名，SFML 会报 “not of any known type, or corrupt”。
拉取后请执行: pip install Pillow && python scripts/convert_intention_webp_to_png.py
