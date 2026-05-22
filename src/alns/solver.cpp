#include "solver.h"

AlgoritmALNS::AlgoritmALNS(ConfigALNS cfg, std::vector<OperadorFn> destroyers,
                            std::vector<OperadorFn> repairers)
    : cfg_(std::move(cfg)), destroyers_(std::move(destroyers)),
      repairers_(std::move(repairers)) {}

ResultadoAlgoritmo AlgoritmALNS::ejecutar(const EstadoLRP &sol_inicial) {
  return ejecutar_alns(sol_inicial, destroyers_, repairers_, cfg_);
}
