"""Checked build-only binary writer shared by the two dropped-item oracles."""
import struct
from pathlib import Path
from tools.extract_battle_entry import require


def write_outputs(root, out, outputs, marker_name, marker_value):
    requested = Path(out)
    require(not requested.is_symlink(), 'Dropped fixture symlink')
    out = requested.resolve()
    build = Path(root).resolve() / 'build'
    require(out != build and out.is_relative_to(build), 'Dropped owned fixture location')
    require(type(marker_name) is str and marker_name.startswith('.owned-') and
            not any(c in marker_name for c in '/\\:\0') and type(marker_value) is bytes
            and 0 < len(marker_value) <= 256, 'Dropped fixture marker schema')
    require(type(outputs) is dict and bool(outputs) and all(type(k) is str and
            Path(k).name == k and not any(c in k for c in '/\\:\0') and
            k not in ['', '.', '..', marker_name] and type(v) is bytes
            for k, v in outputs.items()), 'Dropped fixture output schema')
    marker = out / marker_name
    require(not out.exists() or out.is_dir() and marker.is_file() and not marker.is_symlink()
            and marker.read_bytes() == marker_value, 'Dropped fixture ownership')
    if out.exists():
        require({p.name for p in out.iterdir()} <= set(outputs) | {marker_name}, 'Dropped unowned fixture entry')
    for name, raw in outputs.items():
        path = out / name
        require(not path.is_symlink() and (not path.exists() or path.is_file() and path.read_bytes() == raw),
                'Dropped fixture cache changed')
    out.mkdir(parents=True, exist_ok=True)
    if not marker.exists():
        marker.write_bytes(marker_value)
    for name, raw in outputs.items():
        path = out / name
        if not path.exists():
            path.write_bytes(raw)
    return out


class Writer:
    def __init__(self, magic):
        require(type(magic) is bytes and len(magic) == 8, 'Dropped fixture magic')
        self.data = bytearray(magic)

    def word(self, value):
        require(type(value) is int and 0 <= value <= 0xffffffff, 'Dropped fixture word')
        self.data.extend(struct.pack('<I', value))

    def signed(self, value):
        require(type(value) is int and -(1 << 31) <= value < (1 << 31), 'Dropped fixture signed integer')
        self.data.extend(struct.pack('<i', value))

    def text(self, value):
        require(type(value) is str and len(value.encode()) <= 4096 and '\0' not in value, 'Dropped fixture text')
        raw = value.encode()
        self.word(len(raw))
        self.data.extend(raw)

    def row(self, row, flag, model):
        self.signed(row['frame'])
        for key in ['idle', 'delta']:
            self.word(row[key])
        self.word(int(row['alive']))
        self.word(int(flag in row['flags']))
        self.word(int(row['flags'].get(flag, False)))
        self.word(row['ledger_size'])
        self.word(len(row['dialogues']))
        for path in row['dialogues']:
            self.text(path)
        self.word(len(row['party']))
        for inventory in row['party']:
            self.word(len(inventory))
        self.word(len(row['keys']))
        item = row['global_item']
        self.word(int(item is not None))
        if item is not None:
            self.text(item['name'])
            self.word(item['doses'])
            self.word(int(item['equipped']))
            require(item['uid'].startswith('allocated:'), 'Dropped global item must be a generated source identity')
            self.word(int(item['uid'].split(':')[1]))
        self.word(int(row['prompt'] is not None and row['prompt']['visible']))
        if row['alive']:
            state = row['state']
            for name in ['position', 'scale']:
                for bits in state[name]:
                    self.word(bits)
            for name in ['visible', 'sprite_visible']:
                self.word(int(state[name]))
            self.word(state['rotation'])
            self.word(state['sparkle'])
            self.word(int(state['timer_paused']))
            self.word(state['timer_left'])
            self.word(int(state['timer_stopped']))
            self.word(0 if not state['animation'] else next(c['role'] for c in model['clips'] if c['name'] == state['animation']))
            self.word(state['animation_speed'])
            self.word(0 if state['animation_time'] is None else state['animation_time'])
            self.word(int(state['queued']))
