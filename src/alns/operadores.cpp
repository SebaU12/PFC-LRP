#include "operadores.h"
#include "../solucion_inicial.h" // demanda_ruta_externa
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// ─────────────────────────────────────────
// Helpers internos
// ─────────────────────────────────────────

// Lista todos los clientes actualmente en rutas
static std::vector<int> clientes_ruteados(const EstadoLRP &e) {
  std::vector<int> v;
  for (const auto &[dep, lista] : e.rutas)
    for (const auto &r : lista)
      for (int c : r)
        v.push_back(c);
  return v;
}

// Elimina un cliente de sus rutas y lo pone en no_asignados
static void remover_cliente(EstadoLRP &e, int cliente) {
  for (auto &[dep, lista] : e.rutas) {
    for (auto &r : lista) {
      auto it = std::find(r.begin(), r.end(), cliente);
      if (it != r.end()) {
        r.erase(it);
        e.no_asignados.push_back(cliente);
        return;
      }
    }
  }
}

// Elimina rutas vacías de todos los depósitos
static void limpiar_rutas_vacias(EstadoLRP &e) {
  for (auto &[dep, lista] : e.rutas) {
    lista.erase(std::remove_if(lista.begin(), lista.end(),
                               [](const Ruta &r) { return r.empty(); }),
                lista.end());
  }
}

// Índice sesgado hacia los primeros elementos: int(U^3 * n)
// (espejo exacto del Python para shaw y worst)
static int idx_sesgado(double u, int n) {
  return static_cast<int>(std::pow(u, 3.0) * n);
}

// ═════════════════════════════════════════
// OPERADORES DE DESTRUCCIÓN
// ═════════════════════════════════════════

// ── 1. Random Removal ────────────────────
EstadoLRP random_removal(EstadoLRP estado, std::mt19937 &rng) {
  auto ruteados = clientes_ruteados(estado);
  if (ruteados.empty())
    return estado;

  int q = std::max(1, static_cast<int>(estado.datos->num_clientes * 0.20));
  q = std::min(q, (int)ruteados.size());

  std::shuffle(ruteados.begin(), ruteados.end(), rng);
  for (int i = 0; i < q; ++i)
    remover_cliente(estado, ruteados[i]);

  limpiar_rutas_vacias(estado);
  return estado;
}

