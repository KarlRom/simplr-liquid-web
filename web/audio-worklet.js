/* SIMPL_R Liquid audio renderer — original C++ Engine compiled to WebAssembly.
   One processor owns its Wasm memory, sample and DSP state. No network calls or shared DSP state. */
class LiquidProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x=null;this.ready=false;this.blocks=0;
    this.recording=false;this.recorded=0;this.capture=null;this.captureCount=0;
    this.port.onmessage=e=>{this.command(e.data).catch(err=>this.port.postMessage({type:'error',message:String(err)}));};
  }
  async command(m) {
    if(m.type==='init') {
      const env={atexit:()=>0,pow:Math.pow,round:Math.round,fmod:(a,b)=>a%b,
        cos:Math.cos,sin:Math.sin,log:Math.log,exp:Math.exp,sqrt:Math.sqrt,tanh:Math.tanh};
      const result=await WebAssembly.instantiate(m.bytes,{env});
      this.x=result.instance.exports;
      this.x.app_init(sampleRate);this.ready=true;
      this.port.postMessage({type:'ready',rate:sampleRate});return;
    }
    if(!this.ready)return;
    const x=this.x;
    if(m.type==='params') {
      for(const [name,value] of Object.entries(m.values)) {
        const id=PARAMS[name];if(id!==undefined&&Number.isFinite(+value))x.app_param(id,+value);
      }
    }
    else if(m.type==='note')x.app_note(m.index,m.on?1:0);
    else if(m.type==='step')x.app_step(m.step,m.mask);
    else if(m.type==='sample') {
      const frameCount=m.pcm.length/2;
      const ptr=x.app_prepare_sample(frameCount);
      if(!ptr)throw new Error('Sample too large for the working window');
      new Float32Array(x.memory.buffer,ptr,m.pcm.length).set(m.pcm);
      x.app_commit_sample();this.port.postMessage({type:'sample-ready',request:m.request});
    }
    else if(m.type==='start-record') {
      this.recorded=0;this.capture=new Float32Array(8192*2);this.captureCount=0;this.recording=true;
    }
    else if(m.type==='stop-record') {this.recording=false;this.flushCapture();this.port.postMessage({type:'record-stopped'});}
  }
  flushCapture() {
    if(!this.captureCount)return;
    const chunk=this.capture.slice(0,this.captureCount*2);
    this.port.postMessage({type:'capture-chunk',data:chunk},[chunk.buffer]);
    this.captureCount=0;
  }
  process(_inputs,outputs) {
    const out=outputs[0];if(!out)return true;
    if(!this.ready){for(const channel of out)channel.fill(0);return true;}
    const x=this.x;
    x.app_render();
    const audio=new Float32Array(x.memory.buffer,x.app_audio_ptr(),256);
    const left=out[0],right=out[1]||out[0];
    for(let i=0;i<128;i++){
      const l=audio[i*2],r=audio[i*2+1];left[i]=l;
      if(out[1])right[i]=r;
      if(this.recording&&this.recorded<sampleRate*30){
        const ix=this.captureCount*2;this.capture[ix]=l;this.capture[ix+1]=r;
        this.captureCount++;this.recorded++;
        if(this.captureCount===8192)this.flushCapture();
      }
    }
    if(this.recording&&this.recorded>=sampleRate*30){this.recording=false;this.flushCapture();this.port.postMessage({type:'record-stopped'});}
    if(++this.blocks%18===0){
      const n=x.app_visual_count(),raw=new Float32Array(x.memory.buffer,x.app_visual_ptr(),n*5);
      // Struct GrainVisual is four float32 values and one uint32 slot.
      const slotView=new DataView(x.memory.buffer,x.app_visual_ptr(),n*20);
      const grains=[];
      for(let i=0;i<n;i++){const j=i*5;grains.push([raw[j],raw[j+1],raw[j+2],raw[j+3],slotView.getUint32(i*20+16,true)]);}
      this.port.postMessage({type:'telemetry',grains,active:x.app_active(),started:x.app_started(),
        dropped:x.app_dropped(),stolen:x.app_stolen(),preLimiterOvers:x.app_overs(),preLimiterPeak:x.app_peak()});
    }
    return true;
  }
}
const PARAMS=Object.fromEntries([
'density','grainBloom','grainTiming','bloomVariation','maxGrains','size','spray','pitch','position','gain','attack','release','noteRelease','jitter','pitchrandom',
'travel','speed','spread','fxType','fxMix','fxTime','fxFeedback','shimmerAmount','shimmerTone','shimmerInterval','start','end','keyboardMode','sliceSize',
'bpm','seqPlay','metro','clickVolume','delayDivision','densityMode','freeze','play','direction','shape','bloomCurve'
].map((s,i)=>[s,i]));
registerProcessor('liquid-processor',LiquidProcessor);
