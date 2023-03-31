

class clientServiceImpl final : public raftClientServerService::Service{
    
    Status put(args){
        if (state == FOLLOWER || state == CANDIDATE){
            return false;
        }
        if (state == LEADER){
            //Append entry in log
            //Call raftModule.serverService.appendEntries()
        }
    }

    Status get(args){
        if (state == FOLLOWER || state == CANDIDATE){
            return false;
        }
        if (state == LEADER){
            //Append entry in log
            //Call raftModule.serverService.appendEntries()
        }
    }
}
