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

            executeEntry(raftObject->currentTerm ,entry.command_id, raftObject->log.LastApplied, raftObject->log.commitIdx, value,raftObject);
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
            entry.GetOrPut = 0;
            entry.key = key;
            entry.value = value;
            entry.command_id = raftObject->log.nextIdx;
            entry.command_term = raftObject->currentTerm;
            raftObject->log.LogAppend(entry);
            raftObject->raftLock.unlock();

            executeEntry(raftObject->currentTerm,entry.command_id, raftObject->log.LastApplied, raftObject->log.commitIdx, value, raftObject);

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
            printTime();
            uint32_t term = request->term();
            uint32_t prevLogIndex = request->prevlogidx();
            uint32_t prevLogTerm = request->prevlogterm();
            uint32_t leaderCommit = request->leadercommit();
            uint32_t prevLogTermLHS, prevLogIndexLHS;
            LogEntry *entry = new LogEntry;
            entry->GetOrPut = (request->command() == true)?1:0; //1 - Get, 0 - Put
            entry->key = request->logkey();
            entry->value = request->logvalue();
            entry->command_term = request->logterm();
            entry->command_id = request->logidx();
            bool is_heartbeat = request->isheartbeat();
            bool prune = false;
            raftObject->raftLock.lock();

            if(term < raftObject->currentTerm){             //Wrong leader, turn him down!
                raftObject->currentTerm = term;                //Update currentTerm to latest from the surprisingly new leader!
                raftObject->persist_currentTerm();

                raftObject->raftLock.unlock(); 
                reply->set_appendsuccess(false);
                reply->set_term(raftObject->currentTerm); 
                printf("RaftResponder:: AppendEntries:: wrong leader\n");

                return Status::OK; 
            }
            else{                                                  //Right leader
                if(raftObject->currentTerm < term){                   //Move to follower, because some other leader is up now
                    raftObject->state = FOLLOWER;
                    raftObject->electionCV.notify_all();
                }
                raftObject->currentTerm = term;                       //Update currentTerm to latest from the surprisingly new leader, if term is different from our currentTerm!
                raftObject->persist_currentTerm();
 
 /*
                if(raftObject->state == CANDIDATE){
                    raftObject->state = FOLLOWER;                        //Move to follower, because some other leader is up now
                    raftObject->electionCV.notify_all();
                }
*/

                if(is_heartbeat == false){                            //Not a heartbeat, let's begin!
                    printf("RaftResponder:: AppendEntries:: heartbeat false\n");
                    if(raftObject->log.get_size()>0){
                        prevLogTermLHS = raftObject->log.get_tail().command_term;
                        prevLogIndexLHS = raftObject->log.get_tail().command_id;
                    }
                    else{
                        if(prevLogIndex < raftObject->log.nextIdx - 1){ //Why? if your file is not trailing, time to prune!
                            prevLogTermLHS = raftObject->log.get_file_entry(prevLogIndex).command_term;
                            prevLogIndexLHS = raftObject->log.get_file_entry(prevLogIndex).command_id;
                            prune = true;
                        }
                        else{                //Why? Because if you have a trailing log, deny the request straight away, 0 will fail the below condition
                            prevLogTermLHS = 0;
                            prevLogIndexLHS = 0; 
                        }
                    }
                    if((prevLogTermLHS == prevLogTerm && prevLogIndexLHS == prevLogIndex) || entry->command_id == 1 /*First command, ignore*/){ //Log consistent, let's proceed!
                        raftObject->log.LogAppend(*entry);
                        //raftObject->log.print();

                        printf("RaftResponder:: AppendEntries::leaderCommit = %d, raftObject->log.nextIdx = %d\n", leaderCommit, raftObject->log.nextIdx);
                        raftObject->log.commitIdx = (leaderCommit < raftObject->log.nextIdx-1)?leaderCommit:(raftObject->log.nextIdx-1); //Setting the commitIdx on the follower
                        raftObject->log.persist_commitIdx();
                        raftObject->raftLock.unlock();

                        std::cout<<"RaftResponder:: AppendEntries::leadercommit and follower commit"<<leaderCommit<<" "<<raftObject->log.commitIdx<<std::endl;	    
                        //Offloading the execution to the follower based on its convenience! Expected to execute until there is zero gap between LastApplied and commitIdx
                        int *value;
                        value = new int;
                        std::thread execute_thread(executeEntry,entry->command_term,entry->command_id,std::ref(raftObject->log.LastApplied),std::ref(raftObject->log.commitIdx),std::ref(*value),raftObject);
                        execute_thread.detach();

                        reply->set_appendsuccess(true);
                        reply->set_term(raftObject->currentTerm); 
                        delete entry;
                        delete value;
                        return Status::OK;
                    }
                    else{ //Log inconsistent, turn down the request!
                        reply->set_appendsuccess(false);
                        reply->set_term(raftObject->currentTerm);

                        if(prune){
                            if(raftObject->log.get_size()>0)
                                raftObject->log.LogCleanup();   //Pruning the log here!

                            raftObject->log.PersistentLogCleanup(); //Pruning persistent log here
                        }

                        raftObject->raftLock.unlock();
                        delete entry;
                        return Status::OK;
                    }
                }
                else{        //Proper heartbeat ack, your term is same as the leader term
                    if(raftObject->currentTerm <= term){                   //Move to follower, because some other leader is up now
                        raftObject->state = FOLLOWER;
                    }
                    printf("RaftResponder:: AppendEntries:: Acknowleding heartbeat from leader\n");
                    raftObject->raftLock.unlock(); 
                    reply->set_appendsuccess(true);
                    reply->set_term(raftObject->currentTerm); 
                    raftObject->electionCV.notify_all();
                    delete entry;
                    return Status::OK; 
                }
            }
        }

        Status RequestVote(ServerContext* context, const RaftRequest* request, RaftReply* reply) override {
            std::cout << "Raft:Inside RequestVote\n";

            uint32_t term = request->term();
            LogEntry tailEntry;
            raftObject->raftLock.lock();
            reply->set_term(raftObject->currentTerm);
            // If this server's term is greater than requester, then requester is out of date, you are probably a leader, Don't vote
            printf("Responder:: RequetVote:: currentTerm = %d, receveid term  = %d\n", raftObject->currentTerm, term);
            if (raftObject->currentTerm > term) {
                reply->set_votegranted(false);
            } else {
                // If you are in Candidate state, step down to follower
                if (raftObject->currentTerm < term) {
                    raftObject->state = FOLLOWER;
                    raftObject->currentTerm = term;
                    raftObject->persist_currentTerm();
                    printf("Responder:: RequetVote:: Before notify all\n");
                    raftObject->electionCV.notify_all();
                    printf("Responder:: RequetVote:: After notify all\n");
                }

                if(raftObject->log.nextIdx > 1){
                    tailEntry = (raftObject->log.get_size()>0)?raftObject->log.get_tail(): raftObject->log.get_file_entry(raftObject->log.nextIdx-1);
                }
                else{
                    tailEntry.command_id = 0;    //Expecting 0 on first entry so that check fails
                    tailEntry.command_term = 0;
                }
                printf("RaftResponder:: RequestVote:: tailEntry.commandTerm =%d, tailEntry.command_id = %d, request->prevlogterm() = %d, request->prevlogidx = %d\n", tailEntry.command_term, tailEntry.command_id, request->prevlogterm(), request->prevlogidx());

                if ((term <= raftObject->lastTermVotedFor) && ((raftObject->votedFor != -1))) { //You've already voted for someone or voted for yourself, don't vote!
                    printf("RaftResponder:: RequestVote :: if 1\n");
                    reply->set_votegranted(false);
                } else if ((tailEntry.command_term > request->prevlogterm()) || ((tailEntry.command_term == request->prevlogterm()) && (tailEntry.command_id > request->prevlogidx()))) { //Nope, election rule failed. :(
                    printf("RaftResponder:: RequestVote :: if 2\n");
                    reply->set_votegranted(false);
                } else { // Yes, democracy won, let's vote!
                    printf("RaftResponder:: RequestVote :: if 3\n");
                    reply->set_votegranted(true);
                    raftObject->votedFor = request->candidateidx();
                    raftObject->lastTermVotedFor = request->term();
                    raftObject->persist_vote();
                    raftObject->electionCV.notify_all();
                }
            }
            raftObject->raftLock.unlock();
            return Status::OK;
        }
};


