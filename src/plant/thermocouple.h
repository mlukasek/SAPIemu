#pragma once
// Thermocouples of APS-5 (ČSN 35 6710): thermoelectric voltage against a 0 degC reference.
// Types by the manual: "Fe-Ko" (iron-constantan, DIN 43710 L), "ch-a" (chromel-alumel, K),
// "PtRh30-PtRh6" (B), "PtRh10-Pt" (S), "ch-k" (chromel-copel, GOST L / XK).
#include <string>

enum TcType { TC_FEKO = 0, TC_CHA, TC_B, TC_S, TC_CHK, TC_COUNT };

// mV at `c` degC (reference junction at 0 degC)
double tc_emf_mv(int type, double c);
// degC of a voltage (inverse, by bisection)
double tc_temp_c(int type, double mv);
// "Fe-Ko", "ch-a", "PtRh30-PtRh6", "PtRh10-Pt", "ch-k"; -1 unknown
int tc_parse(const std::string& name);
const char* tc_name(int type);
