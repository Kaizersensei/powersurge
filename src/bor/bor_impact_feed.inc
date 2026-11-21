/* Confirmed cosmetic outcomes; observers cannot apply damage or consume another reader's event. Signed: Mech. */
#define BOR_IMPACT_CAPACITY 128
struct BorImpactRecord {uint64_t id,source,target,action,epoch;double x,y;int material,kind,strength,damage,sound;bool blocked,parried,defeated;const char* animation;};
struct BorImpactReader {uint64_t cursor,dropped;bool valid;struct BorImpactRecord current;};
static struct BorImpactRecord impact_records[BOR_IMPACT_CAPACITY];
static uint64_t impact_serial;
static unsigned impact_count;
static void impact_clear(void){impact_count=0;}
static struct BorImpactRecord* impact_publish(HostActor* source,HostActor* target,const BorFrame* frame,bool blocked,int actual,bool defeated){
 struct BorImpactRecord value={0};value.id=++impact_serial;value.source=source->lifetime;value.target=target->lifetime;value.action=source->a.action_id;value.epoch=source->a.epoch;
 value.material=target->a.model->impact_material;value.kind=blocked?3:source->a.animation->impact_kind;value.strength=source->a.animation->impact_strength;
 if(!value.strength)value.strength=frame->damage<=4?1:frame->damage<=9?2:3;
 value.damage=actual;value.blocked=blocked;value.parried=blocked&&target->guard_kind==2&&target->guard_age<(int)target->job.parry_ticks;value.defeated=defeated;value.animation=source->a.animation->name;
 value.sound=impact_index(source,target,frame,blocked);
 // Presentation estimate from the overlap bounds, never a new collision/damage test.
 double a[8],b[8];value.x=target->a.x;value.y=target->a.y-target->a.model->soft_height*.5;
 if(bor_corners(&source->a,true,a)&&bor_corners(&target->a,false,b)){
  double ax=a[0],ay=a[1],az=a[0],aw=a[1],bx=b[0],by=b[1],bz=b[0],bw=b[1];
  for(int i=2;i<8;i+=2){ax=fmin(ax,a[i]);az=fmax(az,a[i]);ay=fmin(ay,a[i+1]);aw=fmax(aw,a[i+1]);bx=fmin(bx,b[i]);bz=fmax(bz,b[i]);by=fmin(by,b[i+1]);bw=fmax(bw,b[i+1]);}
  value.x=(fmax(ax,bx)+fmin(az,bz))*.5;value.y=(fmax(ay,by)+fmin(aw,bw))*.5;
 }
 impact_records[value.id%BOR_IMPACT_CAPACITY]=value;if(impact_count<BOR_IMPACT_CAPACITY)impact_count++;return &impact_records[value.id%BOR_IMPACT_CAPACITY];
}
static surgescript_var_t* impact_construct(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=calloc(1,sizeof(*r));r->cursor=impact_serial;surgescript_object_set_userdata(obj,r);return NULL;}
static surgescript_var_t* impact_destruct(surgescript_object_t* obj,const surgescript_var_t** p,int n){free(surgescript_object_userdata(obj));surgescript_object_set_userdata(obj,NULL);return NULL;}
static surgescript_var_t* impact_idle(surgescript_object_t* obj,const surgescript_var_t** p,int n){return NULL;}
static surgescript_var_t* impact_next(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);if(!r)return surgescript_var_set_bool(surgescript_var_create(),false);r->valid=false;if(impact_count){uint64_t next=r->cursor+1,first=impact_serial-impact_count+1;if(next<first){r->dropped+=first-next;next=first;}if(next<=impact_serial){r->cursor=next;r->current=impact_records[next%BOR_IMPACT_CAPACITY];r->valid=true;}}return surgescript_var_set_bool(surgescript_var_create(),r->valid);}
#define IMPACT_NUM(name,field) static surgescript_var_t* impact_##name(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),r&&r->valid?r->current.field:0);}
#define IMPACT_BOOL(name,field) static surgescript_var_t* impact_##name(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);return surgescript_var_set_bool(surgescript_var_create(),r&&r->valid&&r->current.field);}
#define IMPACT_ID(name,field,prefix) static surgescript_var_t* impact_##name(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);char text[64]="";if(r&&r->valid)snprintf(text,sizeof text,prefix"%llx",(unsigned long long)r->current.field);return surgescript_var_set_string(surgescript_var_create(),text);}
IMPACT_NUM(x,x) IMPACT_NUM(y,y) IMPACT_NUM(material,material) IMPACT_NUM(kind,kind) IMPACT_NUM(strength,strength) IMPACT_NUM(damage,damage)
IMPACT_BOOL(blocked,blocked) IMPACT_BOOL(parried,parried) IMPACT_BOOL(defeated,defeated)
IMPACT_ID(id,id,"impact:") IMPACT_ID(source,source,"actor:") IMPACT_ID(target,target,"actor:") IMPACT_ID(action,action,"action:") IMPACT_ID(epoch,epoch,"segment:")
static surgescript_var_t* impact_animation(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);return surgescript_var_set_string(surgescript_var_create(),r&&r->valid?r->current.animation:"");}
static surgescript_var_t* impact_sound(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);int index=r&&r->valid?r->current.sound:-1;return surgescript_var_set_string(surgescript_var_create(),index>=0&&index<bor_sound_count?bor_sound_paths[index]:"");}
static surgescript_var_t* impact_dropped(surgescript_object_t* obj,const surgescript_var_t** p,int n){struct BorImpactReader* r=surgescript_object_userdata(obj);return surgescript_var_set_number(surgescript_var_create(),r?(double)r->dropped:0);}
static void register_impact_feed(surgescript_vm_t* vm){
 surgescript_vm_bind(vm,"BORImpactView","constructor",impact_construct,0);surgescript_vm_bind(vm,"BORImpactView","destructor",impact_destruct,0);surgescript_vm_bind(vm,"BORImpactView","state:main",impact_idle,0);surgescript_vm_bind(vm,"BORImpactView","next",impact_next,0);
#define IMPACT_BIND(name) surgescript_vm_bind(vm,"BORImpactView","get_"#name,impact_##name,0)
 IMPACT_BIND(x);IMPACT_BIND(y);IMPACT_BIND(material);IMPACT_BIND(kind);IMPACT_BIND(strength);IMPACT_BIND(damage);IMPACT_BIND(blocked);IMPACT_BIND(parried);IMPACT_BIND(defeated);IMPACT_BIND(id);IMPACT_BIND(source);IMPACT_BIND(target);IMPACT_BIND(action);IMPACT_BIND(epoch);IMPACT_BIND(animation);IMPACT_BIND(sound);IMPACT_BIND(dropped);
#undef IMPACT_BIND
}
#undef IMPACT_NUM
#undef IMPACT_BOOL
#undef IMPACT_ID
