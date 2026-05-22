#pragma once
#include "../parser.h"
#include "cromosoma.h"
#include "local_search.h" // RutasExplicitas
#include "split.h"        // evaluar_cromosoma

// ─────────────────────────────────────────
// Medida de distancia entre dos soluciones — Prins et al. 2006, Sección 3.4
//
// Para cada par de clientes CONSECUTIVOS (i,j) en una RUTA REAL de T
// (post-Split), se cuenta según su relación en U (también post-Split):
//
//   Caso 1: (i,j) o (j,i) son adyacentes en la misma ruta de U  →  0
//   Caso 2: i,j están en la misma ruta de U (no adyacentes)      →  1
//   Caso 3: i,j en el mismo depósito de U pero distinta ruta      →  5
//   Caso 4: i,j en depósitos distintos en U                       → 10
//
// D(T,T) = 0.  D(T,U) >= 0.
// La desigualdad triangular NO se cumple en general.
//
// NOTA: usa RutasExplicitas (post-Split) para ambas soluciones,
//       de modo que "misma ruta" y "mismo depósito distinta ruta"
//       son casos distinguibles (5 vs 1 puntos).
// ─────────────────────────────────────────

// Distancia entre T y U usando sus rutas post-Split.
int distancia(const RutasExplicitas &T, const RutasExplicitas &U,
              const InstanciaLRP &inst);

// Distancia de T al conjunto de soluciones de la población.
// = min_{U in pop} distancia(T, U)
int distancia_poblacion(const RutasExplicitas &T,
                        const std::vector<RutasExplicitas> &pop_rutas,
                        const InstanciaLRP &inst);

// ─────────────────────────────────────────
// Sobrecargas de conveniencia con CromosomaLRP
//
// Construyen internamente las RutasExplicitas vía Split y delegan
// a las funciones principales. Útiles en poblacion.cpp y tests donde
// no se necesita reutilizar el lookup de rutas.
// ─────────────────────────────────────────
int distancia(const CromosomaLRP &T, const CromosomaLRP &U, const Matriz &mat,
              const InstanciaLRP &inst);

int distancia_poblacion(const CromosomaLRP &T,
                        const std::vector<CromosomaLRP> &pop, const Matriz &mat,
                        const InstanciaLRP &inst);
