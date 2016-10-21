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

#include "kudu/kudu-manager/fs_tool.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

#include <boost/function.hpp>
#include <gflags/gflags.h>
#include <glog/logging.h>

#include "kudu/cfile/cfile_reader.h"
#include "kudu/common/rowblock.h"
#include "kudu/common/row_changelist.h"
#include "kudu/consensus/log_util.h"
#include "kudu/consensus/log_reader.h"
#include "kudu/fs/fs_manager.h"
#include "kudu/gutil/strings/human_readable.h"
#include "kudu/gutil/strings/substitute.h"
#include "kudu/gutil/strings/util.h"
#include "kudu/tablet/cfile_set.h"
#include "kudu/tablet/deltafile.h"
#include "kudu/tablet/tablet.h"
#include "kudu/util/env.h"
#include "kudu/util/logging.h"
#include "kudu/util/mem_tracker.h"
#include "kudu/util/memory/arena.h"
#include "kudu/util/status.h"

namespace kudu {
namespace kudu-manager {

using cfile::CFileIterator;
using cfile::CFileReader;
using cfile::DumpIterator;
using cfile::DumpIteratorOptions;
using cfile::ReaderOptions;
using fs::ReadableBlock;
using log::LogReader;
using log::ReadableLogSegment;
using std::shared_ptr;
using std::string;
using std::vector;
using strings::Substitute;
using tablet::CFileSet;
using tablet::DeltaFileReader;
using tablet::DeltaIterator;
using tablet::DeltaKeyAndUpdate;
using tablet::DeltaType;
using tablet::MvccSnapshot;
using tablet::RowSetMetadata;
using tablet::Tablet;
using tablet::TabletMetadata;

static const char* const kSeparatorLine =
  "----------------------------------------------------------------------\n";

namespace {
string Indent(int indent) {
  return string(indent, ' ');
}

string IndentString(const string& s, int indent) {
  return Indent(indent) + StringReplace(s, "\n", "\n" + Indent(indent), true);
}
} // anonymous namespace

FsTool::FsTool(DetailLevel detail_level)
    : initialized_(false),
      detail_level_(detail_level) {
}

FsTool::~FsTool() {
}

Status FsTool::Init() {
  CHECK(!initialized_) << "Already initialized";
  // Allow read-only access to live blocks.
  FsManagerOpts opts;
  opts.read_only = true;
  fs_manager_.reset(new FsManager(Env::Default(), opts));
  RETURN_NOT_OK(fs_manager_->Open());

  LOG(INFO) << "Opened file system with uuid: " << fs_manager_->uuid();

  initialized_ = true;
  return Status::OK();
}

Status FsTool::ListAllTablets() {
  DCHECK(initialized_);

  vector<string> tablets;
  RETURN_NOT_OK(fs_manager_->ListTabletIds(&tablets));
  for (const string& tablet : tablets) {
    if (detail_level_ >= HEADERS_ONLY) {
      std::cout << "Tablet: " << tablet << std::endl;
      RETURN_NOT_OK(PrintTabletMeta(tablet, 2));
    } else {
      std::cout << "\t" << tablet << std::endl;
    }
  }
  return Status::OK();
}

Status FsTool::PrintTabletMeta(const string& tablet_id, int indent) {
  scoped_refptr<TabletMetadata> meta;
  RETURN_NOT_OK(TabletMetadata::Load(fs_manager_.get(), tablet_id, &meta));

  const Schema& schema = meta->schema();

  std::cout << Indent(indent) << "Partition: "
            << meta->partition_schema().PartitionDebugString(meta->partition(), meta->schema())
            << std::endl;
  std::cout << Indent(indent) << "Table name: " << meta->table_name()
            << " Table id: " << meta->table_id() << std::endl;
  std::cout << Indent(indent) << "Schema (version=" << meta->schema_version() << "): "
            << schema.ToString() << std::endl;

  tablet::TabletSuperBlockPB pb;
  RETURN_NOT_OK_PREPEND(meta->ToSuperBlock(&pb), "Could not get superblock");
  std::cout << "Superblock:\n" << pb.DebugString() << std::endl;

  return Status::OK();
}


} // namespace kudu-manager
} // namespace kudu