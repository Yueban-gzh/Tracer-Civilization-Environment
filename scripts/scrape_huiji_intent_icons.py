#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从灰机「杀戮尖塔中文维基」意图条目下载图标到 assets/intention/。

默认页面: https://sts.huijiwiki.com/wiki/%E6%84%8F%E5%9B%BE

站点有 Cloudflare 防护，请先在浏览器登录/通过验证后，把完整 Cookie 粘贴到:
  - assets/intention/cookie.txt  （优先）
  - assets/status/cookie.txt      （备选，已在 .gitignore）

Cookie 文件格式: 单行，与浏览器「复制为 cURL」里的 Cookie 头相同，例如:
  cf_clearance=...; __cf_bm=...; huiji_session=...

依赖: pip install -r scripts/requirements-intent-scrape.txt
"""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path
from urllib.parse import urljoin, urlparse, unquote

try:
    import requests
except ImportError:
    print("请先安装: pip install -r scripts/requirements-intent-scrape.txt", file=sys.stderr)
    raise

try:
    from bs4 import BeautifulSoup
except ImportError:
    print("请先安装: pip install -r scripts/requirements-intent-scrape.txt", file=sys.stderr)
    raise

DEFAULT_WIKI_URL = "https://sts.huijiwiki.com/wiki/%E6%84%8F%E5%9B%BE"

USER_AGENT = (
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
)

IMAGE_EXT = (".png", ".jpg", ".jpeg", ".webp", ".gif", ".PNG", ".JPG", ".JPEG", ".GIF", ".WEBP")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def default_cookie_paths(root: Path) -> list[Path]:
    return [
        root / "assets" / "intention" / "cookie.txt",
        root / "assets" / "status" / "cookie.txt",
    ]


def load_cookie_header(path: Path | None, root: Path) -> str:
    if path is not None:
        if not path.is_file():
            sys.exit(f"找不到 Cookie 文件: {path}")
        return path.read_text(encoding="utf-8", errors="replace").strip().replace("\n", " ").strip()
    for p in default_cookie_paths(root):
        if p.is_file():
            raw = p.read_text(encoding="utf-8", errors="replace").strip().replace("\n", " ").strip()
            if raw:
                print(f"使用 Cookie 文件: {p}")
                return raw
    sys.exit(
        "未找到 Cookie。请将浏览器里的 Cookie 写入 assets/intention/cookie.txt "
        "或 assets/status/cookie.txt（单行）。"
    )


def safe_filename(name: str) -> str:
    name = unquote(name).strip()
    for c in '<>:"/\\|?*\n\r\t':
        name = name.replace(c, "_")
    if not name or name in (".", ".."):
        name = "image.png"
    return name[:180]


def is_probably_intent_asset(url: str) -> bool:
    u = url.lower()
    if any(u.endswith(ext.lower()) for ext in IMAGE_EXT):
        return True
    parsed = urlparse(url)
    path = unquote(parsed.path).lower()
    # 维基缩略图路径常含 /thumb/ 仍以原图扩展结尾
    if "/images/" in path or "/thumb/" in path or "huiji" in parsed.netloc.lower():
        return any(path.endswith(ext.lower()) for ext in (".png", ".jpg", ".jpeg", ".webp", ".gif"))
    return False


def collect_image_urls(html: str, base_url: str) -> list[str]:
    soup = BeautifulSoup(html, "html.parser")
    main = soup.select_one(".mw-parser-output") or soup.body
    if not main:
        return []

    seen: set[str] = set()
    out: list[str] = []

    def push(raw: str) -> None:
        if not raw or raw.startswith("data:"):
            return
        absolute = urljoin(base_url, raw.strip())
        if absolute in seen:
            return
        if not is_probably_intent_asset(absolute):
            return
        # 跳过站点 UI 小图标（可按需调整）
        low = absolute.lower()
        if any(x in low for x in ("poweredby_", "wikimedia", "creativecommons", "huijilogo")):
            return
        seen.add(absolute)
        out.append(absolute)

    for img in main.find_all("img"):
        src = img.get("src")
        if src:
            push(src)
        srcset = img.get("srcset")
        if srcset:
            # 取声明宽度最大的 URL
            best_url = None
            best_w = -1
            for part in srcset.split(","):
                part = part.strip()
                if not part:
                    continue
                chunks = part.split()
                url = chunks[0]
                w = 0
                if len(chunks) >= 2 and chunks[-1].endswith("w"):
                    try:
                        w = int(chunks[-1][:-1])
                    except ValueError:
                        w = 0
                if w >= best_w:
                    best_w = w
                    best_url = url
            if best_url:
                push(best_url)

    return out


def pick_filename_from_url(url: str) -> str:
    path = unquote(urlparse(url).path)
    name = Path(path).name
    if not name or "." not in name:
        name = "intent_image.png"
    return safe_filename(name)


def candidate_download_urls(url: str) -> list[str]:
    """缩略图 thumb 先尝试换为 huiji-public 原图，再回退原 URL。"""
    out: list[str] = []
    if "/uploads/thumb/" in url:
        m = re.search(
            r"/uploads/thumb/((?:[^/]+/){2}[^/]+\.(?:png|jpe?g|gif|webp))(?:/[^/]*)?$",
            url,
            re.IGNORECASE,
        )
        if m:
            full = "https://huiji-public.huijistatic.com/sts/uploads/" + m.group(1)
            if full not in out:
                out.append(full)
    if url not in out:
        out.append(url)
    return out


def download_file(client, url: str, dest: Path, delay: float, extra_headers: dict | None = None) -> bool:
    dest.parent.mkdir(parents=True, exist_ok=True)
    r = client.get(url, timeout=120, stream=True, allow_redirects=True, headers=extra_headers or {})
    if r.status_code != 200:
        print(f"  跳过 HTTP {r.status_code}: {url}")
        return False
    ctype = (r.headers.get("Content-Type") or "").lower()
    if ctype and not any(x in ctype for x in ("image", "octet-stream", "application/x-download")):
        print(f"  跳过非图片 Content-Type ({ctype}): {url}")
        return False
    tmp = dest.with_suffix(dest.suffix + ".part")
    try:
        with open(tmp, "wb") as f:
            for chunk in r.iter_content(chunk_size=65536):
                if chunk:
                    f.write(chunk)
        tmp.replace(dest)
    finally:
        if tmp.exists():
            try:
                tmp.unlink()
            except OSError:
                pass
    if delay > 0:
        time.sleep(delay)
    return True


def fetch_html(session: requests.Session, url: str) -> str:
    r = session.get(url, timeout=120, allow_redirects=True)
    if r.status_code == 403:
        sys.exit(
            f"HTTP 403（拒绝访问）。请用浏览器重新打开维基「意图」页，从 F12→网络→文档 请求里复制最新 Cookie，"
            f"覆盖写入 cookie.txt（cf_clearance / __cf_bm 易过期）。\nURL: {url}"
        )
    if r.status_code != 200:
        sys.exit(f"获取页面失败 HTTP {r.status_code}: {url}")
    text = r.text
    if "Just a moment" in text and "challenge-platform" in text:
        sys.exit(
            "仍被 Cloudflare 拦截（页面含 Just a moment）。请更新浏览器里 sts.huijiwiki.com 的 Cookie，"
            "尤其是 cf_clearance / __cf_bm，再写入 cookie.txt。"
        )
    return text


def build_session(cookie: str, *, for_html: bool) -> requests.Session:
    s = requests.Session()
    accept = (
        "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8"
        if for_html
        else "image/avif,image/webp,image/apng,image/*,*/*;q=0.8"
    )
    h: dict[str, str] = {
        "User-Agent": USER_AGENT,
        "Accept": accept,
        "Accept-Language": "zh-CN,zh;q=0.9,en-US;q=0.8,en;q=0.7",
        "Cookie": cookie,
        "Referer": "https://sts.huijiwiki.com/",
        "DNT": "1",
        "Connection": "keep-alive",
    }
    if for_html:
        h.update(
            {
                "Upgrade-Insecure-Requests": "1",
                "Sec-Fetch-Dest": "document",
                "Sec-Fetch-Mode": "navigate",
                "Sec-Fetch-Site": "none",
                "Sec-Fetch-User": "?1",
                "sec-ch-ua": '"Google Chrome";v="131", "Chromium";v="131", "Not_A Brand";v="24"',
                "sec-ch-ua-mobile": "?0",
                "sec-ch-ua-platform": '"Windows"',
            }
        )
    else:
        h.update(
            {
                "Sec-Fetch-Dest": "image",
                "Sec-Fetch-Mode": "no-cors",
                "Sec-Fetch-Site": "cross-site",
            }
        )
    s.headers.update(h)
    return s


def main() -> None:
    root = repo_root()
    ap = argparse.ArgumentParser(description="爬取灰机维基「意图」页图标到 assets/intention/")
    ap.add_argument(
        "--url",
        default=DEFAULT_WIKI_URL,
        help="维基页面 URL",
    )
    ap.add_argument(
        "--cookie-file",
        type=Path,
        default=None,
        help="Cookie 文件路径（默认: assets/intention/cookie.txt 或 assets/status/cookie.txt）",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=None,
        help="输出目录（默认: 仓库内 assets/intention）",
    )
    ap.add_argument(
        "--html-file",
        type=Path,
        default=None,
        help="若已另存页面为本地 HTML，则只解析该文件、不发起列表请求",
    )
    ap.add_argument(
        "--delay",
        type=float,
        default=0.35,
        help="每张图下载间隔（秒），减轻服务器压力",
    )
    ap.add_argument(
        "--dry-run",
        action="store_true",
        help="只打印将下载的 URL，不写入文件",
    )
    ap.add_argument(
        "--cloudscraper",
        action="store_true",
        help="使用 cloudscraper 库发请求（pip install cloudscraper），部分环境下可绕过 Cloudflare",
    )
    args = ap.parse_args()

    out_dir = (args.out_dir if args.out_dir else root / "assets" / "intention").resolve()
    cookie = load_cookie_header(args.cookie_file, root)

    scraper = None
    if args.html_file:
        if not args.html_file.is_file():
            sys.exit(f"找不到本地 HTML: {args.html_file}")
        html = args.html_file.read_text(encoding="utf-8", errors="replace")
        base = args.url
    elif args.cloudscraper:
        try:
            import cloudscraper
        except ImportError:
            sys.exit("未安装 cloudscraper，请执行: pip install cloudscraper")
        scraper = cloudscraper.create_scraper(
            browser={"browser": "chrome", "platform": "windows", "mobile": False}
        )
        scraper.headers.update(build_session(cookie, for_html=True).headers)
        r = scraper.get(args.url, timeout=120, allow_redirects=True)
        if r.status_code != 200:
            sys.exit(f"cloudscraper 获取页面失败 HTTP {r.status_code}: {args.url}")
        html = r.text
        base = args.url
        if "Just a moment" in html and "challenge-platform" in html:
            sys.exit("cloudscraper 仍拿到挑战页，请换浏览器 Cookie 或使用 --html-file。")
    else:
        session = build_session(cookie, for_html=True)
        html = fetch_html(session, args.url)
        base = args.url

    urls = collect_image_urls(html, base)
    if not urls:
        print("未解析到任何图片 URL。页面结构可能已变，或内容在脚本执行后才动态加载。")
        sys.exit(1)

    print(f"共 {len(urls)} 个图片 URL，输出目录: {out_dir}")

    if args.dry_run:
        for u in urls:
            print(u)
        return

    # 图片 CDN 常校验 Referer；与 wiki 页同会话（cloudscraper）可避免 HTTP 567 等拦截
    if scraper is not None:
        download_client = scraper
        scraper.headers["Referer"] = args.url
        scraper.headers["Accept"] = "image/avif,image/webp,image/apng,image/*,*/*;q=0.8"
        scraper.headers["Sec-Fetch-Dest"] = "image"
        scraper.headers["Sec-Fetch-Mode"] = "no-cors"
        scraper.headers["Sec-Fetch-Site"] = "cross-site"
        img_extra: dict | None = {"Referer": args.url}
    else:
        download_client = build_session(cookie, for_html=False)
        download_client.headers["Referer"] = args.url
        img_extra = {"Referer": args.url}

    used_names: dict[str, int] = {}
    ok = 0
    for u in urls:
        base_name = pick_filename_from_url(u)
        stem = Path(base_name).stem
        suffix = Path(base_name).suffix or ".png"
        n = used_names.get(base_name, 0)
        used_names[base_name] = n + 1
        if n > 0:
            dest = out_dir / f"{stem}_{n}{suffix}"
        else:
            dest = out_dir / base_name

        print(f"下载: {base_name} <- {u[:100]}...")
        done = False
        for try_url in candidate_download_urls(u):
            if download_file(download_client, try_url, dest, args.delay, img_extra):
                ok += 1
                done = True
                break
        if not done:
            print(f"  失败（已尝试原图与缩略图 URL）")

    print(f"完成: 成功 {ok} / {len(urls)}")


if __name__ == "__main__":
    main()
