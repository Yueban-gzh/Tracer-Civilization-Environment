/**
 * 金手指引擎 - 解析并执行单条命令
 * 独立于主游戏逻辑，仅被 CheatPanel 调用
 */
#pragma once

#include <string>
#include <vector>

namespace tce {

class BattleEngine;
class CardSystem;

class CheatEngine {
public:
    CheatEngine(BattleEngine* engine, CardSystem* card_system);
    /** 执行指定命令（单行），返回 1=成功 0=失败 -1=跳过(空行/注释) */
    int execute_line(const std::string& line);
    /**
     * 获取补全候选：根据已输入的命令与参数位置，返回以 prefix 开头的候选 ID/命令列表。
     * command 为第一个词（已完整或前缀），arg_index 为当前要补全的参数下标（1=第二个词，2=第三个词），
     * prefix 为当前参数已输入的前缀。返回匹配的候选（命令名或卡牌/状态 ID）。
     */
    std::vector<std::string> get_completion_candidates(const std::string& command, int arg_index, const std::string& prefix) const;
    /** 返回命令的正确用法说明，未知命令返回空串；失败时面板可显示以提示用户 */
    std::string get_command_usage(const std::string& command) const;

private:
    BattleEngine* engine_ = nullptr;
    CardSystem* card_system_ = nullptr;

    bool parse_int(const std::string& s, int& out);
    void trim(std::string& s);
};

} // namespace tce
