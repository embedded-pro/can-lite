#include "can-lite/core/CanSequenceTable.hpp"

namespace services
{
    uint8_t CanSequenceTable::Allocate(uint16_t peerNodeId)
    {
        bool created = false;
        auto& entry = EntryFor(peerNodeId, created);

        auto allocated = entry.next;
        entry.next = static_cast<uint8_t>(entry.next + 1);
        return allocated;
    }

    CanSequenceTable::ValidationResult CanSequenceTable::Validate(uint16_t peerNodeId, uint8_t sequenceNumber)
    {
        bool created = false;
        auto& entry = EntryFor(peerNodeId, created);

        if (created)
        {
            entry.next = static_cast<uint8_t>(sequenceNumber + 1);
            return ValidationResult{ true, sequenceNumber };
        }

        if (sequenceNumber != entry.next)
            return ValidationResult{ false, entry.next };

        entry.next = static_cast<uint8_t>(sequenceNumber + 1);
        return ValidationResult{ true, sequenceNumber };
    }

    void CanSequenceTable::Resync(uint16_t peerNodeId, uint8_t nextSequenceNumber)
    {
        bool created = false;
        auto& entry = EntryFor(peerNodeId, created);
        entry.next = nextSequenceNumber;
    }

    void CanSequenceTable::Forget()
    {
        for (auto& entry : peers)
            entry = PeerSequence{};

        nextEviction = 0;
    }

    CanSequenceTable::PeerSequence& CanSequenceTable::EntryFor(uint16_t peerNodeId, bool& created)
    {
        for (auto& entry : peers)
            if (entry.occupied && entry.peerNodeId == peerNodeId)
            {
                created = false;
                return entry;
            }

        for (auto& entry : peers)
            if (!entry.occupied)
            {
                entry.occupied = true;
                entry.peerNodeId = peerNodeId;
                entry.next = 0;
                created = true;
                return entry;
            }

        // The table is full. A busy bus must not abort the node, so the oldest
        // slot in round-robin order is reused; the evicted peer simply
        // renegotiates its sequence on its next message.
        auto& entry = peers[nextEviction];
        nextEviction = (nextEviction + 1) % maxPeers;

        entry.peerNodeId = peerNodeId;
        entry.next = 0;
        created = true;
        return entry;
    }
}
