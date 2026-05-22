#include "../src/distancia.h"
#include "../src/mapm/cromosoma.h"
#include "../src/mapm/distancia_sol.h"
#include "../src/mapm/local_search.h"
#include "../src/mapm/poblacion.h"
#include "../src/mapm/split.h"
#include "../src/parser.h"
#include <cassert>
#include <iostream>
#include <limits>
#include <random>
#include <string>

// ─────────────────────────────────────────
static void check(bool ok, const std::string &msg) {
  std::cout << (ok ? "[OK]   " : "[FAIL] ") << msg << "\n";
  if (!ok)
    std::exit(1);
}

// Construir RutasExplicitas de un cromosoma (evalúa + extrae rutas)
static RutasExplicitas rutas_de(CromosomaLRP &crom, const Matriz &mat,
                                const InstanciaLRP &inst) {
  auto sr = evaluar_cromosoma(crom, mat, inst);
  return construir_rutas_explicitas(crom, sr, mat, inst);
}

// ─────────────────────────────────────────
// Test 1: D(T, T) == 0
// ─────────────────────────────────────────
static void test_distancia_reflexiva(const InstanciaLRP &inst,
                                     const Matriz &mat, std::mt19937 &rng) {
  std::cout << "-- Test 1: D(T, T) == 0 --\n";
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  for (int i = 0; i < (int)pop.size(); ++i) {
    auto re = rutas_de(pop[i], mat, inst);
    int d = distancia(re, re, inst);
    check(d == 0, "D(pop[" + std::to_string(i) + "], pop[" + std::to_string(i) +
                      "]) == 0  (got " + std::to_string(d) + ")");
  }
}

// ─────────────────────────────────────────
// Test 2: D(T, U) >= 0  siempre
// ─────────────────────────────────────────
static void test_distancia_no_negativa(const InstanciaLRP &inst,
                                       const Matriz &mat, std::mt19937 &rng) {
  std::cout << "\n-- Test 2: D(T, U) >= 0 --\n";
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  int checks = 0;
  for (int i = 0; i < (int)pop.size(); ++i) {
    for (int j = 0; j < (int)pop.size(); ++j) {
      auto ri = rutas_de(pop[i], mat, inst);
      auto rj = rutas_de(pop[j], mat, inst);
      int d = distancia(ri, rj, inst);
      check(d >= 0, "D(pop[" + std::to_string(i) + "], pop[" +
                        std::to_string(j) + "]) >= 0  (got " +
                        std::to_string(d) + ")");
      ++checks;
    }
  }
  std::cout << "  Pares verificados: " << checks << "\n";
}

// ─────────────────────────────────────────
// Test 3: D(T, U) puede ser asimétrico — D(T,U) != D(U,T) en general
//         Verificamos que al menos para algún par la función no falla.
// ─────────────────────────────────────────
static void test_asimetria(const InstanciaLRP &inst, const Matriz &mat,
                           std::mt19937 &rng) {
  std::cout << "\n-- Test 3: asimetría (no falla, valores razonables) --\n";
  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  if ((int)pop.size() < 2) {
    std::cout << "  [SKIP] población muy pequeña\n";
    return;
  }

  auto r0 = rutas_de(pop[0], mat, inst);
  auto r1 = rutas_de(pop[1], mat, inst);

  int dtu = distancia(r0, r1, inst);
  int dut = distancia(r1, r0, inst);

  std::cout << "  D(T,U) = " << dtu << "   D(U,T) = " << dut << "\n";
  check(dtu >= 0 && dut >= 0, "ambos valores son no negativos");
}

