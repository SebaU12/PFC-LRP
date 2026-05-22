#include "split.h"
#include <algorithm>
#include <limits>

// ═════════════════════════════════════════
// split_deposito
//
// Grafo auxiliar de p+1 nodos (0..p).
// Arco (i,j): sub-ruta que visita clientes[i..j-1].
//   - Existe solo si demanda(clientes[i..j-1]) <= Q
//   - Peso = F + dist(dep→c[i]) + dist(c[i]→c[i+1]) + ... + dist(c[j-1]→dep)
// Camino mínimo 0→p = partición óptima.
// ═════════════════════════════════════════
SplitResultado split_deposito(const std::vector<int> &clientes, int dep_idx,
                              const Matriz &mat, const InstanciaLRP &inst) {
  SplitResultado res;
  res.factible = true;

  int p = (int)clientes.size();
  if (p == 0) {
    res.costo = 0.0;
    return res;
  }

  const double Q = inst.Q;
  const double F = inst.F;
  const double INF = std::numeric_limits<double>::infinity();

  std::vector<double> cost(p + 1, INF);
  std::vector<int> prev(p + 1, -1);
  cost[0] = 0.0;

  for (int i = 0; i < p; ++i) {
    if (cost[i] == INF)
      continue;

    double demanda = 0.0;
    double dist_acum = 0.0; // distancia interna acumulada desde c[i]

    for (int j = i; j < p; ++j) {
      int c_j = clientes[j];
      int idx_cj = c_j - 1; // 0-based en matriz

      demanda += inst.clientes[idx_cj].demanda;
      if (demanda > Q)
        break; // arcos (i,j+1..p) ya no son factibles

      // Distancia interna: dep→c[i]→...→c[j]
      if (j == i) {
        dist_acum = mat[dep_idx][idx_cj];
      } else {
        int idx_prev = clientes[j - 1] - 1;
        dist_acum += mat[idx_prev][idx_cj];
      }

      // Peso del arco (i → j+1):
      // costo de la sub-ruta dep→c[i]→...→c[j]→dep
      double arc_cost = F + dist_acum + mat[idx_cj][dep_idx];

      if (cost[i] + arc_cost < cost[j + 1]) {
        cost[j + 1] = cost[i] + arc_cost;
        prev[j + 1] = i;
      }
    }
  }

  // ── Caso degenerado: algún cliente tiene demanda > Q ─────
  if (cost[p] == INF) {
    res.factible = false;
    res.costo = 0.0;
    for (int k = 0; k < p; ++k) {
      int c = clientes[k];
      res.costo += F + mat[dep_idx][c - 1] * 2.0;
      res.rutas.push_back({c});
    }
    return res;
  }

  // ── Reconstruir rutas desde el vector prev[] ─────────────
  res.costo = cost[p];
  {
    int cur = p;
    while (cur > 0) {
      int ini = prev[cur];
      Ruta ruta;
      ruta.reserve(cur - ini);
      for (int k = ini; k < cur; ++k)
        ruta.push_back(clientes[k]);
      res.rutas.push_back(std::move(ruta));
      cur = ini;
    }
    std::reverse(res.rutas.begin(), res.rutas.end());
  }
  return res;
}

// ═════════════════════════════════════════
// evaluar_cromosoma
// ═════════════════════════════════════════
std::vector<SplitResultado> evaluar_cromosoma(CromosomaLRP &crom,
                                              const Matriz &mat,
                                              const InstanciaLRP &inst,
                                              double alpha) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;

  double costo_total = 0.0;
  std::vector<SplitResultado> resultados(m);

  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0)
      continue; // depósito cerrado

    // Costo fijo de apertura
    costo_total += inst.depositos[i].costo_apertura;

    // Extraer clientes asignados a este depósito en orden de CS
    std::vector<int> cli = crom.clientes_de(i, inst);

    // Split
    int dep_idx = n + i; // 0-based en la matriz
    resultados[i] = split_deposito(cli, dep_idx, mat, inst);
    costo_total += resultados[i].costo;

    // Penalización por exceso de capacidad del depósito
    double demanda_dep = 0.0;
    for (int c : cli)
      demanda_dep += inst.clientes[c - 1].demanda;

    double exceso = demanda_dep - inst.depositos[i].capacidad;
    if (exceso > 0.0)
      costo_total += alpha * exceso;
  }

  crom.fitness = costo_total;
  return resultados;
}
