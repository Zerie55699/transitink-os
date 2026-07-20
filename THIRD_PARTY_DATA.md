# Third-party transport data

The files under `data/catalog/` and the byte-for-byte generated arrays in
`src/generated/TransitCatalogAssets.cpp` are factual transport-directory data,
not project source code. The project software licence does not replace or
relicense the terms that apply to these data sources.

## Current bundled catalog

- Catalog revision: `e8ac05108a9a2f38`
- Extraction date: 2026-07-17 (Hong Kong time)
- Generated files: `index.json.gz`, `stops-kmb.json.gz`,
  `stops-ctb.json.gz`, `stops-gmb.json.gz`, and `rail.json.gz`
- Integrity metadata: `data/catalog/catalog-manifest.json`

## Sources and attribution

| Data in catalog | Source and rights owner | Use in TransitInk OS |
| --- | --- | --- |
| KMB and Long Win routes, route-stop relationships, stop IDs and Traditional Chinese names | KMB/LWB official open-data API, published through DATA.GOV.HK; the relevant operator and/or HKSAR Government retains its rights | Reduced to the fields required for route search, direction selection, stop selection and ETA requests |
| Citybus routes, route-stop relationships, stop IDs and Traditional Chinese names | Citybus official real-time API, published through DATA.GOV.HK; Citybus and/or HKSAR Government retains its rights | Route directions are fetched with bounded concurrency; stop names use an incremental cache before reduction |
| Green Minibus routes, stop sequence, stop ID and Traditional Chinese names | HKSAR Transport Department, *Routes and Fares of Public Transport* dataset; route IDs and variants are cross-checked against the official GMB ETA API | Reduced to region, route code, route ID, route sequence, stop sequence, stop ID and Traditional Chinese labels |
| MTR and Light Rail line, station and direction identifiers | MTR Corporation official data dictionaries and real-time API identifiers | A deterministic projection of the supported static catalog in `src/TransitCatalog.cpp` |

Official references:

- [DATA.GOV.HK terms and conditions](https://data.gov.hk/tc/terms-and-conditions)
- [Transport Department routes and fares dataset](https://data.gov.hk/en-data/dataset/hk-td-tis_3-routes-and-fares-of-public-transport)
- [Transport Department route and fare data specification](https://static.data.gov.hk/td/routes-and-fares/dataspec/ptroutefare_dataspec.pdf)
- [Citybus route-stop API resource](https://data.gov.hk/tc-data/dataset/ctb-eta-transport-realtime-eta/resource/e51f302a-39a2-4034-bd7c-90ace2b6bc8b)
- [Green Minibus real-time arrival dataset](https://data.gov.hk/en-data/dataset/hk-td-sm_7-real-time-arrival-data-of-gmb)

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

After generating a new baseline, review the manifest counts, attribution,
generated date and source changes before committing and publishing new
firmware. End-user route refreshes contact the same official sources from the
device and store only the selected route as a local LittleFS override; they do
not create or redistribute a separate catalog release.
