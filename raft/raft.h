#pragma once
#include<mutex>
#include<functional>
#include <thread>
#include <iostream>
#include "log.h"
#include "database.h"
#include <fstream>
#include <string>
#include <cassert>
#include "server.h"

enum State {LEADER, CANDIDATE, FOLLOWER};

class raftUtil;

void PeerAppendEntry(int serverID, raftUtil* raftObj, RaftRequester &channel);

class raftUtil {
    

    //Just example methods and variables
    public:
    int leaderIdx;
    uint32_t serverIdx;
    int currentTerm;
    State state;
    
    std::mutex raftLock;
    Log log; //Log container doesn't exist
    LogDatabase database;
    std::vector<RaftRequester> peerServers;
    std::map<int, std::string> peerServerIPs;

    raftUtil(uint32_t id) : log(), database() {//And probably many more args
        peerServerIPs[0] = "10.10.1.1:4096";
        peerServerIPs[1] = "10.10.1.2:4096";
        peerServerIPs[2] = "10.10.1.3:4096";

        serverIdx = id;
        currentTerm = 0;
        state = FOLLOWER;
        leaderIdx = -1;
        for (int i = 0; i < 3; ++i) {
	   if (i != id) {
  	       peerServers.push_back(std::move(RaftRequester(grpc::CreateChannel(peerServerIPs[i], grpc::InsecureChannelCredentials()))));
	   }
           else
  	       peerServers.push_back(std::move(RaftRequester(grpc::CreateChannel(peerServerIPs[i], grpc::InsecureChannelCredentials()))));
        }
    }

    raftUtil* get_raft(){
       return this;
    }

/*    int heartbeatTimeoutThread(){
        while(1){
            if (state == FOLLOWER){
                //Wait for timeout such that didn't receive heartbeat or append entry
                raftLock.lock();
                state = CANDIDATE;
                raftLock.unlock();
                serverService.requestVote();

                if (win)
                    state = LEADER;
                    currentTerm++;
                }
                //You either check whether all followers are upto date now or you let the peerthread do it
                //The optimization can be that while requesting vote, they can send their respective commitindex so now the new leader would know which all servers are lagging behind and declare them as bad servers
                //Then the peer thread will take care of it
                //We need to check at put impl which all servers are bad and thus avoid sending RPC to them. 
                //As raft gives strong consistency at the cost of availability, it is ok to block/stall client if the leader thinks that the system is not upto date
            }
        }
    }

    int sendHeatbeatThread(){
        while(1){
            if (state == LEADER){
                //Wait timeout
                //Send heartbeat to all servers
            }
        }
    }
*/
};

void PeerAppendEntry(int serverID, raftUtil* raftObj, RaftRequester &channel){
 LogEntry prev_entry,entry;
 bool ret_resp;
 int ret_term;
 
 int start = raftObj->log.nextIdx - 1;     //This variable is analogous to the nextIdx on the paper for each follower server 
 while(1){
  raftObj->raftLock.lock();
//  std::cout<<"Got the lock for peer append entry thread for server "<<serverID<<std::endl;
  if(start < raftObj->log.nextIdx && start != 0){

  printf("Line : %d\n", __LINE__);

   if(start > raftObj->log.LastApplied){                                 //Get from volatile log
    entry = raftObj->log.get_entry(start - raftObj->log.LastApplied - 1);
    if(start > 1)
     prev_entry = raftObj->log.get_entry(start - raftObj->log.LastApplied - 2);
   }
   else{
    entry = raftObj->log.get_file_entry(start);
    if(start>1)
     prev_entry = raftObj->log.get_file_entry(start-1);
   }
   raftObj->raftLock.unlock();

   bool rpc_status = channel.AppendEntries(raftObj->currentTerm,raftObj->leaderIdx,prev_entry.command_id,prev_entry.command_term,entry.GetOrPut, entry.key, entry.value, entry.command_id, entry.command_term, raftObj->log.commitIdx, false, ret_term, ret_resp); //Call RPC
   if (rpc_status == false) {
     printf("RPC Failed\n");
     continue;
   } else {
     printf("RPC Success for %d\n", serverID);
   }
   raftObj->raftLock.lock();
   if(ret_term < raftObj->currentTerm){ //Have another leader, time to step down!
    raftObj->state = FOLLOWER;
    raftObj->raftLock.unlock();
//    std::cout<<"Got the lock for peer append entry thread because of term violation for server "<<serverID<<std::endl;
    break;
   } 

   if(ret_resp == false) {
    start--;
  printf("Line : %d\n", __LINE__);

   }
   else{
  printf("Line : %d\n", __LINE__);

    raftObj->log.set_matched(start - raftObj->log.LastApplied - 1,serverID);
    start++;
   }
   raftObj->raftLock.unlock();
//   std::cout<<"Lost the lock for peer append entry thread for server "<<serverID<<std::endl;
  }
  else{
   if(start == 0)
    start = raftObj->log.nextIdx - 1;

   raftObj->raftLock.unlock();
   std::this_thread::yield();
  }
 }
}

