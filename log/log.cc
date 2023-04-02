#include"log.h"

//Should be called atomically for each append
void Log::LogAppend(LogEntry& entry){
  log.push_back(entry);
  fh.open("log.txt",std::fstream::app);
  std::string line = std::to_string(entry.GetOrPut) + "$" + entry.key + "$" + std::to_string(entry.value) + "$" + std::to_string(entry.commandID);
  fh << line << std::endl;
  fh.close();
}

void Log::LogAppend(bool command, std::string newKey, int newValue, uint32_t newID) {

  LogEntry entry;
  entry.setEntry(command, newKey, newValue, newID);
  log.push_back(entry);
  fh.open("log.txt",std::fstream::app);
  std::string line = std::to_string(command) + "$" + newKey + "$" + std::to_string(newValue) + "$" + std::to_string(newID);
  fh << line << std::endl;
  fh.close();
}


//Should be called atomically once for every execute
void Log::LogCleanup(){
  log.pop_front();
  fh.open("log.txt",std::fstream::trunc);
  for(LogEntry val : log){
   std::string line = std::to_string(val.GetOrPut) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.commandID);
   fh<<line<<std::endl;
  }
  fh.close();
}

//Used to get the head for execution
LogEntry Log::get_head(){
  return log.front(); 
}

//Needed for debug purposes
void Log::print(){
 for(LogEntry val:log){
  std::string line = std::to_string(val.GetOrPut) + "$" + val.key + "$" + std::to_string(val.value) + "$" + std::to_string(val.commandID);
  std::cout<<line<<std::endl;
 }
}

void Log::commitEntry(uint32_t commandID) {
    assert(commitID < log.size());
    if (commandID != log[commitID].commandID) {
    // Shouldn't be here. The commandID passed should always be the same as that of the log entry waiting to commit
       std::cout << __FILE__ << ":" << __LINE__ << ". Incorrect commandID received! Returing without doing anything" << std::endl;
       return;
    }
    commitID++;
}

int Log::executeEntry() {
    int ret = 0;
    assert(commitID > 0);
    LogEntry entry = get_head();
    if (entry.GetOrPut) {
	ret = database.get(entry.key);
    } else {
    	ret = database.put(entry.key, entry.value);
    }
    commitID--;
    return ret;
}

void LogEntry::setEntry(bool command, std::string newKey, int newValue, uint32_t newID) {
    GetOrPut = command;
    key = newKey;
    value = newValue;
    commandID = newID;
}
