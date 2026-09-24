#!/bin/sh
# Runs the parser over every test image we have. Exits nonzero if any
# relocation fails to resolve.
set -e
M=${1:-/mnt/project-files/dos-material/games}
make >/dev/null
for f in "$M"/bioforge/cd/BIOFORGE/BIOFORGE.EXE \
	 "$M"/ultima8-pagan/u8.exe \
	 "$M"/crusader-no-remorse/CRUSADER.EXE \
	 "$M"/bioforge/cd/BIOFORGE/AILXMI.DLL \
	 "$M"/crusader-no-remorse/ASYLUM.DLL; do
	[ -f "$f" ] || { echo "skip $f"; continue; }
	./nedump "$f" > /dev/null || { echo "FAIL $f"; exit 1; }
	echo "ok   $f"
done
