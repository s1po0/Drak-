#!/usr/bin/env python3
"""
Extract all player-visible English strings from Summertime Saga (1.zip + 2.zip)
for Sorani Kurdish (ckb) localization.
"""
from __future__ import annotations

import ast as pyast
import glob
import gzip
import json
import os
import re
import sys

UNRPYC_DIR = "/tmp/drak_work/unrpyc"
if os.path.isdir(UNRPYC_DIR):
    sys.path.insert(0, UNRPYC_DIR)


def is_translatable_text(s: str) -> bool:
    if not s or not s.strip():
        return False
    # Must contain at least one ASCII letter (skip pure punctuation like "...", "!!!", "???")
    # Strip Ren'Py tags and [substitutions] first to check if there is actual English text outside tags!
    stripped = strip_renpy_tokens(s)
    return any("a" <= ch.lower() <= "z" for ch in stripped)


def tokenize_renpy(s: str) -> list[tuple[str, str]]:
    """
    Tokenize a Ren'Py string into ('TEXT', text) and ('MASK', token) segments.
    Handles:
      - Escaped [[ and {{
      - Tags {...} including tags containing [...] like {color=[game.hex]} or {usd=$[what.cost:,]}
        Wait: for {dom=assertive} or {usd=$[what.cost:,]}, keep the whole tag intact EXCEPT:
        In Jenny's diary, {pen=pink}Hello{/pen} -> {pen=pink} and {/pen} are tags, Hello is TEXT!
        What about {rb}text{/rb} or {art}text{/art} or {rt}text{/rt}? Those are paired tags, so
        {rb}, {/rb}, {art}, {/art}, {rt}, {/rt} are individual tags and the text between them IS translated!
      - Substitutions [...] (balanced brackets)
      - Python printf %(...)s, %s, %d
    """
    tokens: list[tuple[str, str]] = []
    n = len(s)
    i = 0
    buf: list[str] = []

    def flush():
        if buf:
            tokens.append(("TEXT", "".join(buf)))
            buf.clear()

    printf_re = re.compile(r"%(?:\([A-Za-z0-9_]+\))?[#0\- +]?(?:\d+|\*)?(?:\.(?:\d+|\*))?[hlL]?[diouxXeEfFgGcrs]")

    while i < n:
        ch = s[i]
        # Escaped [[ or {{
        if ch == "[" and i + 1 < n and s[i + 1] == "[":
            flush()
            tokens.append(("MASK", "[["))
            i += 2
            continue
        if ch == "{" and i + 1 < n and s[i + 1] == "{":
            flush()
            tokens.append(("MASK", "{{"))
            i += 2
            continue
        # Tag {...}
        if ch == "{":
            j = i + 1
            bracket_depth = 0
            while j < n:
                if s[j] == "[":
                    bracket_depth += 1
                elif s[j] == "]" and bracket_depth > 0:
                    bracket_depth -= 1
                elif s[j] == "}" and bracket_depth == 0:
                    break
                j += 1
            if j < n and s[j] == "}":
                flush()
                tokens.append(("MASK", s[i : j + 1]))
                i = j + 1
                continue
        # Substitution [...]
        if ch == "[":
            j = i + 1
            depth = 1
            while j < n and depth > 0:
                if s[j] == "[":
                    depth += 1
                elif s[j] == "]":
                    depth -= 1
                j += 1
            if depth == 0:
                flush()
                tokens.append(("MASK", s[i:j]))
                i = j
                continue
        # Printf % formatting
        if ch == "%":
            m = printf_re.match(s, i)
            if m:
                flush()
                tokens.append(("MASK", m.group(0)))
                i = m.end()
                continue

        buf.append(ch)
        i += 1

    flush()
    return tokens


def strip_renpy_tokens(s: str) -> str:
    return "".join(val for kind, val in tokenize_renpy(s) if kind == "TEXT")


