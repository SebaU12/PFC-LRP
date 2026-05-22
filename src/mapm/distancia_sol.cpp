#include "distancia_sol.h"
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <unordered_set>

// ═════════════════════════════════════════
// Helpers internos
// ═════════════════════════════════════════

// Clave simétrica para un par de clientes (a,b), a != b.
// Usa a < b para evitar duplicados de orientación.
static inline int par_key(int a, int b, int n) {
    if (a > b) std::swap(a, b);
    return a * (n + 1) + b;
}

// ─────────────────────────────────────────
// Estructura de lookup construida desde RutasExplicitas de U.
//
// Para cada cliente c almacenamos:
//   dep_de[c]   → dep_idx al que pertenece (-1 si no está)
//   ruta_de[c]  → índice global de ruta (único por depósito+ruta)
//
// adj_u: conjunto de claves de pares adyacentes en la misma ruta.
// ─────────────────────────────────────────
struct LookupU {
    std::vector<int>          dep_de;   // [cliente 1-based]
    std::vector<int>          ruta_de;  // [cliente 1-based] — índice global
    std::unordered_set<int>   adj;      // pares adyacentes (clave simétrica)
    int n;
};

static LookupU construir_lookup(const RutasExplicitas &U, int n) {
    LookupU lu;
    lu.n       = n;
    lu.dep_de  .assign(n + 1, -1);
    lu.ruta_de .assign(n + 1, -1);
    lu.adj.reserve(n * 2);

    int id_ruta_global = 0;
    for (int d = 0; d < (int)U.rutas.size(); ++d) {
        for (const auto &ruta : U.rutas[d]) {
            if (ruta.empty()) { ++id_ruta_global; continue; }
            for (int c : ruta) {
                lu.dep_de [c] = d;
                lu.ruta_de[c] = id_ruta_global;
            }
            for (int k = 0; k + 1 < (int)ruta.size(); ++k)
                lu.adj.insert(par_key(ruta[k], ruta[k+1], n));
            ++id_ruta_global;
        }
    }
    return lu;
}

// ═════════════════════════════════════════
// distancia(T, U)   — Prins et al. 2006, Sección 3.4
// ═════════════════════════════════════════
int distancia(const RutasExplicitas &T,
              const RutasExplicitas &U,
              const InstanciaLRP   &inst)
{
    int n = inst.num_clientes;
    LookupU lu = construir_lookup(U, n);

    int total = 0;

    for (int d = 0; d < (int)T.rutas.size(); ++d) {
        for (const auto &ruta : T.rutas[d]) {
            for (int k = 0; k + 1 < (int)ruta.size(); ++k) {
                int a = ruta[k];
                int b = ruta[k + 1];

                int key = par_key(a, b, n);

                if (lu.adj.count(key)) {
                    // Caso 1: adyacentes en la misma ruta de U → 0
                    total += 0;
                } else if (lu.dep_de[a] != -1
                           && lu.dep_de[a] == lu.dep_de[b]
                           && lu.ruta_de[a] == lu.ruta_de[b]) {
                    // Caso 2: misma ruta de U, no adyacentes → 1
                    total += 1;
                } else if (lu.dep_de[a] != -1
                           && lu.dep_de[a] == lu.dep_de[b]
                           && lu.ruta_de[a] != lu.ruta_de[b]) {
                    // Caso 3: mismo depósito de U, distinta ruta → 5
                    total += 5;
                } else {
                    // Caso 4: depósitos distintos (o alguno sin asignar) → 10
                    total += 10;
                }
            }
        }
    }

    return total;
}

// ═════════════════════════════════════════
// distancia_poblacion(T, pop_rutas)
// ═════════════════════════════════════════
int distancia_poblacion(const RutasExplicitas              &T,
                        const std::vector<RutasExplicitas> &pop_rutas,
                        const InstanciaLRP                 &inst)
{
    int dmin = std::numeric_limits<int>::max();
    for (const auto &U : pop_rutas)
        dmin = std::min(dmin, distancia(T, U, inst));
    return dmin;
}

// ═════════════════════════════════════════
// Sobrecargas de conveniencia con CromosomaLRP
// ═════════════════════════════════════════

// Helper: CromosomaLRP → RutasExplicitas (evalúa + construye)
static RutasExplicitas crom_a_rutas(const CromosomaLRP &crom,
                                     const Matriz       &mat,
                                     const InstanciaLRP &inst)
{
    CromosomaLRP tmp = crom;            // copia para no mutar el original
    auto sr = evaluar_cromosoma(tmp, mat, inst);
    return construir_rutas_explicitas(tmp, sr, mat, inst);
}

int distancia(const CromosomaLRP &T,
              const CromosomaLRP &U,
              const Matriz       &mat,
              const InstanciaLRP &inst)
{
    return distancia(crom_a_rutas(T, mat, inst),
                     crom_a_rutas(U, mat, inst),
                     inst);
}

int distancia_poblacion(const CromosomaLRP              &T,
                        const std::vector<CromosomaLRP> &pop,
                        const Matriz                    &mat,
                        const InstanciaLRP              &inst)
{
    auto rt = crom_a_rutas(T, mat, inst);
    int dmin = std::numeric_limits<int>::max();
    for (const auto &U : pop)
        dmin = std::min(dmin, distancia(rt, crom_a_rutas(U, mat, inst), inst));
    return dmin;
}
