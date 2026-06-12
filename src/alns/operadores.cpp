// =============================================================================
// operadores.cpp — Implementación de todos los operadores del ALNS
// =============================================================================
//
// Este archivo implementa 5 operadores de DESTRUCCIÓN y 2 de REPARACIÓN.
//
// La idea del ALNS es destruir una parte de la solución y luego reconstruirla
// de una forma diferente. Si la nueva forma es mejor, la adoptamos; si no,
// quizás igual la aceptamos (según Simulated Annealing). Repetir miles de
// veces buscando la mejor solución posible.
//
// Primero vienen funciones auxiliares que todos los operadores comparten,
// luego los destructores, luego los reparadores.
//
// =============================================================================
#include "operadores.h"
#include "../solucion_inicial.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

// =============================================================================
// FUNCIONES AUXILIARES COMPARTIDAS
// =============================================================================

// -----------------------------------------------------------------------------
// Devuelve la lista de todos los clientes que actualmente están en alguna ruta.
// Se usa para saber de quiénes podemos "robar" en los destructores.
// -----------------------------------------------------------------------------
static std::vector<int> clientes_ruteados(const EstadoLRP &e) {
  std::vector<int> v;
  for (const auto &[dep, lista] : e.rutas)
    for (const auto &r : lista)
      for (int c : r.clientes)
        v.push_back(c);
  return v;
}

