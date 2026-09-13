"""Train full-circuit motor readout against deterministic Ultimate Fish teachers.
No biological weights are optimized. Requires numpy, scipy, native brain library.
"""
import ctypes,hashlib,json,os,subprocess,time
from pathlib import Path
import numpy as np
from scipy.optimize import minimize
ROOT=Path(__file__).resolve().parents[2]
START=time.monotonic();SEED=20260913;rng=np.random.default_rng(SEED)
lib=ctypes.CDLL(os.environ.get('FLY_BRAIN_LIBRARY','/tmp/ultimate-fly-brain.dylib'))
lib.brain_load.argtypes=[ctypes.c_char_p];lib.brain_encode.argtypes=[ctypes.POINTER(ctypes.c_float)];lib.brain_encode.restype=ctypes.POINTER(ctypes.c_float)
DATA=Path(os.environ.get('FLY_DATA','/tmp/ultimate-fly-full'))
N=lib.brain_load(str(DATA/'connectome.bin').encode());assert N==2129
teacher=subprocess.Popen([os.environ.get('FLY_TEACHER','/tmp/ultimate-fly-teacher')],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True)
cache={};nodes=0;inferences=0;inference_seconds=0;seen_positions=set()
fingerprint=hashlib.sha256((DATA/'manifest.json').read_bytes()+(ROOT/'tools/fly/brain.cpp').read_bytes()).hexdigest()
cache_path=Path(os.environ.get('FLY_CACHE','/tmp/ultimate-fly-encoding-cache.npz'))
if cache_path.exists():
 z=np.load(cache_path)
 if 'fingerprint' in z and str(z['fingerprint'])==fingerprint:
  cache={tuple(k):v for k,v in zip(z['keys'],z['values'])}
def cmd(line):
 teacher.stdin.write(line+'\n');teacher.stdin.flush();line=teacher.stdout.readline()
 if not line:raise RuntimeError('Teacher process exited')
 return json.loads(line)
def encode(move):
 global inferences,inference_seconds
 # Inputs already serialized to 6 digits by the shared native referee. No extra quantization.
 key=tuple(move['features'])
 if key not in cache:
  f=np.array(key,dtype=np.float32);t=time.monotonic()
  cache[key]=np.ctypeslib.as_array(lib.brain_encode(f.ctypes.data_as(ctypes.POINTER(ctypes.c_float))),shape=(N,)).copy()
  inference_seconds+=time.monotonic()-t;inferences+=1
 return cache[key]
def save_cache():np.savez(cache_path,fingerprint=fingerprint,keys=np.array(list(cache)),values=np.array(list(cache.values())))
def collect(games,base):
 global nodes
 data=[]
 for g in range(games):
  episode_rng=np.random.default_rng(base+g)
  state=cmd(f'reset {g%3} {base+g}')
  for ply in range(48):
   if state['terminal']:break
   label=cmd('teach');nodes+=label['nodes'];target=label['index']
   if target<0:break
   others=[i for i in range(len(state['moves'])) if i!=target]
   chosen=[target]+episode_rng.choice(others,min(5,len(others)),replace=False).tolist()
   if state['key'] not in seen_positions:
    x=np.array([encode(state['moves'][i]) for i in chosen]);data.append((x,base+g));seen_positions.add(state['key'])
   index=int(episode_rng.integers(len(state['moves']))) if episode_rng.random()<.3 else target
   state=cmd(f'move {index}')
  print(f'Collected {base}: {g+1}/{games}; positions {len(data)}, unique encodings {len(cache)}, seconds {time.monotonic()-START:.1f}',flush=True)
  if g%3==2:save_cache()
 return data
