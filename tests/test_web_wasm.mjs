// node tests/test_web_wasm.mjs — verifies the shipped binary really runs original C++ DSP.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {performance} from 'node:perf_hooks';
const wasm=readFileSync(new URL('../web/liquid-dsp.wasm',import.meta.url));
const env={atexit:()=>0,pow:Math.pow,round:Math.round,fmod:(a,b)=>a%b,cos:Math.cos,
  sin:Math.sin,log:Math.log,exp:Math.exp,sqrt:Math.sqrt,tanh:Math.tanh};
const {instance}=await WebAssembly.instantiate(wasm,{env});
const e=instance.exports;
assert.equal(e.app_init(48000),1);
const frames=48000;
const ptr=e.app_prepare_sample(frames);
assert(ptr>0);
const data=new Float32Array(e.memory.buffer,ptr,frames*2);
for(let i=0;i<frames;i++){
 const a=.38*Math.sin(i*2*Math.PI*220/48000);
 data[2*i]=a;data[2*i+1]=a;
}
e.app_commit_sample();
e.app_param(0,64); // density
e.app_param(9,.8); // gain
e.app_param(36,1); // ambient
let energy=0,peak=0;
const start=performance.now();
for(let block=0;block<1500;block++){
 if(block===300){e.app_param(18,6);e.app_param(19,.8);e.app_param(22,.6);e.app_param(20,.4);}
 if(block===500){e.app_note(3,1);e.app_note(7,1);}
 if(block===600){e.app_note(3,0);e.app_note(7,0);}
 if(block===700){e.app_step(0,1<<3);e.app_param(30,1);}
 if(block===950)e.app_param(30,0);
 if(block===1100){e.app_param(18,5);e.app_param(33,2);}
 e.app_render();
 const audio=new Float32Array(e.memory.buffer,e.app_audio_ptr(),256);
 for(let i=0;i<256;i++){const v=audio[i];assert(Number.isFinite(v));energy+=v*v;peak=Math.max(peak,Math.abs(v));}
}
const elapsed=performance.now()-start;
assert(energy>0.01,'DSP must generate audible audio');
assert(e.app_started()>10,'DSP must schedule granular voices');
assert(e.app_visual_count()>0,'DSP must provide liquid-grain visuals');
assert(peak<=1,'Processed audio must be bounded');
console.log(JSON.stringify({passed:true,energy:Math.round(energy),peak:+peak.toFixed(4),started:e.app_started(),active:e.app_active(),
  visualCount:e.app_visual_count(),renderedSeconds:(1500*128/48000),elapsedMs:+elapsed.toFixed(1),
  ratioToRealtime:+(elapsed/(1500*128/48000*1000)).toFixed(3)},null,2));
