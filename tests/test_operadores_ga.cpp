#include "../src/distancia.h"
#include "../src/mapm/cromosoma.h"
#include "../src/mapm/operadores_ga.h"
#include "../src/mapm/poblacion.h"
#include "../src/mapm/split.h"
#include "../src/parser.h"
#include "../src/solucion_inicial.h"
#include <algorithm>
#include <iostream>
#include <set>

static void check(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "[FAIL] " << msg << "\n";
    std::exit(1);
  }
  std::cout << "[OK]   " << msg << "\n";
}

// Verifica invariantes estructurales de un cromosoma
static void check_cromosoma(const CromosomaLRP &c, const InstanciaLRP &inst,
                            const std::string &nombre) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  check((int)c.CS.size() == n, nombre + ": CS.size() == n");
  check((int)c.DS.size() == m, nombre + ": DS.size() == m");

  // CS es permutación válida
  std::set<int> vistos(c.CS.begin(), c.CS.end());
  check((int)vistos.size() == n, nombre + ": CS sin duplicados");

  // Al menos un depósito abierto
  check(c.n_abiertos() > 0, nombre + ": al menos 1 depósito abierto");

  // Punteros DS válidos
  for (int i = 0; i < m; ++i) {
    if (c.DS[i] == 0)
      continue;
    check(c.DS[i] >= 1 && c.DS[i] <= n,
          nombre + ": DS[" + std::to_string(i) + "] en [1..n]");
  }

  // Todos los clientes cubiertos por depósitos abiertos
  std::set<int> cubiertos;
  for (int i = 0; i < m; ++i) {
    if (c.DS[i] == 0)
      continue;
    for (int cl : c.clientes_de(i, inst))
      cubiertos.insert(cl);
  }
  check((int)cubiertos.size() == n, nombre + ": todos los clientes cubiertos");

  // Capacidad de depósito respetada (sin penalización)
  for (int i = 0; i < m; ++i) {
    if (c.DS[i] == 0)
      continue;
    double dem = 0.0;
    for (int cl : c.clientes_de(i, inst))
      dem += inst.clientes[cl - 1].demanda;
    check(dem <= inst.depositos[i].capacidad + 1e-9,
          nombre + ": dep[" + std::to_string(i) + "] cap respetada");
  }

  // Fitness evaluado y positivo
  check(c.fitness > 0 && c.fitness < 1e13, nombre +
                                               ": fitness finito y positivo (" +
                                               std::to_string(c.fitness) + ")");
}

// ── Test 1: selección ─────────────────────────────────────────
static void test_seleccion(const std::vector<CromosomaLRP> &pop, int beta,
                           std::mt19937 &rng) {
  // Ejecutar múltiples selecciones y verificar índices válidos
  int sz = (int)pop.size();
  bool idx_a_ok = true, idx_b_ok = true, distintos_ok = true;

  for (int t = 0; t < 50; ++t) {
    int ia = seleccionar_padre_a(pop, beta, rng);
    int ib = seleccionar_padre_b(pop, ia, rng);
    if (ia < 0 || ia >= sz)
      idx_a_ok = false;
    if (ib < 0 || ib >= sz)
      idx_b_ok = false;
    if (ia == ib)
      distintos_ok = false;
  }
  check(idx_a_ok, "seleccion: idx_a siempre en [0, NbIndiv)");
  check(idx_b_ok, "seleccion: idx_b siempre en [0, NbIndiv)");
  check(distintos_ok, "seleccion: padre_a != padre_b siempre");

  // Verificar sesgo: padre_a debe tender a los mejores
  // Contamos cuántas veces el padre_a tiene fitness <= mediana
  std::vector<double> fitnesses;
  for (const auto &c : pop)
    fitnesses.push_back(c.fitness);
  std::sort(fitnesses.begin(), fitnesses.end());
  double mediana = fitnesses[sz / 2];

  int veces_bueno = 0;
  for (int t = 0; t < 200; ++t) {
    int ia = seleccionar_padre_a(pop, beta, rng);
    if (pop[ia].fitness <= mediana)
      ++veces_bueno;
  }
  // Con torneo entre beta mejores, debe salir bueno >> 50% del tiempo
  check(veces_bueno > 100, "seleccion: padre_a sesgado hacia mejores (" +
                               std::to_string(veces_bueno) +
                               "/200 veces fitness <= mediana)");
}

// ── Test 2: crossover produce cromosoma válido ────────────────
static void test_crossover(const std::vector<CromosomaLRP> &pop,
                           const Matriz &mat, const InstanciaLRP &inst,
                           std::mt19937 &rng) {
  int beta = std::max(1, (int)pop.size() / 3);

  for (int t = 0; t < 20; ++t) {
    int ia = seleccionar_padre_a(pop, beta, rng);
    int ib = seleccionar_padre_b(pop, ia, rng);
    CromosomaLRP C = crossover(pop[ia], pop[ib], mat, inst, rng);
    check_cromosoma(C, inst, "crossover[" + std::to_string(t) + "]");
  }
  std::cout << "       (20 crossovers verificados)\n";
}

