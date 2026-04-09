# one-off generator: run from repo root optional
import pathlib
root = pathlib.Path(__file__).resolve().parent.parent.parent
extract = (root / "src/UI/_card_body_extract.txt").read_text(encoding="utf-8")
lines = extract.splitlines()
i = 0
while i < len(lines):
    if "void BattleUI::drawDetailedCardAt" in lines[i]:
        i += 1
        while i < len(lines) and ") {" not in lines[i]:
            i += 1
        if i < len(lines):
            i += 1
        break
    i += 1
body_lines = lines[i:]
while body_lines and body_lines[-1].strip() == "}":
    body_lines.pop()
dedented = [ln[4:] if ln.startswith("    ") else ln for ln in body_lines]
core = "\n".join(dedented)
core = core.replace("auto it = cardArtTextures_.find(key)", "auto& cache = card_art_cache(); auto it = cache.find(key)")
core = core.replace("if (it == cardArtTextures_.end())", "if (it == cache.end())")
core = core.replace(
    "if (tex.loadFromFile(artPath)) it = cardArtTextures_.emplace(key, std::move(tex)).first",
    "if (tex.loadFromFile(artPath)) it = cache.emplace(key, std::move(tex)).first",
)
core = core.replace("if (it != cardArtTextures_.end())", "if (it != cache.end())")
core = core.replace("fontForChinese()", "fontZh")
core = core.replace("sf::Text costText(font_, buf, costPt)", "sf::Text costText(fontZh, buf, costPt)")

header_path = root / "src/UI/_card_visual_header.cpppart"
footer_path = root / "src/UI/_card_visual_footer.cpppart"
header = header_path.read_text(encoding="utf-8")
footer = footer_path.read_text(encoding="utf-8")
out = header + core + footer
(root / "src/UI/CardVisual.cpp").write_text(out, encoding="utf-8")
print("ok", len(out))
