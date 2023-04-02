#include"database.hh"

void Database::printStore(){
    auto it = map.begin();
    while(it != map.end()){
        std::cout << "Key = " << it->first << ", Value = " << it->second << std::endl;
        it++;
    }
}

void Database::put(std::string key, int value){
    map[key] = value;
    persist();
}

void Database::persist(){
    std::ofstream f_out;
    f_out.open(fileName, std::fstream::trunc);
    for(auto it = map.begin(); it != map.end(); it++){
        std::string line = it->first + "$" + std::to_string(it->second);
        f_out << line << std::endl;
    }
    f_out.close();
}

int Database::get(std::string key){
    return map[key];
}
