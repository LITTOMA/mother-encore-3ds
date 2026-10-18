"""Read test metadata from the verified artifact's immutable Git source.

The game SHA is checked by the caller against the downloaded CI receipt. Each
metadata blob must occur exactly once in that genuine game's embedded RomFS.
This permits independent checkout work without borrowing its mutable data.
"""
import re,subprocess
from pathlib import PurePosixPath
from tools.ci_console_check import embedded_romfs

def checked_resources(game_blob,source,paths,root):
    if not isinstance(source,str)or not re.fullmatch('[0-9a-f]{40}',source):raise ValueError('Exact artifact source commit required')
    if not isinstance(game_blob,bytes)or not 44<=len(game_blob)<=512*1024*1024:raise ValueError('Bounded artifact bytes required')
    if not isinstance(paths,list)or not 1<=len(paths)<=64:raise ValueError('Bounded source resource list required')
    romfs=embedded_romfs(game_blob);result={}
    for relative in paths:
        if not isinstance(relative,str)or not relative.startswith('romfs/')or '\\'in relative or ':'in relative or PurePosixPath(relative).is_absolute()or any(part in ('','.','..')for part in relative.split('/'))or relative in result:raise ValueError('Canonical unique source resource required')
        expected=subprocess.check_output(['git','show',source+':'+relative],cwd=root)
        if not expected or len(expected)>4*1024*1024 or romfs.count(expected)!=1:raise ValueError('Artifact resource differs from exact source: '+relative)
        result[relative]=expected
    if not result:raise ValueError('Artifact test metadata is empty')
    return result
