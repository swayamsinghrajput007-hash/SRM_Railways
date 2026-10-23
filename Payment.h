#pragma once
#include <string>
#include <regex>
class Payment { public: double amount; virtual ~Payment(){} virtual bool validate() const =0; virtual std::string getType() const=0; };
class UPIPayment : public Payment { public: std::string upiId; UPIPayment(std::string id,double amt){ upiId=id; amount=amt; } bool validate() const override { std::regex r("^[a-zA-Z0-9._-]+@[a-zA-Z0-9]+$"); return std::regex_match(upiId,r); } std::string getType() const override { return "UPI"; } };
class CardPayment : public Payment { public: std::string cardNo; CardPayment(std::string no,double amt){ cardNo=no; amount=amt; } bool validate() const override { int sum=0; bool alt=false; for(int i=cardNo.size()-1;i>=0;i--){ if(!isdigit(cardNo[i])) continue; int n=cardNo[i]-'0'; if(alt){ n*=2; if(n>9) n-=9; } sum+=n; alt=!alt; } return sum%10==0; } std::string getType() const override { return "Card"; } };
class NetBankingPayment : public Payment { public: std::string bank; NetBankingPayment(std::string b,double amt){ bank=b; amount=amt; } bool validate() const override { return !bank.empty(); } std::string getType() const override { return "NetBanking"; } };
class WalletPayment : public Payment { public: std::string walletId; WalletPayment(std::string id,double amt){ walletId=id; amount=amt; } bool validate() const override { return walletId.size()>=4; } std::string getType() const override { return "Wallet"; } };
