// ============================================================
//  Seqnal by Questyn  v7.4
// ============================================================
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <thread>
#include <atomic>
#include <map>
#include <random>
#include <filesystem>
#include <cstdint>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#include <windows.h>
#include <commdlg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

static const CLSID CLSID_MMDeviceEnumerator_ = {0xBCDE0395,0xE52F,0x467C,{0x8E,0x3D,0xC4,0x57,0x92,0x91,0x69,0x2E}};
static const IID   IID_IMMDeviceEnumerator_  = {0xA95664D2,0x9614,0x4F35,{0xA7,0x46,0xDE,0x8D,0xB6,0x36,0x17,0xE6}};
static const IID   IID_IAudioClient_         = {0x1CB9AD4C,0xDBFA,0x4C32,{0xB1,0x78,0xC2,0xF5,0x68,0xA7,0x03,0xB2}};
static const IID   IID_IAudioRenderClient_   = {0xF294ACFC,0x3146,0x4483,{0xA7,0xBF,0xAD,0xDC,0xA7,0xC2,0x60,0xE2}};

namespace fs = std::filesystem;
HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
HANDLE hStdIn  = GetStdHandle(STD_INPUT_HANDLE);

BOOL WINAPI ConsoleHandler(DWORD signal) { if (signal == CTRL_C_EVENT) return TRUE; return FALSE; }

void setConsoleFont() {
    CONSOLE_FONT_INFOEX cfi{};
    cfi.cbSize = sizeof(cfi);
    cfi.dwFontSize.Y = 16;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcsncpy(cfi.FaceName, L"Consolas", LF_FACESIZE - 1);
    SetCurrentConsoleFontEx(hStdOut, FALSE, &cfi);
}
void setConsoleSize(int w, int h) {
    SMALL_RECT ws = { 0, 0, (SHORT)(w - 1), (SHORT)(h - 1) };
    SetConsoleWindowInfo(hStdOut, TRUE, &ws);
    COORD bs = { (SHORT)w, (SHORT)h };
    SetConsoleScreenBufferSize(hStdOut, bs);
}
void applyWindowOpacity(int pct) {
    HWND hwnd = GetConsoleWindow();
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex | WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd, 0, (BYTE)(pct * 255 / 100), LWA_ALPHA);
}

std::vector<unsigned char> compressData(const std::vector<unsigned char>& data) {
    std::vector<unsigned char> out;
    size_t i = 0;
    while (i < data.size()) {
        unsigned char b = data[i];
        size_t run = 1;
        while (i + run < data.size() && data[i + run] == b && run < 255) run++;
        if (run >= 4) { out.push_back(0xFF); out.push_back((unsigned char)run); out.push_back(b); }
        else out.push_back(b);
        i += run;
    }
    return out;
}
std::vector<unsigned char> decompressData(const std::vector<unsigned char>& data, size_t originalSize) {
    std::vector<unsigned char> out;
    out.reserve(originalSize);
    size_t i = 0;
    while (i < data.size()) {
        if (data[i] == 0xFF && i + 2 < data.size()) {
            unsigned char count = data[i+1], val = data[i+2];
            for (int k = 0; k < count; k++) out.push_back(val);
            i += 3;
        } else { out.push_back(data[i]); i++; }
    }
    return out;
}

const std::string B64C = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string b64_enc(const unsigned char* d, unsigned int n) {
    std::string r; int i = 0, j = 0; unsigned char a3[3], a4[4];
    while (n--) {
        a3[i++] = *(d++);
        if (i == 3) {
            a4[0] = (a3[0] & 0xfc) >> 2; a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
            a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6); a4[3] = a3[2] & 0x3f;
            for (i = 0; i < 4; i++) r += B64C[a4[i]];
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 3; j++) a3[j] = '\0';
        a4[0] = (a3[0] & 0xfc) >> 2; a4[1] = ((a3[0] & 0x03) << 4) + ((a3[1] & 0xf0) >> 4);
        a4[2] = ((a3[1] & 0x0f) << 2) + ((a3[2] & 0xc0) >> 6); a4[3] = a3[2] & 0x3f;
        for (j = 0; j < i + 1; j++) r += B64C[a4[j]];
        while (i++ < 3) r += '=';
    }
    return r;
}
std::vector<unsigned char> b64_dec(const std::string& s) {
    int n = (int)s.size(), i = 0, j = 0, k = 0;
    unsigned char a4[4], a3[3];
    std::vector<unsigned char> r;
    while (n-- && s[k] != '=' && (isalnum((unsigned char)s[k]) || s[k]=='+' || s[k]=='/')) {
        a4[i++] = s[k++];
        if (i == 4) {
            for (i = 0; i < 4; i++) a4[i] = (unsigned char)B64C.find((char)a4[i]);
            a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4); a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2); a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
            for (i = 0; i < 3; i++) r.push_back(a3[i]);
            i = 0;
        }
    }
    if (i) {
        for (j = i; j < 4; j++) a4[j] = 0;
        for (j = 0; j < 4; j++) a4[j] = (unsigned char)B64C.find((char)a4[j]);
        a3[0] = (a4[0] << 2) + ((a4[1] & 0x30) >> 4); a3[1] = ((a4[1] & 0xf) << 4) + ((a4[2] & 0x3c) >> 2); a3[2] = ((a4[2] & 0x3) << 6) + a4[3];
        for (j = 0; j < i - 1; j++) r.push_back(a3[j]);
    }
    return r;
}

struct ConsoleBuffer {
    std::vector<CHAR_INFO> buffer;
    int width, height;
    ConsoleBuffer(int w, int h) : width(w), height(h), buffer(w * h) {}
    void clear(WORD c = 0) { for (auto& i : buffer) { i.Char.AsciiChar = ' '; i.Attributes = c; } }
    void setChar(int x, int y, char ch, WORD c) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y * width + x].Char.AsciiChar = ch; buffer[y * width + x].Attributes = c;
        }
    }
    void print(int x, int y, const std::string& s, WORD c) {
        for (size_t i = 0; i < s.length(); ++i) setChar(x + (int)i, y, s[i], c);
    }
    void fillRect(int x, int y, int w, int h, WORD c) {
        for (int j = 0; j < h; ++j) for (int i = 0; i < w; ++i) setChar(x + i, y + j, ' ', c);
    }
    void flush() {
        COORD sz = { (SHORT)width, (SHORT)height }, pos = { 0, 0 };
        SMALL_RECT rc = { 0, 0, (SHORT)(width - 1), (SHORT)(height - 1) };
        WriteConsoleOutputW(hStdOut, buffer.data(), sz, pos, &rc);
    }
};

struct SampleData { std::vector<float> data; int sampleRate = 44100; bool loaded = false; size_t start = 0, end = 0; bool reversed = false; };
const int NUM_TRACKS = 16;
SampleData globalSamples[NUM_TRACKS];

bool loadWav(const std::string& fn, SampleData& s) {
    ma_decoder dec; ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
    if (ma_decoder_init_file(fn.c_str(), &cfg, &dec) != MA_SUCCESS) return false;
    ma_uint64 total = 0; ma_decoder_get_length_in_pcm_frames(&dec, &total);
    s.data.resize(total); ma_uint64 rd = 0;
    ma_decoder_read_pcm_frames(&dec, s.data.data(), total, &rd);
    if (rd != total) s.data.resize(rd);
    s.sampleRate = dec.outputSampleRate; s.loaded = true; s.start = 0; s.end = s.data.size(); s.reversed = false;
    ma_decoder_uninit(&dec); return true;
}
bool loadWavDialog(HWND hwnd, SampleData& sample, std::string& outPath) {
    OPENFILENAMEA ofn{}; char file[260] = { 0 };
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file; ofn.nMaxFile = sizeof(file);
    ofn.lpstrFilter = "WAV Audio\0*.wav\0All Files\0*.*\0";
    ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) { outPath = file; return loadWav(outPath, sample); }
    return false;
}
bool loadProjectDialog(HWND hwnd, std::string& outPath) {
    OPENFILENAMEA ofn{}; char file[260] = { 0 };
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file; ofn.nMaxFile = sizeof(file);
    ofn.lpstrFilter = "Seqnal Project\0*.sq\0All Files\0*.*\0";
    ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn) == TRUE) { outPath = file; return true; }
    return false;
}

struct SynthParams {
    int waveform = 0; int octave = 0; float tune = 0.0f; float detune = 0.0f;
    float fmAmt = 0.0f; float pwdAmt = 0.5f;
    float cutoff = 1.0f; float resonance = 0.1f; int vcfType = 0;
    float volume = 0.5f;
    float attack = 0.01f, decay = 0.1f, sustain = 0.5f, release = 0.05f;
    bool lfoOn = false; float lfoRate = 1.0f; int lfoType = 0; int lfoDest = 0; float lfoDepth = 0.3f;
    bool arpOn = false; int arpRate = 4; float arpGate = 0.5f;
    float eqLow = 0.5f, eqMid = 0.5f, eqHigh = 0.5f;
    float delaySend = 0.0f; float reverbSend = 0.0f; int reverbType = 0;
    bool saturatorOn = false; float saturatorDrive = 1.0f;
    bool distoOn = false; float distoDrive = 1.0f;
    bool chorusOn = false; float chorusRate = 0.5f; float chorusDepth = 0.5f; float chorusMix = 0.5f; float chorusWidth = 0.7f;
};

struct Envelope {
    float state = 0, level = 0;
    void trigger() { state = 1; }
    void releaseNote() { if (state != 0) state = 4; }
    float process(float sr, const SynthParams& p) {
        if (state == 1) { level += 1.0f / (sr * std::max(0.001f, p.attack)); if (level >= 1) { level = 1; state = 2; } }
        else if (state == 2) { level -= (1.0f - p.sustain) / (sr * std::max(0.001f, p.decay)); if (level <= p.sustain) { level = p.sustain; state = 3; } }
        else if (state == 3) { level = p.sustain; }
        else if (state == 4) { level -= 1.0f / (sr * std::max(0.01f, p.release)); if (level <= 0) { level = 0; state = 0; } }
        return level;
    }
};

struct VoiceState {
    float phase = 0, phaseDetune = 0, freq = 440, pitchEnv = 0, fmPhase = 0;
    Envelope env; double samplePos = 0; int clapPhase = 0;
    float eqLow = 0, eqHigh = 0; float lfoPhase = 0, arpPhase = 0;
    float vcfLastCut = -1, vcfLastRes = -1; int vcfLastType = -1;
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    float x1_2 = 0, x2_2 = 0, y1_2 = 0, y2_2 = 0; // 24dB second stage
    std::vector<float> chorusBuf; int chorusIdx = 0; float chorusPhase = 0;
    std::vector<float> delayBufL, delayBufR; int delayIdx = 0;
    struct RevComb { std::vector<float> buf; int idx = 0; float fs = 0; };
    RevComb revCombL[4], revCombR[4];
    struct RevAllpass { std::vector<float> buf; int idx = 0; };
    RevAllpass revApL[2], revApR[2];

    void resetVCF() { x1 = x2 = y1 = y2 = 0; x1_2 = x2_2 = y1_2 = y2_2 = 0; vcfLastCut = vcfLastRes = -1; vcfLastType = -1; }
    inline float vcf(float in, float cut, float res, float sr, int type) {
        if (cut != vcfLastCut || res != vcfLastRes || type != vcfLastType) {
            vcfLastCut = cut; vcfLastRes = res; vcfLastType = type;
            float fc = 80.0f + cut * 14000.0f;
            if (fc > sr * 0.45f) fc = sr * 0.45f;
            float Q = 0.7f + res * 19.3f;
            if (type == 1) Q = 0.5f + res * 8.0f;
            float w = 2.0f * 3.14159265f * fc / sr;
            float sn = std::sin(w), cs = std::cos(w);
            float alpha = sn / (2.0f * Q);
            float a0 = 1.0f + alpha;
            b0 = (1.0f - cs) * 0.5f / a0; b1 = (1.0f - cs) / a0; b2 = b0;
            a1 = -2.0f * cs / a0; a2 = (1.0f - alpha) / a0;
        }
        float out1 = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = in; y2 = y1; y1 = out1;
        if (type == 1) {
            float out2 = b0 * out1 + b1 * x1_2 + b2 * x2_2 - a1 * y1_2 - a2 * y2_2;
            x2_2 = x1_2; x1_2 = out1; y2_2 = y1_2; y1_2 = out2;
            return out2;
        }
        return out1;
    }
};

