# GNU Unifont source

- Font: GNU Unifont 17.0.04, compiled BDF
- Upstream file: `unifont-17.0.04.bdf.gz`
- Download: <https://ftp.gnu.org/pub/gnu/unifont/unifont-17.0.04/unifont-17.0.04.bdf.gz>
- SHA-256: `9a2de4826388242771121c7fe00e412523c318318b8ee38e6be6cd454e7ec802`
- Retrieved: 2026-07-28
- Copyright: 1998–2026 Roman Czyborra, Paul Hardy, Qianqian Fang,
  Andrew Miller, Johnnie Weaver, David Corbett, Ælla Chiana Moskopp,
  Rebecca Bettencourt, Minseo Lee, Ho-Seok Ee, and other contributors
- Licence used by TransitInk: SIL Open Font License 1.1

The upstream BDF identifies the compiled font as dual-licensed under
OFL-1.1 and GPL-2.0-or-later with the GNU Font Embedding Exception.
TransitInk redistributes it and the generated subset under OFL-1.1. The
complete OFL text is retained in `OFL-1.1.txt`.

`scripts/generate_hk_glyph_font.py` reads the original 8×16 and 16×16 bitmap
rows directly. It produces the deterministic subset in
`src/generated/UnifontGlyphFontData.cpp`; it does not rasterize an outline
font or introduce antialiasing.
