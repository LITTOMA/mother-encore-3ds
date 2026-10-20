"""Run the unchanged original generator with explicit UTF-8/LF text output.

Windows Path.write_text otherwise changes LF to CRLF before the generator
hashes dependent IR. This wrapper changes text IO only, never document values.
"""
from pathlib import Path
import sys
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from tools import link_phone_content
original=Path.write_text
def canonical(path,data,encoding=None,errors=None,newline=None):
    return original(path,data,encoding=encoding or 'utf8',errors=errors,newline='\n')
sys.argv=['tools/link_phone_content.py']
with patch.object(Path,'write_text',canonical):
    raise SystemExit(link_phone_content.main())