struct Step { bool active = false; int note = 60; };
enum SoundType { K909_KICK, K909_SNARE, K909_CLAP, K707_RIM, K909_TOM, OH_HAT, NOISE_CC, CYMBAL, RIDE_CYMBAL, CHORD, SHAKE, J106_BASS, J106_LEAD, J8_PAD, M1_STAB, ORGAN };

struct Track {
    std::string name; std::vector<Step> steps; SynthParams p; SoundType type = OH_HAT;
    bool enabled = true; std::string samplePath; Track() : steps(16) {}
};
struct Pattern { std::string name; std::vector<Track> tracks; };
struct FXState { int delayTime = 375; float delayFeedback = 0.4f; bool sidechain = false; float sidechainDepth = 0.5f; };

class Sequencer {
public:
    std::atomic<bool> useSamples{false};
    std::atomic<bool> clearAudio{false};
    int currTrack_ = 0;
    std::vector<Step> clipboard;

    Sequencer() {
        CreateDirectoryA("Projects", NULL); CreateDirectoryA("Tracks", NULL); CreateDirectoryA("Presets", NULL);
        for (int i = 0; i < 16; i++) { Pattern p; p.name = "Pat" + std::to_string(i+1); patterns_.push_back(p); }
        initDemo();
    }

    void initDemo() {
        for (int i = 0; i < 16; ++i) {
            auto& pt = patterns_[i];
            auto add = [&](const char* n, SoundType t, float v, float d, float r = 0.05f) {
                Track tr; tr.name = n; tr.type = t; tr.p.volume = v; tr.p.decay = d; tr.p.sustain = 0.0f; tr.p.release = r;
                pt.tracks.push_back(tr);
            };
            add("KICK",   K909_KICK,   1.00f, 0.15f); add("SNARE",  K909_SNARE,  0.22f, 0.10f, 0.04f);
            add("CLAP",   K909_CLAP,   0.20f, 0.15f); add("RIM",    K707_RIM,    0.15f, 0.03f);
            add("TOM",    K909_TOM,    0.30f, 0.20f); add("OH",     OH_HAT,      0.30f, 0.10f);
            add("CC",     NOISE_CC,    0.12f, 0.03f); add("CYMBAL", CYMBAL,      0.18f, 0.40f, 0.20f);
            add("RIDE",   RIDE_CYMBAL, 0.18f, 0.50f, 0.30f); add("CHORD", CHORD, 0.20f, 0.60f, 0.20f);
            add("SHAKE",  SHAKE,       0.15f, 0.05f); add("BASS",   J106_BASS,   0.35f, 0.10f);
            add("LEAD",   J106_LEAD,   0.25f, 0.20f); add("PAD",    J8_PAD,      0.20f, 0.80f);
            add("STAB",   M1_STAB,     0.25f, 0.05f); add("RHODE",  ORGAN,       0.25f, 0.40f);
        }
    }

    bool hasSound() {
        for (auto& t : currentPattern().tracks)
            for (auto& s : t.steps) if (s.active && t.enabled) return true;
        return false;
    }
    void newPattern() { if (cur_ < 15) { cur_++; clearPattern(); } }

    void saveProject(const std::string& fn) {
        std::stringstream ss;
        ss << "V9\n" << bpm_.load() << " " << swing_.load() << " " << cur_ << " " << useSamples.load() << "\n";
        ss << fx_.sidechain << " " << fx_.sidechainDepth << "\n";
        for (auto& p : patterns_) for (auto& t : p.tracks) {
            auto& q = t.p;
            ss << t.enabled << " " << q.waveform << " " << q.octave << " " << q.tune << " " << q.detune << " " << q.cutoff << " " << q.resonance << " " << q.vcfType << " " << q.volume << " "
               << q.fmAmt << " " << q.pwdAmt << " " << q.attack << " " << q.decay << " " << q.sustain << " " << q.release << " " << q.lfoOn << " " << q.lfoRate << " " << q.lfoType << " " << q.lfoDest << " " << q.lfoDepth << " "
               << q.arpOn << " " << q.arpRate << " " << q.arpGate << " " << q.eqLow << " " << q.eqMid << " " << q.eqHigh << " " << q.delaySend << " " << q.reverbSend << " " << q.reverbType << " "
               << q.saturatorOn << " " << q.saturatorDrive << " " << q.distoOn << " " << q.distoDrive << " " << q.chorusOn << " " << q.chorusRate << " " << q.chorusDepth << " " << q.chorusMix << " " << q.chorusWidth << " ";
            for (auto& s : t.steps) ss << s.active << " " << s.note << " ";
            ss << "\n";
        }
        ss << "SAMPLES\n";
        for (int i = 0; i < NUM_TRACKS; ++i) {
            if (globalSamples[i].loaded) {
                ss << globalSamples[i].start << " " << globalSamples[i].end << " " << globalSamples[i].reversed << "\n";
                ss << b64_enc((const unsigned char*)globalSamples[i].data.data(), (unsigned int)(globalSamples[i].data.size() * sizeof(float))) << "\n";
            } else ss << "0 0 0\n-\n";
        }
        std::string data = ss.str();
        std::vector<unsigned char> comp = compressData(std::vector<unsigned char>(data.begin(), data.end()));
        std::ofstream f("Projects\\" + fn, std::ios::binary);
        f.write((char*)comp.data(), comp.size());
    }

