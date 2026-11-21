// Coordinated lab-data persistence; world adapters are a separate gate. Signed: Atlas.
#pragma once
#include "events.hpp"
#include <filesystem>

namespace bsl {
struct ParticipantData {std::string id,character;int health=0;};
struct SessionData {
 std::string mode,level,content;
 std::vector<ParticipantData> participants;
 InventoryState inventory;
 EventState events;
 std::uint64_t request_id=0,conversation_id=0;
 double calendar_seconds=0;
};
class SessionDataStore {
public:
 static void validate(const Catalog&,const SessionData&);
 // Both live services retain their references; their prepared states swap without allocation.
 static void restore_services(const Catalog&,const SessionData&,Inventory&,Events&);
 static std::string encode(const Catalog&,const SessionData&);
 static SessionData decode(const Catalog&,const std::string&);
 static SessionData read(const Catalog&,const std::filesystem::path&);
 // One cooperating writer, atomic primary replacement, previous-good backup; unreadable primaries preserved.
 static void write(const Catalog&,const std::filesystem::path&,const SessionData&);
 static void recover(const Catalog&,const std::filesystem::path&);
};
}
