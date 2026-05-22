#include "poblacion.h"
#include "distancia_sol.h"
#include <algorithm>
#include <limits>
#include <numeric>

// ═════════════════════════════════════════
// Helpers internos
// ═════════════════════════════════════════

// Construye DS y CS a partir de una asignación explícita
// dep_de[c-1] = índice 0-based del depósito al que va el cliente c
// orden_en_dep[i] = lista ordenada de clientes del depósito i
static CromosomaLRP
construir_cromosoma(const std::vector<std::vector<int>> &orden_en_dep,
                    const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  CromosomaLRP crom;
  crom.DS.assign(m, 0);
  crom.CS.reserve(n);

  int pos = 1; // posición 1-based en CS
  for (int i = 0; i < m; ++i) {
    if (orden_en_dep[i].empty())
      continue;
    crom.DS[i] = pos;
    for (int c : orden_en_dep[i]) {
      crom.CS.push_back(c);
      ++pos;
    }
  }
  return crom;
}

// Demanda total de una lista de clientes
static double demanda_lista(const std::vector<int> &lista,
                            const InstanciaLRP &inst) {
  double d = 0.0;
  for (int c : lista)
    d += inst.clientes[c - 1].demanda;
  return d;
}

// ═════════════════════════════════════════
// Heurística 1 — Nearest Neighbor
// ═════════════════════════════════════════
CromosomaLRP heuristica_nn(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_nn) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // ── Paso 1: asignar cada cliente al depósito más cercano
  // con capacidad disponible.
  // Con probabilidad alpha_nn elegimos entre los 2 mejores
  // candidatos para diversificar.
  std::vector<double> dem_dep(m, 0.0); // demanda acumulada por depósito
  std::vector<int> dep_de(n, -1);      // dep_de[c-1] = dep_idx asignado

  // Orden aleatorio de clientes para romper empates
  std::vector<int> orden_clientes(n);
  std::iota(orden_clientes.begin(), orden_clientes.end(), 1); // IDs 1..n
  std::shuffle(orden_clientes.begin(), orden_clientes.end(), rng);

  std::uniform_real_distribution<double> u01(0.0, 1.0);

  for (int c : orden_clientes) {
    int idx_c = c - 1;
    double dem_c = inst.clientes[idx_c].demanda;

    // Candidatos: depósitos con capacidad suficiente, ordenados por dist
    struct Cand {
      double dist;
      int dep_idx;
    };
    std::vector<Cand> cands;
    for (int i = 0; i < m; ++i) {
      if (dem_dep[i] + dem_c > inst.depositos[i].capacidad)
        continue;
      int dep_mat = inst.num_clientes + i;
      cands.push_back({mat[dep_mat][idx_c], i});
    }

    if (cands.empty()) {
      // Forzar al depósito con menos exceso
      int mejor = 0;
      double exceso = std::numeric_limits<double>::infinity();
      for (int i = 0; i < m; ++i) {
        double ex = dem_dep[i] + dem_c - inst.depositos[i].capacidad;
        if (ex < exceso) {
          exceso = ex;
          mejor = i;
        }
      }
      dep_de[idx_c] = mejor;
      dem_dep[mejor] += dem_c;
      continue;
    }

    std::sort(cands.begin(), cands.end(),
              [](const Cand &a, const Cand &b) { return a.dist < b.dist; });

    // Perturbación: con prob alpha_nn elegir el 2do mejor si existe
    int elegido = 0;
    if (cands.size() >= 2 && u01(rng) < alpha_nn)
      elegido = 1;

    dep_de[idx_c] = cands[elegido].dep_idx;
    dem_dep[cands[elegido].dep_idx] += dem_c;
  }

  // ── Paso 2: ordenar clientes de cada depósito por NN
  std::vector<std::vector<int>> orden_en_dep(m);
  {
    // Agrupar clientes por depósito
    std::vector<std::vector<int>> grupos(m);
    for (int c = 1; c <= n; ++c)
      grupos[dep_de[c - 1]].push_back(c);

    for (int i = 0; i < m; ++i) {
      if (grupos[i].empty())
        continue;
      std::vector<bool> visitado(grupos[i].size(), false);

      // Partir de un cliente aleatorio del grupo
      std::uniform_int_distribution<int> pick(0, (int)grupos[i].size() - 1);
      int cur_pos = pick(rng);
      visitado[cur_pos] = true;
      orden_en_dep[i].push_back(grupos[i][cur_pos]);

      for (int paso = 1; paso < (int)grupos[i].size(); ++paso) {
        int cur_c = grupos[i][cur_pos];
        double mejor_d = std::numeric_limits<double>::infinity();
        int mejor_pos = -1;
        for (int k = 0; k < (int)grupos[i].size(); ++k) {
          if (visitado[k])
            continue;
          double d = mat[cur_c - 1][grupos[i][k] - 1];
          if (d < mejor_d) {
            mejor_d = d;
            mejor_pos = k;
          }
        }
        visitado[mejor_pos] = true;
        cur_pos = mejor_pos;
        orden_en_dep[i].push_back(grupos[i][cur_pos]);
      }
    }
  }

  return construir_cromosoma(orden_en_dep, inst);
}

