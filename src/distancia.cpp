#include "distancia.h"
#include <cmath>

Matriz construir_matriz_distancias(const InstanciaLRP &inst) {
  // Nodos: primero clientes, luego depósitos (igual que Python)
  int N = inst.num_clientes + inst.num_depositos;
  Matriz m(N, std::vector<double>(N, 0.0));

  // Unificar coordenadas en un solo vector para indexar con i, j
  struct Nodo {
    double x, y;
  };
  std::vector<Nodo> nodos;
  nodos.reserve(N);
  for (const auto &c : inst.clientes)
    nodos.push_back({c.x, c.y});
  for (const auto &d : inst.depositos)
    nodos.push_back({d.x, d.y});

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      if (i == j)
        continue;
      double dx = nodos[i].x - nodos[j].x;
      double dy = nodos[i].y - nodos[j].y;
      double dist = std::sqrt(dx * dx + dy * dy);
      m[i][j] = inst.es_entero ? static_cast<int>(dist * 100) : dist;
    }
  }
  return m;
}
