#include "local_search.h"
#include "split.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

// ═════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════

static double costo_ruta(const Ruta &r, int dep_idx, const Matriz &mat,
                         const InstanciaLRP &inst) {
  if (r.empty())
    return 0.0;
  double c = inst.F + mat[dep_idx][r.front() - 1];
  for (int i = 0; i + 1 < (int)r.size(); ++i)
    c += mat[r[i] - 1][r[i + 1] - 1];
  c += mat[r.back() - 1][dep_idx];
  return c;
}

static double demanda_ruta(const Ruta &r, const InstanciaLRP &inst) {
  double d = 0.0;
  for (int c : r)
    d += inst.clientes[c - 1].demanda;
  return d;
}

// ═════════════════════════════════════════
// construir_rutas_explicitas
// ═════════════════════════════════════════
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
    if (crom.DS[i] == 0)
      continue;
    re.costo += inst.depositos[i].costo_apertura;
    re.rutas[i] = split_res[i].rutas;
    re.costo += split_res[i].costo;
  }
  return re;
}

// ═════════════════════════════════════════
// recalcular_costo — siempre desde cero
// ═════════════════════════════════════════
double recalcular_costo(const RutasExplicitas &re, const Matriz &mat,
                        const InstanciaLRP &inst) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;
  double c = 0.0;
  for (int i = 0; i < m; ++i) {
    if (re.rutas[i].empty())
      continue;
    c += inst.depositos[i].costo_apertura;
    int dep_idx = n + i;
    for (const auto &r : re.rutas[i])
      c += costo_ruta(r, dep_idx, mat, inst);
  }
  return c;
}

