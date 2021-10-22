#pragma once
#include "annotation.h"
#include "response.h"

namespace marian::bergamot {

std::vector<Alignment> remapAlignments(const Response &first, const Response &second);

}
