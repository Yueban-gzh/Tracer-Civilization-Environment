/**
 * 怪物行为实现：
 * 1) 统一先 plan（上一回合展示意图）再 execute（下一回合执行）
 * 2) 提供注册表，允许按 monster_id 覆盖行为（后续可接 JSON 提供器）
 */
#include "../../include/BattleEngine/MonsterBehaviors.hpp"
#include "../../include/BattleEngine/BattleEngine.hpp"
#include "../../include/DataLayer/JsonParser.h"
#include <array>
#include <unordered_map>

namespace tce {

namespace {

MonsterTurnPlan plan_unknown(MonsterId, int, const std::vector<StatusInstance>*) { // 兜底规划：未知怪物时返回 Unknown 意图
    return MonsterTurnPlan{MonsterIntent{MonsterIntentKind::Unknown, 0}, 0}; // value=0、action_code=0 表示无可执行动作
}

void execute_unknown(MonsterId, int, const MonsterTurnPlan&, EffectContext&, int) {} // 兜底执行：未知怪物不做任何动作（防崩溃）

enum class JsonStepType { // JSON 行为脚本中的“单步动作类型”
    Attack, // 造成伤害（目前默认目标：玩家）
    ApplyStatusToPlayer, // 给玩家施加状态
    ApplyStatusToMonster // 给行动中的怪物施加状态
};

struct JsonBehaviorStep { // 一条“动作步骤”的运行时结构
    JsonStepType type = JsonStepType::Attack; // 动作类型，默认 Attack
    int value = 0; // 数值参数：attack 的基础伤害
    bool use_strength = false; // attack 是否叠加怪物 strength
    StatusId status_id{}; // 状态 id（如 weak / ritual）
    int stacks = 0; // 状态层数
    int duration = -1; // 状态持续回合（-1 通常表示永久）
};

struct JsonBehaviorTurn { // 一回合配置：显示意图 + 执行步骤列表
    MonsterIntent intent{}; // UI 显示的意图（kind + 可选 ui_label / ui_icon_key）
    enum class IntentLogicKind { // 意图级执行模块（每种意图对应一类执行逻辑）
        BySteps, // 兼容旧写法：按 steps 顺序执行
        Attack, // 单段攻击
        Mul_Attack, // 多段攻击：X 连击 Y 次
        Block, // 单段格挡（给自己加格挡）
        Buff_Self_Status, // 给自己上状态（例如 strength / ritual）
        Debuff_Player_Status, // 给玩家上状态（例如 weak / vulnerable）
        Attack_And_Block, // 同回合攻击 + 格挡
        Attack_And_Weak, // 攻击 + 对玩家施加虚弱（weak）
        Attack_And_Vulnerable, // 攻击 + 对玩家施加易伤（vulnerable）
        Strength_And_Player_Weak, // 自身力量 + 玩家虚弱（weak）
        Strength_And_Self_Block, // 自身力量 + 格挡（无攻击）
        Strength_And_Player_Frail, // 自身力量 + 玩家脆弱（frail）
        Player_Dexterity_Down // 对玩家施加敏捷下降（dexterity_down，回合结束扣敏捷见 StatusModifiers）
    };
    IntentLogicKind logic_kind = IntentLogicKind::BySteps; // 当前 turn 使用的意图模块
    int attack_value = 0; // Attack / Attack_And_Block 的基础伤害
    int attack_times = 1; // Mul_Attack 连击次数 Y（总伤害 X*Y）
    int block_value = 0; // Block / Attack_And_Block 的格挡值
    bool use_strength = false; // 攻击是否叠加 strength
    StatusId status_id{}; // Buff/Debuff 使用的状态 id
    int status_stacks = 0; // Buff/Debuff 状态层数
    int strength_stacks = 0; // Strength_And_Player_Weak：怪物力量层数 X
    int weak_stacks = 0; // Strength_And_Player_Weak：玩家虚弱（weak）层数 Y
    int frail_stacks = 0; // Strength_And_Player_Frail：玩家脆弱（frail）层数 Y
    int dexterity_down_stacks = 0; // Player_Dexterity_Down：玩家 dexterity_down 层数
    int status_duration = -1; // Buff/Debuff 状态持续回合
    std::vector<JsonBehaviorStep> steps; // 本回合实际执行动作（可多步）
};

struct JsonMonsterBehavior { // 某怪物完整行为：按 cycle 循环执行
    std::vector<JsonBehaviorTurn> cycle; // 回合脚本序列，turn_number 会按长度取模
};

std::unordered_map<MonsterId, JsonMonsterBehavior>& json_behavior_registry() { // JSON 行为注册表（id -> 行为）
    static std::unordered_map<MonsterId, JsonMonsterBehavior> reg; // static 保证全局唯一且仅初始化一次
    return reg; // 返回可修改引用，供加载阶段填充
}

MonsterIntentKind parse_intent_kind(const std::string& s) { // 解析 JSON 里的 intent.kind 字符串
    if (s == "Attack") return MonsterIntentKind::Attack; // 攻击意图
    if (s == "Mul_Attack" || s == "Multi_Attack") return MonsterIntentKind::Mul_Attack; // 多段攻击意图
    if (s == "Buff") return MonsterIntentKind::Buff; // 增益意图
    if (s == "Ritual") return MonsterIntentKind::Ritual; // 仪式意图（独立 UI 类型）
    if (s == "Debuff") return MonsterIntentKind::Debuff; // 减益意图
    if (s == "Block") return MonsterIntentKind::Block; // 格挡意图
    if (s == "Attack_And_Block" || s == "AttackBlock" || s == "AttackAndBlock")
        return MonsterIntentKind::Attack_And_Block; // 攻击 + 格挡（别名兼容旧 JSON）
    if (s == "Attack_And_Weak" || s == "Small_Knife" || s == "SmallKnife" || s == "Knife" || s == "AttackWeak")
        return MonsterIntentKind::Attack_And_Weak; // 攻击 + 虚弱（别名兼容）
    if (s == "Attack_And_Vulnerable" || s == "AttackVulnerable")
        return MonsterIntentKind::Attack_And_Vulnerable; // 攻击 + 易伤
    if (s == "Strength_And_Player_Weak" || s == "Strength_And_Weak")
        return MonsterIntentKind::Strength_And_Player_Weak; // 力量 + 玩家虚弱
    if (s == "Strength_And_Block" || s == "Strength_And_Self_Block")
        return MonsterIntentKind::Strength_And_Block; // 自身力量 + 格挡
    if (s == "Strength_And_Player_Frail")
        return MonsterIntentKind::Strength_And_Player_Frail; // 力量 + 玩家脆弱
    if (s == "Player_Dexterity_Down" || s == "Dexterity_Down")
        return MonsterIntentKind::Player_Dexterity_Down; // 降低玩家敏捷（dexterity_down）
    return MonsterIntentKind::Unknown; // 未识别字符串时降级为 Unknown
}

JsonBehaviorTurn::IntentLogicKind parse_intent_logic_kind(const std::string& s) { // intent.kind -> 意图执行模块
    if (s == "Attack") return JsonBehaviorTurn::IntentLogicKind::Attack;
    if (s == "Mul_Attack" || s == "Multi_Attack") return JsonBehaviorTurn::IntentLogicKind::Mul_Attack;
    if (s == "Block") return JsonBehaviorTurn::IntentLogicKind::Block;
    if (s == "Buff") return JsonBehaviorTurn::IntentLogicKind::Buff_Self_Status;
    if (s == "Ritual") return JsonBehaviorTurn::IntentLogicKind::Buff_Self_Status; // 仪式执行为“给自己上状态”
    if (s == "Debuff") return JsonBehaviorTurn::IntentLogicKind::Debuff_Player_Status;
    if (s == "Attack_And_Block" || s == "AttackBlock" || s == "AttackAndBlock")
        return JsonBehaviorTurn::IntentLogicKind::Attack_And_Block;
    if (s == "Attack_And_Weak" || s == "Small_Knife" || s == "SmallKnife" || s == "Knife" || s == "AttackWeak")
        return JsonBehaviorTurn::IntentLogicKind::Attack_And_Weak;
    if (s == "Attack_And_Vulnerable" || s == "AttackVulnerable")
        return JsonBehaviorTurn::IntentLogicKind::Attack_And_Vulnerable;
    if (s == "Strength_And_Player_Weak" || s == "Strength_And_Weak")
        return JsonBehaviorTurn::IntentLogicKind::Strength_And_Player_Weak;
    if (s == "Strength_And_Block" || s == "Strength_And_Self_Block")
        return JsonBehaviorTurn::IntentLogicKind::Strength_And_Self_Block;
    if (s == "Strength_And_Player_Frail")
        return JsonBehaviorTurn::IntentLogicKind::Strength_And_Player_Frail;
    if (s == "Player_Dexterity_Down" || s == "Dexterity_Down")
        return JsonBehaviorTurn::IntentLogicKind::Player_Dexterity_Down;
    return JsonBehaviorTurn::IntentLogicKind::BySteps;
}

bool parse_step(const DataLayer::JsonValue& v, JsonBehaviorStep& out) { // 解析单条 step（JSON -> 结构体）
    if (!v.is_object()) return false; // 非对象节点直接判无效
    const DataLayer::JsonValue* type = v.get_key("type"); // 读取 step.type
    if (!type || !type->is_string()) return false; // type 缺失或非字符串则无效
    const std::string type_s = type->as_string(); // 提取 type 文本
    if (type_s == "attack") { // 处理攻击步骤
        out.type = JsonStepType::Attack; // 标记为 Attack
        if (const DataLayer::JsonValue* p = v.get_key("value")) out.value = p->as_int(); // 可选：基础伤害
        if (const DataLayer::JsonValue* p = v.get_key("use_strength")) out.use_strength = p->as_bool(); // 可选：是否叠 strength
        return true; // attack 解析成功
    }
    if (type_s == "apply_status_to_player" || type_s == "apply_status_to_monster") { // 处理施加状态步骤
        out.type = (type_s == "apply_status_to_player") ? JsonStepType::ApplyStatusToPlayer : JsonStepType::ApplyStatusToMonster; // 区分目标
        if (const DataLayer::JsonValue* p = v.get_key("status")) out.status_id = p->as_string(); // 状态 id
        if (const DataLayer::JsonValue* p = v.get_key("stacks")) out.stacks = p->as_int(); // 层数
        if (const DataLayer::JsonValue* p = v.get_key("duration")) out.duration = p->as_int(); // 持续回合
        return !out.status_id.empty(); // status 不能为空，否则认为无效
    }
    return false; // 其他 type 暂不支持
}

bool parse_turn(const DataLayer::JsonValue& v, JsonBehaviorTurn& out) { // 解析一回合配置（intent + steps）
    if (!v.is_object()) return false; // 非对象直接无效
    const DataLayer::JsonValue* intent = v.get_key("intent"); // 读取 intent 子对象
    if (!intent || !intent->is_object()) return false; // intent 缺失则无效
    std::string kind = "Unknown"; // 默认意图种类
    int value = 0; // 默认意图数值
    if (const DataLayer::JsonValue* p = intent->get_key("kind")) kind = p->as_string(); // 读取 intent.kind
    if (const DataLayer::JsonValue* p = intent->get_key("value")) value = p->as_int(); // 读取 intent.value
    {
        MonsterIntent mi;
        mi.kind = parse_intent_kind(kind);
        mi.value = value;
        mi.ui_label = kind; // 默认直接显示 JSON 里的 intent.kind 原文
        if (const DataLayer::JsonValue* p = intent->get_key("ui_label"))
            mi.ui_label = p->as_string();
        else if (const DataLayer::JsonValue* p = intent->get_key("display_name"))
            mi.ui_label = p->as_string();
        if (const DataLayer::JsonValue* p = intent->get_key("ui_icon"))
            mi.ui_icon_key = p->as_string();
        out.intent = std::move(mi);
    }
    out.logic_kind = parse_intent_logic_kind(kind); // 解析该意图对应的执行模块
    out.attack_value = value; // 默认把 value 作为攻击值（攻击类意图可直接用）
    out.attack_times = 1;
    out.block_value = value; // 默认把 value 也作为格挡值（格挡类意图可直接用）
    if (const DataLayer::JsonValue* p = intent->get_key("attack")) out.attack_value = p->as_int(); // 复合意图可单独指定 attack
    if (const DataLayer::JsonValue* p = intent->get_key("block")) out.block_value = p->as_int(); // 复合意图可单独指定 block
    if (const DataLayer::JsonValue* p = intent->get_key("use_strength")) out.use_strength = p->as_bool(); // 攻击是否叠 strength
    if (const DataLayer::JsonValue* p = intent->get_key("times")) out.attack_times = p->as_int(); // Mul_Attack 的 Y（连击次数）
    else if (const DataLayer::JsonValue* p = intent->get_key("hits")) out.attack_times = p->as_int(); // 别名
    if (const DataLayer::JsonValue* p = intent->get_key("status")) out.status_id = p->as_string(); // Buff/Debuff 状态 id
    if (const DataLayer::JsonValue* p = intent->get_key("stacks")) out.status_stacks = p->as_int(); // Buff/Debuff 层数
    // 注意：intent 层不处理 duration，持续回合由 StatusModifiers 统一决定
    if (kind == "Ritual" && out.status_id.empty()) out.status_id = "ritual"; // Ritual 默认状态 id 固定为 ritual
    if (kind == "Ritual" && out.status_stacks == 0) out.status_stacks = (value > 0 ? value : 1); // 未写 stacks 时用 value，最少 1
    if (kind == "Attack_And_Weak" || kind == "Small_Knife" || kind == "SmallKnife" || kind == "Knife" || kind == "AttackWeak") { // 攻击+虚弱：X/Y 都由 JSON 给出
        if (out.status_id.empty()) out.status_id = "weak";
    }
    if (kind == "Attack_And_Vulnerable" || kind == "AttackVulnerable") { // 攻击+易伤：X/Y 都由 JSON 给出
        if (out.status_id.empty()) out.status_id = "vulnerable";
    }
    if (kind == "Strength_And_Player_Weak" || kind == "Strength_And_Weak") {
        out.strength_stacks = value; // X：默认用 value
        if (const DataLayer::JsonValue* p = intent->get_key("strength")) out.strength_stacks = p->as_int();
        out.weak_stacks = out.status_stacks; // Y：默认用 stacks
        if (const DataLayer::JsonValue* p = intent->get_key("weak")) out.weak_stacks = p->as_int();
        if (out.intent.ui_label == kind)
            out.intent.ui_label = kind + " " + std::to_string(out.strength_stacks) + " " + std::to_string(out.weak_stacks);
        out.intent.value = out.strength_stacks;
    }
    if (kind == "Strength_And_Block" || kind == "Strength_And_Self_Block") {
        out.strength_stacks = value; // X 力量
        if (const DataLayer::JsonValue* p = intent->get_key("strength")) out.strength_stacks = p->as_int();
        if (const DataLayer::JsonValue* p = intent->get_key("block"))
            out.block_value = p->as_int();
        else
            out.block_value = out.status_stacks; // Y 格挡：默认 stacks
        if (out.intent.ui_label == kind)
            out.intent.ui_label = kind + " " + std::to_string(out.strength_stacks) + " " + std::to_string(out.block_value);
        out.intent.value = out.strength_stacks;
    }
    if (kind == "Strength_And_Player_Frail") {
        out.strength_stacks = value;
        if (const DataLayer::JsonValue* p = intent->get_key("strength")) out.strength_stacks = p->as_int();
        out.frail_stacks = out.status_stacks;
        if (const DataLayer::JsonValue* p = intent->get_key("frail")) out.frail_stacks = p->as_int();
        if (out.intent.ui_label == kind)
            out.intent.ui_label = kind + " " + std::to_string(out.strength_stacks) + " " + std::to_string(out.frail_stacks);
        out.intent.value = out.strength_stacks;
    }
    if (kind == "Player_Dexterity_Down" || kind == "Dexterity_Down") {
        if (intent->get_key("stacks"))
            out.dexterity_down_stacks = out.status_stacks;
        else
            out.dexterity_down_stacks = value;
        if (out.intent.ui_label == kind)
            out.intent.ui_label = kind + " " + std::to_string(out.dexterity_down_stacks);
        out.intent.value = out.dexterity_down_stacks;
    }
    if ((kind == "Mul_Attack" || kind == "Multi_Attack") && out.attack_times <= 0)
        out.attack_times = 1;
    if (kind == "Mul_Attack" || kind == "Multi_Attack") {
        if (out.intent.ui_label == kind) // 未显式传 ui_label 时，自动展示 X*Y
            out.intent.ui_label = kind + " " + std::to_string(out.attack_value) + "*" + std::to_string(out.attack_times);
        out.intent.value = out.attack_value * out.attack_times; // value 展示总伤害
    }

    const DataLayer::JsonValue* steps = v.get_key("steps"); // 读取 steps 数组
    if (steps && steps->is_array()) { // 兼容旧写法：可继续用 steps
        for (const auto& step_v : steps->arr) { // 逐条解析 step
            JsonBehaviorStep step; // 临时 step 容器
            if (parse_step(step_v, step)) out.steps.push_back(std::move(step)); // 只收集成功解析的 step
        }
    }
    if (out.logic_kind == JsonBehaviorTurn::IntentLogicKind::BySteps) { // 若没命中意图模块，则要求存在 steps
        return !out.steps.empty();
    }
    return true; // 命中意图模块时，steps 可省略
}

void load_json_demo_behaviors() { // 启动时加载 monster_behaviors.json 到内存注册表
    static bool loaded = false; // 一次性加载开关
    if (loaded) return; // 已加载则跳过
    loaded = true; // 标记已加载，避免重复 IO

    const std::array<std::string, 4> candidates = { // 多工作目录兜底路径
        "data/monster_behaviors.json", // 项目根运行
        "./data/monster_behaviors.json", // 当前目录显式写法
        "../data/monster_behaviors.json", // 上一级目录运行
        "../../data/monster_behaviors.json" // 上上级目录运行
    };

    DataLayer::JsonValue root; // 根节点
    for (const auto& p : candidates) { // 依次尝试候选路径
        root = DataLayer::parse_json_file(p); // 解析 JSON 文件
        if (root.is_object()) break; // 成功读到 object 即停止尝试
    }
    if (!root.is_object()) return; // 全部路径失败则直接返回（保持空注册表）

    const DataLayer::JsonValue* monsters = root.get_key("monsters"); // 读取 monsters 顶层数组
    if (!monsters || !monsters->is_array()) return; // 结构不合法则返回

    auto& reg = json_behavior_registry(); // 取得全局注册表引用
    for (const auto& m : monsters->arr) { // 遍历每个怪物配置
        if (!m.is_object()) continue; // 非对象节点跳过
        const DataLayer::JsonValue* id_v = m.get_key("id"); // 怪物 id
        const DataLayer::JsonValue* cycle_v = m.get_key("cycle"); // 行为 cycle
        if (!id_v || !id_v->is_string() || !cycle_v || !cycle_v->is_array()) continue; // 关键字段不合法则跳过该怪
        JsonMonsterBehavior behavior; // 临时行为结构
        for (const auto& turn_v : cycle_v->arr) { // 解析每个 turn
            JsonBehaviorTurn turn; // 临时 turn
            if (parse_turn(turn_v, turn)) behavior.cycle.push_back(std::move(turn)); // 成功则加入 cycle
        }
        if (!behavior.cycle.empty()) reg[id_v->as_string()] = std::move(behavior); // 注册到 id -> behavior
    }
}

const JsonBehaviorTurn* select_json_turn(const MonsterId& id, int turn_number) { // 按怪物与回合选择当前 turn 脚本
    load_json_demo_behaviors(); // 确保 JSON 已加载
    const auto& reg = json_behavior_registry(); // 读注册表
    const auto it = reg.find(id); // 查找对应怪物
    if (it == reg.end() || it->second.cycle.empty()) return nullptr; // 未配置或 cycle 为空时返回空
    const auto& cycle = it->second.cycle; // 取到该怪的循环脚本
    const size_t idx = static_cast<size_t>((turn_number - 1) % static_cast<int>(cycle.size())); // 回合从 1 开始，转成 cycle 下标
    return &cycle[idx]; // 返回当前回合对应 turn
}

MonsterTurnPlan plan_from_json_behavior(MonsterId id, int turn_number, const std::vector<StatusInstance>*) { // JSON 规划：只返回意图
    if (const JsonBehaviorTurn* turn = select_json_turn(id, turn_number)) // 找到本回合 turn
        return MonsterTurnPlan{turn->intent, 0}; // action_code 先留 0（按 step 顺序执行）
    return plan_unknown(id, turn_number, nullptr); // 无配置时回退 Unknown
}

void execute_from_json_behavior(MonsterId id, int turn_number, const MonsterTurnPlan&, EffectContext& ctx, int monster_index) { // JSON 执行：逐 step 结算
    const JsonBehaviorTurn* turn = select_json_turn(id, turn_number); // 读取当前回合脚本
    if (!turn) return; // 无脚本则不执行
    const auto execute_attack = [&](int base, bool /*use_strength*/) { // Attack 模块（力量在 BattleEngine 统一结算）
        ctx.from_attack = true;
        ctx.deal_damage_to_player(base); // 这里只传基础伤害，避免在行为层重复叠加 buff
    };
    const auto execute_block = [&](int block) { // Block 模块
        if (block > 0) ctx.add_block_to_monster(monster_index, block);
    };
    const auto execute_buff_self = [&]() { // Buff 模块
        if (!turn->status_id.empty())
            ctx.apply_status_to_monster(monster_index, turn->status_id, turn->status_stacks, turn->status_duration);
    };
    const auto execute_debuff_player = [&]() { // Debuff 模块
        if (!turn->status_id.empty())
            ctx.apply_status_to_player(turn->status_id, turn->status_stacks, turn->status_duration);
    };

    switch (turn->logic_kind) { // 每种意图对应自己的处理模块
    case JsonBehaviorTurn::IntentLogicKind::Attack:
        execute_attack(turn->attack_value, turn->use_strength);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Mul_Attack:
        for (int i = 0; i < turn->attack_times; ++i)
            execute_attack(turn->attack_value, turn->use_strength);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Block:
        execute_block(turn->block_value);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Buff_Self_Status:
        execute_buff_self();
        return;
    case JsonBehaviorTurn::IntentLogicKind::Debuff_Player_Status:
        execute_debuff_player();
        return;
    case JsonBehaviorTurn::IntentLogicKind::Attack_And_Block:
        execute_attack(turn->attack_value, turn->use_strength);
        execute_block(turn->block_value);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Attack_And_Weak:
        execute_attack(turn->attack_value, turn->use_strength);
        if (!turn->status_id.empty() && turn->status_stacks > 0)
            ctx.apply_status_to_player(turn->status_id, turn->status_stacks, turn->status_duration);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Attack_And_Vulnerable:
        execute_attack(turn->attack_value, turn->use_strength);
        if (!turn->status_id.empty() && turn->status_stacks > 0)
            ctx.apply_status_to_player(turn->status_id, turn->status_stacks, turn->status_duration);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Strength_And_Player_Weak:
        if (turn->strength_stacks > 0)
            ctx.apply_status_to_monster(monster_index, "strength", turn->strength_stacks, turn->status_duration);
        if (turn->weak_stacks > 0)
            ctx.apply_status_to_player("weak", turn->weak_stacks, turn->status_duration);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Strength_And_Self_Block:
        if (turn->strength_stacks > 0)
            ctx.apply_status_to_monster(monster_index, "strength", turn->strength_stacks, turn->status_duration);
        execute_block(turn->block_value);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Strength_And_Player_Frail:
        if (turn->strength_stacks > 0)
            ctx.apply_status_to_monster(monster_index, "strength", turn->strength_stacks, turn->status_duration);
        if (turn->frail_stacks > 0)
            ctx.apply_status_to_player("frail", turn->frail_stacks, turn->status_duration);
        return;
    case JsonBehaviorTurn::IntentLogicKind::Player_Dexterity_Down:
        if (turn->dexterity_down_stacks > 0)
            ctx.apply_status_to_player("dexterity_down", turn->dexterity_down_stacks, turn->status_duration);
        return;
    case JsonBehaviorTurn::IntentLogicKind::BySteps:
        break; // 走下方兼容逻辑
    }

    // 兼容旧的 step 脚本写法（未指定 intent 模块时）
    for (const auto& step : turn->steps) { // 按配置顺序执行（保持可预期）
        if (step.type == JsonStepType::Attack) { // 攻击步骤
            ctx.from_attack = true; // 标记为攻击来源，供 modifier 判定（如易伤/虚弱）
            ctx.deal_damage_to_player(step.value); // 只传基础伤害，力量/虚弱/易伤在统一管线处理
            continue; // 执行下一 step
        }
        if (step.type == JsonStepType::ApplyStatusToPlayer) { // 给玩家上状态
            ctx.apply_status_to_player(step.status_id, step.stacks, step.duration); // 状态 id / 层数 / 持续回合来自 JSON
            continue; // 执行下一 step
        }
        ctx.apply_status_to_monster(monster_index, step.status_id, step.stacks, step.duration); // 否则按“给自己上状态”处理
    }
}

std::unordered_map<MonsterId, MonsterBehaviorHooks>& behavior_registry() {
    static std::unordered_map<MonsterId, MonsterBehaviorHooks> reg;
    return reg;
}

void ensure_default_behaviors_registered() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    load_json_demo_behaviors();
    // 将 data/monster_behaviors.json 中每个有 cycle 的怪物 id 注册为 JSON 行为（无需在 C++ 里逐个列举）
    const auto& jreg = json_behavior_registry();
    auto&       reg  = behavior_registry();
    for (const auto& kv : jreg) {
        if (!kv.second.cycle.empty())
            reg[kv.first] = MonsterBehaviorHooks{plan_from_json_behavior, execute_from_json_behavior};
    }
}

const MonsterBehaviorHooks* find_behavior(const MonsterId& id) {
    ensure_default_behaviors_registered();
    const auto& reg = behavior_registry();
    const auto it = reg.find(id);
    if (it == reg.end()) return nullptr;
    return &it->second;
}

} // namespace