    void loadProject(const std::string& fn) {
        std::ifstream f(fn, std::ios::binary);
        if (!f) return;
        std::vector<unsigned char> comp((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::vector<unsigned char> decompressed = decompressData(comp, comp.size() * 4);
        std::stringstream ss(std::string(decompressed.begin(), decompressed.end()));
        std::string version; ss >> version;
        bool hasVcfType = (version == "V9");
        int tempBpm, tempSwing, tempCur, tempUseSamples;
        ss >> tempBpm >> tempSwing >> tempCur >> tempUseSamples;
        bpm_.store(tempBpm); swing_.store(tempSwing); cur_ = tempCur; useSamples.store((bool)tempUseSamples);
        ss >> fx_.sidechain >> fx_.sidechainDepth;
        for (auto& p : patterns_) for (auto& t : p.tracks) {
            auto& q = t.p;
            int enabled;
            ss >> enabled >> q.waveform >> q.octave >> q.tune >> q.detune >> q.cutoff >> q.resonance;
            if (hasVcfType) ss >> q.vcfType; else q.vcfType = 0;
            ss >> q.volume >> q.fmAmt >> q.pwdAmt >> q.attack >> q.decay >> q.sustain >> q.release >> q.lfoOn >> q.lfoRate >> q.lfoType >> q.lfoDest >> q.lfoDepth
               >> q.arpOn >> q.arpRate >> q.arpGate >> q.eqLow >> q.eqMid >> q.eqHigh >> q.delaySend >> q.reverbSend >> q.reverbType
               >> q.saturatorOn >> q.saturatorDrive >> q.distoOn >> q.distoDrive >> q.chorusOn >> q.chorusRate >> q.chorusDepth >> q.chorusMix >> q.chorusWidth;
            t.enabled = enabled;
            for (auto& s : t.steps) ss >> s.active >> s.note;
        }
        std::string tok; ss >> tok;
        if (tok == "SAMPLES") {
            for (int i = 0; i < NUM_TRACKS; ++i) {
                size_t s0, s1; bool rev; ss >> s0 >> s1 >> rev;
                std::string b64; ss >> b64;
                if (b64 != "-") {
                    auto raw = b64_dec(b64);
                    globalSamples[i].data.resize(raw.size() / sizeof(float));
                    memcpy(globalSamples[i].data.data(), raw.data(), raw.size());
                    globalSamples[i].loaded = true; globalSamples[i].start = s0; globalSamples[i].end = s1; globalSamples[i].reversed = rev;
                } else globalSamples[i].loaded = false;
            }
        }
        clearAudio = true;
    }

    void toggleStep(int t, int s) {
        if (t >= 0 && t < NUM_TRACKS && s >= 0 && s < 16) {
            auto& st = patterns_[cur_].tracks[t].steps[s];
            st.active = !st.active;
            if (!st.active) st.note = 60;
        }
    }
    void copyTrack() { clipboard = patterns_[cur_].tracks[currTrack_].steps; }
    void pasteTrack() { if (!clipboard.empty()) patterns_[cur_].tracks[currTrack_].steps = clipboard; }
    void clearTrack() { for (auto& s : patterns_[cur_].tracks[currTrack_].steps) { s.active = false; s.note = 60; } }
    void clearPattern() { for (auto& t : patterns_[cur_].tracks) { for (auto& s : t.steps) { s.active = false; s.note = 60; } t.enabled = true; } clearAudio = true; }
    void nextPattern() { if (cur_ < 15) cur_++; }
    void prevPattern() { if (cur_ > 0) cur_--; }
    void setBPM(int b) { bpm_ = std::max(20, std::min(300, b)); }
    int getBPM() const { return bpm_; }
    void setSwing(int s) { swing_ = std::max(0, std::min(100, s)); }
    int getSwing() const { return swing_; }
    void togglePlay() { playing_ = !playing_; if (!playing_) { playStep_ = -1; clearAudio = true; } }
    bool isPlaying() const { return playing_; }
    int getPlayingStep() const { return playStep_; }
    void setPlayingStep(int s) { playStep_ = s; }
    int getCurrentPatternIndex() const { return cur_; }
    Pattern& currentPattern() { return patterns_[cur_]; }
    FXState& getFX() { return fx_; }

private:
    std::vector<Pattern> patterns_;
    int cur_ = 0;
    std::atomic<int> bpm_{125}, swing_{0}, playStep_{-1};
    std::atomic<bool> playing_{false};
    FXState fx_;
};

class AudioEngine {
public:
    AudioEngine(Sequencer& s) : seq_(s) {}
    ~AudioEngine() { stop(); }
    REFERENCE_TIME bufferDuration = 500000;
    std::atomic<float> peakL{0.0f}, peakR{0.0f}, scopePhase{0.0f};
    int outCh = 2; double sr = 44100;
    int currentBufferMode = 1;

    bool init() {
        IMMDeviceEnumerator* en = nullptr;
        if (FAILED(CoCreateInstance(CLSID_MMDeviceEnumerator_, nullptr, CLSCTX_ALL, IID_IMMDeviceEnumerator_, (void**)&en))) return false;
        if (FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &device_))) { en->Release(); return false; }
        en->Release();
        if (FAILED(device_->Activate(IID_IAudioClient_, CLSCTX_ALL, nullptr, (void**)&audioClient_))) return false;
        if (FAILED(audioClient_->GetMixFormat(&mixFormat_))) return false;
        if (FAILED(audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, bufferDuration, 0, mixFormat_, nullptr))) {
            bufferDuration = 1000000;
            if (FAILED(audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, bufferDuration, 0, mixFormat_, nullptr))) return false;
        }
        hEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        audioClient_->SetEventHandle(hEvent_);
        audioClient_->GetBufferSize(&bufferFrameCount_);
        audioClient_->GetService(IID_IAudioRenderClient_, (void**)&renderClient_);
        outCh = mixFormat_->nChannels; sr = mixFormat_->nSamplesPerSec;
        for (int i = 0; i < NUM_TRACKS; ++i) {
            voices_[i].chorusBuf.assign((int)(sr * 0.06f) + 1, 0.0f);
            voices_[i].delayBufL.assign((int)(sr * 2.0f), 0.0f);
            voices_[i].delayBufR.assign((int)(sr * 2.0f), 0.0f);
            for (int j = 0; j < 4; j++) {
                voices_[i].revCombL[j].buf.assign((int)(sr * (0.0297f + j * 0.003f)), 0.0f);
                voices_[i].revCombR[j].buf.assign((int)(sr * (0.0297f + j * 0.003f)) + 23, 0.0f);
            }
            for (int j = 0; j < 2; j++) {
                voices_[i].revApL[j].buf.assign((int)(sr * (0.005f + j * 0.001f)), 0.0f);
                voices_[i].revApR[j].buf.assign((int)(sr * (0.005f + j * 0.001f)) + 23, 0.0f);
            }
        }
        return true;
    }
    void setBufferMode(int mode) {
        currentBufferMode = mode;
        REFERENCE_TIME newDur = 500000;
        if (mode == 0) newDur = 100000;
        else if (mode == 1) newDur = 500000;
        else if (mode == 2) newDur = 1000000;
        else if (mode == 3) newDur = 200000;
        bool wasRunning = running_;
        if (wasRunning) stop();
        bufferDuration = newDur;
        init();
        if (wasRunning) start();
    }
    void start() { running_ = true; audioClient_->Start(); audioThread_ = std::thread(&AudioEngine::audioLoop, this); }
    void stop() {
        running_ = false;
        if (audioThread_.joinable()) audioThread_.join();
        if (audioClient_) { audioClient_->Stop(); audioClient_->Release(); audioClient_ = nullptr; }
        if (device_) { device_->Release(); device_ = nullptr; }
        if (hEvent_) { CloseHandle(hEvent_); hEvent_ = nullptr; }
    }

    void exportToFile() {
        int fileNum = 1; std::string fn;
        while (true) { fn = "Tracks\\Track" + std::to_string(fileNum) + ".wav"; if (!fs::exists(fn)) break; fileNum++; }
        ma_encoder_config cfg = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 44100);
        ma_encoder enc;
        if (ma_encoder_init_file(fn.c_str(), &cfg, &enc) != MA_SUCCESS) return;
        VoiceState lv[NUM_TRACKS];
        for (int i = 0; i < NUM_TRACKS; ++i) {
            lv[i].chorusBuf.assign(44100 * 0.06f + 1, 0.0f);
            lv[i].delayBufL.assign(44100 * 2.0f, 0.0f); lv[i].delayBufR.assign(44100 * 2.0f, 0.0f);
            for (int j = 0; j < 4; j++) {
                lv[i].revCombL[j].buf.assign((int)(44100 * (0.0297f + j * 0.003f)), 0.0f);
                lv[i].revCombR[j].buf.assign((int)(44100 * (0.0297f + j * 0.003f)) + 23, 0.0f);
            }
            for (int j = 0; j < 2; j++) {
                lv[i].revApL[j].buf.assign((int)(44100 * (0.005f + j * 0.001f)), 0.0f);
                lv[i].revApR[j].buf.assign((int)(44100 * (0.005f + j * 0.001f)) + 23, 0.0f);
            }
        }
        int lStep = 0; float scEnv = 1.0f;
        auto& pat = seq_.currentPattern(); auto& fx = seq_.getFX();
        bool useS = seq_.useSamples; int bpm = seq_.getBPM(), swing = seq_.getSwing();
        int dSamp = (int)(44100 * (fx.delayTime / 1000.0f));
        if (dSamp >= 44100 * 2) dSamp = 44100 * 2 - 1;
        double baseDur = 60.0 / (bpm * 4.0);
        float scK = 3.0f / (float)(44100.0 * 60.0 / (double)bpm);
        std::vector<float> buf(1024 * 2, 0.0f);
        uint32_t lfsr = 12345;
        auto noise = [&]() { lfsr ^= lfsr << 13; lfsr ^= lfsr >> 17; lfsr ^= lfsr << 5; return ((float)(lfsr % 10000) / 5000.0f) - 1.0f; };
        auto wv = [](float ph, int w) { if (w == 0) return std::sin(ph); if (w == 1) return (std::sin(ph) > 0) ? 1.0f : -1.0f; return (std::fmod(ph, 6.2831853f) / 3.14159265f) - 1.0f; };

        for (int step = 0; step < 64; ++step) {
            double sF = 1.0 + (swing / 100.0) * 0.6;
            double tD = (lStep % 2 == 0) ? (baseDur * sF) : (baseDur * (2.0 - sF));
            int frames = (int)(44100 * tD);
            for (int t = 0; t < NUM_TRACKS; ++t) {
                auto& tr = pat.tracks[t];
                if (tr.enabled && tr.steps[lStep].active) {
                    lv[t].freq = 440.0f * std::pow(2.0f, (tr.steps[lStep].note - 69) / 12.0f);
                    lv[t].env.trigger(); lv[t].pitchEnv = 1.0f;
                    lv[t].samplePos = 0; lv[t].clapPhase = 0; lv[t].arpPhase = 0.0f;
                } else lv[t].env.releaseNote();
            }
            if (pat.tracks[0].enabled && pat.tracks[0].steps[lStep].active && fx.sidechain) scEnv = 1.0f - fx.sidechainDepth;
            lStep = (lStep + 1) % 16;

            for (int i = 0; i < frames; i += 1024) {
                int fr = std::min(1024, frames - i);
                float* out = buf.data();
                for (int f = 0; f < fr; ++f) {
                    float mL = 0, mR = 0;
                    if (fx.sidechain) {
                        scEnv += (1.0f - scEnv) * scK;
                        if (scEnv > 1.0f) scEnv = 1.0f;
                    } else scEnv = 1.0f;
                    for (int t = 0; t < NUM_TRACKS; ++t) {
                        auto& tr = pat.tracks[t]; if (!tr.enabled) continue;
                        auto& v = lv[t]; auto& p = tr.p;
                        float curF = v.freq * std::pow(2.0f, (p.octave * 12 + p.tune) / 12.0f);

                        if (p.arpOn && tr.steps[lStep].active) {
                            float arpRateHz = ((float)bpm / 240.0f) * std::pow(2.0f, (float)p.arpRate);
                            v.arpPhase += arpRateHz / 44100.0f;
                            if (v.arpPhase >= 1.0f) { v.arpPhase -= 1.0f; v.env.trigger(); v.samplePos = 0; v.clapPhase = 0; }
                            if (v.arpPhase > p.arpGate) v.env.releaseNote();
                        }

                        float lfoVal = 0.0f;
                        if (p.lfoOn) {
                            v.lfoPhase += p.lfoRate / 44100.0f;
                            if (v.lfoPhase >= 1.0f) v.lfoPhase -= 1.0f;
                            if (p.lfoType == 0) lfoVal = std::sin(v.lfoPhase * 6.2831853f);
                            else if (p.lfoType == 1) lfoVal = (v.lfoPhase < 0.5f) ? 1.0f : -1.0f;
                            else lfoVal = v.lfoPhase * 2.0f - 1.0f;
                            if (p.lfoDest == 0) curF *= (1.0f + lfoVal * p.lfoDepth * 0.1f);
                        }
                        float el = v.env.process(44100.0f, p);
                        if (p.lfoOn && p.lfoDest == 2) el *= (0.5f + 0.5f * (1.0f + lfoVal * p.lfoDepth));

                        float smp = 0;
                        if (useS) {
                            if (globalSamples[t].loaded) {
                                size_t sE = globalSamples[t].end == 0 ? globalSamples[t].data.size() : std::min(globalSamples[t].end, globalSamples[t].data.size());
                                size_t sS = std::min(globalSamples[t].start, sE);
                                if (v.samplePos < (double)(sE - sS)) {
                                    double rp = globalSamples[t].reversed ? (sE - 1 - v.samplePos) : (sS + v.samplePos);
                                    int i1 = (int)rp, i2 = std::min(i1 + 1, (int)globalSamples[t].data.size() - 1);
                                    float fr2 = (float)(rp - i1);
                                    smp = globalSamples[t].data[i1] * (1 - fr2) + globalSamples[t].data[i2] * fr2;
                                    float rate = curF / 261.63f * (globalSamples[t].sampleRate / 44100.0f) * (1.0f + p.detune * 0.10f);
                                    v.samplePos += rate;
                                }
                            }
                        } else if (el > 0) {
                            if (tr.type == K909_KICK) { curF = 45.0f + 150.0f * std::exp(-v.pitchEnv * 50.0f); smp = std::sin(v.phase); v.pitchEnv += 1.0f / 44100; }
                            else if (tr.type == J106_LEAD || tr.type == J106_BASS || tr.type == J8_PAD || tr.type == M1_STAB) {
                                float basePhase = v.phase;
                                float pwdWarp = (p.pwdAmt - 0.5f) * 2.0f;
                                float warpedPhase = basePhase + pwdWarp * std::sin(basePhase) * 0.8f;
                                float baseWave;
                                if (p.waveform == 1) {
                                    float duty = 0.05f + p.pwdAmt * 0.9f;
                                    baseWave = (std::fmod(basePhase, 6.2831853f) < (6.2831853f * duty)) ? 1.0f : -1.0f;
                                } else {
                                    baseWave = wv(warpedPhase, p.waveform);
                                }
                                if (p.fmAmt > 0.001f) {
                                    float mod = std::sin(v.fmPhase) * p.fmAmt * 10.0f;
                                    baseWave = wv(basePhase + mod, p.waveform);
                                }
                                if (p.detune > 0.001f) {
                                    float detunedFreq = curF * (1.0f + p.detune * 0.10f);
                                    v.phaseDetune += 6.2831853f * detunedFreq / 44100.0f;
                                    if (v.phaseDetune > 6.2831853f) v.phaseDetune -= 6.2831853f;
                                    float detunedWave;
                                    if (p.waveform == 1) {
                                        float duty = 0.05f + p.pwdAmt * 0.9f;
                                        detunedWave = (std::fmod(v.phaseDetune, 6.2831853f) < (6.2831853f * duty)) ? 1.0f : -1.0f;
                                    } else {
                                        detunedWave = wv(v.phaseDetune, p.waveform);
                                    }
                                    baseWave = baseWave * (1.0f - p.detune * 0.5f) + detunedWave * (p.detune * 0.5f);
                                }
                                smp = baseWave;
                                v.fmPhase += 6.2831853f * curF / 44100.0f;
                            }
                            else smp = noise();
                            v.phase += 6.2831853f * curF / 44100.0f;
                            if (v.phase > 6.2831853f) v.phase -= 6.2831853f;
                        }

                        float cut = p.cutoff;
                        if (p.lfoOn && p.lfoDest == 1) cut = std::max(0.0f, std::min(1.0f, cut + lfoVal * p.lfoDepth * 0.3f));
                        if (smp != 0) smp = v.vcf(smp, cut, p.resonance, 44100.0f, p.vcfType);

                        float oSL = smp * el * p.volume;
                        float oSR = oSL;

                        if (p.saturatorOn) { oSL = std::tanh(oSL * p.saturatorDrive); oSR = std::tanh(oSR * p.saturatorDrive); }
                        if (p.distoOn) {
                            float xL = oSL * p.distoDrive * 4.0f; oSL = xL / (1.0f + std::abs(xL));
                            float xR = oSR * p.distoDrive * 4.0f; oSR = xR / (1.0f + std::abs(xR));
                        }

                        v.eqLow += 0.1f * (oSL - v.eqLow);
                        float lb = v.eqLow;
                        float aboveLow = oSL - lb;
                        v.eqHigh += 0.25f * (aboveLow - v.eqHigh);
                        float mb = v.eqHigh;
                        float hb = aboveLow - mb;
                        oSL = lb * (p.eqLow * 2.0f) + mb * (p.eqMid * 2.0f) + hb * (p.eqHigh * 2.0f);
                        oSR = oSL;

                        if (p.chorusOn) {
                            int cbs = (int)v.chorusBuf.size();
                            v.chorusBuf[v.chorusIdx] = oSL;
                            v.chorusPhase += p.chorusRate / 44100.0f;
                            if (v.chorusPhase >= 1.0f) v.chorusPhase -= 1.0f;
                            float mod = std::sin(v.chorusPhase * 6.2831853f);
                            float delaySampL = (0.010f + 0.008f * (mod * 0.5f + 0.5f)) * 44100.0f * p.chorusDepth;
                            float delaySampR = (0.012f + 0.008f * (-mod * 0.5f + 0.5f)) * 44100.0f * p.chorusDepth;
                            int dLi = (v.chorusIdx - (int)delaySampL + cbs * 2) % cbs;
                            int dRi = (v.chorusIdx - (int)delaySampR + cbs * 2) % cbs;
                            float wetL = v.chorusBuf[dLi], wetR = v.chorusBuf[dRi];
                            oSL = oSL * (1.0f - p.chorusMix) + wetL * p.chorusMix;
                            oSR = oSR * (1.0f - p.chorusMix) + wetR * p.chorusMix;
                            v.chorusIdx = (v.chorusIdx + 1) % cbs;
                        }

                        if (p.delaySend > 0.001f) {
                            int dbs = (int)v.delayBufL.size();
                            int rIdx = (v.delayIdx - dSamp + dbs) % dbs;
                            float wetL = v.delayBufL[rIdx], wetR = v.delayBufR[rIdx];
                            v.delayBufL[v.delayIdx] = oSL * p.delaySend + wetL * fx.delayFeedback;
                            v.delayBufR[v.delayIdx] = oSR * p.delaySend + wetR * fx.delayFeedback;
                            oSL += wetL; oSR += wetR;
                            v.delayIdx = (v.delayIdx + 1) % dbs;
                        }

                        if (p.reverbSend > 0.001f) {
                            float rSL = 0, rSR = 0;
                            float revFb = 0.84f, revDamp = 0.2f;
                            if (p.reverbType == 0) { revFb = 0.70f; revDamp = 0.20f; }
                            else if (p.reverbType == 1) { revFb = 0.92f; revDamp = 0.10f; }
                            else if (p.reverbType == 2) { revFb = 0.60f; revDamp = 0.40f; }
                            else { revFb = 0.84f; revDamp = 0.15f; }
                            for (int j = 0; j < 4; j++) {
                                auto& cL = v.revCombL[j]; auto& cR = v.revCombR[j];
                                float outL = cL.buf[cL.idx]; cL.fs = (outL * (1.0f - revDamp)) + (cL.fs * revDamp);
                                cL.buf[cL.idx] = oSL * p.reverbSend + (cL.fs * revFb);
                                if (++cL.idx >= (int)cL.buf.size()) cL.idx = 0; rSL += outL;
                                float outR = cR.buf[cR.idx]; cR.fs = (outR * (1.0f - revDamp)) + (cR.fs * revDamp);
                                cR.buf[cR.idx] = oSR * p.reverbSend + (cR.fs * revFb);
                                if (++cR.idx >= (int)cR.buf.size()) cR.idx = 0; rSR += outR;
                            }
                            for (int j = 0; j < 2; j++) {
                                auto& apL = v.revApL[j];
                                float bL = apL.buf[apL.idx]; float oL = -rSL + bL;
                                apL.buf[apL.idx] = rSL + bL * 0.5f;
                                if (++apL.idx >= (int)apL.buf.size()) apL.idx = 0; rSL = oL;
                                auto& apR = v.revApR[j];
                                float bR = apR.buf[apR.idx]; float oR = -rSR + bR;
                                apR.buf[apR.idx] = rSR + bR * 0.5f;
                                if (++apR.idx >= (int)apR.buf.size()) apR.idx = 0; rSR = oR;
                            }
                            oSL += rSL; oSR += rSR;
                        }

                        if (fx.sidechain && t >= 1) { oSL *= scEnv; oSR *= scEnv; }
                        mL += oSL; mR += oSR;
                    }
                    mL = std::max(-0.95f, std::min(0.95f, mL)); mR = std::max(-0.95f, std::min(0.95f, mR));
                    out[f*2] = mL; out[f*2+1] = mR;
                }
                ma_encoder_write_pcm_frames(&enc, buf.data(), fr, NULL);
            }
        }
        ma_encoder_uninit(&enc);
    }

