#!/usr/bin/env python3
"""Install this reviewed local build; no administrator access or GUI launch."""
from pathlib import Path
import os
import shutil
import time
source=Path(__file__).resolve().parents[1]
target=Path.home()/'.local/lib/resa-gm320'
if not (source/'node_modules/electron/dist/electron').is_file() or not (source/'bin/resa').is_file():
    raise SystemExit('Build the app and install Electron before deploying')
target.parent.mkdir(parents=True,exist_ok=True)
if target.exists():
    backup=Path.home()/'.local/state/kubuntu-customization/gm320'/('app-backup-'+time.strftime('%Y%m%d-%H%M%S'))
    backup.parent.mkdir(parents=True,exist_ok=True);target.rename(backup)
shutil.copytree(source,target,symlinks=True,ignore=shutil.ignore_patterns('.pnpm-store','__pycache__','verification','gui','*.log','gm320-discovery.json','gm320-settings-read.json','desktop-preview.html'))
launch=target/'launch.sh'
launch.write_text('#!/bin/sh\nset -eu\ncd "'+str(target)+'"\nexec env -u ELECTRON_RUN_AS_NODE ./node_modules/electron/dist/electron --ozone-platform=wayland . "$@"\n')
launch.chmod(0o755)
apps=Path.home()/'.local/share/applications';apps.mkdir(parents=True,exist_ok=True)
desktop=apps/'resa-gm320.desktop'
desktop.write_text('[Desktop Entry]\nType=Application\nName=GM320 Mouse Settings\nComment=Resa adapted for Ant Esports GM320 on Linux\nExec='+str(launch)+'\nIcon='+str(target/'assets/gm320.svg')+'\nTerminal=false\nCategories=Settings;HardwareSettings;\nStartupWMClass=Resa GM320\n')
print('Installed app:',target)
print('Application menu entry:',desktop)
