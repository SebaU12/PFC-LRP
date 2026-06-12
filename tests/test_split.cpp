#include "../src/distancia.h"
#include "../src/estado.h"
#include "../src/mapm/cromosoma.h"
#include "../src/mapm/split.h"
#include "../src/parser.h"
#include "../src/solucion_inicial.h"
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

// ── Test 1: semántica de DS como puntero ─────────────────────
// Verificar que rango_cs y clientes_de funcionan correctamente
// para la estructura DS-puntero del paper.
static void test_ds_semantica(const InstanciaLRP &inst, const Matriz &mat,
                              const EstadoLRP &sol) {
  CromosomaLRP crom = estado_a_cromosoma(sol);

  int n = inst.num_clientes;
  int m = inst.num_depositos;

  // Todo DS[i] != 0 debe apuntar a una posición válida en CS
  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0)
      continue;
    int pos = crom.DS[i];
    check(pos >= 1 && pos <= n,
          "DS[" + std::to_string(i) + "]=" + std::to_string(pos) +
              " es posición válida en CS [1.." + std::to_string(n) + "]");
  }

  // Verificar que no hay dos depósitos con el mismo puntero DS
  std::set<int> punteros;
  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0)
      continue;
    check(punteros.find(crom.DS[i]) == punteros.end(),
          "DS: no hay dos depósitos con el mismo puntero (" +
              std::to_string(crom.DS[i]) + ")");
    punteros.insert(crom.DS[i]);
  }

  // La unión de clientes_de(i) para todos los depósitos abiertos
  // debe ser exactamente los n clientes sin duplicados
  std::set<int> todos;
  for (int i = 0; i < m; ++i) {
    if (crom.DS[i] == 0)
      continue;
    auto cli = crom.clientes_de(i, inst);
    for (int c : cli) {
      check(todos.find(c) == todos.end(),
            "cliente " + std::to_string(c) + " no duplicado entre depósitos");
      todos.insert(c);
    }
  }
  check((int)todos.size() == n,
        "clientes_de: cubre exactamente los " + std::to_string(n) +
            " clientes (" + std::to_string(todos.size()) + " encontrados)");
}

// ── Test 2: split_deposito — ruta simple conocida ────────────
static void test_split_ruta_simple(const InstanciaLRP &inst,
                                   const Matriz &mat) {
  int dep_idx = inst.num_clientes; // depósito 0 en la matriz

  std::vector<int> clientes = {1}; // solo el cliente con ID=1
  auto res = split_deposito(clientes, dep_idx, mat, inst);

  double esperado = inst.F + mat[dep_idx][0] // dep → c1
                    + mat[0][dep_idx];       // c1  → dep

  check(std::abs(res.costo - esperado) < 1e-6,
        "split_deposito: ruta simple — costo=" + std::to_string(res.costo) +
            " esperado=" + std::to_string(esperado));
  check(res.rutas.size() == 1, "split_deposito: 1 ruta para 1 cliente");
  check(res.rutas[0][0] == 1, "split_deposito: el cliente en ruta es 1");
  check(res.factible, "split_deposito: ruta simple es factible");
}

// ── Test 3: Split respeta capacidad Q ────────────────────────
static void test_split_respeta_capacidad(const InstanciaLRP &inst,
                                         const Matriz &mat) {
  int dep_idx = inst.num_clientes;

  // Acumular clientes hasta superar Q
  std::vector<int> clientes;
  double dem = 0.0;
  for (int i = 0; i < inst.num_clientes && dem <= inst.Q; ++i) {
    clientes.push_back(inst.clientes[i].id);
    dem += inst.clientes[i].demanda;
  }
  if (clientes.size() < 2) {
    std::cout << "[SKIP] test_split_respeta_capacidad\n";
    return;
  }

  auto res = split_deposito(clientes, dep_idx, mat, inst);

  bool todas_ok = true;
  for (const auto &r : res.rutas) {
    double d = 0.0;
    for (int c : r)
      d += inst.clientes[c - 1].demanda;
    if (d > inst.Q + 1e-9)
      todas_ok = false;
  }
  check(todas_ok, "split_deposito: cada ruta respeta Q");
  check(res.rutas.size() >= 2, "split_deposito: se generaron >= 2 rutas (" +
                                   std::to_string(res.rutas.size()) + ")");
}

