静态数据文件放此目录（UTF-8 编码），与 tce::CardData / tce::MonsterData 及 DataLayer 设计一致：

  cards.json    - 卡牌表。字段：id, name, cardType, cost, rarity, description, exhaust, ethereal, innate, retain, unplayable。
                 cardType 为枚举名：Attack / Skill / Power / Status / Curse。已升级卡牌用独立 id（如 card_001+）。
  monsters.json - 怪物表。字段：id, name, type（"normal"|"elite"|"boss"）, maxHp。
  events.json   - 事件表。字段：id, title, description, options。option 含 text，以及 next（跳转子事件）或 result（type+value）。

格式详见 docs/DataLayer/DataLayer_README.md。
