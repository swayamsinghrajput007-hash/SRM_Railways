#pragma once
#include <vector>
#include <map>
#include <memory>
#include "Train.h"
#include "Inventory.h"
#include "Exceptions.h"
struct SearchResult { Train* train; int fromIdx, toIdx; int distance; int available; double fare; std::string originDate; };
class RailwaySystem {
public:
    std::vector<std::unique_ptr<Train>> trains;
    std::map<std::string, std::map<std::string, std::vector<TrainDateInventory>>> inventory;
    static RailwaySystem& get(){ static RailwaySystem rs; return rs; }
    void addTrain(std::unique_ptr<Train> t){ trains.push_back(std::move(t)); }
    std::vector<SearchResult> search(const std::string& from, const std::string& to, const std::string& boardingDate, const std::string& coachCode, const std::string& quota);
    double calculateFare(Train* train, CoachClass* cls, int distance, bool isTatkal);
    std::string getOriginDate(const std::string& boardingDate, int dayOffset);
    int weekdayFromDate(const std::string& date);
    void initSampleData();
};
