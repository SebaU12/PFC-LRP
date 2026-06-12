// Medida de distancia entre soluciones (Prins et al. 2006, Sección 3.4).
#include "distancia_sol.h"
#include <algorithm>
#include <limits>
#include <unordered_set>

static inline int par_key(int a, int b, int n) {
  if (a > b) std::swap(a, b);
  return a * (n + 1) + b;
}

struct LookupU {
  std::vector<int> dep_de;   // [cliente 1-based]
  std::vector<int> ruta_de;  // [cliente 1-based] — índice global
  std::unordered_set<int> adj;
  int n;
};

static LookupU construir_lookup(const RutasExplicitas &U, int n) {
  LookupU lu;
  lu.n = n;
  lu.dep_de.assign(n + 1, -1);
  lu.ruta_de.assign(n + 1, -1);
  lu.adj.reserve(n * 2);

  int id_ruta_global = 0;
  for (int d = 0; d < (int)U.rutas.size(); ++d) {
    for (const auto &ruta : U.rutas[d]) {
      if (ruta.empty()) { ++id_ruta_global; continue; }
      for (int c : ruta) {
        lu.dep_de[c]  = d;
        lu.ruta_de[c] = id_ruta_global;
      }
      for (int k = 0; k + 1 < (int)ruta.size(); ++k)
        lu.adj.insert(par_key(ruta[k], ruta[k + 1], n));
      ++id_ruta_global;
    }
  }
  return lu;
}

int distancia(const RutasExplicitas &T, const RutasExplicitas &U,
              const InstanciaLRP &inst) {
  int n = inst.num_clientes;
  LookupU lu = construir_lookup(U, n);
  int total = 0;

  for (int d = 0; d < (int)T.rutas.size(); ++d) {
    for (const auto &ruta : T.rutas[d]) {
      for (int k = 0; k + 1 < (int)ruta.size(); ++k) {
        int a = ruta[k], b = ruta[k + 1];
        int key = par_key(a, b, n);

        if (lu.adj.count(key)) {
          // adyacentes misma ruta → 0
        } else if (lu.dep_de[a] != -1 && lu.dep_de[a] == lu.dep_de[b] &&
                   lu.ruta_de[a] == lu.ruta_de[b]) {
          total += 1; // misma ruta, no adyacentes
        } else if (lu.dep_de[a] != -1 && lu.dep_de[a] == lu.dep_de[b] &&
                   lu.ruta_de[a] != lu.ruta_de[b]) {
          total += 5; // mismo depósito, distinta ruta
        } else {
          total += 10; // depósitos distintos
        }
      }
    }
  }
  return total;
}

int distancia_poblacion(const RutasExplicitas &T,
                        const std::vector<RutasExplicitas> &pop_rutas,
                        const InstanciaLRP &inst) {
  int dmin = std::numeric_limits<int>::max();
  for (const auto &U : pop_rutas)
    dmin = std::min(dmin, distancia(T, U, inst));
  return dmin;
}

static RutasExplicitas crom_a_rutas(const CromosomaLRP &crom,
                                    const Matriz &mat,
                                    const InstanciaLRP &inst) {
  CromosomaLRP tmp = crom;
  auto sr = evaluar_cromosoma(tmp, mat, inst);
  return construir_rutas_explicitas(tmp, sr, mat, inst);
}

int distancia(const CromosomaLRP &T, const CromosomaLRP &U,
              const Matriz &mat, const InstanciaLRP &inst) {
  return distancia(crom_a_rutas(T, mat, inst), crom_a_rutas(U, mat, inst), inst);
}

int distancia_poblacion(const CromosomaLRP &T,
                        const std::vector<CromosomaLRP> &pop,
                        const Matriz &mat, const InstanciaLRP &inst) {
  auto rt = crom_a_rutas(T, mat, inst);
  int dmin = std::numeric_limits<int>::max();
  for (const auto &U : pop)
    dmin = std::min(dmin, distancia(rt, crom_a_rutas(U, mat, inst), inst));
  return dmin;
}
