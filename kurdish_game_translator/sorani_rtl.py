"""Kurdish Sorani (ckb) RTL reshaping, BiDi ordering, and game-tag protection."""

from __future__ import annotations

import re
from typing import Dict, List, Tuple

import arabic_reshaper
from arabic_reshaper.letters import LETTERS_ARABIC_V2, LETTERS_KURDISH
from bidi.algorithm import get_display

# Matches common Unity / VN / RPG placeholders & markup tags so translators
# do not corrupt variable names, rich-text tags, or escaped newlines.
TAG_PATTERN = re.compile(
    r"(\$\{[^}]+\}|<[^>]+>|\{[0-9a-zA-Z_.:-]+\}|%[0-9]*\$?[sdif]|\\n|\\r|\\t|\[[^\]]+\])"
)


def _build_safe_kurdish_letters() -> Dict[str, Tuple[str, str, str, str]]:
    """Build a letter table based on ArabicV2 with Kurdish Sorani letters.

    Unlike arabic_reshaper's built-in 'Kurdish' preset—which emits Private Use
    Area (U+E000..U+E009) glyphs only supported by legacy Unikurd fonts—this
    keeps 'ڵ', 'ڕ', 'ێ', 'ە' as standard Unicode codepoints while preserving
    accurate before/after connection rules so neighboring Arabic letters join.
    """
    letters = dict(LETTERS_ARABIC_V2)
    # Standard Unicode presentation forms supported by Arabic/Kurdish fonts:
    letters["\u067e"] = ("\ufb56", "\ufb58", "\ufb59", "\ufb57")  # پ
    letters["\u0686"] = ("\ufb7a", "\ufb7c", "\ufb7d", "\ufb7b")  # چ
    letters["\u0698"] = ("\ufb8a", "", "", "\ufb8b")              # ژ
    letters["\u06a4"] = ("\ufb6a", "\ufb6c", "\ufb6d", "\ufb6b")  # ڤ
    letters["\u06a9"] = ("\ufb8e", "\ufb90", "\ufb91", "\ufb8f")  # ک
    letters["\u06af"] = ("\ufb92", "\ufb94", "\ufb95", "\ufb93")  # گ
    letters["\u06be"] = ("\ufbaa", "\ufbac", "\ufbad", "\ufbab")  # ھ
    letters["\u06c6"] = ("\ufbd9", "", "", "\ufbda")              # ۆ
    letters["\u06cc"] = ("\ufbfc", "\ufbfe", "\ufbff", "\ufbfd")  # ی

    # Letters without standard Unicode Presentation Forms-B codepoints:
    # Keep standard Unicode char so modern fonts don't draw tofu boxes,
    # while defining connection behavior (dual-joining vs right-joining).
    letters["\u06b5"] = ("\u06b5", "\u06b5", "\u06b5", "\u06b5")  # ڵ (dual-joining)
    letters["\u06ce"] = ("\u06ce", "\u06ce", "\u06ce", "\u06ce")  # ێ (dual-joining)
    letters["\u0695"] = ("\u0695", "", "", "\u0695")              # ڕ (right-joining only)
    letters["\u06d5"] = ("\u06d5", "", "", "\u06d5")              # ە (right-joining only)
    return letters


_SAFE_RESHAPER = arabic_reshaper.ArabicReshaper(configuration={"language": "ArabicV2"})
_SAFE_RESHAPER.letters = _build_safe_kurdish_letters()

_PUA_RESHAPER = arabic_reshaper.ArabicReshaper(configuration={"language": "Kurdish"})


def protect_tags(text: str) -> Tuple[str, List[str]]:
    """Replace game formatting tags/placeholders with indexed markers."""
    tokens: List[str] = []

    def _repl(match: re.Match[str]) -> str:
        idx = len(tokens)
        tokens.append(match.group(0))
        return f"@@TAG{idx}@@"

    return TAG_PATTERN.sub(_repl, text), tokens


def restore_tags(text: str, tokens: List[str]) -> str:
    """Restore previously protected tags/placeholders into translated text."""
    restored = text
    for idx, token in enumerate(tokens):
        # Handle translators that occasionally add spaces inside markers
        pattern = re.compile(rf"@@\s*TAG\s*{idx}\s*@@", re.IGNORECASE)
        restored = pattern.sub(lambda _m, t=token: t, restored)
    return restored


def format_kurdish_rtl(text: str, mode: str = "logical") -> str:
    """Format Kurdish Sorani text for a target game renderer.

    Args:
        text: Translated Kurdish Sorani Unicode string.
        mode:
            - 'logical': Keep standard UTF-8 logical order (for TextMeshPro +
              RTLTMPro, Android native UI, or engines with HarfBuzz RTL).
            - 'visual': Reshape into standard Unicode Arabic presentation forms
              (no PUA tofu) and apply Unicode BiDi visual ordering (for legacy
              Unity UI.Text without RTL support).
            - 'pua': Use legacy Unikurd/arabic-reshaper PUA table + BiDi.
    """
    if not text or mode == "logical":
        return text

    if mode == "visual":
        reshaper = _SAFE_RESHAPER
    elif mode == "pua":
        reshaper = _PUA_RESHAPER
    else:
        raise ValueError(f"Unsupported RTL mode: {mode!r}")

    lines = text.splitlines(keepends=False)
    out_lines: List[str] = []
    for line in lines:
        # Preserve Unity rich text tags intact around reshaped segments if present
        reshaped = reshaper.reshape(line)
        out_lines.append(get_display(reshaped))
    return "\n".join(out_lines)