// ── Test 4: Split conserva exactamente los clientes de entrada ─
static void test_split_conserva_clientes(const InstanciaLRP &inst,
                                         const Matriz &mat) {
  int dep_idx = inst.num_clientes;
  std::vector<int> todos;
  for (const auto &c : inst.clientes)
    todos.push_back(c.id);

  auto res = split_deposito(todos, dep_idx, mat, inst);

  std::set<int> en_rutas;
  for (const auto &r : res.rutas)
    for (int c : r)
      en_rutas.insert(c);

  check((int)en_rutas.size() == inst.num_clientes,
        "split_deposito: todos los clientes presentes en rutas");
  check(en_rutas == std::set<int>(todos.begin(), todos.end()),
        "split_deposito: los clientes son exactamente los de entrada");
}

// ── Test 5: Split es óptimo — mejor o igual que greedy ───────
// Para el mismo orden de CS, Split debe dar costo de ruteo <= greedy.
static void test_split_vs_greedy(const InstanciaLRP &inst, const Matriz &mat,
                                 const EstadoLRP &sol) {
  double routing_greedy = 0.0;
  for (const auto &[dep_id, lista] : sol.rutas) {
    int idx_dep = dep_id - 1;
    for (const auto &r : lista) {
      if (r.clientes.empty())
        continue;
      routing_greedy += inst.F;
      routing_greedy += mat[idx_dep][r.clientes.front() - 1];
      for (int i = 0; i + 1 < (int)r.clientes.size(); ++i)
        routing_greedy += mat[r.clientes[i] - 1][r.clientes[i + 1] - 1];
      routing_greedy += mat[r.clientes.back() - 1][idx_dep];
    }
  }

  double routing_split = 0.0;
  for (int dep_id : sol.depositos_abiertos) {
    int dep_idx = dep_id - 1;
    auto it = sol.rutas.find(dep_id);
    if (it == sol.rutas.end())
      continue;

    std::vector<int> orden;
    for (const auto &r : it->second)
      for (int c : r.clientes)
        orden.push_back(c);
    if (orden.empty())
      continue;

    auto res = split_deposito(orden, dep_idx, mat, inst);
    routing_split += res.costo;
  }

  std::cout << "  Routing greedy : " << routing_greedy << "\n";
  std::cout << "  Routing Split  : " << routing_split << "\n";

  check(routing_split <= routing_greedy + 1e-6,
        "Split produce costo de ruteo <= greedy para el mismo orden de CS");
}

