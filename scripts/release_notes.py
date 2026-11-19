#!/usr/bin/env python3
"""Notes for one version tag.

Prefer the matching CHANGELOG section. Otherwise use the tag message:
an annotated tag's message, or the tagged commit message for a lightweight tag.
The sequencer screenshot stays at the bottom. GitHub still appends the compare link.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

CHANGELOG_NAMES = (
    "CHANGELOG.md",
    "CHANGELOG",
    "changelog.md",
    "docs/CHANGELOG.md",
)

HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
VERSION = re.compile(r"v?(\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?)\Z", re.IGNORECASE)
TRAILER = re.compile(
    r"^(?:Co-authored-by|Signed-off-by|Acked-by|Reviewed-by|Tested-by|Helped-by):\s+\S",
    re.IGNORECASE,
)


def main(argv: list[str] | None = None) -> int:
    args = list(sys.argv[1:] if argv is None else argv)
    if len(args) != 1 or not args[0].strip():
        print("usage: release_notes.py <tag>", file=sys.stderr)
        return 2
    sys.stdout.write(compose(args[0].strip(), Path.cwd()))
    return 0


def compose(tag: str, root: Path) -> str:
    notes = changelog_notes(root, tag) or tag_message(tag)
    repo = os.environ.get("GITHUB_REPOSITORY", "alanmarcero/openFruit")
    image = (
        "![openFruit sequencer]"
        f"(https://raw.githubusercontent.com/{repo}/{tag}/docs/screenshots/preview.png)"
    )
    parts: list[str] = []
    cleaned = notes.strip()
    if cleaned:
        parts.append(cleaned)
    if "docs/screenshots/preview.png" not in cleaned:
        parts.append(image)
    return "\n\n".join(parts) + "\n"


def changelog_notes(root: Path, tag: str) -> str | None:
    for name in CHANGELOG_NAMES:
        path = root / name
        if not path.is_file():
            continue
        notes = section_for(path.read_text(encoding="utf-8-sig"), tag)
        if notes:
            return notes
    return None


def section_for(text: str, tag: str) -> str | None:
    lines = text.splitlines()
    start = None
    level = None
    for index, line in enumerate(lines):
        heading = HEADING.match(line)
        if heading is None or not same_version(heading.group(2), tag):
            continue
        start = index + 1
        level = len(heading.group(1))
        break
    if start is None or level is None:
        return None
    body: list[str] = []
    for line in lines[start:]:
        heading = HEADING.match(line)
        if heading is not None and len(heading.group(1)) <= level:
            break
        body.append(line)
    notes = "\n".join(body).strip()
    return notes or None


def same_version(heading: str, tag: str) -> bool:
    found = version_of(heading)
    wanted = version_of(tag)
    if found and wanted:
        return found == wanted
    return heading.strip().lower() == tag.strip().lower()


def version_of(text: str) -> str | None:
    cleaned = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", text)
    cleaned = cleaned.replace("[", "").replace("]", "")
    cleaned = re.sub(r"^(?:version|release)\s+", "", cleaned.strip(), flags=re.IGNORECASE)
    token = cleaned.split()[0].rstrip(":") if cleaned.split() else ""
    match = VERSION.fullmatch(token)
    return match.group(1) if match else None


def tag_message(tag: str) -> str:
    kind = git("cat-file", "-t", tag).strip()
    if kind == "tag":
        message = strip_trailers(git("for-each-ref", f"refs/tags/{tag}", "--format=%(contents)"))
        if message:
            return message
    return strip_trailers(git("log", "-1", "--format=%B", tag))


def strip_trailers(message: str) -> str:
    lines = message.splitlines()
    end = len(lines)
    while end > 0 and lines[end - 1].strip() == "":
        end -= 1
    trailer_end = end
    while end > 0 and TRAILER.match(lines[end - 1].strip()):
        end -= 1
    if end == trailer_end:
        return "\n".join(lines).strip()
    if end > 0 and lines[end - 1].strip() == "":
        end -= 1
    return "\n".join(lines[:end]).strip()


def git(*args: str) -> str:
    result = subprocess.run(
        ["git", *args],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip()
        print(f"git {' '.join(args)} failed: {detail}", file=sys.stderr)
        raise SystemExit(1)
    return result.stdout


if __name__ == "__main__":
    raise SystemExit(main())
