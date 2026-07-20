# Vendored ESP Web Tools

This directory contains the browser distribution of `esp-web-tools` 10.2.1.
It is served from the same origin as the TransitInk installer so opening the
installer never executes JavaScript fetched from a CDN.

- Upstream: https://github.com/esphome/esp-web-tools
- npm package: `esp-web-tools@10.2.1`
- Package SHA-256: `03e24937ec582709942d61a9c223d27423a2faf7781ad6fd475138dfcbcbd6e7`
- License: Apache-2.0; see `LICENSE`

`vendor/install-button.js` has one local patch: its no-port dialog import points
to `../no-port-dialog-zh.js` for the TransitInk Traditional Chinese copy.
`no-port-dialog-zh.js` is a Traditional Chinese adaptation with same-origin
imports. Both modified files carry a prominent modification notice. All other
files under `vendor/` are copied verbatim from `dist/web/` in the npm package.

Components compiled into the upstream browser bundle and their exact versions
are listed in `THIRD_PARTY_NOTICES.md`. Retained licence texts are published
with the installer under `legal/licenses/`.
