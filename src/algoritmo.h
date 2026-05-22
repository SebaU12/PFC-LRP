#pragma once
#include "estado.h"
#include <string>
#include <utility>
#include <vector>

// ─────────────────────────────────────────
// Resultado genérico de cualquier algoritmo
// ─────────────────────────────────────────
struct ResultadoAlgoritmo {
  EstadoLRP mejor_estado;
  double costo_inicial;
  double costo_final;
  int iteraciones;
  // (iteracion, costo) cada vez que mejora el óptimo global
  std::vector<std::pair<int, double>> historial_mejoras;
};

// ─────────────────────────────────────────
// Interfaz común para todos los algoritmos
// ─────────────────────────────────────────
class IAlgoritmo {
public:
  virtual ~IAlgoritmo() = default;

  // Ejecuta el algoritmo a partir de una solución inicial ya construida.
  // Retorna el resultado con la mejor solución encontrada.
  virtual ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) = 0;

  // Nombre identificador del algoritmo (para logs y tablas).
  virtual std::string nombre() const = 0;
};
