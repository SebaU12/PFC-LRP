#include "estado.h"

// ─────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────

double EstadoLRP::demanda_ruta(const Ruta &r) const {
  double d = 0.0;
  for (int c : r)
    d += datos->clientes[c - 1].demanda;
  return d;
}

double EstadoLRP::demanda_deposito(int dep_id) const {
  double total = 0.0;
  auto it = rutas.find(dep_id);
  if (it != rutas.end())
    for (const auto &r : it->second)
      total += demanda_ruta(r);
  return total;
}

// ─────────────────────────────────────────
// objective()
// ─────────────────────────────────────────
double EstadoLRP::objective() const {
  double z = 0.0;

  // 1. Costos fijos de apertura de depósitos
  for (int dep_id : depositos_abiertos) {
    const auto &dep = datos->depositos[dep_id - datos->num_clientes - 1];
    z += dep.costo_apertura;
  }

  // 2. Costo fijo por vehículo (F) + costo de arcos
  for (const auto &[dep_id, lista_rutas] : rutas) {
    int idx_dep = dep_id - 1; // 0-based para la matriz

    for (const auto &ruta : lista_rutas) {
      if (ruta.empty())
        continue;

      z += datos->F; // costo fijo del vehículo

      // Depósito → primer cliente
      z += (*matriz)[idx_dep][ruta.front() - 1];

      // Cliente → siguiente cliente
      for (int i = 0; i + 1 < (int)ruta.size(); ++i)
        z += (*matriz)[ruta[i] - 1][ruta[i + 1] - 1];

      // Último cliente → depósito
      z += (*matriz)[ruta.back() - 1][idx_dep];
    }
  }

  // 3. Penalización por exceso de capacidad en depósitos
  for (int dep_id : depositos_abiertos) {
    const auto &dep = datos->depositos[dep_id - datos->num_clientes - 1];
    double dem = demanda_deposito(dep_id);
    if (dem > dep.capacidad)
      z += 100000.0 * (dem - dep.capacidad);
  }

  // 4. Penalización por clientes huérfanos
  z += 100000.0 * static_cast<int>(no_asignados.size());

  return z;
}
