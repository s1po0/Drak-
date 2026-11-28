#!/usr/bin/env python3
"""
Fast, tag-safe Google Translate (en -> ckb, Sorani Kurdish) batch translator
for all extracted Ren'Py strings in data/strings_en.json.gz.

Outputs: data/translations_ckb.json.gz
"""
from __future__ import annotations

import concurrent.futures
import gzip
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request
from typing import Dict, List, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extract_strings import tokenize_renpy


USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/127.0.0.0 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36",
]


def google_translate_raw(text: str, src: str = "en", tgt: str = "ckb", timeout: int = 25, retries: int = 5) -> str:
    if not text or not text.strip():
        return text
    body = urllib.parse.urlencode(
        {"client": "gtx", "sl": src, "tl": tgt, "dt": "t", "q": text}
    ).encode("utf-8")
    url = "https://translate.googleapis.com/translate_a/single"
    last_err = None
    for attempt in range(retries):
        try:
            req = urllib.request.Request(
                url,
                data=body,
                headers={
                    "User-Agent": USER_AGENTS[attempt % len(USER_AGENTS)],
                    "Content-Type": "application/x-www-form-urlencoded;charset=UTF-8",
                },
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                payload = json.loads(resp.read().decode("utf-8"))
            return "".join(seg[0] for seg in payload[0] if seg and seg[0] is not None)
        except Exception as e:
            last_err = e
            time.sleep(0.6 * (attempt + 1))
    raise RuntimeError(f"Google Translate failed after {retries} attempts: {last_err}")


SELF_CLOSING_TAGS = {
    "w", "p", "nw", "fast", "done", "clear", "cps", "space", "vspace",
    "image", "emoji", "ko-kp", "chr", "ctx", "dex", "int", "str", "usd",
    "dom", "sub", "l", "r",
}


def parse_tag_name(tag_token: str) -> tuple[bool, str] | None:
    """Return (is_closing, tag_name) for '{...}', or None if not a tag."""
    if not (tag_token.startswith("{") and tag_token.endswith("}")) or tag_token.startswith("{{"):
        return None
    inner = tag_token[1:-1].strip()
    if not inner or inner.startswith("#"):
        return None
    is_close = inner.startswith("/")
    if is_close:
        inner = inner[1:].strip()
    name = re.split(r"[=\s]", inner, maxsplit=1)[0].lower()
    return is_close, name


def check_tags_valid(s: str, orig_masks: List[str]) -> bool:
    """
    Verify:
    1. Every substitution [var] / printf %s from orig_masks is preserved in s.
    2. All Ren'Py tags {...} and brackets [...] are balanced and properly nested.
    """
    tokens = tokenize_renpy(s)
    new_masks = [val for kind, val in tokens if kind == "MASK"]

    # Check all non-tag masks ([...], [[, {{, %s) match multiset
    orig_vars = sorted(m for m in orig_masks if not (m.startswith("{") and not m.startswith("{{")))
    new_vars = sorted(m for m in new_masks if not (m.startswith("{") and not m.startswith("{{")))
    if orig_vars != new_vars:
        return False

    # Check tag nesting stack
    stack: List[str] = []
    for kind, val in tokens:
        if kind == "TEXT":
            # Ensure no stray unescaped single '[' or ']' or '{' or '}' leaked
            if any(c in val for c in "{}[]"):
                return False
        else:
            parsed = parse_tag_name(val)
            if parsed is None:
                continue
            is_close, name = parsed
            if name == "":
                # {/} closes custom style tag {=...}
                if not stack:
                    return False
                stack.pop()
                continue
            if not is_close:
                if name in SELF_CLOSING_TAGS:
                    continue
                stack.append(name)
            else:
                if name in SELF_CLOSING_TAGS:
                    continue
                if not stack or stack[-1] != name:
                    return False
                stack.pop()

    # Original might intentionally have unclosed tags? Check if orig stack was empty
    orig_stack: List[str] = []
    for m in orig_masks:
        parsed = parse_tag_name(m)
        if parsed:
            ic, nm = parsed
            if nm in SELF_CLOSING_TAGS:
                continue
            if not ic:
                orig_stack.append(nm)
            elif orig_stack and orig_stack[-1] == nm:
                orig_stack.pop()
    if not orig_stack and stack:
        return False
    return True


def has_complex_tags(tokens: List[Tuple[str, str]]) -> bool:
    tag_count = 0
    for kind, val in tokens:
        if kind == "MASK" and val.startswith("{") and not val.startswith("{{"):
            tag_count += 1
            if any(val.startswith(p) for p in ("{rb", "{rt", "{art", "{pen=")):
                return True
    return tag_count >= 4


def mask_single_string(s: str) -> Tuple[str, str, List[str], str]:
    """
    Prepare a string for translation.
    Returns (prefix, core_masked, masks_list, suffix).
    Strips outer symmetrical tags/whitespace/parens cleanly so Google focuses on the sentence.
    """
    tokens = tokenize_renpy(s)
    masks: List[str] = []
    parts: List[str] = []
    for kind, val in tokens:
        if kind == "TEXT":
            parts.append(val)
        else:
            idx = len(masks)
            masks.append(val)
            parts.append(f"[#{idx}#]")
    return "", "".join(parts), masks, ""


PLACEHOLDER_RE = re.compile(r"[\[\]]\s*#\s*(\d+)\s*#\s*[\[\]]")


def unmask_string(translated: str, masks: List[str]) -> str:
    def repl(m: re.Match) -> str:
        idx = int(m.group(1))
        if 0 <= idx < len(masks):
            return masks[idx]
        return m.group(0)

    out = PLACEHOLDER_RE.sub(repl, translated)
    return out


def translate_batch_lines(lines: List[str]) -> List[str]:
    """
    Translate a batch of single-line strings (none containing '\\n') in one request.
    Falls back recursively to smaller batches if line count mismatches.
    """
    if not lines:
        return []
    if len(lines) == 1:
        return [google_translate_raw(lines[0]).replace("\n", " ").strip()]

    joined = "\n".join(lines)
    res = google_translate_raw(joined)
    out_lines = res.split("\n")
    if len(out_lines) == len(lines):
        return [x.strip() for x in out_lines]

    # Recursive binary split fallback guarantees 100% alignment
    mid = len(lines) // 2
    return translate_batch_lines(lines[:mid]) + translate_batch_lines(lines[mid:])


def translate_all(
    strings: List[str],
    cache_dict: Dict[str, str] | None = None,
    workers: int = 16,
    batch_char_limit: int = 2800,
    batch_max_items: int = 30,
) -> Dict[str, str]:
    results: Dict[str, str] = dict(cache_dict or {})
    todo = [s for s in strings if s not in results]
    print(f"Total strings: {len(strings)}, cached: {len(results)}, remaining: {len(todo)}", flush=True)
    if not todo:
        return results

    # Separate into:
    # A) Simple single-line strings (can be batched directly with mask/unmask)
    # B) Multi-line or complex-tag strings (translate pure TEXT segments via deduplicated segment pool!)
    simple_items: List[Tuple[str, str, List[str]]] = []  # (orig, masked, masks)
    segment_strings: List[Tuple[str, List[Tuple[str, str]]]] = []  # (orig, tokens)

    for s in todo:
        tokens = tokenize_renpy(s)
        if "\n" in s or has_complex_tags(tokens):
            segment_strings.append((s, tokens))
        else:
            _, masked, masks, _ = mask_single_string(s)
            simple_items.append((s, masked, masks))

    print(f"Simple single-line items: {len(simple_items)}, complex/multiline items: {len(segment_strings)}", flush=True)

    # Build batches for simple_items
    batches: List[List[Tuple[str, str, List[str]]]] = []
    cur: List[Tuple[str, str, List[str]]] = []
    cur_len = 0
    for item in simple_items:
        mlen = len(item[1])
        if cur and (cur_len + mlen + 1 > batch_char_limit or len(cur) >= batch_max_items):
            batches.append(cur)
            cur = []
            cur_len = 0
        cur.append(item)
        cur_len += mlen + 1
    if cur:
        batches.append(cur)

    print(f"Translating {len(batches)} batches with {workers} workers...", flush=True)
    t0 = time.time()
    fallback_to_seg: List[Tuple[str, List[Tuple[str, str]]]] = []

    def run_batch(b: List[Tuple[str, str, List[str]]]):
        masked_lines = [it[1] for it in b]
        trans_lines = translate_batch_lines(masked_lines)
        return list(zip(b, trans_lines))

    done_batches = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        futures = [ex.submit(run_batch, b) for b in batches]
        for fut in concurrent.futures.as_completed(futures):
            pairs = fut.result()
            for (orig, _masked, masks), trans_raw in pairs:
                unmasked = unmask_string(trans_raw, masks)
                # Preserve leading/trailing space if original had it inside tags
                if orig.startswith(" ") and not unmasked.startswith(" "):
                    unmasked = " " + unmasked
                if orig.endswith(" ") and not unmasked.endswith(" "):
                    unmasked = unmasked + " "
                if check_tags_valid(unmasked, masks) and not PLACEHOLDER_RE.search(unmasked):
                    results[orig] = unmasked
                else:
                    # Fallback to segment-by-segment translation so tags NEVER break!
                    fallback_to_seg.append((orig, tokenize_renpy(orig)))
            done_batches += 1
            if done_batches % 50 == 0 or done_batches == len(batches):
                el = time.time() - t0
                print(f"  Batch progress: {done_batches}/{len(batches)} ({el:.1f}s)", flush=True)

    all_seg_items = segment_strings + fallback_to_seg
    print(f"Translating {len(all_seg_items)} complex/fallback items via segment pool...", flush=True)

    # Collect unique translatable text lines across all_seg_items
    unique_sublines: set[str] = set()
    for _orig, tokens in all_seg_items:
        for kind, val in tokens:
            if kind == "TEXT":
                for sub in val.split("\n"):
                    if any("a" <= c.lower() <= "z" for c in sub):
                        unique_sublines.add(sub.strip())

    subline_list = sorted(unique_sublines)
    sub_map: Dict[str, str] = {}
    sub_batches: List[List[str]] = []
    scur: List[str] = []
    slen = 0
    for sl in subline_list:
        if scur and (slen + len(sl) + 1 > batch_char_limit or len(scur) >= batch_max_items):
            sub_batches.append(scur)
            scur = []
            slen = 0
        scur.append(sl)
        slen += len(sl) + 1
    if scur:
        sub_batches.append(scur)

    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as ex:
        futs = {ex.submit(translate_batch_lines, sb): sb for sb in sub_batches}
        for fut in concurrent.futures.as_completed(futs):
            sb = futs[fut]
            tl = fut.result()
            for orig_sub, tr_sub in zip(sb, tl):
                sub_map[orig_sub] = tr_sub

    for orig, tokens in all_seg_items:
        rebuilt: List[str] = []
        masks = [v for k, v in tokens if k == "MASK"]
        for kind, val in tokens:
            if kind == "MASK":
                rebuilt.append(val)
            else:
                lines_out: List[str] = []
                for sub in val.split("\n"):
                    stripped = sub.strip()
                    if stripped in sub_map:
                        lead = sub[: len(sub) - len(sub.lstrip())]
                        trail = sub[len(sub.rstrip()) :]
                        # Clean any stray brackets/braces Google might emit
                        clean_tr = re.sub(r"[\[\]\{\}]", "", sub_map[stripped])
                        lines_out.append(f"{lead}{clean_tr}{trail}")
                    else:
                        lines_out.append(sub)
                rebuilt.append("\n".join(lines_out))
        candidate = "".join(rebuilt)
        if check_tags_valid(candidate, masks):
            results[orig] = candidate
        else:
            results[orig] = orig

    print(f"Completed {len(results)}/{len(strings)} translations in {time.time() - t0:.1f}s!", flush=True)
    return results


def main():
    in_path = "data/strings_en.json.gz"
    out_path = "data/translations_ckb.json.gz"
    with gzip.open(in_path, "rt", encoding="utf-8") as gz:
        data = json.load(gz)
    strings: List[str] = data["strings"]

    cache: Dict[str, str] = {}
    if os.path.exists(out_path):
        with gzip.open(out_path, "rt", encoding="utf-8") as gz:
            cache = json.load(gz)

    results = translate_all(strings, cache_dict=cache)
    with gzip.open(out_path, "wt", encoding="utf-8") as gz:
        json.dump(results, gz, ensure_ascii=False)
    print(f"Wrote {out_path} ({os.path.getsize(out_path) / 1024:.1f} KB)")


if __name__ == "__main__":
    main()
