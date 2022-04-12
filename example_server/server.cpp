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

// A server to receive EchoRequest and send back EchoResponse.

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <brpc/server.h>
#include "../echo.pb.h"
#include "../OPcode.h"
#include <rocksdb/db.h>
#include <rocksdb/options.h>

DEFINE_bool(echo_attachment, false, "Echo attachment as well");
DEFINE_int32(port, 8000, "TCP Port of this server");
DEFINE_string(listen_addr, "", "Server listen address, may be IPV4/IPV6/UDS."
            " If this is set, the flag port will be ignored");
DEFINE_int32(idle_timeout_s, -1, "Connection will be closed if there is no "
             "read/write operations during the last `idle_timeout_s'");
DEFINE_int32(logoff_ms, 2000, "Maximum duration of server's LOGOFF state "
             "(waiting for client to close connection before server stops)");

// Your implementation of example::EchoService
// Notice that implementing brpc::Describable grants the ability to put
// additional information in /status.
namespace example {
class EchoServiceImpl : public EchoService {
public:
    EchoServiceImpl(std::string db_path) {
        _db_path = db_path;
        rocksdb::Options options;
        options.create_if_missing = true;
        rocksdb::Status s = rocksdb::DB::Open(options, db_path, &_db);
        assert(s.ok());
    };
    virtual ~EchoServiceImpl() {};
    // somehow I fail to destory the DB but that should be fine?
    void Destroy_DB() {
        printf("terminating. removing %s\n", _db_path.c_str());
        rocksdb::Status s = _db->Close();
        assert(s.ok());
        int result = std::remove(_db_path.c_str());
        if(result != 0) {
            printf("failed to remove %s\n", _db_path.c_str());
        }
    }

    virtual void Echo(google::protobuf::RpcController* cntl_base,
                      const EchoRequest* request,
                      EchoResponse* response,
                      google::protobuf::Closure* done) {
        // This object helps you to call done->Run() in RAII style. If you need
        // to process the request asynchronously, pass done_guard.release().
        brpc::ClosureGuard done_guard(done);

        brpc::Controller* cntl =
            static_cast<brpc::Controller*>(cntl_base);

        // The purpose of following logs is to help you to understand
        // how clients interact with servers more intuitively. You should 
        // remove these logs in performance-sensitive servers.
        OPCODE_T opcode = request->op();
        if(opcode == OP_WRITE) {
            std::string key = request->key();
            std::string data = request->value();
            rocksdb::WriteOptions wopt;
            rocksdb::Status s = _db->Put(wopt, key, data);

            // LOG(INFO) << "Received Insert request"
            //     << ": " << request->key()
            //     << " -> " << request->value() << ")";

            if (!s.ok()) {
                response->set_status(STATUS_KERROR);
            } else {
                response->set_status(STATUS_KOK);
            }
        } else if (opcode == OP_READ) {
            std::string key = request->key();
            std::string data;
            // LOG(INFO) << "Received read request"
            //     << ": " << request->key()
            //     << " -> " << request->value() << ")";
            rocksdb::Status s = _db->Get(rocksdb::ReadOptions(), key, &data);
            if (s.IsNotFound()) {
                LOG(INFO) << "Key not found";
                response->set_status(STATUS_KNOTFOUND);
            } else if (!s.ok()) {
                LOG(INFO) << "Error";
                response->set_status(STATUS_KERROR);
            } else {
                response->set_status(STATUS_KOK);
                response->set_value(data);
            }
        } else if (opcode == OP_DELETE) {
            ;
        } else if (opcode == OP_MODIFY) {
            ;
        } else {
            LOG(ERROR) << "Unsupported OPcode " << opcode;
        }
        // You can compress the response by setting Controller, but be aware
        // that compression may be costly, evaluate before turning on.
        // cntl->set_response_compress_type(brpc::COMPRESS_TYPE_GZIP);
        if (FLAGS_echo_attachment) {
            // Set attachment which is wired to network directly instead of
            // being serialized into protobuf messages.
            cntl->response_attachment().append(cntl->request_attachment());
        }
    }
private:
    std::string _db_path;
    rocksdb::DB* _db;
};
}  // namespace example

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    // Generally you only need one Server.
    brpc::Server server;

    std::string local_db_path = "/tmp/experiment_rocksdb";
    // Instance of your service.
    example::EchoServiceImpl echo_service_impl(local_db_path);

    // Add the service into server. Notice the second parameter, because the
    // service is put on stack, we don't want server to delete it, otherwise
    // use brpc::SERVER_OWNS_SERVICE.
    if (server.AddService(&echo_service_impl, 
                          brpc::SERVER_DOESNT_OWN_SERVICE) != 0) {
        LOG(ERROR) << "Fail to add service";
        return -1;
    }

    butil::EndPoint point;
    if (!FLAGS_listen_addr.empty()) {
        if (butil::str2endpoint(FLAGS_listen_addr.c_str(), &point) < 0) {
            LOG(ERROR) << "Invalid listen address:" << FLAGS_listen_addr;
            return -1;
        }
    } else {
        point = butil::EndPoint(butil::IP_ANY, FLAGS_port);
    }
    // Start the server.
    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s;
    if (server.Start(point, &options) != 0) {
        LOG(ERROR) << "Fail to start EchoServer";
        return -1;
    }

    // Wait until Ctrl-C is pressed, then Stop() and Join() the server.
    server.RunUntilAskedToQuit();
    echo_service_impl.Destroy_DB();
    return 0;
}
