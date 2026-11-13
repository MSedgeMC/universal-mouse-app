// Main-process integration with a separately installed Input Remapper service.
import {execFile} from 'node:child_process';
import {promisify} from 'node:util';
import * as disk from 'node:fs/promises';
import {constants} from 'node:fs';
import {homedir} from 'node:os';
import path from 'node:path';
import {randomUUID} from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {remapperPreset,macroRequest} from './macro-contract.js';
const execute=promisify(execFile);
const runDefault=async(exe,args)=>(await execute(exe,args,{timeout:6000,maxBuffer:32768,encoding:'utf8'})).stdout;
const stateHelper=fileURLToPath(new URL('../scripts/remapper-state.py',import.meta.url));
export function createLinuxMacros({fs=disk,run=runDefault,home=homedir(),env=process.env,uid=process.getuid?.()}={}){
  const executable='/usr/bin/input-remapper-control';
  const base=env.XDG_CONFIG_HOME&&path.isAbsolute(env.XDG_CONFIG_HOME)?env.XDG_CONFIG_HOME:path.join(home,'.config');
  const root=path.join(base,'resa-gm320','input-remapper');
  let pending=null,active=null;
  async function exclusive(action){
    if(pending)throw Error('A Linux macro operation is already running.');
    let finish;pending=new Promise(resolve=>{finish=resolve;});
    try{return await action();}finally{pending=null;finish();}
  }
  async function state(name){
    const result=JSON.parse(await run('/usr/bin/python3',['-I',stateHelper,name]));
    if(!['UNKNOWN','STARTING','RUNNING','FAILED','STOPPED','NO_GRAB','UPGRADE_EVDEV'].includes(result.state))throw Error('Invalid Input Remapper state.');
    return result.state;
  }
  async function waitState(name,transient){
    let result;
    for(let attempt=0;attempt<12;attempt++){
      result=await state(name);
      if(!transient.includes(result))break;
      await new Promise(resolve=>setTimeout(resolve,100));
    }
    return result;
  }
  async function stopActive(){
    if(!active)return;
    await run(executable,['--command','stop','--device',active.name,'--config-dir',root]);
    const result=await waitState(active.name,['STARTING','RUNNING']);
    if(['STARTING','RUNNING'].includes(result))throw Error(`Input Remapper is still ${result}. Try Stop Linux macro again.`);
    active=null;
  }
  async function privateDirectory(folder){
    await fs.mkdir(folder,{recursive:true,mode:0o700});
    const info=await fs.lstat(folder);
    if(!info.isDirectory()||info.uid!==uid||info.mode&0o022||await fs.realpath(folder)!==folder)throw Error('Macro storage must be a directory owned by you without symlinks.');
  }
  async function writePrivate(file,value){
    const temporary=path.join(path.dirname(file),`.write-${randomUUID()}`);
    let handle;
    try{
      handle=await fs.open(temporary,constants.O_WRONLY|constants.O_CREAT|constants.O_EXCL|constants.O_NOFOLLOW,0o600);
      await handle.writeFile(JSON.stringify(value,null,2)+'\n');await handle.sync();await handle.close();handle=null;
      try{const existing=await fs.lstat(file);if(!existing.isFile()||existing.uid!==uid||existing.mode&0o077)throw Error('Existing macro file is not private.');}catch(error){if(error.code!=='ENOENT')throw error;}
      await fs.rename(temporary,file);
    }finally{await handle?.close();await fs.unlink(temporary).catch(error=>{if(error.code!=='ENOENT')throw error;});}
  }
  async function status(){
    let installed=true;
    try{await fs.access(executable,constants.X_OK);}catch{installed=false;}
    return {installed,active:active!==null,profile:active?.profile??null,
      note:installed?'Linux macros run through Input Remapper. Use Apply Linux macro to enable a preset.':'Install Input Remapper to enable Linux button macros.'};
  }
  async function device(){
    // Group names are exact identifiers; this mouse's kernel name ends in a space.
    const names=(await run(executable,['--list-devices'])).split('\n').map(n=>n.replace(/\r$/,'')).filter(n=>n.trim());
    const matches=names.filter(n=>/^(?:INSTANT USB GAMING MOUSE|USB GAMING MOUSE)(?:\s+\d+)?\s*$/.test(n));
    if(matches.length!==1)throw Error('Input Remapper must detect exactly one INSTANT USB GAMING MOUSE. Reconnect it and retry.');
    return matches[0];
  }
  async function apply(input){
    const request=macroRequest(input),preset=remapperPreset(request);
    if(uid===0)throw Error('Run the mouse app as your normal desktop user.');
    return exclusive(async()=>{
      if(!(await status()).installed)throw Error('Input Remapper is not installed.');
      const name=await device(),folder=path.join(root,'presets',name),presetName=`GM320 Profile ${request.profile+1}`;
      await privateDirectory(root);await privateDirectory(path.join(root,'presets'));await privateDirectory(folder);
      await writePrivate(path.join(root,'config.json'),{version:'2.2.0',autoload:{}});
      await writePrivate(path.join(folder,presetName+'.json'),preset);
      // No shell, arbitrary executable, raw HID command or autoload change.
      active={name,profile:request.profile};
      try{
        await run(executable,['--command','start','--device',name,'--preset',presetName,'--config-dir',root]);
        const result=await waitState(name,['UNKNOWN','STARTING']);
        if(result!=='RUNNING')throw Error(`Input Remapper did not start (${result}).`);
      }catch(error){
        try{await stopActive();}catch(stopError){throw Error(`${error.message} ${stopError.message}`);}
        throw Error(`${error.message} Normal button behavior was restored.`);
      }
      return {status:'active',profile:request.profile,note:'Linux macro enabled for the selected button. It is not stored on the mouse.'};
    });
  }
  async function stop({wait=false}={}){
    if(wait&&pending)await pending;
    return exclusive(async()=>{await stopActive();return {status:'stopped'};});
  }
  return {status,apply,stop};
}
