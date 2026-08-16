#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace services
{
    // Sequence state for one category, tracked per peer node. Both the
    // allocating side (client) and the validating side (server) use this table,
    // so the two ends agree on what "the next sequence number" means for a given
    // (peer, category) pair.
    class CanSequenceTable
    {
    public:
        struct ValidationResult
        {
            bool accepted;
            uint8_t expected;
        };

        static constexpr std::size_t maxPeers = 8;

        uint8_t Allocate(uint16_t peerNodeId);
        ValidationResult Validate(uint16_t peerNodeId, uint8_t sequenceNumber);
        void Resync(uint16_t peerNodeId, uint8_t nextSequenceNumber);
        void Forget();

    private:
        struct PeerSequence
        {
            uint16_t peerNodeId;
            uint8_t next;
            bool occupied;
        };

        PeerSequence& EntryFor(uint16_t peerNodeId, bool& created);

        std::array<PeerSequence, maxPeers> peers{};
        std::size_t nextEviction = 0;
    };
}
