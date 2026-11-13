import test from 'node:test';
import assert from 'node:assert/strict';
import {createKdeSettings} from './kde-settings.js';
test('KDE controls map bounded UI levels to native per-mouse values, with no write during read',async()=>{
  const calls=[],revision='a'.repeat(64);
  const settings=createKdeSettings({run:async(action,input)=>{
    calls.push({action,input});return {status:action==='read'?'read':'applied',revision,pointer_gain:input.changes?.pointer_gain??0,scroll_factor:input.changes?.scroll_factor??1};
  }});
  const r=await settings.read();assert.equal(r.settings.wheel_multiplier,6);assert.equal(calls.length,1);
  const a=await settings.apply({expected_revision:revision,changes:{pointer_level:8,wheel_enabled:true,wheel_multiplier:9}});
  assert.deepEqual(calls[1],{action:'apply',input:{expected_revision:revision,changes:{pointer_gain:0.4,scroll_factor:1.75}}});
  assert.equal(a.settings.wheel_multiplier,9);
});
