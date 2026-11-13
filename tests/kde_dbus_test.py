#!/usr/bin/env python3
"""Run only inside a private Box test bus. Exercise actual native D-Bus adapter."""
import json, os, subprocess, sys
import dbus, dbus.service, dbus.mainloop.glib
from gi.repository import GLib
assert os.environ.get('UMA_ISOLATED_TEST')=='1'
state={'pointerAcceleration':0.25,'scrollFactor':1.0};failed_once=False
class Manager(dbus.service.Object):
    @dbus.service.method('org.kde.KWin.InputDeviceManager',out_signature='as')
    def ListPointers(self):return ['event2']
class Pointer(dbus.service.Object):
    @dbus.service.method('org.freedesktop.DBus.Properties',in_signature='s',out_signature='a{sv}')
    def GetAll(self,iface):return {'vendor':dbus.UInt32(0x30fa),'product':dbus.UInt32(0x1440),'pointer':True,'name':'INSTANT USB GAMING MOUSE ',**state}
    @dbus.service.method('org.freedesktop.DBus.Properties',in_signature='ssv')
    def Set(self,iface,key,value):
        global failed_once
        if key=='scrollFactor' and float(value)==2.0 and not failed_once:
            failed_once=True;raise dbus.exceptions.DBusException('Simulated scroll rejection',name='org.kde.KWin.TestError')
        state[key]=float(value)
if '--service' in sys.argv:
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    # Recreate the connection after setting its GLib event dispatcher.
    bus=dbus.SessionBus(private=True);name=dbus.service.BusName('org.kde.KWin',bus=bus)
    m=Manager(bus,'/org/kde/KWin/InputDevice');p=Pointer(bus,'/org/kde/KWin/InputDevice/event2')
    print('ready',flush=True);GLib.MainLoop().run()
else:
    service=subprocess.Popen([sys.executable,__file__,'--service'],stdout=subprocess.PIPE,text=True)
    try:
        assert service.stdout.readline().strip()=='ready'
        def call(action,request=None,ok=True):
            r=subprocess.run(['./build/universal-mouse-app','--cli','--action',action],input='' if request is None else json.dumps(request),capture_output=True,text=True)
            assert (r.returncode==0)==ok,(r.stdout,r.stderr)
            return json.loads(r.stdout) if ok else r.stderr
        before=call('desktop');assert before['pointer'] and before['native_wheel'],before
        updated=call('desktop-apply',dict(revision=before['revision'],gain=0.5,scroll=1.5,enabled=True))
        assert updated['gain']==0.5 and updated['scroll']==1.5
        call('desktop-apply',dict(revision=before['revision'],gain=0,scroll=1,enabled=True),ok=False)
        call('desktop-apply',dict(revision=updated['revision'],gain=-0.5,scroll=2,enabled=True),ok=False)
        restored=call('desktop');assert restored['gain']==0.5 and restored['scroll']==1.5,restored
        print(json.dumps({'kde_dbus':'read/write/readback/stale/rollback passed','real_desktop_writes':0}))
    finally:
        service.terminate();service.wait(timeout=5)
