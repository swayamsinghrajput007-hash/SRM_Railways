#pragma once
#include <string>
#include <vector>
enum class CoachType { SLEEPER, AC3, AC2, AC1, CC, EC, SECOND };
struct BerthLayout { std::string code; int count; };
class CoachClass {
public:
    std::string code; double ratePerKm; int berthCount;
    std::vector<BerthLayout> layout;
    virtual ~CoachClass(){}
    virtual double getReservationCharge() const { return 20; }
    virtual bool isAC() const { return false; }
    CoachType type;
};
class Sleeper : public CoachClass { public: Sleeper(){ code="SL"; ratePerKm=0.55; berthCount=72; type=CoachType::SLEEPER; layout={{"LB",18},{"MB",18},{"UB",18},{"SL",9},{"SU",9}}; } };
class AC3 : public CoachClass { public: AC3(){ code="3A"; ratePerKm=1.8; berthCount=64; type=CoachType::AC3; layout={{"LB",16},{"MB",16},{"UB",16},{"SL",8},{"SU",8}}; } bool isAC() const override { return true; } double getReservationCharge() const override { return 40; } };
class AC2 : public CoachClass { public: AC2(){ code="2A"; ratePerKm=2.6; berthCount=48; type=CoachType::AC2; layout={{"LB",12},{"UB",12},{"SL",12},{"SU",12}}; } bool isAC() const override { return true; } double getReservationCharge() const override { return 50; } };
class AC1 : public CoachClass { public: AC1(){ code="1A"; ratePerKm=4.2; berthCount=24; type=CoachType::AC1; layout={{"LB",12},{"UB",12}}; } bool isAC() const override { return true; } double getReservationCharge() const override { return 60; } };
class ChairCar : public CoachClass { public: ChairCar(){ code="CC"; ratePerKm=1.2; berthCount=78; type=CoachType::CC; layout={{"WS",39},{"MS",39}}; } };
class ExecChair : public CoachClass { public: ExecChair(){ code="EC"; ratePerKm=2.1; berthCount=56; type=CoachType::EC; layout={{"WS",28},{"MS",28}}; } bool isAC() const override { return true; } };
class Second : public CoachClass { public: Second(){ code="2S"; ratePerKm=0.35; berthCount=108; type=CoachType::SECOND; layout={{"WS",54},{"MS",54}}; } };
