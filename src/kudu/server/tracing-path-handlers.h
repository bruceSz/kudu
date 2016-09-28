// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
#ifndef KUDU_SERVER_TRACING_PATH_HANDLERS_H
#define KUDU_SERVER_TRACING_PATH_HANDLERS_H

#include "kudu/gutil/macros.h"
#include "kudu/server/webserver.h"
#include "kudu/util/status.h"

namespace kudu {
namespace server {

// Web handlers for Chromium tracing.
// These handlers provide AJAX endpoints for /tracing.html provided by
// the trace-viewer package.
class TracingPathHandlers {
 public:
  static void RegisterHandlers(Webserver* server);

  DISALLOW_IMPLICIT_CONSTRUCTORS(TracingPathHandlers);
};

} // namespace server
} // namespace kudu
#endif /* KUDU_SERVER_TRACING_PATH_HANDLERS_H */
