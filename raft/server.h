#pragma once
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
#include <mutex>
#include "log.h"
#include "raft.h"

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

    bool AppendEntries(uint32_t currentTerm, uint32_t leaderIdx, uint32_t prevLogIdx, uint32_t prevLogTerm, bool command, std::string key, int value, uint32_t commandIdx, uint32_t commandTerm, uint32_t leaderCommit, bool isHeartbeat, int &retTerm, bool &isSuccess) {
        RaftRequest request;
        request.set_command(command);
	request.set_logkey(key);
	request.set_logvalue(value);
	request.set_term(currentTerm);
	request.set_leaderidx(leaderIdx);
	request.set_prevlogidx(prevLogIdx);
	request.set_prevlogterm(prevLogTerm);
	request.set_isheartbeat(isHeartbeat);
	request.set_logterm(commandTerm);
	request.set_logidx(commandIdx);
	request.set_leadercommit(leaderCommit);
	printf("Our CIdx : %d, Sent CIdx : %d\n", commandIdx, request.logidx());
	RaftReply reply;

	ClientContext context;

	Status status = stub_->AppendEntries(&context, request, &reply);
	retTerm = reply.term();
	isSuccess = reply.appendsuccess();
	if (status.ok()) {
	  return true;
	} else {
	  std::cout << status.error_code() << 
": " << status.error_message() << std::endl;
	  return false;
	}
    }

    void RequestVote() {
        RaftRequest request;
	request.set_term(term);
//	request.set_leaderidx(raftObject->leaderIdx);
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
