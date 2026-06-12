// =============================================================================
// poblacion.h — Construcción y refresco de la población del MA|PM
// =============================================================================
//
// El MA|PM necesita una población diversa de soluciones para explorar bien
// el espacio de búsqueda. Este archivo provee dos cosas:
//
//   HEURÍSTICAS CONSTRUCTIVAS: generan un cromosoma "razonable" desde cero.
//     - NN (Nearest Neighbor): asigna cada cliente al depósito más cercano,
//       luego ordena los clientes de cada depósito con un tour greedy vecino más cercano.
//     - CW (Clarke & Wright): usa los ahorros clásicos para fusionar rutas individuales.
//
//   GESTIÓN DE POBLACIÓN:
//     - generar_poblacion: crea NbIndiv individuos distintos mezclando NN y CW.
//     - refrescar_poblacion: cuando el loop principal se estanca, descarta todos
//       los individuos excepto el mejor y regenera el resto.
//
// La aleatoriedad en las heurísticas (parámetros alpha_nn, alpha_cw) garantiza
// que la población sea diversa aunque se llamen muchas veces.
//
// =============================================================================
#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include "split.h"
#include <functional>
#include <random>
#include <vector>

// Tipo de función de búsqueda local que se puede inyectar en la construcción.
// Si se pasa, se aplica a cada individuo recién construido antes de añadirlo
// a la población (mejora la calidad inicial sin mucho costo).
using FnLocalSearch =
    std::function<void(CromosomaLRP &, const Matriz &, const InstanciaLRP &)>;

// -----------------------------------------------------------------------------
// Nearest Neighbor aleatorizado: asigna cada cliente al depósito más cercano
// (con probabilidad alpha_nn elige el 2do más cercano en vez del 1ro, para
// introducir variedad). Luego ordena los clientes de cada depósito con un
// tour greedy de vecino más cercano.
// -----------------------------------------------------------------------------
CromosomaLRP heuristica_nn(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_nn = 0.3);

// -----------------------------------------------------------------------------
// Clarke & Wright aleatorizado: construye ahorros = dist(dep,ci) + dist(dep,cj)
// - dist(ci,cj) y fusiona rutas si ambos extremos son compatibles.
// Con probabilidad alpha_cw saltea el saving (introduce variedad).
// -----------------------------------------------------------------------------
CromosomaLRP heuristica_cw(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng, double alpha_cw = 0.3);

// -----------------------------------------------------------------------------
// Genera la población inicial: NbIndiv/2 individuos con NN, el resto con CW.
// Rechaza individuos demasiado similares (distancia < 1 con respecto a los
// ya aceptados), para garantizar diversidad. Si no puede llenarla con
// individuos distintos, acepta duplicados como fallback.
// Aplica ls a cada individuo antes de añadirlo.
// -----------------------------------------------------------------------------
std::vector<CromosomaLRP> generar_poblacion(
    int NbIndiv, const InstanciaLRP &inst, const Matriz &mat, std::mt19937 &rng,
    const FnLocalSearch &ls = [](CromosomaLRP &, const Matriz &,
                                 const InstanciaLRP &) {});

// -----------------------------------------------------------------------------
// GenPop2 del paper: refresca la población cuando el loop exterior termina.
// Conserva solo el mejor individuo actual y regenera el resto desde cero
// con las mismas heurísticas. Evita que la población converja a una sola región.
// -----------------------------------------------------------------------------
void refrescar_poblacion(
    std::vector<CromosomaLRP> &pop, const InstanciaLRP &inst, const Matriz &mat,
    std::mt19937 &rng,
    const FnLocalSearch &ls = [](CromosomaLRP &, const Matriz &,
                                 const InstanciaLRP &) {});
