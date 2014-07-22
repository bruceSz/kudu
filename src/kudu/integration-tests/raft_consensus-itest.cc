// Copyright (c) 2013, Cloudera, inc.

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <boost/foreach.hpp>
#include <boost/assign/list_of.hpp>

#include "kudu/client/client.h"
#include "kudu/client/schema-internal.h"
#include "kudu/common/wire_protocol-test-util.h"
#include "kudu/common/schema.h"
#include "kudu/consensus/consensus_queue.h"
#include "kudu/consensus/raft_consensus.h"
#include "kudu/consensus/raft_consensus_state.h"
#include "kudu/gutil/strings/strcat.h"
#include "kudu/integration-tests/mini_cluster.h"
#include "kudu/master/catalog_manager.h"
#include "kudu/master/master.h"
#include "kudu/master/mini_master.h"
#include "kudu/server/metadata.pb.h"
#include "kudu/tserver/tablet_server.h"
#include "kudu/tserver/mini_tablet_server.h"
#include "kudu/tserver/tablet_server-test-base.h"
#include "kudu/util/random_util.h"
#include "kudu/util/stopwatch.h"
#include "kudu/util/test_util.h"

DECLARE_int32(consensus_entry_cache_size_soft_limit_mb);
DECLARE_int32(consensus_entry_cache_size_hard_limit_mb);
DECLARE_int32(consensus_rpc_timeout_ms);
DECLARE_int32(default_num_replicas);

DEFINE_int32(num_client_threads, 8,
             "Number of client threads to launch");
DEFINE_int64(client_inserts_per_thread, 500,
             "Number of rows inserted by each client thread");
DEFINE_int64(client_num_batches_per_thread, 50,
             "In how many batches to group the rows, for each client");

namespace kudu {

namespace tserver {

using consensus::RaftConsensus;
using consensus::ReplicaState;
using client::FromInternalCompressionType;
using client::FromInternalDataType;
using client::FromInternalEncodingType;
using client::KuduClient;
using client::KuduClientBuilder;
using client::KuduColumnSchema;
using client::KuduColumnStorageAttributes;
using client::KuduSchema;
using client::KuduTable;
using master::GetTableLocationsRequestPB;
using master::GetTableLocationsResponsePB;
using master::TableIdentifierPB;
using master::TabletLocationsPB;
using master::TSInfoPB;
using master::MiniMaster;
using metadata::QuorumPB;
using metadata::QuorumPeerPB;
using std::vector;
using std::tr1::shared_ptr;
using tserver::TabletServer;

static const int kMaxRetries = 20;
static const int kNumReplicas = 3;

// Integration test for distributed consensus.
class DistConsensusTest : public TabletServerTest {
 public:
  DistConsensusTest()
      : inserters_(FLAGS_num_client_threads) {
  }

  struct ProxyDetails {
    master::TSInfoPB ts_info;
    gscoped_ptr<TabletServerServiceProxy> proxy;
  };

  virtual void SetUp() OVERRIDE {
    FLAGS_consensus_entry_cache_size_soft_limit_mb = 5;
    FLAGS_consensus_entry_cache_size_hard_limit_mb = 10;
    FLAGS_consensus_rpc_timeout_ms = 50;
    KuduTest::SetUp();
  }

  void BuildAndStart() {
    CreateCluster();
    CreateClient();
    WaitForAndGetQuorum();
  }

  void CreateCluster() {
    FLAGS_default_num_replicas = kNumReplicas;
    cluster_.reset(new MiniCluster(env_.get(), test_dir_, kNumReplicas));
    ASSERT_STATUS_OK(cluster_->Start());
    ASSERT_STATUS_OK(cluster_->WaitForTabletServerCount(kNumReplicas));
  }

  void CreateClient() {
    // Connect to the cluster.
    ASSERT_STATUS_OK(KuduClientBuilder()
                     .master_server_addr(cluster_->mini_master()->bound_rpc_addr().ToString())
                     .Build(&client_));

    // Create a table with a single tablet, with three replicas.
    //
    // The tests here make extensive use of server schemas, but we need
    // a client schema to create the table.
    KuduSchema client_schema(GetClientSchema(schema_));
    ASSERT_OK(client_->NewTableCreator()
             ->table_name(kTableId)
             .schema(&client_schema)
             .Create());
    ASSERT_STATUS_OK(client_->OpenTable(kTableId, &table_));
  }

