// Native file/document authority shared by all authoring views. Signed: President.
#pragma once
#include "../foundation/foundation.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#include <memory>
namespace ps::authoring {
using Json=nlohmann::json;namespace fs=std::filesystem;
using Read=std::function<std::string(const std::string&)>;
Json parse(const std::string&);
std::vector<Json> read_records(const Read&);
bsl::Catalog compile(const std::vector<Json>&);
std::string read_file(const fs::path&);
void atomic_file(const fs::path&,const std::string&);
struct Entry {std::string id,file,kind,name,storage;};
struct Document {
 const std::string id,file;std::string baseline,source,merge,error;
 explicit Document(std::string identity,std::string path):id(std::move(identity)),file(std::move(path)){}std::vector<std::string> undo,redo;
 bool external=false,recovery=false;std::string checkpoint;
 bool dirty()const{return source!=baseline;}
};
class Workspace {
 fs::path root_,cache_;std::string project_baseline_;std::vector<Entry> entries_;std::map<std::string,std::shared_ptr<Document>> documents_;
 mutable std::map<std::string,std::pair<uintmax_t,fs::file_time_type>> validated_color_assets_;
 void owns(const Document&)const;fs::path source_path(const std::string&)const;fs::path draft_path(const std::string&)const;
 void refresh_index();
 std::vector<Json> candidate(const Document* replacement,std::map<std::string,std::string>* inputs=nullptr)const;
public:
 Workspace(fs::path project_root,fs::path recovery_root);
 const fs::path& root()const{return root_;}const fs::path& cache()const{return cache_;}
 const std::vector<Entry>& entries()const{return entries_;}
 std::shared_ptr<Document> open(const std::string& id);
 void edit(Document&,const std::string&,const std::string& group={});
 void finish_edits();bool undo(Document&);bool redo(Document&);
 void validate(Document&)const;void save(Document&);void save_all();
 void poll();void reload(Document&,bool discard=false);
 void checkpoint();void recover(Document&);
 bool creation_pending()const;
 void recover_creation();
 std::string create_level(const std::string& slug,const std::string& name,const std::string& parent="");
 bool dirty()const;std::vector<std::string> usages(const std::string&)const;
};
}
