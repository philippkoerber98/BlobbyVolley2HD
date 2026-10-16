"""Validate a staged portable build and write its manifest and distributable ZIP."""

import argparse
from datetime import date
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
LANGUAGES = ("de", "en", "cs", "es", "fr", "it")


def preferred_asset_names(kind):
    """Mirror CMake's WebP preference without packaging two copies of an image."""
    extensions = {".png", ".webp", ".ttf", ".txt"} if kind == "gfx" else {".png", ".webp"}
    names = {
        path.relative_to(ROOT / "data").as_posix()
        for path in (ROOT / "data" / kind).iterdir()
        if path.is_file() and path.suffix.lower() in extensions
    }
    return {name for name in names if not (name.endswith(".png") and name[:-4] + ".webp" in names)}


def validate_artwork(package, kind):
    with ZipFile(package / "data" / f"{kind}.zip") as archive:
        names = {name for name in archive.namelist() if not name.endswith("/")}
        expected = preferred_asset_names(kind)
        if names != expected:
            raise ValueError(f"Unexpected {kind} assets: {sorted(names ^ expected)}")
        for name in names:
            if archive.read(name) != (ROOT / "data" / name).read_bytes():
                raise ValueError(f"Packaged asset differs from source: {name}")
    print(f"{kind}: {len(names)} HD-only files verified")


def validate_levels(package):
    expected = {p.relative_to(ROOT / "data/Levels") for p in (ROOT / "data/Levels").rglob("*") if p.is_file()}
    actual = {p.relative_to(package / "Levels") for p in (package / "Levels").rglob("*") if p.is_file()}
    if expected != actual:
        raise ValueError("Packaged Levels differ from source folders")
    for path in expected:
        if (package / "Levels" / path).read_bytes() != (ROOT / "data/Levels" / path).read_bytes():
            raise ValueError(f"Packaged level differs: {path}")
    if not (package / "Custom Levels/README.txt").is_file():
        raise ValueError("Missing Custom Levels instructions")
    if (package / "data/backgrounds.zip").exists():
        raise ValueError("Obsolete duplicated backgrounds archive")
    print(f"Level folders verified: {len(expected)} files")


def validate_translations(package):
    for row in ET.parse(package / "data" / "ui-translations.xml").getroot():
        key = row.get("english")
        if any(not row.get(language) for language in LANGUAGES):
            raise ValueError(f"Missing translation: {key}")
        if row.get("en") != key:
            raise ValueError(f"English translation differs from its key: {key}")
        if re.search(r"\b(Breitbild|Fenster|Spieler|Zurück|Optionen|Einstellungen|Spielfeld)\b", key):
            raise ValueError(f"German word in English translation: {key}")
    print("All six UI languages verified")


def copy_license_notices(package):
    destination = package / "licenses"
    destination.mkdir(exist_ok=True)
    for source, name in (("deps/tinyxml/LICENSE.txt", "tinyxml2.txt"), ("src/raknet/LICENSE", "raknet.txt")):
        (destination / name).write_bytes((ROOT / source).read_bytes())
    source = (ROOT / "src/third_party/stb_truetype.h").read_text(encoding="utf-8")
    start = source.rfind("ALTERNATIVE A - MIT License")
    end = source.rfind("*/")
    if start < 0 or end <= start:
        raise ValueError("Cannot locate stb_truetype license notice")
    (destination / "stb_truetype.txt").write_text(source[start:end], encoding="utf-8")


def write_manifest(package):
    files = []
    for path in sorted(package.rglob("*")):
        if path.is_file() and path.name != "package-manifest.json":
            files.append({
                "path": path.relative_to(package).as_posix(),
                "bytes": path.stat().st_size,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            })
    (package / "package-manifest.json").write_text(json.dumps(files, indent=2), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", type=Path, help="Staged package directory")
    parser.add_argument("--tag", default=date.today().isoformat(), help="Date or label for the ZIP filename")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9._-]+", args.tag):
        parser.error("Tag may contain only letters, digits, dots, underscores, and hyphens")
    if args.package:
        package = args.package.resolve()
    else:
        path_file = ROOT / "out/friends-package-path.txt"
        package = Path(path_file.read_text(encoding="utf-8-sig").strip()).resolve()
    if not (package / "blobby.exe").is_file():
        raise FileNotFoundError(f"Not a staged game package: {package}")

    validate_artwork(package, "gfx")
    validate_levels(package)
    validate_translations(package)
    copy_license_notices(package)
    write_manifest(package)
    archive = ROOT / "out/releases" / f"BlobbyVolley2HD-Windows-{args.tag}.zip"
    archive.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(archive, "w", ZIP_DEFLATED) as output:
        for path in sorted(package.rglob("*")):
            if path.is_file():
                output.write(path, Path("BlobbyVolley2HD") / path.relative_to(package))
    print(f"ZIP: {archive} ({archive.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
