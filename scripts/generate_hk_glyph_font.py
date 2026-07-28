#!/usr/bin/env python3
import argparse
import gzip
import hashlib
import json
import math
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GLYPH_SIZE = 16
GLYPH_BASELINE = 14
DEFAULT_THRESHOLD = 128
DEFAULT_SEED = ROOT / "scripts" / "hk_glyph_seed.txt"
DEFAULT_FONT = (
    ROOT
    / "third_party"
    / "fonts"
    / "noto-sans-cjk-hk"
    / "NotoSansCJKhk-Regular.otf"
)
DEFAULT_FONT_SHA256 = (
    "97c937514d645eae90415d30ba025e08a94d5bdffdc627404864f90aa0c7d83b"
)
DEFAULT_UNIFONT = (
    ROOT
    / "third_party"
    / "fonts"
    / "unifont"
    / "unifont-17.0.04.bdf.gz"
)
DEFAULT_UNIFONT_SHA256 = (
    "9a2de4826388242771121c7fe00e412523c318318b8ee38e6be6cd454e7ec802"
)
GENERATED_SOURCE = ROOT / "src" / "generated" / "HkGlyphFontData.cpp"
GENERATED_UNIFONT_SOURCE = (
    ROOT / "src" / "generated" / "UnifontGlyphFontData.cpp"
)


def should_include(ch):
    codepoint = ord(ch)
    if codepoint > 0xFFFF:
        raise ValueError(f"Unsupported glyph code point U+{codepoint:04X}")
    if ch in "\r\n\t":
        return False
    if 0x20 <= codepoint <= 0x7E:
        return True
    return codepoint > 0x7F


def read_seed_chars(path):
    text = path.read_text(encoding="utf-8")
    marker = "\n---\n"
    if marker not in text:
        raise ValueError(f"Glyph seed is missing payload marker: {path}")
    payload = text.split(marker, 1)[1]
    return {ch for ch in payload if should_include(ch)}


def collect_chars(paths, seed_path):
    chars = read_seed_chars(seed_path)
    for path in paths:
        if not path.exists():
            continue
        payload = json.loads(path.read_text(encoding="utf-8"))
        for row in payload.get("data", []):
            for key, value in row.items():
                if not isinstance(value, str):
                    continue
                if key.endswith("_tc") or key in {"name_tc", "orig_tc", "dest_tc"}:
                    chars.update(ch for ch in value if should_include(ch))

    for path in list((ROOT / "src").rglob("*")) + list((ROOT / "include").rglob("*")):
        if path.suffix not in {".cpp", ".h"} or path in {
            GENERATED_SOURCE,
            GENERATED_UNIFONT_SOURCE,
        }:
            continue
        chars.update(ch for ch in path.read_text(encoding="utf-8") if should_include(ch))

    chars.update(chr(codepoint) for codepoint in range(0x20, 0x7F))
    return sorted(chars, key=ord)


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_font(path, allow_unverified):
    if not path.is_file():
        raise FileNotFoundError(f"Glyph font not found: {path}")

    digest = file_sha256(path)
    if path.resolve() == DEFAULT_FONT.resolve():
        if digest != DEFAULT_FONT_SHA256:
            raise ValueError(
                "Vendored glyph font SHA-256 mismatch: "
                f"expected {DEFAULT_FONT_SHA256}, got {digest}"
            )
    elif not allow_unverified:
        raise ValueError(
            "Custom fonts require --allow-unverified-font; release glyphs must use "
            "the pinned vendored font"
        )
    return digest


def load_font(path):
    try:
        from PIL import ImageFont
    except ImportError as error:
        raise RuntimeError(
            "Pillow is required; run scripts/install_tools.sh and invoke this "
            "generator with .venv/bin/python"
        ) from error

    return ImageFont.truetype(
        str(path),
        GLYPH_SIZE,
        layout_engine=ImageFont.Layout.BASIC,
    )


