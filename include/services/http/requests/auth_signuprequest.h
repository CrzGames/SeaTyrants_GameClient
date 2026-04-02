#pragma once

#include "services/http/types/auth/requests.h"
#include "services/http/types/auth/responses.h"

AuthSignUpHTTPResponse ClientHttp_Auth_SignUpRequest(const AuthSignUpHTTPRequest& request);
