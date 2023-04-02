#include<iostream>
#include<deque>
#include<string>
#include<fstream>
#include<mutex>
#include "database.hh"
#include <limits.h>
#include <cassert>

class LogEntry{
 public:

 bool isRead;
 std::string key;
 int value;
 uint32_t commandID;

 LogEntry() : isRead(true),
	      key(""),
	      value(0),
	      commandID(INT_MAX) {}

 LogEntry(const LogEntry& copy) {
  isRead = copy.isRead;
  key = copy.key;
  value = copy.value;
  commandID = copy.commandID;
 }
};

class Log {

 std::deque<LogEntry> log;
 std::fstream fh;
 std::mutex logLock;
 uint32_t commitID;
 Database database;
 public:

 Log() : database() {
    fh.open("log.txt", std::fstream::app);
    commitID = 0;
 }

 //Should be called atomically for each append
 void LogAppend(LogEntry& Entry);

 //Should be called atomically once for every execute
 void LogCleanup();

 //Used to get the head for execution
 LogEntry get_head();

 void commitEntry(uint32_t commandID);

 int executeEntry();

 //Needed for debug purposes
 void print();
};
