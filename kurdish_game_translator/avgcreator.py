"""AvgCreator / PlayMeow Unity game.json localization & Kurdish font patching helpers."""

from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
import json
import re
import shutil
import tempfile
import textwrap
import threading
import time
import zipfile
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional

import requests
import UnityPy

from .apk_repacker import sign_apk_if_available
from .sorani_rtl import format_kurdish_rtl, protect_tags, restore_tags
from .unityfs_stream import extract_primary_cab, repack_bundle_with_cab

_PURE_EXPR_RE = re.compile(r"^\s*\$\{[^}]+\}\s*$")


def is_non_translatable(text: str) -> bool:
    """Return True for AvgCreator script expressions, pure templates, or pure punctuation/digits."""
    stripped = text.strip()
    if not stripped:
        return True
    if stripped.startswith("/"):
        return True
    if _PURE_EXPR_RE.match(stripped):
        return True
    # Pure punctuation / digits / whitespace (no letters)
    if not any(ch.isalpha() for ch in stripped):
        return True
    return False


def wrap_for_visual_rtl(text: str, max_width: int = 52) -> str:
    """Pre-wrap long lines before applying BiDi visual reversal so top/bottom lines stay in order."""
    if not text or len(text) <= max_width:
        return text
    wrapped_lines = []
    for paragraph in text.splitlines():
        if len(paragraph) <= max_width:
            wrapped_lines.append(paragraph)
        else:
            wrapped_lines.extend(
                textwrap.wrap(
                    paragraph,
                    width=max_width,
                    break_long_words=False,
                    break_on_hyphens=False,
                )
            )
    return "\n".join(wrapped_lines)


def extract_game_json_from_cab(cab_path: Path, out_game_json: Path, asset_name: str = "game") -> int:
    """Extract the 'game' TextAsset JSON from an extracted Unity CAB file."""
    env = UnityPy.load(str(cab_path))
    for obj in env.objects:
        if obj.type.name == "TextAsset":
            ta = obj.read()
            if getattr(ta, "m_Name", None) == asset_name:
                raw = ta.m_Script
                text = raw.decode("utf-8", errors="replace") if isinstance(raw, bytes) else str(raw)
                out_game_json.parent.mkdir(parents=True, exist_ok=True)
                out_game_json.write_text(text, encoding="utf-8")
                return obj.path_id
    raise FileNotFoundError(f"TextAsset {asset_name!r} not found in {cab_path}")


def extract_unique_en_strings(game_json_path: Path, out_unique_json: Path) -> Dict[str, Any]:
    """Collect all unique non-path 'en-US' strings from AvgCreator 't' translation arrays."""
    data = json.loads(game_json_path.read_text(encoding="utf-8"))
    unique_en: Dict[str, str] = {}

    def walk(node: Any) -> None:
        if isinstance(node, dict):
            t_list = node.get("t")
            if isinstance(t_list, list):
                by_key: Dict[str, Dict[str, Any]] = {}
                for item in t_list:
                    if isinstance(item, dict) and "lang" in item and "value" in item:
                        k = str(item.get("key", ""))
                        if k.startswith("/"):
                            continue
                        by_key.setdefault(k, {})[item["lang"]] = item["value"]
                for lmap in by_key.values():
                    en_val = lmap.get("en-US")
                    if en_val is not None and not is_non_translatable(str(en_val)):
                        s = str(en_val)
                        if s not in unique_en:
                            unique_en[s] = str(lmap.get("zh-TW") or lmap.get("zh-CN") or "")
            for k, v in node.items():
                if k != "t":
                    walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(data)
    items = [
        {"id": i, "en": s, "zh": unique_en[s], "ckb": ""}
        for i, s in enumerate(sorted(unique_en.keys(), key=lambda x: (len(x), x)))
    ]
    out_unique_json.parent.mkdir(parents=True, exist_ok=True)
    out_unique_json.write_text(json.dumps(items, ensure_ascii=False, indent=2), encoding="utf-8")
    return {"total_unique": len(items), "output": str(out_unique_json)}


