const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const context={window:{}};vm.runInNewContext(fs.readFileSync(require('node:path').join(__dirname,'../web/encoder-fluid.js'),'utf8'),context);
const {create,step,geometry}=context.window.LiquidEncoderFluid;
for(const hz of [30,60,120]){
 const p=create(0);p.target=100;
 for(let i=0;i<hz*4;i++){step(p,1/hz);assert(p.x>=0&&p.x<=100);const g=geometry(p);assert(!/NaN|Infinity/.test(g.path));assert(g.head.every(Number.isFinite));}
 assert(Math.abs(p.x-100)<.001);assert(p.energy<.001);
 p.target=0;step(p,.1);assert(p.v<0);assert(p.energy>0);
 for(let i=0;i<hz*5;i++)step(p,1/hz);
 assert(Math.abs(p.x)<.001);assert(p.surface.every(v=>Math.abs(v)<.001));
}
const p=create(50);let seed=7;
for(let i=0;i<5000;i++){seed=(seed*16807)%2147483647;p.target=seed%101;step(p,i%3===0?.1:1/60);assert(p.x>=0&&p.x<=100);assert(!/NaN|Infinity/.test(geometry(p).path));assert(p.surface.every(v=>Math.abs(v)<10));}
p.target=82;step(p,.01,true);assert.equal(p.x,82);assert.equal(p.v,0);assert.equal(p.energy,0);assert(p.surface.every(v=>v===0));
// Equivalent elapsed time produces the same fixed-step response.
const samples=[30,60,120].map(hz=>{const p=create(20);p.target=80;for(let i=0;i<hz/2;i++)step(p,1/hz);return p;});
for(const p of samples){assert(Math.abs(p.x-samples[0].x)<.02);assert(Math.abs(p.energy-samples[0].energy)<.02);}
console.log('PASS liquid encoders: settling, reversals, bounds, 30/60/120 Hz timing, long-frame stability, reduced motion');
