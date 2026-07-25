#!/usr/bin/env python3
"""Build the versioned TransitInk frontend transport catalog.

The generated catalog deliberately contains only identifiers and official
Traditional Chinese and English labels required by the settings portal.  A
missing official translation is emitted as an empty string so the client can
fall back to the provider's original language.  ETA responses remain live.
Network refreshes are explicit; normal firmware builds consume checked-in
generated assets and never contact a transport provider.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import gzip
import hashlib
import json
import re
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "data" / "catalog"
DEFAULT_CACHE = ROOT / ".catalog-cache"
GENERATED_HEADER = ROOT / "include" / "generated" / "TransitCatalogAssets.h"
GENERATED_SOURCE = ROOT / "src" / "generated" / "TransitCatalogAssets.cpp"

SCHEMA_VERSION = 1
MAX_RELEASE_BYTES = 3_145_728
MAX_INDEX_GZIP_BYTES = 196_608

KMB_ROUTES_URL = "https://data.etabus.gov.hk/v1/transport/kmb/route/"
KMB_STOPS_URL = "https://data.etabus.gov.hk/v1/transport/kmb/stop"
KMB_ROUTE_STOPS_URL = "https://data.etabus.gov.hk/v1/transport/kmb/route-stop"
CTB_ROUTES_URL = "https://rt.data.gov.hk/v2/transport/citybus/route/CTB"
CTB_ROUTE_STOPS_URL = (
    "https://rt.data.gov.hk/v2/transport/citybus/route-stop/CTB/{route}/{direction}"
)
CTB_STOP_URL = "https://rt.data.gov.hk/v2/transport/citybus/stop/{stop}"
GMB_GEOJSON_URL = "https://static.data.gov.hk/td/routes-fares-geojson/JSON_GMB.json"
GMB_ROUTES_URL = "https://data.etagmb.gov.hk/route/{region}"
GMB_ROUTE_URL = "https://data.etagmb.gov.hk/route/{region}/{route_code}"
TFL_BUS_ROUTES_URL = "https://api.tfl.gov.uk/Line/Mode/bus/Route"
TFL_BUS_SEQUENCE_URL = (
    "https://api.tfl.gov.uk/Line/{route}/Route/Sequence/all"
)
TFL_RAIL_LINES_URL = (
    "https://api.tfl.gov.uk/Line/Mode/"
    "tube,dlr,overground,elizabeth-line,tram"
)
TFL_RAIL_STATIONS_URL = "https://api.tfl.gov.uk/Line/{line}/StopPoints"
TFL_ANONYMOUS_REQUEST_INTERVAL_SECONDS = 1.3

ASSET_NAMES = (
    "index.json.gz",
    "stops-kmb.json.gz",
    "stops-ctb.json.gz",
    "stops-gmb.json.gz",
    "stops-tfl.json.gz",
    "rail.json.gz",
)


class CatalogError(RuntimeError):
    pass


def canonical_json(value: Any) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def gzip_bytes(value: Any) -> bytes:
    return gzip.compress(canonical_json(value), compresslevel=9, mtime=0)


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as handle:
        return json.load(handle)


def write_if_changed(path: Path, content: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == content:
        return
    path.write_bytes(content)


def request_bytes(url: str, attempts: int = 4) -> bytes:
    headers = {"User-Agent": "TransitInk-catalog-generator/1.0"}
    last_error: Exception | None = None
    for attempt in range(attempts):
        try:
            with urllib.request.urlopen(
                urllib.request.Request(url, headers=headers), timeout=45
            ) as response:
                if response.status != 200:
                    raise CatalogError(f"{url}: HTTP {response.status}")
                return response.read()
        except (OSError, urllib.error.URLError, CatalogError) as error:
            last_error = error
            if attempt + 1 < attempts:
                time.sleep(1.5 * (attempt + 1))
    raise CatalogError(f"下載失敗：{url}: {last_error}")


def cached_request(url: str, path: Path) -> bytes:
    if path.exists() and path.stat().st_size > 0:
        return path.read_bytes()
    content = request_bytes(url)
    write_if_changed(path, content)
    return content


def require_text(value: Any, field: str, max_bytes: int = 128) -> str:
    if not isinstance(value, (str, int)):
        raise CatalogError(f"{field} 格式不正確")
    text = str(value).strip()
    if not text or len(text.encode("utf-8")) > max_bytes or "\ufffd" in text:
        raise CatalogError(f"{field} 不可為空、過長或包含亂碼")
    return text


def positive_sequence(value: Any, field: str) -> int:
    try:
        number = int(value)
    except (TypeError, ValueError) as error:
        raise CatalogError(f"{field} 格式不正確") from error
    if number <= 0 or number > 999:
        raise CatalogError(f"{field} 超出範圍")
    return number


def normalized_label(value: Any) -> str:
    label = require_text(value, "站牌名稱")
    # Provider suffixes such as "(...)(TW515)" are identifiers, not display copy.
    label = re.sub(r"\s*\((?:[A-Z]{1,4}\d{2,8}|[A-F0-9]{8,})\)\s*$", "", label)
    return label.strip()


def optional_label(value: Any) -> str:
    if value is None or str(value).strip() == "":
        return ""
    text = str(value).strip()
    if len(text.encode("utf-8")) > 96 or "\ufffd" in text:
        return ""
    return re.sub(
        r"\s*\((?:[A-Z]{1,4}\d{2,8}|[A-F0-9]{8,})\)\s*$", "", text
    ).strip()


def normalized_stop_sequence(
    stops: Iterable[dict[str, Any]], context: str
) -> list[dict[str, Any]]:
    unique: dict[bytes, dict[str, Any]] = {}
    for stop in stops:
        unique[canonical_json(stop)] = stop
    ordered = sorted(unique.values(), key=lambda value: value["sequence"])
    previous = 0
    for stop in ordered:
        sequence = positive_sequence(stop.get("sequence"), f"{context}站序")
        if sequence <= previous:
            raise CatalogError(f"{context}站序必須嚴格遞增")
        previous = sequence
    return ordered


def parse_json_bytes(content: bytes, source: str) -> Any:
    try:
        return json.loads(content.decode("utf-8-sig"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise CatalogError(f"{source} JSON／UTF-8 格式不正確") from error


def fetch_base_sources(cache: Path) -> None:
    sources = {
        "kmb-routes.json": KMB_ROUTES_URL,
        "kmb-stops.json": KMB_STOPS_URL,
        "kmb-route-stops.json": KMB_ROUTE_STOPS_URL,
        "ctb-routes.json": CTB_ROUTES_URL,
        "gmb-routes-stops.json": GMB_GEOJSON_URL,
        "gmb-routes-HKI.json": GMB_ROUTES_URL.format(region="HKI"),
        "gmb-routes-KLN.json": GMB_ROUTES_URL.format(region="KLN"),
        "gmb-routes-NT.json": GMB_ROUTES_URL.format(region="NT"),
    }
    cache.mkdir(parents=True, exist_ok=True)
    for name, url in sources.items():
        content = request_bytes(url)
        parse_json_bytes(content, name)
        write_if_changed(cache / name, content)


def fetch_gmb_catalog(cache: Path, workers: int) -> None:
    jobs: list[tuple[str, str]] = []
    for region in ("HKI", "KLN", "NT"):
        payload = read_json(cache / f"gmb-routes-{region}.json")
        routes = payload.get("data", {}).get("routes") if isinstance(payload, dict) else None
        if not isinstance(routes, list) or not routes:
            raise CatalogError(f"小巴 {region} ETA 路線目錄不可為空")
        jobs.extend((region, require_text(route, "小巴 ETA 路線", 16).upper()) for route in routes)

    def fetch_route(region: str, route_code: str) -> tuple[str, str, dict[str, Any]]:
        safe_code = urllib.parse.quote(route_code, safe="")
        path = cache / "gmb-route-details" / f"{region}-{safe_code}.json"
        content = cached_request(
            GMB_ROUTE_URL.format(region=region, route_code=safe_code), path
        )
        payload = parse_json_bytes(content, str(path))
        if not isinstance(payload, dict) or not isinstance(payload.get("data"), list):
            raise CatalogError(f"小巴 ETA 路線 {region}/{route_code} 格式不正確")
        return region, route_code, payload

    details: dict[str, dict[str, Any]] = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, workers)) as pool:
        futures = {
            pool.submit(fetch_route, region, route_code): (region, route_code)
            for region, route_code in jobs
        }
        for future in concurrent.futures.as_completed(futures):
            region, route_code, payload = future.result()
            details[f"{region}:{route_code}"] = payload
    write_if_changed(cache / "gmb-eta-routes.json", canonical_json(details))


def fetch_tfl_catalog(cache: Path, workers: int) -> None:
    cache.mkdir(parents=True, exist_ok=True)
    bus_routes_content = request_bytes(TFL_BUS_ROUTES_URL)
    bus_routes = parse_json_bytes(bus_routes_content, "tfl-bus-routes.json")
    if not isinstance(bus_routes, list) or not bus_routes:
        raise CatalogError("TfL 巴士路線目錄不可為空")
    write_if_changed(cache / "tfl-bus-routes.json", bus_routes_content)

    rail_lines_content = request_bytes(TFL_RAIL_LINES_URL)
    rail_lines = parse_json_bytes(rail_lines_content, "tfl-rail-lines.json")
    if not isinstance(rail_lines, list) or not rail_lines:
        raise CatalogError("TfL 鐵路路線目錄不可為空")
    write_if_changed(cache / "tfl-rail-lines.json", rail_lines_content)

    limiter_lock = threading.Lock()
    next_request_at = [time.monotonic()]

    def rate_limited_cached_request(url: str, path: Path) -> bytes:
        if path.exists() and path.stat().st_size > 0:
            return path.read_bytes()
        with limiter_lock:
            delay = next_request_at[0] - time.monotonic()
            if delay > 0:
                time.sleep(delay)
            next_request_at[0] = (
                time.monotonic() + TFL_ANONYMOUS_REQUEST_INTERVAL_SECONDS
            )
        content = request_bytes(url)
        write_if_changed(path, content)
        return content

    bus_ids = sorted(
        {
            require_text(row.get("id"), "TfL 巴士路線", 16).lower()
            for row in bus_routes
            if isinstance(row, dict)
        }
    )
    rail_ids = sorted(
        {
            require_text(row.get("id"), "TfL 鐵路路線", 48).lower()
            for row in rail_lines
            if isinstance(row, dict)
        }
    )

    def fetch_bus_sequence(route: str) -> None:
        path = cache / "tfl-bus-sequences" / f"{route}.json"
        content = rate_limited_cached_request(
            TFL_BUS_SEQUENCE_URL.format(
                route=urllib.parse.quote(route, safe="")
            ),
            path,
        )
        payload = parse_json_bytes(content, str(path))
        if not isinstance(payload, dict):
            raise CatalogError(f"TfL 巴士 {route} 站序格式不正確")

    def fetch_rail_stations(line: str) -> None:
        path = cache / "tfl-rail-stations" / f"{line}.json"
        content = rate_limited_cached_request(
            TFL_RAIL_STATIONS_URL.format(
                line=urllib.parse.quote(line, safe="")
            ),
            path,
        )
        payload = parse_json_bytes(content, str(path))
        if not isinstance(payload, list):
            raise CatalogError(f"TfL 鐵路 {line} 車站格式不正確")

    jobs = [
        (fetch_bus_sequence, route)
        for route in bus_ids
    ] + [
        (fetch_rail_stations, line)
        for line in rail_ids
    ]
    with concurrent.futures.ThreadPoolExecutor(
        max_workers=max(1, min(workers, 4))
    ) as pool:
        futures = [pool.submit(function, value) for function, value in jobs]
        for future in concurrent.futures.as_completed(futures):
            future.result()


def ctb_route_stop_rows(cache: Path, route: str, direction: str) -> list[dict[str, Any]]:
    path = cache / "ctb-route-stops" / f"{route}-{direction}.json"
    content = cached_request(
        CTB_ROUTE_STOPS_URL.format(route=route, direction=direction), path
    )
    payload = parse_json_bytes(content, str(path))
    data = payload.get("data") if isinstance(payload, dict) else None
    if data in ({}, None):
        return []
    if not isinstance(data, list):
        raise CatalogError(f"{path} route-stop data 格式不正確")
    return data


def fetch_ctb_catalog(cache: Path, routes_payload: dict[str, Any], workers: int) -> None:
    rows = routes_payload.get("data")
    if not isinstance(rows, list) or not rows:
        raise CatalogError("城巴路線目錄不可為空")
    route_ids = sorted({require_text(row.get("route"), "城巴路線") for row in rows})
    jobs = [(route, direction) for route in route_ids for direction in ("outbound", "inbound")]

    route_rows: dict[tuple[str, str], list[dict[str, Any]]] = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, workers)) as pool:
        futures = {
            pool.submit(ctb_route_stop_rows, cache, route, direction): (route, direction)
            for route, direction in jobs
        }
        for future in concurrent.futures.as_completed(futures):
            key = futures[future]
            route_rows[key] = future.result()

    all_stop_ids = sorted(
        {
            require_text(row.get("stop"), "城巴站牌 ID", 48)
            for rows_for_direction in route_rows.values()
            for row in rows_for_direction
        }
    )

    def fetch_stop(stop_id: str) -> tuple[str, dict[str, Any] | None]:
        path = cache / "ctb-stops" / f"{stop_id}.json"
        content = cached_request(CTB_STOP_URL.format(stop=stop_id), path)
        payload = parse_json_bytes(content, str(path))
        data = payload.get("data") if isinstance(payload, dict) else None
        if not isinstance(data, dict):
            raise CatalogError(f"城巴站牌 {stop_id} 資料格式不正確")
        # Citybus occasionally leaves a retired stop in route-stop while both
        # v1 and v2 stop endpoints return an empty object.  Keep the omission
        # explicit and bounded instead of exposing the raw stop ID as a label.
        return stop_id, data or None

    stop_payloads: dict[str, dict[str, Any]] = {}
    unavailable_stops: list[str] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, workers)) as pool:
        futures = {pool.submit(fetch_stop, stop): stop for stop in all_stop_ids}
        for future in concurrent.futures.as_completed(futures):
            stop_id, payload = future.result()
            if payload is None:
                unavailable_stops.append(stop_id)
            else:
                stop_payloads[stop_id] = payload

    if len(unavailable_stops) > max(10, len(all_stop_ids) // 100):
        raise CatalogError(
            f"城巴有過多站牌沒有名稱：{len(unavailable_stops)}/{len(all_stop_ids)}"
        )

    combined = {
        "generated_timestamp": routes_payload.get("generated_timestamp", ""),
        "route_stops": {
            f"{route}:{direction}": route_rows.get((route, direction), [])
            for route, direction in jobs
        },
        "stops": stop_payloads,
        "unavailable_stops": sorted(unavailable_stops),
    }
    write_if_changed(cache / "ctb-combined.json", canonical_json(combined))


def build_kmb(cache: Path) -> tuple[dict[str, Any], dict[str, Any], str]:
    routes_payload = read_json(cache / "kmb-routes.json")
    stops_payload = read_json(cache / "kmb-stops.json")
    route_stops_payload = read_json(cache / "kmb-route-stops.json")
    routes = routes_payload.get("data")
    stops = stops_payload.get("data")
    route_stops = route_stops_payload.get("data")
    if not all(isinstance(value, list) and value for value in (routes, stops, route_stops)):
        raise CatalogError("九巴 bulk 目錄不可為空")

    labels = {
        require_text(row.get("stop"), "九巴站牌 ID", 48): (
            normalized_label(row.get("name_tc")),
            optional_label(row.get("name_en")),
        )
        for row in stops
    }
    metadata: dict[tuple[str, str, str], dict[str, str]] = {}
    for row in routes:
        route = require_text(row.get("route"), "九巴路線", 16).upper()
        bound = require_text(row.get("bound"), "九巴方向", 4).upper()
        service = require_text(row.get("service_type", "1"), "九巴服務類型", 8)
        metadata[(route, bound, service)] = {
            "direction_id": bound,
            "service_type": service,
            "origin_label_tc": require_text(row.get("orig_tc"), "九巴起點"),
            "destination_label_tc": require_text(row.get("dest_tc"), "九巴終點"),
            "origin_label_en": optional_label(row.get("orig_en")),
            "destination_label_en": optional_label(row.get("dest_en")),
            "stop_key": f"{route}:{bound}:{service}",
        }

    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    unavailable_stops: set[str] = set()
    for row in route_stops:
        route = require_text(row.get("route"), "九巴路線", 16).upper()
        bound = require_text(row.get("bound"), "九巴方向", 4).upper()
        service = require_text(row.get("service_type", "1"), "九巴服務類型", 8)
        key = (route, bound, service)
        if key not in metadata:
            continue
        stop_id = require_text(row.get("stop"), "九巴站牌 ID", 48)
        if stop_id not in labels:
            unavailable_stops.add(stop_id)
            continue
        grouped[metadata[key]["stop_key"]].append(
            {
                "id": stop_id,
                "label_tc": labels[stop_id][0],
                "label_en": labels[stop_id][1],
                "sequence": positive_sequence(row.get("seq"), "九巴站序"),
            }
        )

    if len(unavailable_stops) > max(10, len(labels) // 100):
        raise CatalogError(f"九巴有過多站牌沒有名稱：{len(unavailable_stops)}")

    routes_out: dict[str, list[dict[str, str]]] = defaultdict(list)
    for key in sorted(metadata):
        item = metadata[key]
        stops_for_direction = grouped.get(item["stop_key"], [])
        if not stops_for_direction:
            raise CatalogError(f"九巴方向沒有站牌：{item['stop_key']}")
        grouped[item["stop_key"]] = normalized_stop_sequence(
            stops_for_direction, f"九巴 {item['stop_key']} "
        )
        routes_out[key[0]].append(item)

    index = {"routes": dict(sorted(routes_out.items()))}
    stop_asset = {"provider": "kmb", "routes": dict(sorted(grouped.items()))}
    timestamp = max(
        str(routes_payload.get("generated_timestamp", "")),
        str(stops_payload.get("generated_timestamp", "")),
        str(route_stops_payload.get("generated_timestamp", "")),
    )
    return index, stop_asset, timestamp


def build_ctb(cache: Path) -> tuple[dict[str, Any], dict[str, Any], str]:
    routes_payload = read_json(cache / "ctb-routes.json")
    combined = read_json(cache / "ctb-combined.json")
    routes = routes_payload.get("data")
    if not isinstance(routes, list) or not routes:
        raise CatalogError("城巴路線目錄不可為空")
    stop_payloads = combined.get("stops")
    route_stop_payloads = combined.get("route_stops")
    if not isinstance(stop_payloads, dict) or not isinstance(route_stop_payloads, dict):
        raise CatalogError("城巴站牌 cache 格式不正確，請先執行 --refresh")

    route_meta: dict[str, dict[str, str]] = {}
    for row in routes:
        route = require_text(row.get("route"), "城巴路線", 16).upper()
        route_meta[route] = {
            "origin": require_text(row.get("orig_tc"), "城巴起點"),
            "destination": require_text(row.get("dest_tc"), "城巴終點"),
            "origin_en": optional_label(row.get("orig_en")),
            "destination_en": optional_label(row.get("dest_en")),
        }

    routes_out: dict[str, list[dict[str, str]]] = defaultdict(list)
    stops_out: dict[str, list[dict[str, Any]]] = {}
    for route in sorted(route_meta):
        meta = route_meta[route]
        found = False
        for path_direction, direction_id, origin, destination, origin_en, destination_en in (
            ("outbound", "O", meta["origin"], meta["destination"],
             meta["origin_en"], meta["destination_en"]),
            ("inbound", "I", meta["destination"], meta["origin"],
             meta["destination_en"], meta["origin_en"]),
        ):
            rows = route_stop_payloads.get(f"{route}:{path_direction}")
            if not isinstance(rows, list) or not rows:
                continue
            found = True
            stop_key = f"{route}:{direction_id}:1"
            parsed_stops: list[dict[str, Any]] = []
            for row in rows:
                stop_id = require_text(row.get("stop"), "城巴站牌 ID", 48)
                stop_data = stop_payloads.get(stop_id)
                if not isinstance(stop_data, dict):
                    continue
                parsed_stops.append(
                    {
                        "id": stop_id,
                        "label_tc": normalized_label(stop_data.get("name_tc")),
                        "label_en": optional_label(stop_data.get("name_en")),
                        "sequence": positive_sequence(row.get("seq"), "城巴站序"),
                    }
                )
            if not parsed_stops:
                continue
            stops_out[stop_key] = normalized_stop_sequence(
                parsed_stops, f"城巴 {stop_key} "
            )
            routes_out[route].append(
                {
                    "direction_id": direction_id,
                    "service_type": "1",
                    "origin_label_tc": origin,
                    "destination_label_tc": destination,
                    "origin_label_en": origin_en,
                    "destination_label_en": destination_en,
                    "stop_key": stop_key,
                }
            )
        if not found:
            # Some route-list rows describe inactive variants.  They must not
            # appear as selectable routes without a usable stop sequence.
            continue

    if len(routes_out) < 200:
        raise CatalogError("城巴可用路線數量異常偏低")
    return (
        {"routes": dict(sorted(routes_out.items()))},
        {"provider": "ctb", "routes": dict(sorted(stops_out.items()))},
        str(combined.get("generated_timestamp", "")),
    )


def build_gmb(cache: Path) -> tuple[dict[str, Any], dict[str, Any], str]:
    payload = read_json(cache / "gmb-routes-stops.json")
    eta_routes = read_json(cache / "gmb-eta-routes.json")
    features = payload.get("features")
    if not isinstance(features, list) or not features:
        raise CatalogError("小巴路線站牌資料不可為空")

    verified: set[tuple[str, str, str, str]] = set()
    if not isinstance(eta_routes, dict) or not eta_routes:
        raise CatalogError("小巴 ETA 路線核對資料不可為空，請先執行 --refresh")
    for payload_for_route in eta_routes.values():
        rows = payload_for_route.get("data") if isinstance(payload_for_route, dict) else None
        if not isinstance(rows, list):
            raise CatalogError("小巴 ETA 路線核對資料格式不正確")
        for row in rows:
            region = require_text(row.get("region"), "小巴 ETA 地區", 4).upper()
            route_code = require_text(row.get("route_code"), "小巴 ETA 路線", 16).upper()
            route_id = require_text(row.get("route_id"), "小巴 ETA route_id", 16)
            route_directions = row.get("directions")
            if not isinstance(route_directions, list) or not route_directions:
                raise CatalogError(f"小巴 ETA 路線沒有方向：{region}/{route_code}")
            for direction in route_directions:
                route_seq = require_text(direction.get("route_seq"), "小巴 ETA route_seq", 4)
                verified.add((region, route_code, route_id, route_seq))

    directions: dict[tuple[str, str, str, str], dict[str, Any]] = {}
    grouped_stops: dict[str, list[dict[str, Any]]] = defaultdict(list)
    timestamps: list[str] = []
    for feature in features:
        properties = feature.get("properties") if isinstance(feature, dict) else None
        if not isinstance(properties, dict) or properties.get("companyCode") != "GMB":
            continue
        region = require_text(properties.get("district"), "小巴地區", 4).upper()
        if region not in {"HKI", "KLN", "NT"}:
            raise CatalogError(f"未知小巴地區：{region}")
        route_code = require_text(properties.get("routeNameC"), "小巴路線", 16).upper()
        route_id = require_text(properties.get("routeId"), "小巴 route_id", 16)
        route_seq = require_text(properties.get("routeSeq"), "小巴 route_seq", 4)
        if (region, route_code, route_id, route_seq) not in verified:
            raise CatalogError(
                f"小巴 route_id／route_seq 未通過 ETA API 核對："
                f"{region}/{route_code}/{route_id}/{route_seq}"
            )
        stop_seq = positive_sequence(properties.get("stopSeq"), "小巴站序")
        stop_id = require_text(properties.get("stopId"), "小巴 stop_id", 16)
        stop_key = f"{route_id}:{route_seq}"
        direction_key = (route_code, region, route_id, route_seq)
        directions[direction_key] = {
            "id": stop_key,
            "region": region,
            "route_id": route_id,
            "route_seq": route_seq,
            "origin_label_tc": require_text(properties.get("locStartNameC"), "小巴起點"),
            "destination_label_tc": require_text(properties.get("locEndNameC"), "小巴終點"),
            "origin_label_en": optional_label(properties.get("locStartNameE")),
            "destination_label_en": optional_label(properties.get("locEndNameE")),
            "stop_key": stop_key,
        }
        grouped_stops[stop_key].append(
            {
                "id": str(stop_seq),
                "stop_id": stop_id,
                "stop_seq": str(stop_seq),
                "label_tc": normalized_label(properties.get("stopNameC")),
                "label_en": optional_label(properties.get("stopNameE")),
                "sequence": stop_seq,
            }
        )
        timestamps.append(str(properties.get("lastUpdateDate", "")))

    routes_out: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for key in sorted(directions):
        item = directions[key]
        stops = grouped_stops.get(item["stop_key"], [])
        if not stops:
            raise CatalogError(f"小巴方向沒有站牌：{item['stop_key']}")
        grouped_stops[item["stop_key"]] = normalized_stop_sequence(
            stops, f"小巴 {item['stop_key']} "
        )
        routes_out[key[0]].append(item)
    return (
        {"routes": dict(sorted(routes_out.items()))},
        {"provider": "gmb", "routes": dict(sorted(grouped_stops.items()))},
        max(timestamps, default=""),
    )


def build_tfl(
    cache: Path,
) -> tuple[dict[str, Any], dict[str, Any], list[dict[str, Any]], str]:
    bus_routes = read_json(cache / "tfl-bus-routes.json")
    rail_lines = read_json(cache / "tfl-rail-lines.json")
    if not isinstance(bus_routes, list) or not bus_routes:
        raise CatalogError("TfL 巴士路線目錄不可為空，請先執行 --refresh-tfl")
    if not isinstance(rail_lines, list) or not rail_lines:
        raise CatalogError("TfL 鐵路路線目錄不可為空，請先執行 --refresh-tfl")

    routes_out: dict[str, list[dict[str, str]]] = defaultdict(list)
    stops_out: dict[str, list[dict[str, Any]]] = {}
    timestamps: list[str] = []
    for line in sorted(bus_routes, key=lambda item: str(item.get("id", ""))):
        if not isinstance(line, dict):
            continue
        route_source = require_text(line.get("id"), "TfL 巴士路線", 16)
        if not re.fullmatch(r"[A-Za-z0-9]+", route_source):
            raise CatalogError(f"TfL 巴士路線 ID 格式不正確：{route_source}")
        route = route_source.upper()
        sections = line.get("routeSections")
        if not isinstance(sections, list) or not sections:
            raise CatalogError(f"TfL 巴士路線沒有方向：{route}")
        sequence_path = cache / "tfl-bus-sequences" / f"{route_source.lower()}.json"
        sequence_payload = read_json(sequence_path)
        ordered_routes = (
            sequence_payload.get("orderedLineRoutes")
            if isinstance(sequence_payload, dict)
            else None
        )
        stop_sequences = (
            sequence_payload.get("stopPointSequences")
            if isinstance(sequence_payload, dict)
            else None
        )
        if not isinstance(ordered_routes, list) or not isinstance(
            stop_sequences, list
        ):
            raise CatalogError(f"TfL 巴士 {route} 站序格式不正確")

        labels: dict[str, str] = {}
        for sequence in stop_sequences:
            points = sequence.get("stopPoint") if isinstance(sequence, dict) else None
            if not isinstance(points, list):
                continue
            for point in points:
                if not isinstance(point, dict):
                    continue
                stop_id = require_text(point.get("id"), "TfL 巴士站牌 ID", 48)
                if not re.fullmatch(r"[A-Za-z0-9]+", stop_id):
                    raise CatalogError(f"TfL 巴士站牌 ID 格式不正確：{stop_id}")
                labels.setdefault(
                    stop_id, normalized_label(point.get("name"))
                )

        usable_ordered: list[list[str]] = []
        for ordered in ordered_routes:
            ids = ordered.get("naptanIds") if isinstance(ordered, dict) else None
            if not isinstance(ids, list) or not ids:
                continue
            parsed_ids = [
                require_text(value, "TfL 巴士站牌 ID", 48)
                for value in ids
            ]
            usable_ordered.append(parsed_ids)

        seen_directions: set[str] = set()
        for section in sections:
            if not isinstance(section, dict):
                continue
            direction = require_text(
                section.get("direction"), "TfL 巴士方向", 16
            ).lower()
            if direction not in {"inbound", "outbound"}:
                continue
            originator = require_text(
                section.get("originator"), "TfL 巴士起點 ID", 48
            )
            destination = require_text(
                section.get("destination"), "TfL 巴士終點 ID", 48
            )
            if not re.fullmatch(r"[A-Za-z0-9]+", originator) or not re.fullmatch(
                r"[A-Za-z0-9]+", destination
            ):
                raise CatalogError(f"TfL 巴士 {route} 方向 ID 格式不正確")
            service_type = f"{originator}|{destination}"
            direction_key = f"{direction}:{service_type}"
            if direction_key in seen_directions:
                continue

            matching_segments: dict[tuple[str, ...], list[str]] = {}
            for ids in usable_ordered:
                try:
                    start = ids.index(originator)
                    end = ids.index(destination, start)
                except ValueError:
                    continue
                segment = ids[start : end + 1]
                matching_segments[tuple(segment)] = segment
            if not matching_segments and len(usable_ordered) == 1:
                only_route = usable_ordered[0]
                matching_segments[tuple(only_route)] = only_route
            if not matching_segments:
                raise CatalogError(
                    f"TfL 巴士 {route} 找不到方向站序：{direction_key}"
                )
            # TfL can publish two ordered variants with the same public
            # origin/destination and no distinct service identifier. Keep the
            # longest official variant so every shared stop and the most
            # complete branch remain selectable.
            selected = max(
                matching_segments.values(),
                key=lambda values: (len(values), tuple(values)),
            )
            parsed_stops = []
            for sequence_number, stop_id in enumerate(selected, 1):
                label = labels.get(stop_id)
                if not label:
                    raise CatalogError(
                        f"TfL 巴士 {route} 站牌名稱不完整：{stop_id}"
                    )
                parsed_stops.append(
                    {
                        "id": stop_id,
                        "label_tc": label,
                        "label_en": label,
                        "sequence": sequence_number,
                    }
                )
            stop_key = f"{route}:{direction}:{service_type}"
            stops_out[stop_key] = normalized_stop_sequence(
                parsed_stops, f"TfL 巴士 {stop_key} "
            )
            origin_label = normalized_label(section.get("originationName"))
            destination_label = normalized_label(section.get("destinationName"))
            routes_out[route].append(
                {
                    "direction_id": direction,
                    "service_type": service_type,
                    "origin_label_tc": origin_label,
                    "destination_label_tc": destination_label,
                    "origin_label_en": origin_label,
                    "destination_label_en": destination_label,
                    "stop_key": stop_key,
                }
            )
            seen_directions.add(direction_key)
        if not routes_out[route]:
            raise CatalogError(f"TfL 巴士路線沒有可用方向：{route}")
        timestamps.extend(
            [str(line.get("created", "")), str(line.get("modified", ""))]
        )

    rail_groups: list[dict[str, Any]] = []
    for line in sorted(rail_lines, key=lambda item: str(item.get("id", ""))):
        if not isinstance(line, dict):
            continue
        line_id = require_text(line.get("id"), "TfL 鐵路路線 ID", 48)
        if not re.fullmatch(r"[A-Za-z0-9-]+", line_id):
            raise CatalogError(f"TfL 鐵路路線 ID 格式不正確：{line_id}")
        line_label = normalized_label(line.get("name"))
        stations_payload = read_json(
            cache / "tfl-rail-stations" / f"{line_id.lower()}.json"
        )
        if not isinstance(stations_payload, list) or not stations_payload:
            raise CatalogError(f"TfL 鐵路 {line_id} 車站不可為空")
        stations_by_id: dict[str, dict[str, str]] = {}
        for station in stations_payload:
            if not isinstance(station, dict):
                continue
            station_id = require_text(
                station.get("id"), "TfL 鐵路車站 ID", 64
            )
            if not re.fullmatch(r"[A-Za-z0-9-]+", station_id):
                raise CatalogError(
                    f"TfL 鐵路車站 ID 格式不正確：{station_id}"
                )
            station_label = normalized_label(station.get("commonName"))
            stations_by_id[station_id] = {
                "id": station_id,
                "label_tc": station_label,
                "label_en": station_label,
            }
        if not stations_by_id:
            raise CatalogError(f"TfL 鐵路 {line_id} 沒有可用車站")
        rail_groups.append(
            {
                "id": line_id,
                "label_tc": line_label,
                "label_en": line_label,
                "stations": sorted(
                    stations_by_id.values(),
                    key=lambda item: (item["label_en"], item["id"]),
                ),
                "directions": [
                    {
                        "id": "inbound",
                        "label_tc": "Inbound",
                        "label_en": "Inbound",
                    },
                    {
                        "id": "outbound",
                        "label_tc": "Outbound",
                        "label_en": "Outbound",
                    },
                ],
            }
        )
        timestamps.extend(
            [str(line.get("created", "")), str(line.get("modified", ""))]
        )

    return (
        {"routes": dict(sorted(routes_out.items()))},
        {"provider": "tfl", "routes": dict(sorted(stops_out.items()))},
        rail_groups,
        max((value for value in timestamps if value), default=""),
    )


def extract_rail_catalog(source_path: Path) -> dict[str, Any]:
    source = source_path.read_text(encoding="utf-8")
    arrays: dict[str, list[dict[str, str]]] = {}
    array_pattern = re.compile(
        r"const TransitCatalogItem\s+(k\w+)\[\]\s*=\s*\{(.*?)\n\};", re.S
    )
    item_pattern = re.compile(
        r'\{"((?:\\.|[^"\\])*)",\s*"((?:\\.|[^"\\])*)"'
        r'(?:,\s*"((?:\\.|[^"\\])*)")?\}'
    )
    for match in array_pattern.finditer(source):
        name, body = match.groups()
        arrays[name] = [
            {"id": bytes(item[0], "utf-8").decode("unicode_escape") if "\\" in item[0] else item[0],
             "label_tc": bytes(item[1], "utf-8").decode("unicode_escape") if "\\" in item[1] else item[1],
             "label_en": bytes(item[2], "utf-8").decode("unicode_escape") if "\\" in item[2] else item[2]}
            for item in item_pattern.findall(body)
        ]

    def groups(array_name: str) -> list[dict[str, Any]]:
        match = re.search(
            rf"const TransitCatalogGroup\s+{array_name}\[\]\s*=\s*\{{(.*?)\n\}};",
            source,
            re.S,
        )
        if not match:
            raise CatalogError(f"找不到 {array_name}")
        group_pattern = re.compile(
            r'\{"([^"]+)",\s*"([^"]+)",\s*(k\w+),\s*itemCount\([^)]*\),'
            r'\s*(k\w+),\s*itemCount\([^)]*\)(?:,\s*"([^"]*)")?\}'
        )
        parsed = []
        for group_id, label, stations_name, directions_name, label_en in group_pattern.findall(match.group(1)):
            stations = arrays.get(stations_name)
            directions = arrays.get(directions_name)
            if not stations or not directions:
                raise CatalogError(f"鐵路目錄引用不存在：{group_id}")
            parsed.append(
                {
                    "id": group_id,
                    "label_tc": label,
                    "label_en": label_en,
                    "stations": stations,
                    "directions": directions,
                }
            )
        if not parsed:
            raise CatalogError(f"{array_name} 不可為空")
        return parsed

    return {
        "modes": {
            "heavy_rail": groups("kHeavyRailGroups"),
            "light_rail": groups("kLightRailGroups"),
        }
    }


def with_common_metadata(payload: dict[str, Any], revision: str, generated_at: str) -> dict[str, Any]:
    return {
        "schema_version": SCHEMA_VERSION,
        "revision": revision,
        "generated_at": generated_at,
        **payload,
    }


def build_assets(cache: Path) -> tuple[dict[str, bytes], dict[str, Any]]:
    kmb_index, kmb_stops, kmb_time = build_kmb(cache)
    ctb_index, ctb_stops, ctb_time = build_ctb(cache)
    gmb_index, gmb_stops, gmb_time = build_gmb(cache)
    tfl_index, tfl_stops, tfl_rail, tfl_time = build_tfl(cache)
    rail = extract_rail_catalog(ROOT / "src" / "TransitCatalog.cpp")
    rail["modes"]["london_rail"] = tfl_rail

    generated_at = max(kmb_time, ctb_time, gmb_time, tfl_time)
    if not generated_at:
        generated_at = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    seed = {
        "schema_version": SCHEMA_VERSION,
        "generated_at": generated_at,
        "bus": {"kmb_lwb": kmb_index, "ctb": ctb_index, "tfl": tfl_index},
        "gmb": gmb_index,
        "rail": rail,
    }
    revision = sha256_bytes(canonical_json(seed))[:16]
    index = with_common_metadata(
        {
            "bus": {
                "kmb_lwb": kmb_index,
                "ctb": ctb_index,
                "tfl": tfl_index,
            },
            "gmb": gmb_index,
        },
        revision,
        generated_at,
    )
    payloads = {
        "index.json.gz": gzip_bytes(index),
        "stops-kmb.json.gz": gzip_bytes(with_common_metadata(kmb_stops, revision, generated_at)),
        "stops-ctb.json.gz": gzip_bytes(with_common_metadata(ctb_stops, revision, generated_at)),
        "stops-gmb.json.gz": gzip_bytes(with_common_metadata(gmb_stops, revision, generated_at)),
        "stops-tfl.json.gz": gzip_bytes(with_common_metadata(tfl_stops, revision, generated_at)),
        "rail.json.gz": gzip_bytes(with_common_metadata(rail, revision, generated_at)),
    }
    assets = {
        name: {"bytes": len(content), "sha256": sha256_bytes(content)}
        for name, content in payloads.items()
    }
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "revision": revision,
        "generated_at": generated_at,
        "assets": assets,
        "counts": {
            "kmb_routes": len(kmb_index["routes"]),
            "kmb_stops": sum(len(stops) for stops in kmb_stops["routes"].values()),
            "ctb_routes": len(ctb_index["routes"]),
            "ctb_stops": sum(len(stops) for stops in ctb_stops["routes"].values()),
            "gmb_routes": len(gmb_index["routes"]),
            "gmb_stops": sum(len(stops) for stops in gmb_stops["routes"].values()),
            "tfl_bus_routes": len(tfl_index["routes"]),
            "tfl_bus_stops": sum(
                len(stops) for stops in tfl_stops["routes"].values()
            ),
            "rail_routes": sum(len(groups) for groups in rail["modes"].values()),
            "rail_stations": sum(
                len(group["stations"])
                for groups in rail["modes"].values()
                for group in groups
            ),
        },
    }
    return payloads, manifest


def validate_count_change(
    previous: dict[str, Any] | None,
    current: dict[str, Any],
    allow_large_change: bool,
) -> None:
    if allow_large_change or not previous:
        return
    previous_counts = previous.get("counts")
    current_counts = current.get("counts")
    if not isinstance(previous_counts, dict) or not isinstance(current_counts, dict):
        return
    changed = []
    for name, current_value in current_counts.items():
        previous_value = previous_counts.get(name)
        if not isinstance(previous_value, int) or previous_value <= 0:
            continue
        ratio = abs(current_value - previous_value) / previous_value
        if ratio > 0.10:
            changed.append(f"{name} {previous_value}->{current_value} ({ratio:.1%})")
    if changed:
        raise CatalogError(
            "路線／站牌數量變動超過 10%，需要人工確認並使用 --allow-large-change："
            + ", ".join(changed)
        )


def c_array(name: str, content: bytes) -> str:
    lines = []
    for offset in range(0, len(content), 16):
        chunk = content[offset : offset + 16]
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + ",")
    return (
        f"const uint8_t {name}[] PROGMEM = {{\n"
        + "\n".join(lines)
        + f"\n}};\nconst std::size_t {name}Size = sizeof({name});\n"
    )


def cpp_symbol(name: str) -> str:
    stem = name.replace(".json.gz", "").replace("-", "_")
    return "kCatalog" + "".join(part.capitalize() for part in stem.split("_"))


def generate_cpp(payloads: dict[str, bytes], manifest: dict[str, Any]) -> tuple[bytes, bytes]:
    header = """#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace transitink {

