#pragma once
#include <string>
enum class Gender { M, F, OTHER };
class Person {
public:
    std::string name; int age; Gender gender;
    Person(std::string n,int a,Gender g):name(n),age(a),gender(g){}
    virtual ~Person(){}
    virtual std::string getType() const { return "Person"; }
    bool isSenior() const { return age>=60; }
    bool isFemale45Plus() const { return gender==Gender::F && age>=45; }
};
class User : public Person {
public:
    std::string userId, email, hashedPass;
    User(std::string n,int a,Gender g,std::string uid,std::string em,std::string hp)
    :Person(n,a,g),userId(uid),email(em),hashedPass(hp){}
    std::string getType() const override { return "User"; }
};
class Passenger : public Person {
public:
    std::string berthPref; bool lowerPreferred;
    Passenger(std::string n,int a,Gender g,std::string pref="LB")
    :Person(n,a,g),berthPref(pref){ lowerPreferred = isSenior()||isFemale45Plus(); }
    std::string getType() const override { return "Passenger"; }
};