def translate_unique_items(
    unique_json_path: Path,
    out_json_path: Path,
    translate_fn: Callable[[str], str],
    cache_path: Optional[Path] = None,
    max_workers: int = 1,
) -> Dict[str, Any]:
    """Translate unique_en.json items into logical Kurdish Sorani (ckb) with tag protection."""
    items = json.loads(unique_json_path.read_text(encoding="utf-8"))
    cache: Dict[str, str] = {}
    if cache_path and cache_path.exists():
        cache = json.loads(cache_path.read_text(encoding="utf-8"))

    pending = []
    for item in items:
        if item.get("ckb"):
            continue
        en = item["en"]
        if is_non_translatable(en):
            item["ckb"] = en
            continue
        if en in cache:
            item["ckb"] = cache[en]
            continue
        pending.append(item)

    def _translate_one(en_text: str) -> str:
        protected, tokens = protect_tags(en_text)
        last_err: Optional[Exception] = None
        for attempt in range(4):
            try:
                raw = translate_fn(protected)
                if raw:
                    return restore_tags(raw, tokens)
            except Exception as exc:
                last_err = exc
            time.sleep(0.5 * (2**attempt))
        if last_err:
            raise last_err
        return en_text

    if max_workers <= 1:
        for idx, item in enumerate(pending, 1):
            en = item["en"]
            translated = _translate_one(en)
            item["ckb"] = translated
            cache[en] = translated
            if cache_path and idx % 50 == 0:
                cache_path.parent.mkdir(parents=True, exist_ok=True)
                cache_path.write_text(json.dumps(cache, ensure_ascii=False, indent=2), encoding="utf-8")
    else:
        lock = threading.Lock()
        done_count = 0
        with ThreadPoolExecutor(max_workers=max_workers) as pool:
            fut_map = {pool.submit(_translate_one, item["en"]): item for item in pending}
            for fut in as_completed(fut_map):
                item = fut_map[fut]
                en = item["en"]
                try:
                    translated = fut.result()
                except Exception:
                    translated = en
                with lock:
                    item["ckb"] = translated
                    cache[en] = translated
                    done_count += 1
                    if cache_path and done_count % 100 == 0:
                        cache_path.parent.mkdir(parents=True, exist_ok=True)
                        cache_path.write_text(
                            json.dumps(cache, ensure_ascii=False, indent=2), encoding="utf-8"
                        )

    if cache_path:
        cache_path.parent.mkdir(parents=True, exist_ok=True)
        cache_path.write_text(json.dumps(cache, ensure_ascii=False, indent=2), encoding="utf-8")

    out_json_path.parent.mkdir(parents=True, exist_ok=True)
    out_json_path.write_text(json.dumps(items, ensure_ascii=False, indent=2), encoding="utf-8")
    return {"total": len(items), "translated_now": len(pending), "output": str(out_json_path)}


def gtx_translate_text(text: str, source: str = "en", target: str = "ckb", timeout: int = 15) -> str:
    """Translate text via Google Translate GTX endpoint with explicit timeout."""
    resp = requests.post(
        "https://translate.googleapis.com/translate_a/single",
        params={"client": "gtx", "sl": source, "tl": target, "dt": "t"},
        data={"q": text},
        timeout=timeout,
    )
    resp.raise_for_status()
    payload = resp.json()
    if not payload or not payload[0]:
        return text
    return "".join(seg[0] for seg in payload[0] if seg and seg[0])


def translate_unique_items_batched(
    unique_json_path: Path,
    out_json_path: Path,
    batch_fn: Optional[Callable[[str], str]] = None,
    cache_path: Optional[Path] = None,
    max_batch_chars: int = 3500,
    max_batch_items: int = 25,
) -> Dict[str, Any]:
    """Fast batched translator for unique_en.json (~60 requests for 2,000 strings)."""
    fn = batch_fn or (lambda s: gtx_translate_text(s, source="en", target="ckb"))
    items = json.loads(unique_json_path.read_text(encoding="utf-8"))
    cache: Dict[str, str] = {}
    if cache_path and cache_path.exists():
        cache = json.loads(cache_path.read_text(encoding="utf-8"))

    pending: List[Dict[str, Any]] = []
    for item in items:
        if item.get("ckb"):
            continue
        en = item["en"]
        if is_non_translatable(en):
            item["ckb"] = en
            continue
        if en in cache:
            item["ckb"] = cache[en]
            continue
        pending.append(item)

    sep = "\n§§§\n"
    idx = 0
    while idx < len(pending):
        batch: List[Dict[str, Any]] = []
        prot_list: List[str] = []
        tokens_list: List[List[str]] = []
        cur_chars = 0
        while idx < len(pending) and len(batch) < max_batch_items:
            p, toks = protect_tags(pending[idx]["en"])
            if batch and (cur_chars + len(p) + len(sep) > max_batch_chars):
                break
            batch.append(pending[idx])
            prot_list.append(p)
            tokens_list.append(toks)
            cur_chars += len(p) + len(sep)
            idx += 1

        joined = sep.join(prot_list)
        parts: Optional[List[str]] = None
        for attempt in range(3):
            try:
                tr_joined = fn(joined)
                candidate = [s.strip() for s in re.split(r"\n?\s*§§§\s*\n?", tr_joined)]
                if len(candidate) == len(batch):
                    parts = candidate
                    break
            except Exception:
                time.sleep(1.0 * (attempt + 1))

        if parts is None:
            # Fallback to single-item translation for this batch
            parts = []
            for p in prot_list:
                out_p = p
                for attempt in range(3):
                    try:
                        out_p = fn(p)
                        break
                    except Exception:
                        time.sleep(0.5 * (attempt + 1))
                parts.append(out_p)

        for item, tr_part, toks in zip(batch, parts, tokens_list):
            restored = restore_tags(tr_part, toks)
            item["ckb"] = restored
            cache[item["en"]] = restored

        if cache_path:
            cache_path.parent.mkdir(parents=True, exist_ok=True)
            cache_path.write_text(json.dumps(cache, ensure_ascii=False, indent=2), encoding="utf-8")

    out_json_path.parent.mkdir(parents=True, exist_ok=True)
    out_json_path.write_text(json.dumps(items, ensure_ascii=False, indent=2), encoding="utf-8")
    return {"total": len(items), "translated_now": len(pending), "output": str(out_json_path)}


