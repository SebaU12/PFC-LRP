// =============================================================================
// estado.h — Representación de una solución LRP en memoria
// =============================================================================
//
// Si parser.h define el PROBLEMA, este archivo define una SOLUCIÓN a ese problema.
//
// Una solución LRP responde tres preguntas:
//   1. ¿Qué depósitos abrir?       → depositos_abiertos
//   2. ¿Qué rutas hacer desde cada depósito? → rutas
//   3. ¿Hay clientes que no pudieron asignarse? → no_asignados
//      (en una solución factible esta lista debe estar vacía)
//
// ESTRUCTURA DE UNA RUTA:
//   Una ruta es un camión que sale de un depósito, visita una secuencia de
//   clientes en orden, y regresa al mismo depósito.
//
//   Ejemplo:  Depósito 11 → Cliente 3 → Cliente 7 → Cliente 1 → Depósito 11
//   En código: Ruta{ id_deposito=11, clientes={3, 7, 1}, ... }
//
// POR QUÉ PUNTEROS Y NO COPIAS:
//   EstadoLRP guarda punteros a la Matriz y a la InstanciaLRP en lugar de
//   copias. Esto es intencional: el algoritmo crea miles de copias del estado
//   durante la búsqueda, y copiar los datos del problema cada vez consumiría
//   cientos de MB innecesariamente. Con punteros, todos los estados comparten
//   los mismos datos del problema (8 bytes por puntero en vez de ~90 KB por copia).
//   → Usar .copy() cuando necesites una copia profunda del estado.
//
// =============================================================================
#pragma once
#include "distancia.h"
#include "parser.h"
#include <map>
#include <vector>

// -----------------------------------------------------------------------------
// Ruta: un camión haciendo un recorrido completo desde y hacia un depósito.
//
// El costo y la demanda_acumulada NO se mantienen actualizados automáticamente
// — se calculan bajo demanda en objective() e imprimir_solucion().
// Están aquí para tener acceso conveniente al último valor calculado.
// -----------------------------------------------------------------------------
struct Ruta {
    int id_deposito;              // Depósito del que sale y al que regresa (ID 1-based)
    std::vector<int> clientes;    // Orden de visita: [c1, c2, ..., cp] (IDs 1-based)
    double costo;                 // F + suma de distancias de viaje (se llena al imprimir)
    double demanda_acumulada;     // Suma de d_j de todos los clientes de esta ruta

    Ruta() : id_deposito(0), costo(0.0), demanda_acumulada(0.0) {}

    // Constructor conveniente: Ruta(dep_id, {c1, c2, c3})
    Ruta(int dep_id, std::vector<int> cls)
        : id_deposito(dep_id), clientes(std::move(cls)), costo(0.0), demanda_acumulada(0.0) {}
};

// Agrupación de rutas por depósito:
//   rutas[11] = [ Ruta{3,7,1}, Ruta{5,2} ]   → depósito 11 tiene 2 rutas
//   rutas[12] = [ Ruta{4,6,8} ]               → depósito 12 tiene 1 ruta
using Rutas = std::map<int, std::vector<Ruta>>;

// -----------------------------------------------------------------------------
// EstadoLRP: una solución completa al problema.
//
// Contiene TODO lo necesario para evaluar la calidad de la solución:
//   - qué depósitos se abrieron y qué rutas hacen
//   - acceso a los datos del problema (vía punteros no-owning)
// -----------------------------------------------------------------------------
struct EstadoLRP {
    Rutas            rutas;               // Mapa dep_id → lista de rutas
    std::vector<int> depositos_abiertos;  // IDs de los depósitos que se abrieron
    std::vector<int> no_asignados;        // Clientes sin ruta (vacío en sol. factible)

    // Punteros al problema (no son dueños de la memoria — ver nota arriba)
    const Matriz       *matriz; // Tabla de distancias entre todos los nodos
    const InstanciaLRP *datos;  // Parámetros del problema (Q, F, demandas, costos)

    EstadoLRP(Rutas r, std::vector<int> dep_abiertos, std::vector<int> no_asig,
              const Matriz &mat, const InstanciaLRP &dat)
        : rutas(std::move(r)), depositos_abiertos(std::move(dep_abiertos)),
          no_asignados(std::move(no_asig)), matriz(&mat), datos(&dat) {}

    // Crea una copia independiente del estado (mismos datos del problema, rutas propias).
    // Usar cuando necesites modificar el estado sin afectar al original.
    EstadoLRP copy() const {
        return EstadoLRP(rutas, depositos_abiertos, no_asignados, *matriz, *datos);
    }

    // ── Evaluación ────────────────────────────────────────────────────────────

    // Calcula Z = sum(O_i*y_i) + F*rutas + sum(c_ij*x_ij) + penalizaciones.
    // Las penalizaciones (×100000) entran cuando hay clientes sin asignar o
    // la demanda supera la capacidad — así el algoritmo siempre tiene una señal
    // de cuán "mala" es la solución, aunque sea infactible.
    double objective() const;

    // Suma las demandas de los clientes de una ruta específica.
    double demanda_ruta(const Ruta &r) const;

    // Suma las demandas de todas las rutas que salen de un depósito.
    double demanda_deposito(int dep_id) const;

    // Imprime la solución con detalle de cada ruta: secuencia, carga y costos.
    // También actualiza ruta.costo y ruta.demanda_acumulada en cada ruta.
    void imprimir_solucion();
};
