// alns.h
struct ConfigALNS {
  double temperatura_inicial; // T0
  double temperatura_final;   // T_end
  double factor_enfriamiento; // alpha
  double factor_reaccion;     // rho
  int    segmento;            // T_seg
  double sigma1; // nuevo optimo global
  double sigma2; // mejora sol. actual
  double sigma3; // aceptada por SA
  double sigma4; // rechazada
  int max_iteraciones;
  unsigned int semilla;
};
using OperadorFn =
  function<EstadoLRP(EstadoLRP, mt19937&)>;