private:
    Sequencer& seq_;
    std::thread audioThread_;
    std::atomic<bool> running_{false};
    HANDLE hEvent_ = nullptr;
    IMMDevice* device_ = nullptr;
    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    WAVEFORMATEX* mixFormat_ = nullptr;
    UINT32 bufferFrameCount_ = 0;
    VoiceState voices_[NUM_TRACKS];
    int currentStep_ = 0;
    double stepTimer_ = 0;
    float sidechainEnv_ = 1.0f;
    uint32_t lfsr_ = 12345;
    bool wasPlaying_ = false;
    float scopeTime_ = 0;

    float noise() {
        lfsr_ ^= lfsr_ << 13; lfsr_ ^= lfsr_ >> 17; lfsr_ ^= lfsr_ << 5;
        return ((float)(lfsr_ % 10000) / 5000.0f) - 1.0f;
    }
    float wv(float ph, int w) {
        if (w == 0) return std::sin(ph);
        if (w == 1) return (std::sin(ph) > 0) ? 1.0f : -1.0f;
        return (std::fmod(ph, 6.2831853f) / 3.14159265f) - 1.0f;
    }

    void audioLoop() {
        while (running_) {
            bool pl = seq_.isPlaying();
            if (wasPlaying_ && !pl) seq_.clearAudio = true;
            wasPlaying_ = pl;
            if (seq_.clearAudio) {
                for (int t = 0; t < NUM_TRACKS; ++t) {
                    voices_[t].env.state = 0; voices_[t].env.level = 0; voices_[t].resetVCF();
                    std::fill(voices_[t].chorusBuf.begin(), voices_[t].chorusBuf.end(), 0);
                    std::fill(voices_[t].delayBufL.begin(), voices_[t].delayBufL.end(), 0);
                    std::fill(voices_[t].delayBufR.begin(), voices_[t].delayBufR.end(), 0);
                    for (int j = 0; j < 4; j++) {
                        std::fill(voices_[t].revCombL[j].buf.begin(), voices_[t].revCombL[j].buf.end(), 0);
                        std::fill(voices_[t].revCombR[j].buf.begin(), voices_[t].revCombR[j].buf.end(), 0);
                    }
                    for (int j = 0; j < 2; j++) {
                        std::fill(voices_[t].revApL[j].buf.begin(), voices_[t].revApL[j].buf.end(), 0);
                        std::fill(voices_[t].revApR[j].buf.begin(), voices_[t].revApR[j].buf.end(), 0);
                    }
                    voices_[t].chorusIdx = 0; voices_[t].chorusPhase = 0; voices_[t].delayIdx = 0; voices_[t].arpPhase = 0;
                }
                seq_.clearAudio = false;
            }
            if (!pl) { WaitForSingleObject(hEvent_, 100); peakL = 0; peakR = 0; continue; }
            UINT32 pad = 0; audioClient_->GetCurrentPadding(&pad);
            UINT32 avail = bufferFrameCount_ - pad;
            if (avail > 0) {
                BYTE* buf = nullptr;
                if (SUCCEEDED(renderClient_->GetBuffer(avail, &buf))) {
                    fillBuffer(buf, avail);
                    renderClient_->ReleaseBuffer(avail, 0);
                }
            }
        }
    }

    void fillBuffer(BYTE* buffer, UINT32 frames) {
        float* out = (float*)buffer;
        auto& pat = seq_.currentPattern(); auto& fx = seq_.getFX();
        bool useS = seq_.useSamples;
        float maxL = 0, maxR = 0;
        int dSamp = (int)(sr * (fx.delayTime / 1000.0f));
        if (dSamp >= (int)sr * 2) dSamp = (int)sr * 2 - 1;
        float scK = 3.0f / (float)(sr * 60.0 / (double)seq_.getBPM());

        for (UINT32 i = 0; i < frames; ++i) {
            float masterL = 0, masterR = 0;
            if (!seq_.hasSound()) {
                for (int t = 0; t < NUM_TRACKS; ++t) { voices_[t].env.state = 0; voices_[t].env.level = 0; }
            } else {
                double bDur = 60.0 / (seq_.getBPM() * 4.0);
                double sF = 1.0 + (seq_.getSwing() / 100.0) * 0.6;
                double tD = (currentStep_ % 2 == 0) ? (bDur * sF) : (bDur * (2.0 - sF));
                stepTimer_ += 1.0 / sr;
                scopeTime_ += 1.0f / (float)sr;
                scopePhase = std::fmod((float)scopeTime_ / (float)tD * 3.14159265f, 6.2831853f);
                if (stepTimer_ >= tD) {
                    stepTimer_ -= tD;
                    currentStep_ = (currentStep_ + 1) % 16;
                    seq_.setPlayingStep(currentStep_);
                    for (int t = 0; t < NUM_TRACKS; ++t) {
                        auto& tr = pat.tracks[t];
                        if (tr.enabled && tr.steps[currentStep_].active) {
                            voices_[t].freq = 440.0f * std::pow(2.0f, (tr.steps[currentStep_].note - 69) / 12.0f);
                            voices_[t].env.trigger(); voices_[t].pitchEnv = 1.0f;
                            voices_[t].samplePos = 0; voices_[t].clapPhase = 0; voices_[t].arpPhase = 0.0f;
                        } else voices_[t].env.releaseNote();
                    }
                    if (pat.tracks[0].enabled && pat.tracks[0].steps[currentStep_].active && fx.sidechain)
                        sidechainEnv_ = 1.0f - fx.sidechainDepth;
                }

                if (fx.sidechain) {
                    sidechainEnv_ += (1.0f - sidechainEnv_) * scK;
                    if (sidechainEnv_ > 1.0f) sidechainEnv_ = 1.0f;
                } else sidechainEnv_ = 1.0f;

                for (int t = 0; t < NUM_TRACKS; ++t) {
                    auto& tr = pat.tracks[t]; if (!tr.enabled) continue;
                    auto& v = voices_[t]; auto& p = tr.p;
                    float curF = v.freq * std::pow(2.0f, (p.octave * 12 + p.tune) / 12.0f);

                    if (p.arpOn && tr.steps[currentStep_].active) {
                        float arpRateHz = ((float)seq_.getBPM() / 240.0f) * std::pow(2.0f, (float)p.arpRate);
                        v.arpPhase += arpRateHz / (float)sr;
                        if (v.arpPhase >= 1.0f) { v.arpPhase -= 1.0f; v.env.trigger(); v.samplePos = 0; v.clapPhase = 0; }
                        if (v.arpPhase > p.arpGate) v.env.releaseNote();
                    }

                    float lfoVal = 0.0f;
                    if (p.lfoOn) {
                        v.lfoPhase += p.lfoRate / (float)sr;
                        if (v.lfoPhase >= 1.0f) v.lfoPhase -= 1.0f;
                        if (p.lfoType == 0) lfoVal = std::sin(v.lfoPhase * 6.2831853f);
                        else if (p.lfoType == 1) lfoVal = (v.lfoPhase < 0.5f) ? 1.0f : -1.0f;
                        else lfoVal = v.lfoPhase * 2.0f - 1.0f;
                        if (p.lfoDest == 0) curF *= (1.0f + lfoVal * p.lfoDepth * 0.1f);
                    }
                    float el = v.env.process((float)sr, p);
                    if (p.lfoOn && p.lfoDest == 2) el *= (0.5f + 0.5f * (1.0f + lfoVal * p.lfoDepth));

                    float smp = 0;
                    if (useS) {
                        if (globalSamples[t].loaded) {
                            size_t sE = globalSamples[t].end == 0 ? globalSamples[t].data.size() : std::min(globalSamples[t].end, globalSamples[t].data.size());
                            size_t sS = std::min(globalSamples[t].start, sE);
                            if (v.samplePos < (double)(sE - sS)) {
                                double rp = globalSamples[t].reversed ? (sE - 1 - v.samplePos) : (sS + v.samplePos);
                                int i1 = (int)rp, i2 = std::min(i1 + 1, (int)globalSamples[t].data.size() - 1);
                                float fr2 = (float)(rp - i1);
                                smp = globalSamples[t].data[i1] * (1 - fr2) + globalSamples[t].data[i2] * fr2;
                                float rate = curF / 261.63f * (globalSamples[t].sampleRate / (float)sr) * (1.0f + p.detune * 0.10f);
                                v.samplePos += rate;
                            }
                        }
                    } else if (el > 0) {
                        if (tr.type == K909_KICK) { curF = 45.0f + 150.0f * std::exp(-v.pitchEnv * 50.0f); smp = std::sin(v.phase); v.pitchEnv += 1.0f / (float)sr; }
                        else if (tr.type == J106_LEAD || tr.type == J106_BASS || tr.type == J8_PAD || tr.type == M1_STAB) {
                            float basePhase = v.phase;
                            float pwdWarp = (p.pwdAmt - 0.5f) * 2.0f;
                            float warpedPhase = basePhase + pwdWarp * std::sin(basePhase) * 0.8f;
                            float baseWave;
                            if (p.waveform == 1) {
                                float duty = 0.05f + p.pwdAmt * 0.9f;
                                baseWave = (std::fmod(basePhase, 6.2831853f) < (6.2831853f * duty)) ? 1.0f : -1.0f;
                            } else {
                                baseWave = wv(warpedPhase, p.waveform);
                            }
                            if (p.fmAmt > 0.001f) {
                                float mod = std::sin(v.fmPhase) * p.fmAmt * 10.0f;
                                baseWave = wv(basePhase + mod, p.waveform);
                            }
                            if (p.detune > 0.001f) {
                                float detunedFreq = curF * (1.0f + p.detune * 0.10f);
                                v.phaseDetune += 6.2831853f * detunedFreq / (float)sr;
                                if (v.phaseDetune > 6.2831853f) v.phaseDetune -= 6.2831853f;
                                float detunedWave;
                                if (p.waveform == 1) {
                                    float duty = 0.05f + p.pwdAmt * 0.9f;
                                    detunedWave = (std::fmod(v.phaseDetune, 6.2831853f) < (6.2831853f * duty)) ? 1.0f : -1.0f;
                                } else {
                                    detunedWave = wv(v.phaseDetune, p.waveform);
                                }
                                baseWave = baseWave * (1.0f - p.detune * 0.5f) + detunedWave * (p.detune * 0.5f);
                            }
                            smp = baseWave;
                            v.fmPhase += 6.2831853f * curF / (float)sr;
                        }
                        else smp = noise();
                        v.phase += 6.2831853f * curF / (float)sr; if (v.phase > 6.2831853f) v.phase -= 6.2831853f;
                    }

                    float cut = p.cutoff;
                    if (p.lfoOn && p.lfoDest == 1) cut = std::max(0.0f, std::min(1.0f, cut + lfoVal * p.lfoDepth * 0.3f));
                    if (smp != 0) smp = v.vcf(smp, cut, p.resonance, (float)sr, p.vcfType);

                    float oSL = smp * el * p.volume;
                    float oSR = oSL;

                    if (p.saturatorOn) { oSL = std::tanh(oSL * p.saturatorDrive); oSR = std::tanh(oSR * p.saturatorDrive); }
                    if (p.distoOn) {
                        float xL = oSL * p.distoDrive * 4.0f; oSL = xL / (1.0f + std::abs(xL));
                        float xR = oSR * p.distoDrive * 4.0f; oSR = xR / (1.0f + std::abs(xR));
                    }

                    v.eqLow += 0.1f * (oSL - v.eqLow);
                    float lb = v.eqLow;
                    float aboveLow = oSL - lb;
                    v.eqHigh += 0.25f * (aboveLow - v.eqHigh);
                    float mb = v.eqHigh;
                    float hb = aboveLow - mb;
                    oSL = lb * (p.eqLow * 2.0f) + mb * (p.eqMid * 2.0f) + hb * (p.eqHigh * 2.0f);
                    oSR = oSL;

                    if (p.chorusOn) {
                        int cbs = (int)v.chorusBuf.size();
                        v.chorusBuf[v.chorusIdx] = oSL;
                        v.chorusPhase += p.chorusRate / (float)sr;
                        if (v.chorusPhase >= 1.0f) v.chorusPhase -= 1.0f;
                        float mod = std::sin(v.chorusPhase * 6.2831853f);
                        float delaySampL = (0.010f + 0.008f * (mod * 0.5f + 0.5f)) * sr * p.chorusDepth;
                        float delaySampR = (0.012f + 0.008f * (-mod * 0.5f + 0.5f)) * sr * p.chorusDepth;
                        int dLi = (v.chorusIdx - (int)delaySampL + cbs * 2) % cbs;
                        int dRi = (v.chorusIdx - (int)delaySampR + cbs * 2) % cbs;
                        float wetL = v.chorusBuf[dLi], wetR = v.chorusBuf[dRi];
                        oSL = oSL * (1.0f - p.chorusMix) + wetL * p.chorusMix;
                        oSR = oSR * (1.0f - p.chorusMix) + wetR * p.chorusMix;
                        v.chorusIdx = (v.chorusIdx + 1) % cbs;
                    }

                    if (p.delaySend > 0.001f) {
                        int dbs = (int)v.delayBufL.size();
                        int rIdx = (v.delayIdx - dSamp + dbs) % dbs;
                        float wetL = v.delayBufL[rIdx], wetR = v.delayBufR[rIdx];
                        v.delayBufL[v.delayIdx] = oSL * p.delaySend + wetL * fx.delayFeedback;
                        v.delayBufR[v.delayIdx] = oSR * p.delaySend + wetR * fx.delayFeedback;
                        oSL += wetL; oSR += wetR;
                        v.delayIdx = (v.delayIdx + 1) % dbs;
                    }

                    if (p.reverbSend > 0.001f) {
                        float rSL = 0, rSR = 0;
                        float revFb = 0.84f, revDamp = 0.2f;
                        if (p.reverbType == 0) { revFb = 0.70f; revDamp = 0.20f; }
                        else if (p.reverbType == 1) { revFb = 0.92f; revDamp = 0.10f; }
                        else if (p.reverbType == 2) { revFb = 0.60f; revDamp = 0.40f; }
                        else { revFb = 0.84f; revDamp = 0.15f; }
                        for (int j = 0; j < 4; j++) {
                            auto& cL = v.revCombL[j]; auto& cR = v.revCombR[j];
                            float outL = cL.buf[cL.idx];
                            cL.fs = (outL * (1.0f - revDamp)) + (cL.fs * revDamp);
                            cL.buf[cL.idx] = oSL * p.reverbSend + (cL.fs * revFb);
                            if (++cL.idx >= (int)cL.buf.size()) cL.idx = 0; rSL += outL;
                            float outR = cR.buf[cR.idx];
                            cR.fs = (outR * (1.0f - revDamp)) + (cR.fs * revDamp);
                            cR.buf[cR.idx] = oSR * p.reverbSend + (cR.fs * revFb);
                            if (++cR.idx >= (int)cR.buf.size()) cR.idx = 0; rSR += outR;
                        }
                        for (int j = 0; j < 2; j++) {
                            auto& apL = v.revApL[j];
                            float bL = apL.buf[apL.idx]; float oL = -rSL + bL;
                            apL.buf[apL.idx] = rSL + bL * 0.5f;
                            if (++apL.idx >= (int)apL.buf.size()) apL.idx = 0; rSL = oL;
                            auto& apR = v.revApR[j];
                            float bR = apR.buf[apR.idx]; float oR = -rSR + bR;
                            apR.buf[apR.idx] = rSR + bR * 0.5f;
                            if (++apR.idx >= (int)apR.buf.size()) apR.idx = 0; rSR = oR;
                        }
                        oSL += rSL; oSR += rSR;
                    }

                    if (fx.sidechain && t >= 1) { oSL *= sidechainEnv_; oSR *= sidechainEnv_; }
                    masterL += oSL; masterR += oSR;
                }
            }
            masterL = std::max(-0.95f, std::min(0.95f, masterL));
            masterR = std::max(-0.95f, std::min(0.95f, masterR));
            if (std::abs(masterL) > maxL) maxL = std::abs(masterL);
            if (std::abs(masterR) > maxR) maxR = std::abs(masterR);
            if (outCh == 2) { out[i*2] = masterL; out[i*2+1] = masterR; } else out[i] = (masterL + masterR) * 0.5f;
        }
        peakL = std::max((float)peakL * 0.95f, maxL);
        peakR = std::max((float)peakR * 0.95f, maxR);
    }
};