// ─────────────────────────────────────────
// Test 4: los cuatro casos de puntuación
//         Construimos dos cromosomas manualmente para verificar
//         cada rama (0 / 1 / 5 / 10).
// ─────────────────────────────────────────
static void test_casos_puntuacion(const InstanciaLRP &inst, const Matriz &mat) {
  std::cout << "\n-- Test 4: casos de puntuación (0 / 1 / 5 / 10) --\n";

  if (inst.num_clientes < 4 || inst.num_depositos < 1) {
    std::cout << "  [SKIP] instancia demasiado pequeña\n";
    return;
  }

  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Construimos T y U con rutas explícitas a mano,
  // lo suficientemente controladas para disparar cada caso.

  // T: depósito 0, una sola ruta con clientes 1→2→3→4
  RutasExplicitas T;
  T.rutas.resize(m);
  T.rutas[0].push_back({1, 2, 3, 4});
  T.costo = 0.0;

  // ── Caso 0: U idéntico a T → D = 0
  {
    RutasExplicitas U;
    U.rutas.resize(m);
    U.rutas[0].push_back({1, 2, 3, 4});
    U.costo = 0.0;
    int d = distancia(T, U, inst);
    check(d == 0,
          "caso 0: rutas idénticas → D = 0  (got " + std::to_string(d) + ")");
  }

  // ── Caso 1: U misma ruta pero orden distinto → pares no adyacentes → +1 c/u
  // T tiene pares: (1,2),(2,3),(3,4)
  // U tiene ruta {2,1,4,3} → (1,2) sí adj (simétr.), (2,3) no adj, (3,4) sí adj
  // par (1,2): adyacente en U → 0
  // par (2,3): misma ruta U pero no adj → 1
  // par (3,4): adyacente en U → 0
  // Esperado: 1
  {
    RutasExplicitas U;
    U.rutas.resize(m);
    U.rutas[0].push_back({2, 1, 4, 3});
    U.costo = 0.0;
    int d = distancia(T, U, inst);
    // par (1,2): en U son adyacentes (2→1) → 0
    // par (2,3): en U, ruta {2,1,4,3}: mismo dep, misma ruta, no adj → 1
    // par (3,4): en U son adyacentes (4→3) → 0
    check(d == 1, "caso 1 (misma ruta, no adj) → D = 1  (got " +
                      std::to_string(d) + ")");
  }

  // ── Caso 5: U mismo depósito pero rutas separadas
  // U depósito 0: ruta_0={1,2}, ruta_1={3,4}
  // par (2,3) de T: mismo dep, distinta ruta → 5
  // par (1,2): adj en ruta_0 → 0
  // par (3,4): adj en ruta_1 → 0
  // Esperado: 5
  {
    RutasExplicitas U;
    U.rutas.resize(m);
    U.rutas[0].push_back({1, 2});
    U.rutas[0].push_back({3, 4});
    U.costo = 0.0;
    int d = distancia(T, U, inst);
    check(d == 5, "caso 5 (mismo dep, distinta ruta) → D = 5  (got " +
                      std::to_string(d) + ")");
  }

  // ── Caso 10: clientes en depósitos distintos
  // Requiere m >= 2
  if (m >= 2) {
    // U: dep_0 tiene clientes 1,2 / dep_1 tiene clientes 3,4
    // par (2,3) de T: dep distintos → 10
    RutasExplicitas U;
    U.rutas.resize(m);
    U.rutas[0].push_back({1, 2});
    U.rutas[1].push_back({3, 4});
    U.costo = 0.0;
    int d = distancia(T, U, inst);
    // par (1,2): adj en dep_0 → 0
    // par (2,3): dep distintos → 10
    // par (3,4): adj en dep_1 → 0
    check(d == 10, "caso 10 (depósitos distintos) → D = 10  (got " +
                       std::to_string(d) + ")");
  } else {
    std::cout << "  [SKIP] caso 10: se necesita m >= 2\n";
  }
}

