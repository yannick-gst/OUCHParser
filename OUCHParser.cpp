#include "OUCHParser.h"

#include <iostream>
#include <arpa/inet.h>

std::unordered_map<char, uint32_t> expectedPacketSizes {
    {'S', 13}, {'A', 68}, {'U', 82}, {'E', 43}, {'C', 31}
};

void StreamDetails::addMessage(char type) {
    switch (type) {
        case 'A':
            ++accepted;
            break;
        case 'C':
            ++canceled;
            break;
        case 'E':
            ++executed;
            break;
        case 'U':
            ++replaced;
            break;
        case 'S':
            ++sysEvent;
            break;
        default:
            break;
    }
}

void StreamDetails::print() {
    std::cout << "Stream " << id << std::endl;
    std::cout << " Accepted: " << accepted << " messages" << std::endl;
    std::cout << " System Event: " << sysEvent << " messages" << std::endl;
    std::cout << " Replaced: " << replaced << " messages" << std::endl;
    std::cout << " Canceled: " << canceled << " messages" << std::endl;

    std::cout << " Executed: " << executed << " messages: "
              << executedShares << " executed shares"<< std::endl << std::endl;
}

void Totals::print() {
    std::cout << "Totals: " << std::endl;
    std::cout << " Accepted: " << accepted << " messages" << std::endl;
    std::cout << " System Event: " << sysEvent << " messages" << std::endl;
    std::cout << " Replaced: " << replaced << " messages" << std::endl;
    std::cout << " Canceled: " << canceled << " messages" << std::endl;

    std::cout << " Executed: " << executed << " messages: "
              << executedShares << " executed shares"<< std::endl;
}

Packets packets;
PartialPackets partialPackets;

void readPackets(std::ifstream& ifs) {
    uint16_t streamID;
    uint32_t packetSize;

    readPacketCaptureHeader(streamID, packetSize, ifs);

    readPacket(
        ifs,
        streamID,
        packetSize,
        ::packets,
        ::partialPackets
    );
}

void readPacketCaptureHeader(
    uint16_t& streamID,
    uint32_t& packetSize,
    std::ifstream& ifs
) {
    ifs.read(reinterpret_cast<char*>(&streamID), 2);
    ifs.read(reinterpret_cast<char*>(&packetSize), 4);

    // Network byte order to host byter order coonversions
    streamID = ntohs(streamID);
    packetSize = ntohl(packetSize);
}

void readPacket(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
) {
    // Partial packets need extra logic.
    if (partialPackets.find(streamID) != partialPackets.end()) {
        readPacketPartial(
            ifs,
            streamID,
            packetSize,
            packets,
            partialPackets
        );
        return;
    }

    // Ensure we have an entry to capture the details
    // of the packets for this stream.
    if (packets.find(streamID) == packets.end()) {
        packets[streamID] = StreamDetails{};
        packets[streamID].id = streamID;
    }

    if (packetSize <= 3) {
        // If the packet does not include the message type,
        // create a partial packet entry.
        partialPackets[streamID] = { packetSize, 'p'};
        ifs.seekg(packetSize, std::ios::cur);
    } else {
        // Read the message type.
        ifs.seekg(3, std::ios::cur);
        char type;
        ifs.read(&type, 1);
        packets[streamID].addMessage(type);

        if (type == 'E') {
            // Executed messages need special processing
            // because of the executed shares field.
            readExecuted(
                ifs,
                streamID,
                packetSize,
                packets,
                partialPackets
            );
        } else {
            // We've already read:
            // message length + packet type + message type = 4 bytes.
            ifs.seekg(packetSize - 4, std::ios::cur);

            // If the packet size is not the expected one
            // for this message type, it means we have a
            // partial packet. The logic in readPacketPartial()
            // will ensure that we don't call addNewMessage() on the
            // packet entry again.
            if (packetSize != expectedPacketSizes[type]) {
                partialPackets[streamID] = { packetSize, type};
            }
        }
    }
}

