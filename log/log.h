#include<iostream>
#include<deque>
#include<string>
#include<fstream>

class LogEntry{
 public:

 int GetOrPut;
 std::string key;
 int value;
 int command_id;

 LogEntry(){}

 LogEntry(const LogEntry& copy){
  GetOrPut = copy.GetOrPut;
  key = copy.key;
  value = copy.value;
  command_id = copy.command_id;
 }
};

class Log{

 std::deque<LogEntry> log;
 std::ofstream fout;

 public:

 //Should be called atomically for each append
 void LogAppend(LogEntry& Entry){
   log.push_back(Entry);
   fout.open("log.txt",std::fstream::app);
   std::string line = std::to_string(Entry.GetOrPut) + "$" + Entry.key + "$" + std::to_string(Entry.value) + "$" + std::to_string(Entry.command_id);
   fout<<line<<std::endl;
   fout.close();
 }

 //Should be called atomically once for every execute
 void LogCleanup(){
   log.pop_front();
   fout.open("log.txt",std::fstream::trunc);
   for(LogEntry val : log){
    std::string line = std::to_string(val.GetOrPut) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.command_id);
    fout<<line<<std::endl;
   }
   fout.close();
 }

 //Used to get the head for execution
 LogEntry get_head(){
   return log.front(); 
 }

 //Needed for debug purposes
 void print(){
  for(LogEntry val:log){
   std::string line = std::to_string(val.GetOrPut) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.command_id);
   std::cout<<line<<std::endl;
  }
 }
};
