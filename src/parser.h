// =============================================================================
// parser.h — Estructuras de datos del problema y lector de instancias
// =============================================================================
//
// Este archivo define "de qué está hecho" el problema LRP antes de resolverlo.
// Piénsalo como el molde donde se vacían los datos del archivo .dat.
//
// Hay tres conceptos clave:
//   Cliente  → quien necesita recibir mercancía (tiene ubicación y demanda)
//   Deposito → donde salen los camiones (tiene ubicación, capacidad y costo)
//   InstanciaLRP → el problema completo: todos los clientes, depósitos y
//                  parámetros de los vehículos juntos en un solo lugar
//
// =============================================================================
#pragma once
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// Un cliente en el mapa. Solo necesita saber dónde está y cuánto pide.
// -----------------------------------------------------------------------------
struct Cliente {
    int    id;      // Número del cliente, empieza en 1 (ej: cliente 1, 2, ..., n)
    double x, y;    // Coordenadas en el mapa (en las mismas unidades del .dat)
    double demanda; // Cuántas unidades necesita que le entreguen (d_j en el paper)
};

// -----------------------------------------------------------------------------
// Un depósito candidato. Puede abrirse o no — esa es una de las decisiones del LRP.
// Si se abre, los camiones salen desde aquí. Si no, se ignora.
// -----------------------------------------------------------------------------
struct Deposito {
    int    id;             // Número del depósito: arranca en num_clientes + 1
                           // (ej: si hay 10 clientes, los depósitos son 11, 12, ...)
    double x, y;           // Coordenadas en el mapa
    double capacidad;      // Máximo de demanda que puede atender en total (W_i en el paper)
    double costo_apertura; // Cuánto cuesta "abrir" este depósito (O_i en el paper)
};

// -----------------------------------------------------------------------------
// El problema completo tal como viene en el archivo .dat.
// Todo lo que necesita saber un algoritmo para empezar a resolver.
// -----------------------------------------------------------------------------
struct InstanciaLRP {
    std::string nombre;       // Nombre del archivo (útil para reportes)

    int num_clientes;         // Cuántos clientes hay (n)
    int num_depositos;        // Cuántos depósitos candidatos hay (m)

    std::vector<Cliente>  clientes;   // Lista de clientes (índice 0-based internamente)
    std::vector<Deposito> depositos;  // Lista de depósitos (índice 0-based internamente)

    double Q;           // Capacidad de cada camión — todos los camiones son iguales
    double F;           // Costo fijo por usar un camión (se paga por cada ruta que sale)
    int    flota_maxima;// No se usa activamente en la metaheurística (flota ilimitada)
    bool   es_entero;   // true → las distancias se multiplican por 100 y se truncan a int
                        // false → se usan las distancias euclidianas reales (flotante)
                        // Esto lo define el benchmark para comparar con la literatura
};

// -----------------------------------------------------------------------------
// Lee un archivo .dat en formato Prodhon y devuelve la instancia lista para usar.
// Lanza std::runtime_error si el archivo no existe o tiene formato incorrecto.
// -----------------------------------------------------------------------------
InstanciaLRP leer_instancia(const std::string &ruta);

// Imprime un resumen legible de la instancia en consola (útil para debug).
void mostrar_resumen(const InstanciaLRP &inst);
