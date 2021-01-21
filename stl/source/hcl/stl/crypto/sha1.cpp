#include "sha1.h"
#include "../Adder.h"

template struct hcl::stl::Sha1Generator<>;
template struct hcl::stl::Sha0Generator<>;

template class hcl::stl::HashEngine<hcl::stl::Sha1Generator<>>;