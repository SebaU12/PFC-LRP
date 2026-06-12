// =============================================================================
// alns.cpp — Loop principal del ALNS
// =============================================================================
//
// Aquí están las dos "máquinas" que hacen funcionar el ALNS:
//
//   RouletteWheel: gestiona los pesos de los operadores y los elige
//                 probabilísticamente. Aprende qué operadores funcionan bien.
//
//   SimulatedAnnealing: decide si aceptar una solución peor que la actual.
//                       Al principio acepta casi todo (temperatura alta),
//                       al final solo acepta mejoras (temperatura baja).
//
// =============================================================================
#include "alns.h"
#include <cmath>
#include <numeric>
#include <stdexcept>

// =============================================================================
// Clase RouletteWheel — Ruleta adaptativa de selección de operadores
// =============================================================================
//
// Cada operador tiene un "peso" que representa su historial de éxito relativo.
// Cuando se llama seleccionar(), se elige un operador con probabilidad
// proporcional a su peso — los más pesados salen más seguido.
//
// Al final de cada "segmento" de iteraciones, los pesos se actualizan con
// la fórmula exponencial suavizada:
//   w_i = w_i × (1 − ρ) + ρ × (score_i / usos_i)
//
// Con ρ cercano a 1 el sistema aprende rápido pero olvida rápido.
// Con ρ cercano a 0 el sistema aprende despacio pero retiene memoria.
// =============================================================================
class RouletteWheel {
public:
  // Inicializar con todos los pesos iguales (1.0) — sin preferencia inicial
  explicit RouletteWheel(int n_operadores)
      : n(n_operadores), pesos(n_operadores, 1.0),
        scores(n_operadores, 0.0), usos(n_operadores, 0) {}

  // Seleccionar un operador: genera un número aleatorio en [0, suma_de_pesos]
  // y avanza por la "rueda" sumando pesos hasta superar ese número.
  int seleccionar(std::mt19937 &rng) const {
    double total = std::accumulate(pesos.begin(), pesos.end(), 0.0);
    std::uniform_real_distribution<double> dist(0.0, total);
    double r = dist(rng), acum = 0.0;
    for (int i = 0; i < n; ++i) {
      acum += pesos[i];
      if (r <= acum) return i;
    }
    return n - 1; // fallback por precisión de punto flotante
  }

  // Anotar el puntaje obtenido por el operador i en esta iteración
  void registrar(int i, double sigma) {
    scores[i] += sigma;
    usos[i] += 1;
  }

  // Actualizar pesos al final del segmento usando la fórmula:
  //   w_i = w_i*(1-ρ) + ρ*(score_i/usos_i)
  // Luego resetear scores y usos para el próximo segmento.
  void actualizar_pesos(double rho) {
    for (int i = 0; i < n; ++i) {
      if (usos[i] > 0)
        pesos[i] = pesos[i] * (1.0 - rho) + rho * (scores[i] / usos[i]);
      scores[i] = 0.0;
      usos[i] = 0;
    }
  }

  const std::vector<double> &get_pesos() const { return pesos; }

private:
  int n;
  std::vector<double> pesos;  // pesos actuales de cada operador
  std::vector<double> scores; // suma de puntajes en el segmento actual
  std::vector<int> usos;      // cuántas veces se usó en el segmento actual
};

// =============================================================================
// Clase SimulatedAnnealing — Criterio de aceptación de soluciones peores
// =============================================================================
//
// El SA permite aceptar soluciones peores que la actual con cierta probabilidad.
// Esto evita que el algoritmo quede atrapado en un óptimo local: a veces hay
// que "subir una colina" para luego poder "bajar a un valle más profundo".
//
// La probabilidad de aceptar una solución peor es:
//   P = exp(-(costo_nuevo - costo_actual) / T)
//
// Con T alta (inicio): P ≈ 1 → acepta casi todo (exploración amplia).
// Con T baja (final):  P ≈ 0 → solo acepta mejoras (explotación local).
//
// La temperatura baja en cada iteración: T = max(T × α, T_end).
// =============================================================================
class SimulatedAnnealing {
public:
  SimulatedAnnealing(double T0, double T_end, double alpha)
      : T(T0), T_end(T_end), alpha(alpha) {
    if (alpha <= 0.0 || alpha >= 1.0)
      throw std::invalid_argument("factor_enfriamiento debe estar en (0,1)");
    if (T0 <= T_end)
      throw std::invalid_argument("temperatura_inicial debe ser > temperatura_final");
  }

  // Decidir si aceptar s_nuevo con costo costo_nuevo, viniendo de costo_actual.
  // Si s_nuevo es mejor (o igual), siempre se acepta.
  // Si es peor, se acepta con probabilidad exp(-Δ/T).
  bool aceptar(double costo_nuevo, double costo_actual, std::mt19937 &rng) {
    if (costo_nuevo <= costo_actual) return true;
    double prob = std::exp(-(costo_nuevo - costo_actual) / T);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < prob;
  }

  // Reducir la temperatura: T = max(T × α, T_end).
  // Se llama una vez por iteración.
  void enfriar() { T = std::max(T * alpha, T_end); }

