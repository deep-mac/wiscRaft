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

class DatabaseImpl final : public Database::Service {
  public:
  raftUtil* raftObject;

  Status Get(ServerContext* context, const DatabaseRequest* request, DatabaseResponse* reply) override {
    // Call get impl
    std::cout << "Database:Entering Get\n";
    std::string key = request->datakey();
    int value =  request->datavalue();
    printRequest(request);

    LogEntry entry;

    raftObject->raftLock.lock();

    if(raftObject->state != LEADER){
     reply->set_success(false);
     reply->set_leaderid(raftObject->leaderIdx);
     raftObject->raftLock.unlock();
     return Status::OK;
    }

    entry.GetOrPut = 1;
    entry.key = key;
    entry.value = value;
    entry.command_id = raftObject->log.nextIdx;
    entry.command_term = raftObject->currentTerm;
    raftObject->log.LogAppend(entry);
    raftObject->raftLock.unlock();
   
    executeEntry(raftObject->currentTerm ,raftObject->log.nextIdx, raftObject->log.LastApplied, raftObject->log.commitIdx, value,raftObject);
    reply->set_success(true);
    reply->set_leaderid(raftObject->leaderIdx);
    reply->set_datavalue(value);
    printResponse(reply);

    std::cout << "Database:Exiting Get\n";
    return Status::OK;
  }

  Status Put(ServerContext* context, const DatabaseRequest* request, DatabaseResponse* reply) override {
    // Call put impl
    std::cout << "Database:Entering Put\n";
    std::string key = request->datakey();
    int value =  request->datavalue();
    printRequest(request);

    LogEntry entry;

    raftObject->raftLock.lock();
    
    if(raftObject->state != LEADER){
     reply->set_success(false);
     reply->set_leaderid(raftObject->leaderIdx);
     raftObject->raftLock.unlock();
     return Status::OK;
    }
    entry.GetOrPut = 1;
    entry.key = key;
    entry.value = value;
    entry.command_id = raftObject->log.nextIdx;
    entry.command_term = raftObject->currentTerm;
    raftObject->log.LogAppend(entry);
    raftObject->raftLock.unlock();
   
    executeEntry(raftObject->currentTerm,raftObject->log.nextIdx, raftObject->log.LastApplied, raftObject->log.commitIdx, value, raftObject);
    raftObject->log.LogCleanup();

    reply->set_success(true);
    reply->set_leaderid(raftObject->leaderIdx);
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

class RaftResponder final : public Raft::Service {

    public:
    raftUtil* raftObject;

    Status AppendEntries(ServerContext* context, const RaftRequest* request, RaftReply* reply) override {
	std::cout << "Raft:Inside AppendEntries\n";
         uint32_t term = request->term();
         uint32_t prevLogIndex = request->prevlogidx();
         uint32_t prevLogTerm = request->prevlogterm();
         uint32_t leaderCommit = request->leadercommit();
         LogEntry entry;
         entry.GetOrPut = (request->command() == true)?1:0; //1 - Get, 0 - Put
         entry.key = request->logkey();
         entry.value = request->logvalue();
         entry.command_term = request->logterm();
         entry.command_id = request->logidx();
         bool is_heartbeat = request->isheartbeat();
         
         raftObject->raftLock.lock(); 
         if(term > raftObject->currentTerm){             //Wrong leader, turn him down!
          raftObject->raftLock.unlock(); 
          reply->set_appendsuccess(false);
          reply->set_term(raftObject->currentTerm); 
        
          return Status::OK; 
         }
         else{                            //Right leader, let's begin!
          if(raftObject->log.get_tail().command_term == prevLogTerm && raftObject->log.get_tail().command_id == prevLogIndex){ //Log consistent, let's proceed!
           raftObject->log.LogAppend(entry);
        
           raftObject->log.commitIdx = (leaderCommit < raftObject->log.nextIdx-1)?leaderCommit:(raftObject->log.nextIdx-1); //Setting the commitIdx on the follower
           raftObject->raftLock.unlock();
        
           //Offloading the execution to the follower based on its convenience! Expected to execute until there is zero gap between LastApplied and commitIdx
           int value;
           std::thread execute_thread(executeEntry,std::ref(entry.command_term),std::ref(entry.command_id),std::ref(raftObject->log.LastApplied),std::ref(raftObject->log.commitIdx),std::ref(value),raftObject);
           execute_thread.detach();
          
           reply->set_appendsuccess(true);
           reply->set_term(raftObject->currentTerm); 
        
           return Status::OK;
          }
          else{ //Log inconsistent, turn down the request!
           reply->set_appendsuccess(false);
           reply->set_term(raftObject->currentTerm);
           raftObject->raftLock.lock();
           raftObject->log.LogCleanup();   //Pruning the log here!
	   //TODO: Prune persistent log after new leader election
           raftObject->raftLock.unlock();
           
           return Status::OK;
          }
        }
     }

    Status RequestVote(ServerContext* context, const RaftRequest* request, RaftReply* reply) override {
	std::cout << "Raft:Inside RequestVote\n";
	return Status::OK;
    }
};


void RunDatabase(uint32_t serverIdx, raftUtil& raftObject) {
  std::string server_address;

  switch (serverIdx) {
    case 0 : server_address = "10.10.1.1:50051"; break;
    case 1 : server_address = "10.10.1.2:50051"; break;
    case 2 : server_address = "10.10.1.3:50051"; break;
    default : std::cout << "Illegal serverIdx. Exiting!" << std::endl; break;
  }

  DatabaseImpl service;
  service.raftObject = &raftObject;

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
  std::cout << "Database Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

void RunRaft(uint32_t serverIdx, raftUtil& raftObject) {

  RaftResponder service;
  service.raftObject = &raftObject;
  std::string server_address;
  switch (serverIdx) {
    case 0 : server_address = "10.10.1.1:50051"; break;
    case 1 : server_address = "10.10.1.2:50051"; break;
    case 2 : server_address = "10.10.1.3:50051"; break;
    default : std::cout << "Illegal serverIdx. Exiting!" << std::endl; break;
  }
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
  std::cout << "Raft Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {

  if (argc != 2) {
   std::cout << "Please provide only serverIdx!" << std::endl;
   return 0;
  } 
  uint32_t serverIdx = atoi(argv[1]);
  std::cout <<" This server's ID = " << serverIdx << std::endl;

  raftUtil raft(serverIdx);
  std::thread peerThread(PeerThreadServer, std::ref(raft)); 
  std::thread databaseThread(RunDatabase, serverIdx, std::ref(raft));
  std::thread raftThread(RunRaft, serverIdx, std::ref(raft));
 
  raftThread.join();
  databaseThread.join();
  peerThread.join();
  return 0;
}
