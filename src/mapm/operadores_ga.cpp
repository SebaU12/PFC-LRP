#include "operadores_ga.h"
#include "split.h"
#include <algorithm>
#include <numeric>
#include <stdexcept>

// ═════════════════════════════════════════
// Helpers internos
// ═════════════════════════════════════════

// Torneo binario: dado un vector de índices candidatos,
// elige 2 al azar y retorna el índice del de menor fitness.
static int torneo_binario(const std::vector<CromosomaLRP> &pop,
                          const std::vector<int> &candidatos,
                          std::mt19937 &rng) {
  if (candidatos.size() == 1)
    return candidatos[0];

  std::uniform_int_distribution<int> dist(0, (int)candidatos.size() - 1);
  int ia = dist(rng);
  int ib;
  do {
    ib = dist(rng);
  } while (ib == ia);

  int idx_a = candidatos[ia];
  int idx_b = candidatos[ib];
  return (pop[idx_a].fitness <= pop[idx_b].fitness) ? idx_a : idx_b;
}

// ═════════════════════════════════════════
// seleccionar_padre_a
//
// Torneo binario entre los beta mejores (menor fitness).
// ═════════════════════════════════════════
int seleccionar_padre_a(const std::vector<CromosomaLRP> &pop, int beta,
                        std::mt19937 &rng) {
  int sz = (int)pop.size();
  beta = std::min(beta, sz);

  // Ordenar índices por fitness ascendente
  std::vector<int> orden(sz);
  std::iota(orden.begin(), orden.end(), 0);
  std::sort(orden.begin(), orden.end(),
            [&](int a, int b) { return pop[a].fitness < pop[b].fitness; });

  // Candidatos: los beta mejores
  std::vector<int> candidatos(orden.begin(), orden.begin() + beta);
  return torneo_binario(pop, candidatos, rng);
}

// ═════════════════════════════════════════
// seleccionar_padre_b
//
// Torneo binario sobre toda la población excepto idx_a.
// ═════════════════════════════════════════
int seleccionar_padre_b(const std::vector<CromosomaLRP> &pop, int idx_a,
                        std::mt19937 &rng) {
  std::vector<int> candidatos;
  candidatos.reserve(pop.size() - 1);
  for (int i = 0; i < (int)pop.size(); ++i)
    if (i != idx_a)
      candidatos.push_back(i);

  if (candidatos.empty())
    throw std::runtime_error("seleccionar_padre_b: población de tamaño 1");

  return torneo_binario(pop, candidatos, rng);
}

// ═════════════════════════════════════════
// crossover
// ═════════════════════════════════════════
CromosomaLRP crossover(const CromosomaLRP &A, const CromosomaLRP &B,
                       const Matriz &mat, const InstanciaLRP &inst,
                       std::mt19937 &rng) {
  int n = (int)A.CS.size(); // num_clientes
  int m = (int)A.DS.size(); // num_depositos

  CromosomaLRP C;
  C.DS.resize(m);
  C.CS.resize(n);

  // ── One-point crossover sobre DS ─────────────────────
  // cp ∈ [1, m-1] para garantizar que ambos padres contribuyen
  std::uniform_int_distribution<int> dist_ds(1, std::max(1, m - 1));
  int cp_ds = dist_ds(rng);

  for (int i = 0; i < cp_ds; ++i)
    C.DS[i] = A.DS[i];
  for (int i = cp_ds; i < m; ++i)
    C.DS[i] = B.DS[i];

  // ── Order crossover sobre CS ──────────────────────────
  // cp ∈ [1, n-1]
  std::uniform_int_distribution<int> dist_cs(1, std::max(1, n - 1));
  int cp_cs = dist_cs(rng);

  // Primera parte: copiar CS(A)[0..cp_cs-1]
  std::vector<bool> incluido(n + 1, false); // indexado por ID cliente
  for (int i = 0; i < cp_cs; ++i) {
    C.CS[i] = A.CS[i];
    incluido[A.CS[i]] = true;
  }

  // Segunda parte: recorrer CS(B) desde cp_cs de forma circular,
  // agregar clientes no incluidos hasta completar n posiciones.
  int pos = cp_cs;
  for (int k = 0; k < n && pos < n; ++k) {
    int idx_b = (cp_cs + k) % n;
    int c = B.CS[idx_b];
    if (!incluido[c]) {
      C.CS[pos] = c;
      incluido[c] = true;
      ++pos;
    }
  }

  // ── Repair ───────────────────────────────────────────
  // El crossover de DS puede generar:
  //   1. Ningún depósito abierto (todos DS=0)
  //   2. Punteros DS inconsistentes con CS
  //   3. Violaciones de capacidad de depósito
  //
  // Antes de repair necesitamos reconstruir los punteros DS
  // para que sean consistentes con la nueva asignación implícita.
  // La asignación de clientes a depósitos la inferimos del DS
  // del padre que aportó cada segmento de CS, pero tras el
  // crossover de DS los punteros quedaron mezclados y pueden
  // ser incorrectos.
  //
  // Estrategia: reconstruir DS desde cero usando la asignación
  // de A para clientes en CS[0..cp_cs-1] y de B para el resto,
  // luego dejar que repair maneje violaciones de capacidad.

  // Construir mapa cliente→dep_idx para A y B
  // usando la función clientes_de de cada padre
  std::vector<int> asig(n + 1, -1); // asig[cliente_id] = dep_idx

  for (int i = 0; i < m; ++i) {
    auto cli_a = A.clientes_de(i, inst);
    for (int c : cli_a)
      asig[c] = i;
  }

  // Para clientes de la segunda parte (aportados por B),
  // sobrescribir con la asignación de B
  for (int k = cp_cs; k < n; ++k) {
    int c = C.CS[k];
    // Buscar en B a qué depósito pertenecía c
    for (int i = 0; i < m; ++i) {
      if (B.DS[i] == 0)
        continue;
      auto cli_b = B.clientes_de(i, inst);
      bool found = false;
      for (int cb : cli_b) {
        if (cb == c) {
          asig[c] = i;
          found = true;
          break;
        }
      }
      if (found)
        break;
    }
  }

  // Reconstruir DS y CS según la asignación mezclada
  // Agrupar clientes por depósito manteniendo el orden de C.CS
  std::vector<std::vector<int>> grupos(m);
  for (int k = 0; k < n; ++k) {
    int c = C.CS[k];
    int didx = asig[c];
    if (didx < 0 || didx >= m)
      didx = 0; // fallback: primer depósito
    grupos[didx].push_back(c);
  }

  // Reconstruir CS como concatenación de grupos y DS como punteros
  C.DS.assign(m, 0);
  int ptr = 1; // posición 1-based en CS
  int pos_cs = 0;
  for (int i = 0; i < m; ++i) {
    if (grupos[i].empty())
      continue;
    C.DS[i] = ptr;
    for (int c : grupos[i]) {
      C.CS[pos_cs] = c;
      ++pos_cs;
      ++ptr;
    }
  }

  // Aplicar repair para corregir violaciones de capacidad
  repair(C, mat, inst);

  // Evaluar fitness
  evaluar_cromosoma(C, mat, inst);

  return C;
}

// ═════════════════════════════════════════
// add_in_pop
// ═════════════════════════════════════════
void add_in_pop(std::vector<CromosomaLRP> &pop, const CromosomaLRP &T) {
  // Encontrar el individuo de mayor fitness (el peor)
  int idx_peor = 0;
  for (int i = 1; i < (int)pop.size(); ++i)
    if (pop[i].fitness > pop[idx_peor].fitness)
      idx_peor = i;

  pop[idx_peor] = T;
}
