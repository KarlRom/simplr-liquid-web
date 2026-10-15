/* SIMPL_R Liquid C++ DSP / WASM AudioWorklet.
 * Microphone input is fed into Engine::pushLive before render, at the native
 * AudioContext rate. No network requests or real-time allocation for input DSP.
 */
class LiquidProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.x=null;this.ready=false;this.blocks=0;
    this.recording=false;this.recorded=0;this.capture=null;this.captureCount=0;
    this.live=false;this.liveFrames=0;this.liveLimit=0;
    this.liveParts=null;this.liveCount=0;this.inputLevel=0;
    this.monitor=false;this.monitorGain=0.6;
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
      if(this.live)throw new Error('Stop live capture before loading a sample');
      const frameCount=m.pcm.length/2;
      const ptr=x.app_prepare_sample(frameCount);
      if(!ptr)throw new Error('Sample too large for the working window');
      new Float32Array(x.memory.buffer,ptr,m.pcm.length).set(m.pcm);
      x.app_commit_sample();this.port.postMessage({type:'sample-ready',request:m.request});
    }
    else if(m.type==='live-start') {
      if(this.live)throw new Error('Live capture already running');
      // Match the native 30-second ring, allocated once on control message.
      this.liveLimit=Math.floor(sampleRate*30);x.app_live_start(this.liveLimit);
      this.live=true;this.liveFrames=0;this.liveCount=0;
      this.liveParts=new Float32Array(8192*2);this.inputLevel=0;
      this.port.postMessage({type:'live-started',limit:this.liveLimit});
    }
    else if(m.type==='live-stop')this.stopLive(false);
    else if(m.type==='monitor'){
      this.monitor=!!m.enabled;
      this.monitorGain=Number.isFinite(+m.gain)?Math.max(0,Math.min(1,+m.gain)):0.6;
    }
    else if(m.type==='start-record') {
      this.recorded=0;this.capture=new Float32Array(8192*2);this.captureCount=0;this.recording=true;
    }
    else if(m.type==='stop-record') {
      this.recording=false;this.flushCapture();this.port.postMessage({type:'record-stopped'});
    }
  }
  stopLive(automatic) {
    if(!this.live)return;
    this.live=false;this.flushLive();
    this.x.app_live_stop();
    this.port.postMessage({type:'live-stopped',automatic,frames:this.liveFrames});
  }
  flushLive() {
    if(!this.liveCount)return;
    const data=this.liveParts.slice(0,this.liveCount*2);
    this.port.postMessage({type:'live-chunk',data},[data.buffer]);
    this.liveCount=0;
  }
  flushCapture() {
    if(!this.captureCount)return;
    const data=this.capture.slice(0,this.captureCount*2);
    this.port.postMessage({type:'capture-chunk',data},[data.buffer]);
    this.captureCount=0;
  }
  process(inputs,outputs) {
    const out=outputs[0];if(!out)return true;
    if(!this.ready){for(const channel of out)channel.fill(0);return true;}
    const x=this.x;
    const left=out[0],right=out[1]||out[0];
    const input=inputs[0];
    const micL=input?.[0],micR=input?.[1]||micL;
    if(this.live&&micL?.length){
      const frames=Math.min(128,micL.length,this.liveLimit-this.liveFrames);
      if(frames>0){
        // Sample feed is chronological; native DSP wraps its circular buffer.
        const pointer=x.app_live_input_ptr();
        const pcm=new Float32Array(x.memory.buffer,pointer,256);
        let sum=0;
        for(let i=0;i<frames;i++){
          const l=Number.isFinite(micL[i])?micL[i]:0;
          const r=Number.isFinite(micR[i])?micR[i]:0;
          pcm[i*2]=l;pcm[i*2+1]=r;
          sum+=(l*l+r*r)*.5;
          const pos=this.liveCount*2;
          this.liveParts[pos]=l;this.liveParts[pos+1]=r;
          if(++this.liveCount===8192)this.flushLive();
        }
        x.app_live_push(frames);
        this.liveFrames+=frames;
        this.inputLevel=this.inputLevel*.83+Math.sqrt(sum/frames)*.17;
      }
      if(this.liveFrames>=this.liveLimit)this.stopLive(true);
    }else this.inputLevel*=.92;
    x.app_render();
    const audio=new Float32Array(x.memory.buffer,x.app_audio_ptr(),256);
    for(let i=0;i<128;i++){
      const dryL=(this.live&&this.monitor&&micL?.length)?(micL[i]||0)*this.monitorGain:0;
      const dryR=(this.live&&this.monitor&&micR?.length)?(micR[i]||0)*this.monitorGain:0;
      const l=Math.tanh(audio[i*2]+dryL),r=Math.tanh(audio[i*2+1]+dryR);
      left[i]=l;if(out[1])right[i]=r;
      if(this.recording){
        const ix=this.captureCount*2;this.capture[ix]=l;this.capture[ix+1]=r;
        if(++this.captureCount===8192)this.flushCapture();
        this.recorded++;
      }
    }
    if(++this.blocks%18===0){
      const n=x.app_visual_count(),view=new DataView(x.memory.buffer,x.app_visual_ptr(),n*32);
      const grains=[];
      for(let i=0;i<n;i++){
        const offset=i*32;grains.push([view.getFloat32(offset,true),view.getFloat32(offset+4,true),
          view.getFloat32(offset+8,true),view.getFloat32(offset+12,true),view.getUint32(offset+16,true),
          view.getFloat32(offset+20,true),view.getFloat32(offset+24,true),view.getInt32(offset+28,true)]);
      }
      this.port.postMessage({type:'telemetry',grains,active:x.app_active(),started:x.app_started(),
        dropped:x.app_dropped(),stolen:x.app_stolen(),preLimiterOvers:x.app_overs(),preLimiterPeak:x.app_peak(),
        inputLevel:this.inputLevel,liveFrames:this.liveFrames});
    }
    return true;
  }
}
const PARAMS=Object.fromEntries([
'density','grainBloom','grainTiming','bloomVariation','maxGrains','size','spray','pitch','position','gain','attack','release','noteRelease','jitter','pitchrandom',
'travel','speed','spread','fxType','fxMix','fxTime','fxFeedback','shimmerAmount','shimmerTone','shimmerInterval','start','end','keyboardMode','sliceSize',
'bpm','seqPlay','metro','clickVolume','delayDivision','densityMode','freeze','play','direction','shape','bloomCurve','organicPosition','ripple'
].map((s,i)=>[s,i]));
registerProcessor('liquid-processor',LiquidProcessor);
