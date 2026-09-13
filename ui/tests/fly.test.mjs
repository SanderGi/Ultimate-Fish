import assert from 'node:assert/strict';
import test from 'node:test';
import {readFile} from 'node:fs/promises';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {createHash} from 'node:crypto';
import {refereeGame,validGame} from '../fly-rules-service.mjs';
import {createReferee} from '../fly/referee.mjs';
import {createAnatomy} from '../fly/anatomy.mjs';
import {Vector3} from 'three';
import {validCandidates,isSameOrigin} from '../fly-service.mjs';
import {FLY_PUBLIC_PATHS,isPasswordExemptPath} from '../password-exemption.mjs';
const call=async game=>{const result=await refereeGame(game);assert.equal(result.status,200,JSON.stringify(result.body));return result.body;};
test('origin checks work behind the standalone server and HTTPS proxy',()=>{
 const request=(origin,host='ultimatefish.fly.dev',proto='https')=>new Request('http://localhost:8080/api/fly/choose',{headers:{origin,host,'x-forwarded-proto':proto}});
 assert.equal(isSameOrigin(request('https://ultimatefish.fly.dev')),true);
 assert.equal(isSameOrigin(request('http://127.0.0.1:3013','127.0.0.1:3013','http')),true);
 for(const origin of ['https://unrelated.invalid','http://ultimatefish.fly.dev','null'])assert.equal(isSameOrigin(request(origin)),false);
});

