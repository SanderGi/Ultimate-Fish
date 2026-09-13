import * as T from 'three';
import {OrbitControls} from 'three/addons/controls/OrbitControls.js';
import {createAnatomy} from './anatomy.mjs';
async function resource(url,type='json') {const r=await fetch(url);if(!r.ok)throw Error('Could not load '+url);return r[type]();}
function stage(host,dark=false) {
  const renderer=new T.WebGLRenderer({alpha:true,antialias:true});
  renderer.setPixelRatio(Math.min(devicePixelRatio,1.75));renderer.setClearColor(0,0);
  renderer.outputColorSpace=T.SRGBColorSpace;renderer.toneMapping=T.ACESFilmicToneMapping;
  host.appendChild(renderer.domElement);
  renderer.domElement.setAttribute('aria-label',dark?'Orbit the measured fly neurons':'Orbit the anatomical fruit fly');
  // Anatomy points should retain their scale while orbiting, rather than swell
  // toward the perspective camera and look as though they pass through it.
  const camera=dark?new T.OrthographicCamera(-2.3,2.3,2.3,-2.3,.1,100):new T.PerspectiveCamera(34,1,.01,100);
  camera.position.set(2.8,1.7,4.6);
  const scene=new T.Scene(), controls=new OrbitControls(camera,renderer.domElement);
  controls.enablePan=false;controls.enableDamping=!dark;controls.minDistance=2;controls.maxDistance=9;
  if(dark){
    // The short brain viewport otherwise turns a modest drag into a half turn.
    controls.rotateSpeed=.3;
    controls.minPolarAngle=.15;controls.maxPolarAngle=Math.PI-.15;
    camera.zoom=2;camera.updateProjectionMatrix();
    controls.minZoom=2;controls.maxZoom=2;controls.enableZoom=false;
  }
  controls.target.set(0,0,0);controls.update();
  new ResizeObserver(()=>{
    const {width,height}=host.getBoundingClientRect();if(width<=0||height<=0)return;
    const aspect=width/height;renderer.setSize(width,height,false);
    if(camera.isOrthographicCamera){
      const halfHeight=2.3*Math.max(1,1/aspect);
      camera.left=-halfHeight*aspect;camera.right=halfHeight*aspect;
      camera.top=halfHeight;camera.bottom=-halfHeight;
    }else camera.aspect=aspect;
    camera.updateProjectionMatrix();
  }).observe(host);
  return {renderer,camera,scene,controls};
}
export async function flyScene(host,state) {
  const {renderer,camera,scene,controls}=stage(host);
  camera.position.set(3.2,1.7,4.5);controls.target.set(0,-.05,0);controls.minDistance=3.6;controls.update();
  scene.add(new T.HemisphereLight(0xfff3df,0x354b26,2.7));
  const key=new T.DirectionalLight(0xffecd0,3.8);key.position.set(2,4,3);scene.add(key);
  const rim=new T.DirectionalLight(0xedfbe2,3);rim.position.set(-3,2,-2);scene.add(rim);
  const rig=new T.Group();scene.add(rig);rig.scale.setScalar(6.3);
  const [metadata,data]=await Promise.all([resource('/fly/data/model.json'),resource('/fly/data/model.bin','arrayBuffer')]);
  const anatomy=createAnatomy(metadata,data);rig.add(anatomy.root);
  host.querySelector('.scene-loading')?.remove();
  let previous=null,elapsed=0,effort=0,wasPaused=state.paused;
  function animate(now){
    requestAnimationFrame(animate);
    if(document.hidden){previous=null;return;}
    const dt=previous===null?0:Math.min((now-previous)/1000,.05);previous=now;
    if(!state.paused){
      elapsed+=dt;
      const target=state.thinking?1:state.celebrateUntil>now?.6:0;
      effort+=(target-effort)*(1-Math.exp(-dt*2));
      // A quiet hover with coupled drift and banking, not a grounded body bob.
      rig.position.set(.07*Math.sin(elapsed*.73),.04*Math.sin(elapsed*1.27)+.04*effort,.035*Math.sin(elapsed*.91));
      rig.rotation.set(-.12+.018*Math.sin(elapsed*.91),.12*Math.sin(elapsed*.53),-.025*Math.cos(elapsed*.73));
      anatomy.update(elapsed,false,effort);
    }else if(!wasPaused)anatomy.update(elapsed,true,effort);
    wasPaused=state.paused;
    controls.update();renderer.render(scene,camera);
  }
  rig.rotation.x=-.12;
  requestAnimationFrame(animate);
  renderer.domElement.addEventListener('webglcontextlost',e=>{e.preventDefault();host.dataset.error='WebGL context lost. Reload to restore the fly.';});
}