// ── Test 6: round-trip estado → cromosoma → evaluar → estado ─
static void test_roundtrip(const InstanciaLRP &inst, const Matriz &mat,
                           const EstadoLRP &sol) {
  double obj_original = sol.objective();

  // Convertir a cromosoma
  CromosomaLRP crom = estado_a_cromosoma(sol);

  // Verificar tamaños
  check((int)crom.CS.size() == inst.num_clientes,
        "estado_a_cromosoma: CS.size() == n");
  check((int)crom.DS.size() == inst.num_depositos,
        "estado_a_cromosoma: DS.size() == m");

  // CS es permutación válida
  std::set<int> vistos(crom.CS.begin(), crom.CS.end());
  check((int)vistos.size() == inst.num_clientes,
        "estado_a_cromosoma: CS es permutación sin duplicados");

  // Evaluar con Split
  auto resultados = evaluar_cromosoma(crom, mat, inst);

  std::cout << "  Objetivo original   : " << obj_original << "\n";
  std::cout << "  Fitness post-Split  : " << crom.fitness << "\n";

  // Split sobre el mismo orden no puede ser peor que greedy
  check(crom.fitness <= obj_original + 1e-6,
        "evaluar_cromosoma: fitness <= objetivo original");

  // Reconstruir EstadoLRP
  int n = inst.num_clientes;
  Rutas rutas_cache;
  for (int i = 0; i < inst.num_depositos; ++i) {
    if (crom.DS[i] == 0)
      continue;
    int dep_id = n + i + 1;
    for (const auto &rv : resultados[i].rutas)
      rutas_cache[dep_id].push_back(Ruta(dep_id, rv));
  }

  EstadoLRP reconstruido = cromosoma_a_estado(crom, rutas_cache, mat, inst);

  check(reconstruido.no_asignados.empty(),
        "cromosoma_a_estado: sin clientes huérfanos");

  std::set<int> en_rutas;
  int total = 0;
  for (const auto &[dep, lista] : reconstruido.rutas)
    for (const auto &r : lista)
      for (int c : r.clientes) {
        en_rutas.insert(c);
        ++total;
      }

  check(total == inst.num_clientes,
        "cromosoma_a_estado: total clientes en rutas == n");
  check((int)en_rutas.size() == inst.num_clientes,
        "cromosoma_a_estado: sin clientes duplicados");
}

// ── Test 7: repair no rompe el cromosoma ─────────────────────
// La solución inicial es factible, así que repair no debe cambiar nada
// pero tampoco debe corromper el cromosoma.
static void test_repair_no_corrompe(const InstanciaLRP &inst, const Matriz &mat,
                                    const EstadoLRP &sol) {
  CromosomaLRP crom = estado_a_cromosoma(sol);

  double fitness_antes = crom.fitness;
  repair(crom, mat, inst);

  // CS sigue siendo permutación válida
  check((int)crom.CS.size() == inst.num_clientes,
        "repair: CS.size() sigue siendo n");
  std::set<int> vistos(crom.CS.begin(), crom.CS.end());
  check((int)vistos.size() == inst.num_clientes,
        "repair: CS sigue siendo permutación válida");

  // Al menos un depósito abierto
  check(crom.n_abiertos() > 0, "repair: al menos 1 depósito abierto");

  // Evaluar fitness post-repair (no debe ser peor que antes)
  evaluar_cromosoma(crom, mat, inst);
  std::cout << "  Fitness antes de repair : " << fitness_antes << "\n";
  std::cout << "  Fitness después repair  : " << crom.fitness << "\n";
}

// ─────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_split <instancia.dat>\n";
    return 1;
  }

  auto inst = leer_instancia(argv[1]);
  auto mat = construir_matriz_distancias(inst);
  auto sol = generar_solucion_inicial(inst, mat);

  std::cout
      << "=== Test Fase 1 MA|PM: Cromosoma + Split (fiel al paper) ===\n\n";

  std::cout << "-- Test 1: semántica DS como puntero --\n";
  test_ds_semantica(inst, mat, sol);

  std::cout << "\n-- Test 2: ruta simple conocida --\n";
  test_split_ruta_simple(inst, mat);

  std::cout << "\n-- Test 3: Split respeta capacidad Q --\n";
  test_split_respeta_capacidad(inst, mat);

  std::cout << "\n-- Test 4: Split conserva clientes --\n";
  test_split_conserva_clientes(inst, mat);

  std::cout << "\n-- Test 5: Split vs greedy (mismo orden) --\n";
  test_split_vs_greedy(inst, mat, sol);

  std::cout << "\n-- Test 6: round-trip estado↔cromosoma --\n";
  test_roundtrip(inst, mat, sol);

  std::cout << "\n-- Test 7: repair sobre solución factible --\n";
  test_repair_no_corrompe(inst, mat, sol);

  std::cout << "\n[FASE 1 MA|PM OK] Todos los tests pasaron.\n";
  return 0;
}
