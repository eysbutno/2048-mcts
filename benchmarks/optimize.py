import cma

def avg_score(x):
    C = x[0]
    OPEN = x[1]
    MONO = x[2]

x0 = [0.5, 0.5, 0.5]
sigma0 = 0.2

es = cma.CMAEvolutionStrategy(x0, sigma0);
es.optimize(avg_score)
print(es.result.xfavorite);