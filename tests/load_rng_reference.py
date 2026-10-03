#!/usr/bin/env python3
"""Bounded original-method LOAD UID oracle in official Godot 3.6.2.

Upstream is read-only. Staged Item.get_uid swaps only randomize/randi for
recording wrappers. Wrappers seed the actual global engine stream from explicit
clock fixtures, mirror it with an engine RNG object, and verify every raw draw.
The actual timed built-ins are also exercised in an unmodified-method smoke test.
This is not a full game/boot execution or a cross-platform entropy comparison.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "upstream/MOTHER-Encore"
MASK = (1 << 64) - 1


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def method(text, name):
    match = re.search(r"^(?:static )?func " + re.escape(name) + r"\(.*?(?=^(?:static )?func |\Z)", text, re.M | re.S)
    if not match:
        raise ValueError("missing source method " + name)
    # Only the indented body belongs to the method, not intervening comments.
    lines = match.group().splitlines()
    while lines and (not lines[-1] or not lines[-1].startswith("\t")):
        lines.pop()
    return "\n".join(lines) + "\n"


PROBE = r'''extends Reference
const data = {"mirror": null, "samples": [], "cursor": 0, "trace": [],
    "rows": [], "row": 0, "index": 0, "failures": 0, "current": null}

static func setup(initial_seed, samples):
	seed(initial_seed)
	data.mirror = RandomNumberGenerator.new()
	data.mirror.seed = initial_seed
	data.samples = samples
	data.cursor = 0
	data.trace = []
	data.rows = []
	data.current = null
	data.failures = 0

static func begin_row(row, count):
	data.row = row
	data.index = 0
	data.rows.append([row, count])

static func next_load(samples):
	data.samples = samples
	data.cursor = 0
	data.trace = []
	data.rows = []

static func reseed():
	var sample = data.samples[data.cursor]
	data.cursor += 1
	var value = (int(sample[0]) + int(sample[1])) * data.mirror.state + 1442695040888963407
	seed(value)
	data.mirror.seed = value
	data.current = {"order_id": data.row, "item_index": data.index,
		"seed": str(value), "raw": [], "state_after": ""}
	data.trace.append(data.current)
	data.index += 1

static func draw():
	var actual = randi()
	var mirrored = data.mirror.randi()
	if actual != mirrored:
		data.failures += 1
	data.current.raw.append(actual)
	data.current.state_after = str(data.mirror.state)
	return actual
'''

DRIVER = r'''extends SceneTree
const Probe = preload("res://probe.gd")
const Item = preload("res://item.gd")
const Inventory = preload("res://inventory.gd")
const ActualItem = preload("res://actual_item.gd")
const ActualInventory = preload("res://actual_inventory.gd")
const PartyMember = preload("res://party_member.gd")
const PartyNPC = preload("res://party_npc.gd")
var globaldata = {"key_items": null, "storage": null, "characters": {}}
var cases = __CASES__
var results = []
var failures = 0

func require(value, message):
	if not value:
		printerr("FAIL: ", message)
		failures += 1

func source_inventory_load(save_data):
__LOAD_BLOCK__

func _init():
	# Unmodified original method: actual randomize/randi, present and absent uid.
	var actual = ActualInventory.new()
	ActualItem.used_uids_tab.clear()
	actual.init_from_serialized([{"item_name": "fixture", "uid": 71, "doses": 1}])
	require(actual._items[0].uid == 71, "present saved UID retained with timed built-ins")
	require(ActualItem.used_uids_tab.size() == 1, "present UID still allocates one fallback")
	actual.init_from_serialized([{"item_name": "fixture", "doses": 1}])
	require(ActualItem.used_uids_tab.size() == 2, "missing UID allocates one fallback")
	require(actual._items[0].uid == ActualItem.used_uids_tab[1], "missing UID uses fallback")
	ActualItem.new("fixture", false, 1, 99)
	require(ActualItem.used_uids_tab.size() == 2, "four constructor arguments skip default")
	ActualItem.new("fixture", false, 1)
	require(ActualItem.used_uids_tab.size() == 3, "omitted constructor UID allocates once")
	var timed = {"allocations": 3, "present_uid": 71, "four_argument_extra_allocations": 0}
	for c in cases:
		if c.get("reuse_previous", false):
			# A second original LOAD retains its static UID ledger and live stream.
			Probe.next_load(c.samples)
		else:
			Probe.setup(int(c.initial_seed), c.samples)
			Item.used_uids_tab.clear()
			for prior in c.ledger:
				Item.used_uids_tab.append(int(prior))
		if c.get("force_collision", false):
			var mirror = RandomNumberGenerator.new()
			var sample = c.samples[0]
			mirror.seed = (int(sample[0]) + int(sample[1])) * Probe.data.mirror.state + 1442695040888963407
			Item.used_uids_tab.append(mirror.randi())
			Item.used_uids_tab.append(mirror.randi())
		var initial_ledger = Item.used_uids_tab.duplicate()
		var initial_state = str(Probe.data.mirror.state)
		globaldata.key_items = Inventory.new()
		globaldata.key_items.order_id = 0
		globaldata.storage = Inventory.new()
		globaldata.storage.order_id = 1
		globaldata.characters = __REGISTRY__
		var order_id = 2
		for character in globaldata.characters.values():
			character.order_id = order_id
			order_id += 1
		# The original global.gd block iterates its registry, not save.party.
		source_inventory_load(c.save)
		var retained = []
		for item in globaldata.key_items._items:
			retained.append(item.uid)
		for item in globaldata.storage._items:
			retained.append(item.uid)
		for character in globaldata.characters.values():
			if character._inventory != null:
				for item in character._inventory._items:
					retained.append(item.uid)
		require(Probe.data.failures == 0, "all global draws match engine mirror")
		require(Probe.data.cursor == c.samples.size(), "one reseed per loaded item")
		var after_state = str(Probe.data.mirror.state)
		var sentinel = randi()
		require(sentinel == Probe.data.mirror.randi(), "continuation sentinel")
		results.append({"name": c.name, "initial_state": initial_state,
			"initial_ledger": initial_ledger, "rows": Probe.data.rows,
			"samples": c.samples, "trace": Probe.data.trace,
			"final_state": after_state, "final_ledger": Item.used_uids_tab.duplicate(),
			"retained_uids": retained, "sentinel": sentinel})
	var output = File.new()
	output.open("reference.json", File.WRITE)
	output.store_string(JSON.print({"cases": results, "timed_builtin_smoke": timed}, "  "))
	output.close()
	print("ENGINE=", Engine.get_version_info())
	print("REFERENCE_COMPLETE cases=", results.size(), " failures=", failures)
	quit(1 if failures else 0)
'''


def make_cases(registry):
    def item(uid=None):
        row = dict(item_name="fixture", equipped=False, doses=1)
        if uid is not None:
            row["uid"] = uid
        return row

    def case(name, seed, save, count, **extra):
        return dict(name=name, initial_seed=str(seed), save=save, ledger=[],
                    samples=[[str(1770000000 + n), str(1000 + n * 37)] for n in range(count)], **extra)

    # Source IDs below come from the checked source dictionary, not runtime code.
    source_initial = __import__("yaml").safe_load((SOURCE / "Data/save_new_game.yaml").read_text())
    projected = {"key_items": [], "storage": [], "party": source_initial["party"]}
    count = 0
    for label in ("key_items", "storage"):
        for _ in source_initial.get(label, []):
            count += 1
            projected[label].append(item(800 + count))
    for label, kind in registry:
        projected[label] = {"inventory": []}
        if kind == "PartyMember":
            for _ in source_initial.get(label, {}).get("inventory", []):
                count += 1
                projected[label]["inventory"].append(item(800 + count))
    mixed = {"key_items": [item(10), item()], "storage": [item(20), item(21)], "party": [registry[0][0]]}
    for n, (label, _) in enumerate(registry):
        # Deliberately present inventory on NPC rows: original NPC has no rebuild.
        mixed[label] = {"inventory": [item(30 + n)]}
    cases = [case("full_saved_native_projection", 123, projected, count),
             case("all_registered_order_including_inactive", 987, mixed, 9),
             case("missing_uid", 0, {"key_items": [item()], "party": []}, 1),
             case("empty_load", 55, {"party": []}, 0),
             case("two_collisions_before_saved_uid", 77, {"key_items": [item(456)], "party": []}, 1, force_collision=True)]
    cases.append(case("same_process_prior_ledger", 77, {"key_items": [item(456), item(457)], "party": []}, 2, force_collision=True))
    cases.append(case("same_process_second_load", 0, projected, count, reuse_previous=True))
    cases.append(case("u64_clock_wrap", -1, {"key_items": [item(998)], "party": []}, 1))
    cases[-1]["samples"] = [[str(-1), str(-2)]]
    return cases


def write_binary(path, cases):
    output = bytearray(b"ELRNG362")
    def put(fmt, *values):
        output.extend(struct.pack("<" + fmt, *values))
    def values(rows):
        put("I", len(rows))
        for value in rows:
            put("I", int(value))
    put("I", len(cases))
    for c in cases:
        name = c["name"].encode()
        put("I", len(name))
        output.extend(name)
        put("Q", int(c["initial_state"]) & MASK)
        values(c["initial_ledger"])
        put("I", len(c["rows"]))
        for row in c["rows"]:
            put("II", *row)
        put("I", len(c["samples"]))
        for sample in c["samples"]:
            put("QQ", *(int(v) & MASK for v in sample))
        put("I", len(c["trace"]))
        for trace in c["trace"]:
            put("IIIQQ", trace["order_id"], trace["item_index"], trace["raw"][-1],
                len(trace["raw"]), int(trace["seed"]) & MASK)
        put("Q", int(c["final_state"]) & MASK)
        values(c["final_ledger"])
        values(c["retained_uids"])
        put("I", c["sentinel"])
    path.write_bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--godot", required=True, type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / "reports/load-rng")
    args = parser.parse_args()
    godot = args.godot.resolve()
    version = subprocess.check_output([str(godot), "--version"], text=True).strip()
    if version != "3.6.2.stable.official.3cd3caab6":
        raise SystemExit("Unexpected Godot version: " + version)
    paths = {name: SOURCE / "Scripts/global" / (name + ".gd")
             for name in ("Item", "Inventory", "PartyMember", "PartyNPC", "Character", "global", "globalData")}
    source = {name: path.read_text() for name, path in paths.items()}
    provenance_paths = list(paths.values()) + [SOURCE / "Data/save_new_game.yaml", SOURCE / "Data/save_overrides.yaml"]
    before = {str(path.relative_to(ROOT)): digest(path) for path in provenance_paths}
    out = args.out.resolve()
    runtime = out / "runtime"
    runtime.mkdir(parents=True, exist_ok=True)
    registry_text = re.search(r"var characters := (\{.*?\n\})", source["globalData"], re.S).group(1)
    registry = []
    constants = {}
    for kind, token in re.findall(r"(PartyMember|PartyNPC)\.(\w+):", registry_text):
        value = re.search(r'^const ' + token + r' := "([^"]+)"', source[kind], re.M).group(1)
        constants.setdefault(kind, []).append(f'const {token} := "{value}"')
        registry.append((value, kind))
    item = 'extends Reference\nconst used_uids_tab := []\nvar item_name: String\nvar uid: int\nvar equipped: bool\nvar doses: int\n' + method(source["Item"], "_init") + '\nfunc get_data():\n\treturn {}\n\n' + method(source["Item"], "get_uid")
    (runtime / "actual_item.gd").write_text(item)
    instrumented = item.replace("\trandomize()", "\tProbe.reseed()").replace("= randi()", "= Probe.draw()")
    if instrumented.count("Probe.draw()") != 2 or instrumented.count("Probe.reseed()") != 1:
        raise ValueError("source get_uid shape changed")
    (runtime / "item.gd").write_text(instrumented.replace('extends Reference\n', 'extends Reference\nconst Probe = preload("res://probe.gd")\n', 1))
    inv = 'extends Reference\nconst Item = preload("res://actual_item.gd")\nvar _items := []\n' + method(source["Inventory"], "init_from_serialized")
    (runtime / "actual_inventory.gd").write_text(inv)
    inventory = 'extends Reference\nconst Item = preload("res://item.gd")\nconst Probe = preload("res://probe.gd")\nenum InvType {NORMAL, KEY, STORAGE, STORAGE_GOD}\nvar _type: int\nvar _items := []\nvar order_id := -1\n' + method(source["Inventory"], "_init") + '\nfunc _init_god_storage():\n\tassert(false) # Outside this bounded LOAD oracle.\n\n' + method(source["Inventory"], "init_from_serialized")
    # Character allocation happens in Inventory._init, before assigning order_id;
    # the PartyMember wrapper starts that row first. Key/storage use owner id.
    inventory = inventory.replace('func init_from_serialized(serialized_inv: Array):\n', 'func init_from_serialized(serialized_inv: Array):\n\tif order_id >= 0:\n\t\tProbe.begin_row(order_id, serialized_inv.size())\n')
    (runtime / "inventory.gd").write_text(inventory)
    first_line = method(source["PartyMember"], "init_from_dict").splitlines()[1]
    if first_line != '\t_inventory = Inventory.new(Inventory.InvType.NORMAL, dict.get("inventory", []))':
        raise ValueError("PartyMember inventory-first contract changed")
    member = 'extends Reference\nconst Inventory = preload("res://inventory.gd")\nconst Probe = preload("res://probe.gd")\n' + '\n'.join(constants["PartyMember"]) + '\nvar _inventory = null\nvar order_id := -1\nfunc init_from_dict(dict: Dictionary):\n\tProbe.begin_row(order_id, dict.get("inventory", []).size())\n' + first_line + '\n'
    (runtime / "party_member.gd").write_text(member)
    npc = 'extends Reference\nconst Probe = preload("res://probe.gd")\n' + '\n'.join(constants["PartyNPC"]) + '\nvar _inventory = null\nvar order_id := -1\nfunc init_from_dict(_dict: Dictionary):\n\tProbe.begin_row(order_id, 0) # PartyNPC/Character contain no inventory reconstruction.\n'
    if 'Inventory.' in method(source["PartyNPC"], "init_from_dict") or 'Inventory.' in method(source["Character"], "init_from_dict"):
        raise ValueError("NPC inventory contract changed")
    (runtime / "party_npc.gd").write_text(npc)
    start = source["global"].index('\tglobaldata.key_items.init_from_serialized(')
    end = source["global"].index('\n\t#globaldata.reset_constant_data()', start)
    load_block = source["global"][start:end]
    cases = make_cases(registry)
    driver = DRIVER.replace("__CASES__", json.dumps(cases)).replace("__LOAD_BLOCK__", load_block).replace("__REGISTRY__", registry_text)
    (runtime / "driver.gd").write_text(driver)
    (runtime / "probe.gd").write_text(PROBE)
    (runtime / "project.godot").write_text('config_version=4\n[application]\nconfig/name="LOAD UID bounded original-method probe"\n')
    env = dict(os.environ)
    for key, directory in (("XDG_DATA_HOME", "data"), ("XDG_CACHE_HOME", "cache"), ("XDG_CONFIG_HOME", "config")):
        env[key] = str(runtime / directory)
    command = [str(godot), "--path", str(runtime), "--no-window", "--script", "res://driver.gd"]
    try:
        result = subprocess.run(command, cwd=runtime, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=60)
    except subprocess.TimeoutExpired as failure:
        output = failure.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        (out / "engine-reference.log").write_text(output)
        raise SystemExit("Engine probe timed out; inspect " + str(out / "engine-reference.log")) from failure
    (out / "engine-reference.log").write_text(result.stdout)
    if result.returncode or "REFERENCE_COMPLETE" not in result.stdout or "failures=0" not in result.stdout:
        raise SystemExit("Engine probe failed; inspect " + str(out / "engine-reference.log"))
    report = json.loads((runtime / "reference.json").read_text())
    for fixture, result in zip(cases, report["cases"]):
        expected_saved = []
        for label, kind in [("key_items", "inventory"), ("storage", "inventory")] + registry:
            if kind == "PartyNPC":
                continue
            rows = fixture["save"].get(label, []) if kind == "inventory" else fixture["save"].get(label, {}).get("inventory", [])
            for row in rows:
                allocation = result["trace"][len(expected_saved)]
                expected_saved.append(row.get("uid", allocation["raw"][-1]))
        if result["retained_uids"] != expected_saved:
            raise ValueError("saved UID preservation or fallback selection mismatch")
        if len(result["final_ledger"]) != len(result["initial_ledger"]) + len(result["trace"]):
            raise ValueError("generated ledger append count mismatch")
        if fixture.get("force_collision") and len(result["trace"][0]["raw"]) != 3:
            raise ValueError("expected exact two collision retries")
    after = {str(path.relative_to(ROOT)): digest(path) for path in provenance_paths}
    if before != after:
        raise ValueError("readonly source changed during probe")
    report.update(engine=version, engine_sha256=digest(godot), command=command,
                  source_sha256=before, source_unchanged=True,
                  official_engine_source_base="https://github.com/godotengine/godot/tree/3.6.2-stable",
                  engine_source_sha256={str(path.relative_to(ROOT)): digest(path) for path in
                      sorted((ROOT / "reports/godot-rng/engine-3.6.2").rglob("*")) if path.is_file()},
                  registry=registry,
                  instrumentation="Exact Item _init/get_uid and Inventory _init/init_from_serialized; replace only randomize/randi calls in staged Item copy; exact global inventory-load block and character dictionary; PartyMember only inventory-first line, PartyNPC no-inventory stub. No scene, stats, full boot or platform-time parity claim.")
    (out / "reference.json").write_text(json.dumps(report, indent=2) + "\n")
    write_binary(out / "reference.bin", report["cases"])
    print(f"Official Godot {version}: {len(report['cases'])} cases, timed smoke and seeded draw sentinels passed")


if __name__ == "__main__":
    main()
