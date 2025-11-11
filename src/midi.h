#include <cstdint>
#include <span>
#include <string>
#include <vector>

#define MIDI_NOTE_MAX 127
#define MIDI_NOTE_MIN 0
#define MIDI_NOTE_DEF 64

struct Event {
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    int start_ticks;
};

struct Track {
    std::string name;
    std::vector<Event> events;
};

struct Midi {
    int16_t format;
    int16_t ntracks;
    int16_t tickdiv;
    int32_t ticklen;
    std::string sequence_name;
    std::vector<Track> tracks;
};

Midi LoadMidi(std::span<uint8_t const> data);