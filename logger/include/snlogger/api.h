#pragma once

#include <sncore/api_common.h>

#if defined(SN_LOGGER_STATIC)
    #define SN_LOGGER_API
#elif defined(SN_LOGGER_EXPORT)
    #define SN_LOGGER_API SN_API_HELPER_EXPORT
#else
    #define SN_LOGGER_API SN_API_HELPER_IMPORT
#endif
