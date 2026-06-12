cmake_minimum_required(VERSION 3.16)
project(alns_lrp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-O2 -Wall -Wextra)

# ── Librería base (dominio del problema) ──────────────────────
add_library(lrp_core
    src/parser.cpp
    src/distancia.cpp
    src/estado.cpp
    src/solucion_inicial.cpp
    src/export_json.cpp
    src/validador.cpp
)
target_include_directories(lrp_core PUBLIC ${CMAKE_SOURCE_DIR})

# ── Librería ALNS ─────────────────────────────────────────────
add_library(alns_lib
    src/alns/operadores.cpp
    src/alns/alns.cpp
    src/alns/solver.cpp
)
target_link_libraries(alns_lib PUBLIC lrp_core)

# ── Librería MA|PM ────────────────────────────────────────────
add_library(mapm_lib
    src/mapm/cromosoma.cpp
    src/mapm/split.cpp
    src/mapm/poblacion.cpp
    src/mapm/distancia_sol.cpp
    src/mapm/operadores_ga.cpp
    src/mapm/local_search.cpp
    src/mapm/mapm.cpp          # ← agregado
)
target_link_libraries(mapm_lib PUBLIC lrp_core)

# ── Ejecutables principales ───────────────────────────────────
add_executable(alns_lrp  main.cpp)
target_link_libraries(alns_lrp  PRIVATE alns_lib)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE alns_lib mapm_lib)

# ── Tests ALNS ────────────────────────────────────────────────
add_executable(test_parser           tests/test_parser.cpp)
target_link_libraries(test_parser           PRIVATE lrp_core)

add_executable(test_estado           tests/test_estado.cpp)
target_link_libraries(test_estado           PRIVATE lrp_core)

add_executable(test_solucion_inicial tests/test_solucion_inicial.cpp)
target_link_libraries(test_solucion_inicial PRIVATE lrp_core)

add_executable(test_operadores       tests/test_operadores.cpp)
target_link_libraries(test_operadores       PRIVATE alns_lib)

add_executable(test_alns             tests/test_alns.cpp)
target_link_libraries(test_alns             PRIVATE alns_lib)

# ── Librería Fuerza Bruta ─────────────────────────────────────
add_library(brute_force_lib
    src/brute_force/solver.cpp
)
target_link_libraries(brute_force_lib PUBLIC lrp_core)

# ── Ejecutable de validación ──────────────────────────────────
add_executable(validate validate.cpp)
target_link_libraries(validate PRIVATE brute_force_lib alns_lib mapm_lib)

# ── Test validador ────────────────────────────────────────────
add_executable(test_validador        tests/test_validador.cpp)
target_link_libraries(test_validador        PRIVATE alns_lib mapm_lib)

# ── Tests MA|PM ───────────────────────────────────────────────
add_executable(test_split            tests/test_split.cpp)
target_link_libraries(test_split            PRIVATE mapm_lib)

add_executable(test_operadores_ga    tests/test_operadores_ga.cpp)
target_link_libraries(test_operadores_ga    PRIVATE mapm_lib)

add_executable(test_local_search     tests/test_local_search.cpp)
target_link_libraries(test_local_search     PRIVATE mapm_lib)

add_executable(test_distancia        tests/test_distancia.cpp)
target_link_libraries(test_distancia        PRIVATE mapm_lib)

add_executable(test_mapm             tests/test_mapm.cpp)
target_link_libraries(test_mapm             PRIVATE mapm_lib)
