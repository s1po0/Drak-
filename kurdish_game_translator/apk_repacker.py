"""Repack translated Kurdish Sorani strings (and optional TTF font) into a Unity APK."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import tempfile
import zipfile
from collections import defaultdict
from pathlib import Path
from typing import Any, Dict, List, Optional

import UnityPy

ARRAY_INDEX_RE = re.compile(r"^(.+?)\[(\d+)\]$")


def _set_json_path(root: Any, key_path: str, value: str) -> bool:
    """Update a nested dict/list string by dot+bracket path like 'dialogue[0].text'."""
    parts = key_path.split(".")
    curr = root
    for i, part in enumerate(parts):
        is_last = i == len(parts) - 1
        m = ARRAY_INDEX_RE.match(part)
        if m:
            field_name, idx_str = m.group(1), int(m.group(2))
            if not isinstance(curr, dict) or field_name not in curr:
                return False
            arr = curr[field_name]
            if not isinstance(arr, list) or idx_str >= len(arr):
                return False
            if is_last:
                arr[idx_str] = value
                return True
            curr = arr[idx_str]
        elif part.startswith("[") and part.endswith("]"):
            idx = int(part[1:-1])
            if not isinstance(curr, list) or idx >= len(curr):
                return False
            if is_last:
                curr[idx] = value
                return True
            curr = curr[idx]
        else:
            if not isinstance(curr, dict) or part not in curr:
                return False
            if is_last:
                curr[part] = value
                return True
            curr = curr[part]
    return False


def _apply_entries_to_text(raw_text: str, entries: List[Dict[str, Any]]) -> str:
    stripped = raw_text.lstrip()
    is_json = stripped.startswith("{") or stripped.startswith("[")
    if is_json and any(not str(e.get("json_key_path", "")).startswith("line:") for e in entries):
        try:
            parsed = json.loads(raw_text)
            for entry in entries:
                kp = entry.get("json_key_path")
                tr = entry.get("translated")
                if kp and tr:
                    _set_json_path(parsed, kp, tr)
            return json.dumps(parsed, ensure_ascii=False, indent=2)
        except json.JSONDecodeError:
            pass

    lines = raw_text.splitlines()
    for entry in entries:
        kp = str(entry.get("json_key_path") or "")
        tr = entry.get("translated")
        if kp.startswith("line:") and tr:
            line_no = int(kp.split(":", 1)[1])
            if 0 <= line_no < len(lines):
                lines[line_no] = tr
    return "\n".join(lines) + ("\n" if raw_text.endswith("\n") else "")


def sign_apk_if_available(apk_path: Path) -> Optional[str]:
    """Attempt to sign the APK with apksigner or jarsigner if installed."""
    apksigner = shutil.which("apksigner")
    keytool = shutil.which("keytool")
    jarsigner = shutil.which("jarsigner")
    if not (keytool and (apksigner or jarsigner)):
        return None

    with tempfile.TemporaryDirectory() as tmpdir:
        keystore = Path(tmpdir) / "debug.keystore"
        subprocess.run(
            [
                keytool,
                "-genkey",
                "-v",
                "-keystore",
                str(keystore),
                "-alias",
                "androiddebugkey",
                "-storepass",
                "android",
                "-keypass",
                "android",
                "-keyalg",
                "RSA",
                "-keysize",
                "2048",
                "-validity",
                "10000",
                "-dname",
                "CN=KurdishGameTranslator,O=Community,C=IQ",
            ],
            check=True,
            capture_output=True,
        )
        if apksigner:
            subprocess.run(
                [
                    apksigner,
                    "sign",
                    "--ks",
                    str(keystore),
                    "--ks-pass",
                    "pass:android",
                    "--ks-key-alias",
                    "androiddebugkey",
                    "--key-pass",
                    "pass:android",
                    str(apk_path),
                ],
                check=True,
                capture_output=True,
            )
            return "apksigner"
        if jarsigner:
            subprocess.run(
                [
                    jarsigner,
                    "-keystore",
                    str(keystore),
                    "-storepass",
                    "android",
                    "-keypass",
                    "android",
                    str(apk_path),
                    "androiddebugkey",
                ],
                check=True,
                capture_output=True,
            )
            return "jarsigner"
    return None


def repack_apk(
    source_apk: Path,
    translated_json: Path,
    output_apk: Path,
    font_ttf_path: Optional[Path] = None,
) -> Dict[str, Any]:
    """Write translated strings (and optional Kurdish TTF font) into a new APK."""
    manifest = json.loads(translated_json.read_text(encoding="utf-8"))
    entries = [e for e in manifest.get("entries", []) if e.get("translated")]

    by_container: Dict[str, List[Dict[str, Any]]] = defaultdict(list)
    for entry in entries:
        by_container[entry["container_path"]].append(entry)

    font_bytes = font_ttf_path.read_bytes() if font_ttf_path else None
    patched_files = 0
    patched_entries = 0
    patched_fonts = 0

    output_apk.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(source_apk, "r") as zin, zipfile.ZipFile(
        output_apk, "w", compression=zipfile.ZIP_DEFLATED
    ) as zout:
        for item in zin.infolist():
            # Strip old APK v1 signature files so re-signing stays clean
            if item.filename.startswith("META-INF/") and item.filename.endswith(
                (".SF", ".RSA", ".DSA", ".EC", "MANIFEST.MF")
            ):
                continue

            data = zin.read(item.filename)
            container_entries = by_container.get(item.filename, [])

            if container_entries and all(
                e["source_type"] == "raw_asset" for e in container_entries
            ):
                raw_text = data.decode("utf-8", errors="replace")
                new_text = _apply_entries_to_text(raw_text, container_entries)
                data = new_text.encode("utf-8")
                patched_files += 1
                patched_entries += len(container_entries)
            elif container_entries or (
                font_bytes is not None and item.filename.lower().startswith("assets/bin/data/")
            ):
                try:
                    env = UnityPy.load(data)
                    modified = False

                    entries_by_pid: Dict[Optional[int], List[Dict[str, Any]]] = defaultdict(list)
                    for e in container_entries:
                        entries_by_pid[e.get("path_id")].append(e)

                    for obj in env.objects:
                        if obj.type.name == "Font" and font_bytes is not None:
                            try:
                                font_obj = obj.read()
                                if hasattr(font_obj, "m_FontData") and font_obj.m_FontData:
                                    font_obj.m_FontData = list(font_bytes)
                                    font_obj.save()
                                    patched_fonts += 1
                                    modified = True
                            except Exception:
                                pass

                        pid_entries = entries_by_pid.get(obj.path_id)
                        if not pid_entries:
                            continue

                        if obj.type.name == "TextAsset":
                            ta = obj.read()
                            script = ta.m_Script
                            orig_text = (
                                script.decode("utf-8", errors="replace")
                                if isinstance(script, bytes)
                                else str(script)
                            )
                            updated = _apply_entries_to_text(orig_text, pid_entries)
                            ta.m_Script = updated
                            ta.save()
                            patched_entries += len(pid_entries)
                            modified = True
                        elif obj.type.name == "MonoBehaviour":
                            tree = obj.read_typetree()
                            if isinstance(tree, dict):
                                for e in pid_entries:
                                    kp = e.get("json_key_path")
                                    if kp and _set_json_path(tree, kp, e["translated"]):
                                        patched_entries += 1
                                        modified = True
                                obj.save_typetree(tree)

                    if modified:
                        data = env.file.save()
                        patched_files += 1
                except Exception:
                    pass

            # Preserve uncompressed storage for native libs / resources.arsc when required by Android
            compress_type = (
                zipfile.ZIP_STORED
                if item.compress_type == zipfile.ZIP_STORED
                else zipfile.ZIP_DEFLATED
            )
            out_info = zipfile.ZipInfo(item.filename, date_time=item.date_time)
            out_info.compress_type = compress_type
            zout.writestr(out_info, data)

    signer = sign_apk_if_available(output_apk)
    return {
        "output_apk": str(output_apk),
        "patched_files": patched_files,
        "patched_entries": patched_entries,
        "patched_fonts": patched_fonts,
        "signed_with": signer,
    }
