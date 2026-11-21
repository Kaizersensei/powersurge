// Fixed brush replay compares exact results and actual work; timings are incidental. Signed: President.
struct PerfReport {bool ran=false,equal=false;uint64_t reference_uploads=0,fast_uploads=0,reference_masks=0,fast_masks=0,deferred_masks=0;double reference_seconds=0,fast_seconds=0;} perf;
uint64_t resource_hash(){
 std::vector<std::pair<cell::Key,const Resource*>> ordered;for(const auto& r:resources)ordered.push_back({r.key,&r});std::sort(ordered.begin(),ordered.end());
 uint64_t hash=1469598103934665603ull;
 auto add=[&](uint8_t byte){hash^=byte;hash*=1099511628211ull;};
 for(auto pair:ordered){const auto& r=*pair.second;if(!occupied(r.key))continue;
  for(int v:{r.key.first,r.key.second})for(int b=0;b<4;b++)add((static_cast<unsigned>(v)>>(8*b))&255);
  image_lock(r.image,"r");
  for(int y=0;y<cell::chunk;y++)for(int x=0;x<cell::chunk;x++){uint8_t red,g,b,a;color_unmap(image_getpixel(r.image,x,y),&red,&g,&b,&a);for(auto byte:{red,g,b,a})add(byte);}
  image_unlock(r.image);
  for(auto byte:r.coverage)add(byte);
 }return hash;
}
void performance_check(){
 auto original=field.doc;int selected_before=selected,brush_before=brush;uint64_t hashes[2]={0,0};std::string documents[2];
 for(int pass=0;pass<2;pass++){
  field.doc=original;field.rebuild();visual_dirty.clear();collision_dirty.clear();rebuild();selected=1;brush=32;
  BakeStats start=bake_stats;
  for(int i=0;i<12;i++){
   double x=-352+i*32,previous=i==0?x:x-32;stroke(previous,32,x,32,false,0);
   rebuild(pass==0,pass==0);
  }
  uint64_t deferred=bake_stats.masks-start.masks;
  rebuild(false,true);
  hashes[pass]=resource_hash();documents[pass]=cell::encode(field.doc);
  if(pass==0){perf.reference_uploads=bake_stats.uploads-start.uploads;perf.reference_masks=bake_stats.masks-start.masks;perf.reference_seconds=bake_stats.seconds-start.seconds;}
  else{perf.fast_uploads=bake_stats.uploads-start.uploads;perf.fast_masks=bake_stats.masks-start.masks;perf.fast_seconds=bake_stats.seconds-start.seconds;perf.deferred_masks=deferred;}
 }
 perf.ran=true;perf.equal=hashes[0]==hashes[1]&&documents[0]==documents[1];
 field.doc=original;field.rebuild();rebuild();selected=selected_before;brush=brush_before;
}