test('fly exemption is explicit and does not expose analysis or arbitrary files',()=>{
 for(const p of FLY_PUBLIC_PATHS)assert.equal(isPasswordExemptPath(p),true);
 for(const p of ['/api/engine/search','/api/engine/state','/api/fly/choose/extra','/api/fly/state/extra','/fly/worker.js','/fly/rules.wasm','/fly/rules.mjs','/fly/secrets','/fly/data/connectome.bin','/fly/app.js.map','/fly/source.tar.gz/','/','/_next/static/private.js'])assert.equal(isPasswordExemptPath(p),false,p);
});
test('untrusted fly requests have bounded finite public feature vectors',()=>{
 assert.equal(validCandidates([[0,0,.3,.5,1,0,.7,0]]),true);
 for(const f of [null,[],{},[[NaN,0,0,0,0,0,0,0]],[[0,-1,0,0,0,0,0,0]],[[0,0,0,0,0,0,0,2]],[[0]],Array(257).fill(Array(8).fill(0))])assert.equal(validCandidates(f),false);
});
test('server history accepts only bounded public presets and actions',async()=>{
 for(const value of [null,[],{}, {preset:3,reflection:0,moves:[]}, {preset:0,reflection:2,moves:[]}, {preset:0,reflection:0,moves:[-1]}, {preset:0,reflection:0,moves:[.5]}, {preset:0,reflection:0,moves:Array(401).fill(0)}, {preset:0,reflection:0,moves:[],upn:'private'}]){
  assert.equal(validGame(value),false);assert.equal((await refereeGame(value)).status,400);
 }
 assert.equal((await refereeGame({preset:0,reflection:0,moves:[4095]})).status,400);
});
test('three public armies start legally and actions/undo are isolated between requests',async()=>{
 for(let preset=0;preset<3;preset++)for(let reflection=0;reflection<2;reflection++){
  const game={preset,reflection,moves:[]},initial=await call(game);
  assert.equal(initial.terminal,0);assert.equal(initial.check,false);assert.equal(initial.pieces.length,32);
  assert.ok(initial.moves.length>0);assert.ok(!initial.pieces.some(p=>['ghost','jester'].includes(p.type)));
  for(const move of initial.moves){const after=await call({...game,moves:[move.index]});assert.equal(after.ply,1);}
  // An independent tab and a failed request cannot affect replay/undo of this game.
  await call({preset:(preset+1)%3,reflection:1-reflection,moves:[0]});
  assert.deepEqual(await call(game),initial);
 }
});
test('threefold history ends play and undo restores the preceding legal position',async()=>{
 const game={preset:0,reflection:0,moves:[]};let state=await call(game),before;
 for(let repeat=0;repeat<3&&!state.terminal;repeat++){
  for(const notation of ['b1-a3','b10-a8','a3-b1','a8-b10']){
   const move=state.moves.find(move=>move.notation===notation);assert.ok(move,notation);
   before=state;game.moves.push(move.index);state=await call(game);
   if(state.terminal)break;
  }
 }
 assert.equal(state.terminal,7);assert.equal(state.winner,-1);assert.deepEqual(state.moves,[]);
 assert.equal((await refereeGame({...game,moves:[...game.moves,0]})).status,400);
 assert.deepEqual(await call({...game,moves:game.moves.slice(0,-1)}),before);
});
test('server replay agrees with the native training referee throughout games',async t=>{
 const binary=process.env.FLY_TEACHER;
 if(!binary){t.skip('Set FLY_TEACHER to compare the training executable.');return;}
 const child=spawn(binary,[],{stdio:['pipe','pipe','inherit']});
 const lines=createInterface({input:child.stdout})[Symbol.asyncIterator]();
 const native=async s=>{child.stdin.write(s+'\n');return JSON.parse((await lines.next()).value);};
 try{
  for(let preset=0;preset<3;preset++){
   const game={preset,reflection:0,moves:[]};
   let actual=await call(game),reference=await native(`reset ${preset} 0`);
   for(let ply=0;ply<120&&!actual.terminal;ply++){
    assert.deepEqual(actual,reference);const index=(ply*31+17)%actual.moves.length;
    game.moves.push(index);actual=await call(game);reference=await native(`move ${index}`);
   }
   assert.deepEqual(actual,reference);
   if(actual.terminal)assert.equal((await refereeGame({...game,moves:[...game.moves,0]})).status,400);
  }
 }finally{child.stdin.end();}
});
test('browser history survives failed moves and restores a whole turn in one undo',async()=>{
 const requests=[];
 let fail=false;
 const referee=createReferee(async(url,options)=>{
  assert.equal(url,'/api/fly/state');const game=JSON.parse(options.body);requests.push(game);
  return {ok:!fail,json:async()=>fail?{error:'Unavailable'}:{ply:game.moves.length}};
 });
 await referee('reset',[1,0]);await referee('move',[3]);
 fail=true;await assert.rejects(referee('move',[5]),/Unavailable/);fail=false;
 await referee('move',[2]);assert.deepEqual(requests.at(-1).moves,[3,2]);
 await referee('undo',[0]);assert.deepEqual(requests.at(-1).moves,[]);
 await referee('move',[4]);assert.deepEqual(requests.at(-1).moves,[4]);
});
test('a late response from a replaced game cannot change the new history',async()=>{
 const requests=[];
 const referee=createReferee((url,options)=>new Promise(resolve=>requests.push({game:JSON.parse(options.body),signal:options.signal,resolve})));
 const finish=i=>requests[i].resolve({ok:true,json:async()=>({ply:requests[i].game.moves.length})});
 const old=referee('reset',[0,0]);const rejected=assert.rejects(old,/Game replaced/);
 const current=referee('reset',[2,1]);assert.equal(requests[0].signal.aborted,true);
 finish(1);await current;finish(0);await rejected;
 const move=referee('move',[3]);assert.equal(requests[2].game.preset,2);assert.equal(requests[2].game.reflection,1);finish(2);await move;
});
test('full checkpoint, source topology, and neural visualization share the full circuit',async()=>{
 const [manifest,model,atlas,report]=await Promise.all(['manifest','readout','neurons','training-report'].map(async name=>JSON.parse(await readFile(new URL(`../public/fly/data/${name}.json`,import.meta.url),'utf8'))));
 assert.equal(manifest.neurons,164587);assert.equal(manifest.edges,25563197);assert.equal(model.circuitSha256,manifest.binarySha256);
 for(const key of ['weights','mean','scale']){assert.equal(model[key].length,2129);assert.ok(model[key].every(Number.isFinite));}
 assert.ok(model.weights.some(v=>v!==0));assert.ok(model.scale.every(v=>v>0));
 assert.equal(atlas.indices.length,atlas.positions.length);assert.equal(new Set(atlas.indices).size,manifest.visible);assert.ok(atlas.indices.every(i=>i<manifest.neurons));
 assert.ok(report.trained.crossEntropy<report.initial.crossEntropy);assert.equal(report.validation.excludedAllTrainingKeys,true);
 if(process.env.ULTIMATE_FLY_DATA){const bytes=await readFile(process.env.ULTIMATE_FLY_DATA);assert.equal(createHash('sha256').update(bytes).digest('hex'),manifest.binarySha256);}
});

