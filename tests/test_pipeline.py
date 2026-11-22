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
from kurdish_game_translator.avgcreator import (
    extract_unique_en_strings,
    is_non_translatable,
    patch_game_json_with_translations,
    translate_unique_items,
    wrap_for_visual_rtl,
)
from kurdish_game_translator.sorani_rtl import format_kurdish_rtl, protect_tags, restore_tags
from kurdish_game_translator.translator import translate_manifest
from kurdish_game_translator.unityfs_stream import extract_primary_cab, repack_bundle_with_cab


def test_normalize_dropbox_url() -> None:
    src = "https://www.dropbox.com/scl/fi/abc/Game.apk?rlkey=xyz&st=123&dl=0"
    out = normalize_dropbox_url(src)
    assert "dl=1" in out
    assert "dl=0" not in out


def test_tag_protection_roundtrip() -> None:
    text = "<color=#ff0>Hello {player} and ${db.stat.1}!</color>\\nHP: %d [OK]"
    protected, tokens = protect_tags(text)
    assert "<color=#ff0>" not in protected
    assert "{player}" not in protected
    assert "${db.stat.1}" not in protected
    assert "\\n" not in protected
    simulated_translation = protected.replace("Hello", "سڵاو").replace("HP:", "تەندروستی:")
    restored = restore_tags(simulated_translation, tokens)
    assert "<color=#ff0>" in restored
    assert "{player}" in restored
    assert "${db.stat.1}" in restored
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


def test_avgcreator_extract_translate_and_patch(tmp_path: Path) -> None:
    assert is_non_translatable("/languageGetDisplayName(1)")
    assert is_non_translatable("${db.foo.1}")
    assert is_non_translatable("1234!")
    assert not is_non_translatable("${db.foo.1} %")
    assert not is_non_translatable("Start Game")

    game_data = {
        "scenes": [
            {
                "t": [
                    {"key": "text", "lang": "en-US", "value": "Start Game"},
                    {"key": "text", "lang": "zh-TW", "value": "開始遊戲"},
                    {"key": "text", "lang": "en-US", "value": "/languageGetDisplayName(1)"},
                    {"key": "/img", "lang": "en-US", "value": "bg/title.png"},
                ]
            },
            {
                "t": [
                    {"key": "text", "lang": "en-US", "value": "Start Game"},
                    {"key": "hp", "lang": "en-US", "value": "${db.hero.hp} HP"},
                ]
            },
        ]
    }
    game_json = tmp_path / "game.json"
    unique_json = tmp_path / "unique_en.json"
    tr_json = tmp_path / "unique_ckb.json"
    patched_json = tmp_path / "game_ckb.json"
    game_json.write_text(json.dumps(game_data), encoding="utf-8")

    ext_res = extract_unique_en_strings(game_json, unique_json)
    assert ext_res["total_unique"] == 2  # Start Game + ${db.hero.hp} HP

    translate_unique_items(
        unique_json,
        tr_json,
        translate_fn=lambda s: s.replace("Start Game", "دەستپێکردن").replace("HP", "تەندروستی"),
        cache_path=tmp_path / "cache.json",
    )
    patch_res = patch_game_json_with_translations(
        game_json, tr_json, patched_json, rtl_mode="logical"
    )
    assert patch_res["replaced_entries"] == 3
    patched = json.loads(patched_json.read_text(encoding="utf-8"))
    assert patched["scenes"][0]["t"][0]["value"] == "دەستپێکردن"
    assert patched["scenes"][0]["t"][2]["value"] == "/languageGetDisplayName(1)"
    assert patched["scenes"][0]["t"][3]["value"] == "bg/title.png"
    assert patched["scenes"][1]["t"][1]["value"] == "${db.hero.hp} تەندروستی"

    wrapped = wrap_for_visual_rtl("یەک دوو سێ چوار پێنج شەش حەوت هەشت نۆ دە یازدە دوازدە", max_width=24)
    assert "\n" in wrapped


def test_unityfs_stream_extract_and_repack(tmp_path: Path) -> None:
    import lz4.block
    import struct

    cab_orig = b"CAB_ORIGINAL_" * 15000  # ~195 KB (>1 block)
    ress_orig = b"RESS_DATA_BLOCK_" * 20000  # ~320 KB
    payload = cab_orig + ress_orig

    # Build a valid UnityFS v7 bundle with 128 KiB LZ4HC blocks
    blocks = []
    block_bytes = bytearray()
    for i in range(0, len(payload), 131072):
        chunk = payload[i : i + 131072]
        comp = lz4.block.compress(chunk, mode="high_compression", store_size=False)
        blocks.append((len(chunk), len(comp), 3))
        block_bytes.extend(comp)

    info = bytearray(b"\x00" * 16)
    info.extend(struct.pack(">i", len(blocks)))
    for u, c, fl in blocks:
        info.extend(struct.pack(">IIH", u, c, fl))
    nodes = [
        (0, len(cab_orig), 4, "CAB-test"),
        (len(cab_orig), len(ress_orig), 0, "CAB-test.resS"),
    ]
    info.extend(struct.pack(">i", len(nodes)))
    for off, sz, fl, nm in nodes:
        info.extend(struct.pack(">qqI", off, sz, fl))
        info.extend(nm.encode("utf-8") + b"\x00")
    comp_info = lz4.block.compress(bytes(info), mode="high_compression", store_size=False)

    hdr = bytearray(b"UnityFS\x00")
    hdr.extend(struct.pack(">i", 7))
    hdr.extend(b"5.x.x\x002021.3.5f1\x00")
    size_pos = len(hdr)
    hdr.extend(struct.pack(">qiii", 0, len(comp_info), len(info), 0x243))
    while len(hdr) % 16:
        hdr.append(0)
    hdr.extend(comp_info)
    while len(hdr) % 16:
        hdr.append(0)
    struct.pack_into(">q", hdr, size_pos, len(hdr) + len(block_bytes))

    bundle_path = tmp_path / "test.ab"
    bundle_path.write_bytes(bytes(hdr) + bytes(block_bytes))

    extracted_cab = tmp_path / "cab.bin"
    meta = extract_primary_cab(bundle_path, extracted_cab)
    assert meta["nodes"][0][3] == "CAB-test"
    assert extracted_cab.read_bytes() == cab_orig

    # Repack with larger modified CAB and verify both CAB and trailing resS stay intact
    new_cab = b"CAB_MODIFIED_KURDISH_" * 12000
    mod_cab_path = tmp_path / "mod_cab.bin"
    mod_cab_path.write_bytes(new_cab)
    out_bundle = tmp_path / "repacked.ab"
    repack_bundle_with_cab(bundle_path, mod_cab_path, out_bundle)

    re_extracted = tmp_path / "re_cab.bin"
    re_meta = extract_primary_cab(out_bundle, re_extracted)
    assert re_extracted.read_bytes() == new_cab
    assert re_meta["nodes"][1][0] == len(new_cab)
    assert re_meta["nodes"][1][1] == len(ress_orig)

    # Also verify resS bytes sequentially match original ress_orig
    with out_bundle.open("rb") as f:
        f.seek(re_meta["blocks_start"])
        stream = bytearray()
        for u, c, fl in re_meta["blocks"]:
            raw = f.read(c)
            stream.extend(lz4.block.decompress(raw, uncompressed_size=u) if (fl & 0x3F) in (2, 3) else raw)
    assert bytes(stream[len(new_cab) : len(new_cab) + len(ress_orig)]) == ress_orig


def test_cli_module_imports() -> None:
    import kurdish_game_translator.cli as cli

    assert callable(cli.main)



