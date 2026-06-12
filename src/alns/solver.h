// =============================================================================
// solver.h — AlgoritmALNS: el ALNS envuelto en la interfaz IAlgoritmo
// =============================================================================
//
// Este archivo es el "pegamento" entre el motor genérico del ALNS (alns.cpp)
// y la interfaz común IAlgoritmo que todos los algoritmos deben implementar.
//
// ¿Para qué existe esta capa intermedia?
//   - El código de benchmark y comparación (benchmark.cpp, validate.cpp) solo
//     conoce la interfaz IAlgoritmo. Puede recibir un AlgoritmALNS o un
//     AlgoritmMAPM sin saber cuál es cuál.
//   - Esto permite intercambiarlos fácilmente para comparar resultados.
//
// Para usar el ALNS desde fuera, basta con:
//   AlgoritmALNS alns(config, destroyers, repairers);
//   auto resultado = alns.ejecutar(sol_inicial);
//
// =============================================================================
#pragma once
#include "../algoritmo.h"
#include "alns.h"
#include "operadores.h"

class AlgoritmALNS : public IAlgoritmo {
public:
  // Constructor: recibe la configuración y los catálogos de operadores.
  // Se guardan por movimiento (no copia) para evitar duplicar los vectores.
  AlgoritmALNS(ConfigALNS cfg, std::vector<OperadorFn> destroyers,
               std::vector<OperadorFn> repairers);

  // Ejecuta el ALNS completo y devuelve la mejor solución encontrada.
  ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) override;

  // Nombre para reportes y logs
  std::string nombre() const override { return "ALNS"; }

private:
  ConfigALNS cfg_;
  std::vector<OperadorFn> destroyers_;
  std::vector<OperadorFn> repairers_;
};
