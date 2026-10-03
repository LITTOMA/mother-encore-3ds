#!/usr/bin/env bash
set -euo pipefail
report_dir="reports/m0-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$report_dir"
make test 2>&1 | tee "$report_dir/host-tests.txt"
make sanitize 2>&1 | tee "$report_dir/sanitizer-tests.txt"
make 3dsx 2>&1 | tee "$report_dir/3dsx-build.txt"
make cia 2>&1 | tee "$report_dir/cia-build.txt"
tools/bin/ctrtool --verify --showsyscalls --listromfs dist/encore-native.cia > "$report_dir/cia-inspection.txt" 2>&1
inspection_dir="build/ctr/inspection-$(date -u +%Y%m%dT%H%M%SZ)"
tools/bin/ctrtool --romfsdir="$inspection_dir/romfs" dist/encore-native.cia > "$report_dir/cia-extraction.txt" 2>&1
cmp romfs/sandbox.encpak "$inspection_dir/romfs/sandbox.encpak"
python3 tools/record_ctr_build.py
cp reports/ctr-toolchain-lock.json "$report_dir/toolchain-lock.json"
python3 tools/release.py
echo "Build/test logs: $report_dir"
echo 'Review cia-inspection.txt: retail signatures for Ticket/TMD/AccessDescriptor fail for this homebrew test package.'
echo 'No emulator or hardware run is implied by this script.'
