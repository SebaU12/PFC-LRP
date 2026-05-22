#pragma once
#include "../algoritmo.h"
#include "alns.h"
#include "operadores.h"

// ─────────────────────────────────────────
// Implementación de IAlgoritmo para ALNS
//
// Uso:
//   AlgoritmALNS algo(cfg, destroyers, repairers);
//   auto res = algo.ejecutar(sol_inicial);
// ─────────────────────────────────────────
class AlgoritmALNS : public IAlgoritmo {
public:
  AlgoritmALNS(ConfigALNS cfg, std::vector<OperadorFn> destroyers,
               std::vector<OperadorFn> repairers);

  ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) override;
  std::string nombre() const override { return "ALNS"; }

private:
  ConfigALNS cfg_;
  std::vector<OperadorFn> destroyers_;
  std::vector<OperadorFn> repairers_;
};
