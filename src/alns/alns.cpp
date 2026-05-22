#include "alns.h"
#include <cmath>
#include <numeric>
#include <stdexcept>

// ═════════════════════════════════════════
// RouletteWheel — selección adaptativa
// ═════════════════════════════════════════
class RouletteWheel {
public:
  explicit RouletteWheel(int n_operadores)
      : n(n_operadores),
        pesos(n_operadores, 1.0) // todos arrancan con peso igual
        ,
        scores(n_operadores, 0.0), usos(n_operadores, 0) {}

  // Selecciona un operador según pesos proporcionales
  int seleccionar(std::mt19937 &rng) const {
    double total = std::accumulate(pesos.begin(), pesos.end(), 0.0);
    std::uniform_real_distribution<double> dist(0.0, total);
    double r = dist(rng);
    double acum = 0.0;
    for (int i = 0; i < n; ++i) {
      acum += pesos[i];
      if (r <= acum)
        return i;
    }
    return n - 1; // fallback por precisión flotante
  }

  // Registra el puntaje obtenido por el operador i en esta iteración
  void registrar(int i, double sigma) {
    scores[i] += sigma;
    usos[i] += 1;
  }

  // Actualiza pesos al final de cada segmento
  // w_i = w_i*(1-ρ) + ρ*(score_i/usos_i)   si usos_i > 0
  void actualizar_pesos(double rho) {
    for (int i = 0; i < n; ++i) {
      if (usos[i] > 0) {
        double rendimiento = scores[i] / usos[i];
        pesos[i] = pesos[i] * (1.0 - rho) + rho * rendimiento;
      }
      // Resetear acumuladores para el siguiente segmento
      scores[i] = 0.0;
      usos[i] = 0;
    }
  }

  const std::vector<double> &get_pesos() const { return pesos; }

private:
  int n;
  std::vector<double> pesos;
  std::vector<double> scores;
  std::vector<int> usos;
};

// ═════════════════════════════════════════
// SimulatedAnnealing — criterio de aceptación
// ═════════════════════════════════════════
class SimulatedAnnealing {
public:
  SimulatedAnnealing(double T0, double T_end, double alpha)
      : T(T0), T_end(T_end), alpha(alpha) {
    if (alpha <= 0.0 || alpha >= 1.0)
      throw std::invalid_argument("factor_enfriamiento debe estar en (0,1)");
    if (T0 <= T_end)
      throw std::invalid_argument(
          "temperatura_inicial debe ser > temperatura_final");
  }

  // Decide si aceptar una solución peor
  bool aceptar(double costo_nuevo, double costo_actual, std::mt19937 &rng) {
    if (costo_nuevo <= costo_actual)
      return true;
    double prob = std::exp(-(costo_nuevo - costo_actual) / T);
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng) < prob;
  }

  // Enfriamiento geométrico — llamar una vez por iteración
  void enfriar() { T = std::max(T * alpha, T_end); }

  double temperatura() const { return T; }

private:
  double T;
  double T_end;
  double alpha;
};

// ═════════════════════════════════════════
// ejecutar_alns — loop principal
// ═════════════════════════════════════════
ResultadoAlgoritmo ejecutar_alns(const EstadoLRP &sol_inicial,
                                 const std::vector<OperadorFn> &destroyers,
                                 const std::vector<OperadorFn> &repairers,
                                 const ConfigALNS &cfg) {
  if (destroyers.empty() || repairers.empty())
    throw std::invalid_argument("Se necesita al menos 1 destroy y 1 repair");

  std::mt19937 rng(cfg.semilla);

  RouletteWheel ruleta_d((int)destroyers.size());
  RouletteWheel ruleta_r((int)repairers.size());
  SimulatedAnnealing sa(cfg.temperatura_inicial, cfg.temperatura_final,
                        cfg.factor_enfriamiento);

  EstadoLRP s = sol_inicial.copy();
  EstadoLRP s_best = s.copy();
  double f_s = s.objective();
  double f_best = f_s;
  double f_inicial = f_s;

  ResultadoAlgoritmo resultado{s_best, f_inicial, f_best, 0, {}};
  resultado.historial_mejoras.push_back({0, f_best});

  for (int iter = 1; iter <= cfg.max_iteraciones; ++iter) {

    // ── Seleccionar operadores ────────────────────────
    int id_d = ruleta_d.seleccionar(rng);
    int id_r = ruleta_r.seleccionar(rng);

    // ── Destruir y reparar ────────────────────────────
    EstadoLRP s_nuevo = destroyers[id_d](s.copy(), rng);
    s_nuevo = repairers[id_r](std::move(s_nuevo), rng);
    double f_nuevo = s_nuevo.objective();

    // ── Criterio de aceptación y puntaje ─────────────
    double sigma = cfg.sigma4; // por defecto: rechazado

    if (f_nuevo < f_best) {
      // Nuevo óptimo global
      s_best = s_nuevo.copy();
      f_best = f_nuevo;
      s = std::move(s_nuevo);
      f_s = f_best;
      sigma = cfg.sigma1;
      resultado.historial_mejoras.push_back({iter, f_best});

    } else if (f_nuevo < f_s) {
      // Mejora la solución actual
      s = std::move(s_nuevo);
      f_s = f_nuevo;
      sigma = cfg.sigma2;

    } else if (sa.aceptar(f_nuevo, f_s, rng)) {
      // Aceptada por SA (diversificación)
      s = std::move(s_nuevo);
      f_s = f_nuevo;
      sigma = cfg.sigma3;
    }
    // else: rechazada — s y f_s no cambian, sigma = sigma4

    // ── Registrar puntaje ─────────────────────────────
    ruleta_d.registrar(id_d, sigma);
    ruleta_r.registrar(id_r, sigma);

    // ── Enfriamiento ──────────────────────────────────
    sa.enfriar();

    // ── Actualizar pesos cada segmento ────────────────
    if (iter % cfg.segmento == 0) {
      ruleta_d.actualizar_pesos(cfg.factor_reaccion);
      ruleta_r.actualizar_pesos(cfg.factor_reaccion);
    }
  }

  resultado.mejor_estado = std::move(s_best);
  resultado.costo_final = f_best;
  resultado.iteraciones = cfg.max_iteraciones;
  return resultado;
}
