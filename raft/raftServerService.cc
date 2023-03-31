
class serverServiceImpl final : public raftServerService::Service{
    
    Status appendEntry(args){
        //Reset heartbeat counter 
    }

    Status requestVote(args){
        //Implement leader election rules
        //And demote as necessary
    }
}
