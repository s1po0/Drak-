"""CLI for extracting, translating to Kurdish Sorani, and repacking Unity APKs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from .apk_extractor import download_apk, extract_from_path
from .apk_repacker import repack_apk
from .avgcreator import (
    extract_game_json_from_cab,
    extract_unique_en_strings,
    patch_cab_game_json,
    patch_game_json_with_translations,
    patch_unity_font_file,
    translate_unique_items,
)
from .translator import translate_manifest
from .unityfs_stream import extract_primary_cab, repack_bundle_with_cab


def _resolve_apk(source: str, work_dir: Path) -> Path:
    if source.startswith(("http://", "https://")):
        dest = work_dir / "downloaded_game.apk"
        print(f"Downloading APK from {source} -> {dest} ...")
        return download_apk(source, dest)
    path = Path(source)
    if not path.exists():
        raise FileNotFoundError(f"Source path not found: {path}")
    return path


def main() -> None:
    parser = argparse.ArgumentParser(
        prog="kurdish-game-translate",
        description="Extract, translate to Kurdish Sorani (ckb), and repack Unity APKs.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_ext = sub.add_parser("extract", help="Extract translatable strings from an APK or Dropbox URL")
    p_ext.add_argument("source", help="Local .apk path or Dropbox share URL")
    p_ext.add_argument("-o", "--output", default="work", help="Output workspace directory")

    p_tr = sub.add_parser("translate", help="Translate extracted strings.json to Kurdish Sorani")
    p_tr.add_argument("input_json", help="Path to extracted strings.json")
    p_tr.add_argument("-o", "--output", default="work/strings_ckb.json", help="Translated JSON path")
    p_tr.add_argument(
        "--rtl-mode",
        choices=["logical", "visual", "pua"],
        default="logical",
        help="logical=UTF-8 (TMP/RTLTMPro), visual=Arabic presentation forms + BiDi, pua=Unikurd PUA",
    )
    p_tr.add_argument("--cache", default="work/translation_cache.json", help="Translation cache path")

    p_pack = sub.add_parser("pack", help="Repack translated strings into a new APK")
    p_pack.add_argument("apk", help="Original .apk path")
    p_pack.add_argument("translated_json", help="Path to strings_ckb.json")
    p_pack.add_argument("-o", "--output", default="work/translated_ckb.apk", help="Output .apk path")
    p_pack.add_argument("--font", default=None, help="Optional Kurdish .ttf font to inject into Unity Font assets")

    p_all = sub.add_parser("pipeline", help="Run extract + translate + pack in one step")
    p_all.add_argument("source", help="Local .apk path or Dropbox share URL")
    p_all.add_argument("-w", "--workdir", default="work", help="Working directory")
    p_all.add_argument("-o", "--output-apk", default="work/translated_ckb.apk", help="Output .apk path")
    p_all.add_argument("--rtl-mode", choices=["logical", "visual", "pua"], default="logical")
    p_all.add_argument("--font", default=None, help="Optional Kurdish .ttf font path")

    p_ae = sub.add_parser("avg-extract", help="Stream-extract CAB, game.json, and unique_en.json from large android.ab")
    p_ae.add_argument("bundle", help="Path to android.ab UnityFS bundle")
    p_ae.add_argument("-o", "--output", default="work_avg", help="Output directory")

    p_at = sub.add_parser("avg-translate", help="Translate unique_en.json to Kurdish Sorani (ckb)")
    p_at.add_argument("unique_json", help="Path to unique_en.json")
    p_at.add_argument("-o", "--output", default="work_avg/unique_ckb.json", help="Translated unique JSON")
    p_at.add_argument("--cache", default="work_avg/avg_cache.json", help="Translation cache path")
    p_at.add_argument("--workers", type=int, default=8, help="Parallel translation threads")

    p_ap = sub.add_parser("avg-pack", help="Patch game.json and stream-repack android.ab")
    p_ap.add_argument("bundle", help="Original android.ab UnityFS bundle")
    p_ap.add_argument("translated_unique_json", help="Path to unique_ckb.json")
    p_ap.add_argument("-w", "--workdir", default="work_avg", help="Workdir containing CAB & game.json")
    p_ap.add_argument("-o", "--output", default="work_avg/android_ckb.ab", help="Repacked .ab output path")
    p_ap.add_argument("--rtl-mode", choices=["logical", "visual", "pua"], default="visual")
    p_ap.add_argument("--wrap-width", type=int, default=52, help="Line wrap width before visual BiDi")

    p_pf = sub.add_parser("patch-font", help="Replace m_FontData in a Unity bin/Data Font asset with a Kurdish TTF")
    p_pf.add_argument("font_asset", help="Path to Unity bin/Data font asset file")
    p_pf.add_argument("ttf", help="Path to Kurdish-capable .ttf font (e.g. DejaVuSans.ttf)")
    p_pf.add_argument("-o", "--output", required=True, help="Output patched font asset path")

    args = parser.parse_args()

    if args.command == "extract":
        out_dir = Path(args.output)
        apk = _resolve_apk(args.source, out_dir)
        manifest = extract_from_path(apk, out_dir)
        print(
            json.dumps(
                {
                    "engine": manifest["engine"],
                    "scripting_backend": manifest["scripting_backend"],
                    "unity_asset_files": len(manifest["unity_asset_files"]),
                    "raw_text_files": len(manifest["raw_text_files"]),
                    "total_strings": manifest["total_strings"],
                    "output": str(out_dir / "strings.json"),
                },
                ensure_ascii=False,
                indent=2,
            )
        )
    elif args.command == "translate":
        res = translate_manifest(
            input_json=Path(args.input_json),
            output_json=Path(args.output),
            rtl_mode=args.rtl_mode,
            cache_path=Path(args.cache) if args.cache else None,
        )
        print(f"Translated {res['total_strings']} entries -> {args.output}")
    elif args.command == "pack":
        summary = repack_apk(
            source_apk=Path(args.apk),
            translated_json=Path(args.translated_json),
            output_apk=Path(args.output),
            font_ttf_path=Path(args.font) if args.font else None,
        )
        print(json.dumps(summary, ensure_ascii=False, indent=2))
    elif args.command == "pipeline":
        workdir = Path(args.workdir)
        apk = _resolve_apk(args.source, workdir)
        extract_from_path(apk, workdir)
        strings_json = workdir / "strings.json"
        ckb_json = workdir / "strings_ckb.json"
        translate_manifest(
            input_json=strings_json,
            output_json=ckb_json,
            rtl_mode=args.rtl_mode,
            cache_path=workdir / "translation_cache.json",
        )
        summary = repack_apk(
            source_apk=apk,
            translated_json=ckb_json,
            output_apk=Path(args.output_apk),
            font_ttf_path=Path(args.font) if args.font else None,
        )
        print(json.dumps(summary, ensure_ascii=False, indent=2))
    elif args.command == "avg-extract":
        out_dir = Path(args.output)
        cab_path = out_dir / "primary.cab"
        game_json = out_dir / "game.json"
        unique_json = out_dir / "unique_en.json"
        meta = extract_primary_cab(Path(args.bundle), cab_path)
        extract_game_json_from_cab(cab_path, game_json)
        res = extract_unique_en_strings(game_json, unique_json)
        print(json.dumps({"cab": meta["nodes"][0][3], **res}, ensure_ascii=False, indent=2))
    elif args.command == "avg-translate":
        import threading
        from deep_translator import GoogleTranslator

        tls = threading.local()

        def _thread_translate(text: str) -> str:
            tr = getattr(tls, "tr", None)
            if tr is None:
                tr = GoogleTranslator(source="en", target="ckb")
                tls.tr = tr
            return tr.translate(text) or text

        res = translate_unique_items(
            Path(args.unique_json),
            Path(args.output),
            _thread_translate,
            cache_path=Path(args.cache) if args.cache else None,
            max_workers=args.workers,
        )
        print(json.dumps(res, ensure_ascii=False, indent=2))
    elif args.command == "avg-pack":
        workdir = Path(args.workdir)
        cab_path = workdir / "primary.cab"
        game_json = workdir / "game.json"
        if not cab_path.exists():
            extract_primary_cab(Path(args.bundle), cab_path)
        if not game_json.exists():
            extract_game_json_from_cab(cab_path, game_json)
        patched_json = workdir / "game_ckb.json"
        patched_cab = workdir / "primary_ckb.cab"
        p_stats = patch_game_json_with_translations(
            game_json,
            Path(args.translated_unique_json),
            patched_json,
            rtl_mode=args.rtl_mode,
            wrap_width=args.wrap_width,
        )
        patch_cab_game_json(cab_path, patched_json, patched_cab)
        b_stats = repack_bundle_with_cab(Path(args.bundle), patched_cab, Path(args.output))
        print(json.dumps({**p_stats, **b_stats}, ensure_ascii=False, indent=2))
    elif args.command == "patch-font":
        fname = patch_unity_font_file(Path(args.font_asset), Path(args.ttf), Path(args.output))
        print(f"Patched Unity Font '{fname}' -> {args.output}")


if __name__ == "__main__":
    main()