def extract_all() -> dict:
    import unrpyc
    from xdis.load import load_module

    say_set: set[str] = set()
    menu_set: set[str] = set()
    ui_set: set[str] = set()
    pyc_set: set[str] = set()

    # 1. Walk all .rpyc / .rpymc ASTs in ext2
    rpyc_files = sorted(
        glob.glob("/tmp/drak_work/ext2/**/*.rpyc", recursive=True)
        + glob.glob("/tmp/drak_work/ext2/**/*.rpymc", recursive=True)
    )

    def walk_node(n):
        cls = type(n).__name__
        d = getattr(n, "__dict__", {})
        if cls in ("Say", "TranslateSay"):
            what = getattr(n, "what", None)
            if isinstance(what, str) and is_translatable_text(what):
                say_set.add(what)
        elif cls == "Menu":
            for item in getattr(n, "items", []):
                if item and isinstance(item[0], str) and is_translatable_text(item[0]):
                    menu_set.add(item[0])
                if len(item) >= 3 and item[2]:
                    for child in item[2]:
                        walk_node(child)
            return
        for v in d.values():
            if isinstance(v, list):
                for x in v:
                    if hasattr(x, "__dict__"):
                        walk_node(x)
            elif hasattr(v, "__dict__"):
                walk_node(v)

    for p in rpyc_files:
        ctx = unrpyc.Context()
        with open(p, "rb") as f:
            stmts = unrpyc.read_ast_from_file(f, ctx)
        for s in stmts:
            walk_node(s)

    # 2. Extract _(), __(), ___() from all decompiled .rpy / .rpym in dec1 + dec2
    rpy_all = sorted(
        p
        for p in (
            glob.glob("/tmp/drak_work/dec2/**/*.rpy*", recursive=True)
            + glob.glob("/tmp/drak_work/dec1/**/*.rpy*", recursive=True)
        )
        if p.endswith((".rpy", ".rpym"))
    )
    underscore_re = re.compile(
        r'(?<![A-Za-z0-9_])_{1,3}\(\s*([ruRU]?(?:\'\'\'[\s\S]*?\'\'\'|"""[\s\S]*?"""|\'(?:\\.|[^\'\\])*\'|"(?:\\.|[^"\\])*"))\s*\)'
    )
    for p in rpy_all:
        txt = open(p, encoding="utf-8", errors="ignore").read()
        for m in underscore_re.finditer(txt):
            try:
                val = pyast.literal_eval(m.group(1))
                if isinstance(val, str) and is_translatable_text(val):
                    ui_set.add(val)
            except Exception:
                pass

    # Add un-wrapped screen UI labels found in x-tel.rpy / x-cook.rpy
    for extra in ("Age: [who.age]", "Bust: [who.size]", "Size: ???", "GO"):
        ui_set.add(extra)

    # 3. Extract player-visible strings from x-saga .pyc modules
    def get_pyc_strs(rel: str) -> list[str]:
        co = load_module(os.path.join("/tmp/drak_work/ext2", rel))[3]
        out: list[str] = []

        def w(o):
            if isinstance(o, str):
                out.append(o)
            elif isinstance(o, (tuple, list, frozenset)):
                for x in o:
                    w(x)
            elif hasattr(o, "co_consts"):
                for x in o.co_consts:
                    w(x)

        w(co)
        return out

    # 3a. Jenny's diary lines (all lines containing '{pen=' or '{space=' or '{rb}')
    for s in get_pyc_strs("x-saga/x-data/x-jenny_diary.pyc"):
        if ("{" in s or " " in s) and is_translatable_text(s):
            pyc_set.add(s)

    # 3b. Phone SMS messages (x-saga/x-init/x-mesg.pyc)
    for s in get_pyc_strs("x-saga/x-init/x-mesg.pyc"):
        if (" " in s or "!" in s or "?" in s) and is_translatable_text(s):
            pyc_set.add(s)

    # 3c. Replay gallery descriptions & titles (x-saga/x-init/x-lewd.pyc)
    for s in get_pyc_strs("x-saga/x-init/x-lewd.pyc"):
        if (" " in s or s.endswith(".")) and "/" not in s and "." not in s[:-1] and is_translatable_text(s):
            pyc_set.add(s)
        elif ("[saga." in s or "'s " in s) and is_translatable_text(s):
            pyc_set.add(s)

    # 3d. Curated UI/enum/device strings from x-saga
    explicit_pyc_strings = [
        # Phone apps
        "Home", "Contacts", "Messages", "Notes", "Viewer",
        # Days of week
        "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday",
        # Times of day
        "Morning", "Afternoon", "Evening", "Night",
        # Moon phases & holidays
        "New", "Waxing Crescent", "First Quarter", "Waxing Gibibous", "Full",
        "Waning Gibbous", "Third Quarter", "Waning Crescent",
        "Christmas", "Halloween",
        # Notifications & GUI
        "[what!i] added to basket.",
        "[what!i] is now available to visit.",
        "GAME OVER...",
        "New cookie jar scene unlocked!",
        "New cookie jar variant unlocked!",
        "Assertive", "Submissive", "More",
        "{i}The main story will return in future updates.{/i}",
        "{i}[saga.cast.diane]'s story will return in future updates.{/i}",
        "[saga.cast.anon] is {dom=assertive}.",
        "[saga.cast.anon] is {sub=submissive}.",
        "Shower", "Sink",
        # Relative dates (x-lang.pyc)
        "today", "tomorrow", "the day after tomorrow",
        "a few days from now", "a week or so from now",
        # PC & Jenny's Laptop UI/emails (excluding passwords/usernames!)
        "Sticky", "Recycle Bin", "Photos", "Remote Access", "Homework",
        "New Folder", "Wish List",
        "My favorite toy ;)",
        "FIRST TIME with my new ElectroClit!!",
        "New toy is ULTRA strong! {emoji=heart}",
        "OMG!! I'm soooo FULL!!! ¦D~~",
        "Pablo69 wants to share a picture with you!",
        "Your earnings for this month",
        "Your item(s) have been shipped!",
        "Thank you for subscribing!",
        "I am Nigeria Prince, in needs of assistance!",
        "send nudes",
        # Posters & notes (x-vfx/x-view.pyc)
        "{size=+12}[saga.cast.ronald!u] PRESENTS:{/size}\nThe annual Summerville art contest!\n{vspace=40}{size=+2}FIRST PLACE CASH PRIZE!{/size}",
        "Hey sweetie...\nCan you please come \ndown and take the laundry\nbasket from my room\nto the basement?\n-[saga.cast.debbie]",
        "EMPLOYEE OF THE MONTH",
        # Mini-games
        "Lime", "Olive", "Pineapple", "Ice cube", "Lemon twist", "Coconut",
        "Tequila", "Vodka", "Rum", "margarita", "martini", "piña colada",
        "blueberry glaze", "chocolate frosting", "strawberry glaze", "vanilla frosting", "chocolate chips",
        "the {b}{size=-2}UNWANTED{/size}{/b} items",
        "Pot of cum",
    ]
    for s in explicit_pyc_strings:
        if is_translatable_text(s):
            pyc_set.add(s)

    all_strings = sorted(say_set | menu_set | ui_set | pyc_set)
    os.makedirs("data", exist_ok=True)
    payload = {
        "counts": {
            "say": len(say_set),
            "menu": len(menu_set),
            "ui": len(ui_set),
            "pyc": len(pyc_set),
            "total_unique": len(all_strings),
            "total_chars": sum(len(s) for s in all_strings),
        },
        "strings": all_strings,
    }
    with gzip.open("data/strings_en.json.gz", "wt", encoding="utf-8") as gz:
        json.dump(payload, gz, ensure_ascii=False)
    print(json.dumps(payload["counts"], indent=2))
    print(f"Saved data/strings_en.json.gz ({os.path.getsize('data/strings_en.json.gz') / 1024:.1f} KB)")
    return payload


if __name__ == "__main__":
    extract_all()