try:
 train=collect(24,1000);valid=collect(6,100000)
 all_x=np.concatenate([x for x,g in train]);mean=all_x.mean(0);scale=np.maximum(all_x.std(0),.005)
 def design(data):
  xs=[(x-mean)/scale for x,g in data];sizes=np.array([len(x) for x in xs]);starts=np.r_[0,np.cumsum(sizes)[:-1]]
  return np.concatenate(xs).astype(np.float64),sizes,starts
 X,sizes,starts=design(train);V,vsizes,vstarts=design(valid)
 def objective(w,x=X,sizes=sizes,starts=starts):
  logits=x@w;mx=np.maximum.reduceat(logits,starts);p=np.exp(logits-np.repeat(mx,sizes));sums=np.add.reduceat(p,starts)
  loss=np.mean(np.log(sums)+mx-logits[starts]);p/=np.repeat(sums,sizes);p[starts]-=1
  return loss+.02*np.dot(w,w),x.T@p/len(starts)+.04*w
 trace=[]
 def callback(w):
  trace.append(float(objective(w,V,vsizes,vstarts)[0]))
 trained=minimize(objective,np.zeros(N),jac=True,method='L-BFGS-B',callback=callback,options={'maxiter':100,'ftol':1e-8})
 w=trained.x
 def metrics(x,sizes,starts,weights):
  logits=x@weights;correct=0;regret=0
  for start,size in zip(starts,sizes):
   pick=np.argmax(logits[start:start+size]);correct+=pick==0
  return {'crossEntropy':float(objective(weights,x,sizes,starts)[0]-.02*np.dot(weights,weights)),'bestPick':float(correct/len(starts))}
 model={'version':2,'seed':SEED,'neurons':164587,'connections':25563197,'motor':N,'iterations':8,'mean':mean.tolist(),'scale':scale.tolist(),'weights':w.tolist(),'trainingPositions':len(train),'validationPositions':len(valid),'circuitSha256':json.loads((DATA/'manifest.json').read_text())['binarySha256']}
 report={'seed':SEED,'teacher':{'depth':3,'nodeLimit':512,'nodes':nodes,'tablebases':False},'train':{'games':24,'seedBase':1000,'positions':len(train),'candidateLimit':6,'deduplicatedBy':'Position::key'},'validation':{'games':6,'seedBase':100000,'positions':len(valid),'candidateLimit':6,'excludedAllTrainingKeys':True},'initial':metrics(V,vsizes,vstarts,np.zeros(N)),'trained':metrics(V,vsizes,vstarts,w),'optimizer':{'success':bool(trained.success),'message':str(trained.message),'iterations':int(trained.nit)},'validationLossHistory':trace,'matches':[]}
 # Initial candidate ordering in collection puts the teacher first; initial tie metric
 # must be chance, not argmax of this deliberately ordered training matrix.
 report['initial']['bestPick']=float(np.mean(1/vsizes))
 (ROOT/'ui/public/fly/data/readout.json').write_text(json.dumps(model,separators=(',',':'))+'\n')
 print('MODEL SAVED',json.dumps(report['trained']),flush=True)
 for opponent in ['random','silenced']:
  results={'opponent':opponent,'wins':0,'draws':0,'losses':0,'games':[]}
  for g in range(6):
   seed=200000+g//2;color=g%2;preset=g//2;rand=np.random.default_rng(seed)
   state=cmd(f'reset {preset} {seed}');t=time.monotonic();ply=0
   for ply in range(100):
    if state['terminal']:break
    if state['side']==color:
     scores=[float(((encode(m)-mean)/scale)@w) for m in state['moves']];index=int(np.argmax(scores))
    else:index=int(rand.integers(len(state['moves']))) if opponent=='random' else 0
    state=cmd(f'move {index}')
   result=('wins' if state['winner']==color else 'losses') if state['terminal'] and state['winner']>=0 else 'draws'
   results[result]+=1;results['games'].append({'seed':seed,'preset':preset,'flyColor':color,'actions':state['ply'],'result':result,'terminal':state['terminal'] or '100-action-cap','seconds':time.monotonic()-t})
   print('Match',opponent,g,result,'cache',len(cache),flush=True);save_cache()
  report['matches'].append(results)
 report.update({'elapsedSeconds':time.monotonic()-START,'uniqueFullCircuitInferences':inferences,'neuralSeconds':inference_seconds,'cachedFeatureVectors':len(cache),'circuitSha256':model['circuitSha256'],'sourceSha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['tools/fly/brain.cpp','tools/fly/rules.cpp','tools/fly/train.py','src/ultimate/position.cpp','src/ultimate/position.h']}})
 (ROOT/'docs/fly/training-report.json').write_text(json.dumps(report,indent=2)+'\n');save_cache()
 print(json.dumps({k:report[k] for k in ['trained','initial','elapsedSeconds']}),flush=True)
finally:teacher.stdin.close();teacher.wait(timeout=5)
