#include "../src/distancia.h"
#include "../src/mapm/cromosoma.h"
#include "../src/mapm/local_search.h"
#include "../src/mapm/operadores_ga.h"
#include "../src/mapm/poblacion.h"
#include "../src/mapm/split.h"
#include "../src/parser.h"
#include "../src/solucion_inicial.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>

static void check(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "[FAIL] " << msg << "\n";
    std::exit(1);
  }
  std::cout << "[OK]   " << msg << "\n";
}

// Verifica invariantes del cromosoma post-LS
static void check_cromosoma_ls(const CromosomaLRP &c, const InstanciaLRP &inst,
                               const std::string &tag) {
  int n = inst.num_clientes;
  int m = inst.num_depositos;

  check((int)c.CS.size() == n, tag + ": CS.size()==n");
  check((int)c.DS.size() == m, tag + ": DS.size()==m");
  check(c.n_abiertos() > 0, tag + ": al menos 1 depósito abierto");

  std::set<int> vistos(c.CS.begin(), c.CS.end());
  check((int)vistos.size() == n, tag + ": CS sin duplicados");

  std::set<int> cubiertos;
  for (int i = 0; i < m; ++i) {
    if (c.DS[i] == 0)
      continue;
    for (int cl : c.clientes_de(i, inst))
      cubiertos.insert(cl);
  }
  check((int)cubiertos.size() == n, tag + ": todos los clientes cubiertos");

  check(c.fitness > 0 && c.fitness < 1e13,
        tag + ": fitness finito (" + std::to_string(c.fitness) + ")");
}

// ── Test 1: LS1 no empeora el fitness ────────────────────────
static void test_ls1_no_empeora(const InstanciaLRP &inst, const Matriz &mat,
                                std::mt19937 &rng) {
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  int mejoras = 0;
  for (int t = 0; t < (int)pop.size(); ++t) {
    CromosomaLRP c = pop[t];
    double fitness_antes = c.fitness;
    auto sr = evaluar_cromosoma(c, mat, inst);
    double fitness_ls = c.fitness; // tras evaluar (Split)

    double delta = LS1(c, sr, mat, inst);

    check(c.fitness <= fitness_ls + 1e-6,
          "LS1[" + std::to_string(t) + "]: fitness no empeora (" +
              std::to_string(fitness_ls) + " → " + std::to_string(c.fitness) +
              ")");
    check(std::abs(delta - (fitness_ls - c.fitness)) < 1e-3,
          "LS1[" + std::to_string(t) + "]: delta consistente");
    if (c.fitness < fitness_antes - 1e-6)
      ++mejoras;
    check_cromosoma_ls(c, inst, "LS1[" + std::to_string(t) + "]");
  }
  std::cout << "       LS1 mejoró " << mejoras << "/" << pop.size()
            << " individuos\n";
}

// ── Test 2: LS2 no empeora y es más conservadora que LS1 ─────
static void test_ls2_no_empeora(const InstanciaLRP &inst, const Matriz &mat,
                                std::mt19937 &rng) {
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  for (int t = 0; t < (int)pop.size(); ++t) {
    CromosomaLRP c = pop[t];
    auto sr = evaluar_cromosoma(c, mat, inst);
    double fitness_antes = c.fitness;

    LS2(c, sr, mat, inst);

    check(c.fitness <= fitness_antes + 1e-6,
          "LS2[" + std::to_string(t) + "]: fitness no empeora");
    check_cromosoma_ls(c, inst, "LS2[" + std::to_string(t) + "]");
  }
}

// ── Test 3: LS1 >= LS2 en calidad (LS1 más potente) ──────────
static void test_ls1_mejor_que_ls2(const InstanciaLRP &inst, const Matriz &mat,
                                   std::mt19937 &rng) {
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  double sum_ls1 = 0, sum_ls2 = 0;
  for (auto &base : pop) {
    CromosomaLRP c1 = base, c2 = base;
    auto sr1 = evaluar_cromosoma(c1, mat, inst);
    auto sr2 = evaluar_cromosoma(c2, mat, inst);
    LS1(c1, sr1, mat, inst);
    LS2(c2, sr2, mat, inst);
    sum_ls1 += c1.fitness;
    sum_ls2 += c2.fitness;
  }
  std::cout << "  Fitness promedio LS1: " << sum_ls1 / pop.size() << "\n";
  std::cout << "  Fitness promedio LS2: " << sum_ls2 / pop.size() << "\n";
  check(sum_ls1 <= sum_ls2 + 1e-6,
        "LS1 produce fitness promedio <= LS2 (LS1 más potente o igual)");
}

// ── Test 4: recalcular_costo es consistente con costo de LS ──
static void test_costo_consistente(const InstanciaLRP &inst, const Matriz &mat,
                                   std::mt19937 &rng) {
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  for (int t = 0; t < (int)pop.size(); ++t) {
    CromosomaLRP c = pop[t];
    auto sr = evaluar_cromosoma(c, mat, inst);

    RutasExplicitas re = construir_rutas_explicitas(c, sr, mat, inst);
    double costo_recalc = recalcular_costo(re, mat, inst);

    check(std::abs(re.costo - costo_recalc) < 1e-3,
          "costo_consistente[" + std::to_string(t) +
              "]: re.costo=" + std::to_string(re.costo) +
              " recalc=" + std::to_string(costo_recalc));
  }
}

// ── Test 5: LS1 sobre crossovers ─────────────────────────────
static void test_ls1_sobre_offspring(const InstanciaLRP &inst,
                                     const Matriz &mat, std::mt19937 &rng) {
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  int beta = std::max(1, NbIndiv / 3);
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  int mejoras = 0;
  for (int t = 0; t < 10; ++t) {
    int ia = seleccionar_padre_a(pop, beta, rng);
    int ib = seleccionar_padre_b(pop, ia, rng);
    CromosomaLRP C = crossover(pop[ia], pop[ib], mat, inst, rng);
    double antes = C.fitness;
    auto sr = evaluar_cromosoma(C, mat, inst);
    LS1(C, sr, mat, inst);
    if (C.fitness < antes - 1e-6)
      ++mejoras;
    check_cromosoma_ls(C, inst, "LS1_offspring[" + std::to_string(t) + "]");
  }
  std::cout << "       LS1 mejoró " << mejoras << "/10 offspring\n";
}

// ─────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_local_search <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto mat = construir_matriz_distancias(inst);

  int n = inst.num_clientes;
  int m = inst.num_depositos;
  std::cout << "=== Test Fase 4 MA|PM: Búsqueda Local ===\n";
  std::cout << "  n=" << n << "  m=" << m << "\n\n";

  std::mt19937 rng(42);

  std::cout << "-- Test 1: LS1 no empeora --\n";
  test_ls1_no_empeora(inst, mat, rng);

  std::cout << "\n-- Test 2: LS2 no empeora --\n";
  test_ls2_no_empeora(inst, mat, rng);

  std::cout << "\n-- Test 3: LS1 >= LS2 en calidad --\n";
  test_ls1_mejor_que_ls2(inst, mat, rng);

  std::cout << "\n-- Test 4: costo consistente --\n";
  test_costo_consistente(inst, mat, rng);

  std::cout << "\n-- Test 5: LS1 sobre offspring --\n";
  test_ls1_sobre_offspring(inst, mat, rng);

  std::cout << "\n[FASE 4 MA|PM OK] Todos los tests pasaron.\n";
  return 0;
}