  KuduSchema GetClientSchema(const Schema& server_schema) const {
    std::vector<KuduColumnSchema> client_cols;
    BOOST_FOREACH(const ColumnSchema& col, server_schema.columns()) {
      CHECK_EQ(col.has_read_default(), col.has_write_default());
      if (col.has_read_default()) {
        CHECK_EQ(col.read_default_value(), col.write_default_value());
      }
      KuduColumnStorageAttributes client_attrs(
          FromInternalEncodingType(col.attributes().encoding()),
          FromInternalCompressionType(col.attributes().compression()));
      KuduColumnSchema client_col(col.name(), FromInternalDataType(col.type_info()->type()),
                                  col.is_nullable(), col.read_default_value(),
                                  client_attrs);
      client_cols.push_back(client_col);
    }
    return KuduSchema(client_cols, server_schema.num_key_columns());
  }

  void CreateLeaderAndReplicaProxies(const TabletLocationsPB& locations) {
    leader_.reset();
    STLDeleteElements(&replicas_);
    BOOST_FOREACH(const TabletLocationsPB::ReplicaPB& replica_pb, locations.replicas()) {
      HostPort host_port;
      ASSERT_STATUS_OK(HostPortFromPB(replica_pb.ts_info().rpc_addresses(0), &host_port));
      vector<Sockaddr> addresses;
      host_port.ResolveAddresses(&addresses);
      gscoped_ptr<TabletServerServiceProxy> proxy;
      CreateClientProxy(addresses[0], &proxy);
      if (replica_pb.role() == QuorumPeerPB::LEADER) {
        ProxyDetails* leader = new ProxyDetails();
        leader->proxy.reset(proxy.release());
        leader->ts_info.CopyFrom(replica_pb.ts_info());
        leader_.reset(leader);
      } else if (replica_pb.role() == QuorumPeerPB::FOLLOWER) {
        ProxyDetails* replica = new ProxyDetails();
        replica->proxy.reset(proxy.release());
        replica->ts_info.CopyFrom(replica_pb.ts_info());
        replicas_.push_back(replica);
      }
    }
  }

  // Gets the the locations of the quorum and waits until 1 LEADER and kNumReplicas - 1
  // FOLLOWERS are reported.
  void WaitForAndGetQuorum() {
    GetTableLocationsRequestPB req;
    GetTableLocationsResponsePB resp;
    req.mutable_table()->set_table_name(kTableId);
    CHECK_OK(cluster_->mini_master()->master()->catalog_manager()->GetTableLocations(
        &req, &resp));
    CHECK(resp.tablet_locations_size() > 0);
    tablet_id_ = resp.tablet_locations(0).tablet_id();

    TabletLocationsPB locations;
    int num_retries = 0;
    // make sure the three replicas are up and find the leader
    while (true) {
      if (num_retries >= kMaxRetries) {
        FAIL() << " Reached max. retries while looking up the quorum.";
      }
      // TODO add a way to wait for a tablet to be ready. Also to wait for it to
      // have a certain _active_ replication count.
      replicas_.clear();
      Status status = cluster_->WaitForReplicaCount(tablet_id_,
                                                    kNumReplicas, &locations);
      if (status.IsTimedOut()) {
        LOG(WARNING)<< "Timeout waiting for all three replicas to be online, retrying...";
        num_retries++;
        continue;
      }

      ASSERT_STATUS_OK(status);
      CreateLeaderAndReplicaProxies(locations);

      if (leader_.get() == NULL || replicas_.size() < kNumReplicas - 1) {
        LOG(WARNING)<< "Couldn't find the leader and/or replicas. Locations: "
        << locations.ShortDebugString();
        sleep(1);
        num_retries++;
        continue;
      }
      break;
    }
    CreateSharedRegion();
  }

  void ScanReplica(TabletServerServiceProxy* replica_proxy,
                   vector<string>* results) {

    ScanRequestPB req;
    ScanResponsePB resp;
    RpcController rpc;

    NewScanRequestPB* scan = req.mutable_new_scan_request();
    scan->set_tablet_id(tablet_id_);
    ASSERT_STATUS_OK(SchemaToColumnPBs(schema_, scan->mutable_projected_columns()));

    // Send the call
    {
      req.set_batch_size_bytes(0);
      SCOPED_TRACE(req.DebugString());
      ASSERT_STATUS_OK(replica_proxy->Scan(req, &resp, &rpc));
      SCOPED_TRACE(resp.DebugString());
      ASSERT_FALSE(resp.has_error());
    }

    if (!resp.has_more_results())
      return;

    // Drain all the rows from the scanner.
    ASSERT_NO_FATAL_FAILURE(DrainScannerToStrings(resp.scanner_id(),
                                                  schema_,
                                                  results,
                                                  replica_proxy));

    std::sort(results->begin(), results->end());
  }

