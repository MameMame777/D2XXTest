// test_debug.cpp
//
// FT4232HL SPI デバッグ切り分けガイド の 4 ステップを自動実行するテストツール。
//
// Step 1: Open 確認    — 初期化後に CS#=High, SCK=Low であるか
// Step 2: MPSSE 確認   — エコーバック 0xAA → 0xFA 0xAA が返るか
// Step 3: GPIO トグル  — SCK を手動で High/Low に切り替えてソフト読み返し
// Step 4: SPI 送受信   — 0x31 コマンドで 0xAA 送信、内部ループバックで全一致確認
//
// Usage:  test_debug [channelIndex]
//   channelIndex  D2XX device list index (default: 1 = FT4232H CH-B)
//
// 判定基準:
//   Step 1 NG → FT_Open / ドライバを確認
//   Step 3 NG → チャンネル・ハンドル・配線・ピンアサインを確認
//   Step 4 NG → SPI コマンドの内容・順序を確認

#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>

#include "d2xx_mpsse/MpsseDevice.hpp"
#include "d2xx_mpsse/SpiMaster.hpp"

// ---------------------------------------------------------------------------
// ANSI カラー（Windows Terminal / VSCode terminal で有効）
// ---------------------------------------------------------------------------
#define CLR_OK  "\033[32m"
#define CLR_NG  "\033[31m"
#define CLR_YL  "\033[33m"
#define CLR_RST "\033[0m"

static const char* ok_str(bool ok) {
    return ok ? CLR_OK "[OK]" CLR_RST : CLR_NG "[NG]" CLR_RST;
}

// ---------------------------------------------------------------------------
// ピン状態ダンプ（低バイト 8bit）
// ---------------------------------------------------------------------------
static void printPins(uint8_t pins) {
    std::printf("  FT_GetBitMode → 0x%02X\n", static_cast<unsigned>(pins));
    std::printf("    bit0 SCK  = %u  (%s)\n",
        (pins >> 0) & 1u, (pins & 0x01u) ? "High" : CLR_YL "Low " CLR_RST);
    std::printf("    bit1 MOSI = %u\n", (pins >> 1) & 1u);
    std::printf("    bit2 MISO = %u  (%s)\n",
        (pins >> 2) & 1u, (pins & 0x04u) ? "High" : "Low ");
    std::printf("    bit3 CS#  = %u  (%s)\n",
        (pins >> 3) & 1u, (pins & 0x08u) ? CLR_OK "High  3.3V" CLR_RST
                                           : CLR_NG "Low   0V  " CLR_RST);
}

