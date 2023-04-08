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

typedef std::chrono::duration<int, std::ratio_multiply<std::chrono::hours::period, std::ratio<8>>::type> Days; /* UTC: +8:00 */

void printTime(){

    //auto duration = now.time_since_epoch();
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    Days days = std::chrono::duration_cast<Days>(duration);
    duration -= days;
    auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    duration -= seconds;
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    duration -= milliseconds;
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
    duration -= microseconds;
    auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration);

    std::cout << hours.count() << ":"
        << minutes.count() << ":"
        << seconds.count() << ":"
        << milliseconds.count() << ":"
        << microseconds.count() << ":"
        << nanoseconds.count() << std::endl;

}

class raftUtil;

void PeerAppendEntry(int serverID, raftUtil* raftObj, RaftRequester &channel);

void executeEntry(int commandTerm, int commandID, int &lastApplied, int &commitIdx, int &retValue, raftUtil* raftObject);

class raftUtil {


    //Just example methods and variables
    public:

        std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
        std::mutex electionLock;
        std::condition_variable electionCV;
        std::chrono::time_point<std::chrono::high_resolution_clock> election_start;
        int leaderIdx;
        uint32_t serverIdx;
        int currentTerm;
        int votedFor;
        int lastTermVotedFor;
        State state;
        int heartbeat_interval;
        int election_timeout_duration;

        std::mutex raftLock;
        Log log; //Log container doesn't exist
        LogDatabase database;
        std::vector<RaftRequester> peerServers;
        std::map<int, std::string> peerServerIPs;
        std::vector<std::thread> bringupThreads;
        std::ifstream fin;
        std::ofstream fout;
        std::ifstream fin_term;
        std::ofstream fout_term;

        raftUtil(uint32_t id) : log(), database() {//And probably many more args
            peerServerIPs[0] = "10.10.1.1:2048";
            peerServerIPs[1] = "10.10.1.2:2048";
            peerServerIPs[2] = "10.10.1.3:2048";

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

            votedFor = -1;
            fin.open("votedfor.txt");
            std::string line;
            if(getline(fin,line,'\n')) {
                std::stringstream ss(line);
                std::string s_votedFor, s_lastVotedTerm;
                getline(ss, s_votedFor, '$');
                getline(ss, s_lastVotedTerm, '\n');
                votedFor = stoi(s_votedFor);
                lastTermVotedFor = stoi(s_lastVotedTerm);
                std::cout << "VotedFor is : " << votedFor << ", " << lastTermVotedFor <<std::endl;
            }
            fin.close();

            fin_term.open("current_term.txt");
            std::string line1;
            if (getline(fin, line1, '\n')){
                currentTerm = stoi(line1);
                std::cout << "Current term from txt = " << currentTerm << std::endl;
            }
            fin_term.close();
            persist_currentTerm();

	    printf("BEFORE THE WEIRD LOOP\n");
	    for(int j=log.LastApplied+1 ; j<log.nextIdx; j++){
	       printf("INSIDE THE WEIRD LOOP\n");
               LogEntry entry;
	       int *value;
	       value = new int;
	       entry = log.get_entry(j-log.LastApplied-1);
	       std::thread th(executeEntry,entry.command_term, entry.command_id,std::ref(log.LastApplied),std::ref(log.commitIdx),std::ref(*value), this);
               bringupThreads.push_back(std::move(th));
               bringupThreads.back().detach();
	       delete value;
            }
        }

        //Used to persist votedFor variable
        void persist_vote(){
            fout.open("votedfor.txt",std::fstream::trunc);
            std::string line = std::to_string(votedFor) + "$" + std::to_string(lastTermVotedFor);
            fout<<line<<std::endl;
            fout.close();
        }

