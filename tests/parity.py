"""Run on Box: compare native merge bytes/errors against the retained Python oracle."""
import importlib.util,json,random,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('worker',root/'legacy/resa/internal/device/worker.py')
w=importlib.util.module_from_spec(spec);spec.loader.exec_module(w)
spec=importlib.util.spec_from_file_location('tests',root/'legacy/resa/internal/device/worker_test.py')
t=importlib.util.module_from_spec(spec);spec.loader.exec_module(t)
binary=root/'build/universal-mouse-app'
base=t.BASE
cases=[]
for rate in (125,250,500,1000,True,'500',2000):cases.append((base,{'polling':rate}))
for field in w.BUTTON_SLOTS:
    for action in w.BUTTONS:
        before=dict(base);before[0x44]=1;before[0x46]=1
        cases.append((before,{field:action}))
for flags in (0,32,96,224):
    for action in w.BUTTONS:
        before=dict(base);before[0x44]=flags|5;before[0x46]=1
        cases.append((before,{'button1':action}))
for v in (0,1,5,6,7,True,-1):cases.append((base,{'brightness_raw':v}))
for v in (1,8,50,150,255,256,300,301,True):cases.append((base,{'double_click_ms':v}))
for e in (*w.MODE_NAMES,'Button Response','Unknown'):
    for direction in w.DIRECTIONS:cases.append((base,{'lighting':{'effect':e,'direction':direction}}))
for n in range(8):cases.append((base,{'lighting':{'speed_raw':n}}))
rng=random.Random(1440)
for n in range(100):
    before=dict(base)
    for a,mask in w.MASKS.items():before[a]=(before[a]&mask)|(rng.randrange(256)&~mask)
    cases.append((before,{'dpi':{'values':[rng.choice(w.DPIS) for _ in range(6)],'enabled':[True]*6,'stage':rng.randrange(6)},'lighting':{'palette':[[rng.randrange(16) for _ in range(3)] for _ in range(7)],'response_enabled':[bool(rng.randrange(2)) for _ in range(7)],'responses':[rng.choice(w.RESPONSES) for _ in range(7)],'symmetry':bool(rng.randrange(2))}}))
cases.extend([(base,{'button1':'Disabled'}),(base,{'lighting':{'enabled':[False]*7}}),(base,{'bad':1}),(base,{})])
for index,(before,change) in enumerate(cases):
    try:expected=w.merge(before,change);error=False
    except (ValueError,w.DeviceError):error=True
    r=subprocess.run([str(binary),'--cli','--action','merge'],input=json.dumps({'image':before,'changes':change}),capture_output=True,text=True)
    if error:
        assert r.returncode!=0,(index,change,'native accepted invalid input')
    else:
        assert r.returncode==0,(index,change,r.stderr)
        actual=json.loads(r.stdout)
        assert actual['image']=={str(k):v for k,v in expected.items()},(index,change,actual)
        assert actual['settings']==w.decode(expected),(index,'decode mismatch')
        assert actual['revision']==w.revision(expected),(index,'revision mismatch')
print(json.dumps({'parity_cases':len(cases),'passed':True,'hardware_writes':0}))
