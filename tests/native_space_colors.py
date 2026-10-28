"""Signed SPACE controls inherit established QUILIB attributes."""
from pathlib import Path
import sys
root = Path(sys.argv[1])
def color(label, needle, numeric=False):
    raw = (root / f"ui-{label}.bin").read_bytes()
    text = bytes(c if 32 <= c <= 126 else 32 for c in raw[::2])
    if isinstance(needle, str):
        needle = [needle]
    for value in needle:
        token = value.encode("ascii")
        for row in range(0, len(text), 80):
            line = text[row:row+80]
            pos = line.find(token)
            if pos < 0 or (numeric and line[:pos].strip()):
                continue
            colors = set(raw[2*(row+pos)+1:2*(row+pos+len(token)):2])
            assert len(colors) == 1, (label, value, colors)
            return colors.pop()
    raise AssertionError((label, needle))
assert color("start", ["Devices and tape status", "Geraete und Bandstatus"]) == color(
    "minimum-confirm", ["Native tape controls", "Native Bandsteuerung"])
assert color("start", ["Selected: NRMT0", "Ausgewaehlt: NRMT0"]) == color(
    "minimum-confirm", "-8388608", numeric=True)
assert color("start", ["FOUND != ready", "FOUND != bereit"]) == color(
    "minimum-confirm", ["S Space tape", "S Band bewegen"])
print("SPACE controls preserve QUILIB data/highlight/help colors")
