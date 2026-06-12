// =============================================================================
// local_search.h — Búsqueda local del MA|PM: MOVE, SWAP, 2-OPT
// =============================================================================
//
// Después de cada crossover, el hijo se mejora con búsqueda local antes de
// entrar a la población. La búsqueda opera sobre las rutas EXPLÍCITAS
// (las que produce el Split), no sobre el cromosoma DS+CS directamente.
//
// Se ofrecen dos versiones:
//   LS1 — inter-depósito: puede mover clientes entre depósitos distintos.
//          Más poderosa pero más lenta (O(n² × m²) por iteración).
//   LS2 — intra-depósito: solo mueve clientes dentro de sus rutas de su depósito.
//          Más rápida pero solo mejora el routing, no la asignación depósito→cliente.
//
// Ambas encadenan MOVE → SWAP → 2-OPT con estrategia first-improvement:
// en cuanto encuentran una mejora, la aplican y reinician el ciclo.
// Terminan cuando ninguna de las tres vecindades produce mejora.
//
// =============================================================================
#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include "split.h"
#include <vector>

// -----------------------------------------------------------------------------
// Representación de rutas sobre la que opera la búsqueda local.
// rutas[dep_idx][ruta_idx] = RutaVec (lista de IDs de clientes).
// Es la "vista expandida" del cromosoma: más fácil de modificar que DS+CS.
// -----------------------------------------------------------------------------
struct RutasExplicitas {
  std::vector<std::vector<RutaVec>> rutas; // [dep_idx][ruta_idx] → lista de clientes
  double costo;
};

// Construye RutasExplicitas a partir del cromosoma y su Split ya calculado.
RutasExplicitas construir_rutas_explicitas(
    const CromosomaLRP                &crom,
    const std::vector<SplitResultado> &split_res,
    const Matriz                      &mat,
    const InstanciaLRP                &inst);

// Recalcula el costo total (apertura + routing) de un RutasExplicitas.
double recalcular_costo(const RutasExplicitas &re,
                        const Matriz          &mat,
                        const InstanciaLRP    &inst);

// -----------------------------------------------------------------------------
// Vecindades individuales (first-improvement):
//   move_operator: mover un cliente de su posición actual a otra ruta.
//   swap_operator: intercambiar dos clientes de posiciones distintas.
//   opt2_operator: invertir un segmento intra-ruta (2-opt clásico) o
//                  intercambiar colas entre dos rutas del mismo depósito.
//
// solo_intra_deposito=true: solo considera movimientos dentro del mismo depósito.
// Retornan true si encontraron y aplicaron una mejora.
// -----------------------------------------------------------------------------
bool move_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra_deposito = false);
bool swap_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra_deposito = false);
bool opt2_operator(RutasExplicitas &re, const Matriz &mat,
                   const InstanciaLRP &inst, bool solo_intra_deposito = false);

// -----------------------------------------------------------------------------
// LS1: búsqueda local completa con movimientos inter e intra depósito.
//      Modifica crom (actualiza CS, DS y fitness al final).
//      Devuelve la mejora total obtenida (costo_antes - costo_después).
//
// LS2: igual que LS1 pero solo movimientos intra-depósito. Más rápida.
// -----------------------------------------------------------------------------
double LS1(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
           const Matriz &mat, const InstanciaLRP &inst);
double LS2(CromosomaLRP &crom, std::vector<SplitResultado> &split_res,
           const Matriz &mat, const InstanciaLRP &inst);

// Sincroniza el cromosoma DS+CS con las rutas explícitas mejoradas.
// Se llama al final de LS1/LS2 para que el cromosoma refleje el resultado.
void actualizar_cromosoma_desde_rutas(CromosomaLRP         &crom,
                                      const RutasExplicitas &re,
                                      const InstanciaLRP   &inst);
