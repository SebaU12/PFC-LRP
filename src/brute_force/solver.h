// Fuerza bruta exacta para LRP pequeños.
// Usa Held-Karp (O(2^k * k^2)) por subconjunto de clientes +
// DP set-cover (O(3^k)) para el CVRP por depósito.
// Factible solo para n <= 12, m <= 3.
#pragma once
#include "../algoritmo.h"

class AlgoritmFuerzaBruta : public IAlgoritmo {
public:
  ResultadoAlgoritmo ejecutar(const EstadoLRP &sol_inicial) override;
  std::string nombre() const override { return "Fuerza Bruta"; }
};
