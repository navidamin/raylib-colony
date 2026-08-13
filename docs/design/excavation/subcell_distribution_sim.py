import math, random
G, MIN, MAX = 8, 0.3, 2.0
REACH = [2,4,6,8]

def field(rng):
    n = 1 + rng.randrange(2)
    cl = [(rng.random()*G, rng.random()*G, 0.8+rng.random()*1.5) for _ in range(n)]
    w=[[0.0]*G for _ in range(G)]; tot=0.0
    for y in range(G):
        for x in range(G):
            m=0.1
            for cx,cy,s in cl:
                m=max(m, math.exp(-(((x+.5)-cx)**2+((y+.5)-cy)**2)/(2*s*s)))
            w[y][x]=m; tot+=m
    avg=max(tot/(G*G),.001)
    return [[min(MAX,max(MIN,w[y][x]/avg)) for x in range(G)] for y in range(G)]

def reach_cells(r):
    o=(G-r)//2
    return [(x,y) for y in range(o,o+r) for x in range(o,o+r)]

rng=random.Random(20260813)
N=200000
stats={r:{'mean':0,'best':0,'ratio':0} for r in REACH}
best_all=0; t0best=0
for _ in range(N):
    w=field(rng)
    allv=[w[y][x] for y in range(G) for x in range(G)]
    best_all+=max(allv)
    for r in REACH:
        v=[w[y][x] for (x,y) in reach_cells(r)]
        m=sum(v)/len(v); b=max(v)
        stats[r]['mean']+=m; stats[r]['best']+=b; stats[r]['ratio']+=b/m
    t0best+=max(w[y][x] for (x,y) in reach_cells(2))

print(f"full-grid best (all 64): {best_all/N:.2f}\n")
print("tier  reach  cells   mean   best   best/mean (survey value)   best vs T0 (reach value)")
base=t0best/N
for t,r in enumerate(REACH):
    s=stats[r]; c=r*r
    print(f" T{t}   {r}x{r}   {c:2d}    {s['mean']/N:.2f}   {s['best']/N:.2f}      "
          f"+{100*(s['ratio']/N-1):3.0f}%                  {s['best']/N/base:.2f}x")

# how often is the single best cell of the whole grid reachable at each tier?
rng=random.Random(777)
for t,r in enumerate(REACH):
    hit=0
    for _ in range(50000):
        w=field(rng)
        bx,by=max(((x,y) for y in range(G) for x in range(G)), key=lambda p: w[p[1]][p[0]])
        o=(G-r)//2
        if o<=bx<o+r and o<=by<o+r: hit+=1
    print(f" T{t}: the grid's best cell is reachable {100*hit/50000:2.0f}% of the time")
