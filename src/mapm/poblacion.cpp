// =============================================================================
// poblacion.cpp — Heurísticas constructivas y gestión de población
// =============================================================================
#include "poblacion.h"
#include "distancia_sol.h"
#include <algorithm>
#include <limits>
#include <numeric>

// -----------------------------------------------------------------------------
// Convierte la estructura interna de construcción (orden_en_dep) al formato
// cromosoma. orden_en_dep[i] es la lista de clientes del depósito i en el
// orden en que se visitarán.
// -----------------------------------------------------------------------------
static CromosomaLRP
construir_cromosoma(const std::vector<std::vector<int>> &orden_en_dep,
                    const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  CromosomaLRP crom;
  crom.DS.assign(m, 0);
  crom.CS.reserve(n);

  int pos = 1;
  for (int i = 0; i < m; ++i) {
    if (orden_en_dep[i].empty()) continue;
    crom.DS[i] = pos;
    for (int c : orden_en_dep[i]) {
      crom.CS.push_back(c);
      ++pos;
    }
  }
  return crom;
}

// Suma la demanda de una lista de clientes. Usado para verificar capacidad.
static double demanda_lista(const std::vector<int> &lista,
                            const InstanciaLRP &inst) {
  double d = 0.0;
  for (int c : lista)
    d += inst.clientes[c - 1].demanda;
  return d;
}

// -----------------------------------------------------------------------------
// Nearest Neighbor aleatorizado.
//
// Para cada cliente (en orden aleatorio):
//   1. Buscar los depósitos que tengan capacidad disponible.
//   2. Ordenarlos por distancia al cliente.
//   3. Con probabilidad alpha_nn, elegir el 2do más cercano en vez del 1ro.
//   4. Si ningún depósito tiene capacidad, asignar al que tenga menor exceso.
//
// Luego, dentro de cada depósito, ordenar los clientes con un tour greedy
// de vecino más cercano (partiendo de un cliente aleatorio).
//
// El resultado es un cromosoma listo para evaluar con Split.
// -----------------------------------------------------------------------------
CromosomaLRP heuristica_nn(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_nn) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  std::vector<double> dem_dep(m, 0.0); // demanda acumulada por depósito
  std::vector<int> dep_de(n, -1);      // dep_de[c-1] = depósito asignado al cliente c

  // Procesar clientes en orden aleatorio para diversidad
  std::vector<int> orden_clientes(n);
  std::iota(orden_clientes.begin(), orden_clientes.end(), 1);
  std::shuffle(orden_clientes.begin(), orden_clientes.end(), rng);

  std::uniform_real_distribution<double> u01(0.0, 1.0);

  for (int c : orden_clientes) {
    int idx_c = c - 1;
    double dem_c = inst.clientes[idx_c].demanda;

    struct Cand { double dist; int dep_idx; };
    std::vector<Cand> cands;
    for (int i = 0; i < m; ++i) {
      if (dem_dep[i] + dem_c > inst.depositos[i].capacidad) continue;
      cands.push_back({mat[inst.num_clientes + i][idx_c], i});
    }

    if (cands.empty()) {
      int mejor = 0;
      double exceso = std::numeric_limits<double>::infinity();
      for (int i = 0; i < m; ++i) {
        double ex = dem_dep[i] + dem_c - inst.depositos[i].capacidad;
        if (ex < exceso) { exceso = ex; mejor = i; }
      }
      dep_de[idx_c] = mejor;
      dem_dep[mejor] += dem_c;
      continue;
    }

    std::sort(cands.begin(), cands.end(),
              [](const Cand &a, const Cand &b) { return a.dist < b.dist; });

    int elegido = 0;
    if (cands.size() >= 2 && u01(rng) < alpha_nn)
      elegido = 1;

    dep_de[idx_c] = cands[elegido].dep_idx;
    dem_dep[cands[elegido].dep_idx] += dem_c;
  }

  std::vector<std::vector<int>> orden_en_dep(m);
  {
    std::vector<std::vector<int>> grupos(m);
    for (int c = 1; c <= n; ++c)
      grupos[dep_de[c - 1]].push_back(c);

    for (int i = 0; i < m; ++i) {
      if (grupos[i].empty()) continue;
      std::vector<bool> visitado(grupos[i].size(), false);

      std::uniform_int_distribution<int> pick(0, (int)grupos[i].size() - 1);
      int cur_pos = pick(rng);
      visitado[cur_pos] = true;
      orden_en_dep[i].push_back(grupos[i][cur_pos]);

      for (int paso = 1; paso < (int)grupos[i].size(); ++paso) {
        int cur_c = grupos[i][cur_pos];
        double mejor_d = std::numeric_limits<double>::infinity();
        int mejor_pos = -1;
        for (int k = 0; k < (int)grupos[i].size(); ++k) {
          if (visitado[k]) continue;
          double d = mat[cur_c - 1][grupos[i][k] - 1];
          if (d < mejor_d) { mejor_d = d; mejor_pos = k; }
        }
        visitado[mejor_pos] = true;
        cur_pos = mejor_pos;
        orden_en_dep[i].push_back(grupos[i][cur_pos]);
      }
    }
  }

  return construir_cromosoma(orden_en_dep, inst);
}

