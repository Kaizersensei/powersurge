// Scoped currencies and atomic stack trades over existing inventory/registered values. Signed: Atlas.
#pragma once
#include "events.hpp"
namespace bsl {
struct CurrencyDef {std::string id,name,variable;};
struct ShopOffer {std::string id,item,stock;int buy=0,sell=0;bool restock=false;};
struct ShopDef {std::string id,name,currency;std::vector<ShopOffer> offers;};
struct CommerceCatalog {std::vector<CurrencyDef> currencies;std::vector<ShopDef> shops;};
class Commerce final:public EventTradePort {
public:
 Commerce(Inventory&,Events&,CommerceCatalog,std::set<std::string> players);
 const CommerceCatalog& definitions() const noexcept {return definitions_;}
 int balance(const std::string& currency,const std::string& player) const;
 int stock(const std::string& shop,const std::string& offer,const std::string& player) const;
 Result trade(const std::string& transaction,const std::string& player,const std::string& shop,const std::string& offer,bool buy,int quantity);
 Result invoke(const EventContext&,const EventCommand&,const std::string&) override;
private:
 Inventory& inventory_;Events& events_;const CommerceCatalog definitions_;const std::set<std::string> players_;
 const CurrencyDef* currency(const std::string&) const noexcept;
 const ShopDef* shop(const std::string&) const noexcept;
 const ShopOffer* offer(const ShopDef&,const std::string&) const noexcept;
};
}
