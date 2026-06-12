// =============================================================================
// local_search.cpp — Implementación de las vecindades y el loop LS1/LS2
// =============================================================================
#include "local_search.h"
#include "split.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// Costo de una ruta: F + dist(dep, c1) + ... + dist(cp, dep).
static double costo_ruta(const RutaVec &r, int dep_idx, const Matriz &mat,
                         const InstanciaLRP &inst) {
  if (r.empty()) return 0.0;
  double c = inst.F + mat[dep_idx][r.front() - 1];
  for (int i = 0; i + 1 < (int)r.size(); ++i)
    c += mat[r[i] - 1][r[i + 1] - 1];
  c += mat[r.back() - 1][dep_idx];
  return c;
}

// Demanda total de los clientes en una ruta.
static double demanda_ruta(const RutaVec &r, const InstanciaLRP &inst) {
  double d = 0.0;
  for (int c : r)
    d += inst.clientes[c - 1].demanda;
  return d;
}

// -----------------------------------------------------------------------------
// Construye el RutasExplicitas desde el cromosoma y los resultados del Split.
// Suma los costos de apertura y routing de cada depósito abierto.
// (mat no se usa aquí porque el costo ya está en split_res.)
// -----------------------------------------------------------------------------
RutasExplicitas
construir_rutas_explicitas(const CromosomaLRP &crom,
                           const std::vector<SplitResultado> &split_res,
                           const Matriz &mat, const InstanciaLRP &inst) {
  (void)mat;
  int m = inst.num_depositos;
  RutasExplicitas re;
  re.rutas.resize(m);
  re.costo = 0.0;
  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0) continue;
    re.costo += inst.depositos[i].costo_apertura;
    re.rutas[i] = split_res[i].rutas;
    re.costo += split_res[i].costo;
  }
  return re;
}

// Recalcula el costo completo desde cero: útil después de modificar rutas.
double recalcular_costo(const RutasExplicitas &re, const Matriz &mat,
                        const InstanciaLRP &inst) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;
  double c = 0.0;
  for (int i = 0; i < m; ++i) {
    if (re.rutas[i].empty()) continue;
    c += inst.depositos[i].costo_apertura;
    int dep_idx = n + i;
    for (const auto &r : re.rutas[i])
      c += costo_ruta(r, dep_idx, mat, inst);
  }
  return c;
}

