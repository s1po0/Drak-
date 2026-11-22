"""Extract translatable strings from Unity Android APKs using UnityPy."""

from __future__ import annotations

import json
import re
import zipfile
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional
from urllib.parse import parse_qs, urlencode, urlparse, urlunparse

import requests
import UnityPy

TEXT_EXTENSIONS = {".json", ".txt", ".csv", ".tsv", ".xml", ".yaml", ".yml"}
# Skip purely technical strings (paths, hashes, single symbols, numbers)
SKIP_PATTERN = re.compile(
    r"^(\s*|[\d\s.,:%+\-*/()\[\]{}!?@#$^&_=|\\<>\"'`~]+|[a-zA-Z0-9_./\\-]+\.(png|jpg|jpeg|webp|wav|mp3|ogg|prefab|mat|shader|anim|controller))$",
    re.IGNORECASE,
)


@dataclass
class ExtractedEntry:
    id: str
    source_type: str  # "raw_asset" | "unity_text_asset" | "unity_mono_behaviour"
    container_path: str
    asset_name: str
    path_id: Optional[int]
    json_key_path: Optional[str]
    original: str
    translated: str = ""


def normalize_dropbox_url(url: str) -> str:
    """Convert a Dropbox share URL (?dl=0) to a direct download URL (?dl=1)."""
    parsed = urlparse(url)
    if "dropbox.com" not in parsed.netloc.lower():
        return url
    query = parse_qs(parsed.query, keep_blank_values=True)
    query["dl"] = ["1"]
    new_query = urlencode(query, doseq=True)
    return urlunparse(parsed._replace(query=new_query))


def download_apk(url: str, dest_path: Path, timeout: int = 120) -> Path:
    """Download an APK from a direct URL or Dropbox share link."""
    direct_url = normalize_dropbox_url(url)
    dest_path.parent.mkdir(parents=True, exist_ok=True)
    with requests.get(direct_url, stream=True, timeout=timeout, allow_redirects=True) as resp:
        resp.raise_for_status()
        with dest_path.open("wb") as fh:
            for chunk in resp.iter_content(chunk_size=1024 * 1024):
                if chunk:
                    fh.write(chunk)
    return dest_path


def _is_translatable_text(value: str) -> bool:
    stripped = value.strip()
    if len(stripped) < 2:
        return False
    if SKIP_PATTERN.match(stripped):
        return False
    # Must contain at least one letter (Latin, CJK, Cyrillic, Arabic, etc.)
    return any(ch.isalpha() for ch in stripped)


def _walk_json_strings(
    node: Any, prefix: str = ""
) -> List[tuple[str, str]]:
    found: List[tuple[str, str]] = []
    if isinstance(node, dict):
        for k, v in node.items():
            key_path = f"{prefix}.{k}" if prefix else str(k)
            found.extend(_walk_json_strings(v, key_path))
    elif isinstance(node, list):
        for idx, item in enumerate(node):
            key_path = f"{prefix}[{idx}]"
            found.extend(_walk_json_strings(item, key_path))
    elif isinstance(node, str) and _is_translatable_text(node):
        found.append((prefix, node))
    return found


def _extract_text_blob(
    text: str,
    source_type: str,
    container_path: str,
    asset_name: str,
    path_id: Optional[int],
    counter_start: int,
) -> List[ExtractedEntry]:
    entries: List[ExtractedEntry] = []
    idx = counter_start
    stripped = text.lstrip()
    if stripped.startswith("{") or stripped.startswith("["):
        try:
            parsed = json.loads(text)
            for key_path, val in _walk_json_strings(parsed):
                entries.append(
                    ExtractedEntry(
                        id=f"str_{idx:06d}",
                        source_type=source_type,
                        container_path=container_path,
                        asset_name=asset_name,
                        path_id=path_id,
                        json_key_path=key_path,
                        original=val,
                    )
                )
                idx += 1
            return entries
        except json.JSONDecodeError:
            pass

    for line_no, line in enumerate(text.splitlines()):
        if _is_translatable_text(line):
            entries.append(
                ExtractedEntry(
                    id=f"str_{idx:06d}",
                    source_type=source_type,
                    container_path=container_path,
                    asset_name=asset_name,
                    path_id=path_id,
                    json_key_path=f"line:{line_no}",
                    original=line,
                )
            )
            idx += 1
    return entries


