#!/usr/bin/env python3
"""Verify native macro exports with Input Remapper 2.2.0 on Box; no injection."""
import json, pathlib, subprocess, tempfile
from inputremapper.injection.macros.parse import Parser
from inputremapper.configs.preset import Preset
keys=list('ABCDEFGHIJKLMNOPQRSTUVWXYZ')+list('0123456789')+['F'+str(n) for n in range(1,13)]+['Enter','Escape','Backspace','Tab','Space','Left','Right','Up','Down','Home','End','PageUp','PageDown','Delete','Insert','Ctrl','Shift','Alt','Super']
for key in keys:
    events=[{'type':'down','key':key},{'type':'delay','ms':100},{'type':'up','key':key}]
    r=subprocess.run(['./build/universal-mouse-app','--cli','--action','macro-validate'],input=json.dumps({'events':events}),text=True,capture_output=True,check=True)
    expression=json.loads(r.stdout)['expression'];assert Parser.parse(expression) is not None
    with tempfile.TemporaryDirectory() as d:
        path=pathlib.Path(d)/'preset.json';path.write_text(json.dumps([{'input_combination':[{'type':1,'code':275}],'target_uinput':'keyboard','output_symbol':expression,'macro_key_sleep_ms':0}]))
        p=Preset(path);p.load();assert len(p)==1
print(json.dumps({'remapper_version':'2.2.0','native_key_sequences':len(keys),'parsed_presets':len(keys),'input_injection':0}))
