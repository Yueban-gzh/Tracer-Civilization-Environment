静态数据文件放此目录（UTF-8 编码），与 tce::CardData / tce::MonsterData 及 DataLayer 设计一致：

  cards.json    - 卡牌表（id, name, cardType, cost, rarity, description；可选 exhaust/ethereal/innate/retain/unplayable）
  monsters.json  - 怪物表（id, name, type: "normal"|"elite"|"boss", maxHp）
  events.json   - 事件表（id, title, description, options；option 含 text、next 或 result）

格式见 docs/设计与接口.md 第二节与 include/DataLayer/DataLayer.hpp。
