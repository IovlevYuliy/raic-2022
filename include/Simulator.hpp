#ifndef _SIMULATOR_HPP_
#define _SIMULATOR_HPP_

#include "model/Constants.hpp"

class Simulator {
public:
    Simulator(const model::Constants& constants);

    model::Constants constants;
};

#endif