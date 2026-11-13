import test from 'node:test';
import assert from 'node:assert/strict';
import {macroRequest,remapperPreset,macroExpression} from './macro-contract.js';
const events=[{type:'down',key:'Ctrl'},{type:'down',key:'C'},{type:'delay',ms:100},{type:'up',key:'C'},{type:'up',key:'Ctrl'}];
test('a modifier shortcut exports a balanced Input Remapper sequence',()=>{
  assert.equal(macroExpression(events),'key_down(KEY_LEFTCTRL).key_down(KEY_C).wait(100).key_up(KEY_C).key_up(KEY_LEFTCTRL)');
  const preset=remapperPreset({events,trigger:276,profile:2});
  assert.deepEqual(preset[0].input_combination,[{type:1,code:276}]);
  assert.equal(preset[0].target_uinput,'keyboard');
});
test('rejects unmatched keys, injected expressions, primary-click triggers and unbounded delays',()=>{
  for(const input of [
    {events:[{type:'down',key:'A'}],trigger:276,profile:0},
    {events:[{type:'up',key:'A'}],trigger:276,profile:0},
    {events:[{type:'down',key:'A).exec(sh)'}],trigger:276,profile:0},
    {events,trigger:272,profile:0},
    {events:Array.from({length:4},()=>({type:'delay',ms:10000})).concat(events),trigger:276,profile:0},
  ])assert.throws(()=>macroRequest(input));
});
