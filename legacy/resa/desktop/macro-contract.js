// Linux software macros use Input Remapper's documented language, not flash.
export const macroKeys = [...'ABCDEFGHIJKLMNOPQRSTUVWXYZ',...'0123456789',
  ...Array.from({length:12},(_,i)=>`F${i+1}`),
  'Enter','Escape','Backspace','Tab','Space','Left','Right','Up','Down',
  'Home','End','PageUp','PageDown','Delete','Insert','Ctrl','Shift','Alt','Super'];
const symbols = Object.fromEntries(macroKeys.map(key=>[key,
  {Enter:'KEY_ENTER',Escape:'KEY_ESC',Backspace:'KEY_BACKSPACE',Tab:'KEY_TAB',Space:'KEY_SPACE',
    Left:'KEY_LEFT',Right:'KEY_RIGHT',Up:'KEY_UP',Down:'KEY_DOWN',Home:'KEY_HOME',End:'KEY_END',
    PageUp:'KEY_PAGEUP',PageDown:'KEY_PAGEDOWN',Delete:'KEY_DELETE',Insert:'KEY_INSERT',
    Ctrl:'KEY_LEFTCTRL',Shift:'KEY_LEFTSHIFT',Alt:'KEY_LEFTALT',Super:'KEY_LEFTMETA'}[key]??`KEY_${key}`]));
export const macroTriggers = [{label:'Back / side button',code:275},
  {label:'Forward / side button',code:276},{label:'Middle click',code:274}];
export function validEvents(events){
  return Array.isArray(events)&&events.length<=64&&events.every(e=>e&&typeof e==='object'&&!Array.isArray(e)&&
    (e.type==='delay'?Object.keys(e).sort().join()==='ms,type'&&Number.isInteger(e.ms)&&e.ms>=10&&e.ms<=10000:
      ['down','up'].includes(e.type)&&Object.keys(e).sort().join()==='key,type'&&macroKeys.includes(e.key)));
}
export function macroRequest(input){
  if(!input||typeof input!=='object'||Array.isArray(input)||Object.keys(input).sort().join()!=='events,profile,trigger')throw Error('Invalid Linux macro request.');
  if(!Number.isInteger(input.profile)||input.profile<0||input.profile>3||!macroTriggers.some(t=>t.code===input.trigger)||!validEvents(input.events)||!input.events.length)throw Error('Choose a profile, button and nonempty sequence.');
  const held=new Set();let duration=0,keys=0;
  for(const event of input.events){
    if(event.type==='delay'){duration+=event.ms;continue;}
    keys++;
    if(event.type==='down'){if(held.has(event.key))throw Error(`Release ${event.key} before pressing it again.`);held.add(event.key);}
    else {if(!held.delete(event.key))throw Error(`Press ${event.key} before releasing it.`);}
  }
  if(!keys||held.size)throw Error('Every key press needs a matching release.');
  if(duration>30000)throw Error('Keep each sequence within 30 seconds.');
  return structuredClone(input);
}
export function macroExpression(events){
  macroRequest({events,profile:0,trigger:276});
  return events.map(e=>e.type==='delay'?`wait(${e.ms})`:`key_${e.type}(${symbols[e.key]})`).join('.');
}
export function remapperPreset(request){
  const r=macroRequest(request);
  return [{input_combination:[{type:1,code:r.trigger}],target_uinput:'keyboard',
    output_symbol:macroExpression(r.events),macro_key_sleep_ms:0}];
}
