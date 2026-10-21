"""Suite-scoped reuse of pure parsing, keyed by the complete source text.

Source reads, SHA checks, recipe validation and lowering still execute normally.
Every caller receives a detached tree. Invalid input is parsed and rejected by
the original parser; failures are not cached. No production cache is installed.
"""
import copy
from contextlib import contextmanager
from functools import lru_cache
from unittest.mock import patch
from tools import godot_text


@contextmanager
def cached_source_parser(minimum_bytes=32768):
    original = godot_text.parse
    cached = lru_cache(maxsize=16)(original)

    def detached(text):
        # Tiny unique resource snippets would evict the full maps whose
        # numeric arrays dominate parsing. They still use the real parser.
        if len(text) < minimum_bytes:
            return original(text)
        return copy.deepcopy(cached(text))

    try:
        with patch.object(godot_text, 'parse', new=detached):
            yield cached.cache_info
    finally:
        cached.cache_clear()
