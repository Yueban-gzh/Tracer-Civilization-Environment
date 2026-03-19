/**
 * 金手指引擎 - 解析并执行单条命令
 * 独立于主游戏逻辑，仅被 CheatPanel 调用
 */
#pragma once

#include <string>

namespace tce {

class BattleEngine;
class CardSystem;

class CheatEngine {
public:
    CheatEngine(BattleEngine* engine, CardSystem* card_system);
    /** 执行指定命令（单行），返回 1=成功 0=失败 -1=跳过(空行/注释) */
    int execute_line(const std::string& line);

private:
    BattleEngine* engine_ = nullptr;
    CardSystem* card_system_ = nullptr;

    bool parse_int(const std::string& s, int& out);
    void trim(std::string& s);
};

} // namespace tce
