"""Cache upstream rood-reverse pull-request text for naming research (read-only; never posts anything).

  python tools\\naming\\upstream_prs.py fetch [--token-env GITHUB_TOKEN] [--comments]
  python tools\\naming\\upstream_prs.py import <pulls-page.json> ...
  python tools\\naming\\upstream_prs.py add-comments <number> <comments.json>

Writes build\\port\\upstream_prs.json: [{number, title, body, author, merged_at, comments: [{author, path, body}]}].
`fetch` uses the GitHub REST API (unauthenticated requests are limited to 60/hour, so --comments needs a token).
`import` merges JSON arrays of pull requests saved from the GitHub API or the GitHub MCP server.
PR text is a hint for where to look; naming rows must still cite the source lines they were read from.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "build" / "port" / "upstream_prs.json"
REPO = "ser-pounce/rood-reverse"


def load() -> dict[int, dict]:
    if CACHE.exists():
        return {p["number"]: p for p in json.loads(CACHE.read_text(encoding="utf-8"))}
    return {}


def save(prs: dict[int, dict]) -> None:
    CACHE.parent.mkdir(parents=True, exist_ok=True)
    CACHE.write_text(json.dumps([prs[n] for n in sorted(prs)], indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"{CACHE}: {len(prs)} pull requests, {sum(len(p.get('comments', [])) for p in prs.values())} comments")


def entry(p: dict, old: dict | None) -> dict:
    return {"number": p["number"], "title": p["title"], "body": p.get("body") or "",
            "author": (p.get("user") or {}).get("login"), "merged_at": p.get("merged_at"),
            "comments": (old or {}).get("comments", [])}


def get(url: str, token: str | None):
    req = urllib.request.Request(url, headers={"Accept": "application/vnd.github+json", "User-Agent": "vs-nativeport"})
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    with urllib.request.urlopen(req) as r:
        return json.loads(r.read().decode("utf-8"))


def comment(c: dict) -> dict:
    return {"author": (c.get("user") or (c.get("author") or {})).get("login"), "path": c.get("path"),
            "body": c.get("body") or ""}


def main() -> int:
    ap = argparse.ArgumentParser()
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("--token-env", default="GITHUB_TOKEN")
    f.add_argument("--comments", action="store_true")
    i = sub.add_parser("import")
    i.add_argument("files", nargs="+")
    c = sub.add_parser("add-comments")
    c.add_argument("number", type=int)
    c.add_argument("file")
    a = ap.parse_args()
    prs = load()

    if a.cmd == "import":
        for name in a.files:
            text = Path(name).read_text(encoding="utf-8")
            data, _ = json.JSONDecoder().raw_decode(text)
            for p in data:
                prs[p["number"]] = entry(p, prs.get(p["number"]))
    elif a.cmd == "add-comments":
        data, _ = json.JSONDecoder().raw_decode(Path(a.file).read_text(encoding="utf-8"))
        items = data.get("comments", data) if isinstance(data, dict) else data
        prs[a.number]["comments"] = [comment(x) for x in items]
    else:
        token = os.environ.get(a.token_env)
        page = 1
        while True:
            data = get(f"https://api.github.com/repos/{REPO}/pulls?state=all&per_page=100&page={page}", token)
            if not data:
                break
            for p in data:
                prs[p["number"]] = entry(p, prs.get(p["number"]))
            page += 1
        if a.comments:
            for n in sorted(prs):
                base = f"https://api.github.com/repos/{REPO}"
                items = get(f"{base}/issues/{n}/comments?per_page=100", token)
                items += get(f"{base}/pulls/{n}/comments?per_page=100", token)
                prs[n]["comments"] = [comment(x) for x in items]
    save(prs)
    return 0


if __name__ == "__main__":
    sys.exit(main())
