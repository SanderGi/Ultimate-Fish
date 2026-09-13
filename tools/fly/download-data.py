"""Fetch official source tables; prepare-connectome.py verifies pinned hashes."""
from pathlib import Path
from urllib.request import urlretrieve
import sys
root=Path(sys.argv[1]);root.mkdir(parents=True,exist_ok=True)
base='https://storage.googleapis.com/flyem-male-cns/v1.0/connectome-data/flat-connectome/'
for short,name in [('annotations','body-annotations-male-cns-v1.0-minconf-0.5'),('neurotransmitters','body-neurotransmitters-male-cns-v1.0'),('weights','connectome-weights-male-cns-v1.0-minconf-0.5')]:
 target=root/(short+'.feather')
 if not target.exists():
  # Cache mounts survive failed builds: never leave a partial file looking ready.
  partial=target.with_suffix('.feather.part')
  try:
   urlretrieve(base+name+'.feather',partial)
   partial.replace(target)
  finally:partial.unlink(missing_ok=True)
 print(target,flush=True)
