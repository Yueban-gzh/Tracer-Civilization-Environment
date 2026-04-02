# -*- coding: utf-8 -*-
"""从 data/origin_events.json 生成 docs/EventsDesign/Events.md"""
from __future__ import annotations

import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
JSON_PATH = ROOT / "data" / "origin_events.json"
OUT_PATH = Path(__file__).resolve().parent / "Events.md"

SKIP_NAMES = {
    "事件日志",
    "卡牌总览",
    "遗物收藏",
    "药水小样",
    "敌人图鉴",
    "能力查勘",
}

# 仅匹配「选项」之后的百科小节标题（勿含「选项」「战斗」「结果」等流程内用词）
STOP_BEFORE = re.compile(
    r"(?:\n{2,})(?:策略|轶事|注意|历史更新|更新历史|补丁|每周更新|特殊|漏洞|更多关于)",
    re.MULTILINE,
)


def extract_english_name(content: str) -> str:
    m = re.search(r"英文名\s*\n+\s*([^\n]+)", content)
    return m.group(1).strip() if m else "—"


def extract_location_line(content: str) -> str:
    m = re.search(r"地点\s*\n+\s*([^\n]+)", content)
    return m.group(1).strip() if m else "—"


def extract_options_section(content: str) -> str | None:
    m = re.search(r"选项\s*\n+(.*)", content, re.DOTALL)
    if not m:
        return None
    rest = m.group(1)
    cut = STOP_BEFORE.search(rest)
    if cut:
        rest = rest[: cut.start()]
    # 去掉尾部大量空行
    return rest.strip()


# 灰机 wiki 常用全角/不间断空格作「两格缩进」
_INDENT_CHARS = frozenset(" \t\u00a0\u3000")


def _leading_indent_width(line: str) -> int:
    i = 0
    while i < len(line) and line[i] in _INDENT_CHARS:
        i += 1
    return i


def _next_non_empty_idx(lines: list[str], start: int, n: int) -> int:
    j = start
    while j < n and not lines[j].strip():
        j += 1
    return j


def _prev_non_empty_idx(lines: list[str], start: int) -> int:
    j = start - 1
    while j >= 0 and not lines[j].strip():
        j -= 1
    return j


def _is_type_a_option_start(lines: list[str], k: int, n: int) -> bool:
    """标题行 + 其后第一个非空行为明显缩进（wiki 效果行）。"""
    if k >= n or not lines[k].strip() or _leading_indent_width(lines[k]) >= 2:
        return False
    j = _next_non_empty_idx(lines, k + 1, n)
    if j >= n:
        return False
    return _leading_indent_width(lines[j]) >= 2


# 无缩进「效果行」的选项标题（需白名单，否则会误判大量叙事短句为新选项）
_TYPE_C_TITLES = frozenset(
    {
        "离开",
        "战斗",
        "继续",
        "拒绝",
        "接受",
        "忽视",
        "跳过",
        "走",
        "逃跑",
        "躲藏",
        "下一页",
    }
)


def _is_type_c_option_start(lines: list[str], k: int, n: int) -> bool:
    """如「离开」：单独一行标题，正文为后续叙述，无 wiki 两格缩进效果行。"""
    if k >= n or not lines[k].strip() or _leading_indent_width(lines[k]) >= 2:
        return False
    if _is_type_a_option_start(lines, k, n):
        return False
    if lines[k].strip() not in _TYPE_C_TITLES:
        return False
    prev = _prev_non_empty_idx(lines, k)
    if prev < 0:
        return False
    return _leading_indent_width(lines[prev]) < 2


def _is_subsection_header(line: str) -> bool:
    return line.strip() in (
        "选项",
        "奖励",
        "奖品",
        "特殊选项",
        "下一个界面",
        "伤害与获得遗物的概率",
    )