        void persist_currentTerm(){
            fout.open("current_term.txt", std::fstream::trunc);
            std::string line = std::to_string(currentTerm);
            fout << line << std::endl;
            fout.close();
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

void electionTimer(std::chrono::microseconds us, raftUtil* raftObj, int *timed_out)
{
    printf("Starting election timer\n");
    raftObj->election_start = std::chrono::high_resolution_clock::now();
    do {
        std::this_thread::yield();
    } while (std::chrono::high_resolution_clock::now() < (raftObj->election_start+us));
    printf("electionTimer:: timeout done\n");
    std::unique_lock<std::mutex> lk(raftObj->electionLock, std::defer_lock);
    printf("Exiting election Timer thread\n");
    raftObj->electionCV.notify_all();
    printf("Exiting election Timer thread\n");
    printTime();
    *timed_out = 1;
}

void PeerRequestVote(int serverID, raftUtil* raftObj, RaftRequester &channel, int* term, bool* voteGranted){
    bool ret_resp;
    int ret_term;
    LogEntry prev_entry;
    *voteGranted = 0;
    raftObj->raftLock.lock();
    if (raftObj->log.nextIdx == 1){
        prev_entry.command_term = 0;
        prev_entry.command_id = 0;
    }
    else{
        prev_entry = raftObj->log.get_tail();
    }
    raftObj->raftLock.unlock();

    if (raftObj->state == CANDIDATE){
        bool rpc_status = channel.RequestVote(raftObj->currentTerm,raftObj->serverIdx,prev_entry.command_id,prev_entry.command_term, ret_term, ret_resp); //Call RPC
        if (rpc_status == false){
            printf("requestVote RPC Failed\n");
        }
        else{
            printf("request vote RPC Success for %d\n", serverID);
            if (raftObj->state == CANDIDATE){
                *term = ret_term; 
                if (ret_resp == true){
                    *voteGranted =  true;
                }
            }
        }
        printf("PeerRequestVote:: Received term = %d, received vote = %d\n", *term, *voteGranted);
    }
}

void PeerSendHeartBeat(int serverID, raftUtil* raftObj, RaftRequester &channel, int* term, bool* isSuccess){
    bool ret_resp;
    int ret_term;
    *isSuccess = false;

    if (raftObj->state == LEADER){
        //FIXME - this has to timeout somehow
        bool rpc_status = channel.SendHeartBeat(raftObj->currentTerm,raftObj->leaderIdx, ret_term, ret_resp); //Call RPC
        if (rpc_status == false){
            printf("heartbeat RPC Failed\n");
        }
        else{
            printf("heartbeat RPC Success for %d\n", serverID);
            if (raftObj->state == LEADER){
                *term = ret_term; 
            }
        }
        //Return true no matter what because your job is to only send heartbeat. You will not get responses from all 
        *isSuccess = true;
        printf("PeerSendHeartBeat:: end of thread for peer serverID = %d, rpc_status = %d\n", serverID, rpc_status);
    }
}



void electionTimeout (std::chrono::microseconds timeout_time, raftUtil& raftObj){

    std::array<bool, 3> voted;
    std::array<int , 3> ret_term;
    while(1){
        if (raftObj.state != LEADER){ 
            int timeout_flag = 0;
            int timed_out = 0;
            
            if (raftObj.state != CANDIDATE) {
                std::thread electionTimerThread(electionTimer, std::chrono::microseconds(raftObj.election_timeout_duration), &raftObj, &timed_out);
                while(timeout_flag == 0 ){
                    auto start = std::chrono::high_resolution_clock::now();
                    raftObj.election_start = std::chrono::high_resolution_clock::now();
                    std::unique_lock<std::mutex> lk(raftObj.electionLock, std::defer_lock);
                    raftObj.electionCV.wait(lk);
                    auto current_time = std::chrono::high_resolution_clock::now();
                    if (current_time - start < timeout_time){
                        //do nothing
                        raftObj.election_start = std::chrono::high_resolution_clock::now();
                        printf("Election thread Woke up early, going back to sleep\n");
                        printTime();
                    }
                    else {
                        printf("Timeout\n");
                        timeout_flag = 1;
                        printTime();
                        electionTimerThread.join();
                    }
                }
            }
            printf("Timed out - starting election\n");
            std::thread peerRequestVoteThread[3];
            raftUtil* raftObject = &raftObj;
            timed_out = 0;

            voted = {false, false, false};
            ret_term = {0, 0, 0};
            //There is no need to see if you already voted this term since the request vote rpc will automatically bring you to the latest term
            //So just increment term and vote for yourself`
            int totalVotes = 0;
            int demote = 0;
            std::thread electionInFlightThread(electionTimer, std::chrono::microseconds(raftObj.election_timeout_duration), &raftObj, &timed_out);
            printf("starting election in flight thread\n");
            raftObject->raftLock.lock();
            raftObj.state = CANDIDATE;
            raftObject->currentTerm++;
            raftObject->persist_currentTerm();
            printf("ElectionTimeout:: incremented current term to %d\n", raftObject->currentTerm);
            raftObject->votedFor = raftObject->serverIdx;
            raftObject->lastTermVotedFor = raftObject->currentTerm;
            raftObject->persist_vote();
            raftObject->raftLock.unlock();
            totalVotes++;
            for (int i = 0; i < 3; i++){
                if(i != raftObject->serverIdx)
                    peerRequestVoteThread[i] = std::thread(PeerRequestVote, i, raftObject, std::ref(raftObject->peerServers[i]), &ret_term[i], &voted[i]);
            }
            printf("starting peer request vote thread\n");
            while(1){
                raftObject->raftLock.lock();
                totalVotes = 1;
                for (int i = 0; i < 3; i++){
                    if(voted[i]){
                        totalVotes++;
                    }
                    if (ret_term[i] > raftObject->currentTerm) {
                        demote = 1; 
                        raftObject->currentTerm = ret_term[i];
                        raftObject->persist_currentTerm();
                    }
                }
                //printf("ret term = %d, %d, %d, currentTerm = %d\n", ret_term[0], ret_term[1], ret_term[2], raftObject->currentTerm);
                if (totalVotes >= 2 || timed_out == 1) {
                    raftObject->raftLock.unlock();
                    break;
                }
                if (demote == 1){
                    raftObject->raftLock.unlock();
                    break;
                }
                if (raftObject->state == FOLLOWER){
                    demote = 1;
                    raftObject->raftLock.unlock();
                    break;
                }
                raftObject->raftLock.unlock();
                std::this_thread::yield();
            }
            printf("ElectionTimeout:: Broke out of while 1 - will kill threads next\n");
            raftObject->raftLock.lock();
            if (totalVotes >= 2){
                //Won election
                //This will make sure the election timeout thread dies
                printf("BECAME LEADER\n");
                raftObject->leaderIdx = raftObject->serverIdx;
                raftObject->election_start = raftObject->election_start - std::chrono::microseconds(raftObj.election_timeout_duration);
                raftObject->state = LEADER;
                for (int i = 0; i < 3; i++){
                    if(i != raftObject->serverIdx){
                        if (peerRequestVoteThread[i].joinable()){
                            peerRequestVoteThread[i].detach();
                        }
                        else{
                            peerRequestVoteThread[i].join();
                        }    
                    }
                }
            }
            else if (demote == 1){
                raftObject->state = FOLLOWER;
                raftObject->election_start = raftObject->election_start - std::chrono::microseconds(raftObj.election_timeout_duration);
                for (int i = 0; i < 3; i++){
                    if(i != raftObject->serverIdx){
                        if (peerRequestVoteThread[i].joinable()){
                            peerRequestVoteThread[i].detach();
                        }
                        else{
                            peerRequestVoteThread[i].join();
                        }    
                    }
                }
            }
            else if (timed_out == 1 ){
                //Timed out
                printf("ElectionTimeout Thead:: Timed out in election, so would probably tryin again\n");
                for (int i = 0; i < 3; i++){
                    if(i != raftObject->serverIdx){
                        if (peerRequestVoteThread[i].joinable()){
                            peerRequestVoteThread[i].detach();
                        }
                        else{
                            peerRequestVoteThread[i].join();
                        }    
                    }
                }
                //Not sure if need to wait for new timer to timeout or start new election immediately
                //Right now it will wait for another timeout
            }
            raftObject->raftLock.unlock();
            electionInFlightThread.join();
        }
    }
}

void heartbeatThread(std::chrono::microseconds us, raftUtil& raftObj)
{
    std::array<int, 3> ret_term;
    std::array<bool, 3> success;
    raftUtil* raftObject = &raftObj;
    while (1){
        if (raftObj.state == LEADER){
            auto heartbeat_start = std::chrono::high_resolution_clock::now();
            auto end = heartbeat_start + us;
            do {
                std::this_thread::yield();
            } while (std::chrono::high_resolution_clock::now() < (heartbeat_start+us));
            printf("Sending heartbeat\n");
            printTime();
            //FIXME - call raft requester and send heartbeat to all peers
            if (raftObj.state == LEADER){
                //Send heartbeat
                std::thread peerSendHeartBeatThread[3];
                int totalSuccess = 0;
                int demote = 0;
                ret_term = {0, 0, 0};
                success = {false, false, false};
                for (int i = 0; i < 3; i++){
                    if(i != raftObject->serverIdx){
                        peerSendHeartBeatThread[i] = std::thread(PeerSendHeartBeat, i, raftObject, std::ref(raftObject->peerServers[i]), &ret_term[i], &success[i]);
                        peerSendHeartBeatThread[i].detach();
                    }
                }
                printf("Spawned heartbeat threads\n");
                while(1){
                    raftObject->raftLock.lock();
                    totalSuccess = 0;
                    for (int i = 0; i < 3; i++){
                        if (success[i] == true){
                            totalSuccess++;
                        }
                        if (ret_term[i] > raftObject->currentTerm){
                            demote = 1;
                            raftObject->state = FOLLOWER;
                            raftObject->currentTerm = ret_term[i];
                            raftObject->persist_currentTerm();
                        }
                    }
                    if (totalSuccess >= 2 || demote == 1){
                        raftObject->raftLock.unlock();
                        break;
                    }
                    raftObject->raftLock.unlock();
                    std::this_thread::yield();
                }
                printf("heartbeat thread: After while 1 - threads should die by now\n");
            }
        }
        else{
            //Ideally better to start this thread only when someone becomes a leader. Keeping it in background is useless
            //But will have to do thread within thread. Ideally no problem
            std::this_thread::yield();
        }
    }
}

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
                if(start > 1) {
		    int prevIdx = start - raftObj->log.LastApplied - 2;
 		    if (prevIdx >= 0) {
	                prev_entry = raftObj->log.get_entry(start - raftObj->log.LastApplied - 2);
		    } else {
                        prev_entry = raftObj->log.get_file_entry(start-1);
		    }
		}
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
                raftObj->electionCV.notify_all();
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

        if (raftObject->log.get_size() == 0) {   //This condition fires when an extraneous thread runs after a pruned log entry
            raftObject->raftLock.unlock();
            return;
        }

        int32_t logEntryIdx = raftObject->log.commitIdx - raftObject->log.LastApplied - 1;
        LogEntry head_entry = raftObject->log.get_head();
        //std::cout << logEntryIdx <<" , " << commandID << " , " << head_entry.command_id << " , " << commandTerm << " , " << head_entry.command_term << std::endl;
        if ((logEntryIdx >=0) && (commandID == head_entry.command_id) && (commandTerm == head_entry.command_term)) {
            std::cout << "Here\n";

            assert(head_entry.command_id == commandID);
            assert(head_entry.command_term == commandTerm);
            if (head_entry.GetOrPut) {
                if (raftObject->state == LEADER) {
                    retValue = raftObject->database.get(head_entry.key);
                } else {
                    int ret = raftObject->database.get(head_entry.key);
                }
            } else {
                if (raftObject->state == LEADER) {
                    retValue = raftObject->database.put(head_entry.key, head_entry.value);
                } else {
                    int ret = raftObject->database.put(head_entry.key, head_entry.value);
                }
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