def render_char(ch, font, threshold):
    from PIL import Image, ImageDraw

    image = Image.new("L", (GLYPH_SIZE, GLYPH_SIZE), color=255)
    draw = ImageDraw.Draw(image)
    draw.text(
        (0, GLYPH_BASELINE),
        ch,
        font=font,
        fill=0,
        anchor="ls",
    )

    width = min(max(1, math.ceil(font.getlength(ch))), GLYPH_SIZE)
    rows = []
    for y in range(GLYPH_SIZE):
        row = 0
        for x in range(width):
            if image.getpixel((x, y)) < threshold:
                row |= 1 << (15 - x)
        rows.append(row)
    return ord(ch), width, rows


def render_chars(chars, font_path, threshold):
    font = load_font(font_path)
    return [render_char(ch, font, threshold) for ch in chars]


def load_bdf_glyphs(path):
    glyphs = {}
    current = None
    bitmap = None
    with gzip.open(path, "rt", encoding="utf-8") as source:
        for raw_line in source:
            line = raw_line.strip()
            if line.startswith("STARTCHAR "):
                current = {"bitmap": []}
                bitmap = None
            elif current is None:
                continue
            elif line.startswith("ENCODING "):
                current["codepoint"] = int(line.split()[1])
            elif line.startswith("DWIDTH "):
                current["dwidth"] = int(line.split()[1])
            elif line.startswith("BBX "):
                current["bbx"] = tuple(int(value) for value in line.split()[1:5])
            elif line == "BITMAP":
                bitmap = current["bitmap"]
            elif line == "ENDCHAR":
                required = {"codepoint", "dwidth", "bbx", "bitmap"}
                if not required.issubset(current):
                    raise ValueError("Incomplete BDF glyph")
                glyphs[current["codepoint"]] = current
                current = None
                bitmap = None
            elif bitmap is not None:
                bitmap.append(line)
    return glyphs


def bdf_glyph_rows(glyph):
    width, height, x_offset, y_offset = glyph["bbx"]
    advance = glyph["dwidth"]
    if not 1 <= advance <= GLYPH_SIZE:
        raise ValueError(f"Unsupported BDF advance width: {advance}")
    if width < 0 or height < 0 or width > GLYPH_SIZE or height > GLYPH_SIZE:
        raise ValueError(f"Unsupported BDF bounding box: {glyph['bbx']}")
    if len(glyph["bitmap"]) != height:
        raise ValueError("BDF bitmap row count does not match BBX height")

    top = GLYPH_BASELINE - (y_offset + height)
    rows = [0] * GLYPH_SIZE
    for source_y, encoded_row in enumerate(glyph["bitmap"]):
        target_y = top + source_y
        if target_y < 0 or target_y >= GLYPH_SIZE:
            continue
        storage_width = len(encoded_row) * 4
        bits = int(encoded_row, 16) if encoded_row else 0
        for source_x in range(width):
            target_x = x_offset + source_x
            if (
                0 <= target_x < advance
                and bits & (1 << (storage_width - 1 - source_x))
            ):
                rows[target_y] |= 1 << (15 - target_x)
    return advance, rows


def render_bdf_chars(chars, font_path):
    source_glyphs = load_bdf_glyphs(font_path)
    rendered = []
    for ch in chars:
        codepoint = ord(ch)
        if codepoint not in source_glyphs:
            raise ValueError(f"Unifont is missing U+{codepoint:04X}")
        width, rows = bdf_glyph_rows(source_glyphs[codepoint])
        rendered.append((codepoint, width, rows))
    return rendered


def format_source(glyphs, font_digest, threshold):
    lines = [
        "// SPDX-License-Identifier: OFL-1.1",
        "// Generated by scripts/generate_hk_glyph_font.py; do not edit.",
        "// Source font: Noto Sans CJK HK Regular 2.004",
        f"// Source SHA-256: {font_digest}",
        f"// Raster: {GLYPH_SIZE}px, baseline {GLYPH_BASELINE}px, threshold {threshold}",
        "// Licence: third_party/fonts/noto-sans-cjk-hk/OFL.txt",
        "// Upstream notice: third_party/fonts/noto-sans-cjk-hk/UPSTREAM-NOTICE.md",
        "",
        '#include "HkGlyphFont.h"',
        "",
        "const HkGlyph kHkGlyphs[] = {",
    ]
    for codepoint, width, rows in glyphs:
        row_values = ", ".join(f"0x{row:04X}" for row in rows)
        lines.append(f"    {{0x{codepoint:04X}, {width}, {{{row_values}}}}},")
    lines.extend(
        [
            "};",
            "",
            "const size_t kHkGlyphCount = sizeof(kHkGlyphs) / sizeof(kHkGlyphs[0]);",
            "",
        ]
    )
    return "\n".join(lines)


