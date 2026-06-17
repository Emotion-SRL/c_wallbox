#pragma once

#include <stddef.h>

struct lws;

/*
  parses a textual command frame received from the ocpp-manager and
  dispatches it to the microcontroller via the serial layer.

  for the "status" command, no serial write is done — instead the WS layer
  is asked to fire a writable callback so the regular WRITEABLE handler
  can send a fresh m_status frame back to the server.

  payload is NOT assumed to be NUL-terminated.
  returns NO_ERR on a recognized + successfully handled command, ERR otherwise.
*/
int handle_server_command(struct lws *wsi, const char *payload, size_t len);
