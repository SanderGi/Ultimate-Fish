"""Build full traced MaleCNS graph (CC BY 4.0). No edge threshold or circuit sampling.
Requires numpy and pyarrow. Usage: python prepare-connectome.py RAW_DIR OUTPUT_DIR
"""
import hashlib,json,sys
from pathlib import Path
import numpy as np
import pyarrow as pa
import pyarrow.feather as feather
raw=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
expected={'annotations':'2177e246113e4cfbf1e7772ec37c6da1955ff22e8063d0b1f833101f99a9a3b2','weights':'e35da783d1c686b2b58b3b87cd6a403ae43bfcfba8bff28e08ef752c1a56afc1','neurotransmitters':'95c9289220663abeb3409f3ad9e5a7f8a53f8093f5139d15502cd08da8879621'}
for name,digest in expected.items():
 h=hashlib.sha256()
 with (raw/(name+'.feather')).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 if h.hexdigest()!=digest:raise ValueError('Source checksum: '+name)
# Read only used annotation columns, and stream the 152M-row edge table. Loading
# all decompressed edges plus NumPy copies can exceed an 8 GiB Docker VM.
rows=feather.read_table(raw/'annotations.feather',columns=['bodyId','status','superclass','somaLocation']).to_pylist()
ann={r['bodyId']:r for r in rows if r['status']=='Traced'}
del rows
traced_ids=np.array(sorted(ann),dtype=np.int64)
nt={}
with pa.memory_map(str(raw/'neurotransmitters.feather'),'r') as source:
 reader=pa.ipc.open_file(source)
 for batch_index in range(reader.num_record_batches):
  batch=reader.get_batch(batch_index)
  bodies=batch.column(batch.schema.get_field_index('body')).to_numpy()
  indices=np.searchsorted(traced_ids,bodies)
  keep=(indices<len(traced_ids)) & (traced_ids[np.minimum(indices,len(traced_ids)-1)]==bodies)
  selected=batch.select(['body','consensus_nt']).filter(pa.array(keep))
  nt.update((r['body'],r['consensus_nt']) for r in selected.to_pylist())
parts=[]; synaptic_contacts=0
with pa.memory_map(str(raw/'weights.feather'),'r') as source:
 reader=pa.ipc.open_file(source)
 for batch_index in range(reader.num_record_batches):
  batch=reader.get_batch(batch_index)
  pre=batch.column(batch.schema.get_field_index('body_pre')).to_numpy()
  post=batch.column(batch.schema.get_field_index('body_post')).to_numpy()
  weight=batch.column(batch.schema.get_field_index('weight')).to_numpy()
  a=np.searchsorted(traced_ids,pre);b=np.searchsorted(traced_ids,post)
  keep=(a<len(traced_ids)) & (b<len(traced_ids))
  keep &= (traced_ids[np.minimum(a,len(traced_ids)-1)]==pre)
  keep &= (traced_ids[np.minimum(b,len(traced_ids)-1)]==post)
  if np.any(keep):
   synaptic_contacts+=int(weight[keep].sum())
   parts.append((a[keep].astype('<u4'),b[keep].astype('<u4'),weight[keep].astype('<f4')))
  if batch_index % 500 == 0:print('edge batch',batch_index,'/',reader.num_record_batches,flush=True)
a=np.concatenate([p[0] for p in parts]);b=np.concatenate([p[1] for p in parts]);w=np.concatenate([p[2] for p in parts]);del parts
used=np.zeros(len(traced_ids),dtype=bool);used[a]=True;used[b]=True
ids=traced_ids[used];n=len(ids)
remap=np.cumsum(used,dtype=np.uint32)-1
a=remap[a];b=remap[b]
# Stable ordering preserves the original converter's exact CSR payload.
order=np.argsort(a,kind='stable');a=a[order];b=b[order];w=w[order];del order
print('traced',len(traced_ids),'connected',n,'edges',len(a),flush=True)
indptr=np.concatenate(([0],np.cumsum(np.bincount(a,minlength=n)))).astype('<u4')
signmap={'acetylcholine':1,'gaba':-1,'glutamate':-1}
signs=np.array([signmap.get(nt.get(int(i)),0) for i in ids],dtype=np.float32)
# Measured contact counts preserved; signed weights normalized to total input.
norm=np.bincount(b,weights=w*np.abs(signs[a]),minlength=n)
normalized=(w*signs[a]/np.maximum(1,norm[b])).astype('<f4')
classes=[ann[int(i)]['superclass'] or '' for i in ids]
inputs=np.array([i for i,c in enumerate(classes) if c in ['visual_projection','cb_sensory']],dtype='<u4')
outputs=np.array([i for i,c in enumerate(classes) if c in ['descending_neuron','cb_motor','vnc_motor']],dtype='<u4')
# Balanced deterministic artificial eight-channel assignment; not a receptive-field claim.
channels=(np.arange(len(inputs))%8).astype('<u4')
with (out/'connectome.bin').open('wb') as f:
 np.array([0x55464c59,n,len(b),len(inputs),len(outputs)],dtype='<u4').tofile(f)
 for data in [indptr,b,normalized,inputs,channels,outputs]:data.tofile(f)
# Measured somata only: absent locations never become invented points.
visible=[(i,ann[int(body)]['somaLocation']) for i,body in enumerate(ids) if ann[int(body)]['somaLocation']]
output_set=set(outputs.tolist())
positions={'indices':[i for i,p in visible],'positions':[p for i,p in visible],'groups':[0 if classes[i].startswith(('ol_','visual')) else 2 if i in output_set else 1 for i,p in visible]}
(out/'neurons.json').write_text(json.dumps(positions,separators=(',',':')))
manifest={'dataset':'MaleCNS v1.0','neurons':n,'edges':len(b),'synapticContacts':synaptic_contacts,'sensory':len(inputs),'motor':len(outputs),'visible':len(visible),'source':'https://male-cns.janelia.org/download/','sources':expected,'selection':'All Traced neurons with at least one measured connection to another Traced neuron. Every internal directed connection retained, including one-contact edges. No sampled subcircuit.','license':'CC BY 4.0','binarySha256':hashlib.sha256((out/'connectome.bin').read_bytes()).hexdigest(),'binaryBytes':(out/'connectome.bin').stat().st_size,'zeroSignCells':int((signs==0).sum()),'assumedSignMapping':{'acetylcholine':1,'gaba':-1,'glutamate':-1,'otherOrUnknown':0},'model':'Signed normalized weights; artificial 8-channel sensory input; leaky rate dynamics. Only the motor readout is trained. No biological learning or electrophysiological fidelity claimed.'}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps(manifest,indent=2))