enum FocusArea { GRID, MODULES };
enum SelectedModule { VCO, VCF, VCA, ENV, LFO_M, ARP_M, SATUR, CHORUS, SIDECH, EQ_M, DELAY_M, REVERB_M };
const int NUM_MODULES = 12;

class ConsoleUI {
public:
    ConsoleUI(Sequencer& s, AudioEngine& a, HWND h) : seq_(s), audio_(a), hwnd_(h) {
        width_ = 120; height_ = 45; cb = new ConsoleBuffer(width_, height_);
        opacity_ = 100;
    }
    ~ConsoleUI() { restore(); delete cb; }

    void setup() {
        SetConsoleTitleA("Seqnal by Questyn  v7.4");
        DWORD m; GetConsoleMode(hStdIn, &m);
        m &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
        m |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT;
        SetConsoleMode(hStdIn, m);
        setConsoleFont();
        CONSOLE_CURSOR_INFO ci{ 1, FALSE }; SetConsoleCursorInfo(hStdOut, &ci);
        setConsoleSize(width_, height_);
        applyWindowOpacity(opacity_);
        system("cls");
    }
    void restore() { CONSOLE_CURSOR_INFO ci{ 1, TRUE }; SetConsoleCursorInfo(hStdOut, &ci); SetConsoleTextAttribute(hStdOut, 7); }

    int maxParam(SelectedModule m) {
        switch (m) {
            case VCO: return seq_.useSamples ? 4 : 5;
            case VCF: return 2; // Type, Cut, Res
            case VCA: return 0; case ENV: return 3;
            case LFO_M: return 4; case ARP_M: return 2; case SATUR: return 3; case CHORUS: return 4;
            case SIDECH: return 1;
            case EQ_M: return 2; case DELAY_M: return 0; case REVERB_M: return 2;
        } return 0;
    }
    std::string noteLabel(int off) {
        if (off == 0) return "X";
        if (off > 0) return "X" + std::to_string(off) + "+";
        return "X" + std::to_string(-off) + "-";
    }

