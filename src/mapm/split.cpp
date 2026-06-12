// =============================================================================
// split.cpp — Implementación del algoritmo Split
// =============================================================================
#include "split.h"
#include <algorithm>
#include <limits>

// -----------------------------------------------------------------------------
// Encuentra la partición óptima de una secuencia de clientes en rutas de vehículo
// para un depósito dado.
//
// Algoritmo de programación dinámica (Bellman-Ford sobre grafo auxiliar):
//
//   cost[i] = costo mínimo de rutear los primeros i clientes de la lista.
//   cost[0] = 0  (ningún cliente ruteado, costo cero)
//   cost[j+1] = min sobre todos i ≤ j de:  cost[i] + costo_ruta(clientes[i..j])
//
//   costo_ruta(i..j) = F + dist(dep, ci+1) + dist(ci+1,ci+2) + ... + dist(cj, dep)
//   Solo se puede usar si sum(demandas[i+1..j]) ≤ Q.
//
// Para reconstruir las rutas, se guarda prev[j+1] = i (desde qué posición
// comienza la última ruta que llega al nodo j+1). Se reconstruye al final
// siguiendo los punteros prev hacia atrás desde p.
// -----------------------------------------------------------------------------
SplitResultado split_deposito(const std::vector<int> &clientes, int dep_idx,
                              const Matriz &mat, const InstanciaLRP &inst) {
  SplitResultado res;
  res.factible = true;

  int p = (int)clientes.size();
  if (p == 0) { res.costo = 0.0; return res; } // sin clientes, sin rutas

  const double Q = inst.Q;
  const double F = inst.F;
  const double INF = std::numeric_limits<double>::infinity();

  // cost[i] = costo mínimo para rutear los primeros i clientes
  // prev[i] = índice i' tal que la última ruta va de i' a i
  std::vector<double> cost(p + 1, INF);
  std::vector<int> prev(p + 1, -1);
  cost[0] = 0.0;

  // Para cada posición de inicio i, extender la ruta agregando clientes j
  for (int i = 0; i < p; ++i) {
    if (cost[i] == INF) continue; // nodo inalcanzable

    double demanda = 0.0;   // demanda acumulada del segmento [i..j]
    double dist_acum = 0.0; // distancia acumulada dentro de la ruta

    for (int j = i; j < p; ++j) {
      int c_j = clientes[j];
      int idx_cj = c_j - 1;

      demanda += inst.clientes[idx_cj].demanda;
      if (demanda > Q) break; // supera capacidad del vehículo — no extender más

      // Distancia acumulada: primer cliente desde el depósito, resto continúa
      dist_acum = (j == i) ? mat[dep_idx][idx_cj]
                           : dist_acum + mat[clientes[j - 1] - 1][idx_cj];

      // Costo del arco (i → j+1): hacer una ruta con los clientes [i..j]
      double arc_cost = F + dist_acum + mat[idx_cj][dep_idx];
      if (cost[i] + arc_cost < cost[j + 1]) {
        cost[j + 1] = cost[i] + arc_cost;
        prev[j + 1] = i; // la ruta que llega al nodo j+1 empieza en i
      }
    }
  }

  // Caso degenerado: algún cliente tiene demanda > Q (infactible, no debería pasar)
  // Fallback: cada cliente en su propia ruta, igual viola la capacidad pero al menos
  // devuelve algo usable para la penalización
  if (cost[p] == INF) {
    res.factible = false;
    res.costo = 0.0;
    for (int k = 0; k < p; ++k) {
      int c = clientes[k];
      res.costo += F + mat[dep_idx][c - 1] * 2.0;
      res.rutas.push_back(RutaVec{c});
    }
    return res;
  }

  // Reconstruir las rutas siguiendo los punteros prev hacia atrás
  res.costo = cost[p];
  {
    int cur = p;
    while (cur > 0) {
      int ini = prev[cur];
      RutaVec ruta;
      ruta.reserve(cur - ini);
      for (int k = ini; k < cur; ++k)
        ruta.push_back(clientes[k]);
      res.rutas.push_back(std::move(ruta));
      cur = ini;
    }
    std::reverse(res.rutas.begin(), res.rutas.end()); // quedan en orden correcto
  }
  return res;
}

// -----------------------------------------------------------------------------
// Evalúa el cromosoma completo aplicando Split a cada depósito abierto.
//
// Para cada depósito abierto:
//   1. Suma su costo de apertura O_i
//   2. Extrae su sublista de clientes de CS
//   3. Aplica split_deposito → obtiene rutas óptimas y su costo
//   4. Si la demanda total supera la capacidad W_i del depósito, penaliza
//
// El resultado se guarda en crom.fitness.
// Los SplitResultados se devuelven indexados por dep_idx (0-based), para que
// la búsqueda local pueda usarlos directamente sin recalcular.
// -----------------------------------------------------------------------------
std::vector<SplitResultado> evaluar_cromosoma(CromosomaLRP &crom,
                                              const Matriz &mat,
                                              const InstanciaLRP &inst,
                                              double alpha) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;

  double costo_total = 0.0;
  std::vector<SplitResultado> resultados(m);

  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0) continue; // depósito cerrado — ignorar

    // Costo de apertura del depósito
    costo_total += inst.depositos[i].costo_apertura;

    // Obtener clientes y aplicar Split
    std::vector<int> cli = crom.clientes_de(i, inst);
    int dep_idx = n + i; // índice 0-based en la matriz: clientes[0..n-1], depósitos[n..n+m-1]
    resultados[i] = split_deposito(cli, dep_idx, mat, inst);
    costo_total += resultados[i].costo;

    // Penalizar si la demanda total del depósito supera su capacidad W_i
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
