#!/usr/bin/env python3
"""NETH3T word server: shared daily word, result logging, streaks and a small web dashboard.

Standard library only (Python 3.9+). Configuration via environment variables:
  PORT       listen port                      (default 8088)
  DATA_DIR   folder with the word lists + db  (default ./data)
  TZ_NAME    timezone for the daily word      (default Australia/Melbourne)

Endpoints (all JSON unless noted):
  GET  /                    HTML dashboard
  GET  /health              "ok"
  GET  /api/today           {"date","day","index","answers"}   (the word itself is never sent)
  GET  /api/guess?guess=crane&index=123      {"valid": bool, "result": "gybbg"}   (for other clients)
  POST /api/result          {"player","mode","day","word","won","guesses"} -> stats
  GET  /api/stats?player=neth3t              stats for one player
"""
import html
import json
import os
import re
import threading
import time
from datetime import date, datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

try:
    from zoneinfo import ZoneInfo
except ImportError:  # pragma: no cover
    ZoneInfo = None

DATA_DIR = Path(os.environ.get("DATA_DIR", Path(__file__).parent / "data"))
PORT = int(os.environ.get("PORT", "8088"))
TZ_NAME = os.environ.get("TZ_NAME", "Australia/Melbourne")
RESULTS = DATA_DIR / "results.jsonl"
LOCK = threading.Lock()

MAX_BODY = 2048
PLAYER_RE = re.compile(r"^[A-Za-z0-9_.-]{1,32}$")
WORD_RE = re.compile(r"^[a-z]{5}$")


# ---------------------------------------------------------------- word logic (mirrors firmware)
def load_words():
    answers = [w for w in (DATA_DIR / "answers.txt").read_text().split() if w]
    guesses = set((DATA_DIR / "guesses.txt").read_text().split())
    params = json.loads((DATA_DIR / "params.json").read_text())
    if params["answers"] != len(answers):
        raise SystemExit("params.json does not match answers.txt - re-run tools/gen_words.py")
    return answers, guesses, params


def day_number(d: date, epoch) -> int:
    return (d - date(*epoch)).days


def answer_index_for_day(day: int, count: int, multiplier: int, offset: int) -> int:
    return ((day % count) * multiplier + offset) % count


def score_guess(guess: str, answer: str) -> str:
    result = ["b"] * 5
    remaining = {}
    for i in range(5):
        if guess[i] == answer[i]:
            result[i] = "g"
        else:
            remaining[answer[i]] = remaining.get(answer[i], 0) + 1
    for i in range(5):
        if result[i] == "g":
            continue
        if remaining.get(guess[i], 0) > 0:
            result[i] = "y"
            remaining[guess[i]] -= 1
    return "".join(result)


def today_local() -> date:
    if ZoneInfo:
        try:
            return datetime.now(ZoneInfo(TZ_NAME)).date()
        except Exception:
            pass
    return datetime.now().date()


ANSWERS, GUESSES, PARAMS = None, None, None


# ---------------------------------------------------------------- results + stats
def read_results():
    if not RESULTS.exists():
        return []
    out = []
    with RESULTS.open() as f:
        for line in f:
            try:
                out.append(json.loads(line))
            except ValueError:
                continue
    return out


def player_stats(results, player: str):
    mine = [r for r in results if r.get("player") == player]
    played = len(mine)
    wins = sum(1 for r in mine if r.get("won"))
    dist = [0] * 6
    for r in mine:
        if r.get("won") and 1 <= r.get("guesses", 0) <= 6:
            dist[r["guesses"] - 1] += 1

    # streaks count consecutive *daily* wins on consecutive days
    daily = {}
    for r in mine:
        if r.get("mode") == "daily":
            daily[r["day"]] = daily.get(r["day"], False) or bool(r.get("won"))
    max_streak = 0
    run = 0
    prev = None
    for d in sorted(daily):
        if daily[d]:
            run = run + 1 if prev is not None and d == prev + 1 and daily[prev] else 1
        else:
            run = 0
        prev = d
        max_streak = max(max_streak, run)
    streak = 0
    if daily:
        today = day_number(today_local(), PARAMS["epoch"])
        d = max(daily)
        if d >= today - 1:  # still alive if the last daily was today or yesterday
            while daily.get(d):
                streak += 1
                d -= 1
    return {"player": player, "played": played, "wins": wins, "streak": streak,
            "max_streak": max_streak, "distribution": dist}