// ===========================================================================
int main(int argc, char* argv[]) {
    const int channelIndex = (argc > 1) ? std::atoi(argv[1]) : 1;
    int failCount = 0;

    bool step1_pass = false;
    bool step3_pass = false;
    bool step4_pass = false;

    std::printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    std::printf(" FT4232HL SPI デバッグ切り分けテスト\n");
    std::printf(" Channel index : %d\n", channelIndex);
    std::printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

    try {
        // デバイスを開く（MPSSE 初期化・クロック設定・同期は内部で実行）
        std::printf("  デバイスオープン中...\n");
        d2xx::MpsseDevice dev(channelIndex, 1'000'000);
        std::printf("  FT_Open: " CLR_OK "OK" CLR_RST "\n\n");

        // ─────────────────────────────────────────────────────────────────
        // STEP 1: Open 確認  CS#=High (3.3V), SCK=Low (0V)
        // ─────────────────────────────────────────────────────────────────
        std::printf("┌──────────────────────────────────────────────┐\n");
        std::printf("│ Step 1: Open 確認                            │\n");
        std::printf("│ 初期化後に CS#=3.3V(High), SCK=0V(Low) ？   │\n");
        std::printf("└──────────────────────────────────────────────┘\n");

        // SPI アイドル状態をセット
        //   value     = 0x08 : CS#=High(bit3), SCK=Low(bit0), MOSI=Low(bit1)
        //   direction = 0x0B : bit0=out, bit1=out, bit2=in, bit3=out
        const uint8_t initCmd[] = { 0x80u, 0x08u, 0x0Bu };
        dev.write(std::span<const uint8_t>(initCmd));
        dev.flush();

        const uint8_t pins1 = dev.getBitMode();
        printPins(pins1);

        const bool s1_cs  = !!(pins1 & 0x08u); // CS#=High
        const bool s1_sck = !(pins1 & 0x01u);  // SCK=Low
        step1_pass = s1_cs && s1_sck;

        std::printf("\n  期待: CS#=High / SCK=Low  %s\n", ok_str(step1_pass));
        if (!step1_pass) {
            if (!s1_cs)  std::printf("  " CLR_NG "→ CS# が Low のまま" CLR_RST
                                     " : FT_SetBitMode / 方向設定を確認\n");
            if (!s1_sck) std::printf("  " CLR_NG "→ SCK が High のまま" CLR_RST
                                     " : ピンアサイン / 方向設定を確認\n");
            ++failCount;
        }
        std::printf("\n");

        // ─────────────────────────────────────────────────────────────────
        // STEP 2: MPSSE エコーバック確認
        // ─────────────────────────────────────────────────────────────────
        std::printf("┌──────────────────────────────────────────────┐\n");
        std::printf("│ Step 2: MPSSE エコーバック確認               │\n");
        std::printf("│ 0xAA 送信 → 0xFA 0xAA 受信 ？               │\n");
        std::printf("└──────────────────────────────────────────────┘\n");

        // MpsseDevice コンストラクタ内で 0xAA / 0xAB の両方を確認済み。
        // ここまで到達していること自体が Step 2 PASS の証拠。
        std::printf("  MpsseDevice コンストラクタにて確認済み:\n");
        std::printf("    送信 0xAA → 受信 0xFA 0xAA  %s\n", ok_str(true));
        std::printf("    送信 0xAB → 受信 0xFA 0xAB  %s\n", ok_str(true));
        std::printf("\n  MPSSE エンジン起動確認: %s\n\n", ok_str(true));

        // ─────────────────────────────────────────────────────────────────
        // STEP 3: GPIO 手動 SCK トグル確認
        // ─────────────────────────────────────────────────────────────────
        std::printf("┌──────────────────────────────────────────────┐\n");
        std::printf("│ Step 3: GPIO 手動 SCK トグル確認             │\n");
        std::printf("│ SCK に 1 パルス (High → Low) を出力 ？      │\n");
        std::printf("└──────────────────────────────────────────────┘\n");

        // SCK → High  (bit0=1, CS# そのまま bit3=0 にならないよう注意)
        // CS# も同時に保持したい場合は bit3=1 を立てておく
        const uint8_t sckHi[] = { 0x80u, 0x09u, 0x0Bu }; // bit0=1(SCK), bit3=1(CS#)
        dev.write(std::span<const uint8_t>(sckHi));
        dev.flush();
        const uint8_t p3hi = dev.getBitMode();
        std::printf("  SCK=High コマンド送信後:\n");
        printPins(p3hi);
        const bool s3_hi = !!(p3hi & 0x01u);
        std::printf("  SCK → High: %s\n\n", ok_str(s3_hi));

        // SCK → Low
        const uint8_t sckLo[] = { 0x80u, 0x08u, 0x0Bu }; // bit0=0(SCK), bit3=1(CS#)
        dev.write(std::span<const uint8_t>(sckLo));
        dev.flush();
        const uint8_t p3lo = dev.getBitMode();
        std::printf("  SCK=Low コマンド送信後:\n");
        printPins(p3lo);
        const bool s3_lo = !(p3lo & 0x01u);
        std::printf("  SCK → Low : %s\n\n", ok_str(s3_lo));

        step3_pass = s3_hi && s3_lo;
        std::printf("  SCK トグル (High→Low): %s\n", ok_str(step3_pass));
        if (!step3_pass) {
            std::printf("  " CLR_NG "NG 原因候補:" CLR_RST "\n");
            std::printf("    - チャンネルのハンドル違い（ADBUS vs BDBUS）\n");
            std::printf("    - ピンアサイン / 方向レジスタの誤り\n");
            std::printf("    - 外部で固定 Hi / Lo になっている配線問題\n");
            ++failCount;
        }
        std::printf("\n");

        // ─────────────────────────────────────────────────────────────────
        // STEP 4: SPI コマンド送受信確認（0x31, 内部ループバック使用）
        // ─────────────────────────────────────────────────────────────────
        std::printf("┌──────────────────────────────────────────────┐\n");
        std::printf("│ Step 4: SPI コマンド送受信確認               │\n");
        std::printf("│ 0x31 で 0xAA 送信, 内部ループバックで確認    │\n");
        std::printf("└──────────────────────────────────────────────┘\n");

        d2xx::SpiMaster spi(dev);

        // 内部ループバック ON: MOSI → MISO が内部接続される
        spi.enableLoopback(true);
        std::printf("  内部ループバック ON (0x84)\n");

        // テストパターン: 0xAA, 0x55, 0xFF, 0x00
        const std::array<uint8_t, 4> txPat = { 0xAAu, 0x55u, 0xFFu, 0x00u };
        std::array<uint8_t, 4>       rxPat = {};

        spi.csLow();
        spi.transfer(txPat.data(), rxPat.data(), txPat.size());
        spi.csHigh();

        bool allMatch = true;
        for (std::size_t i = 0; i < txPat.size(); ++i) {
            const bool match = (txPat[i] == rxPat[i]);
            std::printf("    [%zu] TX=0x%02X  RX=0x%02X  %s\n",
                        i, txPat[i], rxPat[i], ok_str(match));
            if (!match) allMatch = false;
        }

        spi.enableLoopback(false);
        std::printf("  内部ループバック OFF (0x85)\n\n");

        step4_pass = allMatch;
        std::printf("  TX == RX (全 %zu バイト): %s\n", txPat.size(), ok_str(step4_pass));
        if (!step4_pass) {
            std::printf("  " CLR_NG "NG 原因候補:" CLR_RST "\n");
            std::printf("    - MPSSE コマンドの内容・順序の問題\n");
            std::printf("    - CS# アサート/デアサートのタイミング\n");
            std::printf("    - SCK クロック設定 / 分周比の問題\n");
            ++failCount;
        }
        std::printf("\n");

    } catch (const d2xx::MpsseError& e) {
        std::fprintf(stderr, CLR_NG "\n[ERROR]" CLR_RST " %s  (FT_STATUS=%u)\n",
                     e.what(), e.status());
        std::fprintf(stderr, "  → デバイスがオープンできない場合:\n");
        std::fprintf(stderr, "      別プロセスが同じハンドルを占有していないか確認\n");
        std::fprintf(stderr, "      Linux: ftdi_sio をアンバインド"
                             "  (sudo rmmod ftdi_sio)\n");
        return 1;
    } catch (const std::exception& e) {
        std::fprintf(stderr, CLR_NG "\n[ERROR]" CLR_RST " %s\n", e.what());
        return 1;
    }

    // ─────────────────────────────────────────────────────────────────────
    // サマリー & 切り分けフロー判定
    // ─────────────────────────────────────────────────────────────────────
    std::printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    std::printf(" RESULT: %s  (%d step(s) failed)\n\n",
        failCount == 0 ? CLR_OK "全 PASS" CLR_RST
                       : CLR_NG "FAIL あり" CLR_RST,
        failCount);

    if (!step1_pass)
        std::printf(" 切り分け: " CLR_NG "Step 1 NG" CLR_RST
                    " → FT_Open 失敗。ドライバ / usbfs を確認\n");
    else if (!step3_pass)
        std::printf(" 切り分け: " CLR_NG "Step 3 NG" CLR_RST
                    " → チャンネル・ハンドル・配線・ピンアサインを確認\n");
    else if (!step4_pass)
        std::printf(" 切り分け: " CLR_NG "Step 4 NG" CLR_RST
                    " → SPI コマンドの内容・順序を確認\n");
    else
        std::printf(" 切り分け: " CLR_OK "正常動作" CLR_RST
                    " → 実 SPI デバイスを接続してテスト可能\n");

    std::printf("\n フロー図:\n");
    std::printf("  FT_Open → %s CS#=3.3V / SCK=0V ?\n",
                step1_pass ? CLR_OK "YES" CLR_RST : CLR_NG " NO" CLR_RST);
    std::printf("          → %s MPSSE 0xFA 0xAA 受信 ?\n", CLR_OK "YES" CLR_RST);
    std::printf("          → %s GPIO で SCK がトグル ?\n",
                step3_pass ? CLR_OK "YES" CLR_RST : CLR_NG " NO" CLR_RST);
    std::printf("          → %s SPI で TX==RX ?\n",
                step4_pass ? CLR_OK "YES → 正常動作" CLR_RST
                           : CLR_NG " NO → SPI コマンド確認" CLR_RST);
    std::printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");

    return failCount == 0 ? 0 : 1;
}
