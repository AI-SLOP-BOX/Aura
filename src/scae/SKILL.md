# SKILL: SCAE (Smart Computational AI Engine)

## 🤖 AI Role: Non-Generative Heuristics (非生成型・計算AI)
SCAEモジュールは、重たいテンソル演算や外部API（LLM等）を使わず、C++のリアルタイムDSP計算だけで動作する「実用的かつ超高速なアシスタントAI」です。オーディオスレッドの邪魔を一切してはなりません。

## 🧠 Intelligence Directives
- **Zero-Latency Inference**: 判定モジュールはすべてオーディオスレッド内で「完全ロックフリー・O(1)」で動くこと。API通信待ちやスレッドブロックは『死罪』です。
- **Masking Detection**: 自動ミキシング（Auto Gain Rider等）の分析は、サイドチェインのRMSエンベロープと主信号のエネルギー差分（マスキング量）を数学的に計算してオートメーション値を出力します。
- **No Python / No Bloat**: 流行りの深層学習モデルは組み込まず、すべて自己相関関数、FFT、ヒューリスティクス（条件分岐と数学フィルタ）だけでAIの「知的な振る舞い」を実装してください。
