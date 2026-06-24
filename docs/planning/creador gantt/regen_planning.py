#!/usr/bin/env python3
"""Read the existing planning xlsx, write a fresh copy to avoid Excel cache issues."""
import openpyxl, os, glob

PLANNING_DIR = os.path.dirname(os.path.abspath(__file__))

# Find the existing planning file
pattern = os.path.join(PLANNING_DIR, "Planif*")
matches = glob.glob(pattern)
if not matches:
    raise RuntimeError("No planning file found")

src = matches[0]
basename = os.path.basename(src)

# Read it
wb = openpyxl.load_workbook(src)
ws = wb.active

# Delete old
os.remove(src)

# Save fresh copy with same name
wb.save(src)
print(f"OK: {os.path.getsize(src)} bytes, {ws.max_row} rows")
