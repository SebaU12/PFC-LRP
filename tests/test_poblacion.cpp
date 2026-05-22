#include "../src/distancia.h"
#include "../src/mapm/cromosoma.h"
#include "../src/mapm/distancia_sol.h"
#include "../src/mapm/poblacion.h"
#include "../src/mapm/split.h"
#include "../src/parser.h"
#include <iostream>
#include <set>

static void check(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "[FAIL] " << msg << "\n";
    std::exit(1);
  }
  std::cout << "[OK]   " << msg << "\n";
}

static void verificar_cromosoma(const CromosomaLRP &crom,
                                const InstanciaLRP &inst,
                                const std::string &nombre) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  check((int)crom.CS.size() == n, nombre + ": CS.size() == n");
  std::set<int> vistos(crom.CS.begin(), crom.CS.end());
  check((int)vistos.size() == n,
        nombre + ": CS es permutación válida (sin duplicados)");

  check((int)crom.DS.size() == m, nombre + ": DS.size() == m");
  check(crom.n_abiertos() > 0, nombre + ": al menos 1 depósito abierto");

  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0)
      continue;
    check(crom.DS[i] >= 1 && crom.DS[i] <= n,
          nombre + ": DS[" + std::to_string(i) + "] en rango [1.." +
              std::to_string(n) + "]");
  }

  std::set<int> cubiertos;
  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0)
      continue;
    for (int c : crom.clientes_de(i, inst))
      cubiertos.insert(c);
  }
  check((int)cubiertos.size() == n,
        nombre + ": todos los clientes cubiertos (" +
            std::to_string(cubiertos.size()) + "/" + std::to_string(n) + ")");

  check(crom.fitness > 0 && crom.fitness < 1e12,
        nombre + ": fitness finito y positivo (" +
            std::to_string(crom.fitness) + ")");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_poblacion <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto mat = construir_matriz_distancias(inst);

  int n = inst.num_clientes;
  int m = inst.num_depositos;
  int NbIndiv = (n + m) / 5 + 1;

  std::cout << "=== Test Fase 2 MA|PM: Inicialización de Población ===\n";
  std::cout << "  n=" << n << "  m=" << m << "  NbIndiv=" << NbIndiv << "\n\n";

  std::mt19937 rng(42);

  // ── Test 1: heurística NN ─────────────────────────────
  std::cout << "-- Test 1: heurística Nearest Neighbor --\n";
  {
    std::mt19937 rng2(42);
    auto crom = heuristica_nn(inst, mat, rng2);
    evaluar_cromosoma(crom, mat, inst);
    verificar_cromosoma(crom, inst, "NN");
    std::cout << "  Fitness NN: " << crom.fitness << "\n";
  }

  // ── Test 2: heurística CW ─────────────────────────────
  std::cout << "\n-- Test 2: heurística Clarke & Wright --\n";
  {
    std::mt19937 rng2(42);
    auto crom = heuristica_cw(inst, mat, rng2);
    evaluar_cromosoma(crom, mat, inst);
    verificar_cromosoma(crom, inst, "CW");
    std::cout << "  Fitness CW: " << crom.fitness << "\n";
  }

  // ── Test 3: diversidad entre soluciones NN ────────────
  std::cout << "\n-- Test 3: diversidad entre soluciones NN --\n";
  {
    std::mt19937 rng_a(1), rng_b(2);
    auto c1 = heuristica_nn(inst, mat, rng_a);
    auto c2 = heuristica_nn(inst, mat, rng_b);
    evaluar_cromosoma(c1, mat, inst);
    evaluar_cromosoma(c2, mat, inst);
    int d = distancia(c1, c2, mat, inst); // ← mat agregado
    std::cout << "  D(NN_1, NN_2) = " << d << "\n";
    check(d >= 0, "distancia(NN_1, NN_2) >= 0");
  }

  // ── Test 4: generar_poblacion ─────────────────────────
  std::cout << "\n-- Test 4: generar_poblacion --\n";
  {
    auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

    check((int)pop.size() == NbIndiv,
          "pop.size() == NbIndiv (" + std::to_string(NbIndiv) + ")");

    for (int k = 0; k < (int)pop.size(); ++k)
      verificar_cromosoma(pop[k], inst, "pop[" + std::to_string(k) + "]");

    // Todos distintos (D >= 1 para cada par)
    bool todos_distintos = true;
    for (int i = 0; i < (int)pop.size(); ++i)
      for (int j = i + 1; j < (int)pop.size(); ++j)
        if (distancia(pop[i], pop[j], mat, inst) < 1) // ← mat agregado
          todos_distintos = false;
    check(todos_distintos,
          "todos los individuos de la población son distintos (D>=1)");

    // Estadísticas de fitness
    double mejor = pop[0].fitness, peor = pop[0].fitness;
    for (const auto &c : pop) {
      mejor = std::min(mejor, c.fitness);
      peor = std::max(peor, c.fitness);
    }
    std::cout << "  Fitness mínimo : " << mejor << "\n";
    std::cout << "  Fitness máximo : " << peor << "\n";
  }

  // ── Test 5: refrescar_poblacion ───────────────────────
  std::cout << "\n-- Test 5: refrescar_poblacion --\n";
  {
    auto pop = generar_poblacion(NbIndiv, inst, mat, rng);
    double mejor_antes = pop[0].fitness;
    for (const auto &c : pop)
      mejor_antes = std::min(mejor_antes, c.fitness);

    refrescar_poblacion(pop, inst, mat, rng);

    check((int)pop.size() == NbIndiv, "pop.size() == NbIndiv tras refrescar");

    double mejor_despues = pop[0].fitness;
    for (const auto &c : pop)
      mejor_despues = std::min(mejor_despues, c.fitness);

    check(mejor_despues <= mejor_antes + 1e-6,
          "refrescar: el mejor individuo se conserva (" +
              std::to_string(mejor_antes) + " → " +
              std::to_string(mejor_despues) + ")");

    for (int k = 0; k < (int)pop.size(); ++k)
      verificar_cromosoma(pop[k], inst,
                          "pop_refrescada[" + std::to_string(k) + "]");

    std::cout << "  Fitness mejor antes  : " << mejor_antes << "\n";
    std::cout << "  Fitness mejor después: " << mejor_despues << "\n";
  }

  std::cout << "\n[FASE 2 MA|PM OK] Todos los tests pasaron.\n";
  return 0;
}