void register_monster_behavior(const MonsterId& id, MonsterBehaviorHooks hooks) {
    ensure_default_behaviors_registered();
    behavior_registry()[id] = std::move(hooks);
}

void unregister_monster_behavior(const MonsterId& id) {
    ensure_default_behaviors_registered();
    behavior_registry().erase(id);
}

MonsterIntent get_monster_intent(MonsterId id, int turn_number, const std::vector<StatusInstance>* monster_statuses) {
    if (const MonsterBehaviorHooks* hooks = find_behavior(id)) {
        if (hooks->plan_turn) return hooks->plan_turn(id, turn_number, monster_statuses).intent;
    }
    return plan_unknown(id, turn_number, monster_statuses).intent;
}

void execute_monster_behavior(MonsterId id, int turn_number, EffectContext& ctx, int monster_index) {
    if (const MonsterBehaviorHooks* hooks = find_behavior(id)) {
        const MonsterTurnPlan planned = hooks->plan_turn ? hooks->plan_turn(id, turn_number, nullptr) : plan_unknown(id, turn_number, nullptr);
        if (hooks->execute_turn) {
            hooks->execute_turn(id, turn_number, planned, ctx, monster_index);
            return;
        }
    }
    execute_unknown(id, turn_number, plan_unknown(id, turn_number, nullptr), ctx, monster_index);
}

} // namespace tce
