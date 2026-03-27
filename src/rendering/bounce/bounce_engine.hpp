#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <functional>
#include <cstdlib>
#include "../../AuraUltimate.hpp"
#include "../../scae/AuraAISuite.hpp"
#include "../../io/persistence/wav_writer.hpp"

namespace Aura::Core::Engine {

/**
 * @class BounceEngine
 * @brief Professional Offline Rendering & Stem Export System.
 * 【超絶肉付け】単なるファイル書き出しから「書き出しワークフロー全体」をカバーするシステムに昇華。
 * 1. マルチフォーマット対応の基盤（WAV/MP3/FLAC）
 * 2. プログレスバーUIのためのリアルタイム進捗コールバック機能
 * 3. AIによる書き出し後のマスタリング自動レビュー
 * 4. Finder/Explorerでの自動ファイル表示 (Logic Proのあの便利な機能)
 */
class BounceEngine {
public:
    enum class Format { WAV_16, WAV_24, WAV_32F, MP3, FLAC };

    struct BounceConfig {
        std::string outputPath;
        uint64_t totalSamples;
        uint32_t sampleRate;
        Format format = Format::WAV_32F;
        bool revealInFinder = true;
        bool runAIMasteringReview = true;
    };

    struct BounceResult {
        bool success;
        double elapsedSeconds;
        std::string aiAdvice;
        std::string message;
    };

    /**
     * @brief MASTER BOUNCE: タイムライン全体をオフライン（非リアルタイム）で最速レンダリングします。
     * @param onProgress UIスレッド（プログレスバー）へ進捗(0.0 - 1.0)を通知するコールバック
     */
    static BounceResult renderMaster(const BounceConfig& config, std::function<void(float)> onProgress = nullptr) {
        auto startTime = std::chrono::high_resolution_clock::now();
        // ★注意：この機能はオーディオスレッドとは別の「バックグラウンド・ワーカー・スレッド」で実行されます。
        auto& engine = ::Aura::AuraEngine::getInstance();
        
        // メモリを一括確保（数十分の曲の場合はストリーミング書き出しにすべきだが、今回は実装の簡略化のためRAM一括）
        std::vector<float> exportL(config.totalSamples, 0.0f);
        std::vector<float> exportR(config.totalSamples, 0.0f);
        
        const uint32_t blockSize = 1024; // オフラインレンダリング用の大きなブロックサイズ
        
        for (uint64_t pos = 0; pos < config.totalSamples; pos += blockSize) {
            uint32_t currentBlock = static_cast<uint32_t>(std::min(static_cast<uint64_t>(blockSize), config.totalSamples - pos));
            
            // エンジンを非リアルタイムモードで駆動（プラグインのハイレゾリューション・モード等をトリガー）
            engine.process(exportL.data() + pos, exportR.data() + pos, currentBlock);
            
            // UIへの進捗報告 (50ブロックに1回程度の頻度でUIを更新し、UIスレッドの詰まりを防ぐ)
            if (onProgress && (pos / blockSize) % 50 == 0) {
                float percent = static_cast<float>(pos) / config.totalSamples;
                onProgress(percent);
            }
        }
        
        if (onProgress) onProgress(1.0f); // 100%
        
        // フォーマットに応じた書き出し (ここではWAVを代表として処理)
        bool writeSuccess = IO::Persistence::WavWriter::write(config.outputPath, exportL.data(), exportR.data(), config.totalSamples, config.sampleRate);
        if (!writeSuccess) return {false, 0.0, "", "Disk write failed."};

        // 【AI マスタリング・レビュー】
        std::string advice = "";
        if (config.runAIMasteringReview) {
            SCAE::SCAEAdvisor advisor(config.sampleRate);
            advice = advisor.analyzeMaster(exportL.data(), exportR.data(), config.totalSamples);
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = endTime - startTime;

        // 【自動ファイル展開】 macOS限定で、書き出したファイルをFinderでハイライト表示する
        if (config.revealInFinder) {
            #ifdef __APPLE__
            std::string command = "open -R \"" + config.outputPath + "\"";
            std::system(command.c_str());
            #endif
        }

        return {true, elapsed.count(), advice, "Render completed successfully in " + std::to_string(elapsed.count()) + "s"};
    }
};

} // namespace Aura::Core::Engine
