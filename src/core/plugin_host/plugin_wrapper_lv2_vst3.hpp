#pragma once
#include <string>
#include "../../dsp/iprocessor.hpp"

namespace Aura::Core::PluginHost {

/**
 * @class ExternalPluginWrapper
 * @brief 【究極の骨組み・外部プラグイン統合】VST3 / LV2 / AU / CLAP ホスティング基盤
 * ArdourやZrythm、すべてのDAWが"絶対"に持っている機能……「サードパーティの連携エコシステム」です！
 * このネイティブDAW内で外部メーカー（NI, Waves, FabFilterなど）のプラグインを動かすための汎用ラッパー層を追加しました。
 * プラグインごとの処理単位やUIスレッドの差異を吸収し、ネイティブの `IProcessor` としてシームレスに
 * TimelineSystemやMasterSuiteのルーティンググラフ（Audio Graph）に組み込むための基幹モジュールです。
 */
class ExternalPluginWrapper : public DSP::IProcessor {
public:
    enum class Format { VST3, LV2, AudioUnit, CLAP };

    ExternalPluginWrapper(Format fmt, const std::string& pluginPath) 
        : m_format(fmt), m_path(pluginPath) 
    {
        // 仮想的なロード処理（dlopen/LoadLibraryによるC++共有ライブラリの動的リンクを展開）
        // LV2ホストならLilv、VST3ならVST3_SDK側のInstantiate処理が走る場所です。
    }

    // ネイティブDAWエンジンから外部プラグインへのサンプリングレート／バッファ情報セット
    void prepareToPlay(double sr, uint32_t bs) override {
        // 例: plugin_handle->setupProcessing(sr, bs);
    }

    void process(AudioBuffer& b, const MidiBuffer& midi) override {
        // 【Zrythm/Ardour式の処理フロー】
        // 1. MidiBufferから規格（LV2/VST3）のネイティブイベントリスト（Atom/Event）へ翻訳・変換する
        // 2. AudioBufferのポインタ配列（ float** ）をプラグインへ渡してインプレース加工させる
        // 3. プラグイン側のプロセスを呼び出し（ plugin_handle->process(...) ）

        // （※モックとして、今回は未実装スルー。骨組みの定義のみです）
    }

    void reset() override {
        // 例: plugin_handle->setProcessing(false); plugin_handle->setProcessing(true);
    }

    uint32_t getLatencySamples() const override {
        // 【PDCの根幹】PDC（プラグイン・ディレイ・コンペンセーション）のための遅延報告。
        // ZrythmやArdour等で最も計算が複雑になるグラフ・トポロジー遅延計算の核となる数値。
        // VST3プラグインが宣言する内部処理遅延をDAW本体に吸い上げます。
        return m_reportedLatency; 
    }

private:
    Format m_format;
    std::string m_path;
    uint32_t m_reportedLatency = 0; // プラグインから通知されたレイテンシー
};

} // namespace Aura::Core::PluginHost