    void draw() {
        if (showSampler_) { drawSampler(); return; }
        cb->clear(0);
        WORD cHdr = 0x0F, cGrid = 0x0C, cText = 0x07, cDim = 0x08, cX = 0x0F;
        if (themeIdx_ == 0) { bool blink = (GetTickCount() % 1000) < 500; cHdr = blink ? 0x0B : 0x03; cGrid = blink ? 0x0B : 0x03; }
        else if (themeIdx_ == 1) { cHdr = 0x0A; cGrid = 0x0A; cText = 0x0A; cDim = 0x02; cX = 0x0F; }

        cb->print(0, 0, std::string(width_, '='), cHdr);
        cb->print(2, 1, " SEQNAL | by Questyn | v7.4 ", cHdr);
        cb->print(36, 1, seq_.isPlaying() ? "* PLAY *" : " STOPPED ", seq_.isPlaying() ? cGrid : cDim);
        std::string ms = seq_.useSamples ? "SAMPLES" : "INIT";
        cb->print(52, 1, " BPM:" + std::to_string(seq_.getBPM()) + " | Swing:" + std::to_string(seq_.getSwing()) + "% | Pat:" + std::to_string(seq_.getCurrentPatternIndex()+1) + "/16 | " + ms, cText);
        cb->print(0, 2, std::string(width_, '='), cHdr);
        cb->print(0, 3, "[Tab]Editor [H]Help [F1]ClearTrack [Shift+1/2]Copy/Paste", cDim);

        int gY = 5, gX = 15;
        for (int s = 0; s < 16; ++s) {
            int x = gX + s * 4;
            cb->print(x, gY, "|", cDim);
            cb->setChar(x+1, gY, '0' + (s+1)/10, s == seq_.getPlayingStep() ? cGrid : cDim);
            cb->setChar(x+2, gY, '0' + (s+1)%10, s == seq_.getPlayingStep() ? cGrid : cDim);
            cb->setChar(x+3, gY, '|', cDim);
        }
        auto& pat = seq_.currentPattern(); seq_.currTrack_ = cursorTrack_;
        int eY = gY + NUM_TRACKS + 3;
        if (focus_ == MODULES) cb->fillRect(0, gY + 1 + cursorTrack_, width_, 1, 0x4C);
        for (int t = 0; t < NUM_TRACKS; ++t) {
            auto& tr = pat.tracks[t]; int y = gY + 1 + t;
            bool isCur = (t == cursorTrack_ && focus_ == GRID);
            WORD nameC = !tr.enabled ? cDim : (isCur ? cHdr : cText);
            if (focus_ == MODULES && t == cursorTrack_) nameC = 0x4F;
            std::string label = " " + tr.name; label.resize(13, ' ');
            cb->print(0, y, label, nameC);
            for (int s = 0; s < 16; ++s) {
                int x = gX + s * 4;
                std::string lb = noteLabel(tr.steps[s].note - 60); while (lb.length() < 4) lb += ' ';
                WORD c; if (!tr.enabled) c = cDim; else if (t == cursorTrack_ && s == cursorStep_ && focus_ == GRID) c = (themeIdx_ == 1) ? 0x0F : cGrid; else if (tr.steps[s].active) c = (themeIdx_ == 1) ? 0x0F : cX; else c = cDim;
                for (int k = 0; k < 4; ++k) cb->setChar(x + k, y, lb[k], c);
            }
        }
        int mX = 84; cb->print(mX, gY, "MASTER", cHdr);
        float pk = std::max((float)audio_.peakL, (float)audio_.peakR); int mh = (int)(pk * 14);
        for (int i = 0; i < 14; ++i) { WORD c = (i < 9) ? cGrid : (i < 12) ? cHdr : 0x0C; cb->setChar(mX, gY + 14 - i, i < mh ? 219 : 176, i < mh ? c : cDim); }
        int sX = 92; cb->print(sX, gY, "SCOPE", cHdr);
        for (int i = 0; i < 24; ++i) { float ph = audio_.scopePhase + i * 0.3f; int v = (int)(std::sin(ph) * 6.0f); cb->setChar(sX + i, gY + 8 - v, 219, cGrid); }

        cb->print(0, eY - 1, std::string(width_, '-'), cDim);
        auto& curStep = pat.tracks[cursorTrack_].steps[cursorStep_];
        cb->print(2, eY, "EDITOR | " + pat.tracks[cursorTrack_].name + " | Step " + std::to_string(cursorStep_+1) + " | Note: " + noteLabel(curStep.note - 60), cHdr);

        const char* mods[] = {"VCO","VCF","VCA","ENV","LFO","ARP","SATUR","CHORUS","SIDECH","EQ","DELAY","REVERB"};
        int x = 2;
        for (int i = 0; i < NUM_MODULES; ++i) { std::string s = std::string("[") + mods[i] + "] "; cb->print(x, eY + 1, s, (i == selectedModule_) ? (focus_ == MODULES ? cHdr : cGrid) : cDim); x += (int)s.length(); }

        auto& p = pat.tracks[cursorTrack_].p; auto& fx = seq_.getFX(); std::string l1;
        if (selectedModule_ == VCO) { std::string o = (p.octave >= 0 ? "+" : "") + std::to_string(p.octave); std::string t = (p.tune >= 0 ? "+" : "") + std::to_string((int)p.tune); std::string d = std::to_string((int)(p.detune * 100)) + "%"; l1 = "Wave:<" + std::string(p.waveform==0?"Sine":p.waveform==1?"Squar":"Saw") + "> Oct:<" + o + "> Tune:<" + t + "> Detune:<" + d + "> FM:<" + std::to_string((int)(p.fmAmt*100)) + "> PWD:<" + std::to_string((int)(p.pwdAmt*100)) + ">"; }
        else if (selectedModule_ == VCF) { std::string tS = (p.vcfType == 0) ? "12db" : "24db"; l1 = "Type:<" + tS + ">  Cut:<" + std::to_string((int)(p.cutoff*100)) + ">  Res:<" + std::to_string((int)(p.resonance*100)) + ">"; }
        else if (selectedModule_ == VCA) l1 = "Volume:<" + std::to_string((int)(p.volume*100)) + ">";
        else if (selectedModule_ == ENV) l1 = "A:<" + std::to_string((int)(p.attack*99)) + "> D:<" + std::to_string((int)(p.decay*99)) + "> S:<" + std::to_string((int)(p.sustain*99)) + "> R:<" + std::to_string((int)(p.release*99)) + ">";
        else if (selectedModule_ == LFO_M) l1 = "On:" + std::string(p.lfoOn?"ON":"OFF") + " Rate:<" + std::to_string((int)(p.lfoRate*10)) + "> Dest:" + std::string(p.lfoDest==0?"Pitch":p.lfoDest==1?"Filter":"Amp") + " Depth:<" + std::to_string((int)(p.lfoDepth*100)) + ">";
        else if (selectedModule_ == ARP_M) l1 = "On:" + std::string(p.arpOn?"ON":"OFF") + " Rate:<1/" + std::to_string((int)std::pow(2,p.arpRate)) + "> Gate:<" + std::to_string((int)(p.arpGate*100)) + "%>";
        else if (selectedModule_ == SATUR) l1 = "Sat:" + std::string(p.saturatorOn?"ON":"OFF") + " Drive:<" + std::to_string((int)(p.saturatorDrive*10)) + "> Dist:" + std::string(p.distoOn?"ON":"OFF") + " Drive:<" + std::to_string((int)(p.distoDrive*10)) + ">";
        else if (selectedModule_ == CHORUS) l1 = "On:" + std::string(p.chorusOn?"ON":"OFF") + " Rate:<" + std::to_string((int)(p.chorusRate*10)) + "> Depth:<" + std::to_string((int)(p.chorusDepth*100)) + "> Mix:<" + std::to_string((int)(p.chorusMix*100)) + ">";
        else if (selectedModule_ == SIDECH) l1 = "On:" + std::string(fx.sidechain?"ON":"OFF") + " Depth:<" + std::to_string((int)(fx.sidechainDepth*100)) + "%>  [Root:KICK @ 1/4]";
        else if (selectedModule_ == EQ_M) l1 = "Low:<" + std::to_string((int)(p.eqLow*100)) + "> Mid:<" + std::to_string((int)(p.eqMid*100)) + "> High:<" + std::to_string((int)(p.eqHigh*100)) + ">";
        else if (selectedModule_ == DELAY_M) l1 = "Delay Send:<" + std::to_string((int)(p.delaySend*100)) + "%>";
        else if (selectedModule_ == REVERB_M) l1 = "Reverb Send:<" + std::to_string((int)(p.reverbSend*100)) + "%> Type:" + (p.reverbType==0?"ROOM":p.reverbType==1?"CATHEDRAL":p.reverbType==2?"DRUM":"HALL");

        if (focus_ == MODULES) { int lineY = eY + 2; cb->fillRect(0, lineY, width_, 1, 0x4C); cb->print(2, lineY, l1, 0x4F); }
        else cb->print(2, eY + 2, l1, cText);
        cb->print(2, eY + 3, focus_==GRID ? "WASD:Move Enter:Toggle Ctrl+W/S:Tune+/- Shift+1/2:Copy/Paste" : "N/M:Module W/S:Param Alt+W/S:+-2 Alt+A/D:+-1", cDim);
        if (showTheme_) drawThemeMenu();
        if (showBufferMenu_) drawBufferMenu();
        if (showSaveMsg_) drawSaveMsg();
        cb->flush();
    }

