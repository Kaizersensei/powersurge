// Derived checkpoint rules. Signed: Atlas.
#pragma once
#include "checkpoints.hpp"
inline const std::vector<bsl::CheckpointDef>& compiled_checkpoints(){static const std::vector<bsl::CheckpointDef> defs={{"\143\150\145\143\153\160\157\151\156\164\072\150\141\154\146\167\141\171","\154\145\166\145\154\163\057\143\150\145\143\153\160\157\151\156\164\055\163\145\162\166\151\143\145\163\056\154\145\166",11869185ULL,bsl::CheckpointKind::temporary,10,72},{"\143\150\145\143\153\160\157\151\156\164\072\162\145\163\164","\154\145\166\145\154\163\057\143\150\145\143\153\160\157\151\156\164\055\163\145\162\166\151\143\145\163\056\154\145\166",11869186ULL,bsl::CheckpointKind::permanent,20,72},{"\143\150\145\143\153\160\157\151\156\164\072\141\156\156\145\170","\154\145\166\145\154\163\057\164\145\154\145\160\157\162\164\055\141\156\156\145\170\056\154\145\166",11869187ULL,bsl::CheckpointKind::permanent,20,72}};return defs;}
inline unsigned compiled_checkpoint_death_ticks(){return 45;}
