
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

#include <string>
#include <memory>

#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_bool(agent_mode, false,
            "run kudu_manager in agent mode(which should be deploy on kudu-tserver node).");;
DEFINE_bool(master_mode, false,
			"run kudu_manger in master mode(not supported now).");

DEFINE_string(kudu_master_addr, "127.0.0.1", 
			"kudu_master_addr for agent to connect to.");


namespace kudu {
namespace kudu_manager {

std::string get_kudu_master_addr {
	 // TODO: get_kudu_master_addr from KuduManagerMaster.
	return FLAGS_kudu_master_addr;
}


class KuduMetaScanner {

  public:
  	~KuduMetaScanner();
  	static Status CreateKuduMetaScanner(std::shared_ptr<KuduMetaScanner> kms);
  private:
    KuduMetaScanner();	

};



class KuduManagerAgent {
  public:
  	~KuduManagerAgent();
  private:
  	friend class KuduManagerAgentBuilder;
  	KuduManagerAgent();
  	void SetDownstreamServiceIf(std::shared_ptr<ServiceIf> s_if);
  	void SetKuduMetaScanner(std::shared_ptr<>)

  	std::shared_ptr<ServiceIf> downstream_service_if_;

};

class KuduManagerAgentBuilder {
public:
	KuduManagerAgentBuilder() {}
	~KuduManagerAgentBuilder() {}
	Status Build(std::shared<KuduManagerAgent>* agent);
	KuduManagerAgentBuilder& add_downstream_if(std::shared<ServiceIf> s_if) {service_if_ = s_if;}
private:
	std::shared_ptr<ServiceIf> service_if_;
}

Status KuduManagerAgentBuilder::Build(std::shared_ptr<KuduManagerAgent>* agent) {
	std::shared_ptr<KuduManagerAgent> a(new KuduManagerAgent());


}

static int KuduManagerAgentMain(int argc, char** argv) {

}

static int KuduManagerMasterMain(int argc, char** argv) {
	// TODO: implement this.
}

} // kudu_manager
} // kudu

int Usage() {
	std::string msg;
	msg += "Usage: --agent_mode|--master_mode";
	std::cout << msg << endl;
	return 1;
}

int main(int argc, char** argv) {
  gflags::ParseCommandLineNonHelpFlags(argc, argv, true);
  FLAGS_logtostderr = true;
  kudu::InitGoogleLoggingSafe(argv[0]);
  if (FLAGS_agent_mode) {
  	return KuduManagerAgentMain(argc, argv);
  } else if (FLAGS_master_mode) {
  	return KUduManagerMasterMain(argc, argv);
  } else {
  	return Usage();
  }
  // should not come here.
  return 0;
}