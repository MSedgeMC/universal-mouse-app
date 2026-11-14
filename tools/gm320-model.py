#!/usr/bin/env python3
"""Build the photo-referenced GM320 GLB. Asset processing must run on Box.

Original geometry, MIT. The product-marking textures are derived from the pinned Ant
Esports image and remains outside the MIT grant; see assets/GM320-IMAGE.md.
All units are normalized. The recipe is a visual reconstruction, not CAD.
"""
import argparse
import collections
import colorsys
import hashlib
import io
import json
import math
import pathlib
import struct
from PIL import Image


def add(a, b):
    return tuple(x + y for x, y in zip(a, b))


def sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def mul(a, scale):
    return tuple(x * scale for x in a)


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def normal(a):
    length = math.sqrt(sum(x*x for x in a))
    return mul(a, 1/length) if length > 1e-12 else (0, 1, 0)


def smooth(samples, t):
    """Monotone cubic Hermite interpolation preserves traced extrema."""
    if t <= samples[0][0]:
        return samples[0][1]
    if t >= samples[-1][0]:
        return samples[-1][1]
    i = next(n for n in range(len(samples)-1) if samples[n+1][0] >= t)
    slopes = [(b[1]-a[1])/(b[0]-a[0]) for a, b in zip(samples, samples[1:])]
    def tangent(n):
        if n == 0:
            return slopes[0]
        if n == len(samples)-1:
            return slopes[-1]
        a, b = slopes[n-1], slopes[n]
        return 0 if a*b <= 0 else 2*a*b/(a+b)
    a, b = samples[i], samples[i+1]
    h = b[0]-a[0]
    f = (t-a[0])/h
    return ((2*f**3-3*f*f+1)*a[1] + (f**3-2*f*f+f)*h*tangent(i)
            + (-2*f**3+3*f*f)*b[1] + (f**3-f*f)*h*tangent(i+1))


def spline(points, count):
    output = []
    for i in range(count+1):
        u = (len(points)-1)*i/count
        n = min(int(u), len(points)-2)
        f = u-n
        a, b, c, d = (points[max(0,n-1)], points[n], points[n+1], points[min(len(points)-1,n+2)])
        output.append(tuple(.5*(2*b[k]+(-a[k]+c[k])*f+(2*a[k]-5*b[k]+4*c[k]-d[k])*f*f
                                  +(-a[k]+3*b[k]-3*c[k]+d[k])*f*f*f) for k in range(len(b))))
    return output


