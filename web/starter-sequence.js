/* A fresh, repeatable four-bar performance on each visit; playback is opt-in. */
(()=>{
 function build(random=Math.random){
  const step=250,duration=8000,onsets=[];
  for(let i=0;i<32;i++){
   if(i>2 && i%8!==0 && random()<.18)continue;
   const pick=random();
   const letter=i<3?['g','h','k'][i]:i%8===0?'g':pick<.5?'g':pick<.82?'h':'k';
   const t=i*step+(i===0?0:Math.round(random()*22));
   onsets.push({letter,t,gate:Math.round(140+random()*290)});
  }
  const notes=[];
  for(let i=0;i<onsets.length;i++){
   const note=onsets[i],next=onsets.slice(i+1).find(n=>n.letter===note.letter);
   const end=Math.min(note.t+note.gate,next?next.t-12:duration-40);
   notes.push({letter:note.letter,kind:'on',t:note.t},{letter:note.letter,kind:'off',t:end});
  }
  notes.sort((a,b)=>a.t-b.t || (a.kind==='off'?-1:1));
  return {notes,duration};
 }
 window.LiquidStarterSequence={build};
})();
