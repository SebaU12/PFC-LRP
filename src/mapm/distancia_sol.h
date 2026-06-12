// Medida de distancia entre soluciones (Prins et al. 2006, Sección 3.4).
// Puntúa pares de clientes consecutivos en T según su relación en U:
//   adyacentes misma ruta → 0, misma ruta no adyacentes → 1,
//   mismo depósito distinta ruta → 5, depósitos distintos → 10.
#pragma once
#include "../parser.h"
#include "cromosoma.h"
#include "local_search.h"
#include "split.h"

int distancia(const RutasExplicitas &T, const RutasExplicitas &U,
              const InstanciaLRP &inst);

int distancia_poblacion(const RutasExplicitas &T,
                        const std::vector<RutasExplicitas> &pop_rutas,
                        const InstanciaLRP &inst);

// Sobrecargas de conveniencia (construyen RutasExplicitas internamente).
int distancia(const CromosomaLRP &T, const CromosomaLRP &U, const Matriz &mat,
              const InstanciaLRP &inst);
int distancia_poblacion(const CromosomaLRP &T,
                        const std::vector<CromosomaLRP> &pop, const Matriz &mat,
                        const InstanciaLRP &inst);