test('flight anatomy keeps every leg segment in its joint chain and moves both wings',async()=>{
 const metadata=JSON.parse(await readFile(new URL('../public/fly/data/model.json',import.meta.url)));
 const bytes=await readFile(new URL('../public/fly/data/model.bin',import.meta.url));
 assert.equal(createHash('sha256').update(bytes).digest('hex'),metadata.binarySha256);
 const {root,groups,update}=createAnatomy(metadata,bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength));
 assert.equal(metadata.outputTriangles,272550);
 for(const node of metadata.nodes)assert.equal(groups[node.name].parent,node.parent?groups[node.parent]:root);
 for(let leg=1;leg<=3;leg++)for(const side of ['left','right']){
  const suffix=`T${leg}_${side}`;
  const names=['coxa','femur','tibia','tarsus','tarsus2','tarsus3','tarsus4','claw'].map(name=>`${name}_${suffix}`);
  for(let i=1;i<names.length;i++)assert.equal(groups[names[i]].parent,groups[names[i-1]]);
  const hip=groups[names[0]];
  const segments=names.slice(1).map(name=>groups[name]);
  root.updateMatrixWorld(true);
  const before=segments.map(group=>group.getWorldPosition(new Vector3()));
  hip.rotateX(.35);root.updateMatrixWorld(true);
  segments.forEach((group,i)=>assert.ok(group.getWorldPosition(new Vector3()).distanceTo(before[i])>1e-5,group.name));
 }
 update(.1,false);root.updateMatrixWorld(true);
 const before=new Map(Object.entries(groups).map(([name,group])=>[name,group.matrixWorld.clone()]));
 update(.31,false);root.updateMatrixWorld(true);
 for(const [name,group] of Object.entries(groups)){
  assert.ok(group.matrixWorld.elements.every(Number.isFinite),name);
  if(/_T[123]_(left|right)$/.test(name)||name.startsWith('wing_'))assert.ok(!group.matrixWorld.equals(before.get(name)),name);
 }
 update(.31,true);
 for(const side of ['left','right']){
  const wing=groups[`wing_${side}`];
  const copies=wing.parent.children.filter(group=>group.name===wing.name);
  assert.equal(copies.length,9);assert.equal(copies.filter(group=>group.visible).length,1);
 }
});

test('waiting animation contains recorded full-circuit telemetry with matching provenance',async()=>{
 const base=new URL('../public/fly/data/',import.meta.url);
 const metadata=JSON.parse(await readFile(new URL('recorded-activity.json',base)));
 const manifest=JSON.parse(await readFile(new URL('manifest.json',base)));
 const bytes=await readFile(new URL('recorded-activity.bin',base));
 assert.equal(metadata.circuitSha256,manifest.binarySha256);
 assert.equal(metadata.neurons,164587);assert.equal(metadata.connections,25563197);
 assert.equal(metadata.runtimeSha256,createHash('sha256').update(await readFile(new URL('../../tools/fly/brain.cpp',import.meta.url))).digest('hex'));
 assert.equal(metadata.binarySha256,createHash('sha256').update(bytes).digest('hex'));
 assert.equal(bytes.length,metadata.clips.length*metadata.framesPerClip*metadata.samples);
 assert.equal(metadata.samples,Math.ceil(metadata.neurons/metadata.stride));
 assert.deepEqual([...new Set(metadata.clips.map(clip=>clip.preset))],[0,1,2]);
 const distinct=new Set();
 for(const [index,clip] of metadata.clips.entries()){
  assert.equal(clip.byteOffset,index*metadata.framesPerClip*metadata.samples);
  assert.equal(validCandidates([clip.features]),true);
  const frames=bytes.subarray(clip.byteOffset,clip.byteOffset+metadata.framesPerClip*metadata.samples);
  assert.ok(frames.some(value=>value>0));
  distinct.add(createHash('sha256').update(frames).digest('hex'));
 }
 assert.ok(distinct.size>6);
});