// ═════════════════════════════════════════
// Heurística 2 — Clarke & Wright extendido
// ═════════════════════════════════════════
CromosomaLRP heuristica_cw(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_cw) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // ── Paso 1: para cada cliente, encontrar su depósito más cercano
  // como depósito "natural". Esto define el saving de referencia.
  std::vector<int> dep_natural(n); // dep_natural[c-1] = dep_idx más cercano
  for (int c = 1; c <= n; ++c) {
    double mejor = std::numeric_limits<double>::infinity();
    int dep_m = 0;
    for (int i = 0; i < m; ++i) {
      double d = mat[inst.num_clientes + i][c - 1];
      if (d < mejor) {
        mejor = d;
        dep_m = i;
      }
    }
    dep_natural[c - 1] = dep_m;
  }

  // ── Paso 2: calcular savings s(i,j) para cada par de clientes
  // del mismo depósito natural.
  // s(i,j) = dist(dep,i) + dist(dep,j) - dist(i,j)
  struct Saving {
    double val;
    int ci, cj; // IDs 1-indexed
    int dep_idx;
  };
  std::vector<Saving> savings;
  savings.reserve(n * n / 2);

  for (int ci = 1; ci <= n; ++ci) {
    for (int cj = ci + 1; cj <= n; ++cj) {
      int di = dep_natural[ci - 1];
      int dj = dep_natural[cj - 1];
      if (di != dj)
        continue; // solo clientes del mismo depósito natural

      int dep_mat = inst.num_clientes + di;
      double s =
          mat[dep_mat][ci - 1] + mat[dep_mat][cj - 1] - mat[ci - 1][cj - 1];
      savings.push_back({s, ci, cj, di});
    }
  }

  std::sort(savings.begin(), savings.end(),
            [](const Saving &a, const Saving &b) { return a.val > b.val; });

  // ── Paso 3: construir rutas por fusión de savings (CW clásico)
  // Cada cliente parte en su propia ruta
  // ruta_de[c-1] = índice en `rutas` de la ruta que contiene c
  // interior[c-1] = true si c está en el interior de su ruta
  struct Ruta {
    std::vector<int> clientes; // orden de visita
    int dep_idx;
    double demanda;
  };
  std::vector<Ruta> rutas;
  rutas.reserve(n);
  std::vector<int> ruta_de(n, -1);
  std::vector<bool> interior(n, false);

  for (int c = 1; c <= n; ++c) {
    ruta_de[c - 1] = (int)rutas.size();
    rutas.push_back({{c}, dep_natural[c - 1], inst.clientes[c - 1].demanda});
  }

  std::uniform_real_distribution<double> u01(0.0, 1.0);

  for (const auto &sv : savings) {
    // Perturbación: con prob alpha_cw saltar este saving
    if (u01(rng) < alpha_cw)
      continue;

    int ci = sv.ci, cj = sv.cj;
    int ri = ruta_de[ci - 1];
    int rj = ruta_de[cj - 1];

    if (ri == rj)
      continue; // ya en la misma ruta
    if (interior[ci - 1] || interior[cj - 1])
      continue; // no son extremos
    if (rutas[ri].dep_idx != rutas[rj].dep_idx)
      continue; // distinto dep

    // Verificar capacidad de vehículo
    if (rutas[ri].demanda + rutas[rj].demanda > inst.Q)
      continue;

    // Verificar capacidad del depósito
    int dep_i = rutas[ri].dep_idx;
    double dem_dep_actual = 0.0;
    for (int k = 0; k < n; ++k)
      if (ruta_de[k] != -1 && (int)rutas[ruta_de[k]].dep_idx == dep_i)
        dem_dep_actual += inst.clientes[k].demanda;
    // La fusión no cambia la demanda del depósito, solo la de las rutas

    // Fusionar: ci debe ser el final de ri, cj el inicio de rj
    // (o viceversa — intentar ambas orientaciones)
    auto &r_i = rutas[ri].clientes;
    auto &r_j = rutas[rj].clientes;

    bool ci_es_final = (r_i.back() == ci);
    bool cj_es_inicio = (r_j.front() == cj);
    bool ci_es_inicio = (r_i.front() == ci);
    bool cj_es_final = (r_j.back() == cj);

    if (!ci_es_final && !ci_es_inicio)
      continue;
    if (!cj_es_inicio && !cj_es_final)
      continue;

    // Orientar para que ci quede al final de ri y cj al inicio de rj
    if (ci_es_inicio)
      std::reverse(r_i.begin(), r_i.end());
    if (cj_es_final)
      std::reverse(r_j.begin(), r_j.end());

    // Fusionar rj al final de ri
    double dem_nueva = rutas[ri].demanda + rutas[rj].demanda;
    for (int c : r_j) {
      r_i.push_back(c);
      ruta_de[c - 1] = ri;
    }
    rutas[ri].demanda = dem_nueva;
    rutas[rj].clientes.clear(); // marcar como vacía

    // Actualizar interior: todos los clientes de ri excepto
    // el primero y el último son interiores
    for (int k = 1; k < (int)r_i.size() - 1; ++k)
      interior[r_i[k] - 1] = true;
    interior[r_i.front() - 1] = false;
    interior[r_i.back() - 1] = false;
  }

  // ── Paso 4: construir orden_en_dep desde las rutas resultantes
  std::vector<std::vector<int>> orden_en_dep(m);
  for (const auto &r : rutas) {
    if (r.clientes.empty())
      continue;
    for (int c : r.clientes)
      orden_en_dep[r.dep_idx].push_back(c);
  }

  return construir_cromosoma(orden_en_dep, inst);
}

