#!/usr/bin/env python3
"""Offline and opt-in remote release gates for CapyBrowser.

The default validation path never contacts the network. The remote subcommand
is intentionally separate and verifies the exact tag, GitHub Release assets and
modules index selected by the publisher.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tarfile
from typing import Dict, Iterable, Mapping
from urllib.error import HTTPError, URLError
from urllib.parse import quote, urlparse
from urllib.request import Request, urlopen


MAX_PACKAGE_BYTES = 8 * 1024 * 1024
MAX_HTTP_BYTES = 16 * 1024 * 1024
DEFAULT_REPOSITORY = "henriquefarisco/CapyBrowser"

PACKAGES = {
    "org.capyos.browser.text": {
        "summary": "CapyBrowse Text portable browser-core (HTML-to-text)",
        "depends": "",
    },
    "org.capyos.browser.core": {
        "summary": "CapyBrowser portable static graphical browser core",
        "depends": "org.capyos.codecs.image-basic",
    },
}

REQUIRED_MANIFEST_KEYS = {
    "name",
    "version",
    "summary",
    "payload_url",
    "payload_sha256",
    "payload_size",
    "install_root",
    "depends",
}


class GateError(RuntimeError):
    """Expected release-gate failure with a user-facing message."""


def require(condition: bool, message: str) -> None:
    if not condition:
        raise GateError(message)


def read_utf8(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as exc:
        raise GateError(f"cannot read UTF-8 file {path}: {exc}") from exc


def current_version(root: Path) -> str:
    version = read_utf8(root / "VERSION").strip()
    require(
        bool(re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version)),
        f"VERSION is not MAJOR.MINOR.PATCH: {version!r}",
    )
    return version


def one_match(pattern: str, text: str, label: str) -> str:
    matches = re.findall(pattern, text, flags=re.MULTILINE)
    require(len(matches) == 1, f"expected exactly one {label}, found {len(matches)}")
    return matches[0]


def capyos_extended_version(version_yaml: Path) -> str:
    lines = read_utf8(version_yaml).splitlines()
    in_channels = False
    in_alpha = False
    for line in lines:
        if line == "channels:":
            in_channels = True
            continue
        if in_channels and line == "  alpha:":
            in_alpha = True
            continue
        if in_alpha and line.startswith("  ") and not line.startswith("    "):
            break
        if in_alpha and line.startswith("    extended:"):
            value = line.split(":", 1)[1].strip().strip("\"'")
            require(bool(value), f"empty channels.alpha.extended in {version_yaml}")
            return value
    raise GateError(f"cannot locate channels.alpha.extended in {version_yaml}")


def check_metadata(root: Path) -> tuple[str, str]:
    version = current_version(root)
    readme = read_utf8(root / "README.md")
    roadmap = read_utf8(root / "docs" / "roadmap.md")
    docs_readme = read_utf8(root / "docs" / "README.md")
    compatibility = read_utf8(root / "docs" / "compatibility.md")
    makefile = read_utf8(root / "Makefile")

    readme_version = one_match(
        r"^Version:\s*([^\s]+)\s*$", readme, "README Version field"
    )
    roadmap_version = one_match(
        r"^\*\*Versão atual:\*\*\s*`([^`]+)`",
        roadmap,
        "roadmap current-version field",
    )
    require(readme_version == version, f"README version {readme_version} != VERSION {version}")
    require(
        roadmap_version == version,
        f"roadmap version {roadmap_version} != VERSION {version}",
    )

    pins = {
        "docs/README.md": one_match(
            r"^Pinned for this release:\s*`([^`]+)`",
            docs_readme,
            "docs README CapyOS pin",
        ),
        "docs/compatibility.md": one_match(
            r"^- CapyOS core pinned for this contract:\s*`([^`]+)`",
            compatibility,
            "compatibility CapyOS pin",
        ),
        "docs/roadmap.md": one_match(
            r"^\*\*Pin CapyOS:\*\*\s*`([^`]+)`",
            roadmap,
            "roadmap CapyOS pin",
        ),
    }
    unique_pins = set(pins.values())
    require(
        len(unique_pins) == 1,
        "CapyOS pin drift: "
        + ", ".join(f"{path}={pin}" for path, pin in pins.items()),
    )
    capyos_pin = next(iter(unique_pins))

    sibling_version = root.parent / "CapyOS" / "VERSION.yaml"
    if sibling_version.is_file():
        sibling_pin = capyos_extended_version(sibling_version)
        require(
            capyos_pin == sibling_pin,
            f"CapyBrowser CapyOS pin {capyos_pin} != sibling current {sibling_pin}",
        )

    require(
        "CAPY_PKG_SUMMARY_core := CapyBrowser portable browser-core stub"
        not in makefile,
        "core package summary still says 'stub'",
    )
    print(f"[metadata] version={version} capyos={capyos_pin} ok")
    return version, capyos_pin


def parse_key_values(text: str, label: str) -> Dict[str, str]:
    values: Dict[str, str] = {}
    saw_end = False
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line == "---":
            require(not saw_end, f"{label}:{number}: duplicate terminator")
            saw_end = True
            continue
        require(not saw_end, f"{label}:{number}: data after terminator")
        require("=" in line, f"{label}:{number}: expected key=value")
        key, value = line.split("=", 1)
        require(bool(key), f"{label}:{number}: empty key")
        require(key not in values, f"{label}:{number}: duplicate key {key}")
        require(
            all(0x20 <= ord(ch) <= 0x7E for ch in value),
            f"{label}:{number}: non-printable/non-ASCII value",
        )
        values[key] = value
    require(saw_end, f"{label}: missing --- terminator")
    return values


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def validate_tar(payload: Path) -> None:
    try:
        with tarfile.open(payload, mode="r:") as archive:
            members = archive.getmembers()
    except (OSError, tarfile.TarError) as exc:
        raise GateError(f"invalid ustar payload {payload}: {exc}") from exc

    require(bool(members), f"empty package payload: {payload}")
    roots = set()
    for member in members:
        posix = PurePosixPath(member.name)
        require(
            not posix.is_absolute() and ".." not in posix.parts,
            f"unsafe tar path {member.name!r} in {payload.name}",
        )
        require(
            member.isfile() or member.isdir(),
            f"unsupported tar member type {member.name!r} in {payload.name}",
        )
        require(
            member.mtime == 0,
            f"non-deterministic mtime for {member.name!r} in {payload.name}",
        )
        require(
            member.uid == 0 and member.gid == 0,
            f"non-canonical uid/gid for {member.name!r} in {payload.name}",
        )
        if posix.parts:
            roots.add(posix.parts[0])
    require(
        {"src", "docs", "VERSION"}.issubset(roots),
        f"payload roots missing from {payload.name}: {sorted(roots)}",
    )


def validate_artifacts(
    directory: Path, version: str, repository: str
) -> Dict[str, bytes]:
    require(directory.is_dir(), f"artifact directory does not exist: {directory}")
    expected_files = set()
    blobs: Dict[str, bytes] = {}

    for package, contract in PACKAGES.items():
        payload_name = f"{package}-{version}.bin"
        manifest_name = f"{package}.manifest"
        expected_files.update((payload_name, manifest_name))
        payload_path = directory / payload_name
        manifest_path = directory / manifest_name
        require(payload_path.is_file(), f"missing package payload {payload_path}")
        require(manifest_path.is_file(), f"missing package manifest {manifest_path}")

        payload = payload_path.read_bytes()
        manifest_blob = manifest_path.read_bytes()
        require(
            len(payload) <= MAX_PACKAGE_BYTES,
            f"{payload_name} exceeds {MAX_PACKAGE_BYTES} bytes",
        )
        validate_tar(payload_path)

        try:
            manifest_text = manifest_blob.decode("ascii")
        except UnicodeDecodeError as exc:
            raise GateError(f"manifest is not ASCII: {manifest_path}") from exc
        manifest = parse_key_values(manifest_text, str(manifest_path))
        missing = REQUIRED_MANIFEST_KEYS - manifest.keys()
        require(not missing, f"{manifest_name} missing keys: {sorted(missing)}")

        expected_url = (
            f"https://github.com/{repository}/releases/download/v{version}/"
            f"{payload_name}"
        )
        expected = {
            "name": package,
            "version": version,
            "summary": str(contract["summary"]),
            "payload_url": expected_url,
            "payload_sha256": sha256_bytes(payload),
            "payload_size": str(len(payload)),
            "install_root": f"/var/capypkg/{package}",
            "depends": str(contract["depends"]),
        }
        for key, wanted in expected.items():
            require(
                manifest.get(key) == wanted,
                f"{manifest_name}: {key}={manifest.get(key)!r}, want {wanted!r}",
            )
        require(
            bool(re.fullmatch(r"[0-9a-f]{64}", manifest["payload_sha256"])),
            f"{manifest_name}: payload_sha256 is not lowercase SHA-256",
        )

        blobs[payload_name] = payload
        blobs[manifest_name] = manifest_blob

    actual_files = {path.name for path in directory.iterdir() if path.is_file()}
    require(
        actual_files == expected_files,
        f"unexpected artifact set in {directory}: "
        f"missing={sorted(expected_files - actual_files)} "
        f"extra={sorted(actual_files - expected_files)}",
    )
    return blobs


def check_offline(
    root: Path, first_dir: Path, second_dir: Path, repository: str
) -> Dict[str, bytes]:
    version, _ = check_metadata(root)
    first = validate_artifacts(first_dir, version, repository)
    second = validate_artifacts(second_dir, version, repository)
    require(first.keys() == second.keys(), "reproducibility artifact sets differ")
    for name in sorted(first):
        require(first[name] == second[name], f"non-reproducible artifact: {name}")
    payloads = sorted(name for name in first if name.endswith(".bin"))
    digest = sha256_bytes(first[payloads[0]])
    print(f"[release-offline] {len(first)} artifacts reproducible; sha256={digest}")
    return first


def request_headers(api: bool = False) -> Dict[str, str]:
    headers = {
        "User-Agent": "CapyBrowser-release-gate/1",
        "Accept": "application/vnd.github+json"
        if api
        else "application/octet-stream",
    }
    token = os.environ.get("GITHUB_TOKEN") or os.environ.get("GH_TOKEN")
    if token and api:
        headers["Authorization"] = f"Bearer {token}"
        headers["X-GitHub-Api-Version"] = "2022-11-28"
    return headers


def http_get(url: str, *, api: bool = False) -> bytes:
    parsed = urlparse(url)
    require(parsed.scheme == "https", f"remote URL must be HTTPS: {url}")
    request = Request(url, headers=request_headers(api))
    try:
        with urlopen(request, timeout=30) as response:
            final_url = response.geturl()
            require(
                urlparse(final_url).scheme == "https",
                f"remote redirected to non-HTTPS URL: {final_url}",
            )
            data = response.read(MAX_HTTP_BYTES + 1)
    except HTTPError as exc:
        detail = exc.read(512).decode("utf-8", "replace").strip()
        raise GateError(f"HTTP {exc.code} resolving {url}: {detail}") from exc
    except (URLError, OSError) as exc:
        raise GateError(f"cannot resolve {url}: {exc}") from exc
    require(
        len(data) <= MAX_HTTP_BYTES,
        f"remote response exceeds {MAX_HTTP_BYTES} bytes: {url}",
    )
    return data


def http_json(url: str) -> Mapping[str, object]:
    data = http_get(url, api=True)
    try:
        parsed = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise GateError(f"invalid JSON from {url}: {exc}") from exc
    require(isinstance(parsed, dict), f"expected JSON object from {url}")
    return parsed


def parse_index(data: bytes, label: str) -> Dict[str, Dict[str, str]]:
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise GateError(f"modules index is not UTF-8: {label}") from exc
    entries: Dict[str, Dict[str, str]] = {}
    block: list[str] = []
    for raw in text.splitlines():
        if raw.strip() == "---":
            parsed = parse_key_values("\n".join(block + ["---"]), label)
            name = parsed.get("name", "")
            require(bool(name), f"{label}: index entry has no name")
            require(name not in entries, f"{label}: duplicate package {name}")
            entries[name] = parsed
            block = []
        else:
            block.append(raw)
    require(
        not any(
            line.strip() and not line.strip().startswith("#") for line in block
        ),
        f"{label}: unterminated final index entry",
    )
    return entries


def check_remote(
    root: Path,
    artifacts_dir: Path,
    repository: str,
    tag: str,
    index_url: str,
) -> None:
    version, _ = check_metadata(root)
    require(tag == f"v{version}", f"remote tag {tag} != v{version}")
    local = validate_artifacts(artifacts_dir, version, repository)

    api_base = f"https://api.github.com/repos/{repository}"
    http_json(f"{api_base}/git/ref/tags/{quote(tag, safe='')}")
    release = http_json(f"{api_base}/releases/tags/{quote(tag, safe='')}")
    require(
        release.get("tag_name") == tag,
        f"GitHub Release tag_name={release.get('tag_name')!r}, want {tag!r}",
    )
    raw_assets = release.get("assets")
    require(isinstance(raw_assets, list), "GitHub Release assets is not a list")
    assets: Dict[str, Mapping[str, object]] = {}
    for asset in raw_assets:
        if isinstance(asset, dict) and isinstance(asset.get("name"), str):
            assets[str(asset["name"])] = asset

    missing_assets = set(local) - assets.keys()
    require(
        not missing_assets,
        f"GitHub Release {tag} missing assets: {sorted(missing_assets)}",
    )
    for name, expected_blob in sorted(local.items()):
        asset = assets[name]
        require(
            asset.get("size") == len(expected_blob),
            f"remote asset size mismatch for {name}: "
            f"{asset.get('size')} != {len(expected_blob)}",
        )
        download_url = asset.get("browser_download_url")
        require(
            isinstance(download_url, str) and bool(download_url),
            f"remote asset has no download URL: {name}",
        )
        expected_url = (
            f"https://github.com/{repository}/releases/download/{tag}/{name}"
        )
        require(
            download_url == expected_url,
            f"unexpected download URL for {name}: {download_url}",
        )
        remote_blob = http_get(download_url)
        require(
            remote_blob == expected_blob,
            f"remote asset bytes differ from local canonical artifact: {name}",
        )

    index_data = http_get(index_url)
    index = parse_index(index_data, index_url)
    for package, contract in PACKAGES.items():
        require(package in index, f"modules index missing package {package}")
        entry = index[package]
        payload_name = f"{package}-{version}.bin"
        payload = local[payload_name]
        expected = {
            "version": version,
            "official": "1",
            "payload_url": (
                f"https://github.com/{repository}/releases/download/{tag}/"
                f"{payload_name}"
            ),
            "payload_sha256": sha256_bytes(payload),
            "payload_size": str(len(payload)),
            "install_root": f"/var/capypkg/{package}",
            "depends": str(contract["depends"]),
        }
        for key, wanted in expected.items():
            require(
                entry.get(key) == wanted,
                f"index {package}: {key}={entry.get(key)!r}, want {wanted!r}",
            )

    print(f"[release-remote] tag={tag} assets={len(local)} index=ok")


def path_from_root(root: Path, value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else root / path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    metadata = subparsers.add_parser("metadata", help="check local version/docs pins")
    metadata.add_argument("--root", default=".")

    offline = subparsers.add_parser("offline", help="check local release artifacts")
    offline.add_argument("--root", default=".")
    offline.add_argument("--first-dir", required=True)
    offline.add_argument("--second-dir", required=True)
    offline.add_argument("--repository", default=DEFAULT_REPOSITORY)

    remote = subparsers.add_parser(
        "remote", help="verify published tag/assets/index"
    )
    remote.add_argument("--root", default=".")
    remote.add_argument("--artifacts-dir", required=True)
    remote.add_argument("--repository", default=DEFAULT_REPOSITORY)
    remote.add_argument("--tag", required=True)
    remote.add_argument("--index-url", required=True)
    return parser


def main(argv: Iterable[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    root = Path(args.root).resolve()
    try:
        if args.command == "metadata":
            check_metadata(root)
        elif args.command == "offline":
            check_offline(
                root,
                path_from_root(root, args.first_dir),
                path_from_root(root, args.second_dir),
                args.repository,
            )
        elif args.command == "remote":
            check_remote(
                root,
                path_from_root(root, args.artifacts_dir),
                args.repository,
                args.tag,
                args.index_url,
            )
        else:  # argparse makes this unreachable.
            raise GateError(f"unknown command: {args.command}")
    except (GateError, OSError) as exc:
        print(f"[release-gate] FAIL: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
