#pragma once
#include "../distancia.h"
#include "../parser.h"
#include "cromosoma.h"
#include "split.h"
#include <vector>

// ─────────────────────────────────────────
// Búsqueda local — Prins et al. 2006, Sección 3.3
//
// Tres vecindades aplicadas con first-improvement:
//
//   MOVE : mover un cliente a otra posición (misma ruta,
//          otra ruta del mismo depósito, o ruta de otro depósito).
//          Respeta Q en la ruta destino.
//
//   SWAP : intercambiar dos clientes entre sí.
//          Pueden estar en la misma ruta o en rutas distintas
//          (mismo o distinto depósito). Respeta Q en ambas rutas.
//
//   OPT  : 2-opt — eliminar dos aristas y reconectar.
//          Caso intra-ruta: inversión del segmento (2-opt clásico).
//          Caso inter-ruta mismo depósito: reconexión cruzada.
//          Caso inter-depósito: reconexión con cambio de depósito
//          del último segmento (respetando que cada ruta empiece
//          y termine en su depósito).
//
// Cada vecindad aplica el PRIMER movimiento que mejore el fitness.
// Si ninguno mejora, retorna false (ya es óptimo local).
//
// LS1: evalúa movimientos entre cualquier par de rutas/depósitos.
// LS2: igual que LS1 pero SOLO movimientos intra-depósito
//      (más rápida, usada con probabilidad p2 cuando no se aplica LS1).
// ─────────────────────────────────────────

// Representación interna de rutas explícitas para la búsqueda local.
// Split produce rutas óptimas; las almacenamos aquí para operar
// sobre ellas sin reconstruir el cromosoma en cada movimiento.
struct RutasExplicitas {
    // rutas[dep_idx] = lista de rutas para ese depósito
    // Cada ruta es un vector de IDs de cliente 1-indexed.
    std::vector<std::vector<Ruta>> rutas;   // [dep_idx][ruta_idx][pos]
    double costo;                            // costo total actual
};

// Construir RutasExplicitas desde el resultado de evaluar_cromosoma
RutasExplicitas construir_rutas_explicitas(
    const CromosomaLRP                  &crom,
    const std::vector<SplitResultado>   &split_res,
    const Matriz                        &mat,
    const InstanciaLRP                  &inst);

// Recalcular costo total desde cero (usado para verificación)
double recalcular_costo(const RutasExplicitas &re,
                        const Matriz          &mat,
                        const InstanciaLRP    &inst);

// ─────────────────────────────────────────
// Vecindades individuales (first-improvement)
// Retornan true si encontraron y aplicaron un movimiento mejorador.
// ─────────────────────────────────────────
bool move_operator(RutasExplicitas  &re,
                   const Matriz     &mat,
                   const InstanciaLRP &inst,
                   bool              solo_intra_deposito = false);

bool swap_operator(RutasExplicitas  &re,
                   const Matriz     &mat,
                   const InstanciaLRP &inst,
                   bool              solo_intra_deposito = false);

bool opt2_operator(RutasExplicitas  &re,
                   const Matriz     &mat,
                   const InstanciaLRP &inst,
                   bool              solo_intra_deposito = false);

// ─────────────────────────────────────────
// LS1 y LS2 completas
//
// Encadenan MOVE → SWAP → OPT2 en loop hasta que ninguna
// vecindad mejora (óptimo local en las tres estructuras).
//
// Modifican crom: actualizan CS, DS y fitness según las rutas
// resultantes de la búsqueda local.
//
// Retornan el delta de mejora (negativo = empeoramiento, no ocurre).
// ─────────────────────────────────────────
double LS1(CromosomaLRP              &crom,
           std::vector<SplitResultado> &split_res,
           const Matriz               &mat,
           const InstanciaLRP         &inst);

double LS2(CromosomaLRP              &crom,
           std::vector<SplitResultado> &split_res,
           const Matriz               &mat,
           const InstanciaLRP         &inst);

// Actualizar cromosoma desde rutas explícitas post-LS
// (reconstruye CS, DS y fitness)
void actualizar_cromosoma_desde_rutas(CromosomaLRP          &crom,
                                       const RutasExplicitas &re,
                                       const InstanciaLRP    &inst);
