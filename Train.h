#pragma once
#include <string>
#include <vector>
#include "CoachClass.h"
struct Stop { std::string code; std::string name; std::string arrival, departure; int halt; int dayOffset; int km; };
class Train {
public:
    std::string no, name; std::vector<Stop> route; int runningDaysMask;
    std::vector<CoachClass*> coachClasses;
    virtual ~Train(){ for(auto c: coachClasses) delete c; }
    virtual double getTariffMultiplier() const = 0;
    virtual double getSuperfastCharge() const { return 0; }
    virtual std::string getType() const =0;
    int getStopIndex(const std::string& code) const { for(int i=0;i<route.size();i++) if(route[i].code==code) return i; return -1; }
    bool matches(const std::string& from, const std::string& to) const { int fi=getStopIndex(from), ti=getStopIndex(to); return fi!=-1 && ti!=-1 && fi<ti; }
    bool runsOn(int weekday) const { return (runningDaysMask>>weekday)&1; }
    int distanceBetween(const std::string& from, const std::string& to) const { int fi=getStopIndex(from), ti=getStopIndex(to); if(fi==-1||ti==-1) return -1; return route[ti].km - route[fi].km; }
};
class Rajdhani : public Train { public: Rajdhani(){ runningDaysMask=0x7F; } double getTariffMultiplier() const override { return 1.5; } double getSuperfastCharge() const override { return 45; } std::string getType() const override { return "Rajdhani"; } };
class Shatabdi : public Train { public: Shatabdi(){ runningDaysMask=0x7E; } double getTariffMultiplier() const override { return 1.3; } std::string getType() const override { return "Shatabdi"; } };
class Duronto : public Train { public: Duronto(){ runningDaysMask=0x7F; } double getTariffMultiplier() const override { return 1.4; } std::string getType() const override { return "Duronto"; } };
class Superfast : public Train { public: Superfast(){ runningDaysMask=0x7F; } double getTariffMultiplier() const override { return 1.2; } double getSuperfastCharge() const override { return 30; } std::string getType() const override { return "Superfast"; } };
class JanShatabdi : public Train { public: JanShatabdi(){ runningDaysMask=0x7F; } double getTariffMultiplier() const override { return 1.0; } std::string getType() const override { return "JanShatabdi"; } };
