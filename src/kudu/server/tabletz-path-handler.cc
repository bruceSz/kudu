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

#include "kudu/server/tabletz-path-handler.h"

#include <boost/bind.hpp>
#include <fstream>
#include <memory>
#include <string>


#include "kudu/server/webserver.h"
#include "kudu/master/catalog_manager.h"
#include "kudu/master/master.pb.h"
#include "kudu/util/jsonwriter.h"


using std::shared_ptr;
using std::stringstream;

namespace kudu {

namespace {

void TabletzPathHandler(master::CatalogManager* catalog_manager,
                     const Webserver::WebRequest& req, stringstream* output) {
  master::DumpTabletsInfoResponsePB tabletsInfo;
  master::DumpTabletsInfoRequestPB dump_req;
  Status s = catalog_manager->DumpTabletsInfoPB(dump_req, &tabletsInfo);
  JsonWriter writer(output, JsonWriter::PRETTY);
  writer.StartObject();
  if (s.ok()) {
    writer.String("ALL");
    writer.Protobuf(tabletsInfo);
  } else {
    writer.String(s.ToString());
  }

  writer.EndObject();
}

} // anonymous namespace

void AddTabletzPathHandlers(Webserver* webserver,
                            master::CatalogManager* catalog_manager) {
  webserver->RegisterPathHandler("/Tablets", "TABLETs",
                                 boost::bind(TabletzPathHandler, catalog_manager, _1, _2),
                                 false, true);
}

} // namespace kudu