  string DumpToString(const master::TSInfoPB& leader_info,
                      const vector<string>& leader_results,
                      const master::TSInfoPB& replica_info,
                      const vector<string>& replica_results) {
    string ret = strings::Substitute("Replica results did not match the leaders."
                                     "\nReplica: $0\nLeader:$1. Results size "
                                     "L: $2 R: $3",
                                     leader_info.ShortDebugString(),
                                     replica_info.ShortDebugString(),
                                     leader_results.size(),
                                     replica_results.size());

    StrAppend(&ret, "Leader Results: \n");
    BOOST_FOREACH(const string& result, leader_results) {
      StrAppend(&ret, result, "\n");
    }

    StrAppend(&ret, "Replica Results: \n");
    BOOST_FOREACH(const string& result, replica_results) {
      StrAppend(&ret, result, "\n");
    }

    return ret;
  }

  void AssertAllReplicasAgree(int expected_result_count = -1) {

    int counter = 0;
    while (true) {
      usleep(10000 * counter);
      vector<string> leader_results;
      vector<string> replica_results;
      ScanReplica(leader_->proxy.get(), &leader_results);

      if (expected_result_count != -1 && leader_results.size() != expected_result_count) {
        if (counter >= kMaxRetries) {
          FAIL() << "LEADER results did not match the expected count.";
        }
        counter++;
        continue;
      }

      ProxyDetails* last_replica;
      bool all_replicas_matched = true;
      BOOST_FOREACH(ProxyDetails* replica, replicas_) {
        ScanReplica(replica->proxy.get(), &replica_results);
        last_replica = replica;
        SCOPED_TRACE(DumpToString(leader_->ts_info, leader_results,
                                  replica->ts_info, replica_results));

        if (leader_results.size() != replica_results.size()) {
          all_replicas_matched = false;
          break;
        }

        // when the result sizes match the rows must match
        // TODO handle the case where we have compactions/flushes
        // (needs post-scan sorting or FT scans)
        ASSERT_EQ(leader_results.size(), replica_results.size());
        for (int i = 0; i < leader_results.size(); i++) {
          ASSERT_EQ(leader_results[i], replica_results[i]);
        }
        replica_results.clear();
      }

      if (all_replicas_matched) {
        break;
      } else {
        if (counter >= kMaxRetries) {
          SCOPED_TRACE(DumpToString(leader_->ts_info, leader_results,
                                    last_replica->ts_info, replica_results));
          FAIL() << "LEADER results did not match one of the replicas.";
        }
        counter++;
      }
    }
  }

  void InsertTestRowsRemoteThread(int tid,
                                  uint64_t first_row,
                                  uint64_t count,
                                  uint64_t num_batches,
                                  TabletServerServiceProxy* proxy) {
    TabletServerTest::InsertTestRowsRemote(tid, first_row, count, num_batches, proxy, tablet_id_);
    inserters_.CountDown();
  }

  // Brings Chaos to a MiniTabletServer by introducing random delays. Does this by stealing the
  // consensus lock a portion of the time.
  // TODO use the consensus/tablet/log hooks _as_well_as_ lock stealing
  void DelayInjectorThread(MiniTabletServer* mini_tablet_server) {
    scoped_refptr<TabletPeer> peer;
    CHECK(mini_tablet_server->server()->tablet_manager()->LookupTabletUnlocked(tablet_id_, &peer));
    RaftConsensus* consensus = down_cast<RaftConsensus*>(peer->consensus());
    ReplicaState* state = consensus->GetReplicaStateForTests();
    while (inserters_.count() > 0) {

      // Adjust the value obtained from the normalized gauss. dist. so that we steal the lock
      // longer than the the timeout a small (~5%) percentage of the times.
      // (95% corresponds to 1.64485, in a normalized (0,1) gaussian distribution).
      double sleep_time_usec = 1000 *
          ((NormalDist(0, 1) * FLAGS_consensus_rpc_timeout_ms) / 1.64485);

      if (sleep_time_usec < 0) sleep_time_usec = 0;

      // Additionally only cause timeouts at all 50% of the time, otherwise sleep.
      double val = (rand() * 1.0) / RAND_MAX;
      if (val < 0.5) {
        usleep(sleep_time_usec);
        continue;
      }

      ReplicaState::UniqueLock lock;
      CHECK_OK(state->LockForRead(&lock));
      usleep(sleep_time_usec);
    }
  }


  virtual void TearDown() OVERRIDE {
    cluster_->Shutdown();
    STLDeleteElements(&replicas_);
  }

