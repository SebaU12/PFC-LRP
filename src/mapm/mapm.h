#pragma once
#include "../algoritmo.h"
#include "../parser.h"

// ─────────────────────────────────────────
// Configuración del MA|PM — Prins et al. 2006, Tabla 1
//
// Todos los parámetros se auto-calculan desde n y m con
// config_por_defecto(n, m), pero pueden sobreescribirse.
// ─────────────────────────────────────────
struct ConfigMAPM {
  int NbIndiv;      // ((n+m)/5) + 1
  int MaxNbAcc;     // (n+m) * 10
  int MaxNbNoAdd;   // (n+m) * 2.5  (cast a int)
  int MaxNbRej;     // NbIndiv
  int Delta_max;    // (n+m)/10 + 10
  int beta;         // NbIndiv / 3
  double p1;        // 0.5
  double p2;        // 0.7
  double alpha_pen; // 1000.0  (penalización en Split)
  unsigned int semilla;
};

// Construye ConfigMAPM con los valores del paper para una
// instancia de n clientes y m depósitos.
ConfigMAPM config_por_defecto(int n, int m, unsigned int semilla = 42);

// ─────────────────────────────────────────
// AlgoritmMAPM — implementa IAlgoritmo
// ─────────────────────────────────────────
class AlgoritmMAPM : public IAlgoritmo {
public:
  explicit AlgoritmMAPM(ConfigMAPM cfg);

  // Ejecuta el MA|PM desde sol_inicial.
  // Retorna la mejor solución encontrada como EstadoLRP.
  ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) override;

  std::string nombre() const override { return "MA|PM"; }

private:
  ConfigMAPM cfg_;
};
