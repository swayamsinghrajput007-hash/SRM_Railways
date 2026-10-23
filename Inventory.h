#pragma once
#include <map>
#include <string>
#include <vector>
#include "Berth.h"
#include "CoachClass.h"
struct CoachInventory { std::string coachNo; CoachClass* cls; std::vector<Berth> berths; CoachInventory(std::string no, CoachClass* c):coachNo(no),cls(c){ for(int i=0;i<c->berthCount;i++){ std::string bType = c->layout[i % c->layout.size()].code; berths.emplace_back(no+"-"+std::to_string(i+1), bType); } } int availableCount(int fromIdx,int toIdx) const { int cnt=0; for(auto &b: berths) if(b.isFree(fromIdx,toIdx)) cnt++; return cnt; } };
struct TrainDateInventory { std::string originDate; std::vector<CoachInventory> coaches; };
