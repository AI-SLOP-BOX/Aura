#pragma once
#include <string>
#include <vector>
#include <cstdlib>

namespace Aura::IO::Media {

/**
 * @class CliMediaProcessor
 * @brief 【OSS標準への完全適応】FFmpeg / SoX メディアバッチエンジン
 * DAW自体の「書き出し機能」とは完全に切り離された、OS不問のマルチメディア・コマンドライン心臓部です。
 * 映像の結合（Kdenlive等との連携）や、数百にのぼる音声ファイルのサンプルレート一括変換などを
 * オープンソース界のメディア王である「FFmpeg」と「SoX」に直接CLIプロセスを通してブン投げ、
 * 数十時間の単純作業を1秒で終わらせるバッチ処理インフラです。
 */
class CliMediaProcessor {
public:
    /**
     * @brief 完成したDAWのWAV音源を、FFmpegを使って元の動画ファイル（MP4等）にマージ（結合）する
     */
    static bool muxAudioToVideo(const std::string& audioPath, const std::string& videoPath, const std::string& outputPath) {
        // ターミナル（WindowsのPowerShell、MacのZsh両対応）でFFmpegを叩くコマンド文字列の自動構築
        // （映像は無劣化コピー、音声だけを高音質AACで上書きエンコードする神のコマンド）
        std::string cmd = "ffmpeg -i \"" + videoPath + "\" -i \"" + audioPath + 
                          "\" -c:v copy -c:a aac -b:a 320k \"" + outputPath + "\" -y -v quiet";
        
        // オペレーティングシステムのプロセスを直接叩く（std::system）
        int result = std::system(cmd.c_str());
        return (result == 0); 
    }

    /**
     * @brief SoX (Sound eXchange) を用いた、数百個の録音テイクの一掃バッチ処理・トリミング
     */
    static bool batchTrimSilenceUsingSoX(const std::vector<std::string>& files) {
        bool success = true;
        for (const auto& file : files) {
            // soXが標準で持つ、「前後の無音部分（Silence）のノイズだけを超高速に切り落とす」コマンド
            std::string cmd = "sox \"" + file + "\" \"trimmed_" + file + 
                              "\" silence 1 0.1 1% reverse silence 1 0.1 1% reverse";
            
            if (std::system(cmd.c_str()) != 0) {
                success = false;
            }
        }
        return success;
    }
};

} // namespace Aura::IO::Media