// ── Test 3: offspring hereda material de ambos padres ─────────
// Al menos algunos clientes de CS deben venir de A y otros de B.
// (No siempre garantizable con puntos de corte extremos,
//  pero estadísticamente debe ocurrir en la mayoría de casos.)
static void test_herencia(const std::vector<CromosomaLRP> &pop,
                          const Matriz &mat, const InstanciaLRP &inst,
                          std::mt19937 &rng) {
  int beta = std::max(1, (int)pop.size() / 3);
  int heredan_ambos = 0;

  for (int t = 0; t < 50; ++t) {
    int ia = seleccionar_padre_a(pop, beta, rng);
    int ib = seleccionar_padre_b(pop, ia, rng);
    const auto &A = pop[ia];
    const auto &B = pop[ib];
    CromosomaLRP C = crossover(A, B, mat, inst, rng);

    // Verificar que DS de C no es idéntico a A ni a B
    bool igual_a = (C.DS == A.DS);
    bool igual_b = (C.DS == B.DS);
    if (!igual_a && !igual_b)
      ++heredan_ambos;
  }
  // El umbral depende de m: con m pequeño (ej. 5) es normal que
  // muchos padres tengan DS idéntico y el crossover no produzca mezcla.
  // Exigimos al menos 10% de casos con herencia mixta.
  // Con poblaciones pequeñas y m pequeño, muchos individuos comparten
  // el mismo patrón DS — exigimos solo que ocurra al menos alguna vez.
  check(heredan_ambos >= 5,
        "crossover: DS hereda de ambos padres en al menos algunos casos (" +
            std::to_string(heredan_ambos) + "/50)");
}

// ── Test 4: repair post-crossover elimina violaciones ─────────
static void test_repair(const std::vector<CromosomaLRP> &pop, const Matriz &mat,
                        const InstanciaLRP &inst, std::mt19937 &rng) {
  int beta = std::max(1, (int)pop.size() / 3);
  bool siempre_factible = true;

  for (int t = 0; t < 30; ++t) {
    int ia = seleccionar_padre_a(pop, beta, rng);
    int ib = seleccionar_padre_b(pop, ia, rng);
    CromosomaLRP C = crossover(pop[ia], pop[ib], mat, inst, rng);

    for (int i = 0; i < inst.num_depositos; ++i) {
      if (C.DS[i] == 0)
        continue;
      double dem = 0.0;
      for (int c : C.clientes_de(i, inst))
        dem += inst.clientes[c - 1].demanda;
      if (dem > inst.depositos[i].capacidad + 1e-9) {
        siempre_factible = false;
        std::cerr << "  [DIAG] dep[" << i << "] dem=" << dem
                  << " cap=" << inst.depositos[i].capacidad << "\n";
      }
    }
  }
  check(siempre_factible,
        "repair: todos los offspring tienen capacidad de depósito respetada");
}

// ── Test 5: add_in_pop reemplaza al peor ──────────────────────
static void test_add_in_pop(std::vector<CromosomaLRP> pop,
                            const InstanciaLRP &inst) {
  int sz = (int)pop.size();

  // Identificar el peor
  double fitness_peor = pop[0].fitness;
  for (const auto &c : pop)
    if (c.fitness > fitness_peor)
      fitness_peor = c.fitness;

  // Crear un cromosoma artificial con fitness muy bueno
  CromosomaLRP mejor = pop[0];
  mejor.fitness = 1.0; // artificialmente bueno

  double mejor_antes = pop[0].fitness;
  for (const auto &c : pop)
    if (c.fitness < mejor_antes)
      mejor_antes = c.fitness;

  add_in_pop(pop, mejor);

  check((int)pop.size() == sz, "add_in_pop: tamaño de pop no cambia");

  // El peor ya no debe estar (a menos que hubiera empate)
  double nuevo_peor = pop[0].fitness;
  for (const auto &c : pop)
    if (c.fitness > nuevo_peor)
      nuevo_peor = c.fitness;

  check(nuevo_peor <= fitness_peor,
        "add_in_pop: el nuevo peor no es peor que el anterior peor");

  // El fitness 1.0 debe estar en la población
  bool encontrado = false;
  for (const auto &c : pop)
    if (std::abs(c.fitness - 1.0) < 1e-9)
      encontrado = true;
  check(encontrado, "add_in_pop: el nuevo individuo está en la población");

  // Verificar integridad estructural de todos los individuos
  for (int i = 0; i < sz; ++i)
    check_cromosoma(pop[i], inst, "add_in_pop pop[" + std::to_string(i) + "]");
}

// ─────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_operadores_ga <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto mat = construir_matriz_distancias(inst);

  int n = inst.num_clientes;
  int m = inst.num_depositos;
  int NbIndiv = (n + m) / 5 + 1;
  int beta = std::max(1, NbIndiv / 3);

  std::cout << "=== Test Fase 3 MA|PM: Operadores Genéticos ===\n";
  std::cout << "  n=" << n << "  m=" << m << "  NbIndiv=" << NbIndiv
            << "  beta=" << beta << "\n\n";

  std::mt19937 rng(42);
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  std::cout << "-- Test 1: selección --\n";
  test_seleccion(pop, beta, rng);

  std::cout << "\n-- Test 2: crossover produce cromosoma válido --\n";
  test_crossover(pop, mat, inst, rng);

  std::cout << "\n-- Test 3: herencia de ambos padres --\n";
  test_herencia(pop, mat, inst, rng);

  std::cout << "\n-- Test 4: repair elimina violaciones de capacidad --\n";
  test_repair(pop, mat, inst, rng);

  std::cout << "\n-- Test 5: add_in_pop reemplaza al peor --\n";
  test_add_in_pop(pop, inst);

  std::cout << "\n[FASE 3 MA|PM OK] Todos los tests pasaron.\n";
  return 0;
}
