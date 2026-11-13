#!/usr/bin/env python3
"""Isolated GUI smoke test and idle measurements; run on Box."""
import argparse, json, os, pathlib, subprocess, tempfile, time
p=argparse.ArgumentParser();p.add_argument('--binary', default='build/universal-mouse-app');p.add_argument('--output', default='test-results');args=p.parse_args()
root=pathlib.Path(args.output).resolve();root.mkdir(parents=True, exist_ok=True)
with tempfile.TemporaryDirectory(prefix='uma-check-') as d:
    base=pathlib.Path(d)
    env=os.environ.copy()
    for k in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','DBUS_SESSION_BUS_ADDRESS','DBUS_SESSION_BUS_PID','XAUTHORITY','QT_QPA_PLATFORMTHEME','QT_PLUGIN_PATH'):
        env.pop(k,None)
    for k,n in [('XDG_RUNTIME_DIR','runtime'),('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_STATE_HOME','state')]:
        (base/n).mkdir(mode=0o700);env[k]=str(base/n)
    env.update(UMA_ISOLATED_TEST='1',QT_QPA_PLATFORM='offscreen')
    subprocess.run(['dbus-run-session','--',args.binary,'--mock','--verify-gui',str(root/'gui')],env=env,check=True,timeout=40)
    # Measure static GUI idle independently of the optional lighting animation.
    path=base/'config/universal-mouse-app/profiles.json'
    profile=json.loads(path.read_text())
    profile['profiles'][0]['settings']['lighting']['effect']='Light On'
    path.write_text(json.dumps(profile))
    # Repeat startup with a saved non-default profile to catch destructive initialization.
    path=base/'config/universal-mouse-app/profiles.json'
    profiles=json.loads(path.read_text());profiles['profiles'][0]['settings']['polling']=500
    path.write_text(json.dumps(profiles))
    subprocess.run(['dbus-run-session','--',args.binary,'--mock','--verify-gui',str(root/'gui-restart')],env=env,check=True,timeout=40)
    assert json.loads(path.read_text())['profiles'][0]['settings']['polling']==500
    started=time.monotonic()
    process=subprocess.Popen(['dbus-run-session','--',args.binary,'--mock','--quit-after','15'],env=env,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    try:
        time.sleep(2)
        children=[]
        for entry in pathlib.Path('/proc').iterdir():
            if not entry.name.isdigit():continue
            try:
                e=(entry/'environ').read_bytes().split(b'\0')
                if ('XDG_RUNTIME_DIR='+env['XDG_RUNTIME_DIR']).encode() in e and (entry/'comm').read_text().startswith('universal-mouse'):children.append(entry)
            except (FileNotFoundError, PermissionError, ProcessLookupError):pass
        assert len(children)==1,children
        target=children[0]
        def ticks():return sum(map(int,(target/'stat').read_text().split()[13:15]))
        before=ticks();window=time.monotonic();time.sleep(5);cpu=(ticks()-before)/os.sysconf('SC_CLK_TCK')/(time.monotonic()-window)*100
        memory={}
        for line in (target/'smaps_rollup').read_text().splitlines():
            if line.startswith(('Rss:','Pss:')):memory[line.split(':')[0].lower()+'_kib']=int(line.split()[1])
        report=dict(platform='Box Ubuntu 24.04 / Qt offscreen / 2 vCPU',idle_cpu_percent_one_core=round(cpu,2),**memory)
        (root/'performance.json').write_text(json.dumps(report,indent=2));print(json.dumps(report))
        process.wait(timeout=18)
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=5)
