// AlgoritmALNS::ejecutar()
AlgoritmALNS(ConfigALNS cfg,
  vector<OperadorFn> destroyers,
  vector<OperadorFn> repairers);

// En benchmark:
destroyers = {random_removal,
  worst_removal, shaw_removal,
  route_removal, depot_closing};
repairers  = {greedy_repair,
              regret2_repair};