// ── 2. Worst Removal ─────────────────────
EstadoLRP worst_removal(EstadoLRP estado, std::mt19937 &rng) {
  int q = std::max(1, static_cast<int>(estado.datos->num_clientes * 0.20));
  const Matriz &mat = *estado.matriz;

  // Calcular ahorro de eliminar cada cliente
  std::vector<std::pair<double, int>> ahorros; // (ahorro, cliente_id)
  for (const auto &[dep, lista] : estado.rutas) {
    int idx_dep = dep - 1;
    for (const auto &ruta : lista) {
      for (int i = 0; i < (int)ruta.size(); ++i) {
        int prev = (i == 0) ? idx_dep : ruta[i - 1] - 1;
        int curr = ruta[i] - 1;
        int next = (i == (int)ruta.size() - 1) ? idx_dep : ruta[i + 1] - 1;

        double costo_orig = mat[prev][curr] + mat[curr][next];
        double costo_nuevo = mat[prev][next];
        ahorros.push_back({costo_orig - costo_nuevo, ruta[i]});
      }
    }
  }
  if (ahorros.empty())
    return estado;

  // Ordenar descendente por ahorro
  std::sort(ahorros.begin(), ahorros.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  std::uniform_real_distribution<double> dist(0.0, 1.0);
  int removidos = 0;
  while (removidos < q && !ahorros.empty()) {
    int idx = idx_sesgado(dist(rng), (int)ahorros.size());
    int victima = ahorros[idx].second;
    ahorros.erase(ahorros.begin() + idx);
    // Eliminar también otras entradas del mismo cliente
    ahorros.erase(std::remove_if(
                      ahorros.begin(), ahorros.end(),
                      [victima](const auto &p) { return p.second == victima; }),
                  ahorros.end());
    remover_cliente(estado, victima);
    ++removidos;
  }

  limpiar_rutas_vacias(estado);
  return estado;
}

// ── 3. Shaw Removal ──────────────────────
EstadoLRP shaw_removal(EstadoLRP estado, std::mt19937 &rng) {
  auto ruteados = clientes_ruteados(estado);
  if (ruteados.empty())
    return estado;

  int q = std::max(1, static_cast<int>(estado.datos->num_clientes * 0.20));
  const Matriz &mat = *estado.matriz;

  std::uniform_int_distribution<int> pick(0, (int)ruteados.size() - 1);
  int nodo_inicial = ruteados[pick(rng)];
  std::vector<int> victimas = {nodo_inicial};
  ruteados.erase(std::find(ruteados.begin(), ruteados.end(), nodo_inicial));

  std::uniform_real_distribution<double> dist(0.0, 1.0);
  while ((int)victimas.size() < q && !ruteados.empty()) {
    // Elegir un nodo base aleatorio de las víctimas ya elegidas
    std::uniform_int_distribution<int> base_pick(0, (int)victimas.size() - 1);
    int nodo_base = victimas[base_pick(rng)];

    // Ordenar candidatos por distancia al nodo base
    std::sort(ruteados.begin(), ruteados.end(), [&](int a, int b) {
      return mat[nodo_base - 1][a - 1] < mat[nodo_base - 1][b - 1];
    });

    int idx = idx_sesgado(dist(rng), (int)ruteados.size());
    victimas.push_back(ruteados[idx]);
    ruteados.erase(ruteados.begin() + idx);
  }

  for (int v : victimas)
    remover_cliente(estado, v);
  limpiar_rutas_vacias(estado);
  return estado;
}

// ── 4. Route Removal ─────────────────────
EstadoLRP route_removal(EstadoLRP estado, std::mt19937 &rng) {
  // Recopilar rutas activas como (dep_id, índice_en_lista)
  std::vector<std::pair<int, int>> activas;
  for (const auto &[dep, lista] : estado.rutas)
    for (int i = 0; i < (int)lista.size(); ++i)
      if (!lista[i].empty())
        activas.push_back({dep, i});

  if (activas.empty())
    return estado;

  int min_r = std::max(1, (int)(activas.size() * 0.10));
  int max_r = std::max(2, (int)(activas.size() * 0.30));
  std::uniform_int_distribution<int> pick(min_r, max_r);
  int n_destruir = std::min(pick(rng), (int)activas.size());

  std::shuffle(activas.begin(), activas.end(), rng);
  for (int i = 0; i < n_destruir; ++i) {
    auto &[dep, ri] = activas[i];
    for (int c : estado.rutas[dep][ri])
      estado.no_asignados.push_back(c);
    estado.rutas[dep][ri].clear();
  }

  limpiar_rutas_vacias(estado);
  return estado;
}

// ── 5. Depot Closing ─────────────────────
EstadoLRP depot_closing(EstadoLRP estado, std::mt19937 &rng) {
  if ((int)estado.depositos_abiertos.size() <= 1)
    return estado;

  std::uniform_int_distribution<int> pick(
      0, (int)estado.depositos_abiertos.size() - 1);
  int idx = pick(rng);
  int dep_cerrar = estado.depositos_abiertos[idx];

  // Liberar clientes de ese depósito
  for (const auto &r : estado.rutas[dep_cerrar])
    for (int c : r)
      estado.no_asignados.push_back(c);

  estado.rutas.erase(dep_cerrar);
  estado.depositos_abiertos.erase(estado.depositos_abiertos.begin() + idx);
  return estado;
}

// ═════════════════════════════════════════
// OPERADORES DE REPARACIÓN
// ═════════════════════════════════════════

// Helper compartido: mejor inserción de un cliente en el estado actual.
// Retorna (costo, dep_id, idx_ruta, pos). idx_ruta==-1 → nueva ruta.
static std::tuple<double, int, int, int> mejor_insercion(int cliente,
                                                         const EstadoLRP &e) {
  const Matriz &mat = *e.matriz;
  const double Q = e.datos->Q;
  const double F = e.datos->F;
  int idx_c = cliente - 1;
  double dem_c = e.datos->clientes[idx_c].demanda;

  double mejor = std::numeric_limits<double>::infinity();
  int m_dep = -1, m_ri = -1, m_pos = -1;

  for (int dep_id : e.depositos_abiertos) {
    int idx_dep = dep_id - 1;
    const auto &lista = e.rutas.at(dep_id);

    for (int ri = 0; ri < (int)lista.size(); ++ri) {
      const auto &ruta = lista[ri];
      if (demanda_ruta_externa(ruta, *e.datos) + dem_c > Q)
        continue;

      for (int pos = 0; pos <= (int)ruta.size(); ++pos) {
        int prev = (pos == 0) ? idx_dep : ruta[pos - 1] - 1;
        int next = (pos == (int)ruta.size()) ? idx_dep : ruta[pos] - 1;
        double c = mat[prev][idx_c] + mat[idx_c][next] - mat[prev][next];
        if (c < mejor) {
          mejor = c;
          m_dep = dep_id;
          m_ri = ri;
          m_pos = pos;
        }
      }
    }

    // Nueva ruta
    double c_nueva = mat[idx_dep][idx_c] * 2.0 + F;
    if (c_nueva < mejor) {
      mejor = c_nueva;
      m_dep = dep_id;
      m_ri = -1;
      m_pos = 0;
    }
  }
  return {mejor, m_dep, m_ri, m_pos};
}

static void aplicar_insercion(EstadoLRP &e, int cliente, int dep_id, int ri,
                              int pos) {
  if (ri == -1) {
    e.rutas[dep_id].push_back({cliente});
  } else {
    auto &ruta = e.rutas[dep_id][ri];
    ruta.insert(ruta.begin() + pos, cliente);
  }
  e.no_asignados.erase(
      std::find(e.no_asignados.begin(), e.no_asignados.end(), cliente));
}

// ── Greedy Repair ────────────────────────
EstadoLRP greedy_repair(EstadoLRP estado, std::mt19937 &rng) {
  // Shuffle para romper empates de forma aleatoria (igual que Python)
  std::shuffle(estado.no_asignados.begin(), estado.no_asignados.end(), rng);

  while (!estado.no_asignados.empty()) {
    int cliente = estado.no_asignados[0];
    auto [costo, dep, ri, pos] = mejor_insercion(cliente, estado);
    if (dep == -1)
      break; // no debería ocurrir
    aplicar_insercion(estado, cliente, dep, ri, pos);
  }
  return estado;
}

// ── Regret-2 Repair ──────────────────────
EstadoLRP regret2_repair(EstadoLRP estado, std::mt19937 &rng) {
  (void)rng; // regret es determinista dado el estado

  while (!estado.no_asignados.empty()) {
    // Para cada cliente calcular su "arrepentimiento":
    // diferencia entre su 2do mejor y su mejor costo de inserción
    std::vector<std::pair<double, int>> regrets; // (regret, cliente)

    for (int cliente : estado.no_asignados) {
      const Matriz &mat = *estado.matriz;
      const double Q = estado.datos->Q;
      const double F = estado.datos->F;
      int idx_c = cliente - 1;
      double dem_c = estado.datos->clientes[idx_c].demanda;

      std::vector<double> costos;

      for (int dep_id : estado.depositos_abiertos) {
        int idx_dep = dep_id - 1;
        const auto &lista = estado.rutas.at(dep_id);

        for (int ri = 0; ri < (int)lista.size(); ++ri) {
          const auto &ruta = lista[ri];
          if (demanda_ruta_externa(ruta, *estado.datos) + dem_c > Q)
            continue;
          for (int pos = 0; pos <= (int)ruta.size(); ++pos) {
            int prev = (pos == 0) ? idx_dep : ruta[pos - 1] - 1;
            int next = (pos == (int)ruta.size()) ? idx_dep : ruta[pos] - 1;
            costos.push_back(mat[prev][idx_c] + mat[idx_c][next] -
                             mat[prev][next]);
          }
        }
        costos.push_back(mat[idx_dep][idx_c] * 2.0 + F);
      }

      std::sort(costos.begin(), costos.end());
      double regret = (costos.size() >= 2)
                          ? costos[1] - costos[0]
                          : std::numeric_limits<double>::infinity();
      regrets.push_back({regret, cliente});
    }

    // Insertar el cliente con mayor arrepentimiento
    auto it = std::max_element(
        regrets.begin(), regrets.end(),
        [](const auto &a, const auto &b) { return a.first < b.first; });
    int cliente = it->second;
    auto [costo, dep, ri, pos] = mejor_insercion(cliente, estado);
    if (dep == -1)
      break;
    aplicar_insercion(estado, cliente, dep, ri, pos);
  }
  return estado;
}
