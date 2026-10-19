"""Fail closed when a build checkout lacks its pinned third-party gitlinks."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = ("vendor/opencc", "vendor/cpp-pinyin")


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def main() -> int:
    entries = {}
    for line in git("ls-files", "--stage", *EXPECTED).splitlines():
        metadata, path = line.split("\t", 1)
        mode, object_id, _stage = metadata.split()
        entries[path] = (mode, object_id)

    missing = [path for path in EXPECTED if entries.get(path, ("", ""))[0] != "160000"]
    if missing:
        raise SystemExit("Missing pinned vendor gitlinks: " + ", ".join(missing))

    failures = []
    for path in EXPECTED:
        expected_commit = entries[path][1]
        directory = ROOT / path
        if not (directory / "CMakeLists.txt").is_file():
            failures.append(path)
            continue
        actual_commit = git("-C", path, "rev-parse", "HEAD")
        if actual_commit != expected_commit:
            failures.append(path)
    if failures:
        raise SystemExit("Vendor checkout missing or differs from its pinned commit: " + ", ".join(failures))
    print("Pinned OpenCC and cpp-pinyin checkouts are present.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