  double temperatura() const { return T; }

private:
  double T;      // temperatura actual (empieza en T0 y baja)
  double T_end;  // temperatura mínima (no bajar de aquí)
  double alpha;  // factor de enfriamiento ∈ (0,1)
};

// =============================================================================
// ejecutar_alns — El loop principal
// =============================================================================
//
// Cada iteración:
//   1. Seleccionar destructor y reparador con la ruleta
//   2. s_nuevo = reparador(destructor(s.copy(), rng), rng)
//   3. Clasificar s_nuevo en uno de 4 casos:
//        σ1: es el nuevo mejor global → guardar como s_best, actualizar s
//        σ2: mejora s actual pero no el global → actualizar s
//        σ3: aceptado por SA aunque sea peor → actualizar s
//        σ4: rechazado → s no cambia
//   4. Registrar el puntaje obtenido en la ruleta
//   5. Enfriar la temperatura
//   6. Cada `segmento` iteraciones, recalcular pesos de la ruleta
//
// Nota sobre s.copy() en el destructor:
//   Se pasa una copia de s, no s mismo, para que si el resultado es peor
//   podamos volver a s sin haber modificado nada.
// =============================================================================
ResultadoAlgoritmo ejecutar_alns(const EstadoLRP &sol_inicial,
                                 const std::vector<OperadorFn> &destroyers,
                                 const std::vector<OperadorFn> &repairers,
                                 const ConfigALNS &cfg) {
  if (destroyers.empty() || repairers.empty())
    throw std::invalid_argument("Se necesita al menos 1 destroy y 1 repair");

  std::mt19937 rng(cfg.semilla);

  // Una ruleta independiente para destructores y otra para reparadores
  RouletteWheel ruleta_d((int)destroyers.size());
  RouletteWheel ruleta_r((int)repairers.size());
  SimulatedAnnealing sa(cfg.temperatura_inicial, cfg.temperatura_final,
                        cfg.factor_enfriamiento);

  // s = solución actual (la que guía la búsqueda)
  // s_best = mejor solución vista en toda la corrida (lo que devolvemos al final)
  EstadoLRP s = sol_inicial.copy();
  EstadoLRP s_best = s.copy();
  double f_s = s.objective();
  double f_best = f_s;

  // Inicializar el resultado con el estado inicial
  ResultadoAlgoritmo resultado{s_best, f_s, f_best, 0, {}};
  resultado.historial_mejoras.push_back({0, f_best}); // punto de partida

  for (int iter = 1; iter <= cfg.max_iteraciones; ++iter) {
    // ── Paso 1: elegir operadores ────────────────────────────────────────────
    int id_d = ruleta_d.seleccionar(rng);
    int id_r = ruleta_r.seleccionar(rng);

    // ── Paso 2: destruir y reparar ───────────────────────────────────────────
    // s.copy() porque los operadores trabajan por valor sobre una copia.
    // std::move en el paso de reparación para evitar una copia extra.
    EstadoLRP s_nuevo = destroyers[id_d](s.copy(), rng);
    s_nuevo = repairers[id_r](std::move(s_nuevo), rng);
    double f_nuevo = s_nuevo.objective();

    // ── Paso 3: clasificar el resultado y actualizar s/s_best ────────────────
    double sigma = cfg.sigma4; // por defecto: rechazado

    if (f_nuevo < f_best) {
      // Caso σ1: nuevo mejor global
      s_best = s_nuevo.copy();
      f_best = f_nuevo;
      s = std::move(s_nuevo);
      f_s = f_best;
      sigma = cfg.sigma1;
      resultado.historial_mejoras.push_back({iter, f_best});
    } else if (f_nuevo < f_s) {
      // Caso σ2: mejora la solución actual pero no el global
      s = std::move(s_nuevo);
      f_s = f_nuevo;
      sigma = cfg.sigma2;
    } else if (sa.aceptar(f_nuevo, f_s, rng)) {
      // Caso σ3: aceptado por SA aunque sea peor (movimiento de exploración)
      s = std::move(s_nuevo);
      f_s = f_nuevo;
      sigma = cfg.sigma3;
    }
    // Caso σ4: rechazado — s no cambia (sigma ya vale cfg.sigma4)

    // ── Paso 4: aprender de este resultado ───────────────────────────────────
    ruleta_d.registrar(id_d, sigma);
    ruleta_r.registrar(id_r, sigma);

    // ── Paso 5: enfriar temperatura ──────────────────────────────────────────
    sa.enfriar();

    // ── Paso 6: actualizar pesos de la ruleta cada `segmento` iteraciones ────
    if (iter % cfg.segmento == 0) {
      ruleta_d.actualizar_pesos(cfg.factor_reaccion);
      ruleta_r.actualizar_pesos(cfg.factor_reaccion);
    }
  }

  // Empaquetar el resultado final
  resultado.mejor_estado = std::move(s_best);
  resultado.costo_final = f_best;
  resultado.iteraciones = cfg.max_iteraciones;
  return resultado;
}
