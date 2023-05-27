#pragma once

#include <memory>
#include "error.hpp"

std::unique_ptr<::Error> new_success();