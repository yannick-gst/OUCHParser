#pragma once

#include <cstdint>
#include <fstream>
#include <unordered_map>

struct Totals {
    uint64_t accepted = 0;
    uint64_t canceled = 0;
    uint64_t executed = 0;
    uint64_t executedShares = 0;
    uint64_t replaced = 0;
    uint64_t sysEvent = 0;
    void print();
};

struct StreamDetails {
    uint64_t id;
    uint64_t accepted = 0;
    uint64_t canceled = 0;
    uint64_t executed = 0;
    uint64_t executedShares = 0;
    uint64_t replaced = 0;
    uint64_t sysEvent = 0;

    void addMessage(char type);

    void addExecutedShares(uint32_t shares) {
        executedShares += shares;
    }

    void print();
};

struct PacketState {
    uint32_t bytesRead = 0;
    char type;
};

using Packets = std::unordered_map<uint16_t, StreamDetails>;
using PartialPackets = std::unordered_map<uint16_t, PacketState>;

void readPacketCaptureHeader(
    uint16_t& streamID,
    uint32_t& packetSize,
    std::ifstream& ifs
);

void readPackets(std::ifstream& ifs);

void readPacket(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
);

void readPacketPartial(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
);

void readExecuted(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
);

void readExecutedPartial(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
);

void printOutput();

/*
void readPackets(std::ifstream& ifs);

void readPacket(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    std::unordered_map<uint16_t, StreamDetails>& streamDetails
);

void readMessage(
    StreamDetails& streamDetails,
    char type
);

void readExecuted(
    StreamDetails& streamDetails,
    std::ifstream& ifs,
    char type,
    uint32_t packetSize
);
*/