def format_unifont_source(glyphs, font_digest):
    lines = [
        "// SPDX-License-Identifier: OFL-1.1",
        "// Generated by scripts/generate_hk_glyph_font.py; do not edit.",
        "// Source font: GNU Unifont 17.0.04 BDF",
        f"// Source SHA-256: {font_digest}",
        "// Raster: original 8x16 or 16x16 bitmap glyphs; no antialiasing",
        "// Licence: third_party/fonts/unifont/OFL-1.1.txt",
        "// Source notice: third_party/fonts/unifont/SOURCE.md",
        "",
        '#include "HkGlyphFont.h"',
        "",
        "const HkGlyph kUnifontGlyphs[] = {",
    ]
    for codepoint, width, rows in glyphs:
        row_values = ", ".join(f"0x{row:04X}" for row in rows)
        lines.append(f"    {{0x{codepoint:04X}, {width}, {{{row_values}}}}},")
    lines.extend(
        [
            "};",
            "",
            "const size_t kUnifontGlyphCount = "
            "sizeof(kUnifontGlyphs) / sizeof(kUnifontGlyphs[0]);",
            "",
        ]
    )
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seed", type=Path, default=DEFAULT_SEED)
    parser.add_argument("--stop-json", type=Path, default=None)
    parser.add_argument("--route-json", type=Path, default=None)
    parser.add_argument("--font", type=Path, default=DEFAULT_FONT)
    parser.add_argument("--unifont", type=Path, default=DEFAULT_UNIFONT)
    parser.add_argument("--threshold", type=int, default=DEFAULT_THRESHOLD)
    parser.add_argument("--allow-unverified-font", action="store_true")
    parser.add_argument(
        "--check",
        action="store_true",
        help="verify that the checked-in generated table is current",
    )
    args = parser.parse_args()

    if not 0 <= args.threshold <= 255:
        parser.error("--threshold must be between 0 and 255")

    font_digest = validate_font(args.font, args.allow_unverified_font)
    unifont_digest = file_sha256(args.unifont)
    if unifont_digest != DEFAULT_UNIFONT_SHA256:
        raise ValueError(
            "Vendored Unifont SHA-256 mismatch: "
            f"expected {DEFAULT_UNIFONT_SHA256}, got {unifont_digest}"
        )
    external_paths = [
        path for path in (args.stop_json, args.route_json) if path is not None
    ]
    chars = collect_chars(external_paths, args.seed)
    glyphs = render_chars(chars, args.font, args.threshold)
    unifont_glyphs = render_bdf_chars(chars, args.unifont)
    generated = format_source(glyphs, font_digest, args.threshold)
    generated_unifont = format_unifont_source(unifont_glyphs, unifont_digest)

    if args.check:
        stale = []
        if (
            not GENERATED_SOURCE.exists()
            or GENERATED_SOURCE.read_text(encoding="utf-8") != generated
        ):
            stale.append(GENERATED_SOURCE)
        if (
            not GENERATED_UNIFONT_SOURCE.exists()
            or GENERATED_UNIFONT_SOURCE.read_text(encoding="utf-8")
            != generated_unifont
        ):
            stale.append(GENERATED_UNIFONT_SOURCE)
        if stale:
            print(
                "Generated glyph table is out of date: "
                + ", ".join(str(path) for path in stale)
            )
            return 1
        print(
            f"Verified {len(glyphs)} glyphs in pinned Noto Sans CJK HK "
            "and GNU Unifont"
        )
        return 0

    GENERATED_SOURCE.parent.mkdir(parents=True, exist_ok=True)
    GENERATED_SOURCE.write_text(generated, encoding="utf-8")
    GENERATED_UNIFONT_SOURCE.write_text(generated_unifont, encoding="utf-8")
    print(
        f"Generated {len(glyphs)} glyphs from pinned Noto Sans CJK HK "
        "and GNU Unifont"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
