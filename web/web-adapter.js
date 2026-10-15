/* Browser-local compatibility adapter for SIMPL_R v0.23.10 UI server endpoints.
   AudioWorklet handles C++ Wasm rendering; only decoded audio, commands and telemetry cross ports. */
(()=>{'use strict';
const nativeFetch=window.fetch.bind(window);
const initial={duration:0,sourceDuration:0,cacheStart:0,cacheSeconds:0,inputLevel:0,name:'BROWSER DEMO',
 peaks:[],grains:[],active:0,started:0,dropped:0,stolen:0,preLimiterOvers:0,preLimiterPeak:0,
 outputRecording:false,outputReady:false,canExport:false,recording:false,callbackAvgUs:0,callbackMaxUs:0,callbackMisses:0};
let state={...initial}, ctx=null,node=null,starting=null,source=null,sequence=Array(16).fill(0),
 displayWorker=null,displaySequence=0,displayPromises=new Map(),
  importedName='BROWSER DEMO',captureParts=[],readySample=null,sampleRequest=0,stoppedCapture=null,
  microphone=null,micNode=null,liveParts=[],stoppedLive=null,finalizingLive=null,deviceId='',
  livePeakBins=new Float32Array(1000),liveChunksFrames=0;
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
   else if(m.type==='live-chunk'){
    const base=liveChunksFrames;
    liveParts.push(m.data);
    const n=m.data.length/2;
    for(let i=0;i<n;i++){
     const bin=Math.min(999,Math.floor((base+i)*1000/(ctx.sampleRate*30)));
     const peak=Math.max(Math.abs(m.data[2*i]),Math.abs(m.data[2*i+1]));
     if(peak>livePeakBins[bin])livePeakBins[bin]=peak;
    }
    liveChunksFrames+=n;
    if(state.recording)state.peaks=Array.from(livePeakBins);
   }
   else if(m.type==='live-stopped'){
    finalizingLive=finishLive(m).then(result=>{stoppedLive?.resolve(result);stoppedLive=null;return result;})
      .catch(err=>{stoppedLive?.reject(err);stoppedLive=null;msg('CAPTURE FAILED · '+err.message);throw err;});
    finalizingLive.catch(()=>{});
   }
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
 if(state.recording)throw new Error('Stop live capture before importing audio');
 if(!ready())await init();
 const decoded=await ctx.decodeAudioData(buffer.slice(0)); // native browser WAV/MP3 decoding.
 const frames=decoded.length;
 // Decoding happens once; only up to 20 seconds is copied to the worker on each seek.
 source={length:frames,duration:decoded.duration,left:decoded.getChannelData(0),right:decoded.getChannelData(Math.min(1,decoded.numberOfChannels-1))};
 importedName='IMPORTED AUDIO';
 await uploadWindow(0);state.canExport=false;liveParts=[];
 return {ok:true,cacheStart:0,cacheSeconds:state.duration};
}
// Microphone capture is local to this tab: MediaStream → AudioWorklet input
// → original C++ Engine::pushLive() → granular processing + optional dry monitor.
function cleanupMicrophone(){
 try{micNode?.disconnect();}catch(_){}
 micNode=null;
 if(microphone){for(const track of microphone.getTracks())track.stop();microphone=null;}
}
async function refreshInputDevices(){
 if(!navigator.mediaDevices?.enumerateDevices)return;
 try{
  const devices=(await navigator.mediaDevices.enumerateDevices()).filter(d=>d.kind==='audioinput');
  for(const select of document.querySelectorAll('[data-simplr-input]')){
   const previous=deviceId||select.value;
   select.replaceChildren(new Option('DEFAULT MICROPHONE / INTERFACE',''));
   devices.forEach((d,i)=>select.add(new Option(d.label||`Audio input ${i+1}`,d.deviceId)));
   select.value=devices.some(d=>d.deviceId===previous)?previous:'';
  }
 }catch(e){msg('INPUT LIST UNAVAILABLE · '+e.message);}
}
function syncLiveControls(){
 const active=state.recording;
 for(const b of document.querySelectorAll('[data-simplr-live]')){
  b.textContent=active?'■ STOP LIVE CAPTURE':'● LIVE CAPTURE';
  b.setAttribute('aria-pressed',String(active));
 }
 const legacy=document.getElementById('record');if(legacy){legacy.textContent=active?'■ STOP CAPTURE':'● LIVE CAPTURE';legacy.setAttribute('aria-pressed',String(active));}
 const exp=document.getElementById('export');if(exp)exp.disabled=active||!state.canExport;
 for(const b of document.querySelectorAll('[data-simplr-export-live]'))b.disabled=active||!state.canExport;
 for(const select of document.querySelectorAll('[data-simplr-input]'))select.disabled=active;
 for(const el of document.querySelectorAll('[data-simplr-capture-state]'))
  el.textContent=active?`● LIVE ${state.duration.toFixed(1)} / 30.0s`:state.canExport?'✓ CAPTURE READY · 30s MAX':'INPUT IDLE · HEADPHONES RECOMMENDED';
 for(const el of document.querySelectorAll('[data-simplr-level]'))el.value=Math.max(0,Math.min(1,state.inputLevel||0));
 for(const [attr,id] of [['data-simplr-output-record','outputRecord'],['data-simplr-output-stop','outputStop'],['data-simplr-output-export','outputExport']]){
  const main=document.getElementById(id);
  for(const b of document.querySelectorAll('['+attr+']'))b.disabled=!!main?.disabled;
 }
}
async function startLive(){
 if(state.recording)return {ok:true};
 if(!navigator.mediaDevices?.getUserMedia)throw Error('Microphone requires HTTPS or localhost and a compatible browser');
 if(!ready())await init();
 if(finalizingLive)await finalizingLive;
 const device=document.querySelector('[data-simplr-input]');
 deviceId=device?.value||deviceId||'';
 let stream;
 try{
  stream=await navigator.mediaDevices.getUserMedia({audio:{
   ...(deviceId?{deviceId:{exact:deviceId}}:{}),
   channelCount:{ideal:2},echoCancellation:false,noiseSuppression:false,autoGainControl:false
  },video:false});
  microphone=stream;
  // Keep microphone disconnected until the DSP has created its live ring.
  const initialized=new Promise((resolve,reject)=>{
   const timeout=setTimeout(()=>{node.port.removeEventListener('message',on);reject(Error('Live DSP initialization timed out'));},8000);
   function on(e){if(e.data?.type==='live-started'){clearTimeout(timeout);node.port.removeEventListener('message',on);resolve();}}
   node.port.addEventListener('message',on);
  });
  node.port.postMessage({type:'live-start'});
  await initialized;
  liveParts=[];liveChunksFrames=0;livePeakBins.fill(0);state.canExport=false;state.recording=true;state.duration=0;
  state.cacheSeconds=0;state.cacheStart=0;state.sourceDuration=0;
  state.name='LIVE INPUT';state.peaks=[];state.inputLevel=0;
  micNode=ctx.createMediaStreamSource(microphone);
  micNode.connect(node);
  microphone.getAudioTracks().forEach(t=>t.addEventListener('ended',()=>{
   if(state.recording)stopLive().catch(e=>msg('INPUT DISCONNECTED · '+e.message));
  },{once:true}));
  node.port.postMessage({type:'monitor',enabled:!!document.querySelector('[data-simplr-monitor]')?.checked});
  syncLiveControls();refreshInputDevices();
  msg('LIVE GRANULATION ON · INPUT IS PROCESSED LOCALLY');
  return {ok:true};
 }catch(err){
  cleanupMicrophone();
  // If ring creation succeeded, don't leave DSP in live state after input setup failure.
  node?.port.postMessage({type:'live-stop'});
  state.recording=false;syncLiveControls();throw err;
 }
}
async function finishLive(m){
 cleanupMicrophone();state.recording=false;state.inputLevel=0;
 const total=liveChunksFrames;
 state.canExport=total>0;
 if(total){
  const left=new Float32Array(total),right=new Float32Array(total);
  let at=0;
  for(const part of liveParts){for(let j=0;j<part.length;j+=2){left[at]=part[j];right[at++]=part[j+1];}}
  source={length:total,duration:total/ctx.sampleRate,left,right};
  importedName='CAPTURED LIVE AUDIO';
  await uploadWindow(0); // preserve full raw WAV, but use the normal 20 s sample window
 }
 syncLiveControls();
 msg(total?(m.automatic?'30s LIVE CAPTURE COMPLETE · PLAYABLE SAMPLE READY':'LIVE CAPTURE SAVED · PLAYABLE SAMPLE READY'):'LIVE CAPTURE STOPPED · NO INPUT RECEIVED');
 return {ok:true,frames:total};
}
async function stopLive(){
 if(!state.recording){if(finalizingLive)await finalizingLive;return {ok:true};}
 if(stoppedLive)throw Error('Live capture already stopping');
 // Avoid leaving the microphone active if the AudioWorklet stalls.
 const wait=new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>{stoppedLive=null;cleanupMicrophone();state.recording=false;syncLiveControls();reject(Error('Live capture stop timed out'));},10000);
  stoppedLive={resolve:r=>{clearTimeout(timer);resolve(r);},reject:e=>{clearTimeout(timer);reject(e);}};
 });
 micNode?.disconnect();node.port.postMessage({type:'live-stop'});
 return wait;
}
function downloadLiveWav(){
 if(!state.canExport||!liveParts.length)return;
 downloadBlob(encodeWav(liveParts,ctx.sampleRate),'SIMPL_R_LIVE_CAPTURE.wav');
}
function downloadBlob(blob,filename){
 const url=URL.createObjectURL(blob),a=document.createElement('a');
 a.href=url;a.download=filename;document.body.appendChild(a);a.click();a.remove();
 setTimeout(()=>URL.revokeObjectURL(url),30000);
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
function downloadWav(){if(captureParts.length)downloadBlob(encodeWav(captureParts,ctx.sampleRate),'SIMPL_R_LIQUID_PROCESSED.wav');}
function renderDisplay(path,body){
 if(!displayWorker){
  displayWorker=new Worker('./display-worker.js?v=23.35');
  displayWorker.onmessage=({data:m})=>{const pending=displayPromises.get(m.id);if(!pending)return;
   displayPromises.delete(m.id);if(m.error)pending.reject(new Error(m.error));else pending.resolve(new Response(m.pixels,{headers:{'Content-Type':'application/octet-stream'}}));};
  displayWorker.onerror=e=>{for(const p of displayPromises.values())p.reject(new Error('Display worker failed: '+e.message));displayPromises.clear();};
 }
 const id=++displaySequence;const payload=body?.slice(0);
 if(!(payload instanceof ArrayBuffer))return Promise.reject(new Error('Display renderer requires an ArrayBuffer'));
 return new Promise((resolve,reject)=>{displayPromises.set(id,{resolve,reject});displayWorker.postMessage({id,profile:path.endsWith('sharp')?0:1,payload},[payload]);});
}
async function backend(url,options={}){
 const u=new URL(url,location.href),q=u.searchParams;
 if(!ready()&&!['/status','/grains','/record/start','/record/stop','/upload','/display/sharp','/display/waveshare'].includes(u.pathname)){
  return reply({error:'Click ENABLE AUDIO first'},503);
 }
 switch(u.pathname){
 case '/display/sharp':case '/display/waveshare':return renderDisplay(u.pathname,options.body);
 case '/status':return reply({...state});
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
  if(state.recording)return reply({error:'Stop capture before seeking'},409);
  try{return reply(await uploadWindow(+q.get('seconds')||0));}catch(e){return reply({error:e.message},400);}
 }
 case '/output/start':{
  if(state.outputRecording)return reply({error:'Already recording output'},409);
  captureParts=[];state.outputReady=false;state.outputRecording=true;node.port.postMessage({type:'start-record'});return reply({ok:true});
 }
 case '/output/stop':{
  if(!state.outputRecording)return reply({error:'Not recording'},400);
  await new Promise(resolve=>{stoppedCapture=resolve;node.port.postMessage({type:'stop-record'});});
  return reply({ok:true});
 }
 case '/record/start':{
  try{return reply(await startLive());}catch(e){return reply({error:e.message},400);}
 }
 case '/record/stop':{
  try{return reply(await stopLive());}catch(e){return reply({error:e.message},400);}
 }
 default:return null;
 }
}
window.fetch=function(input,options){
 if(typeof input==='string'&&input.startsWith('/'))return backend(input,options).then(x=>x??nativeFetch(input,options));
 return nativeFetch(input,options);
};
// The original button uses /output.wav, which cannot be served on a static host.
window.addEventListener('DOMContentLoaded',()=>{
 const buttons=[...document.querySelectorAll('[data-simplr-audio-enable]')];
 for(const btn of buttons){
  btn.style.cssText='background:#111;color:#fff;border:2px solid #111;white-space:nowrap;';
  btn.addEventListener('click',async()=>{
   for(const b of buttons){b.disabled=true;b.textContent='INITIALIZING DSP…';}
   try{await init();for(const b of buttons)b.textContent='◉ AUDIO ACTIVE';}
   catch(_){for(const b of buttons){b.disabled=false;b.textContent='▶ RETRY AUDIO';}}
  });
 }
 const capture=document.getElementById('record'),exportBtn=document.getElementById('export');
 const toggleLive=async()=>{
  const button=state.recording?'■ STOPPING…':'● REQUESTING MICROPHONE…';
  document.querySelectorAll('[data-simplr-live]').forEach(b=>b.textContent=button);
  try{state.recording?await stopLive():await startLive();}
  catch(e){msg('LIVE CAPTURE · '+e.message);}
  finally{syncLiveControls();}
 };
 for(const btn of document.querySelectorAll('[data-simplr-live]'))
  btn.addEventListener('click',toggleLive);
 if(capture)capture.addEventListener('click',e=>{e.stopImmediatePropagation();e.preventDefault();toggleLive();},true);
 if(exportBtn)exportBtn.addEventListener('click',e=>{e.stopImmediatePropagation();e.preventDefault();downloadLiveWav();},true);
 for(const btn of document.querySelectorAll('[data-simplr-export-live]'))btn.addEventListener('click',downloadLiveWav);
 for(const [attr,id] of [['data-simplr-output-record','outputRecord'],['data-simplr-output-stop','outputStop'],['data-simplr-output-export','outputExport']]){
  for(const btn of document.querySelectorAll('['+attr+']'))btn.addEventListener('click',()=>document.getElementById(id)?.click());
 }
 for(const select of document.querySelectorAll('[data-simplr-input]'))select.addEventListener('change',e=>{
  deviceId=e.target.value;
  for(const other of document.querySelectorAll('[data-simplr-input]'))other.value=deviceId;
 });
 for(const check of document.querySelectorAll('[data-simplr-monitor]'))check.addEventListener('change',e=>{
  for(const other of document.querySelectorAll('[data-simplr-monitor]'))other.checked=e.target.checked;
  node?.port.postMessage({type:'monitor',enabled:e.target.checked,gain:.6});
  msg(e.target.checked?'DRY MONITOR ON · USE HEADPHONES TO PREVENT FEEDBACK':'DRY MONITOR OFF · GRANULAR OUTPUT ONLY');
 });
 if(navigator.mediaDevices?.addEventListener)
  navigator.mediaDevices.addEventListener('devicechange',refreshInputDevices);
 refreshInputDevices();syncLiveControls();
 // Redirect the processed-WAV export to a client-side encoder.
 const processed=document.getElementById('outputExport');
 if(processed)processed.addEventListener('click',e=>{e.stopImmediatePropagation();e.preventDefault();downloadWav();},true);
 // Ensure AudioContext is allowed to resume after returning to a background tab.
 document.addEventListener('pointerdown',()=>{if(ctx?.state==='suspended')ctx.resume().catch(()=>{});},{passive:true});
 setInterval(()=>{if(state.recording){state.duration=(state.liveFrames||0)/(ctx?.sampleRate||48000);
    state.sourceDuration=state.duration;state.cacheSeconds=state.duration;}
   syncLiveControls();},250);
 window.addEventListener('pagehide',()=>{if(state.recording)node?.port.postMessage({type:'live-stop'});cleanupMicrophone();});
});
window.SIMPLR_WEB={init,get state(){return state;},get context(){return ctx;},get node(){return node;},startLive,stopLive,downloadWav,downloadLiveWav};
})();
