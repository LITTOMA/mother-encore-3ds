"""Build exact original seven-coffin/item lifecycle comparison fixtures."""
import hashlib
import json
import struct
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import dropped_items as d
from tools.dropped_breakable_protocol import validate, semantic_digest
from tests.dropped_reference_fixture import Writer


def prepare(out):
    out = Path(out).resolve()
    d.require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'Fresh composed dropped fixture directory')
    model, proof = d.build_model()
    native = config = None
    digests = []
    sources = d.f.Sources()
    for index in [1, 2]:
        base = ROOT / ('reports/dropped-items/composed-original-' + str(index))
        receipt = json.loads((base / 'receipt.json').read_bytes())
        inputs = json.loads((base / 'inputs.json').read_bytes())
        for name, sha in [('reference.json', receipt['result_sha256']), ('producer.py', receipt['producer_sha256']),
                          ('runner.py', receipt['runner_sha256']), ('protocol.py', receipt['protocol_sha256']),
                          ('source-adapter.py', inputs['producer_sha256']), ('cover-adapter.py', inputs['cover_adapter_sha256']),
                          ('config.json', receipt['generated_sha256']['input.json'])]:
            d.require(hashlib.sha256((base / name).read_bytes()).hexdigest() == sha,
                      'Dropped composition retained original fingerprint')
        for name, sha in receipt['copied_sha256'].items():
            d.require(hashlib.sha256(sources.data(name)).hexdigest() == sha,
                      'Dropped composition executed original source changed')
        control = json.loads((base / 'config.json').read_bytes())
        document = validate(json.loads((base / 'reference.json').read_bytes()), control)
        digests.append(semantic_digest(document))
        if native is None:
            native, config = document, control
        else:
            d.require(config == control, 'Dropped composition original configuration mismatch')
    d.require(digests[0] == digests[1], 'Dropped composition original outcomes differ')
    d.require(config['bindings'] == model['bindings'] and config['templates'] ==
              {str(v['id']): v for v in model['templates']}, 'Dropped composition source bindings changed')
    _, breaks, _ = d.b.source()
    by_path = {v['path']: v for v in breaks['owners']}
    for cover in config['covers']:
        d.require(all(cover[key] == value for key, value in by_path[cover['path']].items()),
                  'Dropped composition original cover binding changed')
    out.mkdir(parents=True)
    (out / 'source.encdrp').write_bytes(d.encode(model))
    (out / 'breakables.encbrk').write_bytes(d.b.encode(breaks))
    w = Writer(b'ENCDRPB1')
    test_offsets = {}
    w.word(len(config['cases']))
    for case, observed in zip(config['cases'], native['cases']):
        owner = config['bindings'][case['binding']]
        cover = config['covers'][case['cover']]
        w.text(case['name'])
        for value in [owner['id'], cover['id'], config['seed'], observed['sparkle_rng_tail']]:
            w.word(value)
        w.data.extend(struct.pack('<f', case['distance']))
        w.signed(case['action'])
        w.signed(case['repeat'])
        w.word(int(case['flag']))
        w.word(len(case['fill']))
        for count in case['fill']:
            w.word(count)
        w.text(config['filler'])
        w.data.extend(struct.pack('<2f', 13, 27))  # Explicit original observer player provider.
        w.word(len(observed['rows']))
        for row in observed['rows']:
            test_offsets.setdefault('first_alive', len(w.data) + 12)
            w.row(row, owner['flag'], model)
            if row['alive']:
                state = row['state']
                w.text(state['parent'])
                for key in ['physics', 'body_disabled', 'area_disabled']:
                    w.word(int(state[key]))
            cover_state = row['cover']
            w.text(cover_state['parent'])
            for bits in cover_state['position']:
                w.word(bits)
            for key in ['visible', 'sprite_visible']:
                w.word(int(cover_state[key]))
            w.word(cover_state['frame_index'])
            test_offsets.setdefault('first_cover_animation', len(w.data))
            w.word(['', 'Break', 'Breakable', 'RESET'].index(cover_state['animation']))
            w.word(int(cover_state['playing']))
            w.word(0 if cover_state['time'] is None else cover_state['time'])
            w.word(int(cover_state['body_disabled']))
            for key in ['body_layer', 'hit_layer', 'hit_mask']:
                w.word(cover_state[key])
            w.word(cover_state['timer_left'])
            w.word(int(cover_state['timer_stopped']))
            w.word(len(cover_state['audio']))
            for frame, value in cover_state['audio']:
                w.word(frame)
                w.word(int(value))
            w.word(len(cover_state['rumble']))
            for frame, device, weak, strong, duration in cover_state['rumble']:
                w.word(frame)
                w.word(device)
                w.data.extend(struct.pack('<3d', weak, strong, duration))
    (out / 'native.bin').write_bytes(w.data)
    (out / 'source.json').write_text(json.dumps(dict(source=proof, semantic_sha256=digests[0],
        copied_sources=sources.digests(), fixture_producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        shared_writer_sha256=hashlib.sha256((ROOT / 'tests/dropped_reference_fixture.py').read_bytes()).hexdigest(),
        scope=__doc__, test_offsets=test_offsets, bytes=len(w.data), sha256=hashlib.sha256(w.data).hexdigest()), indent=2) + '\n')
    return out


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.out))
