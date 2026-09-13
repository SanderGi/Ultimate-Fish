import {build} from 'esbuild';
import {readFile,writeFile,mkdir,rm,copyFile} from 'node:fs/promises';
import {createElement} from 'react';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {renderToStaticMarkup} from 'react-dom/server';
const root=new URL('../',import.meta.url);
await build({entryPoints:[new URL('app.mjs',import.meta.url).pathname],bundle:true,format:'esm',target:'es2022',minify:true,outfile:new URL('public/fly/app.js',root).pathname,legalComments:'eof'});
const temp=new URL('.fly-build/',root);await mkdir(temp,{recursive:true});
try {
 await build({entryPoints:[new URL('app/PieceIcon.tsx',root).pathname],bundle:true,format:'esm',platform:'node',jsx:'automatic',packages:'external',outfile:new URL('piece.mjs',temp).pathname});
 const {default:PieceIcon}=await import(new URL('piece.mjs',temp));
 const types=['king','jester','knight','pawn','queen','rook','bishop','berserker','bomb','ninja','turtle','ghost','mage','goop','penguin','parasite','devil','minion','sludge','sniper','prince','checker','checkerKing','giant','copycat','copycatClone','angel','halo','fisherman','dragon'];
 const symbols=types.map(id=>{const svg=renderToStaticMarkup(createElement(PieceIcon,{id}));return `<symbol id="${id.toLowerCase()}" viewBox="0 0 64 64">${svg.replace(/^<svg[^>]*>/,'').replace(/<\/svg>$/,'')}</symbol>`;});
 await writeFile(new URL('public/fly/pieces.svg',root),`<svg xmlns="http://www.w3.org/2000/svg">${symbols.join('')}</svg>`);
}finally{await rm(temp,{recursive:true,force:true});}
await copyFile(new URL('../../docs/fly/training-report.json',import.meta.url),new URL('public/fly/data/training-report.json',root));
// Checkpoint and data manifests are required; never silently ship an untrained fallback.
const model=JSON.parse(await readFile(new URL('public/fly/data/readout.json',root)));
const manifest=JSON.parse(await readFile(new URL('public/fly/data/manifest.json',root)));
if(model.circuitSha256!==manifest.binarySha256||model.neurons!==164587)throw Error('Full circuit checkpoint mismatch');
const recording=JSON.parse(await readFile(new URL('public/fly/data/recorded-activity.json',root)));
const recordedBytes=await readFile(new URL('public/fly/data/recorded-activity.bin',root));
if(recording.circuitSha256!==manifest.binarySha256||createHash('sha256').update(recordedBytes).digest('hex')!==recording.binarySha256)throw Error('Recorded activity provenance mismatch');
console.log('Built Ultimate Fly browser assets.');
execFileSync('tar',['--exclude=__pycache__','-czf',new URL('public/fly/source.tar.gz',root).pathname,'Copying.txt','src/ultimate','tools/fly','ui/fly','ui/fly-service.mjs','ui/fly-policy.mjs','ui/fly-rules-service.mjs','ui/app/api/fly','ui/app/PieceIcon.tsx','ui/package.json','ui/package-lock.json','ui/public/fly/index.html','ui/public/fly/style.css','ui/public/fly/data','docs/fly'],{cwd:new URL('../',root)});
