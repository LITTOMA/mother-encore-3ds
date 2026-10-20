"""Prepare an offline programme candidate; never write release resources."""
import argparse, copy, json, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import field_prop_dialogues as d
from tools.extract_battle_entry import require
from tools import native_content as nc

a = argparse.ArgumentParser()
a.add_argument('--out', type=Path, required=True)
p = a.parse_args()
out = p.out.resolve()
require(out != ROOT and out != ROOT / 'romfs' and not out.is_relative_to(ROOT / 'romfs'), 'Candidate must not replace release resources')
require(not out.is_relative_to(ROOT) or out.is_relative_to(ROOT / 'build') or out.is_relative_to(ROOT / 'dist'), 'Candidate cannot replace source files')
policy, _ = d.read()
blob, programmes, dependencies = d.candidate((ROOT / policy['base_fixture']).read_bytes())
out.mkdir(parents=True, exist_ok=True)
(out / 'podunk.encroom').write_bytes(blob)
(out / 'story.encstory').write_bytes(d.story_candidate((ROOT / policy['story_fixture']).read_bytes()))
for kind in ('actor', 'side-effect'):
    ir = copy.deepcopy(nc.parse_pack(blob))
    first = ir['sections']['Program'][-2]['first_command']
    if kind == 'actor':
        ir['sections']['Command'][first + 4]['actor_index'] = 0
    else:
        ir['sections']['Command'][first + 4].update(opcode=nc.OPCODES.index('SetFlag'), target_index=0, value=1)
    ir.pop('build_input_sha256')
    ir['provenance'] = {'sources': dependencies}
    raw, _ = nc.compile_ir(ir)
    (out / (kind + '.encroom')).write_bytes(raw)
(out / 'source.json').write_text(json.dumps(dict(programmes=programmes, dependencies=dependencies, scope='Offline actorless programmes only; no live A/ray/selection admission'), indent=2) + '\n', encoding='utf8')
