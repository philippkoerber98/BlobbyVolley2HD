"""Version and package a Windows CI build without rewriting the repository."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+-build\.[1-9][0-9]*\.[1-9][0-9]*")


def build_version(cmake, run, attempt):
    match = re.search(r"project\(blobby VERSION ([0-9]+\.[0-9]+\.[0-9]+)\)", cmake)
    if not match or run < 1 or attempt < 1:
        raise ValueError("A CMake base version and positive run/attempt are required")
    if run > 65535:
        raise ValueError("Build number exceeds the Windows resource limit; bump the base version and reset the counter")
    return f"{match[1]}-build.{run}.{attempt}"


def package_build(package, version):
    if not VERSION.fullmatch(version):
        raise ValueError("Invalid CI version")
    package = package.resolve()
    if not (package / "blobby.exe").is_file():
        raise ValueError("Not a portable build directory")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    metadata = {"version": version, "commit": commit, "platform": "windows-x64",
                "workflow_run": os.environ.get("GITHUB_RUN_ID"),
                "workflow_url": (f"{os.environ.get('GITHUB_SERVER_URL', 'https://github.com')}/"
                                 f"{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/"
                                 f"{os.environ.get('GITHUB_RUN_ID', '')}")}
    (package / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    (package / "VERSION.txt").write_text(version + "\n", encoding="utf-8")
    subprocess.run([sys.executable, str(ROOT / "tools/finalize_friends_package.py"),
                    "--package", str(package), "--tag", version], cwd=ROOT, check=True)
    output = ROOT / "out/publish"
    output.mkdir(parents=True, exist_ok=True)
    binary = ROOT / "out/releases" / f"BlobbyVolley2HD-Windows-{version}.zip"
    destination = output / binary.name
    shutil.copy2(binary, destination)
    source = output / f"BlobbyVolley2HD-Source-{version}.zip"
    subprocess.run(["git", "archive", "--format=zip", f"--output={source}",
                    f"--prefix=BlobbyVolley2HD-Source-{version}/", commit], cwd=ROOT, check=True)
    checksums = "".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n"
                        for p in (destination, source))
    (output / "SHA256SUMS.txt").write_text(checksums, encoding="ascii")
    print(f"Validated build: {version} ({commit})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    version = commands.add_parser("version")
    version.add_argument("--run", type=int, required=True)
    version.add_argument("--attempt", type=int, required=True)
    package = commands.add_parser("package")
    package.add_argument("--package", type=Path, required=True)
    package.add_argument("--version", required=True)
    args = parser.parse_args()
    if args.command == "version":
        value = build_version((ROOT / "CMakeLists.txt").read_text(encoding="utf-8"),
                              args.run, args.attempt)
        if os.environ.get("GITHUB_OUTPUT"):
            with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as stream:
                stream.write(f"version={value}\n")
        print(value)
    else:
        package_build(args.package, args.version)


if __name__ == "__main__":
    main()
