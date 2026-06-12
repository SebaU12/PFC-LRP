// Fuerza bruta exacta para LRP pequeños.
// Held-Karp + DP set-cover por depósito. Buffers pre-alojados (arreglos planos 1D)
// para evitar miles de heap-allocations por llamada.
#include "solver.h"
#include <algorithm>
#include <vector>

static constexpr double INF = 1e18;

// ─── Workspace pre-alojado ────────────────────────────────────────────────────
// Todos los buffers tienen tamaño fijo para n_max clientes por depósito.
struct VRPWorkspace {
  int n_max;
  int sz;                    // 1 << n_max
  std::vector<double> sdmnd; // sz
  std::vector<double> hk;    // sz * n_max  — acceso: hk[mask*n_max + last]
  std::vector<int>    hk_prev;
  std::vector<double> route_cost;
  std::vector<int>    route_last;
  std::vector<double> dp;
  std::vector<int>    dp_prev;
  std::vector<int>    dp_route;

  explicit VRPWorkspace(int n_max)
      : n_max(n_max), sz(1 << n_max),
        sdmnd(sz), hk(sz * n_max), hk_prev(sz * n_max),
        route_cost(sz), route_last(sz), dp(sz), dp_prev(sz), dp_route(sz) {}
};

// ─── VRP exacto (usa workspace ya alojado) ────────────────────────────────────
// clients : índices 0-based en la matriz
// n       : cantidad de clientes (puede ser < ws.n_max)
// dep_idx : índice 0-based del depósito en la matriz
// Tipo local: secuencia de IDs de clientes 0-based (solo interno a este solver).
using BFRuta = std::vector<int>;
struct VRPResult { double cost; std::vector<BFRuta> rutas; };

static VRPResult vrp_exact(const std::vector<int> &clients, int n,
                            const std::vector<double> &demands, int dep_idx,
                            const Matriz &mat, double Q, double F,
                            VRPWorkspace &ws) {
  if (n == 0) return {0.0, {}};

  int sz = 1 << n;
  int nm = ws.n_max; // stride para indexar hk/hk_prev

  // ── 1. Demanda por subconjunto ────────────────────────────────────────────
  ws.sdmnd[0] = 0.0;
  for (int mask = 1; mask < sz; ++mask) {
    int lsb = mask & (-mask);
    int i   = __builtin_ctz(lsb);
    ws.sdmnd[mask] = ws.sdmnd[mask ^ lsb] + demands[i];
  }

  // ── 2. Held-Karp ──────────────────────────────────────────────────────────
  // hk[mask*nm+last] = min costo: depósito → visitar todos en mask → clients[last]
  for (int k = 0; k < sz * nm; ++k) ws.hk[k] = INF;
  for (int k = 0; k < sz * nm; ++k) ws.hk_prev[k] = -1;

  for (int i = 0; i < n; ++i)
    if (ws.sdmnd[1 << i] <= Q)
      ws.hk[(1 << i) * nm + i] = mat[dep_idx][clients[i]];

  for (int S = 1; S < sz; ++S) {
    if (ws.sdmnd[S] > Q) continue;
    for (int i = 0; i < n; ++i) {
      if (!(S & (1 << i))) continue;
      if (S == (1 << i))   continue; // base inicializado arriba
      int pS = S ^ (1 << i);
      double best = INF;
      int    best_j = -1;
      for (int j = 0; j < n; ++j) {
        if (!(pS & (1 << j))) continue;
        double v = ws.hk[pS * nm + j];
        if (v >= INF / 2) continue;
        double c = v + mat[clients[j]][clients[i]];
        if (c < best) { best = c; best_j = j; }
      }
      if (best_j >= 0) {
        ws.hk[S * nm + i]      = best;
        ws.hk_prev[S * nm + i] = best_j;
      }
    }
  }

  // ── 3. Costo de ruta (ida y vuelta) ──────────────────────────────────────
  for (int mask = 0; mask < sz; ++mask) ws.route_cost[mask] = INF;

  for (int mask = 1; mask < sz; ++mask) {
    if (ws.sdmnd[mask] > Q) continue;
    for (int last = 0; last < n; ++last) {
      if (!(mask & (1 << last))) continue;
      double v = ws.hk[mask * nm + last];
      if (v >= INF / 2) continue;
      double c = F + v + mat[clients[last]][dep_idx];
      if (c < ws.route_cost[mask]) {
        ws.route_cost[mask] = c;
        ws.route_last[mask] = last;
      }
    }
  }

  // ── 4. VRP DP set-cover ───────────────────────────────────────────────────
  for (int k = 0; k < sz; ++k) { ws.dp[k] = INF; ws.dp_prev[k] = -1; ws.dp_route[k] = -1; }
  ws.dp[0] = 0.0;

  for (int mask = 0; mask < sz; ++mask) {
    if (ws.dp[mask] >= INF / 2) continue;
    int remaining = (sz - 1) & ~mask;
    for (int route = remaining; route > 0; route = (route - 1) & remaining) {
      if (ws.route_cost[route] >= INF / 2) continue;
      int    new_mask  = mask | route;
      double new_cost  = ws.dp[mask] + ws.route_cost[route];
      if (new_cost < ws.dp[new_mask]) {
        ws.dp[new_mask]      = new_cost;
        ws.dp_prev[new_mask] = mask;
        ws.dp_route[new_mask]= route;
      }
    }
  }

  if (ws.dp[sz - 1] >= INF / 2) return {INF, {}};

  // ── 5. Reconstruir rutas ──────────────────────────────────────────────────
  std::vector<BFRuta> rutas;
  int cur = sz - 1;
  while (cur != 0) {
    int rmask     = ws.dp_route[cur];
    int prev_mask = ws.dp_prev[cur];

    BFRuta ruta;
    int last = ws.route_last[rmask];
    int m    = rmask;
    while (m != 0) {
      ruta.push_back(clients[last] + 1); // ID 1-indexed global
      int pl = ws.hk_prev[m * nm + last];
      m ^= (1 << last);
      last = pl;
      if (last == -1) break;
    }
    std::reverse(ruta.begin(), ruta.end());
    rutas.push_back(std::move(ruta));
    cur = prev_mask;
  }

  return {ws.dp[sz - 1], rutas};
}

