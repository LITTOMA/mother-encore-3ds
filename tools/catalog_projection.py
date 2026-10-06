"""Typed resource-binding provenance, independent of unrelated catalog entries."""
import hashlib
import json
from pathlib import Path


def require(value, message):
    if not value:
        raise ValueError(message)


def binding_projection(root, path, identities):
    from tools.resource_catalog import load_ir, validate
    require(type(path) is str and path == 'content/native-resource-catalog.json',
            'Unknown catalog projection source')
    require(type(identities) is list and 0 < len(identities) <= 128 and
            all(type(i) is int and 0 < i <= 0xffffffff for i in identities) and
            identities == sorted(set(identities)), 'Invalid projected binding identities')
    source = Path(root) / path
    require(source.stat().st_size <= 2 * 1024 * 1024, 'Catalog projection source exceeds authoring limit')
    catalog = validate(load_ir(source))
    rows = []
    for identity in identities:
        candidates = [r for r in catalog['bindings'] if r['id'] == identity]
        require(len(candidates) == 1, 'Missing projected catalog binding')
        rows.append(candidates[0])
    # Pin, schema and selected rows are semantic inputs. Catalog order, scope
    # prose and unrelated bindings are not inputs to this particular consumer.
    value = dict(schema=catalog['schema'], kind=catalog['kind'],
                 commit=catalog['commit'], bindings=rows)
    raw = json.dumps(value, sort_keys=True, separators=(',', ':'),
                     ensure_ascii=False, allow_nan=False).encode('utf-8')
    return catalog, dict(kind='catalog-bindings-v1', path=path,
                         identities=identities, sha256=hashlib.sha256(raw).hexdigest())


def verify_projection(root, record):
    require(type(record) is dict and set(record) ==
            {'kind', 'path', 'identities', 'sha256'} and
            record['kind'] == 'catalog-bindings-v1', 'Unknown provenance projection')
    _, actual = binding_projection(root, record['path'], record['identities'])
    require(record == actual, 'Changed reviewed catalog bindings: ' + record['path'])