// ─────────────────────────────────────────
// Test 5: distancia_poblacion filtra duplicados exactos
// ─────────────────────────────────────────
static void test_filtrado_duplicados(const InstanciaLRP &inst,
                                     const Matriz &mat, std::mt19937 &rng) {
  std::cout << "\n-- Test 5: filtrado de duplicados con umbral Δ --\n";

  int NbIndiv = (inst.num_clientes + inst.num_depositos) / 5 + 1;
  auto pop = generar_poblacion(NbIndiv, inst, mat, rng);

  // Construir vector de rutas de la población
  std::vector<RutasExplicitas> pop_rutas;
  for (auto &c : pop) {
    auto sr = evaluar_cromosoma(c, mat, inst);
    pop_rutas.push_back(construir_rutas_explicitas(c, sr, mat, inst));
  }

  // Un individuo idéntico al primero debe tener distancia 0
  {
    int d = distancia_poblacion(pop_rutas[0], pop_rutas, inst);
    check(d == 0, "individuo idéntico a pop[0] tiene distancia 0 a la pop");
  }

  // Con Δ=1, ese individuo sería rechazado (d < Δ)
  {
    int Delta = 1;
    int d = distancia_poblacion(pop_rutas[0], pop_rutas, inst);
    bool rechazado = (d < Delta);
    check(rechazado, "duplicado es rechazado con Δ=1 (d=" + std::to_string(d) +
                         " < " + std::to_string(Delta) + ")");
  }

  // Una solución nueva (que no está en la pop) debería tener d > 0
  // la generamos con una semilla distinta
  {
    std::mt19937 rng2(999);
    auto pop2 = generar_poblacion(NbIndiv, inst, mat, rng2);
    std::vector<RutasExplicitas> pop2_rutas;
    for (auto &c : pop2) {
      auto sr = evaluar_cromosoma(c, mat, inst);
      pop2_rutas.push_back(construir_rutas_explicitas(c, sr, mat, inst));
    }

    int diversas = 0;
    for (auto &re : pop2_rutas) {
      int d = distancia_poblacion(re, pop_rutas, inst);
      if (d > 0)
        ++diversas;
    }
    std::cout << "  Soluciones de pop2 con d>0 respecto a pop1: " << diversas
              << "/" << pop2_rutas.size() << "\n";
    check(diversas > 0,
          "al menos una solución de pop2 tiene distancia > 0 a pop1");
  }
}

// ─────────────────────────────────────────
// Test 6: lógica de umbral Δ dinámico
// ─────────────────────────────────────────
static void test_umbral_dinamico(const InstanciaLRP &inst, const Matriz &mat,
                                 std::mt19937 &rng) {
  std::cout << "\n-- Test 6: lógica de umbral Δ dinámico --\n";

  int n = inst.num_clientes;
  int m = inst.num_depositos;
  int NbIndiv = (n + m) / 5 + 1;
  int Delta_max = (n + m) / 10 + 10;
  int MaxNbRej = NbIndiv;

  // Simular la lógica de control de Δ del paper (Sección 3.4):
  //   - Si NbRej > MaxNbRej → Δ = max(1, Δ-1)
  //   - Si nuevo best       → Δ = Delta_max
  int Delta = Delta_max;
  int NbRej = 0;

  // Simular MaxNbRej+1 rechazos → Δ debe bajar
  for (int i = 0; i < MaxNbRej + 1; ++i)
    ++NbRej;
  if (NbRej > MaxNbRej)
    Delta = std::max(1, Delta - 1);

  check(Delta == Delta_max - 1, "tras MaxNbRej+1 rechazos, Δ baja en 1 (Δ=" +
                                    std::to_string(Delta) + ")");

  // Nuevo best → Δ se resetea
  Delta = Delta_max;
  check(Delta == Delta_max, "tras nuevo best, Δ se resetea a Delta_max (" +
                                std::to_string(Delta) + ")");

  // Δ nunca baja de 1
  Delta = 1;
  NbRej = MaxNbRej + 1;
  if (NbRej > MaxNbRej)
    Delta = std::max(1, Delta - 1);
  check(Delta == 1, "Δ no baja de 1 (Δ=" + std::to_string(Delta) + ")");

  (void)mat;
  (void)rng; // usados en otros tests
}

// ─────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_distancia <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto mat = construir_matriz_distancias(inst);

  int n = inst.num_clientes;
  int m = inst.num_depositos;

  std::cout << "=== Test Fase 5 MA|PM: Distancia + Population Management ===\n";
  std::cout << "  n=" << n << "  m=" << m << "\n\n";

  std::mt19937 rng(42);

  test_distancia_reflexiva(inst, mat, rng);
  test_distancia_no_negativa(inst, mat, rng);
  test_asimetria(inst, mat, rng);
  test_casos_puntuacion(inst, mat);
  test_filtrado_duplicados(inst, mat, rng);
  test_umbral_dinamico(inst, mat, rng);

  std::cout << "\n[FASE 5 MA|PM OK] Todos los tests pasaron.\n";
  return 0;
}
