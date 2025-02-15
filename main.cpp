#include "RtMidi.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <stdio.h>
#include <string>
#include <random>
uint getInPort(std::string str);
uint getOutPort(std::string str);
void sequencer();
int limit(int v, int min, int max);
int random(int min, int max);
void refreshSequence();
void onMIDI(double deltatime, std::vector<unsigned char> *message, void * /*userData*/); // handles incomind midi

const int MIDI_CLOCK_TICKS_PER_BEAT = 24; // MIDI-Clock-Signale pro Viertelnote
const int BPM = 120;                      // Beats per Minute
const int TICK_INTERVAL_MS = 60000 / (BPM * MIDI_CLOCK_TICKS_PER_BEAT);
bool extClock = true;
bool isRunning = false;
int tickCount = 0;
int stepCount = 0;
int maxSteps = 16;
int clockDiv = 6;
int gateLength = 1;
int midiChannelOut = 0;
int volMin = 99;
int volMax = 127;
bool NoteONisActive = false;

int sequence[16]{60, 127, 127, 127, 60, 127, 127, 127, 60, 127, 127, 127, 60, 127, 127, 127};
int sequenceBuffer[16]{60, 127, 127, 127, 60, 127, 127, 127, 60, 127, 127, 127, 60, 127, 127, 127};

#define MAX_TRACKS 12
#define MAX_STEPS 16

RtMidiIn *midiIn = 0;
RtMidiOut *midiOut = 0;
void sendNote(unsigned char type, unsigned char ch, unsigned char note, unsigned char vel)
{
    std::vector<unsigned char> messageOut;
    messageOut.push_back(type + ch);
    messageOut.push_back(note);
    messageOut.push_back(vel);
    midiOut->sendMessage(&messageOut);
}
void midiClock(RtMidiOut *midiOut)
{
    auto lastTick = std::chrono::steady_clock::now();

    char keypressed;
    while (!extClock)
    {

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() >= TICK_INTERVAL_MS)
        {
            sequencer();
            tickCount = (tickCount + 1) % 96;
            isRunning = true;
            // message.push_back(0xFA);
            // midiOut->sendMessage(&message);

            if (tickCount == 0)
            {
                std::cout << "A" << std::flush;
                refreshSequence();
            }
            else if (tickCount % 24 == 0)
                std::cout << "*" << std::flush;

            lastTick = now;
        }
        // std::this_thread::sleep_for(std::chrono::milliseconds(1)); // CPU-Last reduzieren
    }
}

int main()
{
    try
    {
        std::cout << "MIDI Clock Simulation (BPM: " << BPM << ")" << std::endl;
        std::cout << "STOP WITH STRG + C" << std::endl;
        std::cout << TICK_INTERVAL_MS << std::endl;
        midiIn = new RtMidiIn();
        midiIn->setCallback(&onMIDI);
        midiIn->ignoreTypes(false, false, false); // dont ignore clocK
        midiOut = new RtMidiOut();
        // normal midi
        midiIn->openVirtualPort("PotSeq");
        midiOut->openVirtualPort("PotSeq");

        while (true)
        {
            midiClock(midiOut);
        }
        delete midiOut; // Speicher freigeben
    }
    catch (RtMidiError &error)
    {
        error.printMessage();
        return 1;
    }
    return 0;
}

void onMIDI(double deltatime, std::vector<unsigned char> *message, void * /*userData*/) // handles incomind midi
{

    unsigned char statusByte = message->at(0);         // Erstes Byte enthält Status + Kanal
    unsigned char midiCommand = statusByte & 0xF0;     // Die oberen 4 Bits sind der Befehl
    unsigned char midiChannelIn = (statusByte & 0x0F); // Die unteren 4 Bits sind der Kanal (1-basiert)

    uint size = message->size();

    if (statusByte == 0xF8) // system realtime message MIDI CLOCK
    {

        tickCount = (tickCount + 1) % 96;
        stepCount = (tickCount / clockDiv) % maxSteps;
        extClock = true;
        sequencer();
    }
    if (statusByte == 0xFA) // system realtime message START
    {
        std::cout << "MIDI START" << std::endl;
        tickCount = -1;
        stepCount = 0;
        // refreshSequence();
    }

    if (midiCommand == 0x90) // note on
    {
        std::cout << "recieve Note ON" << std::endl;

        return;
    }

    if (midiCommand == 0xB0) // cc message
    {

        /*

        */
        unsigned char midiCC = (int)message->at(1);
        unsigned char midiVAL = (int)message->at(2);
        unsigned char stepToSet = midiCC - 100;
        // Set Note Values

        if (midiCC >= 100 && midiCC < 116)
        {
            sequenceBuffer[stepToSet] = limit(midiVAL, 0, 127);
            std::cout << "Set Note : " << sequenceBuffer[stepToSet] << " on Step: " << stepToSet << std::endl;
            return;
        }
        if (midiCC == 116)
        {
            clockDiv = limit(midiVAL, 1, 95);
            std::cout << "Set clock Division to: " << clockDiv << std::endl;

            return;
        }
        if (midiCC == 117)
        {
            gateLength = limit(midiVAL, 1, (96 / maxSteps) - 1);
            std::cout << "Set clock Division to: " << gateLength << std::endl;

            return;
        }
        if (midiCC == 118)
        {
            maxSteps = limit(midiVAL, 1, 16);
            std::cout << "Set max Steps to: " << maxSteps << std::endl;

            return;
        }
        if (midiCC == 119)
        {
            midiChannelOut = limit(midiVAL, 0, 15);
            std::cout << "Set midiChannelOut to: " << midiChannelOut << std::endl;
            return;
        }
        if (midiCC == 25)
        {
            volMin = limit(midiVAL, 0, volMax);
            std::cout << "Set volMin to: " << volMin << std::endl;
            return;
        }
        if (midiCC == 26)
        {
            volMax = limit(midiVAL, volMin, 127);
            std::cout << "Set volMax to: " << volMax << std::endl;
            return;
        }
    }
}



void refreshSequence()
{
    for (int i = 0; i < MAX_STEPS; i++)
    {
        sequence[i] = sequenceBuffer[i];
        // std::cout << "Step " << i << " = " << sequence[i] << std::endl;
    }
}
void sequencer()
{
    if (stepCount == 0)
    {
        refreshSequence();
    }
    // note ON
    if (tickCount % clockDiv == 0 && sequence[stepCount] < 127 && !NoteONisActive)
    {
        int volSend = random(volMin, volMax);
        sendNote(0x90, midiChannelOut, sequence[stepCount], volSend);
        NoteONisActive = true;
    }
    // note Off
    if (tickCount % clockDiv == gateLength && NoteONisActive)
    {
        sendNote(0x80, midiChannelOut, sequence[stepCount], 0);
        NoteONisActive = false;
    }
}

int limit(int v, int min, int max)
{
    if (v < min)
        v = min;
    if (v > max)
        v = max;
    return v;
}
int random(int min, int max)
{
    // Zufallszahlengenerator und Verteilung
    std::random_device rd;                         // Zufallsquelle
    std::mt19937 gen(rd());                        // Mersenne-Twister-Generator
    std::uniform_int_distribution<> dis(min, max); // Gleichverteilte Zufallszahlen im Bereich [min, max]

    return dis(gen); // Zufallszahl im gewünschten Bereich
}