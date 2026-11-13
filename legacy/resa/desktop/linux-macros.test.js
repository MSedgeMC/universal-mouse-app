import test from 'node:test';
import assert from 'node:assert/strict';
import * as fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import {createLinuxMacros} from './linux-macros.js';
const events=[{type:'down',key:'A'},{type:'delay',ms:100},{type:'up',key:'A'}];
test('writes private app presets and starts/stops only the selected mouse with fixed CLI arguments',async()=>{
  const home=await fs.mkdtemp(path.join(os.tmpdir(),'gm320-macros-'));
  const calls=[];let state='RUNNING';
  const fakeFS={...fs,access:async()=>{}};
  const macros=createLinuxMacros({fs:fakeFS,home,env:{},run:async(exe,args)=>{
    if(exe==='/usr/bin/python3')return JSON.stringify({state});
    assert.equal(exe,'/usr/bin/input-remapper-control');calls.push(args);
    if(args[1]==='stop')state='STOPPED';
    return args[0]==='--list-devices'?'Some keyboard\nINSTANT USB GAMING MOUSE \n':'';
  }});
  try{
    const result=await macros.apply({events,profile:1,trigger:275});assert.equal(result.status,'active');
    const root=path.join(home,'.config/resa-gm320/input-remapper');
    assert.deepEqual(calls[1],['--command','start','--device','INSTANT USB GAMING MOUSE ','--preset','GM320 Profile 2','--config-dir',root]);
    const preset=path.join(root,'presets/INSTANT USB GAMING MOUSE /GM320 Profile 2.json');
    assert.equal((await fs.stat(preset)).mode&0o777,0o600);
    assert.equal(JSON.parse(await fs.readFile(preset,'utf8'))[0].output_symbol,'key_down(KEY_A).wait(100).key_up(KEY_A)');
    assert.equal((await macros.status()).active,true);await macros.stop();assert.equal((await macros.status()).active,false);
    assert.equal(calls[2][1],'stop');
  }finally{await fs.rm(home,{recursive:true,force:true});}
});
test('a failed injector cannot be reported as a working macro even if its CLI returns zero',async()=>{
 const home=await fs.mkdtemp(path.join(os.tmpdir(),'gm320-macros-')),calls=[];
 const macros=createLinuxMacros({fs:{...fs,access:async()=>{}},home,env:{},run:async(exe,args)=>{
   calls.push(args);if(exe==='/usr/bin/python3')return '{"state":"FAILED"}';
   return args[0]==='--list-devices'?'INSTANT USB GAMING MOUSE\n':'';
 }});
 try{await assert.rejects(macros.apply({events,profile:0,trigger:275}),/did not start/);assert.equal((await macros.status()).active,false);assert(calls.some(args=>args[1]==='stop'));}
 finally{await fs.rm(home,{recursive:true,force:true});}
});
test('an interrupted start still stops the possibly running injector',async()=>{
  const home=await fs.mkdtemp(path.join(os.tmpdir(),'gm320-macros-')),calls=[];
  const macros=createLinuxMacros({fs:{...fs,access:async()=>{}},home,env:{},run:async(exe,args)=>{
    calls.push(args);
    if(exe==='/usr/bin/python3')return '{"state":"STOPPED"}';
    if(args[0]==='--list-devices')return 'INSTANT USB GAMING MOUSE \n';
    if(args[1]==='start')throw Error('CLI timed out');
    return '';
  }});
  try{
    await assert.rejects(macros.apply({events,profile:0,trigger:275}),/CLI timed out.*restored/);
    assert(calls.some(args=>args[1]==='stop'));assert.equal((await macros.status()).active,false);
  }finally{await fs.rm(home,{recursive:true,force:true});}
});
test('shutdown waits for an in-flight start before stopping its injector',async()=>{
  const home=await fs.mkdtemp(path.join(os.tmpdir(),'gm320-macros-'));
  let releaseStart,signalStart,state='RUNNING';
  const started=new Promise(resolve=>{signalStart=resolve;}),gate=new Promise(resolve=>{releaseStart=resolve;});
  const macros=createLinuxMacros({fs:{...fs,access:async()=>{}},home,env:{},run:async(exe,args)=>{
    if(exe==='/usr/bin/python3')return JSON.stringify({state});
    if(args[0]==='--list-devices')return 'INSTANT USB GAMING MOUSE \n';
    if(args[1]==='start'){signalStart();await gate;}
    if(args[1]==='stop')state='STOPPED';
    return '';
  }});
  try{
    const applying=macros.apply({events,profile:0,trigger:275});await started;
    const stopping=macros.stop({wait:true});releaseStart();
    assert.equal((await applying).status,'active');assert.equal((await stopping).status,'stopped');
    assert.equal((await macros.status()).active,false);assert.equal(state,'STOPPED');
  }finally{releaseStart();await fs.rm(home,{recursive:true,force:true});}
});
test('Stop cannot report restoration while the service still reports RUNNING',async()=>{
  const home=await fs.mkdtemp(path.join(os.tmpdir(),'gm320-macros-'));let state='RUNNING';
  const macros=createLinuxMacros({fs:{...fs,access:async()=>{}},home,env:{},run:async(exe,args)=>{
    if(exe==='/usr/bin/python3')return JSON.stringify({state});
    return args[0]==='--list-devices'?'INSTANT USB GAMING MOUSE \n':'';
  }});
  try{
    await macros.apply({events,profile:0,trigger:275});
    await assert.rejects(macros.stop(),/still RUNNING/);assert.equal((await macros.status()).active,true);
    state='STOPPED';await macros.stop();assert.equal((await macros.status()).active,false);
  }finally{await fs.rm(home,{recursive:true,force:true});}
});
test('ambiguous devices and a symlinked app directory are rejected before starting injection',async()=>{
  const home=await fs.mkdtemp(path.join(os.tmpdir(),'gm320-macros-'));
  try{
    const ambiguous=createLinuxMacros({fs:{...fs,access:async()=>{}},home,env:{},run:async()=> 'INSTANT USB GAMING MOUSE \nINSTANT USB GAMING MOUSE  2\n'});
    await assert.rejects(ambiguous.apply({events,profile:0,trigger:275}),/exactly one/);
    await fs.mkdir(path.join(home,'.config'));await fs.mkdir(path.join(home,'target'));
    await fs.symlink(path.join(home,'target'),path.join(home,'.config/resa-gm320'));
    const macros=createLinuxMacros({fs:{...fs,access:async()=>{}},home,env:{},run:async()=> 'INSTANT USB GAMING MOUSE\n'});
    await assert.rejects(macros.apply({events,profile:0,trigger:275}),/without symlinks/);
  }finally{await fs.rm(home,{recursive:true,force:true});}
});