def _is_option_start(lines: list[str], k: int, n: int) -> bool:
    if not lines[k].strip() or _leading_indent_width(lines[k]) >= 2:
        return False
    if _is_subsection_header(lines[k]):
        return True
    return _is_type_a_option_start(lines, k, n) or _is_type_c_option_start(lines, k, n)


def split_option_chunks(opt_sec: str) -> list[tuple[str, str]]:
    """
    先标出所有「选项起点」行索引，再按区间切成 (标题, 正文)。
    支持 NBSP/全角空格缩进；支持「离开」类无缩进效果行。
    """
    if not opt_sec:
        return []
    lines = opt_sec.strip().split("\n")
    n = len(lines)
    starts: list[int] = []
    for k in range(n):
        if _is_option_start(lines, k, n):
            starts.append(k)

    if not starts:
        return []

    chunks: list[tuple[str, str]] = []
    for idx, start in enumerate(starts):
        end = starts[idx + 1] if idx + 1 < len(starts) else n
        title = lines[start].strip()
        body = "\n".join(lines[start + 1 : end]).strip()
        chunks.append((title, body if body else "—"))

    return [(a, b) for a, b in chunks if a]


def main() -> None:
    data = json.loads(JSON_PATH.read_text(encoding="utf-8"))
    events = [e for e in data if e.get("name") not in SKIP_NAMES]

    lines: list[str] = []
    lines.append("# 事件设计一览（来源：origin_events.json）")
    lines.append("")
    lines.append(
        "本文档由 `data/origin_events.json` **自动生成**，请勿手工大段修改正文表格；"
        "请改 JSON 后运行：`python docs/EventsDesign/gen_events_md.py`"
    )
    lines.append("")
    lines.append("## 1. 事件总览")
    lines.append("")
    lines.append("| 序号 | 事件名 | 英文名 | 出现阶段/条件（摘录） | 原文链接 |")
    lines.append("|-----|--------|--------|----------------------|----------|")

    for i, e in enumerate(events, 1):
        name = e.get("name", "—")
        url = e.get("url", "—")
        content = e.get("content") or ""
        en = extract_english_name(content)
        loc = extract_location_line(content)
        loc_esc = loc.replace("|", "\\|")
        name_esc = name.replace("|", "\\|")
        en_esc = en.replace("|", "\\|")
        lines.append(f"| {i} | {name_esc} | {en_esc} | {loc_esc} | {url} |")

    lines.append("")
    lines.append("## 2. 选项明细（按事件）")
    lines.append("")
    lines.append("下列「效果/说明」为 wiki 抓取原文摘录，可能与游戏内文案略有出入。")
    lines.append("")

    for e in events:
        name = e.get("name", "—")
        url = e.get("url", "")
        content = e.get("content") or ""
        en = extract_english_name(content)
        opt_sec = extract_options_section(content)

        lines.append(f"### {name}")
        lines.append("")
        lines.append(f"- **英文名**：{en}")
        lines.append(f"- **链接**：{url}")
        lines.append("")

        if not opt_sec:
            lines.append("*（未解析到「选项」区块，请见 JSON 中 `content` 全文）*")
            lines.append("")
            continue

        chunks = split_option_chunks(opt_sec)
        if not chunks:
            lines.append("| # | 选项 | 效果/说明（原文摘录） |")
            lines.append("|:-:|------|----------------------|")
            lines.append("| 1 | — | （选项区存在但未能拆分，见 JSON） |")
            lines.append("")
            continue

        lines.append("| # | 选项 | 效果/说明（原文摘录） |")
        lines.append("|:-:|------|----------------------|")
        for j, (title, body) in enumerate(chunks, 1):
            t = title.replace("|", "\\|").replace("\n", "<br>")
            b = body.replace("|", "\\|").replace("\n", "<br>")
            if len(b) > 1200:
                b = b[:1200] + "…"
            lines.append(f"| {j} | {t} | {b} |")
        lines.append("")

    OUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"Wrote {OUT_PATH}")


if __name__ == "__main__":
    main()
