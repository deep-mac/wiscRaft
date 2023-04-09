#pragma once

#include<iostream>
#include<vector>
#include<deque>
#include<string>
#include<array>
#include<fstream>
#include<sstream>

class LogEntry{
    public:

        int GetOrPut;
        std::string key;
        int value;
        int command_id;
        int command_term;
        int client_id;                    //Command ID from client perspective
        std::array<bool,3> matched;       //To indicate whether peer server has acknowledged, we are gonna assume 3 servers for this project

        LogEntry(){
            for(int i=0; i<3; i++)
                matched[i] = false;
        }

        LogEntry& operator=(const LogEntry& copy){

            GetOrPut = copy.GetOrPut;
            key = copy.key;
            value = copy.value;
            command_id = copy.command_id;
            command_term = copy.command_term;
            client_id = copy.client_id;
            matched = copy.matched; 

            return *this; 
        }

        LogEntry(const LogEntry& copy){
            GetOrPut = copy.GetOrPut;
            key = copy.key;
            value = copy.value;
            command_id = copy.command_id;
            command_term = copy.command_term;
            client_id = copy.client_id;
            matched = copy.matched; 
        }

        void printEntry() {
            std::cout << " Printing Log Entry!";
            std::cout << " command : " << GetOrPut << ", key : " << key << ", value : " <<value << ", command ID : " << command_id << ", commandTerm : " << command_term << ", client ID : " << client_id  << std::endl;
        }
};

class Log{

    std::deque<LogEntry> log;
    std::ofstream fout;
    std::ifstream fin;

    public:

    int LastApplied;
    int nextIdx;
    int commitIdx;
    int matchIdx;

    Log(){
        //Initializing LastApplied on boot/crash comeback
        LastApplied = 0;
        fin.open("applied.txt");
        std::string line;
        if(getline(fin,line,'\n'))
        {
            LastApplied = stoi(line);
            std::cout << "LastApplied : " << LastApplied << std::endl;
        }
        fin.close();

        //Initializing log on boot/crash comeback 
        LogEntry copy;
        int i;
        nextIdx = 1;
        fin.open("log.txt");
        for(i=1; getline(fin,line); i++){
            if(i >= LastApplied+1){
                std::stringstream stream(line);
                std::string temp;
                getline(stream,temp,'$');
                copy.GetOrPut = stoi(temp);
                getline(stream,temp,'$');
                copy.key = temp;
                getline(stream,temp,'$');
                copy.value = stoi(temp);
                getline(stream,temp,'$');
                copy.command_id = stoi(temp);
                getline(stream,temp,'$');
                copy.command_term = stoi(temp);
                getline(stream,temp,'\n');
                copy.client_id = stoi(temp);
                copy.printEntry();
                log.push_back(copy);     
            }
        }
        nextIdx = i;
        fin.close();

        //Initializing commitIdx on boot/crash comeback
        commitIdx = 0;
        matchIdx = 0;
        fin.open("commit.txt",std::fstream::in);
        if(getline(fin,line,'\n')){
            commitIdx = stoi(line);
            matchIdx = commitIdx;
        }
        fin.close();
    }

    //Should be called atomically for each append
    void LogAppend(LogEntry& Entry){
        std::cout << "Log::Entering LogAppend\n";
        log.push_back(Entry);
        nextIdx++;
        fout.open("log.txt",std::fstream::app);
        std::string line = std::to_string(Entry.GetOrPut) + "$" + Entry.key + "$" + std::to_string(Entry.value) + "$" + std::to_string(Entry.command_id) + "$" + std::to_string(Entry.command_term) + "$" + std::to_string(Entry.client_id);
        fout<<line<<std::endl;
        fout.close();
    }

    //Should be called atomically once for every execute
    void LogCleanup(){
        std::cout<<"Log Applied!"<<std::endl;
        log.pop_front();
        LastApplied++;

        fout.open("applied.txt",std::fstream::trunc);
        fout<<std::to_string(LastApplied)<<std::endl;
        fout.close();

        //Why? : If we un-persist the data on leader after execution (which happend on majority), some straggler server (not part of majority) might need it after deleting the entry which is wrong, so commenting this.
        /*
           fout.open("log.txt",std::fstream::trunc);
           for(LogEntry val : log){
           std::string line = std::to_string(val.GetOrPut) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.command_id) + "$" + std::to_string(val.command_term) + "$" + std::to_string(val.client_id);
           fout<<line<<std::endl;
           }
           fout.close();
         */
    }

    //Used to clean the persistent log entry, should be done atomically
    void PersistentLogCleanup(){
        std::vector<std::string> vec;
        std::string line;
        int i;

        fin.open("log.txt",std::fstream::in);
        for(i=0; getline(fin,line); i++)
            vec.push_back(line);
        fin.close();

        fout.open("log.txt",std::fstream::trunc);
        for(int j=0; j<i-1; j++)
            fout<<vec[j];
        fout.close();
    }

    //Used to get the head for execution
    LogEntry get_head(){
        return log.front(); 
    }

    //Used to get the tail for checking
    LogEntry get_tail(){
        return log.back();
    }

    //Used to access a index of a log
    LogEntry& get_entry(int idx){
        return log[idx];
    }

    //Used to get the entry from file log
    LogEntry get_file_entry(int idx){
        LogEntry copy;
        std::string line;

        fin.open("log.txt",std::fstream::in);
        for(int i=1; getline(fin,line); i++){
            if(i == idx){
                std::stringstream stream(line);
                std::string temp;
                getline(stream,temp,'$');
                copy.GetOrPut = stoi(temp);
                getline(stream,temp,'$');
                copy.key = temp;
                getline(stream,temp,'$');
                copy.value = stoi(temp);
                getline(stream,temp,'$');
                copy.command_id = stoi(temp);
                getline(stream,temp,'$');
                copy.command_term = stoi(temp);
                getline(stream,temp,'\n');
                copy.client_id = stoi(temp);
                break;
            }
        }
        fin.close();

        return copy;
    }

    //To set the matched bit based on the server
    void set_matched(int LogIdx, int ServerIdx){
        log[LogIdx].matched[ServerIdx] = true;
    }

    //Used to get the size of log
    int get_size(){
        return log.size();  
    }

    //Need to persist atomically
    void persist_commitIdx(){
        fout.open("commit.txt",std::fstream::trunc);
        std::string line = std::to_string(commitIdx);
        fout<<line<<std::endl;
        fout.close();
    }

    //Needed for debug purposes
    void print(){
        std::cout << "Volatile Log -  " << std::endl;
        for(LogEntry val:log){
            std::string line = std::to_string(val.GetOrPut) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.command_id) + "$" + std::to_string(val.command_term) + "$" + std::to_string(val.client_id);
            std::cout<<line<<std::endl;
        }
        std::cout << "Persistent Log -  " << std::endl;

        std::ifstream fh;
        fh.open("log.txt");
        std::string line;
        while(getline(fh, line)){
            std::cout << line << std::endl;
        }
        fh.close();
    }
};
