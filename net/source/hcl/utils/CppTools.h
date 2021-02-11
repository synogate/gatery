#pragma once

namespace hcl::utils {

template <typename T>
class RestrictTo { friend T; RestrictTo() {} };

}