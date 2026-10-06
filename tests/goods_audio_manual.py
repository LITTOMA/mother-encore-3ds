"""Manual-only source audio admission cases; never invoked by development CI."""
import copy
from pathlib import Path
from unittest.mock import patch


def manual_goods_audio_cases(root):
    from tools import goods_audio
    root = Path(root)
    expected = goods_audio.read_json(root / 'content/goods-audio-binding.json')
    rows = goods_audio.bindings(root)
    assert len(rows) == 2 and {r['source'] for r in rows} == {
        'Audio/Sound effects/M3/menu_open.wav', 'Audio/Sound effects/M3/menu_close.wav'}
    variants = []
    missing = copy.deepcopy(expected); missing['assets'].pop(); variants.append(missing)
    duplicate = copy.deepcopy(expected)
    duplicate['assets'][1]['identity'] = duplicate['assets'][0]['identity'].copy()
    variants.append(duplicate)
    substituted = copy.deepcopy(expected)
    substituted['assets'][1]['source'] = 'Audio/Sound effects/EB/close.wav'
    variants.append(substituted)
    unknown = copy.deepcopy(expected); unknown['assets'][0]['conversion'] = {}
    variants.append(unknown)
    unknown = copy.deepcopy(expected); unknown['assets'][0]['extra'] = True
    variants.append(unknown)
    for bad in variants:
        with patch.object(goods_audio, 'read_json', return_value=bad):
            try:
                goods_audio.bindings(root)
            except (ValueError, KeyError, OSError):
                continue
            raise AssertionError('unreviewed Goods sound binding accepted')
