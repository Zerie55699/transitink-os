# Third-party transport data

The files under `data/catalog/` and the byte-for-byte generated arrays in
`src/generated/TransitCatalogAssets.cpp` are factual transport-directory data,
not project source code. The project software licence does not replace or
relicense the terms that apply to these data sources.

## Current bundled catalog

- Catalog revision: `6ae8772c6350cce9`
- Extraction dates: Hong Kong sources 2026-07-17; TfL sources 2026-07-23
- Generated files: `index.json.gz`, `stops-kmb.json.gz`,
  `stops-ctb.json.gz`, `stops-gmb.json.gz`, `stops-tfl.json.gz`, and
  `rail.json.gz`
- Integrity metadata: `data/catalog/catalog-manifest.json`

## Sources and attribution

| Data in catalog | Source and rights owner | Use in TransitInk OS |
| --- | --- | --- |
| KMB and Long Win routes, route-stop relationships, stop IDs, and Traditional Chinese and English names | KMB/LWB official open-data API, published through DATA.GOV.HK; the relevant operator and/or HKSAR Government retains its rights | Reduced to the fields required for route search, direction selection, stop selection and ETA requests |
| Citybus routes, route-stop relationships, stop IDs, and Traditional Chinese and English names | Citybus official real-time API, published through DATA.GOV.HK; Citybus and/or HKSAR Government retains its rights | Route directions are fetched with bounded concurrency; stop names use an incremental cache before reduction |
| Green Minibus routes, stop sequence, stop ID, and Traditional Chinese and English names | HKSAR Transport Department, *Routes and Fares of Public Transport* dataset; route IDs and variants are cross-checked against the official GMB ETA API | Reduced to region, route code, route ID, route sequence, stop sequence, stop ID and bilingual labels |
| MTR and Light Rail line, station and direction identifiers and bilingual names | MTR Corporation official line-and-station CSV files and real-time API identifiers | A deterministic projection of the supported static catalog in `src/TransitCatalog.cpp` |
| London bus routes, directions, route-stop relationships, NaPTAN stop IDs and English names; London Underground, DLR, London Overground, Elizabeth line and London Trams lines and stations | Transport for London Unified API and TfL Open Data; Transport for London retains its rights | Reduced to the identifiers and original-language labels required for offline setup and live arrival requests |

## Live and device-cached sources

London bus and rail directory data are bundled as a firmware baseline so setup
does not require Internet access. Live arrivals and explicit refreshes for
newly added routes are obtained directly from the Transport for London Unified
API and may be saved as LittleFS overrides. The settings portal displays
`Powered by TfL Open Data` and `Data provided by Transport for London`; the
project does not use TfL branding or imply TfL endorsement.

Hong Kong weather is obtained directly from the Hong Kong Observatory Open
Data API. The device requests the official Traditional Chinese or English
response that matches the selected interface language. United Kingdom weather
is obtained from the Open-Meteo forecast API using its UK Met Office model and
is displayed with the required `Weather data by Open-Meteo.com` attribution.
Open-Meteo data are licensed under CC BY 4.0. The public free API is offered for
non-commercial use; anyone shipping TransitInk OS as part of a commercial
product must use a suitable paid Open-Meteo customer endpoint, self-host an
eligible data stack, or replace this source after reviewing the applicable
terms. The source was last reviewed on 2026-07-23.

If an official translation is not present in a source, the original
source-language label is retained instead of being machine translated.

Official references:

- [DATA.GOV.HK terms and conditions](https://data.gov.hk/tc/terms-and-conditions)
- [Transport Department routes and fares dataset](https://data.gov.hk/en-data/dataset/hk-td-tis_3-routes-and-fares-of-public-transport)
- [Transport Department route and fare data specification](https://static.data.gov.hk/td/routes-and-fares/dataspec/ptroutefare_dataspec.pdf)
- [Citybus route-stop API resource](https://data.gov.hk/tc-data/dataset/ctb-eta-transport-realtime-eta/resource/e51f302a-39a2-4034-bd7c-90ace2b6bc8b)
- [Green Minibus real-time arrival dataset](https://data.gov.hk/en-data/dataset/hk-td-sm_7-real-time-arrival-data-of-gmb)
- [MTR routes, stations and facilities dataset](https://data.gov.hk/en-data/dataset/mtr-data-routes-fares-barrier-free-facilities)
- [Hong Kong Observatory Open Data API documentation](https://data.weather.gov.hk/weatherAPI/doc/HKO_Open_Data_API_Documentation.pdf)
- [Open-Meteo UK Met Office API](https://open-meteo.com/en/docs/ukmo-api)
- [Open-Meteo licence](https://open-meteo.com/en/license)
- [Open-Meteo terms](https://open-meteo.com/en/terms)
- [TfL Open Data](https://tfl.gov.uk/info-for/open-data-users/)
- [TfL Transport Data Service terms](https://tfl.gov.uk/corporate/terms-and-conditions/transport-data-service)

When redistributing a catalog release, attribute the HKSAR Government, the
relevant transport operator or institution, and DATA.GOV.HK as applicable. Do
not imply that those parties endorse TransitInk OS. Review the linked terms and
source metadata again before each public release, because provider notices and
conditions may change.

## Refresh and review policy

Catalog refresh is manual. `scripts/generate_transit_route_catalog.py --refresh`
downloads current source data, validates UTF-8 and required IDs, removes exact
duplicates, checks strictly increasing stop sequences, verifies GMB route IDs,
and enforces gzip size limits. A route or stop count movement above 10% fails
closed unless a maintainer has reviewed it and passes `--allow-large-change`.
For a TfL-only maintainer refresh, use `--refresh-tfl`; anonymous requests are
rate-limited to stay within TfL's published access limit.

After generating a new baseline, review the manifest counts, attribution,
generated date and source changes before committing and publishing new
firmware. End-user route refreshes contact the same official sources from the
device and store only the selected route as a local LittleFS override; they do
not create or redistribute a separate catalog release.
