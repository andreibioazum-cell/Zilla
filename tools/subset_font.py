#!/usr/bin/env python3
"""Rebuilds the shipped TTFs from the woff2 files in the Godot sources.

Godot ships its interface font as woff2 (Inter_Regular.woff2 /
Inter_Bold.woff2 in thirdparty/fonts). stb_truetype needs plain TrueType, so we
convert and subset the fonts down to Latin + Cyrillic + the few symbols the
interface uses: 330 KB per font becomes ~17 KB.

Usage:
    pip install fonttools brotli
    python3 tools/subset_font.py thirdparty/fonts/Inter_Regular.woff2 \
                                thirdparty/fonts/Inter_Bold.woff2 assets/fonts

Or straight from a Godot checkout:
    python3 tools/subset_font.py /path/to/godot/thirdparty/fonts/*.woff2 assets/fonts
"""

import pathlib
import subprocess
import sys

# Everything the interface can print: ASCII, Cyrillic (including Ё/ё) and the
# symbols used by the HUD and the panels.
CHARACTERS = "".join(
    [chr(c) for c in range(0x20, 0x7F)]
    + [chr(c) for c in range(0x400, 0x460)]
    + list(" ·–—‘’“„”•…«»№×→▪▫")
)


def subset(source: pathlib.Path, target_dir: pathlib.Path) -> None:
    characters_file = target_dir / ".charset.txt"
    characters_file.write_text(CHARACTERS, encoding="utf-8")

    target = target_dir / (source.stem + ".ttf")
    subprocess.run(
        [
            sys.executable,
            "-m",
            "fontTools.subset",
            str(source),
            f"--output-file={target}",
            f"--text-file={characters_file}",
            "--layout-features=",
            "--no-hinting",
            "--drop-tables+=DSIG",
            "--name-IDs=*",
            "--obfuscate-names",
        ],
        check=True,
    )
    characters_file.unlink(missing_ok=True)
    print(f"wrote {target} ({target.stat().st_size} bytes)")


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 1
    target_dir = pathlib.Path(argv[-1])
    target_dir.mkdir(parents=True, exist_ok=True)
    for name in argv[1:-1]:
        subset(pathlib.Path(name), target_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
