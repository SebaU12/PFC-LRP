// Validador de restricciones del LRP.
// Verifica una EstadoLRP contra las restricciones formales del modelo de dos índices
// (Löffler 2023, Cavagnini 2025). Cada restricción referenciada por número de ecuación.
#pragma once
#include "estado.h"
#include <string>
#include <vector>

// ── Resultado por restricción ─────────────────────────────────────────────────

struct ViolacionRuta {
    int num_ruta;         // número de ruta (1-based, para impresión)
    int dep_id;           // depósito al que pertenece
    double demanda;       // carga de la ruta
    double limite;        // Q (capacidad del vehículo)
};

struct ViolacionDeposito {
    int dep_id;
    double demanda_total;
    double capacidad;     // W_i
};

// ── Resultado completo de la validación ──────────────────────────────────────

struct ResultadoValidacion {
    // ── Estado global ─────────────────────────────────────────────────────────
    bool factible;   // true sólo si TODAS las restricciones se satisfacen

    // ── R1 (Ecs. 2 & 3): cada cliente visitado exactamente una vez ────────────
    bool r1_ok;
    std::vector<int> clientes_huerfanos;   // en no_asignados
    std::vector<int> clientes_duplicados;  // aparecen en más de una ruta

    // ── R2 (Ec. 4): cada cliente asignado a exactamente un depósito ──────────
    bool r2_ok;   // implícito de r1 + estructura de rutas por depósito

    // ── R3 (Ec. 5): asignación sólo a depósitos abiertos ─────────────────────
    bool r3_ok;
    std::vector<int> rutas_deposito_cerrado;  // dep_id de rutas cuyo depósito no está abierto

    // ── R4 (Ec. 6): capacidad de depósito ────────────────────────────────────
    bool r4_ok;
    std::vector<ViolacionDeposito> violaciones_deposito;

    // ── R5 (Ec. 11): capacidad de vehículo por ruta ──────────────────────────
    bool r5_ok;
    std::vector<ViolacionRuta> violaciones_vehiculo;

    // ── R6 (coherencia estructural): id_deposito en Ruta == clave del mapa ───
    bool r6_ok;
    int rutas_con_id_inconsistente;

    // ── Desglose del costo (función objetivo, Ec. 1) ─────────────────────────
    double costo_apertura;      // sum O_i * y_i
    double costo_fijo_rutas;    // sum F * (rutas activas)
    double costo_viaje;         // sum c_ij * x_ij
    double costo_total;         // Z = objective()
};

// ── Función principal ─────────────────────────────────────────────────────────

// Valida e imprime el reporte de restricciones del LRP.
// Si imprimir=true muestra el reporte completo en stdout.
// Siempre retorna el ResultadoValidacion con todos los detalles.
ResultadoValidacion validar_solucion(const EstadoLRP &e, bool imprimir = true);
