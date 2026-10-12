import base64, json, math, struct, zlib
from pathlib import Path
root = Path(__file__).resolve().parent.parent
folder = root / 'Assets/Map/EnemyMovementTest'
(folder / 'Models').mkdir(parents=True, exist_ok=True)
def save(path, value):
    path.write_bytes((json.dumps(value, ensure_ascii=False, indent=2)+'\n').replace('\n','\r\n').encode('utf-8'))
blob=bytearray(); views=[]; accessors=[]; primitives=[]
def accessor(values, kind, component, fmt, target):
    while len(blob)%4: blob.append(0)
    offset=len(blob)
    for value in values:
        blob.extend(struct.pack('<'+fmt, *value) if isinstance(value, tuple) else struct.pack('<'+fmt,value))
    views.append(dict(buffer=0,byteOffset=offset,byteLength=len(blob)-offset,target=target))
    a=dict(bufferView=len(views)-1,componentType=component,count=len(values),type=kind)
    if kind=='VEC3': a.update(min=[min(v[i] for v in values) for i in range(3)],max=[max(v[i] for v in values) for i in range(3)])
    accessors.append(a); return len(accessors)-1
def quad(points, material):
    u=[points[1][i]-points[0][i] for i in range(3)]; v=[points[2][i]-points[0][i] for i in range(3)]
    n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]); length=math.sqrt(sum(x*x for x in n)); n=tuple(x/length for x in n)
    # glTF is right-handed; existing visual/collision loaders flip Z and winding.
    p=accessor([(x,y,-z) for x,y,z in points],'VEC3',5126,'fff',34962); normal=accessor([(n[0],n[1],-n[2])]*4,'VEC3',5126,'fff',34962)
    uv=accessor([(0.,0.),(0.,1.),(1.,1.),(1.,0.)],'VEC2',5126,'ff',34962)
    ind=accessor([0,2,1,0,3,2],'SCALAR',5123,'H',34963)
    primitives.append(dict(attributes={'POSITION':p,'NORMAL':normal,'TEXCOORD_0':uv},indices=ind,material=material,mode=4))
def plane(x0,x1,z0,z1,y0,y1,mat): quad([(x0,y0,z0),(x0,y1,z1),(x1,y1,z1),(x1,y0,z0)],mat)
def box(x0,x1,y0,y1,z0,z1,mat):
    plane(x0,x1,z0,z1,y1,y1,mat)
    quad([(x0,y0,z0),(x1,y0,z0),(x1,y0,z1),(x0,y0,z1)],mat)
    quad([(x0,y0,z0),(x0,y1,z0),(x1,y1,z0),(x1,y0,z0)],mat)
    quad([(x1,y0,z1),(x1,y1,z1),(x0,y1,z1),(x0,y0,z1)],mat)
    quad([(x0,y0,z1),(x0,y1,z1),(x0,y1,z0),(x0,y0,z0)],mat)
    quad([(x1,y0,z0),(x1,y1,z0),(x1,y1,z1),(x1,y0,z1)],mat)
plane(-80,80,-80,100,0,0,0)
plane(-24,-16,-65,65,0.02,0.02,1)
plane(25,35,0,20,0,4,2); box(25,35,3.6,4,20,40,2); plane(25,35,40,60,4,0,2)
box(-6,6,0,3,-11,-9,3)
colors=[[0.22,0.32,0.2,1],[0.35,0.35,0.35,1],[0.15,0.4,0.65,1],[0.6,0.35,0.15,1]]
def solid_png(c):
    def chunk(kind, data): return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(bytes([0]+[int(x*255) for x in c])))+chunk(b'IEND',b'')
    return 'data:image/png;base64,'+base64.b64encode(png).decode()
