#!/usr/bin/env python3
"""Record actual official Godot 3.6.2 global RNG calls without decimal JSON loss.

This does not run the complete game. It records global built-ins, object-state
mirrors and sentinel calls in a tiny isolated project. First-round semantics are
audited separately, and the original-method battle harness supplies that proof.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess


ROOT = Path(__file__).resolve().parents[1]
MASK = (1 << 64) - 1


def signed(value):
    value &= MASK
    return value if value < (1 << 63) else value - (1 << 64)


def seed_for_state(state):
    # Invert the two standard PCG seeding steps solely to test edge states in
    # the actual engine. This does not generate any expected random results.
    inc = (1442695040888963407 << 1) | 1
    return signed(((state - inc) * pow(6364136223846793005, -1, 1 << 64) - inc) & MASK)


GDSCRIPT = r'''extends SceneTree
var mirror := RandomNumberGenerator.new()
var output := File.new()
var cases = __CASES__
var failures := 0
var raw_count := 0
var current_raw := []

func raw_advance(count):
	for _i in range(count):
		current_raw.append(mirror.randi())
		raw_count += 1

func record(op):
	current_raw = []
	var kind = op[0]
	var low = float(op[1])
	var high = float(op[2])
	var result
	if kind == 0:
		result = randi()
		var mirrored = mirror.randi()
		current_raw.append(mirrored)
		raw_count += 1
		if result != mirrored:
			printerr("GLOBAL/OBJECT STATE MISMATCH: ", result, " != ", mirrored)
			failures += 1
	elif kind == 1:
		result = randf()
		raw_advance(1)
	else:
		result = rand_range(low, high)
		var exponent = mirror.randi()
		current_raw.append(exponent)
		raw_count += 1
		if exponent != 0:
			raw_advance(2)
	output.store_32(kind)
	output.store_double(low)
	output.store_double(high)
	if kind == 0:
		output.store_64(result)
	else:
		output.store_double(result)
	output.store_64(mirror.state)
	output.store_64(raw_count)
	output.store_32(current_raw.size())
	for i in range(3):
		output.store_32(current_raw[i] if i < current_raw.size() else 0)

func _init():
	if output.open("reference.bin", File.WRITE) != OK:
		quit(2)
		return
	output.store_buffer("ERNG362\n".to_ascii())
	output.store_32(cases.size())
	for c in cases:
		var s = int(c.seed)
		seed(s)
		mirror.seed = s
		raw_count = 0
		output.store_64(s)
		output.store_64(mirror.state)
		output.store_32(c.ops.size())
		for op in c.ops:
			record(op)
	output.close()
	# Direct engine evidence that object randf is a different API.
	var object_report = File.new()
	object_report.open("object-reference.bin", File.WRITE)
	for s in [0, 1, 123, -1]:
		mirror.seed = s
		object_report.store_64(s)
		object_report.store_64(mirror.state)
		object_report.store_double(mirror.randf())
		object_report.store_64(mirror.state)
		object_report.store_double(mirror.randf_range(-4.0, 4.0))
		object_report.store_64(mirror.state)
	object_report.close()
	var numeric_report = File.new()
	numeric_report.open("variant-reference.bin", File.WRITE)
	var typed_float: float = 16777217.0
	numeric_report.store_double(typed_float)
	numeric_report.store_double(0.1 + 0.2)
	seed(123)
	numeric_report.store_double(randf())
	numeric_report.close()
	print("ENGINE=", Engine.get_version_info())
	print("REFERENCE_COMPLETE cases=", cases.size(), " sentinel_failures=", failures)
	quit(1 if failures else 0)
'''


def make_cases():
    # PCG state chosen so XSH-RR yields UINT32_MAX with rotation zero.
    wanted = 0xffffffff << 27
    maximum_state = wanted ^ (wanted >> 18) ^ (wanted >> 36) ^ (wanted >> 54)
    seeds = [0, 1, 123, -1, (1 << 63) - 1, -(1 << 63),
             seed_for_state(0), seed_for_state(maximum_state)]
    cases = []
    for s in seeds:
        for mode, op in (("randi", [0, 0, 0]), ("randf", [1, 0, 0]),
                         ("range_unit", [2, 0, 1]), ("range_signed", [2, -4, 4])):
            ops = []
            for _ in range(32):
                ops.append(op)
                if mode != "randi":
                    # Every float operation is immediately checked against the
                    # engine object's subsequent randi: exact stream position.
                    ops.append([0, 0, 0])
            cases.append({"name": f"{mode}/seed={s}", "seed": str(s), "ops": ops})
        cases.append({"name": f"mixed_bounds/seed={s}", "seed": str(s), "ops": [
            [2, -3.75, 9.125], [0, 0, 0], [2, 5, -5], [0, 0, 0],
            [2, 2, 2], [0, 0, 0], [1, 0, 0], [0, 0, 0],
            [2, -1e30, 1e-20], [0, 0, 0], [2, 1e-200, 1e200], [0, 0, 0]]})
    return cases


def decode_reference(data, cases):
    if data[:8] != b"ERNG362\n":
        raise ValueError("invalid reference header")
    offset = 8
    def take(fmt):
        nonlocal offset
        size = struct.calcsize(fmt)
        value = struct.unpack_from(fmt, data, offset)
        offset += size
        return value
    count, = take("<I")
    if count != len(cases):
        raise ValueError("incorrect case count")
    rows = []
    for case in cases:
        seed, state, n = take("<QQI")
        if seed != int(case["seed"]) & MASK or n != len(case["ops"]):
            raise ValueError("incorrect case identity")
        ops = []
        for _ in range(n):
            kind, low, high, result_bits, after, draws, raw_n, raw_a, raw_b, raw_c = take("<IddQQQI3I")
            value = result_bits if kind == 0 else struct.unpack("<d", struct.pack("<Q", result_bits))[0]
            ops.append({"kind": ["randi", "randf", "rand_range"][kind], "from": low,
                        "to": high, "value": value, "value_bits_hex": f"{result_bits:016x}",
                        "state_after": str(after), "raw_draws_since_seed": draws,
                        "raw_outputs": [raw_a, raw_b, raw_c][:raw_n]})
        rows.append({"name": case["name"], "seed_u64": str(seed),
                     "seeded_state": str(state), "operations": ops})
    if offset != len(data):
        raise ValueError("unexpected reference tail")
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--godot", required=True, type=Path)
    p.add_argument("--out", type=Path, default=ROOT / "reports/battle-round-random")
    args = p.parse_args()
    godot = args.godot.resolve()
    version = subprocess.check_output([str(godot), "--version"], text=True).strip()
    if version != "3.6.2.stable.official.3cd3caab6":
        raise SystemExit(f"expected official Godot3.6.2, received {version}")
    out = args.out.resolve()
    runtime = out / "runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    (runtime / "project.godot").write_text('config_version=4\n[application]\nconfig/name="Battle RNG reference"\n')
    cases = make_cases()
    script = GDSCRIPT.replace("__CASES__", json.dumps(cases, separators=(",", ":")))
    (runtime / "reference.gd").write_text(script)
    env = dict(os.environ)
    for key, directory in (("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache"), ("XDG_CONFIG_HOME", "config")):
        env[key] = str(runtime / directory)
    cmd = [str(godot), "--path", str(runtime), "--no-window", "--script", "res://reference.gd"]
    result = subprocess.run(cmd, cwd=runtime, env=env, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=60)
    (out / "engine-reference.log").write_text(result.stdout)
    if result.returncode or "REFERENCE_COMPLETE" not in result.stdout or "sentinel_failures=0" not in result.stdout:
        raise SystemExit(f"engine reference failed; inspect {out / 'engine-reference.log'}")
    data = (runtime / "reference.bin").read_bytes()
    (out / "reference.bin").write_bytes(data)
    decoded = decode_reference(data, cases)
    engine_sources = ROOT / "reports/godot-rng/engine-3.6.2"
    source_hashes = {str(f.relative_to(ROOT)): hashlib.sha256(f.read_bytes()).hexdigest()
                     for f in sorted(engine_sources.rglob("*")) if f.is_file()}
    report = {"engine": version, "engine_sha256": hashlib.sha256(godot.read_bytes()).hexdigest(),
              "command": cmd, "reference_sha256": hashlib.sha256(data).hexdigest(),
              "scope": "actual global built-ins; state from independent engine RandomNumberGenerator mirror, checked by global randi sentinels after every float call",
              "not_claimed": "complete game execution, production seed continuation, hardware verification",
              "official_source_base": "https://github.com/godotengine/godot/tree/3.6.2-stable",
              "engine_source_sha256": source_hashes, "cases": decoded}
    (out / "reference.json").write_text(json.dumps(report, indent=2) + "\n")
    obj = (runtime / "object-reference.bin").read_bytes()
    objects = []
    for start in range(0, len(obj), 48):
        seed, state, value, after, ranged, range_after = struct.unpack_from("<QQdQdQ", obj, start)
        objects.append({"seed_u64": str(seed), "seeded_state": str(state),
                        "object_randf": value, "state_after_object_randf": str(after),
                        "object_randf_range": ranged, "state_after_object_range": str(range_after)})
    (out / "object-api-difference.json").write_text(json.dumps(objects, indent=2) + "\n")
    numeric = (runtime / "variant-reference.bin").read_bytes()
    values = struct.unpack("<3d", numeric)
    numerics = {name: {"value": value, "binary64_hex": struct.pack(">d", value).hex()}
                for name, value in zip(("typed_gdscript_float_16777217", "variant_0_1_plus_0_2", "global_randf_seed123"), values)}
    (out / "variant-numeric-precision.json").write_text(json.dumps(numerics, indent=2) + "\n")
    print(f"Recorded {len(cases)} official-engine cases / {sum(len(c['ops']) for c in cases)} operations; all sentinels match")


if __name__ == "__main__":
    main()
