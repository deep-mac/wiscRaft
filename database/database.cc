#include"database.h"
#include <limits.h>

void LogDatabase::printStore(){
    auto it = map.begin();
    while(it != map.end()){
        std::cout << "Key = " << it->first << ", Value = " << it->second << std::endl;
        it++;
    }
}

int LogDatabase::put(std::string key, int value){
    map[key] = value;
    persist();
    return map[key];
}

void LogDatabase::persist(){
    std::ofstream f_out;
    f_out.open(fileName, std::fstream::trunc);
    for(auto it = map.begin(); it != map.end(); it++){
        std::string line = it->first + "$" + std::to_string(it->second);
        f_out << line << std::endl;
    }
    f_out.close();
}

int LogDatabase::get(std::string key){
    std::ifstream f_in(fileName);
    std::string line;
    std::string delimiter("$");

    while(getline(f_in, line)) {
	std::string storedKey = line.substr(0, line.find(delimiter));
	std::string storedValue = line.substr(line.find(delimiter)+1);
	if (key == storedKey) {
	    return stoi(storedValue);
	}
    }

    return INT_MAX;
}
