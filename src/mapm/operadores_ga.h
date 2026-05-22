#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include <random>
#include <vector>

// ─────────────────────────────────────────
// Selección — Prins et al. Sección 3.2
//
// Padre A: torneo binario entre los beta mejores de la población.
// Padre B: torneo binario sobre toda la población EXCEPTO padre A.
//
// "binary tournament": de los candidatos elegibles se toman 2 al
// azar y gana el de menor fitness.
// ─────────────────────────────────────────

// Retorna índice en pop del padre A
int seleccionar_padre_a(const std::vector<CromosomaLRP> &pop, int beta,
                        std::mt19937 &rng);

// Retorna índice en pop del padre B (excluye idx_a)
int seleccionar_padre_b(const std::vector<CromosomaLRP> &pop, int idx_a,
                        std::mt19937 &rng);

// ─────────────────────────────────────────
// Crossover — Prins et al. Sección 3.2
//
// DS : one-point crossover (igual que cromosomas binarios).
//      Se elige un punto de corte cp ∈ [1, m-1].
//      offspring.DS = DS(A)[0..cp-1] + DS(B)[cp..m-1]
//
// CS : order crossover adaptado para permutaciones
//      (Murata & Ishibuchi 1994, citado en el paper de Derbel;
//       Prins lo describe idénticamente en Sección 3.2):
//      - offspring.CS[0..cp-1] = CS(A)[0..cp-1]
//      - Recorrer CS(B) desde cp en adelante (circular);
//        agregar clientes no incluidos aún hasta completar n.
//
// Tras el crossover se aplica repair() para corregir
// infactibilidades introducidas por la mezcla de DS.
// ─────────────────────────────────────────
CromosomaLRP crossover(const CromosomaLRP &A, const CromosomaLRP &B,
                       const Matriz &mat, const InstanciaLRP &inst,
                       std::mt19937 &rng);

// ─────────────────────────────────────────
// AddInPop — reemplazar el peor individuo
//
// Inserta T en la población reemplazando al individuo de mayor
// fitness (el peor).  La población mantiene su tamaño constante.
// ─────────────────────────────────────────
void add_in_pop(std::vector<CromosomaLRP> &pop, const CromosomaLRP &T);