// -----------------------------------------------------------------------------
// MOVE: mueve un cliente de su posición actual a cualquier otra posición
// en cualquier ruta (de cualquier depósito si solo_intra=false).
//
// Por cada cliente cli en la ruta (di, ri, pi), prueba insertarlo en todas
// las posiciones de todas las rutas válidas (que tengan capacidad). Si alguna
// alternativa baja el costo total, aplica el movimiento y retorna true.
//
// Estrategia first-improvement: retorna al primer movimiento que mejora.
// -----------------------------------------------------------------------------
bool move_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;
  double Q = inst.Q;

  // Precalcular carga actual de cada depósito (R4: sum d_j ≤ W_i).
  // Solo necesario para movimientos inter-depósito (LS1).
  std::vector<double> carga_dep(m, 0.0);
  if (!solo_intra) {
    for (int i = 0; i < m; ++i)
      for (const auto &r : re.rutas[i])
        for (int c : r) carga_dep[i] += inst.clientes[c - 1].demanda;
  }

  for (int di = 0; di < m; ++di) {
    if (re.rutas[di].empty()) continue;
    int dep_i = n + di;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      for (int pi = 0; pi < (int)re.rutas[di][ri].size(); ++pi) {
        int cli = re.rutas[di][ri][pi];

        for (int dj = 0; dj < m; ++dj) {
          if (solo_intra && dj != di) continue;
          if (re.rutas[dj].empty() && dj != di) continue;
          int dep_j = n + dj;

          // R4: al mover cli a otro depósito, verificar que dj tenga capacidad
          double dem_cli = inst.clientes[cli - 1].demanda;
          if (dj != di &&
              carga_dep[dj] + dem_cli > inst.depositos[dj].capacidad + 1e-9)
            continue;

          for (int rj = 0; rj < (int)re.rutas[dj].size(); ++rj) {
            if (di == dj && ri == rj) continue;
            if (demanda_ruta(re.rutas[dj][rj], inst) +
                    inst.clientes[cli - 1].demanda > Q)
              continue;

            for (int pj = 0; pj <= (int)re.rutas[dj][rj].size(); ++pj) {
              RutasExplicitas tmp = re;
              tmp.rutas[di][ri].erase(tmp.rutas[di][ri].begin() + pi);
              tmp.rutas[dj][rj].insert(tmp.rutas[dj][rj].begin() + pj, cli);
              if (tmp.rutas[di][ri].empty())
                tmp.rutas[di].erase(tmp.rutas[di].begin() + ri);

              double nuevo = recalcular_costo(tmp, mat, inst);
              if (nuevo < re.costo - 1e-9) {
                re = std::move(tmp);
                re.costo = nuevo;
                return true;
              }
            }
          }

          if (!solo_intra && dj != di &&
              carga_dep[dj] + dem_cli <= inst.depositos[dj].capacidad + 1e-9) {
            RutasExplicitas tmp = re;
            tmp.rutas[di][ri].erase(tmp.rutas[di][ri].begin() + pi);
            if (tmp.rutas[di][ri].empty())
              tmp.rutas[di].erase(tmp.rutas[di].begin() + ri);
            tmp.rutas[dj].push_back({cli});

            double nuevo = recalcular_costo(tmp, mat, inst);
            if (nuevo < re.costo - 1e-9) {
              re = std::move(tmp);
              re.costo = nuevo;
              return true;
            }
          }
        }
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
// SWAP: intercambia dos clientes de posiciones distintas.
//
// Por cada par de clientes (ca, cb) en posiciones diferentes, verifica que
// ambas rutas sigan siendo factibles después del intercambio (capacidad Q).
// Si el intercambio baja el costo total, lo aplica y retorna true.
//
// Evitar comparar un cliente consigo mismo y optimizar recorriendo solo
// pares (di,ri,pi) < (dj,rj,pj) para no probar el mismo par dos veces.
// -----------------------------------------------------------------------------
bool swap_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;
  double Q = inst.Q;

  for (int di = 0; di < m; ++di) {
    if (re.rutas[di].empty()) continue;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      for (int pi = 0; pi < (int)re.rutas[di][ri].size(); ++pi) {
        int ca = re.rutas[di][ri][pi];
        double dem_a = inst.clientes[ca - 1].demanda;

        for (int dj = di; dj < m; ++dj) {
          if (solo_intra && dj != di) continue;
          if (re.rutas[dj].empty()) continue;
          (void)(n);

          int rj_start = (dj == di) ? ri : 0;
          for (int rj = rj_start; rj < (int)re.rutas[dj].size(); ++rj) {
            int pj_start = (dj == di && rj == ri) ? pi + 1 : 0;

            for (int pj = pj_start; pj < (int)re.rutas[dj][rj].size(); ++pj) {
              int cb = re.rutas[dj][rj][pj];
              if (ca == cb) continue;
              double dem_b = inst.clientes[cb - 1].demanda;

              double dem_ra = demanda_ruta(re.rutas[di][ri], inst);
              double dem_rb = demanda_ruta(re.rutas[dj][rj], inst);
              if (dem_ra - dem_a + dem_b > Q + 1e-9) continue;
              if (dem_rb - dem_b + dem_a > Q + 1e-9) continue;
              if (di == dj && ri == rj && std::abs(pi - pj) == 1) continue;

              RutasExplicitas tmp = re;
              tmp.rutas[di][ri][pi] = cb;
              tmp.rutas[dj][rj][pj] = ca;

              double nuevo = recalcular_costo(tmp, mat, inst);
              if (nuevo < re.costo - 1e-9) {
                re = std::move(tmp);
                re.costo = nuevo;
                return true;
              }
            }
          }
        }
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
// 2-OPT: dos variantes.
//
// Intra-ruta (siempre disponible):
//   Dado un segmento [i..j] dentro de una ruta, invierte su orden.
//   Beneficio: elimina "cruces" en la ruta que aumentan la distancia.
//   Delta = -dist(prev_i, r[i]) - dist(r[j], next_j)
//           +dist(prev_i, r[j]) + dist(r[i], next_j)
//   Si delta < 0, invertir mejora.
//
// Inter-ruta mismo depósito (solo si !solo_intra):
//   Intercambiar los sufijos de dos rutas del mismo depósito.
//   ra = [a0..ai, ai+1..] y rb = [b0..bj, bj+1..]
//   → nueva_ra = [a0..ai, bj+1..] y nueva_rb = [b0..bj, ai+1..]
//   Se verifica capacidad Q antes de aplicar.
// -----------------------------------------------------------------------------
bool opt2_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;

  // 2-opt intra-ruta
  for (int di = 0; di < m; ++di) {
    if (re.rutas[di].empty()) continue;
    int dep_i = n + di;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      auto &r = re.rutas[di][ri];
      int sz = (int)r.size();
      if (sz < 2) continue;

      for (int i = 0; i < sz - 1; ++i) {
        for (int j = i + 1; j < sz; ++j) {
          int pi = (i == 0) ? dep_i : r[i - 1] - 1;
          int nj = (j == sz - 1) ? dep_i : r[j + 1] - 1;
          double delta = -mat[pi][r[i] - 1] - mat[r[j] - 1][nj] +
                         mat[pi][r[j] - 1] + mat[r[i] - 1][nj];
          if (delta < -1e-9) {
            std::reverse(r.begin() + i, r.begin() + j + 1);
            re.costo = recalcular_costo(re, mat, inst);
            return true;
          }
        }
      }
    }
  }

  if (solo_intra) return false;

  // 2-opt inter-ruta mismo depósito
  for (int di = 0; di < m; ++di) {
    if ((int)re.rutas[di].size() < 2) continue;
    int dep_i = n + di;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      const auto &ra = re.rutas[di][ri];
      if (ra.empty()) continue;

      for (int rj = ri + 1; rj < (int)re.rutas[di].size(); ++rj) {
        const auto &rb = re.rutas[di][rj];
        if (rb.empty()) continue;

        for (int i = 0; i < (int)ra.size(); ++i) {
          int a_next = (i + 1 < (int)ra.size()) ? ra[i + 1] - 1 : dep_i;

          for (int j = 0; j < (int)rb.size(); ++j) {
            int b_next = (j + 1 < (int)rb.size()) ? rb[j + 1] - 1 : dep_i;
            double delta = -mat[ra[i] - 1][a_next] - mat[rb[j] - 1][b_next] +
                           mat[ra[i] - 1][b_next] + mat[rb[j] - 1][a_next];
            if (delta >= -1e-9) continue;

            RutaVec nueva_ra(ra.begin(), ra.begin() + i + 1);
            nueva_ra.insert(nueva_ra.end(), rb.begin() + j + 1, rb.end());
            RutaVec nueva_rb(rb.begin(), rb.begin() + j + 1);
            nueva_rb.insert(nueva_rb.end(), ra.begin() + i + 1, ra.end());

            if (demanda_ruta(nueva_ra, inst) > inst.Q) continue;
            if (demanda_ruta(nueva_rb, inst) > inst.Q) continue;

            re.rutas[di][ri] = std::move(nueva_ra);
            re.rutas[di][rj] = std::move(nueva_rb);
            if (re.rutas[di][rj].empty())
              re.rutas[di].erase(re.rutas[di].begin() + rj);
            if (re.rutas[di][ri].empty())
              re.rutas[di].erase(re.rutas[di].begin() + ri);

            re.costo = recalcular_costo(re, mat, inst);
            return true;
          }
        }
      }
    }
  }
  return false;
}

// -----------------------------------------------------------------------------
// Loop interno de búsqueda local: encadena MOVE → SWAP → 2-OPT
// con evaluación lazy (||: evalúa el siguiente solo si el anterior no mejoró).
//
// Si alguno de los tres mejora, se reinicia desde MOVE. Termina cuando ninguno
// de los tres encuentra mejora en un ciclo completo (óptimo local para estas tres
// vecindades).
//
// solo_intra controla si se permiten movimientos entre depósitos (LS1 vs LS2).
// Al terminar, sincroniza el cromosoma DS+CS con las rutas finales.
// -----------------------------------------------------------------------------
static double ls_loop(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
                      const Matriz &mat, const InstanciaLRP &inst, bool solo_intra) {
  RutasExplicitas re = construir_rutas_explicitas(crom, split_res, mat, inst);
  double costo_inicial = re.costo;

  while (true) {
    double costo_antes = re.costo;
    // Probar en orden: si MOVE mejora, volver a empezar; si no, probar SWAP, etc.
    bool alguna = move_operator(re, mat, inst, solo_intra)
               || swap_operator(re, mat, inst, solo_intra)
               || opt2_operator(re, mat, inst, solo_intra);
    if (!alguna || re.costo >= costo_antes - 1e-9) break; // óptimo local alcanzado
  }

  actualizar_cromosoma_desde_rutas(crom, re, inst);
  return costo_inicial - re.costo; // delta de mejora (≥ 0)
}

// LS1: búsqueda completa inter e intra depósito
double LS1(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
           const Matriz &mat, const InstanciaLRP &inst) {
  return ls_loop(crom, split_res, mat, inst, false);
}

// LS2: búsqueda solo intra-depósito (más rápida)
double LS2(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
           const Matriz &mat, const InstanciaLRP &inst) {
  return ls_loop(crom, split_res, mat, inst, true);
}

// -----------------------------------------------------------------------------
// Sincroniza el cromosoma DS+CS con el estado de las rutas explícitas.
// Se llama después de que la búsqueda local modifica las rutas.
// Reconstruye DS (qué depósitos están abiertos y desde qué posición en CS)
// y CS (concatenación de todas las rutas en orden de depósito).
// -----------------------------------------------------------------------------
void actualizar_cromosoma_desde_rutas(CromosomaLRP &crom,
                                      const RutasExplicitas &re,
                                      const InstanciaLRP &inst) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;

  crom.DS.assign(m, 0);
  crom.CS.clear();
  crom.CS.reserve(n);

  int ptr = 1; // posición 1-based en CS
  for (int i = 0; i < m; ++i) {
    if (re.rutas[i].empty()) continue; // depósito sin rutas = cerrado
    crom.DS[i] = ptr;
    for (const auto &r : re.rutas[i])
      for (int c : r) { crom.CS.push_back(c); ++ptr; }
  }
  crom.fitness = re.costo;
}
