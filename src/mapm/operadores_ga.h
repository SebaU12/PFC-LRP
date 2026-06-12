// =============================================================================
// operadores_ga.h — Operadores genéticos del MA|PM
// =============================================================================
//
// Aquí viven los operadores clásicos de un algoritmo genético adaptados para
// el cromosoma DS+CS del LRP (Prins et al. 2006, Sección 3.2).
//
// SELECCIÓN: torneo binario — elegir al azar dos candidatos, quedarse con el mejor.
//   Es simple, rápido, y controla la presión selectiva vía el parámetro beta.
//
// CROSSOVER: combinar DS y CS de dos padres para crear un hijo.
//   El reto es que DS y CS están acoplados: si mezclas DS de A con CS de B
//   directamente, las posiciones pueden no coincidir. Se usan dos crossovers:
//     - One-point en DS: copiar los primeros cp depósitos de A, el resto de B.
//     - Order crossover en CS: copiar los primeros cp clientes de A en orden,
//       luego completar con los de B en el orden en que aparecen (sin repetir).
//
// INSERCIÓN: cuando un hijo es aceptado en la población, reemplaza al peor.
//
// =============================================================================
#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include <random>
#include <vector>

// -----------------------------------------------------------------------------
// Selecciona el padre A: torneo binario entre los beta mejores de la población.
// Usar solo los beta mejores (en vez de toda la población) reduce el riesgo de
// seleccionar individuos muy malos como padre.
// Devuelve el índice en pop del padre elegido.
// -----------------------------------------------------------------------------
int seleccionar_padre_a(const std::vector<CromosomaLRP> &pop, int beta,
                        std::mt19937 &rng);

// -----------------------------------------------------------------------------
// Selecciona el padre B: torneo binario sobre toda la población excepto idx_a.
// Se excluye idx_a para que el cruce sea entre dos individuos distintos.
// Devuelve el índice en pop del padre elegido.
// -----------------------------------------------------------------------------
int seleccionar_padre_b(const std::vector<CromosomaLRP> &pop, int idx_a,
                        std::mt19937 &rng);

// -----------------------------------------------------------------------------
// Crea un hijo cruzando los cromosomas A y B.
// Pasos: one-point crossover en DS + order crossover en CS → repair → evaluar.
// El hijo devuelto ya tiene fitness calculado y está listo para la población.
// -----------------------------------------------------------------------------
CromosomaLRP crossover(const CromosomaLRP &A, const CromosomaLRP &B,
                       const Matriz &mat, const InstanciaLRP &inst,
                       std::mt19937 &rng);

// -----------------------------------------------------------------------------
// Inserta el individuo T en la población reemplazando al de mayor fitness (peor).
// Se llama solo cuando T fue aceptado (pasó el filtro de diversidad).
// -----------------------------------------------------------------------------
void add_in_pop(std::vector<CromosomaLRP> &pop, const CromosomaLRP &T);