def patch_game_json_with_translations(
    game_json_path: Path,
    translated_unique_json: Path,
    out_game_json: Path,
    target_lang: str = "en-US",
    rtl_mode: str = "visual",
    wrap_width: int = 52,
) -> Dict[str, int]:
    """Replace target_lang ('en-US') entries in game.json with formatted Kurdish Sorani."""
    items = json.loads(translated_unique_json.read_text(encoding="utf-8"))
    tr_map: Dict[str, str] = {}
    for item in items:
        ckb = item.get("ckb", "").strip()
        if ckb:
            if rtl_mode == "visual":
                wrapped = wrap_for_visual_rtl(ckb, max_width=wrap_width)
                tr_map[item["en"]] = format_kurdish_rtl(wrapped, mode="visual")
            else:
                tr_map[item["en"]] = format_kurdish_rtl(ckb, mode=rtl_mode)

    data = json.loads(game_json_path.read_text(encoding="utf-8"))
    replaced = 0

    def walk(node: Any) -> None:
        nonlocal replaced
        if isinstance(node, dict):
            t_list = node.get("t")
            if isinstance(t_list, list):
                for item in t_list:
                    if (
                        isinstance(item, dict)
                        and item.get("lang") == target_lang
                        and not str(item.get("key", "")).startswith("/")
                    ):
                        orig = str(item.get("value", ""))
                        if orig in tr_map:
                            item["value"] = tr_map[orig]
                            replaced += 1
            for k, v in node.items():
                if k != "t":
                    walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(data)
    out_game_json.parent.mkdir(parents=True, exist_ok=True)
    out_game_json.write_text(
        json.dumps(data, ensure_ascii=False, separators=(",", ":")), encoding="utf-8"
    )
    return {"replaced_entries": replaced, "unique_translations": len(tr_map)}


def patch_cab_game_json(
    source_cab: Path,
    patched_game_json: Path,
    out_cab: Path,
    asset_name: str = "game",
) -> None:
    """Write modified game.json back into a Unity CAB serialized file."""
    env = UnityPy.load(str(source_cab))
    new_text = patched_game_json.read_text(encoding="utf-8")
    updated = False
    for obj in env.objects:
        if obj.type.name == "TextAsset":
            ta = obj.read()
            if getattr(ta, "m_Name", None) == asset_name:
                ta.m_Script = new_text
                ta.save()
                updated = True
                break
    if not updated:
        raise FileNotFoundError(f"TextAsset {asset_name!r} not found in {source_cab}")
    out_cab.parent.mkdir(parents=True, exist_ok=True)
    out_cab.write_bytes(env.file.save())


def patch_unity_font_file(
    source_font_asset: Path,
    ttf_font_path: Path,
    out_font_asset: Path,
) -> str:
    """Replace m_FontData inside a Unity bin/Data Font asset with a Kurdish-capable TTF."""
    ttf_bytes = ttf_font_path.read_bytes()
    env = UnityPy.load(str(source_font_asset))
    font_name = ""
    for obj in env.objects:
        if obj.type.name == "Font":
            tree = obj.read_typetree()
            font_name = str(tree.get("m_Name", "Font"))
            tree["m_FontData"] = list(ttf_bytes)
            obj.save_typetree(tree)
    if not font_name:
        raise ValueError(f"No Font object found in {source_font_asset}")
    out_font_asset.parent.mkdir(parents=True, exist_ok=True)
    out_font_asset.write_bytes(env.file.save())
    return font_name


