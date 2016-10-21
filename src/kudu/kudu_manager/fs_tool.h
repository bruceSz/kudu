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
//
// Shared fields and methods for querying local files and directories
#ifndef KUDU_TOOLS_FS_TOOL_H
#define KUDU_TOOLS_FS_TOOL_H

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "kudu/gutil/gscoped_ptr.h"
#include "kudu/util/status.h"
#include "kudu/tablet/delta_key.h"

namespace kudu {

class FsManager;
class RandomAccessFile;

namespace tablet {
class TabletMetadata;

}

namespace kudu-manager {


class FsTool {
 public:

  enum DetailLevel {
    MINIMUM = 0, // Minimum amount of information
    HEADERS_ONLY = 1, // Tablet/segment headers only
    MAXIMUM = 2,
  };

  explicit FsTool(DetailLevel detail_level);
  ~FsTool();

  Status Init();

  // Lists all tablets in a tablet server's local file system.
  Status ListAllTablets();

  // Prints the tablet metadata for a tablet 'tablet_id'.
  Status PrintTabletMeta(const std::string& tablet_id, int indent);


  // Prints the server's UUID to whom the data belongs and nothing else.
  Status PrintUUID(int indent);
 private:
  bool initialized_;
  const DetailLevel detail_level_;
  gscoped_ptr<FsManager> fs_manager_;
};

} // namespace kudu-manager
} // namespace kudu

#endif // KUDU_TOOLS_FS_TOOL_H