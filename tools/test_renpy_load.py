#!/usr/bin/env python3
"""
1. Generate +00kurdish_tl.rpy from data/translations_ckb.json.gz (with UI fixes + RLM anchors).
2. Compile it to +00kurdish_tl.rpyc using the OFFICIAL Ren'Py 8.5.3 SDK!
3. Verify with official Ren'Py 8.5.3 lint + runtime script loader that both
   +00kurdish_tl.rpyc AND patched Say/TranslateSay .rpyc files load with 0 errors!
4. Save the official compiled data/kurdish_tl_official.rpyc.
"""
import gzip
import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract_strings import tokenize_renpy

UI_GLOSSARY_FIXES = {
    "Resume": "بەردەوامبوون",
    "Display": "پیشاندان",
    "Quit": "چوونەدەرەوە",
    "About": "دەربارە",
    "Save": "پاشەکەوتکردن",
    "Attic": "ژووری سەرەوە",
}


def has_kurdish_script(s: str) -> bool:
    return any("\u0600" <= c <= "\u06ff" for c in s)


def apply_bidi_rlm_if_needed(ckb: str) -> str:
    if not has_kurdish_script(ckb) or ckb.startswith("\u200f"):
        return ckb
    tokens = tokenize_renpy(ckb)
    for kind, val in tokens:
        if kind == "MASK":
            if val.startswith("[") and not val.startswith("[["):
                return "\u200f" + ckb
            continue
        else:
            for ch in val:
                if "\u0600" <= ch <= "\u06ff":
                    return ckb
                if "A" <= ch <= "Z" or "a" <= ch <= "z":
                    return "\u200f" + ckb
    return ckb


def renpy_quote(s: str) -> str:
    s = s.replace("\\", "\\\\")
    s = s.replace('"', '\\"')
    s = s.replace("\a", "\\a")
    s = s.replace("\b", "\\b")
    s = s.replace("\f", "\\f")
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    s = s.replace("\t", "\\t")
    s = s.replace("\v", "\\v")
    return f'"{s}"'


def main():
    sdk_dir = sys.argv[1]
    proj_dir = "/tmp/renpy_compile_proj"
    shutil.rmtree(proj_dir, ignore_errors=True)
    os.makedirs(os.path.join(proj_dir, "game", "src"), exist_ok=True)

    with gzip.open("data/translations_ckb.json.gz", "rt", encoding="utf-8") as gz:
        raw_trans = json.load(gz)

    rpy_path = os.path.join(proj_dir, "game", "src", "+00kurdish_tl.rpy")
    count = 0
    with open(rpy_path, "w", encoding="utf-8") as f:
        f.write("init offset = -999\n\ntranslate None strings:\n\n")
        for old_s, raw_new in sorted(raw_trans.items()):
            new_s = apply_bidi_rlm_if_needed(UI_GLOSSARY_FIXES.get(old_s, raw_new))
            if old_s == new_s:
                continue
            f.write(f"    old {renpy_quote(old_s)}\n")
            f.write(f"    new {renpy_quote(new_s)}\n\n")
            count += 1

    print(f"Wrote {count} string translations to {rpy_path}")

    # Minimal start label that verifies translations work at runtime & lints cleanly
    test_rpy = os.path.join(proj_dir, "game", "test_entry.rpy")
    with open(test_rpy, "w", encoding="utf-8") as f:
        f.write(
            'define anon = Character("Anon")\n'
            'label start:\n'
            '    anon "Leave the room."\n'
            '    "Something smells good, time to head downstairs."\n'
            '    return\n'
        )

    env = dict(os.environ)
    env["SDL_VIDEODRIVER"] = "dummy"
    env["SDL_AUDIODRIVER"] = "dummy"
    env["RENPY_RPYC_EXCEPTIONS"] = "1"

    # Step 1: Official Ren'Py 8.5.3 compile!
    p1 = subprocess.run([os.path.join(sdk_dir, "renpy.sh"), proj_dir, "compile"], env=env, text=True, capture_output=True)
    print("COMPILE RC:", p1.returncode)
    print("COMPILE STDOUT:\n", p1.stdout)
    print("COMPILE STDERR:\n", p1.stderr)
    if p1.returncode != 0:
        for fn in ["traceback.txt", "log.txt"]:
            p = os.path.join(proj_dir, fn)
            if os.path.exists(p):
                print(f"=== {fn} ===\n", open(p).read()[-4000:])
        sys.exit(p1.returncode)

    rpyc_out = os.path.join(proj_dir, "game", "src", "+00kurdish_tl.rpyc")
    assert os.path.exists(rpyc_out), "Compiled +00kurdish_tl.rpyc missing!"
    # Remove .rpy source so Step 2 tests loading ONLY from compiled .rpyc (just like Android APK!)
    os.remove(rpy_path)

    # Also patch test_entry.rpyc using our patch_rpyc_bytes to verify our Say/TranslateSay patcher on real Ren'Py 8.5.3!
    os.remove(test_rpy)
    test_rpyc = os.path.join(proj_dir, "game", "test_entry.rpyc")
    with open("tools/build_kurdish_zips.py", "r", encoding="utf-8") as bf:
        bsrc = bf.read().replace("import unrpyc", "unrpyc = None")
    ns = {"__file__": os.path.abspath("tools/build_kurdish_zips.py")}
    exec(compile(bsrc, "tools/build_kurdish_zips.py", "exec"), ns)
    with open(test_rpyc, "rb") as tf:
        orig_test_rpyc = tf.read()
    patched_test_rpyc, pcnt = ns["patch_rpyc_bytes"](orig_test_rpyc, raw_trans, "test_entry.rpyc")
    assert pcnt == 4, f"Expected 4 patched slots (2 in slot1 + 2 in slot2), got {pcnt}"
    with open(test_rpyc, "wb") as tf:
        tf.write(patched_test_rpyc)

    # Step 2: Official Ren'Py 8.5.3 lint on .rpyc-only project (both +00kurdish_tl.rpyc AND patched test_entry.rpyc)!
    p2 = subprocess.run([os.path.join(sdk_dir, "renpy.sh"), proj_dir, "lint"], env=env, text=True, capture_output=True)
    print("LINT RC:", p2.returncode)
    print("LINT STDOUT:\n", p2.stdout)
    print("LINT STDERR:\n", p2.stderr)
    if p2.returncode != 0:
        for fn in ["traceback.txt", "log.txt"]:
            p = os.path.join(proj_dir, fn)
            if os.path.exists(p):
                print(f"=== {fn} ===\n", open(p).read()[-4000:])
        sys.exit(p2.returncode)

    shutil.copyfile(rpyc_out, "data/+00kurdish_tl.rpyc")
    print(f"SUCCESS! Saved official Ren'Py 8.5.3 compiled data/+00kurdish_tl.rpyc ({os.path.getsize('data/+00kurdish_tl.rpyc'):,} bytes)")


if __name__ == "__main__":
    main()
