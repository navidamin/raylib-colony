import math, random, statistics
MIN, MAX = 0.3, 2.0

def gen(gridSize, rng):
    n = 1 + rng.randrange(2)          # 1-2 clusters
    cl = [(rng.random()*gridSize, rng.random()*gridSize, 0.8+rng.random()*1.5) for _ in range(n)]
    w = [[0.0]*gridSize for _ in range(gridSize)]
    tot = 0.0
    for y in range(gridSize):
        for x in range(gridSize):
            m = 0.1
            for (cx, cy, sig) in cl:
                dx, dy = (x+0.5)-cx, (y+0.5)-cy
                inf = math.exp(-(dx*dx+dy*dy)/(2*sig*sig))
                m = max(m, inf)
            w[y][x] = m; tot += m
    avg = max(tot/(gridSize*gridSize), 0.001)
    return [min(MAX, max(MIN, w[y][x]/avg)) for y in range(gridSize) for x in range(gridSize)]

for g in (3,4,5,6):
    rng = random.Random(12345+g)
    means, bests, ratios, medians = [], [], [], []
    for _ in range(200000):
        v = gen(g, rng)
        m = sum(v)/len(v); b = max(v)
        means.append(m); bests.append(b); ratios.append(b/m); medians.append(statistics.median(v))
    print(f"grid {g}x{g} (n={g*g:2d}): mean {sum(means)/len(means):.3f}  median {sum(medians)/len(medians):.3f}  "
          f"best {sum(bests)/len(bests):.3f}  best/mean {sum(ratios)/len(ratios):.3f}  "
          f"(+{100*(sum(ratios)/len(ratios)-1):.0f}%)")

# how often is the best cell at the 2.0 clamp?
for g in (3,6):
    rng = random.Random(999+g)
    clamped = sum(1 for _ in range(100000) if max(gen(g,rng)) >= MAX-1e-6)
    print(f"grid {g}x{g}: best cell sits at the 2.0 ceiling {100*clamped/100000:.0f}% of the time")
