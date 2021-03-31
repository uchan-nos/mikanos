#pragma once

#ifdef NULL
#undef NULL
#endif

#include <Uefi.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#undef NULL
#include <cstddef>

#define EFIAPI __attribute__((ms_abi))

inline EFI_RUNTIME_SERVICES* uefi_rt;
