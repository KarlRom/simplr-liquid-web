// Node emulates only the AudioWorklet host API to test actual processor message/render protocol.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {runInThisContext} from 'node:vm';
let Processor;
const events=[];
globalThis.sampleRate=48000;
globalThis.AudioWorkletProcessor=class {constructor(){this.port={onmessage:null,postMessage:(message)=>events.push(message)};}};
globalThis.registerProcessor=(name,constructor)=>{assert.equal(name,'liquid-processor');Processor=constructor;};
runInThisContext(readFileSync(new URL('../web/audio-worklet.js',import.meta.url),'utf8'));
const p=new Processor();
const wasm=readFileSync(new URL('../web/liquid-dsp.wasm',import.meta.url));
await p.command({type:'init',bytes:wasm});
assert(events.find(m=>m.type==='ready'));
const input=new Float32Array(48000*2);
for(let i=0;i<48000;i++){input[2*i]=input[2*i+1]=Math.sin(i/48000*440*Math.PI*2)*.35;}
await p.command({type:'sample',pcm:input,request:9});
assert(events.find(m=>m.type==='sample-ready'&&m.request===9));
await p.command({type:'params',values:{density:64,size:175,spray:.47,gain:.45,play:1}});
let sum=0;
for(let i=0;i<480;i++){
 const l=new Float32Array(128),r=new Float32Array(128);
 assert.equal(p.process([],[ [l,r] ]),true);
 for(const v of l)sum+=v*v;
}
assert(sum>1,'Render should produce non-silent audio');
const telemetry=events.find(m=>m.type==='telemetry');
assert(telemetry&&telemetry.started>0,'Should deliver granular telemetry');
await p.command({type:'start-record'});
for(let i=0;i<96;i++)p.process([],[[new Float32Array(128),new Float32Array(128)]]);
await p.command({type:'stop-record'});
const capture=events.filter(x=>x.type==='capture-chunk');
assert(capture.length>0,'Should stream output capture chunks');
assert(capture.reduce((n,x)=>n+x.data.length,0)===96*128*2);
assert(events.find(x=>x.type==='record-stopped'));
console.log(JSON.stringify({passed:true,energy:sum.toFixed(1),telemetry:telemetry.started,
 records:96*128,captureParts:capture.length,events:events.length},null,2));
