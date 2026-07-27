# Components bundled in ESP Web Tools 10.2.1

TransitInk vendors the upstream browser distribution of ESP Web Tools 10.2.1.
The upstream build contains the following runtime components. Versions are
resolved from the upstream 10.2.1 build inputs; components in the same family
share the listed licence.

| Component | Version | Licence | Retained text |
| --- | --- | --- | --- |
| ESP Web Tools | 10.2.1 | Apache-2.0 | `LICENSE` |
| Material Web | 2.4.1 | Apache-2.0 | `../../third_party/licenses/Material-Web-Apache-2.0.txt` |
| Material Web Components (`@material/mwc-*`) | 0.27.x | Apache-2.0 | `../../third_party/licenses/Material-Web-Apache-2.0.txt` |
| esptool-js | 0.5.7 | Apache-2.0 | `../../third_party/licenses/esptool-js-Apache-2.0.txt` |
| Improv Wi-Fi Serial SDK | 2.5.0 | Apache-2.0 | `../../third_party/licenses/Improv-WiFi-Serial-SDK-Apache-2.0.txt` |
| Lit, lit-element, lit-html, reactive-element | 3.3.2 family and the 2.8.0 compatibility tree | BSD-3-Clause | `../../third_party/licenses/Lit-BSD-3-Clause.txt` |
| pako | 2.1.0 | MIT | `../../third_party/licenses/Pako-MIT.txt` |
| tslib | 2.8.1 | 0BSD | `../../third_party/licenses/tslib-0BSD.txt` |
| atob-lite | 2.0.0 | MIT | `../../third_party/licenses/atob-lite-MIT.txt` |

All retained texts are copied into `legal/licenses/` by the release packager.
Upstream repositories and package metadata remain available through
<https://github.com/esphome/esp-web-tools> and the npm registry.

TransitInk loads `full-erase-policy.js` alongside the vendored installer. This
local policy forces the upstream install dialog to use its full-erase path
because TransitInk's browser installer writes a merged factory image at offset
zero. The upstream licence and retained notices remain unchanged.