// ═════════════════════════════════════════
// generar_poblacion
// ═════════════════════════════════════════
std::vector<CromosomaLRP>
generar_poblacion(int NbIndiv, const InstanciaLRP &inst, const Matriz &mat,
                  std::mt19937 &rng, const FnLocalSearch &ls) {

  std::vector<CromosomaLRP> pop;
  pop.reserve(NbIndiv);

  int mitad = NbIndiv / 2;
  int max_intentos = NbIndiv * 20;
  int intentos = 0;

  auto aceptar = [&](const CromosomaLRP &cand) -> bool {
    for (const auto &existing : pop)
      if (distancia(cand, existing, mat, inst) < 1) // ← agrega mat
        return false;
    return true;
  };

  // Primera mitad: NN
  while ((int)pop.size() < mitad && intentos < max_intentos) {
    auto crom = heuristica_nn(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    if (aceptar(crom))
      pop.push_back(std::move(crom));
    ++intentos;
  }

  // Segunda mitad: CW
  intentos = 0;
  while ((int)pop.size() < NbIndiv && intentos < max_intentos) {
    auto crom = heuristica_cw(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    if (aceptar(crom))
      pop.push_back(std::move(crom));
    ++intentos;
  }

  // Rellenar sin restricción si es necesario (instancias muy pequeñas)
  while ((int)pop.size() < NbIndiv) {
    auto crom = heuristica_nn(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    pop.push_back(std::move(crom));
  }

  return pop;
}

// ═════════════════════════════════════════
// refrescar_poblacion  (GenPop2)
// ═════════════════════════════════════════
void refrescar_poblacion(std::vector<CromosomaLRP> &pop,
                         const InstanciaLRP &inst, const Matriz &mat,
                         std::mt19937 &rng, const FnLocalSearch &ls) {
  if (pop.empty())
    return;

  // Encontrar el mejor individuo
  auto it_best = std::min_element(
      pop.begin(), pop.end(), [](const CromosomaLRP &a, const CromosomaLRP &b) {
        return a.fitness < b.fitness;
      });
  CromosomaLRP mejor = *it_best;

  int NbIndiv = (int)pop.size();

  // Regenerar toda la población excepto el mejor
  pop.clear();
  pop.push_back(mejor);

  int intentos = 0;
  int max_intentos = NbIndiv * 20;
  int mitad = NbIndiv / 2;

  auto aceptar = [&](const CromosomaLRP &cand) -> bool {
    for (const auto &e : pop)
      if (distancia(cand, e, mat, inst) < 1) // ← agrega mat
        return false;
    return true;
  };

  while ((int)pop.size() < NbIndiv && intentos < max_intentos) {
    CromosomaLRP crom;
    if ((int)pop.size() <= mitad)
      crom = heuristica_nn(inst, mat, rng);
    else
      crom = heuristica_cw(inst, mat, rng);

    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    if (aceptar(crom))
      pop.push_back(std::move(crom));
    ++intentos;
  }

  // Rellenar sin restricción si es necesario
  while ((int)pop.size() < NbIndiv) {
    auto crom = heuristica_nn(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    pop.push_back(std::move(crom));
  }
}
