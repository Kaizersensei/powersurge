// Prepare bag, wallet and stock; commit without callbacks/allocations. Signed: Atlas.
#include "commerce.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>
namespace bsl {
namespace {bool key(const std::string& s){return !s.empty()&&s.size()<=128&&s.find('\0')==std::string::npos;}void need(bool ok){if(!ok)throw std::invalid_argument("invalid commerce catalog/service bundle");}}
const CurrencyDef* Commerce::currency(const std::string& id) const noexcept {for(const auto& c:definitions_.currencies)if(c.id==id)return &c;return nullptr;}
const ShopDef* Commerce::shop(const std::string& id) const noexcept {for(const auto& s:definitions_.shops)if(s.id==id)return &s;return nullptr;}
const ShopOffer* Commerce::offer(const ShopDef& shop,const std::string& id) const noexcept {for(const auto& o:shop.offers)if(o.id==id)return &o;return nullptr;}
Commerce::Commerce(Inventory& inventory,Events& events,CommerceCatalog definitions,std::set<std::string> players):inventory_(inventory),events_(events),definitions_(std::move(definitions)),players_(std::move(players)){
 need(&events_.inventory_==&inventory_&&events_.catalog_signature()==inventory_.catalog_signature()&&!players_.empty()&&players_.size()<=64&&definitions_.currencies.size()<=16&&definitions_.shops.size()<=64);
 for(const auto& player:players_)need(key(player));
 std::set<std::string> ids,variables;
 for(const auto& c:definitions_.currencies){const auto* v=events_.catalog_.variable(c.variable);need(key(c.id)&&key(c.name)&&ids.insert(c.id).second&&v&&v->minimum>=0&&variables.insert(c.variable).second);}
 ids.clear();for(const auto& s:definitions_.shops){need(key(s.id)&&key(s.name)&&ids.insert(s.id).second&&currency(s.currency)&&!s.offers.empty()&&s.offers.size()<=64);std::set<std::string> offers;
  for(const auto& o:s.offers){const auto* item=events_.catalog_.item(o.item);need(key(o.id)&&offers.insert(o.id).second&&item&&item->storage==Storage::stack&&o.buy>=0&&o.sell>=0&&(o.buy>0||o.sell>0)&&(o.buy==0||o.sell<=o.buy));
   if(!o.stock.empty()){const auto* v=events_.catalog_.variable(o.stock);need(v&&v->minimum==0&&variables.insert(o.stock).second);}else need(!o.restock);
  }
 }
}
int Commerce::balance(const std::string& id,const std::string& player) const {const auto* c=currency(id);need(c&&players_.count(player));return events_.variable(c->variable,player);}
int Commerce::stock(const std::string& id,const std::string& ref,const std::string& player) const {const auto* s=shop(id);need(s&&players_.count(player));const auto* o=offer(*s,ref);need(o);return o->stock.empty()?-1:events_.variable(o->stock,player);}
Result Commerce::trade(const std::string& transaction,const std::string& player,const std::string& id,const std::string& ref,bool buy,int quantity){
 if(inventory_.busy_)return {Code::busy};
 if(!inventory_.enabled_)return {Code::disabled};
 const auto* s=shop(id);if(!s||!players_.count(player)||(transaction.empty()||transaction.size()>247||transaction.find('\0')!=std::string::npos)||quantity<1||quantity>1000000)return {Code::invalid};
 const auto* o=offer(*s,ref);if(!o)return {Code::invalid};const int price=buy?o->buy:o->sell;if(price<=0)return {Code::denied};const auto* c=currency(s->currency);const auto* wallet=events_.catalog_.variable(c->variable);
 const auto context=s->id+"|"+o->id+"|"+c->id+"|"+(buy?"buy":"sell")+"|"+std::to_string(price)+"|"+o->stock+"|"+(o->restock?"1":"0");
 Request request{"commerce/"+transaction,player,buy?Op::grant:Op::remove,o->item,"","",quantity,context};
 const auto prior=inventory_.state_.receipts.find({player,request.transaction});if(prior!=inventory_.state_.receipts.end())return {prior->second.request==request?Code::ok:Code::conflict,prior->second.request==request};
 const std::int64_t amount=static_cast<std::int64_t>(price)*quantity,current=events_.variable(c->variable,player),next_money=current+(buy?-amount:amount);
 if(next_money<wallet->minimum||next_money>wallet->maximum)return {Code::denied};
 const VariableDef* stock=o->stock.empty()?nullptr:events_.catalog_.variable(o->stock);std::int64_t next_stock=0;
 if(stock){next_stock=events_.variable(o->stock,player)+(buy?-static_cast<std::int64_t>(quantity):o->restock?quantity:0);if(next_stock<stock->minimum||next_stock>stock->maximum)return {Code::denied};}
 auto values=events_.values_;values[{c->variable,wallet->scope==Scope::session?"":player}]=static_cast<int>(next_money);
 if(stock)values[{o->stock,stock->scope==Scope::session?"":player}]=static_cast<int>(next_stock);
 Inventory next(inventory_.catalog_,inventory_.enabled_);next.restore(inventory_.state_);auto result=next.execute(request);if(result.code!=Code::ok)return result;if(result.replay)return {Code::conflict};
 using std::swap;swap(inventory_.state_,next.state_);events_.values_.swap(values);return {Code::ok};
}
Result Commerce::invoke(const EventContext& context,const EventCommand& command,const std::string& transaction){
 if(command.op!=EventOp::buy_shop_item&&command.op!=EventOp::sell_shop_item)return {Code::invalid};
 return trade(transaction,context.player,command.target,command.offer,command.op==EventOp::buy_shop_item,command.value);
}
}
