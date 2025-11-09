#include "midi.h"
#include <istream>
#include <format>
#include <span>


#define EXPECTS_ENOUGH_DATA(required_size, data_size, position)\
if (position > data_size || required_size > data_size - position) {\
    size_t actual_size = position >= data_size ? 0 : data_size - position;\
    throw std::runtime_error(std::format("Not enough data! Expected {}, got {}", required_size, actual_size));\
}

#define EXPECTS_EQUAL(expected, actual)\
if (expected != expected) {\
    throw std::runtime_error(std::format("Expected '{}', got '{}'", expected, actual));\
}

std::string ReadString(std::span<uint8_t const> data, size_t& pos, int nbytes) {
    EXPECTS_ENOUGH_DATA(nbytes, data.size(), pos)
    std::string buffer(reinterpret_cast<char const*>(data.data() + pos), nbytes);
    pos += nbytes;
    return buffer;
}

uint32_t ReadUint32(std::span<uint8_t const> data, size_t& pos) {
    EXPECTS_ENOUGH_DATA(4, data.size(), pos)
    uint32_t value = (uint32_t(data[pos]) << 24) |
                     (uint32_t(data[pos + 1]) << 16) |
                     (uint32_t(data[pos + 2]) << 8) |
                     (uint32_t(data[pos + 3]));
    pos += 4;
    return value;
}

uint16_t ReadUint16(std::span<uint8_t const> data, size_t& pos) {
    EXPECTS_ENOUGH_DATA(2, data.size(), pos)
    uint16_t value = (uint16_t(data[pos]) << 8) | uint16_t(data[pos + 1]);
    pos += 2;
    return value;
}

uint8_t ReadUint8(std::span<uint8_t const> data, size_t& pos) {
    EXPECTS_ENOUGH_DATA(1, data.size(), pos)
    uint8_t value = data[pos];
    pos += 1;
    return value;
}

uint32_t ReadVariableLengthQuantity(std::span<uint8_t const> data, size_t& pos) {
    uint32_t value = 0;
    uint8_t byte = 0;
    int nread = 0;
    do {
        byte = ReadUint8(data, pos);
        value = (value << 7) | (byte & 0x7f);
        if (nread) {
            throw std::runtime_error("Variable length quantity should be max 4 bytes.");
        }
    } while (byte & 0x80);
    return value;
}

Midi LoadMidi(std::span<uint8_t const> data)
{
    Midi midi;
    size_t pos = 0;
    std::string identifier = ReadString(data, pos, 4);
    EXPECTS_EQUAL(std::string("MThd"), identifier)
    uint32_t chunklen = ReadUint32(data, pos);
    midi.format = ReadUint16(data, pos);
    midi.ntracks = ReadUint16(data, pos);
    midi.tickdiv = ReadUint16(data, pos);

    for (int itrack = 0; itrack < midi.ntracks; itrack++) {
        std::string identifier = ReadString(data, pos, 4);
        EXPECTS_EQUAL(std::string("MTrk"), identifier)
        uint32_t chunklen = ReadUint32(data, pos);
        Track& track = midi.tracks.emplace_back();
        int ticks = 0;
        uint8_t current_status = 0;
        size_t start_pos = pos;
        while (pos < start_pos + chunklen) {
            uint32_t delta_time = ReadVariableLengthQuantity(data, pos);
            ticks += delta_time;
            uint8_t status = ReadUint8(data, pos);
            if (status < 0x80) { // Running status
                status = current_status;
                pos--; // Unread byte
            } else {
                current_status = status;
            }
            if (status == 0xff) { // Meta event
                uint8_t msg = ReadUint8(data, pos);
                uint32_t length = ReadVariableLengthQuantity(data, pos);
                if (msg == 0x03) { // Sequence/Track name
                    std::string track_or_seq_name = ReadString(data, pos, length);
                    if (midi.format < 2 && itrack == 0) {
                        midi.sequence_name = std::move(track_or_seq_name);
                    }
                    else {
                        track.name = std::move(track_or_seq_name);
                    }
                }
                else { // Skip data
                    pos += length;
                }
            }
            else if (status == 0xf0) { // SysEx event
                uint32_t length = ReadVariableLengthQuantity(data, pos);
                pos += length;
            }
            else if (status == 0xf7) { // SysEx event
                uint32_t length = ReadVariableLengthQuantity(data, pos);
                pos += length;
            }
            else if ((status & 0xf0) >= 0x80) { // MIDI event
                uint8_t channel = status & 0x0f;
                uint8_t message = (status & 0xf0) >> 4;
                uint32_t length = (message >= 0xc && message < 0xe) ? 1 : 2;
                if (message == 0x9) { // Note On
                    uint8_t note = ReadUint8(data, pos);
                    uint8_t velocity = ReadUint8(data, pos);
                    Event& event = track.events.emplace_back();
                    event.channel = channel;
                    event.start_ticks = ticks;
                    event.note = note;
                    event.velocity = velocity;
                }
                else {
                    pos += length;
                }
            }
        }
    }
    return midi;
}