def repack_avg_apk(
    source_apk: Path,
    translated_unique_json: Path,
    output_apk: Path,
    ttf_regular: Optional[Path] = None,
    ttf_bold: Optional[Path] = None,
    rtl_mode: str = "visual",
    wrap_width: int = 52,
    font_asset_names: Optional[List[str]] = None,
) -> Dict[str, Any]:
    """Patch android.ab + bin/Data Kurdish fonts inside an AvgCreator APK and sign it."""
    target_fonts = set(
        font_asset_names
        or [
            "20c529ce41df043bba11419a7bb4ec8d",  # Inter-Regular
            "6f78546fd16db4d2c827303b6c7f13c9",  # Inter-Bold
            "99dda42022873461eaf67aac0f6dcbb6",  # NotoSansCJKsc-Medium
        ]
    )
    bold_font_id = "6f78546fd16db4d2c827303b6c7f13c9"

    with tempfile.TemporaryDirectory() as tmpdir_str:
        tmp = Path(tmpdir_str)
        orig_ab = tmp / "orig_android.ab"
        patched_ab = tmp / "patched_android.ab"
        cab_path = tmp / "primary.cab"
        game_json = tmp / "game.json"
        patched_json = tmp / "game_ckb.json"
        patched_cab = tmp / "primary_ckb.cab"

        ab_arcname: Optional[str] = None
        ab_compress_type = zipfile.ZIP_DEFLATED

        with zipfile.ZipFile(source_apk, "r") as zin:
            for item in zin.infolist():
                if item.filename.endswith("/android.ab") or item.filename == "android.ab":
                    ab_arcname = item.filename
                    ab_compress_type = item.compress_type
                    with zin.open(item) as src, orig_ab.open("wb") as dst:
                        shutil.copyfileobj(src, dst, 4 * 1024 * 1024)
                    break

        if not ab_arcname:
            raise FileNotFoundError(f"android.ab not found inside {source_apk}")

        extract_primary_cab(orig_ab, cab_path)
        extract_game_json_from_cab(cab_path, game_json)
        p_stats = patch_game_json_with_translations(
            game_json,
            translated_unique_json,
            patched_json,
            rtl_mode=rtl_mode,
            wrap_width=wrap_width,
        )
        patch_cab_game_json(cab_path, patched_json, patched_cab)
        b_stats = repack_bundle_with_cab(orig_ab, patched_cab, patched_ab)

        patched_fonts: List[str] = []
        output_apk.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(source_apk, "r") as zin, zipfile.ZipFile(
            output_apk, "w", compression=zipfile.ZIP_DEFLATED
        ) as zout:
            for item in zin.infolist():
                if item.filename.startswith("META-INF/") and item.filename.endswith(
                    (".SF", ".RSA", ".DSA", ".EC", "MANIFEST.MF")
                ):
                    continue

                compress_type = (
                    zipfile.ZIP_STORED
                    if item.compress_type == zipfile.ZIP_STORED
                    else zipfile.ZIP_DEFLATED
                )

                if item.filename == ab_arcname:
                    zout.write(
                        patched_ab,
                        arcname=ab_arcname,
                        compress_type=(
                            zipfile.ZIP_STORED
                            if ab_compress_type == zipfile.ZIP_STORED
                            else zipfile.ZIP_DEFLATED
                        ),
                    )
                    continue

                base = Path(item.filename).name
                if ttf_regular and item.filename.lower().startswith("assets/bin/data/") and base in target_fonts:
                    src_font = tmp / f"src_{base}"
                    out_font = tmp / f"out_{base}"
                    with zin.open(item) as src, src_font.open("wb") as dst:
                        shutil.copyfileobj(src, dst)
                    chosen_ttf = ttf_bold if (base == bold_font_id and ttf_bold) else ttf_regular
                    fname = patch_unity_font_file(src_font, chosen_ttf, out_font)
                    patched_fonts.append(f"{fname} ({base})")
                    zout.write(out_font, arcname=item.filename, compress_type=compress_type)
                    continue

                # Stream copy all remaining entries without buffering huge blobs in RAM
                out_info = zipfile.ZipInfo(item.filename, date_time=item.date_time)
                out_info.compress_type = compress_type
                with zin.open(item) as src, zout.open(out_info, "w") as dst:
                    shutil.copyfileobj(src, dst, 4 * 1024 * 1024)

    signer = sign_apk_if_available(output_apk)
    return {
        "output_apk": str(output_apk),
        "android_ab_entry": ab_arcname,
        "patched_fonts": patched_fonts,
        "signed_with": signer,
        **p_stats,
        "repacked_ab_bytes": b_stats["total_size"],
    }
