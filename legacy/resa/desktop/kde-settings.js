import {execFile} from 'node:child_process';
import {fileURLToPath} from 'node:url';
import {sessionRequest,pointerGain} from './session-settings.js';
export const scrollFactors=[0.25,0.4,0.55,0.7,0.85,1,1.25,1.5,1.75,2,2.5];
const helper=fileURLToPath(new URL('../scripts/kwin-settings.py',import.meta.url));
const runDefault=(action,request)=>new Promise((resolve,reject)=>{
  const child=execFile('/usr/bin/python3',['-I',helper,action],{timeout:5000,maxBuffer:8192,encoding:'utf8'},(error,stdout,stderr)=>{
    if(error){reject(Error(stderr.trim().slice(0,500)||'KDE settings are unavailable.'));return;}
    try{resolve(JSON.parse(stdout));}catch{reject(Error('Invalid KDE settings response.'));}
  });child.stdin.on('error',()=>{});child.stdin.end(JSON.stringify(request));
});
export function createKdeSettings({run=runDefault}={}){
  let busy=false;
  const adapt=r=>{
    if(!r||!/^[a-f0-9]{64}$/.test(r.revision)||!Number.isFinite(r.pointer_gain)||Math.abs(r.pointer_gain)>1||!Number.isFinite(r.scroll_factor)||r.scroll_factor<=0)throw Error('Invalid KWin mouse response.');
    const wheel=scrollFactors.reduce((best,n,i)=>Math.abs(n-r.scroll_factor)<Math.abs(scrollFactors[best]-r.scroll_factor)?i:best,0)+1;
    return {status:r.status,revision:r.revision,settings:{pointer_gain:r.pointer_gain,pointer_level:Math.round(r.pointer_gain*5+6),wheel_multiplier:wheel,wheel_enabled:true,wheel_factor:r.scroll_factor},
      capabilities:{pointer:true,wheel:true,wheel_native:true,pointer_note:'Applies to your GM320 pointer in KDE. Stored by KWin.',wheel_note:'Native KDE scroll factor for your GM320. Persists after this app closes.'}};
  };
  async function read(){try{return adapt(await run('read',{}));}catch(error){return {status:'read',revision:'0'.repeat(64),settings:{pointer_gain:0,pointer_level:6,wheel_multiplier:6,wheel_enabled:true,wheel_factor:1},capabilities:{pointer:false,wheel:false,wheel_native:true,pointer_note:error.message,wheel_note:error.message}};}}
  async function apply(input){
    const request=sessionRequest(input);if(busy)throw Error('KDE settings update is already running.');busy=true;
    try{
      const c=request.changes,changes={};
      if('pointer_level'in c)changes.pointer_gain=pointerGain(c.pointer_level);
      if('wheel_multiplier'in c){if(!c.wheel_enabled)throw Error('Native KDE scrolling does not need a helper.');changes.scroll_factor=scrollFactors[c.wheel_multiplier-1];}
      return adapt(await run('apply',{expected_revision:request.expected_revision,changes}));
    }finally{busy=false;}
  }
  return {read,apply,close(){}};
}