struct EmbeddedCatalogAsset {
    const char* path;
    const uint8_t* data;
    std::size_t size;
    const char* sha256;
};

extern const char kEmbeddedCatalogRevision[];
extern const char kEmbeddedCatalogGeneratedAt[];
extern const EmbeddedCatalogAsset kEmbeddedCatalogAssets[];
extern const std::size_t kEmbeddedCatalogAssetCount;

}  // namespace transitink
"""
    declarations = []
    entries = []
    for asset_name in ASSET_NAMES:
        symbol = cpp_symbol(asset_name)
        declarations.append(c_array(symbol, payloads[asset_name]))
        path = asset_name.removesuffix(".gz")
        entries.append(
            f'    {{"{path}", {symbol}, {symbol}Size, "{manifest["assets"][asset_name]["sha256"]}"}},'
        )
    source = f"""#include \"generated/TransitCatalogAssets.h\"

namespace transitink {{

const char kEmbeddedCatalogRevision[] = \"{manifest['revision']}\";
const char kEmbeddedCatalogGeneratedAt[] = \"{manifest['generated_at']}\";

namespace {{

{''.join(declarations)}
}}  // namespace

const EmbeddedCatalogAsset kEmbeddedCatalogAssets[] = {{
{chr(10).join(entries)}
}};
const std::size_t kEmbeddedCatalogAssetCount =
    sizeof(kEmbeddedCatalogAssets) / sizeof(kEmbeddedCatalogAssets[0]);

}}  // namespace transitink
"""
    return header.encode("utf-8"), source.encode("utf-8")


def validate_release(payloads: dict[str, bytes], manifest: dict[str, Any]) -> None:
    total = sum(len(value) for value in payloads.values())
    if len(payloads["index.json.gz"]) > MAX_INDEX_GZIP_BYTES:
        raise CatalogError("index.json.gz 超過 192 KiB")
    if total > MAX_RELEASE_BYTES:
        raise CatalogError(f"完整目錄 {total} bytes 超過 3 MiB")
    for asset_name, content in payloads.items():
        asset_metadata = manifest.get("assets", {}).get(asset_name, {})
        if asset_metadata.get("bytes") != len(content):
            raise CatalogError(f"{asset_name} 大小不一致")
        if asset_metadata.get("sha256") != sha256_bytes(content):
            raise CatalogError(f"{asset_name} SHA-256 不一致")
        try:
            decoded = json.loads(gzip.decompress(content).decode("utf-8"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
            raise CatalogError(f"{asset_name} 無法解壓或解析") from error
        if decoded.get("schema_version") != SCHEMA_VERSION:
            raise CatalogError(f"{asset_name} schema 不正確")
        if decoded.get("revision") != manifest["revision"]:
            raise CatalogError(f"{asset_name} revision 不一致")


def write_outputs(
    output: Path,
    payloads: dict[str, bytes],
    manifest: dict[str, Any],
    allow_large_change: bool,
) -> None:
    previous_path = output / "catalog-manifest.json"
    previous = read_json(previous_path) if previous_path.exists() else None
    validate_count_change(previous, manifest, allow_large_change)
    output.mkdir(parents=True, exist_ok=True)
    for name, content in payloads.items():
        write_if_changed(output / name, content)
    manifest_bytes = canonical_json(manifest) + b"\n"
    write_if_changed(output / "catalog-manifest.json", manifest_bytes)
    header, source = generate_cpp(payloads, manifest)
    write_if_changed(GENERATED_HEADER, header)
    write_if_changed(GENERATED_SOURCE, source)


def check_outputs(output: Path, payloads: dict[str, bytes], manifest: dict[str, Any]) -> None:
    expected = {**payloads, "catalog-manifest.json": canonical_json(manifest) + b"\n"}
    header, source = generate_cpp(payloads, manifest)
    expected[str(GENERATED_HEADER)] = header
    expected[str(GENERATED_SOURCE)] = source
    failures = []
    for name, content in expected.items():
        path = Path(name) if name.startswith("/") else output / name
        if not path.exists() or path.read_bytes() != content:
            failures.append(str(path))
    if failures:
        raise CatalogError("生成資源不是最新版本：" + ", ".join(failures))


def check_committed_outputs(output: Path) -> tuple[dict[str, bytes], dict[str, Any]]:
    manifest_path = output / "catalog-manifest.json"
    if not manifest_path.is_file():
        raise CatalogError(f"缺少已提交目錄 manifest：{manifest_path}")
    manifest = read_json(manifest_path)
    if not isinstance(manifest, dict):
        raise CatalogError("已提交目錄 manifest 格式不正確")
    payloads: dict[str, bytes] = {}
    for name in ASSET_NAMES:
        path = output / name
        if not path.is_file():
            raise CatalogError(f"缺少已提交目錄資源：{path}")
        payloads[name] = path.read_bytes()
    validate_release(payloads, manifest)
    check_outputs(output, payloads, manifest)
    return payloads, manifest


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--refresh", action="store_true", help="download official sources")
    parser.add_argument(
        "--refresh-tfl",
        action="store_true",
        help="download only TfL bus and rail catalogue sources",
    )
    parser.add_argument("--check", action="store_true", help="verify checked-in outputs")
    parser.add_argument("--cache-dir", type=Path, default=DEFAULT_CACHE)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument(
        "--allow-large-change",
        action="store_true",
        help="confirm a reviewed route or stop count change above 10 percent",
    )
    return parser.parse_args(list(argv))


def main(argv: Iterable[str] = sys.argv[1:]) -> int:
    args = parse_args(argv)
    try:
        if args.check:
            payloads, manifest = check_committed_outputs(args.output_dir)
        else:
            if args.refresh:
                fetch_base_sources(args.cache_dir)
                fetch_ctb_catalog(
                    args.cache_dir,
                    read_json(args.cache_dir / "ctb-routes.json"),
                    args.workers,
                )
                fetch_gmb_catalog(args.cache_dir, args.workers)
                fetch_tfl_catalog(args.cache_dir, args.workers)
            elif args.refresh_tfl:
                fetch_tfl_catalog(args.cache_dir, args.workers)
            payloads, manifest = build_assets(args.cache_dir)
            validate_release(payloads, manifest)
            write_outputs(
                args.output_dir,
                payloads,
                manifest,
                args.allow_large_change,
            )
        total = sum(len(value) for value in payloads.values())
        print(
            f"catalog revision={manifest['revision']} assets={len(payloads)} "
            f"gzip_bytes={total}"
        )
        return 0
    except CatalogError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
