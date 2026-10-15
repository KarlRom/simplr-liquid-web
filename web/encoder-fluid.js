/* Visual-only liquid dynamics. Audio parameters always use the exact input value. */
(()=>{
 const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
 const point=(t,r)=>{const a=(135+270*t)*Math.PI/180;return [75+r*Math.cos(a),75+r*Math.sin(a)];};
 function create(value=50){return {target:value,x:value,v:0,energy:0,lean:0,accumulator:0,surface:Array(33).fill(0),velocity:Array(33).fill(0)};}
 function step(p,elapsed,reduced=false){
  p.target=clamp(p.target,0,100);
  if(reduced){p.x=p.target;p.v=p.energy=p.lean=p.accumulator=0;p.surface.fill(0);p.velocity.fill(0);return;}
  // Fixed substeps keep touch, mouse, and 30/60/120 Hz displays consistent.
  p.accumulator+=clamp(elapsed,0,.1);
  const h=1/120;
  while(p.accumulator>=h){
   p.accumulator-=h;
   const previous=p.v;
   p.v+=(22*22*(p.target-p.x)-2*.84*22*p.v)*h;
   p.x+=p.v*h;
   if(p.x<0||p.x>100){p.x=clamp(p.x,0,100);p.v=0;}
   const speed=clamp(p.v/220,-1,1),impulse=clamp((p.v-previous)/22,-1,1);
   p.energy+=(Math.abs(speed)-p.energy)*(1-Math.exp(-h*9));
   p.lean+=(speed-p.lean)*(1-Math.exp(-h*12));
   const acceleration=[];
   for(let i=0;i<33;i++){
    const y=p.surface[i],left=p.surface[Math.max(0,i-1)],right=p.surface[Math.min(32,i+1)];
    const distance=i/32-p.x/100,local=Math.exp(-distance*distance/.012);
    // Neighbor coupling supplies surface tension; damping supplies viscosity.
    acceleration[i]=100*(left+right-2*y)-70*y-13*p.velocity[i]+local*(impulse*100+speed*24);
   }
   for(let i=0;i<33;i++){p.velocity[i]+=acceleration[i]*h;p.surface[i]+=p.velocity[i]*h;}
  }
  if(Math.abs(p.x-p.target)<.001&&Math.abs(p.v)<.01){p.x=p.target;p.v=0;}
 }
 function smoothClosed(points){
  const f=v=>v.toFixed(2),n=points.length;let d=`M${f(points[0][0])},${f(points[0][1])}`;
  for(let i=0;i<n;i++){
   const a=points[(i+n-1)%n],b=points[i],c=points[(i+1)%n],e=points[(i+2)%n];
   d+=` C${f(b[0]+(c[0]-a[0])/6)},${f(b[1]+(c[1]-a[1])/6)} ${f(c[0]-(e[0]-b[0])/6)},${f(c[1]-(e[1]-b[1])/6)} ${f(c[0])},${f(c[1])}`;
  }
  return d+' Z';
 }
 function geometry(p){
  const t=clamp(p.x/100,0,1),outer=[],inner=[];
  const count=Math.max(3,Math.ceil(t*48));
  const direction=Math.sign(p.lean)||1;
  for(let i=0;i<=count;i++){
   const q=t*i/count,index=q*32,lo=Math.floor(index),mix=index-lo;
   const wave=p.surface[lo]*(1-mix)+p.surface[Math.min(32,lo+1)]*mix;
   const fromHead=t-q;
   // A rounded meniscus thickens at the moving edge and draws a narrow neck
   // behind it. Reverse turns reverse the displacement without detached dots.
   const bulge=Math.exp(-Math.pow(fromHead/.045,2));
   const neck=Math.exp(-Math.pow((fromHead-.07)/.04,2));
   const width=4+p.energy*(3.2*bulge-1.1*neck);
   const radius=52+wave+direction*p.energy*1.8*Math.exp(-fromHead/.12);
   outer.push(point(q,radius+width));inner.push(point(q,radius-width));
  }
  const at=t*32,lo=Math.floor(at),mix=at-lo;
  const radius=52+p.surface[lo]*(1-mix)+p.surface[Math.min(32,lo+1)]*mix+direction*p.energy*1.8;
  return {path:t>.001?smoothClosed(outer.concat(inner.reverse())):'',head:point(t,radius),radius:4+p.energy*3.2};
 }
 window.LiquidEncoderFluid={create,step,geometry};
})();