// ═════════════════════════════════════════
// move_operator  (MOVE — first improvement)
// Usa recalcular_costo para evitar drift numérico.
// ═════════════════════════════════════════
bool move_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;
  double Q = inst.Q;

  for (int di = 0; di < m; ++di) {
    if (re.rutas[di].empty())
      continue;
    int dep_i = n + di;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      for (int pi = 0; pi < (int)re.rutas[di][ri].size(); ++pi) {
        int cli = re.rutas[di][ri][pi];

        for (int dj = 0; dj < m; ++dj) {
          if (solo_intra && dj != di)
            continue;
          if (re.rutas[dj].empty() && dj != di)
            continue;
          int dep_j = n + dj;

          // ── Insertar en ruta existente ────────────
          for (int rj = 0; rj < (int)re.rutas[dj].size(); ++rj) {
            if (di == dj && ri == rj)
              continue;

            if (demanda_ruta(re.rutas[dj][rj], inst) +
                    inst.clientes[cli - 1].demanda >
                Q)
              continue;

            for (int pj = 0; pj <= (int)re.rutas[dj][rj].size(); ++pj) {
              // Copia, aplica, mide
              RutasExplicitas tmp = re;
              tmp.rutas[di][ri].erase(tmp.rutas[di][ri].begin() + pi);
              tmp.rutas[dj][rj].insert(tmp.rutas[dj][rj].begin() + pj, cli);

              // Limpiar ruta vacía
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

          // ── Nueva ruta en dj ─────────────────────
          if (!solo_intra && dj != di) {
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

// ═════════════════════════════════════════
// swap_operator  (SWAP — first improvement)
// ═════════════════════════════════════════
bool swap_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;
  double Q = inst.Q;

  for (int di = 0; di < m; ++di) {
    if (re.rutas[di].empty())
      continue;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      for (int pi = 0; pi < (int)re.rutas[di][ri].size(); ++pi) {
        int ca = re.rutas[di][ri][pi];
        double dem_a = inst.clientes[ca - 1].demanda;

        for (int dj = di; dj < m; ++dj) {
          if (solo_intra && dj != di)
            continue;
          if (re.rutas[dj].empty())
            continue;
          (void)(n); // dep_j no usado directamente aquí

          int rj_start = (dj == di) ? ri : 0;
          for (int rj = rj_start; rj < (int)re.rutas[dj].size(); ++rj) {
            int pj_start = (dj == di && rj == ri) ? pi + 1 : 0;

            for (int pj = pj_start; pj < (int)re.rutas[dj][rj].size(); ++pj) {
              int cb = re.rutas[dj][rj][pj];
              if (ca == cb)
                continue;
              double dem_b = inst.clientes[cb - 1].demanda;

              // Verificar capacidad
              double dem_ra = demanda_ruta(re.rutas[di][ri], inst);
              double dem_rb = demanda_ruta(re.rutas[dj][rj], inst);
              if (dem_ra - dem_a + dem_b > Q + 1e-9)
                continue;
              if (dem_rb - dem_b + dem_a > Q + 1e-9)
                continue;

              // Saltar adyacentes en misma ruta
              if (di == dj && ri == rj && std::abs(pi - pj) == 1)
                continue;

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

// ═════════════════════════════════════════
// opt2_operator  (2-OPT — first improvement)
// ═════════════════════════════════════════
bool opt2_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;

  // ── Intra-ruta ────────────────────────────────────────
  for (int di = 0; di < m; ++di) {
    if (re.rutas[di].empty())
      continue;
    int dep_i = n + di;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      auto &r = re.rutas[di][ri];
      int sz = (int)r.size();
      if (sz < 2)
        continue;

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

  if (solo_intra)
    return false;

  // ── Inter-ruta mismo depósito: intercambio de sufijos ─
  for (int di = 0; di < m; ++di) {
    if ((int)re.rutas[di].size() < 2)
      continue;
    int dep_i = n + di;

    for (int ri = 0; ri < (int)re.rutas[di].size(); ++ri) {
      const auto &ra = re.rutas[di][ri];
      if (ra.empty())
        continue;

      for (int rj = ri + 1; rj < (int)re.rutas[di].size(); ++rj) {
        const auto &rb = re.rutas[di][rj];
        if (rb.empty())
          continue;

        for (int i = 0; i < (int)ra.size(); ++i) {
          int a_next = (i + 1 < (int)ra.size()) ? ra[i + 1] - 1 : dep_i;

          for (int j = 0; j < (int)rb.size(); ++j) {
            int b_next = (j + 1 < (int)rb.size()) ? rb[j + 1] - 1 : dep_i;

            double delta = -mat[ra[i] - 1][a_next] - mat[rb[j] - 1][b_next] +
                           mat[ra[i] - 1][b_next] + mat[rb[j] - 1][a_next];

            if (delta >= -1e-9)
              continue;

            Ruta nueva_ra(ra.begin(), ra.begin() + i + 1);
            nueva_ra.insert(nueva_ra.end(), rb.begin() + j + 1, rb.end());
            Ruta nueva_rb(rb.begin(), rb.begin() + j + 1);
            nueva_rb.insert(nueva_rb.end(), ra.begin() + i + 1, ra.end());

            if (demanda_ruta(nueva_ra, inst) > inst.Q)
              continue;
            if (demanda_ruta(nueva_rb, inst) > inst.Q)
              continue;

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

// ═════════════════════════════════════════
// ls_loop
// ═════════════════════════════════════════
static double ls_loop(CromosomaLRP &crom,
                      std::vector<SplitResultado> &split_res, const Matriz &mat,
                      const InstanciaLRP &inst, bool solo_intra) {
  RutasExplicitas re = construir_rutas_explicitas(crom, split_res, mat, inst);
  double costo_inicial = re.costo;

  while (true) {
    double costo_antes = re.costo;
    bool alguna = false;
    if (move_operator(re, mat, inst, solo_intra))
      alguna = true;
    if (swap_operator(re, mat, inst, solo_intra))
      alguna = true;
    if (opt2_operator(re, mat, inst, solo_intra))
      alguna = true;
    if (!alguna)
      break;
    if (re.costo >= costo_antes - 1e-9)
      break;
  }

  actualizar_cromosoma_desde_rutas(crom, re, inst);
  return costo_inicial - re.costo;
}

double LS1(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
           const Matriz &mat, const InstanciaLRP &inst) {
  return ls_loop(crom, split_res, mat, inst, false);
}

double LS2(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
           const Matriz &mat, const InstanciaLRP &inst) {
  return ls_loop(crom, split_res, mat, inst, true);
}

// ═════════════════════════════════════════
// actualizar_cromosoma_desde_rutas
// ═════════════════════════════════════════
void actualizar_cromosoma_desde_rutas(CromosomaLRP &crom,
                                      const RutasExplicitas &re,
                                      const InstanciaLRP &inst) {
  int m = inst.num_depositos;
  int n = inst.num_clientes;

  crom.DS.assign(m, 0);
  crom.CS.clear();
  crom.CS.reserve(n);

  int ptr = 1;
  for (int i = 0; i < m; ++i) {
    if (re.rutas[i].empty())
      continue;
    crom.DS[i] = ptr;
    for (const auto &r : re.rutas[i])
      for (int c : r) {
        crom.CS.push_back(c);
        ++ptr;
      }
  }
  crom.fitness = re.costo;
}
