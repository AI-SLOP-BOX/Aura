#pragma once

#include <atomic>
#include <array>
#include <vector>

namespace Aura::Core::Concurrency {

/**
 * @brief MidiInputBuffer: 【完全ロックフリー】リアルタイム・ハードウェアMIDIキャプチャ
 * 以前のバージョンは悪名高い「偽のリアルタイム安全（Fake Real-Time Safe）」でした。
 * OSのMIDI割り込みスレッドと、DAWの1分1秒を争うAudioスレッド間で `std::mutex` を使ってロックしていたため、
 * タイミング悪く待機が重なると、音切れ（デジタルのプツッというノイズ）が100%発生する構造的欠陥がありました。
 * さらには `std::vector::push_back` でメモリを動的確保するというリアルタイムプログラミングの「最悪の禁じ手」も犯していました。
 *
 * 【最新修正版】はミューテックス（排他制御）をメモリ空間から完全に排除し、固定長のリングバッファ（Circular Buffer）と
 * `std::atomic` メモリオペレーションを用いたSPSC（Single Producer, Single Consumer）キューとして完全に再設計されています。
 */
class MidiInputBuffer {
public:
    static MidiInputBuffer& getInstance() {
        static MidiInputBuffer instance;
        return instance;
    }

    struct RawMidiEvent {
        uint8_t status;
        uint8_t data1;
        uint8_t data2;
        uint64_t timestamp;
    };

    /**
     * @brief 外部MIDIハードウェア（USBキーボード等）から押し込まれる（プロデューサー・スレッド）
     * メモリ割り当て（new）も、スレッドロック（mutex）も一切行わず、ナノ秒レベルで書き込みを完了させます。
     */
    void pushEvent(uint8_t s, uint8_t d1, uint8_t d2, uint64_t ts) {
        size_t currentWrite = m_writeIndex.load(std::memory_order_relaxed);
        size_t nextWrite = (currentWrite + 1) & (MAX_MIDI_EVENTS - 1);

        if (nextWrite != m_readIndex.load(std::memory_order_acquire)) {
            m_ringBuffer[currentWrite] = {s, d1, d2, ts};
            m_writeIndex.store(nextWrite, std::memory_order_release);
        }
    }

    size_t pullEvents(std::array<RawMidiEvent, 512>& outputBuffer) {
        size_t currentRead = m_readIndex.load(std::memory_order_relaxed);
        size_t currentWrite = m_writeIndex.load(std::memory_order_acquire);
        
        size_t count = 0;
        while (currentRead != currentWrite && count < outputBuffer.size()) {
            outputBuffer[count] = m_ringBuffer[currentRead];
            currentRead = (currentRead + 1) & (MAX_MIDI_EVENTS - 1);
            count++;
        }
        
        if (count > 0) {
            m_readIndex.store(currentRead, std::memory_order_release);
        }
        
        return count;
    }

private:
    MidiInputBuffer() : m_writeIndex(0), m_readIndex(0) {}

    static constexpr size_t MAX_MIDI_EVENTS = 2048;
    std::array<RawMidiEvent, MAX_MIDI_EVENTS> m_ringBuffer;
    
    alignas(64) std::atomic<size_t> m_writeIndex;
    alignas(64) std::atomic<size_t> m_readIndex;
};

} // namespace Aura::Core::Concurrency
