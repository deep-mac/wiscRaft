#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>

#ifdef BAZEL_BUILD
#include "examples/protos/client.grpc.pb.h"
#include "examples/protos/server.grpc.pb.h"
#else
#include "database.grpc.pb.h"
#include "raft.grpc.pb.h"
#endif
#include "log.hh"

using grpc::Channel;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ClientContext;
using grpc::Status;
using database::Database;
using database::DatabaseRequest;
using database::DatabaseResponse;
using raft::Raft;
using raft::RaftReply;
using raft::RaftRequest;

class DatabaseImpl final : public Database::Service {
  public:
  Log log;
  uint32_t serverID;
  uint32_t leaderID;

  DatabaseImpl(uint32_t id) {
    serverID = id;
    //TODO: Change this. Add leader state
    leaderID = id;
  }

  Status Get(ServerContext* context, const DatabaseRequest* request, DatabaseResponse* reply) override {
    // Call get impl
    std::cout << "Database:Entering Get\n";
    uint32_t commandID = request->sequenceid();
    std::string key = request->datakey();
    int value =  request->datavalue();
    printRequest(request);
    log.LogAppend(true /*isRead*/, key, value, commandID);
   
    // TODO: Branch next few things into separate thread
    log.commitEntry(commandID);
    value = log.executeEntry();
    reply->set_success(true);
    reply->set_leaderid(leaderID);
    reply->set_datavalue(value);
    printResponse(reply);

    std::cout << "Database:Exiting Get\n";
    return Status::OK;
  }

  Status Put(ServerContext* context, const DatabaseRequest* request, DatabaseResponse* reply) override {
    // Call put impl
    std::cout << "Database:Entering Put\n";
    uint32_t commandID = request->sequenceid();
    std::string key = request->datakey();
    int value =  request->datavalue();
    printRequest(request);
    log.LogAppend(false /*isRead*/, key, value, commandID);
   
    // TODO: Branch next few things into separate thread
    log.commitEntry(commandID);
    value = log.executeEntry();
    reply->set_success(true);
    reply->set_leaderid(leaderID);
    reply->set_datavalue(value);
    printResponse(reply);
    std::cout << "Database:Exiting Put\n";
    return Status::OK;
  }

  void printRequest(const DatabaseRequest* request) {
      std::cout <<" Printing Request : \n";
      std::cout << "SequenceID : " << request->sequenceid() << ", dataKey : " <<request->datakey() << ", dataValue : " << request->datavalue() << std::endl;
  }

  void printResponse(DatabaseResponse* reply) {
      std::cout <<" Printing Response : \n";
      std::cout << "success : " << reply->success() << ", leaderID : " <<reply->leaderid() << ", dataValue : " << reply->datavalue() << std::endl;
  }
};

class RaftRequester {
  private:
    uint32_t term;
    uint32_t serverID;
    uint32_t prevLogIdx;
    uint32_t prevLogTerm;
    uint32_t lastLogIdx;
    uint32_t lastLogTerm;
    
  public:
    RaftRequester(std::shared_ptr<Channel> channel)
	: stub_(Raft::NewStub(channel)),
	  term(0),
	  serverID(0),
	  prevLogIdx(0),
	  prevLogTerm(0),
	  lastLogIdx(0),
	  lastLogTerm(0) {
    }

    void AppendEntries(bool command, std::string key, int value = 0) {
        RaftRequest request;
        request.set_command(command);
	request.set_logkey(key);
	request.set_logvalue(value);
	request.set_term(term);
	request.set_serverid(serverID);
	request.set_prevlogidx(prevLogIdx);
	request.set_prevlogterm(prevLogTerm);
	request.set_isheartbeat(false);

	RaftReply reply;

	ClientContext context;

	Status status = stub_->AppendEntries(&context, request, &reply);

	if (status.ok()) {
	  return;
	} else {
	  std::cout << status.error_code() << 
": " << status.error_message() << std::endl;
	  return;
	}
    }

    void RequestVote() {
        RaftRequest request;
	request.set_term(term);
	request.set_serverid(serverID);
	request.set_prevlogidx(lastLogIdx);
	request.set_prevlogterm(lastLogTerm);
	request.set_isheartbeat(false);

	RaftReply reply;

	ClientContext context;

	Status status = stub_->RequestVote(&context, request, &reply);

	if (status.ok()) {
	  return;
	} else {
	  std::cout << status.error_code() << 
": " << status.error_message() << std::endl;
	  return;
	}
    }

  private:
    std::unique_ptr<Raft::Stub> stub_;
};

class RaftResponder final : public Raft::Service {
    Status AppendEntries(ServerContext* context, const RaftRequest* request, RaftReply* reply) override {
	std::cout << "Raft:Inside AppendEntries\n";
	return Status::OK;
    }

    Status RequestVote(ServerContext* context, const RaftRequest* request, RaftReply* reply) override {
	std::cout << "Raft:Inside RequestVote\n";
	return Status::OK;
    }
};


void RunDatabase() {
  std::string server_address("10.10.1.3:50051");

  DatabaseImpl service(1);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

void RunRaft() {
  std::string server_address("10.10.1.2:50051");

  RaftResponder service;

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
 
  RunDatabase();
//  RunRaft();

  return 0;
}
