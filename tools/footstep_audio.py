"""Source run-loop media appended to the existing checked application bank."""
from pathlib import Path
from tools.field_footsteps import source

def bindings(root):
    root=Path(root);_,metadata,_=source(root/'content/mayor-footsteps.json',root=root)
    return [dict(source=s['source'][6:],identity=dict(kind='stable',value=s['id']),pcm=s['pcm'],gain_db=0,conversion=None)for s in metadata['sounds']]
