// Reproducible full-circuit parity and latency benchmark. GPL-3.0-or-later.
// node tools/fly/benchmark-inference.mjs /tmp/fly-reference [report.json]
import assert from 'node:assert/strict';
import {spawn,execFileSync} from 'node:child_process';
import {createInterface} from 'node:readline';
import {readFile,writeFile} from 'node:fs/promises';
import {createReadStream} from 'node:fs';
import {createHash} from 'node:crypto';
import {cpus,arch,platform} from 'node:os';
import {fileURLToPath} from 'node:url';
import path from 'node:path';
import {createFlyPolicy} from '../../ui/fly-policy.mjs';
const root=fileURLToPath(new URL('../../',import.meta.url));
const referenceBinary=process.argv[2];
if(!referenceBinary)throw Error('Pass a scalar reference executable built from tools/fly/brain.cpp with -DFLY_BRAIN_CLI.');
const model=JSON.parse(await readFile(path.join(root,'ui/public/fly/data/readout.json')));
const graph=process.env.ULTIMATE_FLY_DATA??path.join(root,'networks/fly/connectome.bin');
const graphHash=createHash('sha256');
for await(const chunk of createReadStream(graph))graphHash.update(chunk);
assert.equal(graphHash.digest('hex'),model.circuitSha256);
const binary=process.env.ULTIMATE_FLY_BINARY??path.join(root,'src/ultimate_fly_brain');
const referee=process.env.ULTIMATE_FLY_RULES_BINARY??path.join(root,'src/ultimate_fly_rules');
const workers=[];
async function worker(binary){
 const child=spawn(binary,[graph],{stdio:['pipe','pipe','inherit']});workers.push(child);
 const lines=createInterface({input:child.stdout})[Symbol.asyncIterator]();
 assert.equal((await lines.next()).value,`ready ${model.motor}`);
 return async line=>{child.stdin.write(line+'\n');const next=await lines.next();assert.equal(next.done,false);return JSON.parse(next.value);};
}
const batch=(command,rows)=>command(`${rows.length} ${rows.flat().join(' ')}`);
async function timed(task){const start=performance.now();const result=await task();return {result,ms:performance.now()-start};}
const fixtures=[];
try{
 const reference=await worker(referenceBinary),optimized=await worker(binary);
 const warmPolicy=createFlyPolicy(model,optimized);
 for(let preset=0;preset<3;preset++)for(let reflection=0;reflection<2;reflection++){
  const moves=[];
  for(let ply=0;ply<=40;ply++){
   const ruleStart=performance.now();
   const state=JSON.parse(execFileSync(referee,[],{input:`${preset} ${reflection} ${moves.length}\n${moves.join(' ')}\n`,encoding:'utf8'}));
   const rulesMs=performance.now()-ruleStart;
   if(state.terminal)break;
   if([0,16,40].includes(ply)){
    const rows=state.moves.map(move=>move.features);
    let expectedRows;
    const baseline=await timed(async()=>{
     expectedRows=await batch(reference,rows);
     const scores=expectedRows.map(row=>row.reduce((sum,v,i)=>sum+(v-model.mean[i])/model.scale[i]*model.weights[i],0));
     const index=scores.indexOf(Math.max(...scores));
     await batch(reference,[rows[index]]);
     return {index,scores,...await reference('0')};
    });
    // Check every candidate motor output, not just the selected move.
    const actualRows=await batch(optimized,rows);
    assert.deepEqual(actualRows,expectedRows);
    const cold=await timed(()=>createFlyPolicy(model,optimized)(rows));
    assert.deepEqual(cold.result,baseline.result);
    const warm=await timed(()=>warmPolicy(rows));
    assert.deepEqual(warm.result,baseline.result);
    const repeat=await timed(()=>warmPolicy(rows));
    assert.deepEqual(repeat.result,baseline.result);
    const result={preset,reflection,ply,key:state.key,candidates:rows.length,
     unique:new Set(rows.map(row=>row.join(' '))).size,chosen:baseline.result.index,
     rulesMs,baselineMs:baseline.ms,coldMs:cold.ms,warmMs:warm.ms,repeatMs:repeat.ms};
    fixtures.push(result);console.log(JSON.stringify(result));
   }
   moves.push((ply*31+17)%state.moves.length);
  }
 }
 const total=key=>fixtures.reduce((sum,f)=>sum+f[key],0);
 const report={date:new Date().toISOString(),platform:platform(),architecture:arch(),cpu:cpus()[0]?.model,
  graphSha256:model.circuitSha256,historyRule:'index = (ply * 31 + 17) % legalMoves.length',
  parity:'Exact JSON motor outputs, scores, selected indices, activity counts and all eight telemetry frames',
  fixtures,summary:{positions:fixtures.length,baselineMs:total('baselineMs'),coldMs:total('coldMs'),warmMs:total('warmMs'),repeatMs:total('repeatMs'),
   coldSpeedup:total('baselineMs')/total('coldMs'),warmSpeedup:total('baselineMs')/total('warmMs'),repeatSpeedup:total('baselineMs')/total('repeatMs')},sourceSha256:{}};
 for(const p of ['tools/fly/brain.cpp','tools/fly/brain-server.cpp','ui/fly-policy.mjs'])report.sourceSha256[p]=createHash('sha256').update(await readFile(path.join(root,p))).digest('hex');
 if(process.argv[3])await writeFile(process.argv[3],JSON.stringify(report,null,2)+'\n');
 console.log(JSON.stringify(report.summary));
}finally{
 for(const child of workers){child.stdin.end();child.kill();}
}
