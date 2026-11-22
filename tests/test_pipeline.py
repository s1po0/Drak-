"""Tests for Kurdish Sorani APK extraction, RTL shaping, translation, and repacking."""

from __future__ import annotations

import json
import zipfile
from pathlib import Path

from kurdish_game_translator.apk_extractor import (
    extract_from_apk,
    extract_from_path,
    normalize_dropbox_url,
)
from kurdish_game_translator.apk_repacker import repack_apk
from kurdish_game_translator.sorani_rtl import format_kurdish_rtl, protect_tags, restore_tags
from kurdish_game_translator.translator import translate_manifest


def test_normalize_dropbox_url() -> None:
    src = "https://www.dropbox.com/scl/fi/abc/Game.apk?rlkey=xyz&st=123&dl=0"
    out = normalize_dropbox_url(src)
    assert "dl=1" in out
    assert "dl=0" not in out


def test_tag_protection_roundtrip() -> None:
    text = "<color=#ff0>Hello {player}!</color>\\nHP: %d [OK]"
    protected, tokens = protect_tags(text)
    assert "<color=#ff0>" not in protected
    assert "{player}" not in protected
    assert "\\n" not in protected
    simulated_translation = protected.replace("Hello", "سڵاو").replace("HP:", "تەندروستی:")
    restored = restore_tags(simulated_translation, tokens)
    assert "<color=#ff0>" in restored
    assert "{player}" in restored
    assert "\\n" in restored
    assert "%d" in restored
    assert "[OK]" in restored


def test_kurdish_rtl_visual_no_pua_and_joins_neighbors() -> None:
    sample = "سڵاو فڕێیا! ئەڤە کوردیی سۆرانییە"
    assert format_kurdish_rtl(sample, mode="logical") == sample

    visual = format_kurdish_rtl(sample, mode="visual")
    # Must not emit legacy Unikurd PUA glyphs (U+E000..U+E0FF)
    assert all(not (0xE000 <= ord(ch) <= 0xE0FF) for ch in visual)
    # In 'سڵاو', 'س' connects to 'ڵ' using its initial form U+FEB3 ('ﺳ')
    assert "\ufeb3" in visual
    # In 'ئەڤە', 'ئ' connects to 'ە' using its initial form U+FE8B ('ﺋ')
    assert "\ufe8b" in visual


def test_apk_extract_translate_repack_roundtrip(tmp_path: Path) -> None:
    apk_path = tmp_path / "sample.apk"
    work_dir = tmp_path / "work"
    out_apk = tmp_path / "sample_ckb.apk"

    sample_json = {
        "title": "Fallen Elf Freya",
        "dialogue": [
            {"speaker": "Otto", "line": "Welcome to Falandir, {name}!"},
            {"speaker": "Freya", "line": "I will never surrender."},
        ],
        "stats": {"hp": 100},
    }
    sample_txt = "Start Game\n12345\nOptions\n"

    with zipfile.ZipFile(apk_path, "w") as zf:
        zf.writestr("META-INF/CERT.RSA", b"fake-signature")
        zf.writestr("lib/arm64-v8a/libil2cpp.so", b"\x7fELF")
        zf.writestr("assets/localization/en.json", json.dumps(sample_json))
        zf.writestr("assets/menu.txt", sample_txt)

    manifest = extract_from_apk(apk_path, work_dir)
    assert manifest["scripting_backend"] == "IL2CPP"
    assert manifest["total_strings"] == 7

    translations = {
        "Fallen Elf Freya": "ئێلفی کەوتوو فرێیا",
        "Otto": "ئۆتۆ",
        "Welcome to Falandir, @@TAG0@@!": "بەخێربێیت بۆ فالاندیر، @@TAG0@@!",
        "Freya": "فرێیا",
        "I will never surrender.": "هەرگیز خۆم بەدەستەوە نادەم.",
        "Start Game": "دەستپێکردنی یاری",
        "Options": "ڕێکخستنەکان",
    }

    translated = translate_manifest(
        input_json=work_dir / "strings.json",
        output_json=work_dir / "strings_ckb.json",
        translate_fn=lambda s: translations.get(s, s),
        rtl_mode="logical",
        cache_path=work_dir / "cache.json",
        delay_seconds=0,
    )
    assert translated["target_language"] == "ckb"

    summary = repack_apk(
        source_apk=apk_path,
        translated_json=work_dir / "strings_ckb.json",
        output_apk=out_apk,
    )
    assert summary["patched_files"] == 2
    assert summary["patched_entries"] == 7

    with zipfile.ZipFile(out_apk, "r") as zout:
        names = zout.namelist()
        assert "META-INF/CERT.RSA" not in names
        new_json = json.loads(zout.read("assets/localization/en.json").decode("utf-8"))
        assert new_json["title"] == "ئێلفی کەوتوو فرێیا"
        assert new_json["dialogue"][0]["line"] == "بەخێربێیت بۆ فالاندیر، {name}!"
        assert new_json["stats"]["hp"] == 100
        new_txt = zout.read("assets/menu.txt").decode("utf-8")
        assert "دەستپێکردنی یاری" in new_txt
        assert "12345" in new_txt
        assert "ڕێکخستنەکان" in new_txt


def test_extract_from_unpacked_directory(tmp_path: Path) -> None:
    unpacked = tmp_path / "FallenElfFreya"
    (unpacked / "bin" / "Data" / "Managed" / "Metadata").mkdir(parents=True)
    (unpacked / "bin" / "Data" / "Managed" / "Metadata" / "global-metadata.dat").write_bytes(b"meta")
    (unpacked / "localization.json").write_text(
        json.dumps({"greeting": "Hello Freya"}), encoding="utf-8"
    )
    out_dir = tmp_path / "out"
    manifest = extract_from_path(unpacked, out_dir)
    assert manifest["scripting_backend"] == "IL2CPP"
    assert manifest["total_strings"] == 1
    assert manifest["entries"][0]["original"] == "Hello Freya"

