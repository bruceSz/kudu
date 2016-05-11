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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "kudu/client/client.h"
#include "kudu/common/column_predicate.h"
#include "kudu/common/scan_spec.h"
#include "kudu/util/auto_release_pool.h"
#include "kudu/util/memory/arena.h"

namespace kudu {
namespace client {

// A configuration object which holds Kudu scan options.
//
// Unless otherwise specified, the method documentation matches the
// corresponding methods on KuduScanner.
class ScanConfiguration {
 public:

  static const int64_t kNoTimestamp = -1;
  static const int kHtTimestampBitsToShift = 12;

  explicit ScanConfiguration(KuduTable* table);
  ~ScanConfiguration() = default;

  Status SetProjectedColumnNames(const std::vector<std::string>& col_names) WARN_UNUSED_RESULT;

  Status SetProjectedColumnIndexes(const std::vector<int>& col_indexes) WARN_UNUSED_RESULT;

  Status AddConjunctPredicate(KuduPredicate* pred) WARN_UNUSED_RESULT;

  void AddConjunctPredicate(ColumnPredicate pred);

  Status AddLowerBound(const KuduPartialRow& key);

  Status AddUpperBound(const KuduPartialRow& key);

  Status AddLowerBoundRaw(const Slice& key);

  Status AddUpperBoundRaw(const Slice& key);

  Status AddLowerBoundPartitionKeyRaw(const Slice& partition_key);

  Status AddUpperBoundPartitionKeyRaw(const Slice& partition_key);

  Status SetCacheBlocks(bool cache_blocks);

  Status SetBatchSizeBytes(uint32_t batch_size);

  Status SetSelection(KuduClient::ReplicaSelection selection) WARN_UNUSED_RESULT;

  Status SetReadMode(KuduScanner::ReadMode read_mode) WARN_UNUSED_RESULT;

  Status SetFaultTolerant(bool fault_tolerant) WARN_UNUSED_RESULT;

  void SetSnapshotMicros(uint64_t snapshot_timestamp_micros);

  void SetSnapshotRaw(uint64_t snapshot_timestamp);

  void SetTimeoutMillis(int millis);

  void OptimizeScanSpec();

  const KuduTable& table() {
    return *table_;
  }

  // Returns the projection schema.
  //
  // The ScanConfiguration retains ownership of the projection.
  const Schema* projection() const {
    return projection_;
  }

  // Returns the client projection schema.
  //
  // The ScanConfiguration retains ownership of the projection.
  const KuduSchema* client_projection() const {
    return &client_projection_;
  }

  const ScanSpec& spec() const {
    return spec_;
  }

  bool has_batch_size_bytes() const {
    return has_batch_size_bytes_;
  }

  uint32_t batch_size_bytes() const {
    CHECK(has_batch_size_bytes_);
    return batch_size_bytes_;
  }

  KuduClient::ReplicaSelection selection() const {
    return selection_;
  }

  KuduScanner::ReadMode read_mode() const {
    return read_mode_;
  }

  bool is_fault_tolerant() const {
    return is_fault_tolerant_;
  }

  int64_t snapshot_timestamp() const {
    return snapshot_timestamp_;
  }

  const MonoDelta& timeout() const {
    return timeout_;
  }

  Arena* arena() {
    return &arena_;
  }

 private:
  friend class KuduScanTokenBuilder;

  // Non-owned, non-null table.
  KuduTable* table_;

  // Non-owned, non-null projection schema.
  Schema* projection_;

  // Owned client projection.
  KuduSchema client_projection_;

  ScanSpec spec_;

  bool has_batch_size_bytes_;
  uint32 batch_size_bytes_;

  KuduClient::ReplicaSelection selection_;

  KuduScanner::ReadMode read_mode_;

  bool is_fault_tolerant_;

  int64_t snapshot_timestamp_;

  MonoDelta timeout_;

  // Manages interior allocations for the scan spec and copied bounds.
  Arena arena_;

  // Manages objects which need to live for the lifetime of the configuration,
  // such as schemas, predicates, and keys.
  AutoReleasePool pool_;
};

} // namespace client
} // namespace kudu

