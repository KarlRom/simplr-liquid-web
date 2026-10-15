/* A native modal keeps focus inside the guide and supports Escape. */
(()=>{
 const guide=document.querySelector('#quickGuide');
 const preference=document.querySelector('#guideRemember');
 const key='simplr-quick-guide-v1';
 const open=()=>{document.querySelector('#moreActions').open=false;guide.showModal();};
 document.querySelector('#openGuide').addEventListener('click',open);
 document.querySelector('#closeGuide').addEventListener('click',()=>{
  guide.close();
  // Keep activation in this tap so browsers can unlock the audio context.
  const enable=document.querySelector('#webAudioEnable');
  if(enable&&!enable.disabled)enable.click();
 });
 guide.addEventListener('close',()=>{
  try{if(preference.checked)localStorage.setItem(key,'dismissed');else localStorage.removeItem(key);}catch(_){}
  document.querySelector('#moreActions summary').focus({preventScroll:true});
 });
 let dismissed=false;try{dismissed=localStorage.getItem(key)==='dismissed';}catch(_){}
 preference.checked=dismissed||preference.checked;
 if(!dismissed)open();
})();
