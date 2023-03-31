#include<mutex>

enum State {LEADER, CANDIDATE, FOLLOWER};
std::mutex raftLock;
Store store;
Log log; //Log container doesn't exist


class raft{
    

    //Just example methods and variables
    public:
    int commitIdx;
    int leaderIdx;
    int currentTerm;
    State state;
    
    //Class for sending messages to other servers
    //In current implementation server won't initiate communication with client
    class serverService{
    
        serverService(std::shared_ptr<Channel> channel)
            : stub_(serverServiceRPC::NewStub(channel)) {
            }

        int appendEntries(args);

        int requestVote(args);
    }

    raft(std::string target) {//And probably many more args
        peerServers.push_back(target[i])
        
    }
    std::vector<serverService> peerServers;

    int heartbeatTimeoutThread(){
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

    //Need to start individual thread for all servers
    int peerServerThread(int serverID){
        
        //Ideally should wake-up at only specific intervals. Not a continuous while 1
        while(1){
            if (state == LEADER){
                //if serverID in badServerList
                    //Start with tail end of log and send append entry
                    //Keep sending append entries by decrementing log when see a failure
            }
        }
    }
    


}
