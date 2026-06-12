// =============================================================================
// operadores_ga.cpp — Selección, crossover e inserción
// =============================================================================
#include "operadores_ga.h"
#include "split.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

// -----------------------------------------------------------------------------
// Torneo binario: elige dos índices al azar de la lista de candidatos y
// devuelve el que tenga menor fitness (mejor solución).
// Con un solo candidato, lo devuelve directamente.
// -----------------------------------------------------------------------------
static int torneo_binario(const std::vector<CromosomaLRP> &pop,
                          const std::vector<int> &candidatos,
                          std::mt19937 &rng) {
  if (candidatos.size() == 1) return candidatos[0];

  std::uniform_int_distribution<int> dist(0, (int)candidatos.size() - 1);
  int ia = dist(rng), ib;
  do { ib = dist(rng); } while (ib == ia); // asegurar dos candidatos distintos

  int idx_a = candidatos[ia], idx_b = candidatos[ib];
  return (pop[idx_a].fitness <= pop[idx_b].fitness) ? idx_a : idx_b;
}

// Padre A: torneo entre los beta mejores (mayor presión selectiva).
// Ordenar la población por fitness y tomar solo los primeros beta como candidatos.
int seleccionar_padre_a(const std::vector<CromosomaLRP> &pop, int beta,
                        std::mt19937 &rng) {
  int sz = (int)pop.size();
  beta = std::min(beta, sz);

  std::vector<int> orden(sz);
  std::iota(orden.begin(), orden.end(), 0);
  std::sort(orden.begin(), orden.end(),
            [&](int a, int b) { return pop[a].fitness < pop[b].fitness; });

  std::vector<int> candidatos(orden.begin(), orden.begin() + beta);
  return torneo_binario(pop, candidatos, rng);
}

// Padre B: torneo sobre toda la población excepto el padre A ya elegido.
int seleccionar_padre_b(const std::vector<CromosomaLRP> &pop, int idx_a,
                        std::mt19937 &rng) {
  std::vector<int> candidatos;
  candidatos.reserve(pop.size() - 1);
  for (int i = 0; i < (int)pop.size(); ++i)
    if (i != idx_a) candidatos.push_back(i);

  if (candidatos.empty())
    throw std::runtime_error("seleccionar_padre_b: población de tamaño 1");

  return torneo_binario(pop, candidatos, rng);
}

// -----------------------------------------------------------------------------
// Crossover DS+CS para el LRP (Prins et al. 2006).
//
// El hijo C se construye en tres pasos:
//
// PASO 1 — One-point crossover en DS:
//   Elegir un punto de corte cp_ds al azar.
//   C.DS[0..cp_ds-1] = A.DS[0..cp_ds-1]
//   C.DS[cp_ds..m-1] = B.DS[cp_ds..m-1]
//   → C hereda la "apertura de depósitos" de A hasta cp_ds y de B después.
//
// PASO 2 — Order crossover (OX) en CS:
//   Elegir un punto de corte cp_cs al azar.
//   C.CS[0..cp_cs-1] = A.CS[0..cp_cs-1]  (segmento de A en orden)
//   Luego recorrer B.CS empezando desde cp_cs (cíclico) y agregar los clientes
//   no incluidos aún. Esto preserva el orden relativo de B.
//   → C hereda la "secuencia de visita" de A en la primera parte, de B en la segunda.
//
// PASO 3 — Reconstruir asignación cliente→depósito:
//   Los primeros cp_cs clientes de C heredan su depósito de A.
//   Los siguientes heredan su depósito de B.
//   Reagrupar CS por depósito para que el formato DS+CS sea consistente.
//
// PASO 4 — repair + evaluar:
//   Corregir posibles infactibilidades (depósito sin clientes, capacidad excedida).
//   Calcular fitness con Split.
// -----------------------------------------------------------------------------
CromosomaLRP crossover(const CromosomaLRP &A, const CromosomaLRP &B,
                       const Matriz &mat, const InstanciaLRP &inst,
                       std::mt19937 &rng) {
  int n = (int)A.CS.size();
  int m = (int)A.DS.size();

  CromosomaLRP C;
  C.DS.resize(m);
  C.CS.resize(n);

  // PASO 1: one-point crossover en DS
  std::uniform_int_distribution<int> dist_ds(1, std::max(1, m - 1));
  int cp_ds = dist_ds(rng);
  for (int i = 0; i < cp_ds; ++i) C.DS[i] = A.DS[i];
  for (int i = cp_ds; i < m; ++i)  C.DS[i] = B.DS[i];

  // PASO 2: order crossover en CS
  std::uniform_int_distribution<int> dist_cs(1, std::max(1, n - 1));
  int cp_cs = dist_cs(rng);

  std::vector<bool> incluido(n + 1, false);
  for (int i = 0; i < cp_cs; ++i) {     // copiar primer segmento de A
    C.CS[i] = A.CS[i];
    incluido[A.CS[i]] = true;
  }
  int pos = cp_cs;
  for (int k = 0; k < n && pos < n; ++k) { // completar con B en orden cíclico
    int c = B.CS[(cp_cs + k) % n];
    if (!incluido[c]) { C.CS[pos] = c; incluido[c] = true; ++pos; }
  }

  // PASO 3: reconstruir asignación cliente→depósito
  // Los primeros cp_cs clientes de C heredan su depósito de A
  std::vector<int> asig(n + 1, -1);
  for (int i = 0; i < m; ++i)
    for (int c : A.clientes_de(i, inst))
      asig[c] = i;

  // Los restantes cp_cs..n-1 heredan su depósito de B
  for (int k = cp_cs; k < n; ++k) {
    int c = C.CS[k];
    for (int i = 0; i < m; ++i) {
      if (B.DS[i] == 0) continue;
      bool found = false;
      for (int cb : B.clientes_de(i, inst))
        if (cb == c) { asig[c] = i; found = true; break; }
      if (found) break;
    }
  }

  // Reagrupar CS por depósito para reconstruir DS+CS consistente
  std::vector<std::vector<int>> grupos(m);
  for (int k = 0; k < n; ++k) {
    int didx = asig[C.CS[k]];
    if (didx < 0 || didx >= m) didx = 0; // fallback si la asignación falla
    grupos[didx].push_back(C.CS[k]);
  }

  C.DS.assign(m, 0);
  int ptr = 1, pos_cs = 0;
  for (int i = 0; i < m; ++i) {
    if (grupos[i].empty()) continue;
    C.DS[i] = ptr;
    for (int c : grupos[i]) { C.CS[pos_cs] = c; ++pos_cs; ++ptr; }
  }

  // PASO 4: reparar y evaluar
  repair(C, mat, inst);
  evaluar_cromosoma(C, mat, inst);
  return C;
}

// Insertar T reemplazando al peor individuo de la población.
void add_in_pop(std::vector<CromosomaLRP> &pop, const CromosomaLRP &T) {
  int idx_peor = 0;
  for (int i = 1; i < (int)pop.size(); ++i)
    if (pop[i].fitness > pop[idx_peor].fitness)
      idx_peor = i;
  pop[idx_peor] = T;
}
