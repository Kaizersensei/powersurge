/* One coherent shop/quest/HP/checkpoint/save journey using native event adapters. Signed: Atlas. */
static void commerce_test_tick(void){
 const char* mode=getenv("BSL_COMMERCE_TEST");if(!mode)return;static int stage,wait,checks,failures;static uint64_t original;
 if(++wait<60)return;
 HostActor* p=hero();HostActor* alternate=equipment_alternate();BslCommerceProbe shop={0};BslGameplayProbe bag={0};BslQuestProbe quest={0};BslStatProbe xp={0};BslProgressProbe progress={0};
 bsl_host_commerce_probe(&shop);bsl_host_gameplay_probe(&bag);bsl_host_quest_probe("quest:training_supplies",&quest);bsl_host_stat_probe("stat:experience",&xp);bsl_host_progress_probe(&progress);
#define SHOP_CHECK(x,m) do{bool ok=(x);++checks;if(!ok)++failures;logfile_message("COMMERCE %s: %s",ok?"PASS":"FAIL",m);if(!ok)logfile_message("COMMERCE observed coins=%d stock=%d items=%d quest=%d XP=%llu",shop.balance,shop.stock,bag.items,quest.status,(unsigned long long)xp.total);}while(0)
 if(stage==0){
  SHOP_CHECK(p&&alternate&&shop.enabled&&quest.enabled,"live shop, quest and explicit player binding exist");if(!p||!alternate||!shop.enabled){stage=99;return;}original=p->lifetime;checkpoint_park_enemies();
  if(!strcmp(mode,"read")){
   SHOP_CHECK(progress.loads==1&&shop.balance==9&&shop.stock==3&&bag.items==4&&quest.status==4&&xp.total==5000,"fresh Continue restores wallet, finite stock, owned bag and completed quest together");
   SHOP_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",p->lifetime)==0,"fresh actor starts a new buy intent after durable journals were compacted");bsl_host_simulation_tick();bsl_host_commerce_probe(&shop);bsl_host_gameplay_probe(&bag);SHOP_CHECK(shop.balance==6&&shop.stock==2&&bag.items==5,"continued buy commits exactly one new item and charge");p->a.health=50;SHOP_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"purchased item uses the existing owner-bound effect on actual BOR HP");stage=99;
  }else{
   SHOP_CHECK(shop.balance==12&&shop.stock==4&&bag.items==3&&quest.status==1,"new mode fixture has authored funds/stock and base bag");SHOP_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",alternate->lifetime)!=0,"inactive actor cannot spend the controlled player's wallet");checkpoint_place(p,230,p->a.y);stage=1;wait=0;
  }
 }else if(stage==1){
  SHOP_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:halfway")==0,"temporary checkpoint captures pre-trade wallet, stock, bag and quest state");
  SHOP_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_accept",p->lifetime)==0,"accept quest through existing authored command");bsl_host_simulation_tick();SHOP_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",p->lifetime)==0,"shop purchase queues through the same captured-player event executor");bsl_host_simulation_tick();bsl_host_commerce_probe(&shop);bsl_host_gameplay_probe(&bag);SHOP_CHECK(shop.balance==9&&shop.stock==3&&bag.items==4,"buy transfers one stock unit into the bag for the authored price");
  SHOP_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"exploration event supplies the quest's remaining fact");stage=2;wait=0;
 }else if(stage==2){
  SHOP_CHECK(quest.status==3&&xp.total==2500,"bought supplies and exploration jointly satisfy the live quest projection");SHOP_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"claim ready quest");bsl_host_simulation_tick();bsl_host_quest_probe("quest:training_supplies",&quest);bsl_host_stat_probe("stat:experience",&xp);SHOP_CHECK(quest.status==4&&xp.total==5000,"quest completion and reward consume the same services after commerce");
  p->a.health=50;SHOP_CHECK(bsl_host_use_item("item:strawberry")==0&&p->a.health==55,"one purchased supply heals the live actor");SHOP_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_sell",p->lifetime)==0,"sell one remaining owned supply through authored shop command");bsl_host_simulation_tick();bsl_host_commerce_probe(&shop);bsl_host_gameplay_probe(&bag);SHOP_CHECK(shop.balance==10&&shop.stock==3&&bag.items==2,"sale pays the sell price without restocking when policy is disabled");
  SHOP_CHECK(bsl_host_checkpoint_request()==0,"queue coherent pre-trade rewind");stage=3;wait=0;
 }else if(stage==3){
  SHOP_CHECK(shop.balance==12&&shop.stock==4&&bag.items==3&&quest.status==1&&xp.total==0,"rewind restores original wallet, merchant stock, inventory, quest and XP together");checkpoint_park_enemies();
  SHOP_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_accept",p->lifetime)==0,"quest is available after rewind");bsl_host_simulation_tick();SHOP_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",p->lifetime)==0,"rewound shop accepts a legitimate new purchase");bsl_host_simulation_tick();SHOP_CHECK(bsl_host_anchor_start(0xb51f01,"event:objective_reward",p->lifetime)==0,"rewound exploration reward remains coherently available");stage=4;wait=0;
 }else if(stage==4){
  SHOP_CHECK(shop.balance==9&&shop.stock==3&&bag.items==4&&quest.status==3,"replayed purchase and facts produce the expected ready quest");SHOP_CHECK(bsl_host_anchor_start(0xb51f02,"event:quest_claim",p->lifetime)==0,"reclaim quest after coherent rewind");bsl_host_simulation_tick();checkpoint_place(p,120,p->a.y);stage=5;wait=0;
 }else if(stage==5){
  SHOP_CHECK(bsl_host_checkpoint_activate(p->lifetime,"checkpoint:rest")==0&&bsl_host_progress_save()==0,"explicit rest save publishes selected funds/stock and owned progress through the verified temporary slot");SHOP_CHECK(bsl_host_anchor_start(0xb51f03,"event:shop_buy",p->lifetime)==0,"commit an unsaved subsequent purchase");bsl_host_simulation_tick();bsl_host_commerce_probe(&shop);SHOP_CHECK(shop.balance==6&&shop.stock==2,"unsaved purchase changes live economy only");SHOP_CHECK(bsl_host_progress_load()==0,"load requests fresh saved content/progress reconstruction");stage=6;wait=0;
 }else if(stage==6){
  SHOP_CHECK(progress.loads==1&&shop.balance==9&&shop.stock==3&&bag.items==4&&quest.status==4&&xp.total==5000,"fresh reconstruction restores the saved economic and quest bundle, removing unsaved trade");SHOP_CHECK(p->lifetime!=original&&bsl_host_anchor_start(0xb51f03,"event:shop_buy",original)!=0,"stale actor lifetime cannot spend restored funds");stage=99;
 }
 if(stage==99){const char* path=getenv("BSL_COMMERCE_REPORT");FILE* f=path?fopen(path,"w"):NULL;if(f){fprintf(f,"{\"ok\":%s,\"checks\":%d,\"failures\":%d}\n",failures?"false":"true",checks,failures);fclose(f);}engine_quit();}
#undef SHOP_CHECK
}