class Builder:
    def __init__(self, spec):
        self.spec = spec
        self.vertices = []
        self.groups = collections.defaultdict(list)
        self.materials = []
        self.names = {}
        self.anchors = [None]*8
        c = spec['coordinates']
        self.scale = c['body_length']/(c['rear_y']-c['front_y'])

    def point(self, px, py, lift=0):
        c = self.spec['coordinates']
        x = (px-c['center_x'])*self.scale
        z = (py-c['front_y'])*self.scale-c['body_length']/2
        return (x, self.roof(px, py)+lift, z)

    def roof(self, px, py):
        left, right = smooth(self.spec['main_left'], py), smooth(self.spec['main_right'], py)
        middle = (left+right)/2
        radius = max(1, (right-left)/2)
        u = min(1, abs((px-middle)/radius))
        t = min(1, max(0, (py-15)/773))
        peak = smooth(self.spec['roof_height'], py)
        edge = min(peak, .10+.43*math.sin(math.pi*t)**.85)
        blend = min(1,max(0,(py-210)/140))
        blend = blend*blend*(3-2*blend)
        exponent = 4.4-1.8*blend
        return edge+(peak-edge)*(max(0, 1-u**exponent)**(1/exponent))

    def material(self, name, color, roughness=.5, button=-1, emission=0, texture=False):
        if name not in self.names:
            self.names[name] = len(self.materials)
            self.materials.append(dict(name=name, color=color, roughness=roughness,
                                       button=button, emission=emission, texture=texture))
        return self.names[name]

    def vertex(self, p, uv=(0,0), color=(1,1,1,1)):
        self.vertices.append([p, [0,0,0], uv, color])
        return len(self.vertices)-1

    def triangle(self, a, b, c, material):
        v = self.vertices
        n = cross(sub(v[b][0],v[a][0]), sub(v[c][0],v[a][0]))
        if sum(x*x for x in n) < 1e-18:
            return
        self.groups[material].extend((a,b,c))
        for k in (a,b,c):
            v[k][1] = list(add(v[k][1],n))

    def grid(self, nu, nv, point, material, reverse=False, mask=None, uv=None, color=None):
        rows = []
        for j in range(nv+1):
            row = []
            for i in range(nu+1):
                u, v = i/nu, j/nv
                row.append(self.vertex(point(u,v), uv(u,v) if uv else (u,v),
                                       color(u,v) if color else (1,1,1,1)))
            rows.append(row)
        for j in range(nv):
            for i in range(nu):
                u, v = (i+.5)/nu, (j+.5)/nv
                if mask and not mask(u,v):
                    continue
                mat = material(u,v) if callable(material) else material
                a,b,c,d = rows[j][i], rows[j][i+1], rows[j+1][i+1], rows[j+1][i]
                if reverse:
                    self.triangle(a,c,b,mat);self.triangle(a,d,c,mat)
                else:
                    self.triangle(a,b,c,mat);self.triangle(a,c,d,mat)

    def ribbon(self, points, width, material, lift=.004, hues=None, count=110):
        path = spline(points,count)
        def point(u,v):
            n = min(round(v*count), count)
            a,b = path[max(0,n-1)],path[min(count,n+1)]
            length = max(1e-8,math.hypot(b[0]-a[0],b[1]-a[1]))
            offset = (u-.5)*width
            px = path[n][0]-(b[1]-a[1])/length*offset
            py = path[n][1]+(b[0]-a[0])/length*offset
            return self.point(px,py,lift)
        color = None
        if hues:
            color = lambda u,v: (*colorsys.hsv_to_rgb((hues[0]+(hues[1]-hues[0])*v)%1,.95,1),1)
        self.grid(2,count,point,material,color=color)

    def top_box(self, px, py, width, length, material, lift=.022):
        # Chamfered, gently crowned switch surfaces, separate from the shell.
        outline = []
        radius = min(width,length)*.20
        for n in range(4):
            cx = (width/2-radius)*(1 if n in (0,3) else -1)
            cy = (length/2-radius)*(1 if n < 2 else -1)
            for step in range(6):
                angle = math.radians(n*90+step*18)
                outline.append((cx+radius*math.cos(angle),cy+radius*math.sin(angle)))
        top = [self.vertex(self.point(px+x,py+y,lift)) for x,y in outline]
        center = self.vertex(self.point(px,py,lift))
        dark = self.names['seams']
        bottom = [self.vertex(self.point(px+x,py+y,.005)) for x,y in outline]
        for i in range(len(top)):
            j = (i+1)%len(top)
            self.triangle(center,top[j],top[i],material)
            self.triangle(top[i],top[j],bottom[j],dark)
            self.triangle(top[i],bottom[j],bottom[i],dark)

    def side_box(self, py, center_y, height, length, material, offset=.038, slant=0):
        c = self.spec['coordinates']
        z = (py-c['front_y'])*self.scale-c['body_length']/2
        def position(dz,dy,depth):
            ppy = py+dz/self.scale
            px = smooth(self.spec['main_left'], ppy)
            x = (px-c['center_x'])*self.scale-offset-depth
            return (x,center_y+dy+dz*slant,z+dz)
        center = self.vertex(position(0,0,.008))
        outline = []
        radius = min(height,length)*.20
        for n in range(4):
            cz = (length/2-radius)*(1 if n in (0,3) else -1)
            cy = (height/2-radius)*(1 if n < 2 else -1)
            for step in range(6):
                angle = math.radians(n*90+step*18)
                outline.append((cz+radius*math.cos(angle),cy+radius*math.sin(angle)))
        front = [self.vertex(position(dz,dy,0)) for dz,dy in outline]
        back = [self.vertex(position(dz,dy,-.025)) for dz,dy in outline]
        for i in range(len(outline)):
            j = (i+1)%len(outline)
            self.triangle(center,front[i],front[j],material)
            self.triangle(front[i],front[j],back[j],self.names['seams'])
            self.triangle(front[i],back[j],back[i],self.names['seams'])
        return position(0,0,.008)

    def build(self):
        palm = self.material('palm',(.145,.150,.158,1),.37)
        left = self.material('left-click',(.135,.140,.150,1),.34,0)
        right = self.material('right-click',(.135,.140,.150,1),.34,1)
        sides = self.material('side-shell',(.100,.105,.113,1),.55)
        base = self.material('base',(.07,.075,.085,1),.65)
        rest = self.material('thumb-rest',(.105,.115,.13,1),.52)
        seam = self.material('seams',(.018,.022,.028,1),.55)
        rubber = self.material('rubber',(.075,.08,.09,1),.76,2)
        lights = self.material('rgb-diffusers',(1,1,1,1),.4,emission=1)
        wheel_light = self.material('wheel-diffusers',(1,.02,.005,1),.4,2,1)
        forward = self.material('forward-button',(.14,.15,.17,1),.42,3)
        back = self.material('back-button',(.14,.15,.17,1),.42,4)
        dpi = self.material('dpi-button',(.08,.085,.10,1),.4,5)
        led = self.material('lighting-button',(.08,.085,.10,1),.4,6)
        fire = self.material('fire-button',(.075,.08,.095,1),.45,7)
        logo = self.material('official-product-markings',(1,1,1,1),.6,texture=True)

        def row(u,v):
            py = 15+773*v
            a,b = smooth(self.spec['main_left'],py),smooth(self.spec['main_right'],py)
            return a+(b-a)*u,py
        def surface_material(u,v):
            px,py = row(u,v)
            if py < 264:
                return left if px < 263 else right if px > 308 else seam
            return palm
        # The switch gap is cut into the front roof, not painted over it.
        self.grid(44,94,lambda u,v:self.point(*row(u,v)),surface_material,reverse=True,
                  mask=lambda u,v: not (263 < row(u,v)[0] < 308 and row(u,v)[1] < 265))
        for side in ('main_left','main_right'):
            def flank(u,v,side=side):
                py = 15+773*v
                px = smooth(self.spec[side],py)
                top = self.point(px,py)
                sign = -1 if side=='main_left' else 1
                # Molded upper lip and a slightly inset flat bottom edge.
                x = top[0]+sign*(.012*math.sin(math.pi*u)-.022*(1-u))
                return (x,.035+(top[1]-.035)*u,top[2])
            self.grid(9,94,flank,sides,reverse=side=='main_left')
        # Closed underside, deliberately without invented sensor/label details.
        self.grid(18,70,lambda u,v:(self.point(*row(u,v))[0],.035,self.point(*row(u,v))[2]),base)
        for py in (15,788):
            a,b = smooth(self.spec['main_left'],py),smooth(self.spec['main_right'],py)
            self.grid(18,2,lambda u,v,py=py,a=a,b=b:(self.point(a+(b-a)*u,py)[0],
                       .035+(self.roof(a+(b-a)*u,py)-.035)*v,self.point(a,py)[2]),sides,reverse=py==15)
        # Recessed center spine and wheel well.
        self.grid(8,38,lambda u,v:self.point(263+45*u,15+252*v,-.035),seam,reverse=True,
                  mask=lambda u,v:not (108 < 15+252*v < 247 and .12<u<.88))
        self.grid(6,20,lambda u,v:self.point(267+37*u,105+145*v,-.13),base,reverse=True)

        # The characteristic low, asymmetric thumb wing is a separate volume.
        def thumb(u,v):
            py = 250+303*v
            outer = smooth(self.spec['thumb_outer'],py)
            inner = smooth(self.spec['main_left'],py)+3
            px = inner+(outer-inner)*u
            p = self.point(px,py)
            y = .11+.105*(1-u)+.055*math.sin(math.pi*u)*math.sin(math.pi*v)
            return p[0],y,p[2]
        self.grid(18,42,thumb,rest)
        self.grid(2,42,lambda u,v:(thumb(1,v)[0],.035+(thumb(1,v)[1]-.035)*u,thumb(1,v)[2]),sides,reverse=True)
        self.grid(18,42,lambda u,v:(thumb(u,v)[0],.035,thumb(u,v)[2]),base,reverse=True)
        for end in (0,1):
            self.grid(18,2,lambda u,v,end=end:(thumb(u,end)[0],.035+(thumb(u,end)[1]-.035)*v,thumb(u,end)[2]),sides,reverse=end==1)

        # Deep S channels with a dark bezel and inset diffusers.
        for key,hues in [('rgb_left',(.01,.52)),('rgb_right',(.73,.17))]:
            self.ribbon(self.spec[key],13,seam,lift=.003)
            self.ribbon(self.spec[key],10,lights,lift=.006,hues=hues)
        rear = [[smooth(self.spec['main_left'],y),y] for y in (578,600,630,660,690,720,750,774,788)]
        rear += [[smooth(self.spec['main_right'],y),y] for y in (774,750,720,690,660,630,600,578)]
        self.ribbon(rear,4,lights,lift=.004,hues=(.52,1.17),count=130)
        # Front lighting trim follows the exposed side seam in side photographs.
        for side,reverse_hue in [('main_left',False),('main_right',True)]:
            def trim(u,v,side=side):
                py = 35+224*v
                px = smooth(self.spec[side],py)
                p = self.point(px,py)
                sign = -1 if side=='main_left' else 1
                return p[0]+sign*.008,p[1]-.025+(u-.5)*.022,p[2]
            self.grid(2,40,trim,lights,reverse=side=='main_left',
                      color=lambda u,v,reverse_hue=reverse_hue:(*colorsys.hsv_to_rgb((.5+(.5 if reverse_hue else -.5)*v)%1,.95,1),1))
        # Three small illuminated vents in front of the left fire control.
        for py in (166,193,220):
            vent=self.material('rgb-vent-'+str(py),(*colorsys.hsv_to_rgb(.80-(py-166)*.003,.95,1),1),.5,emission=1)
            self.side_box(py,.205,.073,.037,vent,offset=.009,slant=-.25)

        self.anchors[3] = self.side_box(385,.535,.105,.275,forward)
        self.anchors[4] = self.side_box(470,.505,.105,.275,back)
        self.anchors[7] = self.side_box(290,.292,.132,.155,fire,slant=-.16)
        self.top_box(286,311,31,46,dpi)
        self.top_box(286,362,31,44,led)
        self.grid(4,6,lambda u,v:self.point(275+22*u,298+28*v,.0226),logo,reverse=True,
                  uv=lambda u,v:((2+29*u)/256,(130+29*v)/256))
        self.grid(4,6,lambda u,v:self.point(275+22*u,349+27*v,.0226),logo,reverse=True,
                  uv=lambda u,v:((66+29*u)/256,(130+28*v)/256))
        self.anchors[5] = self.point(286,311,.032)
        self.anchors[6] = self.point(286,362,.032)
        self.anchors[0] = self.point(190,162,.004)
        self.anchors[1] = self.point(380,158,.004)

        # A real rotating wheel axis, grooved rubber tread and colored side discs.
        wheel = self.point(285,173)
        wheel = wheel[0],wheel[1]-.080,wheel[2]
        radius = .254
        def wheel_point(u,v):
            angle = 2*math.pi*v
            r = radius+.006*(.5+.5*math.cos(2*math.pi*32*v))
            return wheel[0]+(u-.5)*.135,wheel[1]+r*math.sin(angle),wheel[2]+r*math.cos(angle)
        self.grid(8,128,wheel_point,rubber)
        # Raised tread ribs follow the axle, as visible in the official image.
        groove = self.material('wheel-tread',(.095,.100,.107,1),.78,2)
        for n in range(32):
            def rib(u,v,n=n):
                angle=2*math.pi*(n+(v-.5)*.24)/32
                r=radius+.011
                return wheel[0]+(u-.5)*.125,wheel[1]+r*math.sin(angle),wheel[2]+r*math.cos(angle)
            self.grid(6,2,rib,groove)
        for sign in (-1,1):
            x = wheel[0]+sign*.072
            def disc(u,v,x=x):
                a=2*math.pi*v
                return x,wheel[1]+radius*u*math.sin(a),wheel[2]+radius*u*math.cos(a)
            self.grid(4,64,disc,wheel_light,reverse=sign==1)
        self.anchors[2] = wheel[0],wheel[1]+radius+.008,wheel[2]

        # Braided cable stub and ribbed strain relief are visible in the references.
        cable = self.material('braided-cable',(.048,.052,.061,1),.85)
        root = self.point(216,24)
        cable_y = .17
        def cord(u,v):
            angle=2*math.pi*u
            z=root[2]-.59*v
            x=root[0]-.055*v*v
            radius=.029 if v>.24 else .042+.004*math.cos(v*180)
            return x+radius*math.cos(angle),cable_y+radius*math.sin(angle),z
        self.grid(16,38,cord,cable,reverse=True,
                  color=lambda u,v:(*(3*[.86+.14*math.cos(u*2*math.pi*8+v*70)]),1))
        # Printed branding remains the original logo texture, conformed to the palm.
        self.grid(12,10,lambda u,v:self.point(207+154*u,574+89*v,.003),logo,reverse=True,
                  uv=lambda u,v:((2+154*u)/256,(2+89*v)/256))

    def write(self,path,logo_png):
        for vertex in self.vertices:
            vertex[1] = normal(vertex[1])
        # POSITION / NORMAL / TEXCOORD_0 / COLOR_0, interleaved in 48 bytes.
        buffer=bytearray()
        for p,n,uv,color in self.vertices:
            buffer.extend(struct.pack('<12f',*p,*n,*uv,*color))
        positions=[v[0] for v in self.vertices]
        views=[dict(buffer=0,byteOffset=0,byteLength=len(buffer),byteStride=48,target=34962)]
        accessors=[]
        for offset,typ in [(0,'VEC3'),(12,'VEC3'),(24,'VEC2'),(32,'VEC4')]:
            accessor=dict(bufferView=0,byteOffset=offset,componentType=5126,count=len(self.vertices),type=typ)
            if offset==0:
                accessor.update(min=[min(p[k] for p in positions) for k in range(3)],
                                max=[max(p[k] for p in positions) for k in range(3)])
            accessors.append(accessor)
        primitives=[]
        for material,indices in sorted(self.groups.items()):
            offset=len(buffer)
            buffer.extend(struct.pack('<'+'I'*len(indices),*indices))
            views.append(dict(buffer=0,byteOffset=offset,byteLength=len(buffer)-offset,target=34963))
            accessors.append(dict(bufferView=len(views)-1,componentType=5125,count=len(indices),type='SCALAR'))
            primitives.append(dict(attributes=dict(POSITION=0,NORMAL=1,TEXCOORD_0=2,COLOR_0=3),
                                   indices=len(accessors)-1,material=material,mode=4))
        while len(buffer)%4:buffer.append(0)
        logo_offset=len(buffer);buffer.extend(logo_png)
        views.append(dict(buffer=0,byteOffset=logo_offset,byteLength=len(logo_png)))
        materials=[]
        for m in self.materials:
            pbr=dict(baseColorFactor=m['color'],metallicFactor=0,roughnessFactor=m['roughness'])
            if m['texture']:pbr['baseColorTexture']=dict(index=0)
            materials.append(dict(name=m['name'],pbrMetallicRoughness=pbr,
                                  emissiveFactor=[m['emission']]*3,
                                  doubleSided=False,alphaMode='BLEND' if m['texture'] else 'OPAQUE',
                                  extras=dict(button=m['button'])))
        data=dict(asset=dict(version='2.0',generator='Orbit GM320 reference reconstruction 1'),
                  scene=0,scenes=[dict(nodes=[0])],nodes=[dict(mesh=0,name='GM320')],
                  meshes=[dict(name='Ant Esports GM320',primitives=primitives,
                               extras=dict(buttonAnchors=self.anchors,units='normalized',bodyLength=2.8,
                                           accuracy=self.spec['accuracy']))],
                  materials=materials,buffers=[dict(byteLength=len(buffer))],bufferViews=views,
                  accessors=accessors,images=[dict(bufferView=len(views)-1,mimeType='image/png')],
                  textures=[dict(source=0,sampler=0)],samplers=[dict(magFilter=9729,minFilter=9729,wrapS=33071,wrapT=33071)])
        js=json.dumps(data,separators=(',',':')).encode()
        js+=b' '*(-len(js)%4);buffer+=b'\0'*(-len(buffer)%4)
        result=struct.pack('<III',0x46546c67,2,12+8+len(js)+8+len(buffer))
        result+=struct.pack('<II',len(js),0x4e4f534a)+js
        result+=struct.pack('<II',len(buffer),0x004e4942)+buffer
        path.write_bytes(result)
        print(json.dumps(dict(path=str(path),vertices=len(self.vertices),triangles=sum(len(i)//3 for i in self.groups.values()),
                              bytes=len(result),sha256=hashlib.sha256(result).hexdigest()),indent=2))


def prepare_logo(image):
    crop=image.crop((207,574,361,663)).convert('RGBA')
    for y in range(crop.height):
        for x in range(crop.width):
            r,g,b,a=crop.getpixel((x,y))
            saturation=max(r,g,b)-min(r,g,b)
            coverage=min(1,max(0,(saturation-30)/45))
            crop.putpixel((x,y),(r,g,b,round(a*coverage)))
    atlas=Image.new('RGBA',(256,256))
    atlas.paste(crop,(2,2))
    for region,origin in [((271,297,300,326),(2,130)),((271,348,300,376),(66,130))]:
        label=image.crop(region).convert('RGBA')
        for y in range(label.height):
            for x in range(label.width):
                r,g,b,a=label.getpixel((x,y))
                level=(r+g+b)/3
                coverage=min(1,max(0,(level-30)/60))
                label.putpixel((x,y),(r,g,b,round(a*coverage)))
        atlas.paste(label,origin)
    output=io.BytesIO();atlas.save(output,format='PNG',optimize=True)
    return output.getvalue()


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spec',type=pathlib.Path,default=pathlib.Path('assets/gm320-model-spec.json'))
    parser.add_argument('--image',type=pathlib.Path,default=pathlib.Path('assets/gm320-top.png'))
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('assets/gm320-model.glb'))
    args=parser.parse_args()
    spec=json.loads(args.spec.read_text())
    assert hashlib.sha256(args.image.read_bytes()).hexdigest()==spec['source_sha256'], 'Review changed official image before modeling'
    mesh=Builder(spec);mesh.build()
    mesh.write(args.output,prepare_logo(Image.open(args.image)))
