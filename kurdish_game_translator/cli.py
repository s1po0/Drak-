"""CLI for extracting, translating to Kurdish Sorani, and repacking Unity APKs."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from .apk_extractor import download_apk, extract_from_path
from .apk_repacker import repack_apk
from .translator import translate_manifest


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


if __name__ == "__main__":
    main()