export async function brainScene(host,state) {
 const {renderer,camera,scene,controls}=stage(host,true);
 const atlas=await resource('/fly/data/neurons.json');
 // Optional prerecorded native telemetry; failures leave the dim anatomy usable.
 const recording=await Promise.all([resource('/fly/data/recorded-activity.json'),resource('/fly/data/recorded-activity.bin','arrayBuffer')]).then(([metadata,bytes])=>({
   metadata,frames:metadata.clips.flatMap(clip=>Array.from({length:metadata.framesPerClip},(_,frame)=>new Uint8Array(bytes,clip.byteOffset+frame*metadata.samples,metadata.samples)))
 })).catch(()=>null);
 state.recordedActivityAvailable=!!recording;
 const n=atlas.indices.length,positions=new Float32Array(n*3),colors=new Float32Array(n*3),levels=new Float32Array(n),nextLevels=new Float32Array(n);
 const bounds=new T.Box3();for(const p of atlas.positions)bounds.expandByPoint(new T.Vector3(...p));
 const center=bounds.getCenter(new T.Vector3()),extent=bounds.getSize(new T.Vector3());const scale=3.8/Math.max(extent.x,extent.y,extent.z);
 const palette=[new T.Color('#70b9c5'),new T.Color('#b5c7a0'),new T.Color('#f0ab65')];
 for(let i=0;i<n;i++) {const p=atlas.positions[i];positions.set([(p[0]-center.x)*scale,-(p[1]-center.y)*scale,-(p[2]-center.z)*scale],i*3);palette[atlas.groups[i]].toArray(colors,i*3);}
 const geometry=new T.BufferGeometry();geometry.setAttribute('position',new T.BufferAttribute(positions,3));geometry.setAttribute('color',new T.BufferAttribute(colors,3));geometry.setAttribute('activity',new T.BufferAttribute(levels,1));geometry.setAttribute('nextActivity',new T.BufferAttribute(nextLevels,1));
 const uniforms={frameMix:{value:0},pixelRatio:{value:renderer.getPixelRatio()}};
 const material=new T.ShaderMaterial({transparent:true,depthWrite:false,vertexColors:true,blending:T.AdditiveBlending,uniforms,
  vertexShader:`
    attribute float activity;
    attribute float nextActivity;
    uniform float frameMix;
    uniform float pixelRatio;
    varying vec3 vColor;
    varying float a;
    void main(){
      a=clamp(mix(activity,nextActivity,frameMix),0.0,1.0);
      vColor=mix(color,vec3(1.0,.96,.8),a*.5);
      gl_Position=projectionMatrix*modelViewMatrix*vec4(position,1.0);
      gl_PointSize=(1.8+4.5*sqrt(a))*pixelRatio;
    }`,
  fragmentShader:`
    varying vec3 vColor;
    varying float a;
    void main(){
      float d=length(gl_PointCoord-0.5)*2.0;
      if(d>1.0)discard;
      float core=1.0-smoothstep(0.0,1.0,d);
      // A visible floor for every measured soma, including unsampled cells.
      gl_FragColor=vec4(vColor*(.65+a*2.0),(.20+a*.65)*core);
    }`});
 const cloud=new T.Points(geometry,material);scene.add(cloud);camera.position.set(0,.1,6);controls.update();
 host.querySelector('.scene-loading')?.remove();let lastFrame=-2,lastTrace=null,previous=null,playback=0;
 function animate(now) {
  requestAnimationFrame(animate);
  if(document.hidden){previous=null;return;}
  const dt=previous===null?0:Math.min((now-previous)/1000,.05);previous=now;
  const waiting=state.thinking&&!state.paused&&recording;
  // Sequential measured responses, slowed for viewing. No spatial waves, random
  // spikes, or invented activity: each frame is the native runtime's telemetry.
  if(waiting)playback+=dt*1000/recording.metadata.displayFrameMs;
  const trace=waiting?recording.frames:state.trace;
  const progress=waiting?playback%trace.length:trace?(state.paused?7:Math.max(0,Math.min(7,(now-state.traceStart)/170))):0;
  const frame=trace?Math.floor(progress):-1;
  const nextFrame=waiting?(frame+1)%trace.length:Math.min(7,frame+1);
  if(trace!==lastTrace||frame!==lastFrame){
   for(let i=0;i<n;i++){
    const sampled=trace&&atlas.indices[i]%8===0,index=atlas.indices[i]/8;
    levels[i]=sampled?(trace[frame]?.[index]??0)/255:0;
    nextLevels[i]=sampled?(trace[nextFrame]?.[index]??0)/255:0;
   }
   geometry.attributes.activity.needsUpdate=true;geometry.attributes.nextActivity.needsUpdate=true;lastFrame=frame;lastTrace=trace;
  }
  uniforms.frameMix.value=progress-Math.floor(progress);
  controls.update();renderer.render(scene,camera);
 }requestAnimationFrame(animate);
}
