/* Browser-local compatibility adapter for SIMPL_R v0.23.10 UI server endpoints.
   AudioWorklet handles C++ Wasm rendering; only decoded audio, commands and telemetry cross ports. */
(()=>{'use strict';
const nativeFetch=window.fetch.bind(window);
const initial={duration:0,sourceDuration:0,cacheStart:0,cacheSeconds:0,name:'BROWSER DEMO',
 peaks:[],grains:[],active:0,started:0,dropped:0,stolen:0,preLimiterOvers:0,preLimiterPeak:0,
 outputRecording:false,outputReady:false,canExport:false,recording:false,callbackAvgUs:0,callbackMaxUs:0,callbackMisses:0};
let state={...initial}, ctx=null,node=null,starting=null,source=null,sequence=Array(16).fill(0),
  importedName='BROWSER DEMO',captureParts=[],readySample=null,sampleRequest=0,stoppedCapture=null;
const LIMIT_SECONDS=20;
const msg=t=>{const el=document.getElementById('msg');if(el)el.textContent=t;};
const ready=()=>node!==null;
const reply=(body={},status=200)=>Promise.resolve(new Response(JSON.stringify(body),{status,headers:{'Content-Type':'application/json'}}));
function waveform(left,right,frames){const peaks=[];const size=1000;for(let p=0;p<size;p++){
 const start=Math.floor(p*frames/size),end=Math.min(frames,Math.ceil((p+1)*frames/size));let a=0;
 for(let j=start;j<end;j++){a=Math.max(a,Math.abs(left[j]),Math.abs(right[j]));}peaks.push(a);
 }return peaks;}
function makeDemo(rate){const length=Math.floor(rate*4),l=new Float32Array(length),r=new Float32Array(length);
 for(let i=0;i<length;i++){
  const t=i/rate;const fade=Math.min(1,t*8, (4-t)*2);const low=Math.sin(t*2*Math.PI*110)*.14;
  const pad=Math.sin(t*2*Math.PI*(196+3*Math.sin(t*0.4)))*.12;
  const pulse=(Math.sin(t*2*Math.PI*1.2)>0?1:-1)*Math.sin(t*2*Math.PI*330)*.055;
  const a=(low+pad+pulse+Math.sin(t*2*Math.PI*440)*.05)*fade;
  l[i]=a;r[i]=a*.88+Math.sin(t*2*Math.PI*224)*.028*fade;
 }return {length,duration:4,left:l,right:r};}
function uploadWindow(seconds){
 if(!source||!ready())return Promise.reject(new Error('Audio not initialized'));
 const rate=ctx.sampleRate;
 const offset=Math.max(0,Math.min(source.length-2,Math.floor(seconds*rate)));
 const frames=Math.min(source.length-offset,Math.floor(LIMIT_SECONDS*rate));
 const interleaved=new Float32Array(frames*2);
 // source was decoded on this browser's main thread; only the active window reaches Wasm.
 for(let i=0;i<frames;i++){interleaved[i*2]=source.left[offset+i];interleaved[i*2+1]=source.right[offset+i];}
 const request=++sampleRequest;
 state.duration=frames/rate;state.cacheStart=offset/rate;state.cacheSeconds=frames/rate;
 state.sourceDuration=source.duration;state.name=importedName;
 state.peaks=waveform(source.left.subarray(offset,offset+frames),source.right.subarray(offset,offset+frames),frames);
 return new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>{if(readySample?.request===request){readySample=null;reject(new Error('Wasm sample load timed out'));}},8000);
  readySample={request,resolve:()=>{clearTimeout(timer);resolve({ok:true,cacheStart:state.cacheStart,cacheSeconds:state.cacheSeconds});}};
  node.port.postMessage({type:'sample',pcm:interleaved,request},[interleaved.buffer]);
 });
}
async function init(){
 if(starting)return starting;
 starting=(async()=>{
  if(!window.AudioWorkletNode)throw new Error('AudioWorklet is not supported in this browser');
  ctx=new (window.AudioContext||window.webkitAudioContext)({latencyHint:'interactive'});
  // Request activation within the original click gesture (Safari/Chrome requirement).
  const activate=ctx.resume();activate.catch(()=>{});
  await ctx.audioWorklet.addModule('./audio-worklet.js');
  const bytes=await nativeFetch('./liquid-dsp.wasm').then(r=>{if(!r.ok)throw Error('Could not load liquid-dsp.wasm');return r.arrayBuffer();});
  node=new AudioWorkletNode(ctx,'liquid-processor',{outputChannelCount:[2]});
  node.port.onmessage=e=>{
   const m=e.data;
   if(m.type==='ready'){
    state.rate=m.rate; window.dispatchEvent(new Event('simplr:ready'));
   } else if(m.type==='telemetry')Object.assign(state,m);
   else if(m.type==='sample-ready'&&readySample?.request===m.request){readySample.resolve();readySample=null;}
   else if(m.type==='capture-chunk')captureParts.push(m.data);
   else if(m.type==='record-stopped'){
    state.outputRecording=false;state.outputReady=captureParts.length>0;
    if(stoppedCapture){stoppedCapture();stoppedCapture=null;}
   }else if(m.type==='error'){msg('AUDIO ERROR: '+m.message);}
  };
  node.connect(ctx.destination);
  node.port.postMessage({type:'init',bytes},[bytes]);
  await new Promise((resolve,reject)=>{
    if(state.rate){resolve();return;}
    const to=setTimeout(()=>{window.removeEventListener('simplr:ready',on);reject(Error('DSP initialization timed out'));},8000);
    const on=()=>{clearTimeout(to);resolve();};window.addEventListener('simplr:ready',on,{once:true});
  });
  source=makeDemo(ctx.sampleRate);importedName='BROWSER DEMO';
  await uploadWindow(0);
  await activate;
  window.sendImmediate?.();
  node.port.postMessage({type:'params',values:{density:64,size:175,spray:.47,gain:.45,play:1}});
  msg('AUDIO ON · ORIGINAL C++ ENGINE (WASM) · LOCAL PLAYBACK');
  return true;
 })().catch(e=>{starting=null;msg('AUDIO START FAILED · '+e.message);throw e;});
 return starting;
}
async function loadFile(buffer){
 if(!ready())await init();
 const decoded=await ctx.decodeAudioData(buffer.slice(0)); // native browser WAV/MP3 decoding.
 const frames=decoded.length;
 // Decoding happens once; only up to 20 seconds is copied to the worker on each seek.
 source={length:frames,duration:decoded.duration,left:decoded.getChannelData(0),right:decoded.getChannelData(Math.min(1,decoded.numberOfChannels-1))};
 importedName='IMPORTED AUDIO';
 await uploadWindow(0);state.canExport=false;state.outputReady=false;
 return {ok:true,cacheStart:0,cacheSeconds:state.duration};
}
function encodeWav(chunks,rate){
 const frames=chunks.reduce((sum,c)=>sum+c.length/2,0);
 const bytes=44+frames*4;const buffer=new ArrayBuffer(bytes),dv=new DataView(buffer);
 const str=(at,s)=>{for(let i=0;i<s.length;i++)dv.setUint8(at+i,s.charCodeAt(i));};
 str(0,'RIFF');dv.setUint32(4,bytes-8,true);str(8,'WAVE');str(12,'fmt ');dv.setUint32(16,16,true);
 dv.setUint16(20,1,true);dv.setUint16(22,2,true);dv.setUint32(24,rate,true);dv.setUint32(28,rate*4,true);
 dv.setUint16(32,4,true);dv.setUint16(34,16,true);str(36,'data');dv.setUint32(40,frames*4,true);
 let index=44;for(const chunk of chunks)for(const v of chunk){const clamped=Math.min(1,Math.max(-1,Number.isFinite(v)?v:0));
 dv.setInt16(index,clamped<0?Math.round(clamped*32768):Math.round(clamped*32767),true);index+=2;}
 return new Blob([buffer],{type:'audio/wav'});
}
function downloadWav(){if(!captureParts.length)return;const blob=encodeWav(captureParts,ctx.sampleRate),url=URL.createObjectURL(blob);
 const a=document.createElement('a');a.href=url;a.download='SIMPL_R_LIQUID_PROCESSED.wav';document.body.appendChild(a);a.click();a.remove();
 setTimeout(()=>URL.revokeObjectURL(url),30000);
}
async function backend(url,options={}){
 const u=new URL(url,location.href),q=u.searchParams;
 if(!ready()&&!['/status','/grains','/record/start','/record/stop','/upload'].includes(u.pathname)){
  return reply({error:'Click ENABLE AUDIO first'},503);
 }
 switch(u.pathname){
 case '/status':return reply({...state,canExport:false});
 case '/grains':return reply({grains:state.grains});
 case '/params':node.port.postMessage({type:'params',values:Object.fromEntries(q.entries())});return reply({ok:true});
 case '/slice':node.port.postMessage({type:'note',index:+q.get('index'),on:q.get('action')==='on'});return reply({ok:true});
 case '/step':{
  const step=+q.get('step'),note=+q.get('note');if(step<0||step>=16||note<0||note>=13)return reply({error:'Invalid step'},400);
  if(+q.get('on'))sequence[step]|=1<<note;else sequence[step]&=~(1<<note);
  node.port.postMessage({type:'step',step,mask:sequence[step]});return reply({ok:true});
 }
 case '/upload':{
  try{await loadFile(options.body);return reply({ok:true});}catch(e){return reply({error:e.message},400);}
 }
 case '/cache/seek':{
  try{return reply(await uploadWindow(+q.get('seconds')||0));}catch(e){return reply({error:e.message},400);}
 }
 case '/output/start':{
  captureParts=[];state.outputReady=false;state.outputRecording=true;node.port.postMessage({type:'start-record'});return reply({ok:true});
 }
 case '/output/stop':{
  if(!state.outputRecording)return reply({error:'Not recording'},400);
  await new Promise(resolve=>{stoppedCapture=resolve;node.port.postMessage({type:'stop-record'});});
  return reply({ok:true});
 }
 case '/record/start':case '/record/stop':return reply({error:'Live microphone capture is not included in the Web Edition'},501);
 default:return null;
 }
}
window.fetch=function(input,options){
 if(typeof input==='string'&&input.startsWith('/'))return backend(input,options).then(x=>x??nativeFetch(input,options));
 return nativeFetch(input,options);
};
// The original button uses /output.wav, which cannot be served on a static host.
window.addEventListener('DOMContentLoaded',()=>{
 const buttons=[];
 for(const [id,host] of [['webAudioEnable','header'],['webAudioEnableInstrument','#teensyPage .teensy-head']]){
  const btn=document.createElement('button');btn.id=id;btn.type='button';btn.className='btn';btn.textContent='▶ ENABLE AUDIO';
  btn.style.cssText='background:#111;color:#fff;border:2px solid #111;white-space:nowrap;';
  btn.addEventListener('click',async()=>{
   for(const b of buttons){b.disabled=true;b.textContent='INITIALIZING DSP…';}
   try{await init();for(const b of buttons)b.textContent='◉ AUDIO ACTIVE';}
   catch(_){for(const b of buttons){b.disabled=false;b.textContent='▶ RETRY AUDIO';}}
  });
  document.querySelector(host)?.append(btn);buttons.push(btn);
 }
 const capture=document.getElementById('record'),exportBtn=document.getElementById('export');
 if(capture)capture.hidden=true;if(exportBtn)exportBtn.hidden=true;
 // Redirect the processed-WAV export to a client-side encoder.
 const processed=document.getElementById('outputExport');
 if(processed)processed.addEventListener('click',e=>{e.stopImmediatePropagation();e.preventDefault();downloadWav();},true);
 // Ensure AudioContext is allowed to resume after returning to a background tab.
 document.addEventListener('pointerdown',()=>{if(ctx?.state==='suspended')ctx.resume().catch(()=>{});},{passive:true});
});
window.SIMPLR_WEB={init,get state(){return state;},get context(){return ctx;},get node(){return node;},downloadWav};
})();