void executeEntry(int commandTerm, int commandID, int &lastApplied, int &commitIdx, int &retValue, raftUtil* raftObject) {

  //retValue = INT_MAX;
      std::cout<< commandID << " , " << commandTerm << ","<<raftObject->log.LastApplied << std::endl;
  while (1) {
      raftObject->raftLock.lock();
      int32_t logEntryIdx = raftObject->log.commitIdx - raftObject->log.LastApplied - 1;
      LogEntry head_entry = raftObject->log.get_head();
      //std::cout << logEntryIdx <<" , " << commandID << " , " << head_entry.command_id << " , " << commandTerm << " , " << head_entry.command_term << std::endl;
      if ((logEntryIdx >=0) && (commandID == head_entry.command_id) && (commandTerm == head_entry.command_term)) {
 	std::cout << "Here\n";

      	assert(head_entry.command_id == commandID);
      	assert(head_entry.command_term == commandTerm);
              if (head_entry.GetOrPut) {
                 int ret = raftObject->database.get(head_entry.key);
              } else {
                 int ret = raftObject->database.put(head_entry.key, head_entry.value);
              }

      	raftObject->log.LogCleanup();
      	raftObject->raftLock.unlock();
      	break;
      } else {
         raftObject->raftLock.unlock();
         std::this_thread::yield();
      }
  }
}

void PeerThreadServer(raftUtil& raftObj){
      std::thread AppendEntryThread[3]; //2 Servers other than leader
      std::array<bool,3> matched;
      raftUtil* raftObject = &raftObj;
    
      while(1){
        raftObject->raftLock.lock();
        if(raftObject->state == LEADER){
         raftObject->raftLock.unlock();
    
         for(int i=0;i < 3; i++)
          if(i != raftObject->serverIdx)
           AppendEntryThread[i] = std::thread(PeerAppendEntry,i,raftObject,std::ref(raftObject->peerServers[i]));
    
         while(1){
          raftObject->raftLock.lock();
//          std::cout<<"Peer Server Master Thread locked"<<std::endl;
    
          if(raftObject->log.get_size() > raftObject->log.matchIdx - raftObject->log.LastApplied){                     //Check for unmatched entries in the volatile log
           raftObject->log.set_matched(raftObject->log.matchIdx-raftObject->log.LastApplied,raftObject->serverIdx);             //Set Committed status for the leader server 
           matched = raftObject->log.get_entry(raftObject->log.matchIdx-raftObject->log.LastApplied).matched;        //Read the matched status, no need for locks as no scope for race around, maybe double check?
           for(int i=0; i<3; i++)
            if(i != raftObject->serverIdx && matched[i]){                                   //Check for majority, since we have only 3 servers, getting one other ack will give majority!
             raftObject->log.matchIdx++;
             for(int j=0; j<raftObject->log.get_size(); j++){                                         //Well, fun part here, checking if matched till last term, then commit (commit rule)
              LogEntry val = raftObject->log.get_entry(j);
              if(val.command_term == raftObject->currentTerm){
               if(raftObject->log.matchIdx >= val.command_id){
                raftObject->log.commitIdx = raftObject->log.matchIdx;
                raftObject->log.persist_commitIdx();                                                
               }
               else
                break;
              }
             }
            }
    
           raftObject->raftLock.unlock();
//           std::cout<<"Peer Server Master Thread unlocked"<<std::endl;
          }
          else{
           raftObject->raftLock.unlock();
//           std::cout<<"Peer Server Master Thread unlocked"<<std::endl;
           std::this_thread::yield();
          }
         }
        }
        else{
         raftObject->raftLock.unlock();
         std::this_thread::yield();
        }  
      }
    }

