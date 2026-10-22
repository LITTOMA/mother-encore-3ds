"""Build-only original dropped-item resources and CPU-reference fixture.

Prompt visibility is supplied from the independently executed original widget.
The shared actor comparison does not implement that widget or automatic input.
"""
import copy
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools import dropped_items as d
from tools.dropped_item_protocol import validate, semantic_digest


def prepare(out):
    d.require(not Path(out).is_symlink(), 'Dropped fixture symlink')
    out = Path(out).resolve()
    d.require(out.is_relative_to(ROOT / 'build') and out != ROOT / 'build', 'Dropped fixture directory')
    _, model, proof = d.source()
    report = ROOT / 'reports/dropped-items/original-1'
    native = validate(json.loads((report / 'reference.json').read_bytes()),
                      config := json.loads((report / 'config.json').read_bytes()))
    second = ROOT / 'reports/dropped-items/original-2'
    other = validate(json.loads((second / 'reference.json').read_bytes()),
                     json.loads((second / 'config.json').read_bytes()))
    d.require(semantic_digest(native) == semantic_digest(other), 'Dropped two-run original disagreement')
    d.require(config['bindings'] == model['bindings'] and config['templates'] ==
              {str(v['id']): v for v in model['templates']}, 'Dropped source bindings differ from actual original runs')
    sources = d.f.Sources()
    for base in [report, second]:
        receipt = json.loads((base / 'receipt.json').read_bytes())
        inputs = json.loads((base / 'inputs.json').read_bytes())
        for file, key in [('producer.py', 'producer_sha256'), ('protocol.py', 'protocol_sha256')]:
            d.require(hashlib.sha256((base / file).read_bytes()).hexdigest() == receipt[key],
                      'Dropped original producer/protocol fingerprint')
        d.require(hashlib.sha256((base / 'source-adapter.py').read_bytes()).hexdigest() == inputs['producer_sha256']
                  and hashlib.sha256((base / 'reference.json').read_bytes()).hexdigest() == receipt['result_sha256'],
                  'Dropped original adapter/raw result fingerprint')
        d.require(hashlib.sha256((base / 'config.json').read_bytes()).hexdigest() == receipt['generated_sha256']['input.json'],
                  'Dropped actual original input configuration changed')
        for name, digest in receipt['copied_sha256'].items():
            d.require(hashlib.sha256(sources.data(name)).hexdigest() == digest, 'Dropped original executed file changed')
    key_document = sources.extractor.yaml('Data/Items/DogTreats.yaml')
    key_template = dict(id=d.b.stable('dropped-template:DogTreats'), source='DogTreats',
                        yaml='Data/Items/DogTreats.yaml', document=key_document,
                        texture='Graphics/Objects/Items/DogTreats.png',
                        size=sources.extractor.png_size('Graphics/Objects/Items/DogTreats.png'),
                        doses=key_document.get('doses', 1), key_item=key_document['keyitem'])
    outputs = {}
    outputs['source.encdrp'] = d.encode(model)
    partial = copy.deepcopy(model)
    partial['bindings'] = partial['bindings'][:-1]
    outputs['partial.encdrp'] = d.encode(partial)
    outputs['breakables.encbrk'] = d.b.encode(d.b.source()[1])
    from tests.dropped_reference_fixture import Writer, write_outputs
    writer = Writer(b'ENCDRPR1')
    trace = writer.data
    word, signed, text = writer.word, writer.signed, writer.text

    word(len(config['cases']))
    for index, (case, observed) in enumerate(zip(config['cases'], native['cases'])):
        d.require(case['mode'] in ['collect', 'disappear'] and case['empty_dialogue'] == '',
                  'Dropped fixture unmapped operation/empty-dialogue override')
        variant = copy.deepcopy(model)
        variant['default_bits'] = int(case['pickup']) | (int(case['reset']) << 1)
        owner = variant['bindings'][case['binding']]
        if 'item' in case:
            d.require(case['item'] == key_template['source'], 'Dropped fixture unsupported item override')
            variant['templates'].append(copy.deepcopy(key_template))
            owner['template'] = key_template['id']
        name = 'case-' + str(index) + '.encdrp'
        outputs[name] = d.encode(variant)
        if case['name'] == 'key-bypass':
            outputs['key-control.encdrp'] = outputs[name]
        text(case['name'])
        text(name)
        word(owner['id'])
        word(config['seed'])
        word(observed['sparkle_rng_tail'])
        word(1 if case['mode'] == 'collect' else 2)
        for key in ['action', 'repeat', 'pause', 'unpause', 'move_player']:
            signed(case.get(key, -100))
        word(int(case['flag']))
        word(len(case['fill']))
        for count in case['fill']:
            word(count)
        text(config['filler'])
        # These are explicit observer-provided player poses, not game content.
        # Their exact generated source is retained with the native receipts.
        trace.extend(struct.pack('<4f', 13, 27, 99, 101))
        word(len(observed['rows']))
        for row in observed['rows']:
            writer.row(row, owner['flag'], model)
    outputs['native.bin'] = bytes(trace)
    outputs['source.json'] = (json.dumps(dict(source=proof, original_semantic_sha256=semantic_digest(native),
        copied_sources=sources.digests(), fixture_producer_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        shared_writer_sha256=hashlib.sha256((ROOT / 'tests/dropped_reference_fixture.py').read_bytes()).hexdigest(),
        scope=__doc__, trace_bytes=len(trace), trace_sha256=hashlib.sha256(trace).hexdigest()), indent=2) + '\n').encode()
    return write_outputs(ROOT, out, outputs, '.owned-dropped-items-fixture', b'dropped-items-fixture-v1')


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(prepare(args.out))
