const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const context={window:{}};vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../web/starter-sequence.js'),'utf8'),context);
for(let seed=1;seed<=100;seed++){
 let n=seed;const {notes,duration}=context.window.LiquidStarterSequence.build(()=>{n=n*16807%2147483647;return n/2147483647});
 const held=new Set(),used=new Set();let t=-1;
 assert.equal(duration,8000);
 for(const event of notes){assert(event.t>=t&&event.t>=0&&event.t<duration);t=event.t;assert(['g','h','k'].includes(event.letter));used.add(event.letter);if(event.kind==='on'){assert(!held.has(event.letter));held.add(event.letter);}else{assert(held.has(event.letter));held.delete(event.letter);}}
 assert.equal(held.size,0);assert.equal(used.size,3);assert(notes.length<=64);
}
console.log('PASS starter sequences: 100 variants, G/H/K focus, paired notes, bounded duration, no overlapping same-key gates');
