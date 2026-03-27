#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "../audio_region.hpp"

namespace Aura::Core::Editing {

/**
 * @struct CompRegion
 * @brief コンピング専用のオーディオ区間データ
 */
struct CompRegion {
    std::shared_ptr<AudioRegion> sourceTake; // どのテイク由来か
    uint64_t startSample;       // タイムライン上の開始位置
    uint64_t endSample;         // タイムライン上の終了位置
    uint32_t crossfadeSamples;  // 前後との自動クロスフェード長
};

/**
 * @class TakeCompingManager
 * @brief 【超絶肉付け・王道DAW必須機能】Logic Proの「Quick Swipe Comping（テイク・フォルダ）」
 * ボーカリストに同じサビを10回歌わせ（ループ録音）、
 * 「1回目はAメロの出だしが良い」「3回目はサビのピッチが完璧」といった風に、
 * 各テイクの一番良い部分だけをマウスで「スワイプ（なぞる）」して繋ぎ合わせ、
 * 繋ぎ目の「ブツッ」というノイズを自動クロスフェード（フェードエンベロープ）で消し去り、
 * 1つの完璧な架空のボーカルトラック（コンポジット）を作り上げる、絶対に不可欠な編集コアです。
 */
class TakeCompingManager {
public:
    void addTake(std::shared_ptr<AudioRegion> newTake) {
        m_takes.push_back(newTake);
    }

    /**
     * @brief ユーザーが「このテイクのこの部分を使う！」とスワイプ（選択）した際の処理
     */
    void swipeTakeRegion(size_t takeIndex, uint64_t start, uint64_t end) {
        if (takeIndex >= m_takes.size()) return;

        // 【アルゴリズム的解決】
        // 既存のコンピング（選択）領域と時間が被っている場合、
        // 重なっている部分を数学的に分割（Split）し、新しいテイクで上書きします。
        std::vector<CompRegion> newComps;
        
        for (auto& existing : m_activeRegions) {
            // 被っていない領域はそのまま残す
            if (existing.endSample <= start || existing.startSample >= end) {
                newComps.push_back(existing);
                continue;
            }

            // [既存]が[新規]より前から始まっていれば、前半を残す
            if (existing.startSample < start) {
                newComps.push_back({existing.sourceTake, existing.startSample, start, 512 /* 512サンプルの自動Xf */});
            }

            // [既存]が[新規]より後まで続いていれば、後半を残す
            if (existing.endSample > end) {
                newComps.push_back({existing.sourceTake, end, existing.endSample, 512});
            }
        }

        // ユーザーが新しくなぞった（スワイプした）一番美味しい部分を挿入
        newComps.push_back({m_takes[takeIndex], start, end, 512});

        // タイムライン順にソート（再生エンジン用）
        std::sort(newComps.begin(), newComps.end(), [](const CompRegion& a, const CompRegion& b) {
            return a.startSample < b.startSample;
        });

        m_activeRegions = newComps;
    }

    /**
     * @brief オーディオスレッドから呼ばれて、結合された「最強のコンポジット波形」を返す
     */
    void renderComposite(float* outBuffer, uint64_t renderStart, uint32_t numSamples) {
        // ※実際にはここで m_activeRegions を検索し、クロスフェードの重なりを処理しながら
        // 各テイクからシームレスに波形を引っ張ってきて出力します。（モック）
    }

private:
    std::vector<std::shared_ptr<AudioRegion>> m_takes; // 失敗も含めた全録音テイク
    std::vector<CompRegion> m_activeRegions; // ユーザーがなぞって選択した「最強のキメラ」領域
};

} // namespace Aura::Core::Editing