# ---------------------------------------------------------------- HTTP
def dashboard_html():
    results = read_results()
    players = sorted({r.get("player", "?") for r in results})
    today = today_local()
    day = day_number(today, PARAMS["epoch"])
    rows = []
    for p in players:
        s = player_stats(results, p)
        rows.append(f"<tr><td>{html.escape(p)}</td><td>{s['played']}</td><td>{s['wins']}</td>"
                    f"<td>{s['streak']}</td><td>{s['max_streak']}</td></tr>")
    recent = []
    for r in reversed(results[-15:]):
        when = time.strftime("%d %b %H:%M", time.localtime(r.get("ts", 0)))
        outcome = f"won in {int(r.get('guesses', 0))}" if r.get("won") else "lost"
        recent.append(f"<tr><td>{when}</td><td>{html.escape(str(r.get('player', '?')))}</td>"
                      f"<td>{html.escape(str(r.get('mode', '')))}</td>"
                      f"<td>{html.escape(str(r.get('word', ''))).upper()}</td><td>{outcome}</td></tr>")
    return f"""<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="60"><title>NETH3T</title>
<style>body{{font-family:system-ui,sans-serif;background:#111;color:#eee;max-width:640px;margin:2rem auto;padding:0 1rem}}
h1{{color:#fd7b00}}table{{width:100%;border-collapse:collapse;margin:.5rem 0 1.5rem}}th,td{{padding:.4rem .6rem;text-align:left;border-bottom:1px solid #333}}
th{{color:#999;font-weight:600}}.muted{{color:#888}}</style></head><body>
<h1>NETH3T</h1>
<p class="muted">Daily Wordle #{day} &middot; {today.isoformat()} ({html.escape(TZ_NAME)}) &middot; {len(ANSWERS)} answers loaded</p>
<h2>Players</h2><table><tr><th>Player</th><th>Played</th><th>Won</th><th>Streak</th><th>Best</th></tr>{''.join(rows) or '<tr><td colspan=5 class=muted>No games yet</td></tr>'}</table>
<h2>Recent games</h2><table><tr><th>When</th><th>Player</th><th>Mode</th><th>Word</th><th>Result</th></tr>{''.join(recent) or '<tr><td colspan=5 class=muted>Nothing yet</td></tr>'}</table>
</body></html>"""


class Handler(BaseHTTPRequestHandler):
    server_version = "CYDArcade/1.0"

    def log_message(self, fmt, *args):
        print("%s %s" % (self.address_string(), fmt % args), flush=True)

    def _send(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else (json.dumps(body) if ctype == "application/json" else body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype + ("; charset=utf-8" if ctype != "application/json" else ""))
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        url = urlparse(self.path)
        q = parse_qs(url.query)
        if url.path == "/":
            return self._send(200, dashboard_html(), "text/html")
        if url.path == "/health":
            return self._send(200, "ok", "text/plain")
        if url.path == "/api/today":
            today = today_local()
            day = day_number(today, PARAMS["epoch"])
            idx = answer_index_for_day(day, len(ANSWERS), PARAMS["multiplier"], PARAMS["offset"])
            return self._send(200, {"date": today.isoformat(), "day": day, "index": idx, "answers": len(ANSWERS)})
        if url.path == "/api/stats":
            player = (q.get("player") or ["neth3t"])[0]
            if not PLAYER_RE.match(player):
                return self._send(400, {"error": "bad player"})
            return self._send(200, player_stats(read_results(), player))
        if url.path == "/api/guess":
            guess = (q.get("guess") or [""])[0].lower()
            try:
                idx = int((q.get("index") or ["-1"])[0])
            except ValueError:
                idx = -1
            if not WORD_RE.match(guess) or not 0 <= idx < len(ANSWERS):
                return self._send(400, {"error": "need guess=<5 letters>&index=<answer index>"})
            if guess not in GUESSES:
                return self._send(200, {"valid": False})
            return self._send(200, {"valid": True, "result": score_guess(guess, ANSWERS[idx])})
        return self._send(404, {"error": "not found"})

    def do_POST(self):
        if urlparse(self.path).path != "/api/result":
            return self._send(404, {"error": "not found"})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_BODY:
                raise ValueError("size")
            body = json.loads(self.rfile.read(length))
            player = str(body.get("player", ""))
            mode = body.get("mode")
            word = str(body.get("word", "")).lower()
            won = bool(body.get("won"))
            guesses = int(body.get("guesses", 0))
            day = int(body.get("day", 0))
            if not PLAYER_RE.match(player) or mode not in ("daily", "random") or not WORD_RE.match(word) or not 1 <= guesses <= 6:
                raise ValueError("fields")
        except (ValueError, TypeError, json.JSONDecodeError):
            return self._send(400, {"error": "bad request"})
        record = {"ts": int(time.time()), "player": player, "mode": mode, "day": day,
                  "word": word, "won": won, "guesses": guesses}
        with LOCK:
            DATA_DIR.mkdir(parents=True, exist_ok=True)
            with RESULTS.open("a") as f:
                f.write(json.dumps(record) + "\n")
            stats = player_stats(read_results(), player)
        return self._send(200, stats)


def main():
    global ANSWERS, GUESSES, PARAMS
    ANSWERS, GUESSES, PARAMS = load_words()
    srv = ThreadingHTTPServer(("0.0.0.0", PORT), Handler)
    print(f"NETH3T word server on :{PORT}  answers={len(ANSWERS)} guesses={len(GUESSES)} tz={TZ_NAME}", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
