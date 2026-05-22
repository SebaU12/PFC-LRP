#include "solucion_inicial.h"
#include <limits>
#include <tuple>

// ─────────────────────────────────────────
// Helper — debe declararse antes de su uso
// ─────────────────────────────────────────
double demanda_ruta_externa(const Ruta &r, const InstanciaLRP &datos) {
  double d = 0.0;
  for (int c : r)
    d += datos.clientes[c - 1].demanda;
  return d;
}

// ─────────────────────────────────────────
// Greedy Insertion
// ─────────────────────────────────────────
static EstadoLRP greedy_insertion(EstadoLRP estado) {
  const Matriz &mat = *estado.matriz;
  const double Q = estado.datos->Q;
  const double F = estado.datos->F;

  while (!estado.no_asignados.empty()) {

    double mejor_costo = std::numeric_limits<double>::infinity();
    std::tuple<int, int, int, int> mejor_mov = {-1, -1, -1, -1};

    int cliente = estado.no_asignados[0];
    int idx_c = cliente - 1;
    double dem_c = estado.datos->clientes[idx_c].demanda;

    for (int dep_id : estado.depositos_abiertos) {
      int idx_dep = dep_id - 1;
      auto &lista = estado.rutas[dep_id];

      // a) Inserción en rutas existentes
      for (int ri = 0; ri < (int)lista.size(); ++ri) {
        auto &ruta = lista[ri];
        // Verificar capacidad ANTES de evaluar posiciones
        if (demanda_ruta_externa(ruta, *estado.datos) + dem_c > Q)
          continue;

        for (int pos = 0; pos <= (int)ruta.size(); ++pos) {
          int prev = (pos == 0) ? idx_dep : ruta[pos - 1] - 1;
          int next = (pos == (int)ruta.size()) ? idx_dep : ruta[pos] - 1;

          double costo = mat[prev][idx_c] + mat[idx_c][next] - mat[prev][next];

          if (costo < mejor_costo) {
            mejor_costo = costo;
            mejor_mov = {cliente, dep_id, ri, pos};
          }
        }
      }

      // b) Nueva ruta desde este depósito
      double costo_nueva = mat[idx_dep][idx_c] * 2.0 + F;
      if (costo_nueva < mejor_costo) {
        mejor_costo = costo_nueva;
        mejor_mov = {cliente, dep_id, -1, 0};
      }
    }

    auto [c_ins, m_dep, m_ri, m_pos] = mejor_mov;
    if (m_ri == -1) {
      estado.rutas[m_dep].push_back({c_ins});
    } else {
      auto &ruta = estado.rutas[m_dep][m_ri];
      ruta.insert(ruta.begin() + m_pos, c_ins);
    }

    estado.no_asignados.erase(estado.no_asignados.begin());
  }

  return estado;
}

// ─────────────────────────────────────────
// generar_solucion_inicial
// ─────────────────────────────────────────
EstadoLRP generar_solucion_inicial(const InstanciaLRP &inst, const Matriz &m) {
  // All-Open: abrir todos los depósitos
  std::vector<int> dep_ids;
  Rutas rutas_vacias;
  for (const auto &d : inst.depositos) {
    dep_ids.push_back(d.id);
    rutas_vacias[d.id] = {};
  }

  // Todos los clientes al banco de no asignados
  std::vector<int> todos;
  for (const auto &c : inst.clientes)
    todos.push_back(c.id);

  EstadoLRP estado(rutas_vacias, dep_ids, todos, m, inst);
  return greedy_insertion(std::move(estado));
}