def _is_unity_data_member(member_name: str) -> bool:
    lower = member_name.lower()
    if not lower.startswith("assets/"):
        return False
    if lower.endswith((".assets", ".ress", ".resource", ".bundle", ".unity3d")):
        return True
    if "assets/bin/data/" in lower and not lower.endswith(".ress"):
        return True
    return False


def extract_from_apk(apk_path: Path, output_dir: Path) -> Dict[str, Any]:
    """Extract translatable strings and asset inventory from a Unity APK."""
    output_dir.mkdir(parents=True, exist_ok=True)
    raw_dir = output_dir / "raw_assets"
    raw_dir.mkdir(parents=True, exist_ok=True)

    entries: List[ExtractedEntry] = []
    unity_members: List[str] = []
    raw_text_members: List[str] = []
    fonts_found: List[str] = []

    with zipfile.ZipFile(apk_path, "r") as zf:
        members = zf.namelist()
        is_il2cpp = any("libil2cpp.so" in m for m in members)
        has_mono = any("assets/bin/data/managed/" in m.lower() for m in members)

        for member in members:
            lower = member.lower()
            if member.endswith("/"):
                continue

            # 1. Direct text/JSON/CSV files in APK assets/
            if lower.startswith("assets/") and Path(lower).suffix in TEXT_EXTENSIONS:
                raw_bytes = zf.read(member)
                try:
                    text = raw_bytes.decode("utf-8")
                except UnicodeDecodeError:
                    text = raw_bytes.decode("utf-8-sig", errors="replace")
                raw_text_members.append(member)
                extracted = _extract_text_blob(
                    text=text,
                    source_type="raw_asset",
                    container_path=member,
                    asset_name=Path(member).name,
                    path_id=None,
                    counter_start=len(entries) + 1,
                )
                entries.extend(extracted)
                continue

            # 2. Unity serialized files & AssetBundles
            if _is_unity_data_member(member):
                unity_members.append(member)
                data = zf.read(member)
                try:
                    env = UnityPy.load(data)
                except Exception:
                    continue

                for obj in env.objects:
                    if obj.type.name == "Font":
                        try:
                            font_data = obj.read()
                            fonts_found.append(f"{member}:{getattr(font_data, 'm_Name', 'Font')}")
                        except Exception:
                            pass
                    elif obj.type.name == "TextAsset":
                        try:
                            ta = obj.read()
                            raw_script = ta.m_Script
                            if isinstance(raw_script, bytes):
                                text = raw_script.decode("utf-8", errors="replace")
                            else:
                                text = str(raw_script)
                            extracted = _extract_text_blob(
                                text=text,
                                source_type="unity_text_asset",
                                container_path=member,
                                asset_name=getattr(ta, "m_Name", f"TextAsset_{obj.path_id}"),
                                path_id=obj.path_id,
                                counter_start=len(entries) + 1,
                            )
                            entries.extend(extracted)
                        except Exception:
                            continue
                    elif obj.type.name == "MonoBehaviour":
                        try:
                            tree = obj.read_typetree()
                            if isinstance(tree, dict):
                                asset_name = str(tree.get("m_Name") or f"MonoBehaviour_{obj.path_id}")
                                for key_path, val in _walk_json_strings(tree):
                                    if key_path == "m_Name":
                                        continue
                                    entries.append(
                                        ExtractedEntry(
                                            id=f"str_{len(entries) + 1:06d}",
                                            source_type="unity_mono_behaviour",
                                            container_path=member,
                                            asset_name=asset_name,
                                            path_id=obj.path_id,
                                            json_key_path=key_path,
                                            original=val,
                                        )
                                    )
                        except Exception:
                            # MonoBehaviour without embedded TypeTree (common in IL2CPP without dump)
                            continue

    manifest = {
        "apk_name": apk_path.name,
        "engine": "Unity",
        "scripting_backend": "IL2CPP" if is_il2cpp else ("Mono" if has_mono else "Unknown"),
        "unity_asset_files": unity_members,
        "raw_text_files": raw_text_members,
        "fonts_found": fonts_found,
        "total_strings": len(entries),
        "entries": [asdict(e) for e in entries],
    }

    out_json = output_dir / "strings.json"
    out_json.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    return manifest
