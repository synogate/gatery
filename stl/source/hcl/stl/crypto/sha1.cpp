#include "sha1.h"

namespace hcl::stl
{

    template class Sha1Config<hcl::BVec>;
    template class ShaState<hcl::BVec>;

}