 protected:
  gscoped_ptr<MiniCluster> cluster_;
  shared_ptr<KuduClient> client_;
  scoped_refptr<KuduTable> table_;
  gscoped_ptr<ProxyDetails> leader_;
  vector<ProxyDetails*> replicas_;

  QuorumPB quorum_;
  string tablet_id_;

  std::vector<scoped_refptr<kudu::Thread> > threads_;
  CountDownLatch inserters_;
};

// TODO allow the scan to define an operation id, fetch the last id
// from the leader and then use that id to make the replica wait
// until it is done. This will avoid the sleeps below.
TEST_F(DistConsensusTest, TestInsertAndMutateThroughConsensus) {
  BuildAndStart();

  int num_iters = AllowSlowTests() ? 10 : 1;

  for (int i = 0; i < num_iters; i++) {
    InsertTestRowsRemoteThread(0, i * FLAGS_client_inserts_per_thread,
                               FLAGS_client_inserts_per_thread,
                               FLAGS_client_num_batches_per_thread,
                               leader_->proxy.get());
  }
  AssertAllReplicasAgree(FLAGS_client_inserts_per_thread * num_iters);
}

TEST_F(DistConsensusTest, TestFailedTransaction) {
  BuildAndStart();

  WriteRequestPB req;
  req.set_tablet_id(tablet_id_);
  ASSERT_STATUS_OK(SchemaToPB(schema_, req.mutable_schema()));

  RowOperationsPB* data = req.mutable_row_operations();
  data->set_rows("some gibberish!");

  WriteResponsePB resp;
  RpcController controller;
  controller.set_timeout(MonoDelta::FromSeconds(FLAGS_rpc_timeout));

  ASSERT_STATUS_OK(DCHECK_NOTNULL(leader_->proxy.get())->Write(req, &resp, &controller));
  ASSERT_TRUE(resp.has_error());

  // Add a proper row so that we can verify that all of the replicas continue
  // to process transactions after a failure. Additionally, this allows us to wait
  // for all of the replicas to finish processing transactions before shutting down,
  // avoiding a potential stall as we currently can't abort transactions (see KUDU-341).
  data->Clear();
  AddTestRowToPB(RowOperationsPB::INSERT, schema_, 0, 0, "original0", data);

  controller.Reset();
  controller.set_timeout(MonoDelta::FromSeconds(FLAGS_rpc_timeout));

  ASSERT_STATUS_OK(DCHECK_NOTNULL(leader_->proxy.get())->Write(req, &resp, &controller));
  SCOPED_TRACE(resp.ShortDebugString());
  ASSERT_FALSE(resp.has_error());

  AssertAllReplicasAgree(1);
}

// Inserts rows through consensus and also starts one delay injecting thread
// that steals consensus peer locks for a while. This is meant to test that
// even with timeouts and repeated requests consensus still works.
TEST_F(DistConsensusTest, MultiThreadedMutateAndInsertThroughConsensus) {
  BuildAndStart();

  if (500 == FLAGS_client_inserts_per_thread) {
    if (this->AllowSlowTests()) {
      FLAGS_client_inserts_per_thread = FLAGS_client_inserts_per_thread * 10;
      FLAGS_client_num_batches_per_thread = FLAGS_client_num_batches_per_thread * 10;
    }
  }

  int num_threads = FLAGS_num_client_threads;
  for (int i = 0; i < num_threads; i++) {
    scoped_refptr<kudu::Thread> new_thread;
    CHECK_OK(kudu::Thread::Create("test", strings::Substitute("ts-test$0", i),
                                  &DistConsensusTest::InsertTestRowsRemoteThread,
                                  this, i, i * FLAGS_client_inserts_per_thread,
                                  FLAGS_client_inserts_per_thread,
                                  FLAGS_client_num_batches_per_thread,
                                  leader_->proxy.get(),
                                  &new_thread));
    threads_.push_back(new_thread);
  }
  for (int i = 0; i < kNumReplicas; i++) {
    scoped_refptr<kudu::Thread> new_thread;
    CHECK_OK(kudu::Thread::Create("test", strings::Substitute("chaos-test$0", i),
                                  &DistConsensusTest::DelayInjectorThread,
                                  this, cluster_->mini_tablet_server(i),
                                  &new_thread));
    threads_.push_back(new_thread);
  }
  BOOST_FOREACH(scoped_refptr<kudu::Thread> thr, threads_) {
   CHECK_OK(ThreadJoiner(thr.get()).Join());
  }

  AssertAllReplicasAgree(FLAGS_client_inserts_per_thread * FLAGS_num_client_threads);
}

TEST_F(DistConsensusTest, TestInsertOnNonLeader) {
  BuildAndStart();

  // Manually construct a write RPC to a replica and make sure it responds
  // with the correct error code.
  WriteRequestPB req;
  WriteResponsePB resp;
  RpcController rpc;
  req.set_tablet_id(tablet_id_);
  ASSERT_STATUS_OK(SchemaToPB(schema_, req.mutable_schema()));
  AddTestRowToPB(RowOperationsPB::INSERT, schema_, 1234, 5678,
                 "hello world via RPC", req.mutable_row_operations());
  ASSERT_STATUS_OK(replicas_[0]->proxy->Write(req, &resp, &rpc));
  SCOPED_TRACE(resp.DebugString());
  ASSERT_TRUE(resp.has_error());
  Status s = StatusFromPB(resp.error().status());
  EXPECT_TRUE(s.IsIllegalState());
  ASSERT_STR_CONTAINS(s.ToString(), "Replica is not leader of this quorum");
  // TODO: need to change the error code to be something like REPLICA_NOT_LEADER
  // so that the client can properly handle this case! plumbing this is a little difficult
  // so not addressing at the moment.
  AssertAllReplicasAgree(0);
}

TEST_F(DistConsensusTest, TestInsertWhenTheQueueIsFull) {
  FLAGS_consensus_entry_cache_size_soft_limit_mb = 0;
  FLAGS_consensus_entry_cache_size_hard_limit_mb = 1;
  BuildAndStart();
  RaftConsensus* replica_consensus = NULL;

  // now get a RaftConsensus instance from one of the replicas and steal the lock
  // so that operations get appended and requests successfully completed, but the
  // queue doesn't get trimmed.
  const TSInfoPB& info = replicas_[0]->ts_info;
  for (int i = 0; i < kNumReplicas; i++) {
    TabletServer* server = cluster_->mini_tablet_server(i)->server();
    if (server->instance_pb().permanent_uuid() != info.permanent_uuid()) continue;
    scoped_refptr<TabletPeer> peer;
    CHECK(server->tablet_manager()->LookupTabletUnlocked(tablet_id_, &peer));
    replica_consensus = down_cast<RaftConsensus*>(peer->consensus());
    break;
  }

  ASSERT_TRUE(replica_consensus != NULL);

  // generate a 128Kb dummy payload
  string test_payload(128 * 1024, '0');

  WriteRequestPB req;
  req.set_tablet_id(tablet_id_);
  ASSERT_STATUS_OK(SchemaToPB(schema_, req.mutable_schema()));
  RowOperationsPB* data = req.mutable_row_operations();

  WriteResponsePB resp;
  RpcController rpc;
  Status s;

  int key = 0;
  int successful_writes_counter = 0;

  // Steal a replica's lock
  {
    ReplicaState::UniqueLock lock;
    ASSERT_OK(replica_consensus->GetReplicaStateForTests()->LockForRead(&lock));
    LOG(INFO)<< "Acquired lock for one of the replicas, starting to write.";

    // .. and insert until insertions fail because the queue is full
    while (true) {
      rpc.Reset();
      data->Clear();
      AddTestRowToPB(RowOperationsPB::INSERT, schema_, key, key,
                     test_payload, data);
      key++;
      ASSERT_OK(leader_->proxy->Write(req, &resp, &rpc));

      if (resp.has_error()) {
        s = StatusFromPB(resp.error().status());
      }
      if (s.ok()) {
        successful_writes_counter++;
        continue;
      }
      break;
    }
    // Make sure the we get Status::ServiceUnavailable()
    ASSERT_TRUE(s.IsServiceUnavailable());
  }

  // .. now release the lock, the lagging replica should eventually ACK the
  // pending request and allow to discard messages from the queue, thus
  // accepting new requests.
  // .. and insert until insertions fail because the queue is full
  int failure_counter = 0;
  while (true) {
    rpc.Reset();
    data->Clear();
    AddTestRowToPB(RowOperationsPB::INSERT, schema_, key, key,
                   test_payload, data);
    key++;
    ASSERT_OK(leader_->proxy->Write(req, &resp, &rpc));
    if (resp.has_error()) {
      s = StatusFromPB(resp.error().status());
    } else {
      successful_writes_counter++;
      s = Status::OK();
    }
    if (s.IsServiceUnavailable()) {
      if (failure_counter++ == 1000) {
        FAIL() << "Wasn't able to write to the tablet.";
      }
      usleep(100 * 1000);
      continue;
    }
    ASSERT_OK(s);
    break;
  }

  AssertAllReplicasAgree(successful_writes_counter);
}

}  // namespace tserver
}  // namespace kudu
