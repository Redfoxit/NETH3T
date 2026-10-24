#!/usr/bin/env python3
"""Self-test for server.py. Run:  python3 test_server.py [path/to/host_tests]

If you pass the compiled firmware/test/host_tests binary, the daily-word maths is also cross-checked
against the C++ firmware logic for a handful of dates.
"""
import json
import os
import subprocess
import sys
import tempfile
import threading
import urllib.error
import urllib.request
from datetime import date
from http.server import ThreadingHTTPServer
from pathlib import Path

HERE = Path(__file__).parent
tmp = tempfile.mkdtemp()
os.environ["DATA_DIR"] = tmp
for name in ("answers.txt", "guesses.txt", "params.json"):
    (Path(tmp) / name).write_bytes((HERE / "data" / name).read_bytes())

sys.path.insert(0, str(HERE))
import server  # noqa: E402

server.DATA_DIR = Path(tmp)
server.RESULTS = Path(tmp) / "results.jsonl"
server.ANSWERS, server.GUESSES, server.PARAMS = server.load_words()

failures = 0


def check(cond, msg):
    global failures
    if not cond:
        failures += 1
        print("FAIL:", msg)


httpd = ThreadingHTTPServer(("127.0.0.1", 0), server.Handler)
port = httpd.server_address[1]
threading.Thread(target=httpd.serve_forever, daemon=True).start()
base = f"http://127.0.0.1:{port}"


def get(path):
    try:
        with urllib.request.urlopen(base + path, timeout=5) as r:
            return r.status, r.read().decode()
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode()


def post(path, payload, raw=None):
    data = raw if raw is not None else json.dumps(payload).encode()
    req = urllib.request.Request(base + path, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return r.status, json.loads(r.read())
    except urllib.error.HTTPError as e:
        return e.code, json.loads(e.read() or b"{}")


# --- basic endpoints
check(get("/health") == (200, "ok"), "health")
code, body = get("/api/today")
t = json.loads(body)
check(code == 200 and t["answers"] == len(server.ANSWERS) and 0 <= t["index"] < t["answers"], "today shape")
check("word" not in t, "today must not leak the word")
code, body = get("/")
check(code == 200 and "NETH3T" in body, "dashboard renders")

# --- scoring
check(server.score_guess("crane", "crane") == "ggggg", "exact")
check(server.score_guess("speed", "abide") == "bbyby", "duplicate letters")
check(server.score_guess("geese", "erase") == "bybgg", "geese/erase")
check(server.score_guess("llama", "hello") == "yybbb", "llama/hello")
check(server.score_guess("lolly", "hello") == "byggb", "lolly/hello")
idx = server.ANSWERS.index("crane")
check(json.loads(get(f"/api/guess?guess=crane&index={idx}")[1]) == {"valid": True, "result": "ggggg"}, "guess api hit")
check(json.loads(get(f"/api/guess?guess=zzzzq&index={idx}")[1]) == {"valid": False}, "guess api invalid word")
check(get("/api/guess?guess=toolong&index=1")[0] == 400, "guess api validation")

# --- results & streaks
today = server.day_number(server.today_local(), server.PARAMS["epoch"])


def result(day, won=True, guesses=3, mode="daily", player="neth3t", word="crane"):
    return post("/api/result", {"player": player, "mode": mode, "day": day, "word": word, "won": won, "guesses": guesses})


code, s = result(today - 3)
check(code == 200 and s["played"] == 1 and s["wins"] == 1, "first result stored")
result(today - 2)
result(today - 1, won=False, guesses=6)
code, s = result(today)
check(s["streak"] == 1, f"streak resets after a loss (got {s['streak']})")
check(s["max_streak"] == 2, f"max streak is 2 (got {s['max_streak']})")
check(s["played"] == 4 and s["wins"] == 3, "played/wins counted")
code, s = result(today + 0, mode="random", won=True, guesses=2)
check(s["streak"] == 1, "random games don't affect the daily streak")
result(today - 1, player="other", mode="daily")
code, s2 = result(today, player="other", mode="daily")
check(s2["streak"] == 2, "consecutive daily wins build a streak")

# --- validation / abuse
check(post("/api/result", {"player": "x y", "mode": "daily", "day": 1, "word": "crane", "won": True, "guesses": 1})[0] == 400, "bad player name")
check(post("/api/result", {"player": "a", "mode": "daily", "day": 1, "word": "<script>", "won": True, "guesses": 1})[0] == 400, "bad word")
check(post("/api/result", {"player": "a", "mode": "daily", "day": 1, "word": "crane", "won": True, "guesses": 9})[0] == 400, "bad guess count")
check(post("/api/result", None, raw=b"not json")[0] == 400, "garbage body")
check(post("/api/result", None, raw=b"x" * 5000)[0] == 400, "oversize body")
code, body = get("/")
check("<script>" not in body, "dashboard escapes input")

# --- cross-check daily word maths with the C++ firmware logic
if len(sys.argv) > 1:
    out = subprocess.run([sys.argv[1], "dates"], capture_output=True, text=True, check=True).stdout.split("\n")
    n = 0
    for line in out:
        if not line.strip():
            continue
        d, daynum, index, word = line.split()
        y, m, dd = map(int, d.split("-"))
        pd = server.day_number(date(y, m, dd), server.PARAMS["epoch"])
        pi = server.answer_index_for_day(pd, len(server.ANSWERS), server.PARAMS["multiplier"], server.PARAMS["offset"])
        check(pd == int(daynum) and pi == int(index) and server.ANSWERS[pi] == word, f"python/C++ disagree on {d}: py=({pd},{pi},{server.ANSWERS[pi]}) c++=({daynum},{index},{word})")
        n += 1
    print(f"cross-checked {n} dates against the C++ firmware logic")

print("server tests:", "ALL OK" if not failures else f"{failures} FAILED")
sys.exit(1 if failures else 0)
