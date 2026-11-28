#!/usr/bin/env python3
"""
Test loading our generated x-+00kurdish_tl.rpyc and sample patched .rpyc
inside the REAL Ren'Py 8.5.3 SDK Cython runtime!
"""
import gzip
import json
import os
import shutil
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

sdk_dir = sys.argv[1]
os.makedirs("/tmp/test_proj/game/src", exist_ok=True)

# Import build_translate_strings_rpyc without needing local unrpyc
with open("tools/build_kurdish_zips.py", "r", encoding="utf-8") as f:
    src = f.read()

# Strip unrpyc import for standalone SDK test
src = src.replace("import unrpyc", "unrpyc = None")
ns = {"__file__": os.path.abspath("tools/build_kurdish_zips.py")}
exec(compile(src, "tools/build_kurdish_zips.py", "exec"), ns)

with gzip.open("data/translations_ckb.json.gz", "rt", encoding="utf-8") as gz:
    trans = json.load(gz)

tl_bytes = ns["build_translate_strings_rpyc"](trans)
with open("/tmp/test_proj/game/src/+00kurdish_tl.rpyc", "wb") as f:
    f.write(tl_bytes)

# Write a minimal script.rpy that tests translation + runs lint!
with open("/tmp/test_proj/game/test_main.rpy", "w", encoding="utf-8") as f:
    f.write(
        'label start:\n'
        '    $ assert _("Leave the room.") != "Leave the room.", "Translation missing!"\n'
        '    "Hello"\n'
        '    return\n'
    )

env = dict(os.environ)
env["SDL_VIDEODRIVER"] = "dummy"
env["SDL_AUDIODRIVER"] = "dummy"
env["RENPY_RPYC_EXCEPTIONS"] = "1"

cmd = [os.path.join(sdk_dir, "renpy.sh"), "/tmp/test_proj", "lint"]
print("Running:", cmd, flush=True)
proc = subprocess.run(cmd, env=env, text=True, capture_output=True)
print("STDOUT:\n", proc.stdout)
print("STDERR:\n", proc.stderr)
for fn in ["/tmp/test_proj/traceback.txt", "/tmp/test_proj/log.txt"]:
    if os.path.exists(fn):
        print(f"=== {fn} ===")
        with open(fn) as lf:
            print(lf.read()[-4000:])
sys.exit(proc.returncode)
