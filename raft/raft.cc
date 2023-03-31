#include<iostream>
#include<thread>
#include<vector>

int main(){

    //Use argc, argv to set config - 
    //Number of servers and probably timeout ranges


    raft raftModule; //raft class

    std::vector<std::thread> threads;

    threads.push_back(runClientServerService); //The impls for receiving Client to server messages
    threads.push_back(runServerService); //The impls for receiving Server to server messages

    threads.push_back(raftModule.heartbeatTimeoutThread);

    threads.push_back(raftModule.sendHeartbeatThread);

    threads.push_back(raftModule.peerServerThread);



}


