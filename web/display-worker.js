/* Native v0.23.35 C++ LCD renderer compiled to WASM, isolated in a Worker
   to protect real-time AudioWorklet playback from display frame rendering. */
let loading=null;
async function engine(){
 if(!loading)loading=(async()=>{
  const response=await fetch('./liquid-display.wasm?v=23.35');
  if(!response.ok)throw Error('Display WASM download failed ('+response.status+')');
  const bytes=await response.arrayBuffer();
  const math={cos:Math.cos,sin:Math.sin,pow:Math.pow,log:Math.log,exp:Math.exp,
    sqrt:Math.sqrt,tanh:Math.tanh,fmod:(a,b)=>a%b,round:Math.round,
    atan2:Math.atan2,hypot:Math.hypot,ceil:Math.ceil,floor:Math.floor,lroundf:Math.round,atexit:()=>0};
  const {instance}=await WebAssembly.instantiate(bytes,{env:math});
  const x=instance.exports;if(!x.display_init())throw Error('Display WASM initialization failed');return x;
 })();return loading;
}
self.onmessage=async({data:{id,profile,payload}})=>{
 try{
  const x=await engine();
  if(!(payload instanceof ArrayBuffer)||payload.byteLength>792*4||payload.byteLength<280*4)throw Error('Invalid display packet');
  const source=new Float32Array(payload);const ptr=x.display_input_ptr();
  new Float32Array(x.memory.buffer,ptr,source.length).set(source);
  if(!x.display_render(profile))throw Error('Native display render failed');
  const output=new Uint8Array(x.memory.buffer,x.display_output_ptr(profile),x.display_output_bytes(profile)).slice();
  self.postMessage({id,pixels:output.buffer},[output.buffer]);
 }catch(err){self.postMessage({id,error:String(err)});}
};
