#!/usr/bin/env python3
"""Run native suites inside private Box display/session/configuration context."""
import argparse, os, pathlib, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--build',default='build');args=p.parse_args()
with tempfile.TemporaryDirectory(prefix='uma-tests-') as d:
    root=pathlib.Path(d);env=os.environ.copy()
    for key in ('DISPLAY','WAYLAND_DISPLAY','WAYLAND_SOCKET','DBUS_SESSION_BUS_ADDRESS','DBUS_SESSION_BUS_PID','XAUTHORITY','QT_QPA_PLATFORMTHEME','QT_PLUGIN_PATH'):
        env.pop(key,None)
    for key,name in [('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache'),('XDG_RUNTIME_DIR','runtime'),('XDG_STATE_HOME','state')]:
        (root/name).mkdir(mode=0o700);env[key]=str(root/name)
    env.update(UMA_ISOLATED_TEST='1',QT_QPA_PLATFORM='offscreen',XDG_CURRENT_DESKTOP='KDE',ASAN_OPTIONS='detect_leaks=0')
    subprocess.run(['dbus-run-session','--','ctest','--test-dir',args.build,'--output-on-failure'],env=env,check=True,timeout=60)
    if args.build=='build':
        subprocess.run(['dbus-run-session','--','python3','tests/kde_dbus_test.py'],env=env,check=True,timeout=40)