    void handleInput() {
        DWORD n = 0; GetNumberOfConsoleInputEvents(hStdIn, &n); if (!n) return;
        INPUT_RECORD rec; DWORD r; ReadConsoleInput(hStdIn, &rec, 1, &r);
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown) return;
        WORD vk = rec.Event.KeyEvent.wVirtualKeyCode; char ch = rec.Event.KeyEvent.uChar.AsciiChar;
        bool alt = (rec.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        bool ctrl = (rec.Event.KeyEvent.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        bool shift = (rec.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) != 0;

        if (showSaveMsg_) { showSaveMsg_ = false; return; }
        if (showTheme_) {
            if (ch == '1') { themeIdx_ = 0; showTheme_ = false; }
            else if (ch == '2') { themeIdx_ = 1; showTheme_ = false; }
            else if (ch == '5') { seq_.saveProject("project_" + std::to_string(GetTickCount()) + ".sq"); showSaveMsg_ = true; saveMsgText_ = "SAVE PROJECT"; showTheme_ = false; }
            else if (ch == '6') {
                std::string path;
                if (loadProjectDialog(hwnd_, path)) {
                    seq_.loadProject(path);
                    cursorTrack_ = 0; cursorStep_ = 0; selectedModule_ = VCO; selectedParam_ = 0; focus_ = GRID;
                    showSaveMsg_ = true; saveMsgText_ = "PROJECT LOADED"; showTheme_ = false;
                }
            }
            else if (ch == '7') { audio_.exportToFile(); showSaveMsg_ = true; saveMsgText_ = "EXPORT SONG"; showTheme_ = false; }
            else if (ch == '8') { opacity_ = std::min(100, opacity_ + 10); applyWindowOpacity(opacity_); }
            else if (ch == '9') { opacity_ = std::max(10, opacity_ - 10); applyWindowOpacity(opacity_); }
            else if (ch == '0') { showBufferMenu_ = true; showTheme_ = false; }
            else if (vk == VK_ESCAPE) showTheme_ = false;
            return;
        }
        if (showBufferMenu_) {
            if (ch == '1') { audio_.setBufferMode(0); showSaveMsg_ = true; saveMsgText_ = "BUFFER LOW"; showBufferMenu_ = false; }
            else if (ch == '2') { audio_.setBufferMode(1); showSaveMsg_ = true; saveMsgText_ = "BUFFER MEDIUM"; showBufferMenu_ = false; }
            else if (ch == '3') { audio_.setBufferMode(2); showSaveMsg_ = true; saveMsgText_ = "BUFFER HIGH"; showBufferMenu_ = false; }
            else if (ch == '4') { audio_.setBufferMode(3); showSaveMsg_ = true; saveMsgText_ = "BUFFER AUTO"; showBufferMenu_ = false; }
            else if (vk == VK_ESCAPE) showBufferMenu_ = false;
            return;
        }
        if (showSampler_) {
            auto& s = globalSamples[cursorTrack_];
            if (vk == VK_ESCAPE) { showSampler_ = false; return; }
            if (s.loaded) {
                int step = std::max(1, (int)s.data.size() / 50);
                if (alt && vk == 'W') s.start = std::max(0, (int)s.start - step);
                else if (alt && vk == 'S') s.start = std::min((int)s.end - 1, (int)s.start + step);
                else if (alt && vk == 'A') s.end = std::max((int)s.start + 1, (int)s.end - step);
                else if (alt && vk == 'D') s.end = std::min((int)s.data.size(), (int)s.end + step);
                else if (alt && vk == 'F') s.reversed = !s.reversed;
            }
            return;
        }
        if (vk == VK_ESCAPE) { showSampler_ = true; return; }
        if (ctrl && focus_ == GRID && (vk == 'W' || vk == 'S')) {
            auto& st = seq_.currentPattern().tracks[cursorTrack_].steps[cursorStep_];
            if (vk == 'W') { if (st.note < 84) st.note++; } else { if (st.note > 36) st.note--; } return;
        }
        if (shift && vk == '1') { seq_.copyTrack(); showSaveMsg_ = true; saveMsgText_ = "COPIED"; return; }
        if (shift && vk == '2') { seq_.pasteTrack(); showSaveMsg_ = true; saveMsgText_ = "PASTED"; return; }
        if (vk == VK_RETURN) { seq_.toggleStep(cursorTrack_, cursorStep_); return; }

        if (alt && vk == '1') { seq_.currentPattern().tracks[0].enabled = !seq_.currentPattern().tracks[0].enabled; return; }
        if (alt && vk == '2') { seq_.currentPattern().tracks[1].enabled = !seq_.currentPattern().tracks[1].enabled; return; }
        if (alt && vk == '3') { seq_.currentPattern().tracks[2].enabled = !seq_.currentPattern().tracks[2].enabled; return; }
        if (alt && vk == '4') { seq_.currentPattern().tracks[3].enabled = !seq_.currentPattern().tracks[3].enabled; return; }
        if (alt && vk == '5') { seq_.currentPattern().tracks[4].enabled = !seq_.currentPattern().tracks[4].enabled; return; }
        if (alt && vk == '6') { seq_.currentPattern().tracks[5].enabled = !seq_.currentPattern().tracks[5].enabled; return; }
        if (alt && vk == '7') { seq_.currentPattern().tracks[6].enabled = !seq_.currentPattern().tracks[6].enabled; return; }
        if (alt && vk == '8') { seq_.currentPattern().tracks[7].enabled = !seq_.currentPattern().tracks[7].enabled; return; }
        if (alt && vk == '9') { seq_.currentPattern().tracks[8].enabled = !seq_.currentPattern().tracks[8].enabled; return; }
        if (alt && vk == '0') { seq_.currentPattern().tracks[9].enabled = !seq_.currentPattern().tracks[9].enabled; return; }
        if (alt && vk == 'Z') { seq_.currentPattern().tracks[10].enabled = !seq_.currentPattern().tracks[10].enabled; return; }
        if (alt && vk == 'X') { seq_.currentPattern().tracks[11].enabled = !seq_.currentPattern().tracks[11].enabled; return; }
        if (alt && vk == 'C') { seq_.currentPattern().tracks[12].enabled = !seq_.currentPattern().tracks[12].enabled; return; }
        if (alt && vk == 'V') { seq_.currentPattern().tracks[13].enabled = !seq_.currentPattern().tracks[13].enabled; return; }
        if (alt && vk == 'B') { seq_.currentPattern().tracks[14].enabled = !seq_.currentPattern().tracks[14].enabled; return; }
        if (alt && vk == 'N') { seq_.currentPattern().tracks[15].enabled = !seq_.currentPattern().tracks[15].enabled; return; }

        if (vk == VK_F1) seq_.clearTrack();
        else if (vk == VK_F2) { seq_.useSamples = !seq_.useSamples; seq_.clearAudio = true; selectedParam_ = 0; }
        else if (vk == VK_F3) seq_.setBPM(seq_.getBPM() - 1);
        else if (vk == VK_F4) seq_.setBPM(seq_.getBPM() + 1);
        else if (vk == VK_F5) running_ = false;
        else if (vk == VK_F6) showTheme_ = true;
        else if (vk == VK_F7) seq_.newPattern();
        else if (vk == VK_F8) seq_.prevPattern();
        else if (vk == VK_F9) seq_.setSwing(seq_.getSwing() - 5);
        else if (vk == VK_F10) seq_.setSwing(seq_.getSwing() + 5);
        else if (vk == VK_F11) seq_.nextPattern();
        else if (vk == VK_SPACE) seq_.togglePlay();
        else if (vk == 'h' || vk == 'H') drawHelp();
        else if (vk == 'L' || vk == 'l') {
            if (seq_.useSamples) {
                std::string path;
                if (loadWavDialog(hwnd_, globalSamples[cursorTrack_], path))
                    seq_.currentPattern().tracks[cursorTrack_].samplePath = path;
            }
        }
        else if (vk == VK_TAB) focus_ = (focus_ == GRID) ? MODULES : GRID;
        else if (focus_ == GRID) {
            if (vk == VK_UP || vk == 'W') cursorTrack_ = std::max(0, cursorTrack_ - 1);
            else if (vk == VK_DOWN || vk == 'S') cursorTrack_ = std::min(NUM_TRACKS - 1, cursorTrack_ + 1);
            else if (vk == VK_LEFT || vk == 'A') cursorStep_ = std::max(0, cursorStep_ - 1);
            else if (vk == VK_RIGHT || vk == 'D') cursorStep_ = std::min(15, cursorStep_ + 1);
        } else if (focus_ == MODULES) {
            auto& p = seq_.currentPattern().tracks[cursorTrack_].p; auto& fx = seq_.getFX();
            if (vk == VK_OEM_COMMA) { selectedModule_ = (SelectedModule)((selectedModule_ + NUM_MODULES - 1) % NUM_MODULES); selectedParam_ = 0; }
            else if (vk == VK_OEM_PERIOD) { selectedModule_ = (SelectedModule)((selectedModule_ + 1) % NUM_MODULES); selectedParam_ = 0; }
            else if (!alt && (vk == VK_UP || vk == 'W' || vk == VK_LEFT || vk == 'A')) { if (selectedParam_ > 0) selectedParam_--; }
            else if (!alt && (vk == VK_DOWN || vk == 'S' || vk == VK_RIGHT || vk == 'D')) { if (selectedParam_ < maxParam(selectedModule_)) selectedParam_++; }
            else if (alt && (vk == 'W' || vk == 'S' || vk == 'A' || vk == 'D')) {
                bool up = (vk == 'W' || vk == 'A');
                bool fast = (vk == 'W' || vk == 'S');
                float st = fast ? 0.10f : 0.01f;
                int stInt = fast ? 2 : 1;

                if (selectedModule_ == VCO) {
                    if (selectedParam_ == 0) p.waveform = up ? (p.waveform + 1) % 3 : (p.waveform + 2) % 3;
                    else if (selectedParam_ == 1) p.octave = up ? std::min(24, p.octave + stInt) : std::max(-24, p.octave - stInt);
                    else if (selectedParam_ == 2) p.tune = up ? std::min(24.0f, p.tune + stInt) : std::max(-24.0f, p.tune - stInt);
                    else if (selectedParam_ == 3) p.detune = up ? std::min(1.0f, p.detune + st) : std::max(0.0f, p.detune - st);
                    else if (selectedParam_ == 4) p.fmAmt = up ? std::min(1.0f, p.fmAmt + st) : std::max(0.0f, p.fmAmt - st);
                    else if (selectedParam_ == 5) p.pwdAmt = up ? std::min(1.0f, p.pwdAmt + st) : std::max(0.0f, p.pwdAmt - st);
                }
                else if (selectedModule_ == VCF) {
                    if (selectedParam_ == 0) p.vcfType = up ? 1 : 0;
                    else if (selectedParam_ == 1) p.cutoff = up ? std::min(1.0f, p.cutoff + st) : std::max(0.0f, p.cutoff - st);
                    else p.resonance = up ? std::min(1.0f, p.resonance + st) : std::max(0.0f, p.resonance - st);
                }
                else if (selectedModule_ == VCA) p.volume = up ? std::min(1.0f, p.volume + st) : std::max(0.0f, p.volume - st);
                else if (selectedModule_ == ENV) {
                    if (selectedParam_ == 0) p.attack = up ? std::min(1.0f, p.attack + st) : std::max(0.0f, p.attack - st);
                    else if (selectedParam_ == 1) p.decay = up ? std::min(1.0f, p.decay + st) : std::max(0.0f, p.decay - st);
                    else if (selectedParam_ == 2) p.sustain = up ? std::min(1.0f, p.sustain + st) : std::max(0.0f, p.sustain - st);
                    else p.release = up ? std::min(1.0f, p.release + st) : std::max(0.0f, p.release - st);
                }
                else if (selectedModule_ == LFO_M) {
                    if (selectedParam_ == 0) p.lfoOn = up;
                    else if (selectedParam_ == 1) p.lfoRate = up ? std::min(20.0f, p.lfoRate + st) : std::max(0.1f, p.lfoRate - st);
                    else if (selectedParam_ == 2) p.lfoType = up ? (p.lfoType + 1) % 3 : (p.lfoType + 2) % 3;
                    else if (selectedParam_ == 3) p.lfoDest = up ? (p.lfoDest + 1) % 3 : (p.lfoDest + 2) % 3;
                    else p.lfoDepth = up ? std::min(1.0f, p.lfoDepth + st) : std::max(0.0f, p.lfoDepth - st);
                }
                else if (selectedModule_ == ARP_M) {
                    if (selectedParam_ == 0) p.arpOn = up;
                    else if (selectedParam_ == 1) p.arpRate = up ? std::min(7, p.arpRate + stInt) : std::max(0, p.arpRate - stInt);
                    else p.arpGate = up ? std::min(1.0f, p.arpGate + st) : std::max(0.0f, p.arpGate - st);
                }
                else if (selectedModule_ == SATUR) {
                    if (selectedParam_ == 0) p.saturatorOn = up;
                    else if (selectedParam_ == 1) p.saturatorDrive = up ? std::min(5.0f, p.saturatorDrive + st) : std::max(0.1f, p.saturatorDrive - st);
                    else if (selectedParam_ == 2) p.distoOn = up;
                    else p.distoDrive = up ? std::min(10.0f, p.distoDrive + st) : std::max(0.1f, p.distoDrive - st);
                }
                else if (selectedModule_ == CHORUS) {
                    if (selectedParam_ == 0) p.chorusOn = up;
                    else if (selectedParam_ == 1) p.chorusRate = up ? std::min(5.0f, p.chorusRate + st) : std::max(0.1f, p.chorusRate - st);
                    else if (selectedParam_ == 2) p.chorusDepth = up ? std::min(1.0f, p.chorusDepth + st) : std::max(0.0f, p.chorusDepth - st);
                    else if (selectedParam_ == 3) p.chorusMix = up ? std::min(1.0f, p.chorusMix + st) : std::max(0.0f, p.chorusMix - st);
                    else p.chorusWidth = up ? std::min(1.0f, p.chorusWidth + st) : std::max(0.0f, p.chorusWidth - st);
                }
                else if (selectedModule_ == SIDECH) {
                    if (selectedParam_ == 0) fx.sidechain = up;
                    else fx.sidechainDepth = up ? std::min(0.95f, fx.sidechainDepth + st) : std::max(0.0f, fx.sidechainDepth - st);
                }
                else if (selectedModule_ == EQ_M) {
                    if (selectedParam_ == 0) p.eqLow = up ? std::min(1.0f, p.eqLow + st) : std::max(0.0f, p.eqLow - st);
                    else if (selectedParam_ == 1) p.eqMid = up ? std::min(1.0f, p.eqMid + st) : std::max(0.0f, p.eqMid - st);
                    else p.eqHigh = up ? std::min(1.0f, p.eqHigh + st) : std::max(0.0f, p.eqHigh - st);
                }
                else if (selectedModule_ == DELAY_M) p.delaySend = up ? std::min(1.0f, p.delaySend + st) : std::max(0.0f, p.delaySend - st);
                else if (selectedModule_ == REVERB_M) {
                    if (selectedParam_ == 0) p.reverbSend = up ? std::min(1.0f, p.reverbSend + st) : std::max(0.0f, p.reverbSend - st);
                    else p.reverbType = up ? (p.reverbType + 1) % 4 : (p.reverbType + 3) % 4;
                }
            }
        }
    }

    void drawSampler() {
        cb->clear(0);
        auto& s = globalSamples[cursorTrack_];
        cb->print(2, 2, "=== SAMPLE EDITOR | " + seq_.currentPattern().tracks[cursorTrack_].name + " ===", 0x0F);
        if (!s.loaded) {
            cb->print(2, 4, "No sample loaded. Press L in SAMPLES mode (F2) to load.", 0x0C);
            cb->print(2, 5, "Press ESC to return.", 0x07);
            cb->flush(); return;
        }
        cb->print(2, 4, "Frames: " + std::to_string(s.data.size()) + "  Start: " + std::to_string(s.start) + "  End: " + std::to_string(s.end) + "  Rev: " + (s.reversed?"ON":"OFF"), 0x0B);
        int startX = 5, w = 108, h = 10, baseY = 20;
        for (int i = 0; i < w; ++i) {
            size_t fS = (size_t)((float)i / w * s.data.size()); size_t fE = (size_t)((float)(i + 1) / w * s.data.size());
            float mx = 0; for (size_t j = fS; j < fE && j < s.data.size(); ++j) if (std::abs(s.data[j]) > mx) mx = std::abs(s.data[j]);
            int bh = (int)(mx * h); bool inSel = (fS >= s.start && fE <= s.end); WORD c = inSel ? 0x0A : 0x08;
            for (int k = 0; k < bh; ++k) { cb->setChar(startX + i, baseY - k, 219, c); cb->setChar(startX + i, baseY + k, 219, c); }
        }
        int sP = startX + (int)((float)s.start / s.data.size() * w); int eP = startX + (int)((float)s.end / s.data.size() * w);
        for (int y = baseY - h - 2; y <= baseY + h + 2; ++y) { cb->setChar(sP, y, '|', 0x0A); cb->setChar(eP, y, '|', 0x0C); }
        cb->print(2, 33, "Alt+W/S: Start -/+ | Alt+A/D: End -/+ | Alt+F: Reverse | Ctrl/Esc: Close", 0x0B);
        cb->flush();
    }
    void drawThemeMenu() {
        cb->print(25, 6, "+------------------------------------------------+", 0x0F);
        cb->print(25, 7, "|               THEME SELECTOR                   |", 0x0F);
        cb->print(25, 8, "+------------------------------------------------+", 0x0F);
        cb->print(25, 9, "|  1. Default Cyan                               |", 0x0F);
        cb->print(25, 10, "|  2. Software (Matrix Green)                    |", 0x0F);
        cb->print(25, 11, "+------------------------------------------------+", 0x0F);
        cb->print(25, 12, "|  5. Save Project (.sq)                         |", 0x0F);
        cb->print(25, 13, "|  6. Open Project                               |", 0x0F);
        cb->print(25, 14, "|  7. Export Song (WAV)                          |", 0x0F);
        cb->print(25, 15, "|  8. Opacity +     9. Opacity -                 |", 0x0F);
        cb->print(25, 16, "|  0. Buffer Config                              |", 0x0F);
        cb->print(25, 17, "+------------------------------------------------+", 0x0F);
        cb->print(25, 18, "|  ESC. Cancel                                   |", 0x0F);
        cb->print(25, 19, "+------------------------------------------------+", 0x0F);
    }
    void drawBufferMenu() {
        cb->print(25, 10, "+-------------------------+", 0x0F);
        cb->print(25, 11, "|     BUFFER CONFIG       |", 0x0F);
        cb->print(25, 12, "+-------------------------+", 0x0F);
        cb->print(25, 13, "| 1. Low  (10ms)          |", 0x0F);
        cb->print(25, 14, "| 2. Medium (50ms)        |", 0x0F);
        cb->print(25, 15, "| 3. High (100ms)         |", 0x0F);
        cb->print(25, 16, "| 4. Auto (20ms)          |", 0x0F);
        cb->print(25, 17, "| ESC. Cancel             |", 0x0F);
        cb->print(25, 18, "+-------------------------+", 0x0F);
    }
    void drawSaveMsg() {
        cb->print(40, 20, "+---------------------------+", 0x0F);
        cb->print(40, 21, "|                           |", 0x0F);
        std::string m = " " + saveMsgText_ + " ";
        int pad = (27 - m.length()) / 2; if (pad < 0) pad = 0;
        cb->print(40, 21, "|" + std::string(pad, ' ') + m + std::string(27 - pad - m.length(), ' ') + "|", 0x0F);
        cb->print(40, 22, "|                           |", 0x0F);
        cb->print(40, 23, "+---------------------------+", 0x0F);
    }

    void drawHelpPage1() {
        cb->clear(0);
        cb->print(2, 1, "=== SEQNAL v7.4 - HELP (Page 1/3: GLOBAL & GRID) ===", 0x0F);
        cb->print(2, 3, ">> TRANSPORT / GLOBAL:", 0x0B);
        cb->print(4, 4,  "Space         Play / Stop", 0x07);
        cb->print(4, 5,  "F1            Clear current track", 0x07);
        cb->print(4, 6,  "F2            Toggle INIT (synth) <-> SAMPLES (audio)", 0x07);
        cb->print(4, 7,  "F3 / F4       BPM -/+", 0x07);
        cb->print(4, 8,  "F5            Quit program", 0x07);
        cb->print(4, 9,  "F6            Theme / Save / Load / Export / Buffer menu", 0x07);
        cb->print(4, 10, "F7            Clear current pattern (all tracks)", 0x07);
        cb->print(4, 11, "F8 / F11      Previous / Next pattern", 0x07);
        cb->print(4, 12, "F9 / F10      Swing -/+", 0x07);
        cb->print(4, 13, "H             This help (3 pages)", 0x07);
        cb->print(4, 14, "L             Load WAV (SAMPLES mode only)", 0x07);
        cb->print(4, 15, "ESC           Open sampler editor (trim/reverse)", 0x07);
        cb->print(4, 16, "Tab           Toggle Grid <-> Modules editor", 0x07);
        cb->print(2, 18, ">> GRID (Step Sequencer):", 0x0B);
        cb->print(4, 19, "W/A/S/D or Arrows    Move cursor", 0x07);
        cb->print(4, 20, "Enter                Toggle step ON/OFF (resets note to X)", 0x07);
        cb->print(4, 21, "Ctrl+W               Tune step UP   (X, X1+..X24+)", 0x07);
        cb->print(4, 22, "Ctrl+S               Tune step DOWN (X, X1-..X24-)", 0x07);
        cb->print(4, 23, "Shift+1              Copy current track pattern", 0x07);
        cb->print(4, 24, "Shift+2              Paste into current track", 0x07);
        cb->print(2, 26, ">> TRACK MUTES (toggle track ON/OFF):", 0x0B);
        cb->print(4, 27, "Alt+1..0      Kick Snare Clap Rim Tom OH CC Cymbal Ride Chord", 0x07);
        cb->print(4, 28, "Alt+Z         Shake", 0x07);
        cb->print(4, 29, "Alt+X         Bass", 0x07);
        cb->print(4, 30, "Alt+C         Lead", 0x07);
        cb->print(4, 31, "Alt+V         Pad", 0x07);
        cb->print(4, 32, "Alt+B         Stab", 0x07);
        cb->print(4, 33, "Alt+N         Rhode", 0x07);
        cb->print(2, 35, ">> MODULE EDITOR (Tab):", 0x0B);
        cb->print(4, 36, ", / .             Previous / Next module", 0x07);
        cb->print(4, 37, "W/S/A/D or Arrows Previous / Next parameter", 0x07);
        cb->print(4, 38, "Alt+W / Alt+S     +2 / -2 (fast adjust)", 0x07);
        cb->print(4, 39, "Alt+A / Alt+D     +1 / -1 (fine adjust)", 0x07);
        cb->print(4, 40, "                  Also works for ON/OFF toggles", 0x07);
        cb->print(2, 42, "Press any key for Page 2...", 0x0E);
        cb->flush(); FlushConsoleInputBuffer(hStdIn);
        INPUT_RECORD ir; DWORD r; while (ReadConsoleInput(hStdIn, &ir, 1, &r)) if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) break;
    }
    void drawHelpPage2() {
        cb->clear(0);
        cb->print(2, 1, "=== SEQNAL v7.4 - HELP (Page 2/3: MODULES) ===", 0x0F);
        cb->print(2, 3, ">> VCO (Oscillator):", 0x0B);
        cb->print(4, 4,  "Wave        Sine / Square / Saw", 0x07);
        cb->print(4, 5,  "Octave      -24 to +24 semitones (octave shift)", 0x07);
        cb->print(4, 6,  "Tune        -24 to +24 semitones (fine)", 0x07);
        cb->print(4, 7,  "Detune      0-100% -> second detuned oscillator", 0x07);
        cb->print(4, 8,  "            adds thickness/massive sound with beating", 0x07);
        cb->print(4, 9,  "FM          0-100% -> frequency modulation (metallic tone)", 0x07);
        cb->print(4, 10, "PWD         0-100% -> Pulse Width modulation", 0x07);
        cb->print(4, 11, "            Square: duty cycle 5%-95% (nasal to full)", 0x07);
        cb->print(4, 12, "            Sine/Saw: phase warp (asymmetric timbre)", 0x07);
        cb->print(2, 14, ">> VCF (Filter):", 0x0B);
        cb->print(4, 15, "Type        12db = smooth/bright (Butterworth-like)", 0x07);
        cb->print(4, 16, "            24db = aggressive/dark (cascade 2x filter)", 0x07);
        cb->print(4, 17, "Cut         0-100 (80Hz to 14kHz)", 0x07);
        cb->print(4, 18, "Res         0-100% -> resonance peak at cutoff", 0x07);
        cb->print(4, 19, "            emphasizes and amplifies frequencies AT cutoff", 0x07);
        cb->print(4, 20, "            12db: Q up to 20, 24db: cascade Q per stage", 0x07);
        cb->print(2, 22, ">> VCA (Amplifier):", 0x0B);
        cb->print(4, 23, "Volume      0-100% (track output level)", 0x07);
        cb->print(2, 25, ">> ENV (Envelope):", 0x0B);
        cb->print(4, 26, "Attack / Decay / Sustain / Release (0-99 each)", 0x07);
        cb->print(2, 28, ">> LFO (Low Frequency Oscillator):", 0x0B);
        cb->print(4, 29, "On          Toggle LFO on/off", 0x07);
        cb->print(4, 30, "Rate        Speed in Hz (0.1 to 20)", 0x07);
        cb->print(4, 31, "Type        Sine / Square / Saw", 0x07);
        cb->print(4, 32, "Dest        Pitch / Filter / Amp (modulation target)", 0x07);
        cb->print(4, 33, "Depth       0-100% (modulation amount)", 0x07);
        cb->print(2, 35, ">> ARP (Arpeggiator):", 0x0B);
        cb->print(4, 36, "On          Toggle ARP on/off", 0x07);
        cb->print(4, 37, "Rate        1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64, 1/128", 0x07);
        cb->print(4, 38, "            rhythmic division of retriggering", 0x07);
        cb->print(4, 39, "Gate        0-100% (note length per arp step)", 0x07);
        cb->print(4, 40, "            0% = staccato peaks, 100% = sustained", 0x07);
        cb->print(4, 41, "            fast rates produce rolling 'rilled' effect", 0x07);
        cb->print(2, 43, "Press any key for Page 3...", 0x0E);
        cb->flush(); FlushConsoleInputBuffer(hStdIn);
        INPUT_RECORD ir; DWORD r; while (ReadConsoleInput(hStdIn, &ir, 1, &r)) if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) break;
    }
    void drawHelpPage3() {
        cb->clear(0);
        cb->print(2, 1, "=== SEQNAL v7.4 - HELP (Page 3/3: EFFECTS & FILES) ===", 0x0F);
        cb->print(2, 3, ">> SATUR (Saturation & Distortion):", 0x0B);
        cb->print(4, 4,  "Sat On      Toggle soft-clip saturation (tape-like)", 0x07);
        cb->print(4, 5,  "Sat Drive   1-50 (aggressiveness)", 0x07);
        cb->print(4, 6,  "Dist On     Toggle hard-clip distortion", 0x07);
        cb->print(4, 7,  "Dist Drive  1-100 (crush amount)", 0x07);
        cb->print(2, 9, ">> CHORUS (INDEPENDENT per track):", 0x0B);
        cb->print(4, 10, "On / Rate / Depth / Mix / Width", 0x07);
        cb->print(4, 11, "Modulated short delay -> wide/ensemble effect", 0x07);
        cb->print(4, 12, "ONLY affects the track where it's enabled (not master)", 0x07);
        cb->print(2, 14, ">> SIDECH (Sidechain / Ducking):", 0x0B);
        cb->print(4, 15, "On          Enable ducking effect", 0x07);
        cb->print(4, 16, "Depth       0-100% (aggression of pumping)", 0x07);
        cb->print(4, 17, "ROOT locked to KICK track @ 1/4 note rate", 0x07);
        cb->print(4, 18, "When kick hits, other tracks duck smoothly", 0x07);
        cb->print(4, 19, "Higher depth = more pumping. Master unaffected.", 0x07);
        cb->print(2, 21, ">> EQ (3-band):", 0x0B);
        cb->print(4, 22, "Low / Mid / High  0-100% gain per band", 0x07);
        cb->print(4, 23, "50 = unity | 0 = -inf | 100 = +6dB boost", 0x07);
        cb->print(2, 25, ">> DELAY (INDEPENDENT per track):", 0x0B);
        cb->print(4, 26, "Send        0-100% (wet amount)", 0x07);
        cb->print(4, 27, "ONLY affects tracks where Send > 0 (no master mix)", 0x07);
        cb->print(2, 29, ">> REVERB (INDEPENDENT per track):", 0x0B);
        cb->print(4, 30, "Send        0-100% (wet amount)", 0x07);
        cb->print(4, 31, "Type        Room / Cathedral / Drum / Hall", 0x07);
        cb->print(4, 32, "ONLY affects tracks where Send > 0 (no master mix)", 0x07);
        cb->print(4, 33, "Each track has its OWN reverb, no bleed between", 0x07);
        cb->print(2, 35, ">> THEMES & FILES (F6):", 0x0B);
        cb->print(4, 36, "1  Default Cyan (with minimal blink)", 0x07);
        cb->print(4, 37, "2  Software (Matrix green, X = white)", 0x07);
        cb->print(4, 38, "5  Save Project (.sq compressed + samples embedded)", 0x07);
        cb->print(4, 39, "6  Open Project (reloads .sq, resets all state)", 0x07);
        cb->print(4, 40, "7  Export Song to WAV (Tracks/TrackN.wav, standalone)", 0x07);
        cb->print(4, 41, "8 / 9  Opacity +/-   |  0  Buffer config (10/50/100/20ms)", 0x07);
        cb->print(4, 42, "Note: exported WAV works WITHOUT the terminal open", 0x07);
        cb->print(2, 44, "Press any key to return to main screen...", 0x0E);
        cb->flush(); FlushConsoleInputBuffer(hStdIn);
        INPUT_RECORD ir; DWORD r; while (ReadConsoleInput(hStdIn, &ir, 1, &r)) if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) break;
    }
    void drawHelp() {
        drawHelpPage1();
        drawHelpPage2();
        drawHelpPage3();
    }

    void run() { setup(); while (running_) { draw(); handleInput(); Sleep(16); } restore(); }

private:
    Sequencer& seq_; AudioEngine& audio_; HWND hwnd_; ConsoleBuffer* cb;
    int width_, height_; int cursorTrack_ = 0, cursorStep_ = 0;
    FocusArea focus_ = GRID; SelectedModule selectedModule_ = VCO; int selectedParam_ = 0;
    bool running_ = true; bool showTheme_ = false, showProject_ = false, showSampler_ = false;
    bool showSaveMsg_ = false; bool showBufferMenu_ = false; std::string saveMsgText_ = "";
    int themeIdx_ = 0; int opacity_ = 100;
};

int main() {
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    CoInitialize(nullptr); HWND hwnd = GetConsoleWindow();
    Sequencer seq; AudioEngine audio(seq);
    if (!audio.init()) return 1; audio.start();
    ConsoleUI ui(seq, audio, hwnd); ui.run();
    audio.stop(); CoUninitialize(); return 0;
}