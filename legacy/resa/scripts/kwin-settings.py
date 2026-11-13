#!/usr/bin/env python3
"""Typed KWin settings for only the connected 30fa:1440 pointer."""
import dbus
import hashlib
import json
import sys
INTERFACE='org.kde.KWin.InputDevice'
ROOT='/org/kde/KWin/InputDevice'
def main():
    action=sys.argv[1] if len(sys.argv)==2 else ''
    request=json.loads(sys.stdin.read(4097))
    bus=dbus.SessionBus()
    manager=dbus.Interface(bus.get_object('org.kde.KWin',ROOT),'org.kde.KWin.InputDeviceManager')
    matches=[]
    for name in manager.ListPointers():
        if not str(name).startswith('event') or not str(name)[5:].isdigit():continue
        props=dbus.Interface(bus.get_object('org.kde.KWin',ROOT+'/'+str(name)),'org.freedesktop.DBus.Properties')
        values=props.GetAll(INTERFACE)
        if int(values['vendor'])==0x30fa and int(values['product'])==0x1440 and bool(values['pointer']) and str(values['name']).strip() in ('INSTANT USB GAMING MOUSE','FANTECH FANTECH-X16v2'):
            matches.append((props,values,str(name)))
    if len(matches)!=1:raise ValueError('KWin must expose exactly one matching mouse pointer')
    props,values,name=matches[0]
    def snapshot():
        gain=float(props.Get(INTERFACE,'pointerAcceleration'))
        factor=float(props.Get(INTERFACE,'scrollFactor'))
        state={'name':name,'pointer_gain':gain,'scroll_factor':factor}
        token=hashlib.sha256(json.dumps(state,sort_keys=True).encode()).hexdigest()
        return {'status':'read','revision':token,**state}
    before=snapshot()
    if action=='read':
        if request!={}:raise ValueError('Read accepts no settings')
        print(json.dumps(before));return
    if action!='apply' or not isinstance(request,dict) or set(request)!={'expected_revision','changes'} or request['expected_revision']!=before['revision']:
        raise ValueError('KDE mouse settings changed; reload before applying')
    changes=request['changes']
    if not isinstance(changes,dict) or not changes or set(changes)-{'pointer_gain','scroll_factor'}:raise ValueError('Invalid KDE settings')
    for key,value in changes.items():
        low,high=(-1,1) if key=='pointer_gain' else (0.25,2.5)
        if type(value) not in (int,float) or not low<=value<=high:raise ValueError('KDE value is outside its supported range')
    attempted=[]
    properties={'pointer_gain':'pointerAcceleration','scroll_factor':'scrollFactor'}
    try:
        for key,value in changes.items():
            attempted.append(key);props.Set(INTERFACE,properties[key],dbus.Double(value))
        after=snapshot()
        if any(abs(after[key]-value)>1e-8 for key,value in changes.items()):raise ValueError('KDE settings readback did not match')
        after['status']='applied';print(json.dumps(after))
    except Exception:
        for key in reversed(attempted):props.Set(INTERFACE,properties[key],dbus.Double(before[key]))
        raise
if __name__=='__main__':
    try:main()
    except Exception as error:print(str(error),file=sys.stderr);sys.exit(1)