// ─── AlgoritmFuerzaBruta::ejecutar ───────────────────────────────────────────

ResultadoAlgoritmo
AlgoritmFuerzaBruta::ejecutar(const EstadoLRP &sol_inicial) {
  const InstanciaLRP &inst = *sol_inicial.datos;
  const Matriz       &mat  = *sol_inicial.matriz;
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Pre-alojar workspace para n clientes (máximo posible por depósito)
  VRPWorkspace ws(n);

  double           best_cost = INF;
  std::vector<int> best_dep_ids;
  Rutas            best_rutas_map;

  ResultadoAlgoritmo res{sol_inicial.copy(), sol_inicial.objective(), INF, 0, {}};

  // ── Subconjuntos de depósitos ─────────────────────────────────────────────
  for (int dep_mask = 1; dep_mask < (1 << m); ++dep_mask) {
    std::vector<int> open_deps;
    double open_cost = 0.0;
    for (int d = 0; d < m; ++d) {
      if (dep_mask & (1 << d)) {
        open_deps.push_back(d);
        open_cost += inst.depositos[d].costo_apertura;
      }
    }
    int k = (int)open_deps.size();

    // ── Asignaciones: clientes → depósitos abiertos ───────────────────────
    std::vector<int> assignment(n, 0);
    bool done = false;

    while (!done) {
      res.iteraciones++;

      // Verificar capacidad de depósito
      std::vector<double> dep_dem(k, 0.0);
      for (int i = 0; i < n; ++i)
        dep_dem[assignment[i]] += inst.clientes[i].demanda;

      bool cap_ok = true;
      for (int d = 0; d < k; ++d)
        if (dep_dem[d] > inst.depositos[open_deps[d]].capacidad) { cap_ok = false; break; }

      if (cap_ok) {
        double routing_cost = 0.0;
        std::vector<std::pair<int, std::vector<Ruta>>> dep_routes; // dep_global → rutas convertidas
        bool feasible = true;

        for (int d = 0; d < k && feasible; ++d) {
          int dep_global  = open_deps[d];
          int dep_mat_idx = n + dep_global;

          std::vector<int>    dc;
          std::vector<double> dd;
          for (int i = 0; i < n; ++i)
            if (assignment[i] == d) { dc.push_back(i); dd.push_back(inst.clientes[i].demanda); }

          auto vr = vrp_exact(dc, (int)dc.size(), dd, dep_mat_idx, mat, inst.Q, inst.F, ws);
          if (vr.cost >= INF / 2) feasible = false;
          else {
            routing_cost += vr.cost;
            // Convertir BFRuta → Ruta struct antes de guardar
            int dep_id_tmp = n + dep_global + 1;
            std::vector<Ruta> rutas_conv;
            for (const auto &bfr : vr.rutas)
              rutas_conv.push_back(Ruta(dep_id_tmp, bfr));
            dep_routes.push_back({dep_global, std::move(rutas_conv)});
          }
        }

        if (feasible) {
          double total = open_cost + routing_cost;
          if (total < best_cost) {
            best_cost = total;
            best_dep_ids.clear();
            best_rutas_map.clear();
            for (auto &[dep_g, routes] : dep_routes) {
              int dep_id = n + dep_g + 1;
              best_dep_ids.push_back(dep_id);
              best_rutas_map[dep_id] = std::move(routes);
            }
            res.historial_mejoras.push_back({res.iteraciones, best_cost});
          }
        }
      }

      // Avanzar al siguiente assignment
      int pos = n - 1;
      while (pos >= 0 && assignment[pos] == k - 1) { assignment[pos] = 0; pos--; }
      if (pos < 0) done = true; else assignment[pos]++;
    }
  }

  if (best_cost < INF) {
    res.mejor_estado = EstadoLRP(best_rutas_map, best_dep_ids, {},
                                 *sol_inicial.matriz, *sol_inicial.datos);
    res.costo_final  = best_cost;
  }
  return res;
}
