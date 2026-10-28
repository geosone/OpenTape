"""Compare actual VGA attributes against the surrounding QUILIB screen."""
from pathlib import Path
import sys

root = Path(sys.argv[1])

def attributes(label, needles):
    raw = (root / f"ui-{label}.bin").read_bytes()
    text = bytes(c if 32 <= c <= 126 else 32 for c in raw[::2])
    for needle in needles:
        encoded = needle.encode("ascii")
        start = text.find(encoded)
        if needle.isdecimal():
            # The input label also contains the maximum address. Compare the
            # separate numeric value row, not the normal-colored range label.
            starts = [row + text[row:row + 80].find(encoded)
                      for row in range(0, len(text), 80)
                      if text[row:row + 54].strip() == encoded]
            start = starts[-1] if starts else -1
        if start >= 0:
            colors = set(raw[2 * start + 1:2 * (start + len(needle)):2])
            assert len(colors) == 1, (label, needle, colors)
            return colors.pop()
    raise AssertionError((label, needles))

assert attributes("start", ["Devices and tape status", "Geraete und Bandstatus"]) == attributes(
    "panel", ["Native tape controls", "Native Bandsteuerung"])
assert attributes("start", ["Selected: NRMT0", "Ausgewaehlt: NRMT0"]) == attributes(
    "target-position", ["18446744073709551615"])
assert attributes("start", ["FOUND != ready", "FOUND != bereit"]) == attributes(
    "target-position", ["FOUND != ready", "FOUND != bereit"])
print("Native controls preserve QUILIB data/highlight/help colors")
