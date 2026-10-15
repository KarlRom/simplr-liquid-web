const {chromium}=require('playwright');
const fs=require('fs'),path=require('path'),assert=require('assert');
(async()=>{
 const browser=await chromium.launch({...(process.env.CHROME_PATH ? {executablePath:process.env.CHROME_PATH} : {}),headless:true});
 const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.route('http://simplr.test/**',route=>{let f=path.join(__dirname,'../web',new URL(route.request().url()).pathname);if(f.endsWith('/'))f+='index.html'; const ext=path.extname(f);route.fulfill({body:fs.readFileSync(f),contentType:({'.html':'text/html','.js':'application/javascript','.css':'text/css','.wasm':'application/wasm','.png':'image/png','.mp3':'audio/mpeg'})[ext]||'application/octet-stream'});});
 for(const [w,h] of [[390,844],[844,390],[667,375],[740,360],[1024,768],[1440,900]]){
 await page.setViewportSize({width:w,height:h});await page.goto('http://simplr.test/');
 await page.waitForSelector('#teensyPads button');
 assert.equal(await page.evaluate(()=>vals.liquidScale),1.3);
 assert.equal(await page.evaluate(()=>vals.direction),2);
 assert.equal(await page.locator('#direction').inputValue(),'2');
 assert.equal(await page.locator('.action-pad[data-action="DIRECTION"]').textContent(),'DIR: OUTWARD');
 for(const [id,value] of [['bloom','82%'],['texture','100%'],['motion','77%'],['color','100%'],['space','59%'],['pitch','0 st'],['fxsync','1/16'],['fxtype','DELAY']])assert.equal(await page.locator('#macro-'+id+' output').textContent(),value);
 if(await page.locator('#quickGuide').isVisible()){
  assert(await page.locator('#guideTitle').evaluate(e=>e===document.activeElement));
  assert(await page.locator('#quickGuide').evaluate(e=>e.clientHeight>=e.scrollHeight || getComputedStyle(e).overflowY==='auto'));
  await page.locator('#closeGuide').click();
 }
 await page.locator('#moreActions summary').click();await page.locator('#openGuide').click();
 assert(await page.locator('#quickGuide').isVisible());await page.keyboard.press('Escape');
 assert(!await page.locator('#quickGuide').isVisible());

 const data=await page.evaluate(()=>{const box=s=>{let r=document.querySelector(s).getBoundingClientRect();return {x:r.x,y:r.y,right:r.right,bottom:r.bottom}};return {left:box('.hardware-left'),right:box('.hardware-right'),knobs:document.querySelectorAll('.macro-knob').length,pads:document.querySelectorAll('#teensyPads button').length,selection:getComputedStyle(document.querySelector('.hw-pad')).userSelect,overflow:document.documentElement.scrollWidth>innerWidth,toolbar:getComputedStyle(document.querySelector('#appToolbar')).position};});
 assert.equal(data.knobs,8);assert.equal(data.pads,16);assert.equal(data.selection,'none');assert(!data.overflow,JSON.stringify(data));
 if(w<=1024){assert.equal(data.toolbar,'relative');await page.evaluate(()=>scrollTo(0,250));assert((await page.locator('#appToolbar').boundingBox()).y<0);await page.evaluate(()=>scrollTo(0,0));}
 if(w>h)assert(data.right.x>=data.left.right-1,JSON.stringify(data));else assert(data.right.y>=data.left.bottom-1);
 await page.locator('#macro-pitch').focus();await page.keyboard.press('End');assert.equal(await page.evaluate(()=>vals.pitch),12);
 await page.locator('.action-pad[data-action="DIRECTION"]').click();assert.equal(await page.evaluate(()=>vals.direction),3);
 if(process.env.SCREENSHOT_DIR)await page.screenshot({path:path.join(process.env.SCREENSHOT_DIR,`view-${w}.png`),fullPage:true});
 await page.locator('#exitTeensy').click();assert(await page.locator('#fullPage').isVisible());
 assert.equal(await page.locator('#slicePiano button').first().evaluate(e=>getComputedStyle(e).userSelect),'none');
 console.log(`PASS ${w}x${h}: layout, scroll, selection, eight macros, sixteen pads, controls, editor`);
 }

 await page.addInitScript(()=>{
 window.audioMessages=[];
 window.AudioContext=class {constructor(){this.sampleRate=48000;this.audioWorklet={addModule:async()=>{}};}async resume(){}async decodeAudioData(){return {length:480,duration:.01,numberOfChannels:1,getChannelData:()=>new Float32Array(480)}}};
 window.AudioWorkletNode=class {constructor(){this.port={postMessage:m=>{window.audioMessages.push(m);if(m.type==='init')setTimeout(()=>this.port.onmessage({data:{type:'ready',rate:48000}}),0);if(m.type==='sample')setTimeout(()=>this.port.onmessage({data:{type:'sample-ready',request:m.request}}),0)}};}connect(){}};
 });
 await page.goto('http://simplr.test/');
 assert(!await page.locator('#quickGuide').isVisible());
 await page.locator('#webAudioEnable').click();
 await page.waitForFunction(()=>document.querySelector('#webAudioEnable').textContent.includes('ACTIVE'));
 assert.equal(await page.evaluate(()=>SIMPLR_WEB.state.name),'JUNIOR_MESA.MP3');
 assert.equal(await page.evaluate(()=>playing),false);
 assert(await page.evaluate(()=>audioMessages.filter(m=>m.type==='params'&&'play' in m.values).every(m=>Number(m.values.play)===0)));
 await page.evaluate(()=>upload(new File([new Uint8Array(44)],'first.wav')));
 assert.equal(await page.evaluate(()=>playing),false);
 assert.equal(await page.locator('#play').textContent(),'PLAY');
 assert(await page.evaluate(()=>audioMessages.filter(m=>m.type==='params'&&'play' in m.values).every(m=>Number(m.values.play)===0)));
 await page.locator('#play').click();await page.waitForTimeout(150);
 assert.equal(await page.evaluate(()=>playing),true);
 await page.evaluate(()=>{audioMessages.length=0;return upload(new File([new Uint8Array(44)],'replacement.wav'))});
 await page.waitForTimeout(150);
 assert.equal(await page.evaluate(()=>playing),false);
 assert(await page.evaluate(()=>audioMessages.filter(m=>m.type==='params'&&'play' in m.values).every(m=>Number(m.values.play)===0)));
 assert.equal(await page.locator('#name').textContent(),'REPLACEMENT.WAV');
 console.log('PASS first import and replacement while playing stay stopped; explicit Play starts transport (mock audio host)');
 assert.deepEqual(errors,[]);await browser.close();
})().catch(e=>{console.error(e);process.exit(1)});