// -----------------------------------------------------------------------------
// Busca al cliente en todas las rutas, lo elimina de allí y lo mueve a la
// lista de no_asignados (los que están esperando ser recolocados).
// Devuelve apenas lo encuentra — cada cliente está en exactamente una ruta.
// -----------------------------------------------------------------------------
static void remover_cliente(EstadoLRP &e, int cliente) {
  for (auto &[dep, lista] : e.rutas) {
    for (auto &r : lista) {
      auto it = std::find(r.clientes.begin(), r.clientes.end(), cliente);
      if (it != r.clientes.end()) {
        r.clientes.erase(it);
        e.no_asignados.push_back(cliente);
        return;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Después de quitar clientes, algunas rutas pueden quedar sin nadie.
// Una ruta vacía no aporta nada y solo confunde. Esta función las elimina.
// (El costo fijo F no se cuenta para rutas vacías en objective(), pero es
// más limpio no tenerlas en la estructura.)
// -----------------------------------------------------------------------------
static void limpiar_rutas_vacias(EstadoLRP &e) {
  for (auto &[dep, lista] : e.rutas)
    lista.erase(std::remove_if(lista.begin(), lista.end(),
                               [](const Ruta &r) { return r.clientes.empty(); }),
                lista.end());
}

// -----------------------------------------------------------------------------
// Truco de sesgo probabilístico: dada una lista ordenada de mayor a menor
// "atractivo", queremos elegir preferentemente los primeros elementos,
// pero con algo de aleatoriedad para no volvernos deterministas.
//
// La fórmula int(u³ × n) convierte un número uniforme u ∈ [0,1) en un índice
// sesgado hacia 0. Como u³ < u (para u < 1), el índice tiende al inicio.
// Esto viene de la literatura de LNS (Shaw 1998, Ropke & Pisinger 2006).
// -----------------------------------------------------------------------------
static int idx_sesgado(double u, int n) {
  return static_cast<int>(std::pow(u, 3.0) * n);
}

// =============================================================================
// OPERADORES DE DESTRUCCIÓN
// =============================================================================

// -----------------------------------------------------------------------------
// DESTRUCTOR 1: Random Removal — quita clientes al azar
//
// El más sencillo. Mezcla todos los clientes ruteados y elimina el primer 20%.
// No tiene criterio — pura aleatoriedad. Esto es útil cuando el algoritmo
// lleva mucho tiempo estancado en el mismo óptimo local y necesita un sacudón.
// -----------------------------------------------------------------------------
EstadoLRP random_removal(EstadoLRP estado, std::mt19937 &rng) {
  auto ruteados = clientes_ruteados(estado);
  if (ruteados.empty()) return estado;

  // Quitar ~20% de los clientes, pero al menos 1
  int q = std::max(1, static_cast<int>(estado.datos->num_clientes * 0.20));
  q = std::min(q, (int)ruteados.size());

  std::shuffle(ruteados.begin(), ruteados.end(), rng);
  for (int i = 0; i < q; ++i)
    remover_cliente(estado, ruteados[i]);

  limpiar_rutas_vacias(estado);
  return estado;
}

// -----------------------------------------------------------------------------
// DESTRUCTOR 2: Worst Removal — quita los clientes más "caros" en su posición
//
// Para cada cliente calcula su "ahorro": cuánto bajaría el costo de viaje si
// lo sacáramos de donde está. Fórmula:
//   ahorro = dist(prev, cliente) + dist(cliente, next) − dist(prev, next)
// (Es el costo marginal de tenerlo ahí — si es grande, está "mal ubicado".)
//
// Ordena de mayor a menor ahorro y quita los peores, con un poco de
// aleatoriedad (idx_sesgado) para no ser completamente determinista.
// Luego elimina las entradas duplicadas en la lista por si el mismo cliente
// apareciera dos veces (no debería, pero por si acaso).
// -----------------------------------------------------------------------------
EstadoLRP worst_removal(EstadoLRP estado, std::mt19937 &rng) {
  int q = std::max(1, static_cast<int>(estado.datos->num_clientes * 0.20));
  const Matriz &mat = *estado.matriz;

  // Calcular el ahorro de quitar cada cliente de su posición actual
  std::vector<std::pair<double, int>> ahorros;
  for (const auto &[dep, lista] : estado.rutas) {
    int idx_dep = dep - 1;
    for (const auto &ruta : lista) {
      for (int i = 0; i < (int)ruta.clientes.size(); ++i) {
        int prev = (i == 0) ? idx_dep : ruta.clientes[i - 1] - 1;
        int curr = ruta.clientes[i] - 1;
        int next = (i == (int)ruta.clientes.size() - 1) ? idx_dep : ruta.clientes[i + 1] - 1;
        double ahorro = mat[prev][curr] + mat[curr][next] - mat[prev][next];
        ahorros.push_back({ahorro, ruta.clientes[i]});
      }
    }
  }
  if (ahorros.empty()) return estado;

  // Ordenar: los que más "ahorraríamos" al sacarlos van primero
  std::sort(ahorros.begin(), ahorros.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  // Elegir víctimas sesgando hacia el inicio (mayor ahorro) pero con ruido
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  int removidos = 0;
  while (removidos < q && !ahorros.empty()) {
    int idx = idx_sesgado(dist(rng), (int)ahorros.size());
    int victima = ahorros[idx].second;
    // Eliminar la entrada elegida y cualquier duplicado del mismo cliente
    ahorros.erase(ahorros.begin() + idx);
    ahorros.erase(std::remove_if(ahorros.begin(), ahorros.end(),
                                 [victima](const auto &p) { return p.second == victima; }),
                  ahorros.end());
    remover_cliente(estado, victima);
    ++removidos;
  }

  limpiar_rutas_vacias(estado);
  return estado;
}

// -----------------------------------------------------------------------------
// DESTRUCTOR 3: Shaw Removal — quita un cluster de clientes cercanos entre sí
//
// Propuesto por Paul Shaw (1998). La intuición: si sacamos clientes que están
// geográficamente cerca, es probable que al reparar podamos rearmarlos en rutas
// más eficientes (aprovechando que ya están juntos).
//
// Algoritmo:
//   1. Elegir un cliente inicial al azar.
//   2. Ordenar los restantes por distancia al nodo base (un cliente ya elegido).
//   3. Elegir el siguiente sesgando hacia los más cercanos (idx_sesgado).
//   4. Repetir hasta tener ~20% de los clientes.
//
// En cada iteración se elige un nuevo "nodo base" al azar de los ya
// seleccionados, para que el cluster crezca de forma natural y no en línea recta.
// -----------------------------------------------------------------------------
EstadoLRP shaw_removal(EstadoLRP estado, std::mt19937 &rng) {
  auto ruteados = clientes_ruteados(estado);
  if (ruteados.empty()) return estado;

  int q = std::max(1, static_cast<int>(estado.datos->num_clientes * 0.20));
  const Matriz &mat = *estado.matriz;

  // Paso 1: elegir el primer cliente al azar como semilla del cluster
  std::uniform_int_distribution<int> pick(0, (int)ruteados.size() - 1);
  int nodo_inicial = ruteados[pick(rng)];
  std::vector<int> victimas = {nodo_inicial};
  ruteados.erase(std::find(ruteados.begin(), ruteados.end(), nodo_inicial));

  // Pasos 2-4: crecer el cluster eligiendo vecinos cercanos
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  while ((int)victimas.size() < q && !ruteados.empty()) {
    // Elegir un nodo base de los ya seleccionados
    std::uniform_int_distribution<int> base_pick(0, (int)victimas.size() - 1);
    int nodo_base = victimas[base_pick(rng)];

    // Ordenar los candidatos por distancia al nodo base
    std::sort(ruteados.begin(), ruteados.end(), [&](int a, int b) {
      return mat[nodo_base - 1][a - 1] < mat[nodo_base - 1][b - 1];
    });

    // Elegir sesgando hacia los más cercanos (principio del cluster)
    int idx = idx_sesgado(dist(rng), (int)ruteados.size());
    victimas.push_back(ruteados[idx]);
    ruteados.erase(ruteados.begin() + idx);
  }

  for (int v : victimas)
    remover_cliente(estado, v);
  limpiar_rutas_vacias(estado);
  return estado;
}

// -----------------------------------------------------------------------------
// DESTRUCTOR 4: Route Removal — elimina rutas completas
//
// Los otros destructores quitan clientes individuales. Este quita rutas enteras
// (con todos sus clientes). La presión que genera es diferente: obliga al
// algoritmo a explorar soluciones con MENOS vehículos usados, ahorrando
// el costo fijo F por cada ruta eliminada.
//
// Cuántas rutas destruir: entre el 10% y el 30% de las rutas activas,
// elegido al azar. Al menos 1, al menos 2 si hay suficientes.
// -----------------------------------------------------------------------------
EstadoLRP route_removal(EstadoLRP estado, std::mt19937 &rng) {
  // Recopilar pares (dep_id, índice_ruta) de todas las rutas con clientes
  std::vector<std::pair<int, int>> activas;
  for (const auto &[dep, lista] : estado.rutas)
    for (int i = 0; i < (int)lista.size(); ++i)
      if (!lista[i].clientes.empty())
        activas.push_back({dep, i});

  if (activas.empty()) return estado;

  // Decidir cuántas rutas destruir (10%–30% de las activas)
  int min_r = std::max(1, (int)(activas.size() * 0.10));
  int max_r = std::max(2, (int)(activas.size() * 0.30));
  std::uniform_int_distribution<int> pick(min_r, max_r);
  int n_destruir = std::min(pick(rng), (int)activas.size());

  // Mezclar y tomar las primeras n_destruir
  std::shuffle(activas.begin(), activas.end(), rng);
  for (int i = 0; i < n_destruir; ++i) {
    auto &[dep, ri] = activas[i];
    // Mover todos los clientes de esta ruta a no_asignados
    for (int c : estado.rutas[dep][ri].clientes)
      estado.no_asignados.push_back(c);
    estado.rutas[dep][ri].clientes.clear();
  }

  limpiar_rutas_vacias(estado);
  return estado;
}

// -----------------------------------------------------------------------------
// DESTRUCTOR 5: Depot Closing — cierra un depósito entero
//
// El más agresivo. No solo quita clientes: cierra un depósito y elimina todas
// sus rutas, liberando el costo de apertura O_i. Esto fuerza a explorar
// configuraciones con MENOS depósitos abiertos.
//
// El reparador que venga después tendrá que reasignar todos esos clientes a
// otros depósitos (o abrir uno diferente si es necesario).
//
// Condición de seguridad: no cerramos si solo hay 1 depósito abierto —
// la solución quedaría sin depósitos y sería imposible de reparar.
// -----------------------------------------------------------------------------
EstadoLRP depot_closing(EstadoLRP estado, std::mt19937 &rng) {
  // Necesitamos al menos 2 depósitos abiertos para cerrar uno
  if ((int)estado.depositos_abiertos.size() <= 1)
    return estado;

  // Elegir uno al azar
  std::uniform_int_distribution<int> pick(0, (int)estado.depositos_abiertos.size() - 1);
  int idx = pick(rng);
  int dep_cerrar = estado.depositos_abiertos[idx];

  // Mover todos los clientes de ese depósito a no_asignados
  for (const auto &r : estado.rutas[dep_cerrar])
    for (int c : r.clientes)
      estado.no_asignados.push_back(c);

  // Eliminar las rutas y el registro del depósito
  estado.rutas.erase(dep_cerrar);
  estado.depositos_abiertos.erase(estado.depositos_abiertos.begin() + idx);
  return estado;
}

// =============================================================================
// FUNCIONES AUXILIARES DE LOS REPARADORES
// =============================================================================

// -----------------------------------------------------------------------------
// Encuentra el lugar más barato para insertar un cliente en la solución actual.
//
// Evalúa todas las posiciones posibles:
//   - Cada hueco en cada ruta de cada depósito abierto (si cabe por capacidad Q)
//   - Abrir una ruta nueva en cualquier depósito (costo = F + ida + vuelta)
//
// Devuelve una tupla (costo_marginal, dep_id, idx_ruta, posición).
// Si idx_ruta == -1, significa "abrir una ruta nueva en ese depósito".
// Si dep_id == -1, no se encontró ninguna posición válida (caso extremo).
// -----------------------------------------------------------------------------
static std::tuple<double, int, int, int> mejor_insercion(int cliente,
                                                         const EstadoLRP &e) {
  const Matriz &mat = *e.matriz;
  const double Q = e.datos->Q;
  const double F = e.datos->F;
  int nc = e.datos->num_clientes;
  int idx_c = cliente - 1;
  double dem_c = e.datos->clientes[idx_c].demanda;

  // Precalcular la carga actual de cada depósito abierto (R4: sum d_j ≤ W_i)
  std::unordered_map<int, double> carga_dep;
  for (int dep_id : e.depositos_abiertos) {
    double carga = 0.0;
    for (const auto &r : e.rutas.at(dep_id))
      for (int c : r.clientes)
        carga += e.datos->clientes[c - 1].demanda;
    carga_dep[dep_id] = carga;
  }

  double mejor = std::numeric_limits<double>::infinity();
  int m_dep = -1, m_ri = -1, m_pos = -1;
  // Mejor opción ignorando capacidad de depósito (fallback si todo está lleno)
  double mejor_fb = std::numeric_limits<double>::infinity();
  int fb_dep = -1, fb_ri = -1, fb_pos = -1;

  for (int dep_id : e.depositos_abiertos) {
    int idx_dep = dep_id - 1;
    int dep_idx = dep_id - nc - 1;
    double cap_dep = e.datos->depositos[dep_idx].capacidad;
    bool dep_tiene_espacio = (carga_dep[dep_id] + dem_c <= cap_dep + 1e-9);
    const auto &lista = e.rutas.at(dep_id);

    // Opción A: insertar en una ruta existente de este depósito
    for (int ri = 0; ri < (int)lista.size(); ++ri) {
      const auto &ruta = lista[ri];
      if (demanda_ruta_externa(ruta, *e.datos) + dem_c > Q)
        continue; // no cabe en el vehículo — saltar esta ruta
      for (int pos = 0; pos <= (int)ruta.clientes.size(); ++pos) {
        int prev = (pos == 0) ? idx_dep : ruta.clientes[pos - 1] - 1;
        int next = (pos == (int)ruta.clientes.size()) ? idx_dep : ruta.clientes[pos] - 1;
        double c = mat[prev][idx_c] + mat[idx_c][next] - mat[prev][next];
        if (dep_tiene_espacio && c < mejor)
          { mejor = c; m_dep = dep_id; m_ri = ri; m_pos = pos; }
        if (c < mejor_fb)
          { mejor_fb = c; fb_dep = dep_id; fb_ri = ri; fb_pos = pos; }
      }
    }

    // Opción B: abrir una ruta nueva solo para este cliente en este depósito
    double c_nueva = mat[idx_dep][idx_c] * 2.0 + F;
    if (dep_tiene_espacio && c_nueva < mejor)
      { mejor = c_nueva; m_dep = dep_id; m_ri = -1; m_pos = 0; }
    if (c_nueva < mejor_fb)
      { mejor_fb = c_nueva; fb_dep = dep_id; fb_ri = -1; fb_pos = 0; }
  }

  // Si ningún depósito tiene espacio, usar el fallback (viola R4 pero evita
  // dejar el cliente sin asignar, lo que penaliza aún más).
  if (m_dep == -1) { m_dep = fb_dep; m_ri = fb_ri; m_pos = fb_pos; }

  return {mejor, m_dep, m_ri, m_pos};
}

// -----------------------------------------------------------------------------
// Aplica la inserción decidida por mejor_insercion().
// Si ri == -1, crea una ruta nueva; si no, inserta en la posición dada.
// Después borra al cliente de la lista no_asignados.
// -----------------------------------------------------------------------------
static void aplicar_insercion(EstadoLRP &e, int cliente, int dep_id, int ri, int pos) {
  if (ri == -1) {
    e.rutas[dep_id].push_back(Ruta(dep_id, {cliente}));
  } else {
    auto &ruta = e.rutas[dep_id][ri];
    ruta.clientes.insert(ruta.clientes.begin() + pos, cliente);
  }
  e.no_asignados.erase(
      std::find(e.no_asignados.begin(), e.no_asignados.end(), cliente));
}

// =============================================================================
// OPERADORES DE REPARACIÓN
// =============================================================================

// -----------------------------------------------------------------------------
// REPARADOR 1: Greedy Repair — inserción codiciosa
//
// El más rápido. Para cada cliente pendiente (en orden aleatorio), busca la
// inserción de menor costo marginal y la aplica inmediatamente, sin mirar
// el futuro.
//
// Ventaja: muy rápido — O(n × posiciones).
// Desventaja: puede tomar decisiones localmente buenas pero globalmente malas.
//   Ejemplo: insertar al cliente A barato ahora puede bloquear una inserción
//   mucho más barata del cliente B que vendría después.
//
// El shuffle inicial evita que el orden de procesamiento sea siempre el mismo,
// lo que daría soluciones idénticas en cada llamada.
// -----------------------------------------------------------------------------
EstadoLRP greedy_repair(EstadoLRP estado, std::mt19937 &rng) {
  std::shuffle(estado.no_asignados.begin(), estado.no_asignados.end(), rng);
  while (!estado.no_asignados.empty()) {
    int cliente = estado.no_asignados[0];
    auto [costo, dep, ri, pos] = mejor_insercion(cliente, estado);
    if (dep == -1) break; // no encontró posición — caso de emergencia, salir
    aplicar_insercion(estado, cliente, dep, ri, pos);
  }
  return estado;
}

// -----------------------------------------------------------------------------
// REPARADOR 2: Regret-2 Repair — inserción por arrepentimiento
//
// Más sofisticado que greedy. En lugar de insertar siempre el primero,
// pregunta: "¿qué tan arrepentido estaría si NO inserto a este cliente ahora
// en su mejor posición?" (Ropke & Pisinger 2006).
//
// El "arrepentimiento" de un cliente = diferencia entre su 1ra y 2da mejor opción.
//   regret(cliente) = costo_2da_mejor − costo_1ra_mejor
//
// Un regret alto significa: "si no lo pongo ahora en su mejor lugar, el
// siguiente mejor lugar es mucho más caro". Esos clientes difíciles se
// insertan primero para no dejarlos sin buena opción.
//
// Proceso por iteración:
//   1. Calcular el regret de cada cliente pendiente.
//   2. Insertar el que tenga el mayor regret (el más urgente).
//   3. Repetir hasta vaciar no_asignados.
//
// Esto es más lento (O(n²) por iteración) pero genera soluciones de mejor
// calidad que greedy_repair, especialmente cuando hay muchos clientes difíciles.
//
// Nota: (void)rng — este operador es determinista; no necesita aleatoriedad.
// Se recibe el rng por consistencia de interfaz con los demás operadores.
// -----------------------------------------------------------------------------
EstadoLRP regret2_repair(EstadoLRP estado, std::mt19937 &rng) {
  (void)rng; // determinista — no usa aleatoriedad

  while (!estado.no_asignados.empty()) {
    std::vector<std::pair<double, int>> regrets;

    // Para cada cliente pendiente, calcular todos sus costos de inserción posibles
    for (int cliente : estado.no_asignados) {
      const Matriz &mat = *estado.matriz;
      const double Q = estado.datos->Q;
      const double F = estado.datos->F;
      int idx_c = cliente - 1;
      double dem_c = estado.datos->clientes[idx_c].demanda;

      std::vector<double> costos;
      for (int dep_id : estado.depositos_abiertos) {
        int idx_dep = dep_id - 1;
        const auto &lista = estado.rutas.at(dep_id);
        for (int ri = 0; ri < (int)lista.size(); ++ri) {
          const auto &ruta = lista[ri];
          if (demanda_ruta_externa(ruta, *estado.datos) + dem_c > Q)
            continue;
          for (int pos = 0; pos <= (int)ruta.clientes.size(); ++pos) {
            int prev = (pos == 0) ? idx_dep : ruta.clientes[pos - 1] - 1;
            int next = (pos == (int)ruta.clientes.size()) ? idx_dep : ruta.clientes[pos] - 1;
            costos.push_back(mat[prev][idx_c] + mat[idx_c][next] - mat[prev][next]);
          }
        }
        costos.push_back(mat[idx_dep][idx_c] * 2.0 + F);
      }

      // regret = diferencia entre la 1ra y 2da opción (cuánto se perdería)
      // Si solo hay una opción, el regret es infinito — insertar ya o nunca.
      std::sort(costos.begin(), costos.end());
      double regret = (costos.size() >= 2)
                          ? costos[1] - costos[0]
                          : std::numeric_limits<double>::infinity();
      regrets.push_back({regret, cliente});
    }

    // Insertar el cliente con el mayor regret (el más "urgente")
    auto it = std::max_element(regrets.begin(), regrets.end(),
                               [](const auto &a, const auto &b) { return a.first < b.first; });
    int cliente = it->second;
    auto [costo, dep, ri, pos] = mejor_insercion(cliente, estado);
    if (dep == -1) break;
    aplicar_insercion(estado, cliente, dep, ri, pos);
  }
  return estado;
}
