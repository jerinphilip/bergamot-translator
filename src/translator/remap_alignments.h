#pragma once
#include "annotation.h"
#include "response.h"

namespace marian::bergamot {

Alignment tranferThroughCharacters(const std::vector<ByteRange> &sQ, const std::vector<ByteRange> &Qt,
                                   const std::vector<ByteRange> &T, const Alignment &QtT);
std::vector<Alignment> remapAlignments(const Response &first, const Response &second);

}  // namespace marian::bergamot
