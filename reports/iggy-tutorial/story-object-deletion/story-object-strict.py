from pathlib import Path
import subprocess
r=Path('/mnt/d/GameDev/3DS/mother-encore-3ds')
flags=['g++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fno-exceptions','-fno-rtti','-Iinclude','-fsyntax-only']
with(r/'build/full-port/story-object-strict-result.log').open('wb')as f:
 for n in ['runtime/field_objects.cpp','runtime/room_data.cpp','runtime/dialogue.cpp','runtime/world.cpp']:
  f.write((n+'\n').encode());f.flush();subprocess.run(flags+[n],cwd=r,stdout=f,stderr=subprocess.STDOUT,check=True)
print('Four modified shared-core execution/codec units strict GCC syntax checks passed')
