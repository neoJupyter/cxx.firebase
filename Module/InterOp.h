#pragma once

#include <cstddef>
#include <cstdint>
#include <kls/Macros.h>

using handle = uintptr_t;

using njp_phttp_response_callback = void (*)(int code, const char *content, void *user);
using njp_phttp_request_callback = void (*)(handle context, const char *data, void *user);

extern "C" {
KLS_EXPORT handle njp_phttp_connect(const char *address, int port);
KLS_EXPORT void njp_phttp_start_request(
        handle server,
        const char *method, const char *resource, const char *data,
        njp_phttp_response_callback callback, void *callback_data
);
KLS_EXPORT void njp_phttp_disconnect(handle client);

KLS_EXPORT handle njp_phttp_start_serve(const char *address, int port);
KLS_EXPORT void njp_phttp_handle_response(
        handle server, const char *method, const char *resource,
        njp_phttp_request_callback callback, void *callback_data
);
KLS_EXPORT void njp_phttp_done_response(handle response, int code, const char *content);
KLS_EXPORT void njp_phttp_stop_serve(handle);
}