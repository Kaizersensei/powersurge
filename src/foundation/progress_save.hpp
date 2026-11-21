// Selected-value durable progress; no level, VM or physics snapshot. Signed: Atlas.
#pragma once
#include "checkpoints.hpp"
#include "gameplay.hpp"
#include <filesystem>
namespace bsl {
struct ProgressSavePolicy {std::string revision;std::vector<std::string> globals;bool calendar=false;};
struct ProgressDocument {std::string revision,checkpoint;RewindProgress values;};
class ProgressSaveStore {
public:
 ProgressSaveStore(const Catalog&,GameplayProfile,StatCatalog,LearningCatalog,ProgressSavePolicy,std::vector<CheckpointDef>);
 ProgressDocument select(const CheckpointRecord&) const;
 // Rebuild defaults and overlay selected values before runtime initialization/commit.
 CheckpointRecord reconstruct(const ProgressDocument&,double x,double y) const;
 std::string encode(const ProgressDocument&) const;
 ProgressDocument decode(const std::string&) const;
 ProgressDocument read(const std::filesystem::path&) const;
 void write(const std::filesystem::path&,const ProgressDocument&) const;
 void recover(const std::filesystem::path&) const;
private:
 const Catalog& catalog_;GameplayProfile profile_;StatCatalog stats_;LearningCatalog learning_;ProgressSavePolicy policy_;std::vector<CheckpointDef> checkpoints_;
 RewindProgress defaults() const;
 RewindProgress expand(const ProgressDocument&) const;
 const CheckpointDef& destination(const std::string&) const;
};
}