save(folder/'Models/Course.gltf',dict(asset={'version':'2.0','generator':'Tank EnemyMovementTest'},scene=0,scenes=[{'nodes':[0,2]}],nodes=[{'name':'Visual','children':[1]},{'mesh':0,'name':'CourseVisual'},{'name':'Hit','children':[3]},{'mesh':0,'name':'CourseCollision'}],meshes=[{'primitives':primitives}],materials=[{'pbrMetallicRoughness':{'baseColorFactor':[1,1,1,1],'baseColorTexture':{'index':i},'metallicFactor':0,'roughnessFactor':0.9},'doubleSided':True} for i,c in enumerate(colors)],images=[{'uri':solid_png(c)} for c in colors],textures=[{'source':i} for i in range(len(colors))],buffers=[{'byteLength':len(blob),'uri':'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()}],bufferViews=views,accessors=accessors))
def pt(x,z,h=0,surface='ground'): return {'position':[x,h+0.84,z],'surfaceId':surface}
enemies=[{'id':'patrol','unitType':'Patrol tank','position':[-20,0.84,0],'rotationDegrees':[0,0,0]}, {'id':'pursuer','unitType':'Pursuit tank','position':[0,0.84,0],'rotationDegrees':[0,180,0]}, {'id':'bridge','unitType':'Patrol tank','position':[30,1.84,5],'rotationDegrees':[0,0,0]}]
routes=[[pt(-20,30),pt(-20,0)],[pt(0,20),pt(0,0)],[pt(30,35,4,'bridge'),pt(30,60),pt(30,5,1,'bridge')]]
groups=[{'id':e['id']+'-group','members':[{'enemyId':e['id'],'commandPriority':0}], 'patrol':{'points':route,'attackStopMeters':10,'resumeApproachMeters':14,'arrivalMeters':1,'searchWaitSeconds':4}} for e,route in zip(enemies,routes)]
save(folder/'Manifest.json',{'version':1,'playerSpawn':{'position':[-20,2,-45],'rotationDegrees':[0,0,0]},'instances':[{'id':'course','asset':'Models/Course.gltf','position':[0,0,0],'rotationDegrees':[0,0,0],'scale':1}], 'clearAreas':[],'enemies':enemies,'enemyGroups':groups})
catalog={'version':1,'simulation':{'physicalEnterMeters':30,'simpleReturnMeters':40,'brakingDecelerationMetersPerSecondSquared':2,'nodeArrivalMeters':0.5,'observationRadiusMeters':6,'supportHeightToleranceMeters':0.2}, 'attackTypes':[{'name':'Quiet patrol','detectionRangeMeters':1,'reachMeters':1,'firingRangeMeters':0.5,'firingIntervalSeconds':60}, {'name':'Pursuit gun','detectionRangeMeters':35,'reachMeters':16,'firingRangeMeters':12,'projectileSpeedMetersPerSecond':20,'firingIntervalSeconds':5,'maximumYawSpeedDegreesPerSecond':90}], 'unitTypes':[{'name':'Patrol tank','attackMounts':[{'attackType':'Quiet patrol','localPosition':[0,0.85,1.8]}]}, {'name':'Pursuit tank','attackMounts':[{'attackType':'Pursuit gun','localPosition':[0,0.85,1.8]}]}]}
save(folder/'enemy_types.json',catalog)
cells=[]; ids={}; links=[]
def cell(x,z,h=0,surface='ground',gradient=0,passable=True,terrain='ground',clearance=10):
    ids[(x,z,surface)]=len(cells)
    c=dict(x=x//5,z=z//5,surfaceId=surface,position=[x,h+0.84,z],terrain=terrain,clearanceWidthMeters=10,clearanceHeightMeters=clearance,slopeDegrees=math.degrees(math.atan(abs(gradient))),waterDepthMeters=0,passable=passable,supportHeightMeters=h)
    if gradient: c['supportGradientXZ']=[0,gradient]
    cells.append(c)
for x in range(-60,61,5):
    for z in range(-65,81,5):
        blocked=(abs(x)<=5 and z==-10) or (25<=x<=35 and (5<=z<=15 or 45<=z<=55))
        cell(x,z,passable=not blocked,terrain='road' if x==-20 else 'ground',clearance=3.6 if 25<=x<=35 and 20<=z<=40 else 10)
for z in range(5,60,5):
    h=min(z/5,4,(60-z)/5); gradient=0.2 if z<20 else (-0.2 if z>=40 else 0)
    cell(30,z,h,'bridge',gradient,terrain='road')
def join(a,b):
    passage=dict(clearanceWidthMeters=10,clearanceHeightMeters=3.6,stepMeters=0)
    links.extend([dict(**passage,**{'from':ids[a],'to':ids[b]}),dict(**passage,**{'from':ids[b],'to':ids[a]})])
for key in list(ids):
    x,z,s=key
    for other in [(x+5,z,s),(x,z+5,s)]:
        if other in ids: join(key,other)
join((30,0,'ground'),(30,5,'bridge')); join((30,55,'bridge'),(30,60,'ground'))
mobility=dict(speedMetersPerSecond=2,widthMeters=2.8,heightMeters=2,maximumSlopeDegrees=30,maximumWaterDepthMeters=0.5,maximumStepMeters=0.5,terrainCosts={'road':1,'ground':1.25,'rough':2,'marsh':4})
save(folder/'navigation.json',{'version':1,'grid':{'version':1,'cells':cells,'links':links},'drivers':[{'groupId':e['id']+'-group','enemyId':e['id'],'surfaceId':'bridge' if e['id']=='bridge' else 'ground','snapDistanceMeters':1,'mobility':mobility} for e in enemies]})
print('Generated EnemyMovementTest:',len(cells),'cells,',len(links),'links')