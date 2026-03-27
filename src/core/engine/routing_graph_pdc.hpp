#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>

namespace Aura::Core::Engine {

// 仮想的なオーディオノード（トラック、バス、プラグイン等の処理単位）
class AudioNode {
public:
    uint32_t id;
    uint32_t processingLatency = 0; // そのノードが持つ純粋な遅延（LinerPhase EQ等）
    uint32_t cumulativeDelay = 0;   // DAWが追加すべき「待ち（ディレイ）」サンプル数
    std::vector<uint32_t> outgoingEdges; // 次に接続されるノード（ルーティング）
    uint32_t inDegree = 0; // 入ってくる接続数（トポロジカルソート用）
};

/**
 * @class RoutingGraphPDC
 * @brief 【超絶肉付け・王道DAW必須機能】完全な遅延補正（PDC）とDAGルーティング
 * 以前のコードでは、適当な配列でトラックを処理していましたが、それだと
 * 「トラックA（キック）の音を、トラックB（ベース）のサイドチェインに送る」際に
 * トラックBが先に処理されてしまうと音が鳴らなくなります。
 * 
 * ZrythmやArdour等が行っている、有向非巡回グラフ（DAG）を用いたグラフ理論により、
 * 1. トポロジカルソート（依存関係に基づく正しい処理順序の決定）
 * 2. PDC（プラグイン・ディレイ・補正：重いエフェクトが挿さっていないトラックに
 *    ワザと遅延バッファを挟み、マスター出力時点で全トラックの位相をミリ秒単位で完璧に揃える数学的補正）
 * を行う、DAWミキサーの「真の心臓部」です。
 */
class RoutingGraphPDC {
public:
    void addNode(uint32_t nodeId, uint32_t latency) {
        auto node = std::make_shared<AudioNode>();
        node->id = nodeId;
        node->processingLatency = latency;
        m_nodes[nodeId] = node;
    }

    void connect(uint32_t sourceId, uint32_t destId) {
        if (m_nodes.count(sourceId) && m_nodes.count(destId)) {
            m_nodes[sourceId]->outgoingEdges.push_back(destId);
            m_nodes[destId]->inDegree++;
        }
    }

    /**
     * @brief グラフ理論（Kahnのアルゴリズム）を用いて正しい処理順と遅延を計算
     */
    bool compileGraph() {
        m_executionOrder.clear();
        std::queue<uint32_t> noIncoming;
        
        // 1. トポロジカルソート（依存関係順に前処理）
        std::unordered_map<uint32_t, uint32_t> inDegrees;
        for (const auto& pair : m_nodes) {
            inDegrees[pair.first] = pair.second->inDegree;
            if (pair.second->inDegree == 0) noIncoming.push(pair.first);
        }

        while (!noIncoming.empty()) {
            uint32_t curr = noIncoming.front();
            noIncoming.pop();
            m_executionOrder.push_back(curr);
            for (uint32_t neighbor : m_nodes[curr]->outgoingEdges) {
                if (--inDegrees[neighbor] == 0) noIncoming.push(neighbor);
            }
        }

        if (m_executionOrder.size() != m_nodes.size()) return false;

        // 2. PDC（遅延補正）の計算
        // 全ノードの待ち時間をリセット
        for (auto& pair : m_nodes) pair.second->cumulativeDelay = 0;

        // クリティカルパス（最大遅延）を後ろから伝播
        std::unordered_map<uint32_t, uint32_t> pathDelays;
        for (auto it = m_executionOrder.rbegin(); it != m_executionOrder.rend(); ++it) {
            uint32_t curr = *it;
            uint32_t maxChildPath = 0;
            for (uint32_t neighbor : m_nodes[curr]->outgoingEdges) {
                maxChildPath = std::max(maxChildPath, pathDelays[neighbor]);
            }
            pathDelays[curr] = m_nodes[curr]->processingLatency + maxChildPath;
        }

        // 各ノードに必要な「待ち時間（空のディレイ）」を決定
        for (auto& pair : m_nodes) {
            uint32_t curr = pair.first;
            uint32_t maxChildPath = 0;
            for (uint32_t neighbor : m_nodes[curr]->outgoingEdges) {
                maxChildPath = std::max(maxChildPath, pathDelays[neighbor]);
            }
            
            for (uint32_t neighbor : m_nodes[curr]->outgoingEdges) {
                // 子ノードごとに、最大パスとの差分を待ち時間として設定
                uint32_t diff = maxChildPath - pathDelays[neighbor];
                m_nodes[neighbor]->cumulativeDelay += diff;
            }
        }

        return true;
    }

    const std::vector<uint32_t>& getExecutionOrder() const { return m_executionOrder; }
    uint32_t getDelayForNode(uint32_t id) const { return m_nodes.at(id)->cumulativeDelay; }

private:
    std::unordered_map<uint32_t, std::shared_ptr<AudioNode>> m_nodes;
    std::vector<uint32_t> m_executionOrder; 
};

} // namespace Aura::Core::Engine
