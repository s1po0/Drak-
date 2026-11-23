"""Translate extracted game strings into Kurdish Sorani (ckb)."""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Callable, Dict, Optional

from deep_translator import GoogleTranslator

from .sorani_rtl import format_kurdish_rtl, protect_tags, restore_tags


def make_google_ckb_translator() -> Callable[[str], str]:
    translator = GoogleTranslator(source="auto", target="ckb")

    def _translate(text: str) -> str:
        return translator.translate(text)

    return _translate


def translate_manifest(
    input_json: Path,
    output_json: Path,
    translate_fn: Optional[Callable[[str], str]] = None,
    rtl_mode: str = "logical",
    cache_path: Optional[Path] = None,
    delay_seconds: float = 0.15,
) -> Dict[str, Any]:
    """Translate all entries in an extracted strings.json file to Kurdish Sorani.

    Args:
        input_json: Path to strings.json produced by apk_extractor.
        output_json: Destination path for translated JSON.
        translate_fn: Optional custom callable(text) -> translated_text.
            Defaults to GoogleTranslator(source='auto', target='ckb').
        rtl_mode: 'logical', 'visual', or 'pua'.
        cache_path: Optional JSON file storing original -> translated cache.
        delay_seconds: Polite delay between remote translation calls.
    """
    data = json.loads(input_json.read_text(encoding="utf-8"))
    entries = data.get("entries", [])

    if translate_fn is None:
        translate_fn = make_google_ckb_translator()

    cache: Dict[str, str] = {}
    if cache_path and cache_path.exists():
        cache = json.loads(cache_path.read_text(encoding="utf-8"))

    for entry in entries:
        if entry.get("translated"):
            continue
        orig = entry["original"]
        if orig in cache:
            translated_logical = cache[orig]
        else:
            protected, tokens = protect_tags(orig)
            raw_translated = translate_fn(protected)
            translated_logical = restore_tags(raw_translated, tokens)
            cache[orig] = translated_logical
            if cache_path:
                cache_path.parent.mkdir(parents=True, exist_ok=True)
                cache_path.write_text(
                    json.dumps(cache, ensure_ascii=False, indent=2),
                    encoding="utf-8",
                )
            if delay_seconds > 0:
                time.sleep(delay_seconds)

        entry["translated_logical"] = translated_logical
        entry["translated"] = format_kurdish_rtl(translated_logical, mode=rtl_mode)

    data["target_language"] = "ckb"
    data["rtl_mode"] = rtl_mode
    output_json.parent.mkdir(parents=True, exist_ok=True)
    output_json.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    return data
