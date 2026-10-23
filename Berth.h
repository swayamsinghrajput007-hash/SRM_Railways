#pragma once
#include <vector>
#include <utility>
class Berth {
public:
    std::string berthId; std::string berthType;
    std::vector<std::pair<int,int>> occupied;
    Berth(std::string id,std::string type):berthId(id),berthType(type){}
    bool isFree(int fromIdx, int toIdx) const {
        for(auto &oc: occupied){
            if(!(toIdx <= oc.first || fromIdx >= oc.second)) return false;
        }
        return true;
    }
    void occupy(int fromIdx,int toIdx){ occupied.emplace_back(fromIdx,toIdx); }
};