void readExecuted(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
) {
    if (packetSize > 26) {
        // If we are here, it means that we have the
        // executed shares field. We need to skip:
        // timestamp + order token = 22 bytes to reach
        // the executed shares field.
        ifs.seekg(22, std::ios::cur);
        uint32_t shares;
        ifs.read(reinterpret_cast<char*>(&shares), 4);
        shares = ntohl(shares); // Network to host byte order conversion
        packets[streamID].addExecutedShares(shares);

        // If we don't have the full message, create
        // a partial packet entry.
        if (packetSize != expectedPacketSizes['E']) {
            // We've already read:
            // message length + packet type + message type +
            // timestamp + order token +
            // executed shares = 30 bytes.
            ifs.seekg(packetSize - 30, std::ios::cur);
            partialPackets[streamID] = { packetSize, 'E'};
        }
    } else {
        // We've already read:
        // message length + packet type + message type = 4 bytes.
        // We do not have the executed shares field, so this is a
        // partial packet.
        ifs.seekg(packetSize - 4, std::ios::cur);
        partialPackets[streamID] = { packetSize, 'E'};
    }
}

void readPacketPartial(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
) {
    auto& partialPacket = partialPackets[streamID];

    // We first need to check for the case where the
    // message type has yet to be read from the partial
    // packet.
    if (partialPacket.bytesRead <= 3) { // Message length + packet type = 3 bytes.
        if (partialPacket.bytesRead + packetSize <= 3) {
            // Adding the new bytes to the partial packet will
            // not give us the message type. It is safe to advance,
            // we will get the message type on subsequent read.
            ifs.seekg(packetSize, std::ios::cur);
            partialPacket.bytesRead += packetSize;
            return;
        }

        // We can read the message type from the partial packet.
        // We first need to rewind to where the message type is
        // located.
        ifs.seekg(3 - partialPacket.bytesRead, std::ios::cur);
        char type;
        ifs.read(&type, 1);
        packets[streamID].addMessage(type);
        partialPacket.type = type;

        if (type == 'E') {
            readExecutedPartial(
                ifs,
                streamID,
                packetSize,
                packets,
                partialPackets
            );
        } else {
            // We've read:
            // Message length + packet type + message type = 4 bytes.
            ifs.seekg(
                partialPacket.bytesRead + packetSize - 4,
                std::ios::cur
            );

            auto type = partialPacket.type;

            if (partialPacket.bytesRead == expectedPacketSizes[type]) {
                // We don't need the partial packet
                // anymore if we've read the full message.
                partialPackets.erase(streamID);
            } else {
                partialPacket.bytesRead += packetSize;
            }
        }
    } else { // We have read the message type already
        if (partialPacket.type == 'E') {
            readExecutedPartial(
                ifs,
                streamID,
                packetSize,
                packets,
                partialPackets
            );
        } else {
            ifs.seekg(packetSize, std::ios::cur);

            if (
                partialPacket.bytesRead + packetSize ==
                    expectedPacketSizes[partialPacket.type]
            ) {
                partialPackets.erase(streamID);
            } else {
                partialPacket.bytesRead += packetSize;
            }
        }
    }
}

void readExecutedPartial(
    std::ifstream& ifs,
    uint16_t streamID,
    uint32_t packetSize,
    Packets& packets,
    PartialPackets& partialPackets
) {
    auto& partialPacket = partialPackets[streamID];

    if (partialPacket.bytesRead + packetSize > 26) {
        // If we are here, it means that we have the
        // executed shares field. We need to skip:
        // timestamp + order token = 22 bytes to reach
        // the executed shares field.
        ifs.seekg(22, std::ios::cur);
        uint32_t shares;
        ifs.read(reinterpret_cast<char*>(&shares), 4);
        packets[streamID].addExecutedShares(shares);

        // We've already read:
        // message length + packet type + message type +
        // timestamp + order token + executed shares = 30 bytes.
        ifs.seekg(partialPacket.bytesRead + packetSize - 30);

        if (partialPacket.bytesRead + packetSize == expectedPacketSizes['E']) {
            partialPackets.erase(streamID);
        } else {
            partialPacket.bytesRead += packetSize;
        }
    } else {
        partialPacket.bytesRead += packetSize;
    }
}

void printOutput() {
    Totals totals;

    for (auto[k,v]: ::packets) {
        totals.accepted += v.accepted;
        totals.canceled += v.canceled;
        totals.executed += v.executed;
        totals.executedShares += v.executedShares;
        totals.sysEvent += v.sysEvent;
        v.print();
    }

    totals.print();
}
