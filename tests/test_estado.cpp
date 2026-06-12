#include "../src/distancia.h"
#include "../src/estado.h"
#include "../src/parser.h"
#include <cassert>
#include <cmath>
#include <iostream>

// ── Helpers ──────────────────────────────────────────────────

static void check(bool cond, const std::string &msg) {
  if (!cond) {
    std::cerr << "[FAIL] " << msg << "\n";
    std::exit(1);
  }
  std::cout << "[OK]   " << msg << "\n";
}

// ── Test 1: dimensiones de la matriz ─────────────────────────
static void test_matriz_dimensiones(const InstanciaLRP &inst, const Matriz &m) {
  int N = inst.num_clientes + inst.num_depositos;
  check(static_cast<int>(m.size()) == N, "Matriz: filas == N");
  check(static_cast<int>(m[0].size()) == N, "Matriz: columnas == N");
}

// ── Test 2: diagonal cero ────────────────────────────────────
static void test_matriz_diagonal(const Matriz &m) {
  for (int i = 0; i < (int)m.size(); ++i)
    check(m[i][i] == 0.0, "Diagonal [" + std::to_string(i) + "][" +
                              std::to_string(i) + "] == 0");
}

// ── Test 3: simetría ─────────────────────────────────────────
static void test_matriz_simetria(const Matriz &m) {
  for (int i = 0; i < (int)m.size(); ++i)
    for (int j = i + 1; j < (int)m.size(); ++j)
      check(m[i][j] == m[j][i],
            "Simetría [" + std::to_string(i) + "][" + std::to_string(j) + "]");
  std::cout << "       (simetría verificada para todos los pares)\n";
}

// ── Test 4: valor conocido depósito 0 ↔ cliente 0 ───────────
// Depósito 1 (idx=num_clientes): x=6, y=7
// Cliente  1 (idx=0):            x=20, y=35
// dist = sqrt((20-6)^2 + (35-7)^2) = sqrt(196+784) = sqrt(980) ≈ 31.3050
// es_entero=true → int(31.3050 * 100) = 3130
static void test_valor_conocido(const InstanciaLRP &inst, const Matriz &m) {
  int idx_c0 = 0;                 // cliente 1 → índice 0
  int idx_d0 = inst.num_clientes; // depósito 1 → índice num_clientes
  double esperado = inst.es_entero ? static_cast<int>(std::sqrt(980.0) * 100)
                                   : std::sqrt(980.0);
  check(m[idx_c0][idx_d0] == esperado,
        "dist(cliente1, dep1) == " + std::to_string(esperado));
}

// ── Test 5: objective() con solución vacía (todos huérfanos) ─
static void test_objective_huerfanos(const InstanciaLRP &inst,
                                     const Matriz &m) {
  std::vector<int> dep_ids;
  for (const auto &d : inst.depositos)
    dep_ids.push_back(d.id);

  std::vector<int> todos;
  for (const auto &c : inst.clientes)
    todos.push_back(c.id);

  Rutas r;
  for (int d : dep_ids)
    r[d] = {};

  EstadoLRP s(r, dep_ids, todos, m, inst);

  // Costo esperado = apertura de todos los depósitos + 100000 * num_clientes
  double apertura = 0;
  for (const auto &d : inst.depositos)
    apertura += d.costo_apertura;
  double esperado = apertura + 100000.0 * inst.num_clientes;

  check(s.objective() == esperado,
        "objective() con todos los clientes huérfanos");
}

// ── Test 6: objective() con una ruta mínima manual ───────────
// Depósito 101 (idx=100), cliente 1 (idx=0): ruta [1]
// Costo = apertura_101 + F + dist(dep→c1) + dist(c1→dep)
//       = 11220 + 5000 + 1720 + 1720 = 19660
static void test_objective_ruta_simple(const InstanciaLRP &inst,
                                       const Matriz &m) {
  int dep_id = inst.depositos[0].id; // 101
  Rutas r;
  r[dep_id] = {Ruta(dep_id, {1})}; // una ruta con solo el cliente 1

  std::vector<int> dep_ids = {dep_id};
  EstadoLRP s(r, dep_ids, {}, m, inst);

  double esperado = inst.depositos[0].costo_apertura + inst.F +
                    m[inst.num_clientes][0]    // dep101 → cliente1
                    + m[0][inst.num_clientes]; // cliente1 → dep101

  check(std::abs(s.objective() - esperado) < 1e-6,
        "objective() ruta simple dep101→c1→dep101 == " +
            std::to_string(esperado));
}

// ─────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Uso: ./test_estado <instancia.dat>\n";
    return 1;
  }

  InstanciaLRP inst = leer_instancia(argv[1]);
  Matriz m = construir_matriz_distancias(inst);

  std::cout << "=== Test Fase 2: Matriz de Distancias y EstadoLRP ===\n\n";

  std::cout << "-- Matriz de distancias --\n";
  test_matriz_dimensiones(inst, m);
  test_matriz_diagonal(m);
  test_matriz_simetria(m);
  test_valor_conocido(inst, m);

  std::cout << "\n-- EstadoLRP::objective() --\n";
  test_objective_huerfanos(inst, m);
  test_objective_ruta_simple(inst, m);

  std::cout << "\n[FASE 2 OK] Todos los tests pasaron.\n";
  return 0;
}