// -----------------------------------------------------------------------------
// Clarke & Wright aleatorizado.
//
// Idea original (Clarke & Wright, 1964): en vez de que cada cliente tenga su
// propia ruta (dep→ci→dep), conviene fusionar dos rutas si los clientes ci y cj
// están cerca entre sí. El "ahorro" de fusionar es:
//   saving(ci, cj) = dist(dep, ci) + dist(dep, cj) - dist(ci, cj)
//
// Algoritmo:
//   1. Cada cliente empieza en su propia ruta (dep→ci→dep).
//   2. Calcular todos los savings entre pares de clientes del mismo depósito natural.
//   3. Ordenar de mayor a menor saving.
//   4. Para cada saving, intentar fusionar las dos rutas (si ambos clientes están
//      en los extremos de sus rutas, son del mismo depósito y caben en capacidad).
//      Con probabilidad alpha_cw, saltear el saving (introduce variedad).
//
// La versión "natural" de CW es determinista. La aleatorización (alpha_cw)
// produce soluciones distintas en cada llamada.
// -----------------------------------------------------------------------------
CromosomaLRP heuristica_cw(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_cw) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  std::vector<int> dep_natural(n);
  for (int c = 1; c <= n; ++c) {
    double mejor = std::numeric_limits<double>::infinity();
    int dep_m = 0;
    for (int i = 0; i < m; ++i) {
      double d = mat[inst.num_clientes + i][c - 1];
      if (d < mejor) { mejor = d; dep_m = i; }
    }
    dep_natural[c - 1] = dep_m;
  }

  struct Saving { double val; int ci, cj, dep_idx; };
  std::vector<Saving> savings;
  savings.reserve(n * n / 2);

  for (int ci = 1; ci <= n; ++ci) {
    for (int cj = ci + 1; cj <= n; ++cj) {
      int di = dep_natural[ci - 1];
      int dj = dep_natural[cj - 1];
      if (di != dj) continue;
      int dep_mat = inst.num_clientes + di;
      double s = mat[dep_mat][ci - 1] + mat[dep_mat][cj - 1] - mat[ci - 1][cj - 1];
      savings.push_back({s, ci, cj, di});
    }
  }

  std::sort(savings.begin(), savings.end(),
            [](const Saving &a, const Saving &b) { return a.val > b.val; });

  struct Ruta { std::vector<int> clientes; int dep_idx; double demanda; };
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
    if (u01(rng) < alpha_cw) continue;

    int ci = sv.ci, cj = sv.cj;
    int ri = ruta_de[ci - 1];
    int rj = ruta_de[cj - 1];

    if (ri == rj) continue;
    if (interior[ci - 1] || interior[cj - 1]) continue;
    if (rutas[ri].dep_idx != rutas[rj].dep_idx) continue;
    if (rutas[ri].demanda + rutas[rj].demanda > inst.Q) continue;

    auto &r_i = rutas[ri].clientes;
    auto &r_j = rutas[rj].clientes;

    bool ci_es_final = (r_i.back() == ci);
    bool cj_es_inicio = (r_j.front() == cj);
    bool ci_es_inicio = (r_i.front() == ci);
    bool cj_es_final = (r_j.back() == cj);

    if (!ci_es_final && !ci_es_inicio) continue;
    if (!cj_es_inicio && !cj_es_final) continue;

    if (ci_es_inicio) std::reverse(r_i.begin(), r_i.end());
    if (cj_es_final)  std::reverse(r_j.begin(), r_j.end());

    double dem_nueva = rutas[ri].demanda + rutas[rj].demanda;
    for (int c : r_j) {
      r_i.push_back(c);
      ruta_de[c - 1] = ri;
    }
    rutas[ri].demanda = dem_nueva;
    rutas[rj].clientes.clear();

    for (int k = 1; k < (int)r_i.size() - 1; ++k)
      interior[r_i[k] - 1] = true;
    interior[r_i.front() - 1] = false;
    interior[r_i.back() - 1] = false;
  }

  std::vector<std::vector<int>> orden_en_dep(m);
  for (const auto &r : rutas) {
    if (r.clientes.empty()) continue;
    for (int c : r.clientes)
      orden_en_dep[r.dep_idx].push_back(c);
  }

  return construir_cromosoma(orden_en_dep, inst);
}

