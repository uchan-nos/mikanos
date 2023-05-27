#include "error.hpp"
#include <memory>

std::unique_ptr<Error> new_success()
{
    return MAKE_ERROR(Error::kSuccess);
}