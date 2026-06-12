// =============================================================================
// solver.cpp — Implementación de AlgoritmALNS
// =============================================================================
//
// Esta implementación es intencionalmente mínima. Todo el trabajo real está
// en alns.cpp (el motor). AlgoritmALNS solo:
//   1. Guarda la configuración y los operadores al construirse.
//   2. Delega la ejecución a ejecutar_alns() cuando se llama ejecutar().
//
// Este patrón de "envolver" la función libre en una clase que implementa
// IAlgoritmo es lo que permite usar ALNS y MA|PM indistintamente.
//
// =============================================================================
#include "solver.h"

// Guardar configuración y operadores por movimiento (sin copias innecesarias)
AlgoritmALNS::AlgoritmALNS(ConfigALNS cfg, std::vector<OperadorFn> destroyers,
                            std::vector<OperadorFn> repairers)
    : cfg_(std::move(cfg)), destroyers_(std::move(destroyers)),
      repairers_(std::move(repairers)) {}

// Delegar completamente al motor genérico. No hay lógica adicional aquí.
ResultadoAlgoritmo AlgoritmALNS::ejecutar(const EstadoLRP &sol_inicial) {
  return ejecutar_alns(sol_inicial, destroyers_, repairers_, cfg_);
}