// -----------------------------------------------------------------------------
// Genera la población inicial.
//
// Estrategia: mezclar NN y CW para maximizar diversidad de fenotipos.
//   - Primera mitad: NN (buena en problemas con clientes dispersos)
//   - Segunda mitad: CW (buena en problemas con clientes agrupados)
//
// Para garantizar diversidad, se rechaza cualquier candidato que esté a
// distancia < 1 de algún individuo ya aceptado (usando la métrica de
// distancia entre soluciones definida en distancia_sol.h).
//
// Si no puede llenar la población con individuos distintos (tras max_intentos),
// acepta duplicados para no bloquear la inicialización.
// -----------------------------------------------------------------------------
std::vector<CromosomaLRP>
generar_poblacion(int NbIndiv, const InstanciaLRP &inst, const Matriz &mat,
                  std::mt19937 &rng, const FnLocalSearch &ls) {
  std::vector<CromosomaLRP> pop;
  pop.reserve(NbIndiv);

  int mitad = NbIndiv / 2;
  int max_intentos = NbIndiv * 20;
  int intentos = 0;

  // Función de diversidad: rechazar si es demasiado parecido a uno ya aceptado
  auto aceptar = [&](const CromosomaLRP &cand) -> bool {
    for (const auto &existing : pop)
      if (distancia(cand, existing, mat, inst) < 1)
        return false;
    return true;
  };

  // Primera mitad: NN
  while ((int)pop.size() < mitad && intentos < max_intentos) {
    auto crom = heuristica_nn(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    if (aceptar(crom)) pop.push_back(std::move(crom));
    ++intentos;
  }

  // Segunda mitad: CW
  intentos = 0;
  while ((int)pop.size() < NbIndiv && intentos < max_intentos) {
    auto crom = heuristica_cw(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    if (aceptar(crom)) pop.push_back(std::move(crom));
    ++intentos;
  }

  // Fallback: completar sin filtro de diversidad si fue difícil llenar
  while ((int)pop.size() < NbIndiv) {
    auto crom = heuristica_nn(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    pop.push_back(std::move(crom));
  }

  return pop;
}

// -----------------------------------------------------------------------------
// GenPop2 del paper: refresca la población cuando el loop exterior termina un ciclo.
//
// ¿Por qué refrescar?
//   Después de muchas generaciones, la población puede converger: todos los
//   individuos se parecen demasiado entre sí y el crossover ya no produce
//   descendencia nueva. El MA|PM detecta esto contando NbNoAdd (cuántos hijos
//   seguidos fueron rechazados por ser muy similares).
//
//   Al refrescar, se descarta todo excepto el mejor individuo y se regenera
//   la población con nuevas soluciones diversas. El mejor se preserva para no
//   perder el trabajo ya hecho.
// -----------------------------------------------------------------------------
void refrescar_poblacion(std::vector<CromosomaLRP> &pop,
                         const InstanciaLRP &inst, const Matriz &mat,
                         std::mt19937 &rng, const FnLocalSearch &ls) {
  if (pop.empty()) return;

  // Preservar el mejor individuo actual
  auto it_best = std::min_element(
      pop.begin(), pop.end(),
      [](const CromosomaLRP &a, const CromosomaLRP &b) {
        return a.fitness < b.fitness;
      });
  CromosomaLRP mejor = *it_best;

  int NbIndiv = (int)pop.size();
  pop.clear();
  pop.push_back(mejor); // el mejor sobrevive

  int intentos = 0;
  int max_intentos = NbIndiv * 20;
  int mitad = NbIndiv / 2;

  auto aceptar = [&](const CromosomaLRP &cand) -> bool {
    for (const auto &e : pop)
      if (distancia(cand, e, mat, inst) < 1)
        return false;
    return true;
  };

  while ((int)pop.size() < NbIndiv && intentos < max_intentos) {
    CromosomaLRP crom = ((int)pop.size() <= mitad)
                            ? heuristica_nn(inst, mat, rng)
                            : heuristica_cw(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    if (aceptar(crom)) pop.push_back(std::move(crom));
    ++intentos;
  }

  while ((int)pop.size() < NbIndiv) {
    auto crom = heuristica_nn(inst, mat, rng);
    evaluar_cromosoma(crom, mat, inst);
    ls(crom, mat, inst);
    pop.push_back(std::move(crom));
  }
}
