// =============================================================================
// mapm.h — AlgoritmMAPM: el MA|PM envuelto en la interfaz IAlgoritmo
// =============================================================================
//
// MA|PM = Memetic Algorithm with Population Management (Prins et al. 2006).
// Es un algoritmo genético con búsqueda local integrada y gestión activa de
// la diversidad de la población.
//
// En términos simples: es como un GA clásico (población → selección → crossover
// → inserción → repetir), pero con dos diferencias clave:
//   1. Cada hijo se mejora con búsqueda local antes de entrar a la población.
//   2. La población se "refresca" periódicamente cuando converge demasiado.
//
// El resultado es un algoritmo que equilibra bien exploración (crossover, refresco)
// y explotación (búsqueda local).
//
// =============================================================================
#pragma once
#include "../algoritmo.h"
#include "../parser.h"

// -----------------------------------------------------------------------------
// Parámetros del MA|PM. Todos se pueden calcular automáticamente con
// config_por_defecto(n, m) o ajustar manualmente.
// -----------------------------------------------------------------------------
struct ConfigMAPM {
  // ── Población ──────────────────────────────────────────────────────────────
  int NbIndiv;      // tamaño de la población ≈ (n+m)/5 + 1

  // ── Criterios de parada ───────────────────────────────────────────────────
  int MaxNbAcc;     // iteraciones totales aceptadas antes de terminar ≈ (n+m)*10
  int MaxNbNoAdd;   // máx intentos sin añadir nadie → refrescar ≈ (n+m)*2.5

  // ── Diversidad ────────────────────────────────────────────────────────────
  int MaxNbRej;     // si NbRej > MaxNbRej seguidos, bajar el umbral Δ
  int Delta_max;    // umbral inicial de distancia mínima para aceptar un hijo

  // ── Selección ─────────────────────────────────────────────────────────────
  int beta;         // padre A se elige entre los beta mejores ≈ NbIndiv/3

  // ── Búsqueda local ────────────────────────────────────────────────────────
  double p1;        // probabilidad de aplicar LS1 (búsqueda completa) al hijo
  double p2;        // probabilidad de aplicar LS2 si no se aplicó LS1
                    // (con probabilidad 1-p1-p2*(1-p1) no se aplica ninguna)
  double alpha_pen; // factor de penalización en Split por exceso de cap. depósito

  unsigned int semilla;
};

// Calcula parámetros razonables en función del tamaño del problema (n clientes, m depósitos).
ConfigMAPM config_por_defecto(int n, int m, unsigned int semilla = 42);

class AlgoritmMAPM : public IAlgoritmo {
public:
  explicit AlgoritmMAPM(ConfigMAPM cfg);

  ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) override;
  std::string nombre() const override { return "MA|PM"; }

private:
  ConfigMAPM cfg_;
};
