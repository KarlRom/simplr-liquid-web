// Ensure the browser-side PCM export produces valid, little-endian stereo WAV.
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
const src=readFileSync(new URL('../web/web-adapter.js',import.meta.url),'utf8');
const begin=src.indexOf('function encodeWav('),end=src.indexOf('\nfunction downloadWav()',begin);
assert(begin>=0&&end>begin,'WAV encoder exists in shipped adapter');
const encodeWav=Function('Blob',src.slice(begin,end)+'\nreturn encodeWav;')(Blob);
const chunks=[Float32Array.of(0,-1,1,0.5),Float32Array.of(-0.5,0.25)];
const wav=Buffer.from(await encodeWav(chunks,48000).arrayBuffer());
assert.equal(wav.toString('ascii',0,4),'RIFF');
assert.equal(wav.toString('ascii',8,12),'WAVE');
assert.equal(wav.toString('ascii',36,40),'data');
assert.equal(wav.readUInt32LE(4),wav.length-8);
assert.equal(wav.readUInt16LE(22),2); // stereo
assert.equal(wav.readUInt32LE(24),48000);
assert.equal(wav.readUInt16LE(34),16); // signed PCM
assert.equal(wav.readUInt32LE(40),12); // three stereo frames
assert.equal(wav.readInt16LE(44),0);
assert.equal(wav.readInt16LE(46),-32768);
assert.equal(wav.readInt16LE(48),32767);
assert.equal(wav.readInt16LE(50),16384);
console.log('PASS WAV export: RIFF header, stereo, 48 kHz, 16-bit PCM, signed samples');
