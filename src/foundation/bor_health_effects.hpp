// Adapter uses existing BOR actor health; it does not own another HP value. Signed: Atlas.
#pragma once
#include "foundation.hpp"
extern "C" {
#include "bor_core.h"
}
namespace bsl {
class BorHealthEffects final : public EffectPort {
public:
 void bind(const std::string& owner,BorActor& actor);
 void unbind(const std::string& owner) noexcept;
 bool can_apply(const std::string&,const EffectDef&) const noexcept override;
 void apply(const std::string&,const EffectDef&) noexcept override;
private:
 std::map<std::string,BorActor*> actors_;
};
}
