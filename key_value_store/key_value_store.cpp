#include"key_value_store.h"

void Store::printStore(){
    auto it = map.begin();
    while(it != map.end()){
        std::cout << "Key = " << it->first << ", Value = " << it->second << std::endl;
        it++;
    }
}

void Store::set(std::string key, int value){
    map[key] = value;
    persist();
}

void Store::persist(){
    std::ofstream f_out;
    f_out.open(file_name, std::fstream::trunc);
    for(auto it = map.begin(); it != map.end(); it++){
        std::string line = it->first + "$" + std::to_string(it->second);
        f_out << line << std::endl;
    }
    f_out.close();
}

int Store::get(std::string key){
    return map[key];
}