void RunDatabase(uint32_t serverIdx, raftUtil& raftObject) {
    std::string server_address;

    switch (serverIdx) {
        case 0 : server_address = "10.10.1.2:50051"; break;
        case 1 : server_address = "10.10.1.2:50052"; break;
        case 2 : server_address = "10.10.1.2:50053"; break;
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
        case 0 : server_address = "10.10.1.2:2048"; break;
        case 1 : server_address = "10.10.1.2:2049"; break;
        case 2 : server_address = "10.10.1.2:2050"; break;
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
    std::thread sendHeartbeatThread(heartbeatThread, std::chrono::microseconds(raftObject.heartbeat_interval), std::ref(raftObject));
    std::thread electionTimeoutThread(electionTimeout, std::chrono::microseconds(raftObject.election_timeout_duration), std::ref(raftObject));
    server->Wait();
    sendHeartbeatThread.join(); //FIXME - this thread should only start when a server becomes a leader. 
    electionTimeoutThread.join();
}

int main(int argc, char** argv) {

    if (argc != 2) {
        std::cout << "Please provide only serverIdx!" << std::endl;
        return 0;
    }
    uint32_t serverIdx = atoi(argv[1]);
    std::cout <<" This server's ID = " << serverIdx << std::endl;

    raftUtil raft(serverIdx);
    raft.election_timeout_duration = 3000000;
    raft.heartbeat_interval = 500000;
    //raft.state = LEADER;
    std::thread peerThread(PeerThreadServer, std::ref(raft)); 
    std::thread databaseThread(RunDatabase, serverIdx, std::ref(raft));
    std::thread raftThread(RunRaft, serverIdx, std::ref(raft));
    raftThread.join();
    databaseThread.join();
    peerThread.join();
    return 0;
}
