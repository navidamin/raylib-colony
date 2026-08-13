import math, random
MIN, MAX = 0.3, 2.0
def field(g, rng):
    n = 1 + rng.randrange(2)
    cl = [(rng.random()*g, rng.random()*g, 0.8+rng.random()*1.5) for _ in range(n)]
    w = [[max(0.1, max(math.exp(-(((x+.5)-cx)**2+((y+.5)-cy)**2)/(2*s*s)) for cx,cy,s in cl))
          for x in range(g)] for y in range(g)]
    tot = sum(sum(r) for r in w); avg = max(tot/(g*g), .001)
    return [min(MAX, max(MIN, w[y][x]/avg)) for y in range(g) for x in range(g)]

for g in (3, 6):
    rng = random.Random(4242+g)
    tr, qr = [], []
    for _ in range(100000):
        NRES = 6
        base = [0.4+rng.random()*1.2 for _ in range(NRES)]      # per-resource base abundance
        fields = [field(g, rng) for _ in range(NRES)]
        target = 0
        # yield of the TARGET resource per spot = base_t * w_t
        ty = [base[target]*fields[target][i] for i in range(g*g)]
        # TOTAL quantity per spot = sum over resources
        q  = [sum(base[r]*fields[r][i] for r in range(NRES)) for i in range(g*g)]
        tr.append(max(ty)/(sum(ty)/len(ty)))
        qr.append(max(q)/(sum(q)/len(q)))
    print(f"grid {g}x{g}:  best/mean for TARGET resource {sum(tr)/len(tr):.2f}x (+{100*(sum(tr)/len(tr)-1):.0f}%)   "
          f"|  for TOTAL quantity {sum(qr)/len(qr):.2f}x (+{100*(sum(qr)/len(qr)-1):.0f}%)")
