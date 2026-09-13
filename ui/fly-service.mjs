// Public, bounded full-circuit inference. No engine search or position input. GPLv3+.
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {readFile} from 'node:fs/promises';
import {createReadStream} from 'node:fs';
import {createHash} from 'node:crypto';
import path from 'node:path';
let worker, lines, readout, busy=false;
export function isSameOrigin(request) {
  const origin = request.headers.get('origin');
  if (!origin) return true;
  try {
    // Next's standalone server can use localhost for request.url even when
    // the browser used a public hostname. Compare the incoming Host instead.
    const url = new URL(origin);
    const host = request.headers.get('host') ?? new URL(request.url).host;
    const protocol = request.headers.get('x-forwarded-proto')?.split(',')[0].trim()
      ?? new URL(request.url).protocol.slice(0, -1);
    return ['http', 'https'].includes(protocol) &&
      url.protocol === `${protocol}:` && url.host === host.toLowerCase();
  } catch { return false; }
}
export function validCandidates(candidates) {
  return Array.isArray(candidates) && candidates.length>0 && candidates.length<=256 &&
    candidates.every(f=>Array.isArray(f)&&f.length===8&&f.every((v,i)=>
      typeof v==='number'&&Number.isFinite(v)&&v>=(i===0?-1:0)&&v<=1));
}
async function initialize() {
  if(worker)return;
  const data=process.env.ULTIMATE_FLY_DATA ?? path.resolve(process.cwd(),'../networks/fly/connectome.bin');
  const binary=process.env.ULTIMATE_FLY_BINARY ?? path.resolve(process.cwd(),'../src/ultimate_fly_brain');
  readout=JSON.parse(await readFile(path.resolve(process.cwd(),'public/fly/data/readout.json'),'utf8'));
  const hash=createHash('sha256');for await(const chunk of createReadStream(/* turbopackIgnore: true */ data))hash.update(chunk);
  if(hash.digest('hex')!==readout.circuitSha256)throw new Error('Fly circuit and checkpoint do not match');
  const child=spawn(/* turbopackIgnore: true */ binary,[data],{stdio:['pipe','pipe','inherit']});
  child.on('error',()=>{});
  child.stdin.on('error',()=>{});
  lines=createInterface({input:child.stdout})[Symbol.asyncIterator]();
  worker=child;
  child.on('exit',()=>{if(worker===child)worker=undefined;});
  const ready=await lines.next();
  if(ready.value!==`ready ${readout.motor}`)throw new Error('Fly circuit failed to load');
}
async function command(line) {
  worker.stdin.write(line+'\n');const result=await lines.next();
  if(result.done)throw new Error('Fly simulation interrupted');
  return JSON.parse(result.value);
}
export async function inferFly(candidates) {
  if(!validCandidates(candidates))return {status:400,body:{error:'Expected 1–256 public move feature vectors.'}};
  if(busy)return {status:429,body:{error:'The fly is thinking in another game. Try again shortly.'}};
  busy=true;const start=performance.now();
  let timeout;
  try {
    const task=(async()=>{
      await initialize();
      const rows=await command(`${candidates.length} ${candidates.flat().join(' ')}`);
      const scores=rows.map(row=>row.reduce((sum,v,i)=>sum+(v-readout.mean[i])/readout.scale[i]*readout.weights[i],0));
      const index=scores.indexOf(Math.max(...scores));
      await command(`1 ${candidates[index].join(' ')}`);
      const telemetry=await command('0');
      return {index,scores,...telemetry,elapsedMs:Math.round(performance.now()-start),neurons:164587,connections:25563197};
    })();
    const body=await Promise.race([task,new Promise((_,reject)=>{timeout=setTimeout(()=>reject(Error('Fly inference timed out')),120000);})]);
    return {status:200,body};
  }catch(error) {
    worker?.kill('SIGKILL');worker=undefined;
    console.error('Ultimate Fly:',error.message);
    return {status:503,body:{error:'The fly brain is unavailable. Please retry.'}};
  }finally {clearTimeout(timeout);busy=false;}
}
