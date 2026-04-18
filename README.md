# D2XXTest — FTDI D2XX / MPSSE C++ Library

C++20 library and test executables for controlling FTDI FT4232H / FT2232H chips
via the **D2XX MPSSE** interface (SPI + GPIO + EEPROM).

Tested hardware: **FT2232H** (Digilent Adept USB Device, VID=0x0403 PID=0x6010),
Channel B (D2XX device index 1).

## 動作確認済み環境

| OS | コンパイラ | ビルド方法 | 結果 |
|---|---|---|---|
| Windows 11 (x64) | MSVC 19.44 (VS2022) | CMake + Visual Studio | ✅ 全テスト PASS |
| Ubuntu 24.04 on WSL2 (x86_64) | GCC 13.3.0 | g++ 直接 (静的リンク) | ✅ 全テスト PASS |

> WSL2 で USB デバイスを使用するには **usbipd-win** によるパススルーが必要です。
> 詳細は [WSL2 USB セットアップ](#wsl2-usb-パススルー-usbipd-win) を参照してください。

---

## Features

| Class | Responsibility |
|---|---|
| `d2xx::MpsseDevice` | RAII open/close, MPSSE init & sync, raw command I/O |
| `d2xx::SpiMaster` | SPI Mode 0 MSB-first full-duplex transfer, CS# control, internal loopback |
| `d2xx::Gpio` | Low-byte / high-byte GPIO set-direction / write / read |
| `d2xx::Eeprom` | FTDI internal EEPROM (`FT_ReadEE`) + external 25-series SPI EEPROM read |

### Test executables

| Executable | Description |
|---|---|
| `test_open` | Enumerate D2XX devices, open Channel B in MPSSE mode, verify sync |
| `test_loopback` | 4-pattern internal loopback (256 B sequential, 64 B random, 1 B, 128 B 0xFF) |
| `test_eeprom_ftdi` | Dump FTDI internal EEPROM words via `FT_ReadEE`, decode VID/PID |
| `test_eeprom_spi` | Read 32 bytes from an external 25-series SPI EEPROM |
| `test_debug` | 4-step SPI debug procedure (Open / MPSSE echo / SCK toggle / SPI loopback) |

---

## Pin Assignment (FT4232H Channel B low byte)

```
BDBUS0  bit0  SCK   (clock out)
BDBUS1  bit1  MOSI  (data out)
BDBUS2  bit2  MISO  (data in)
BDBUS3  bit3  CS#   (chip-select, active-low)
BDBUS4-7      GPIO  (managed by Gpio class)
BCBUS0-7      High-byte GPIO
```

---

## Requirements

- **FTDI D2XX SDK** — headers (`ftd2xx.h`, `WinTypes.h`) and import library (`ftd2xx.lib`)
  - Place in `third_party/ftd2xx/libftd2xx/` (official SDK zip layout) **or**
    directly in `third_party/ftd2xx/`
  - See [`third_party/ftd2xx/README.txt`](third_party/ftd2xx/README.txt) for details
- **CMake** ≥ 3.20
- **C++20** compiler
  - Windows: MSVC (Visual Studio 2022 recommended)
  - Linux: GCC 11+ / Clang 13+
- Windows: D2XX driver installed (`ftd2xx.dll` present in System32)
- Linux: `ftdi_sio` kernel module unbound (`sudo rmmod ftdi_sio`)

---

## Directory Layout

```
D2XXTest/
├── CMakeLists.txt
├── include/
│   └── d2xx_mpsse/
│       ├── MpsseDevice.hpp
│       ├── SpiMaster.hpp
│       ├── Gpio.hpp
│       └── Eeprom.hpp
├── src/
│   ├── MpsseDevice.cpp
│   ├── SpiMaster.cpp
│   ├── Gpio.cpp
│   └── Eeprom.cpp
├── tests/
│   ├── test_open.cpp
│   ├── test_loopback.cpp
│   ├── test_eeprom_ftdi.cpp
│   ├── test_eeprom_spi.cpp
│   └── test_debug.cpp
├── third_party/
│   └── ftd2xx/
│       ├── README.txt          ← SDK placement guide
│       └── libftd2xx/          ← Place SDK headers here (not committed)
│           ├── ftd2xx.h
│           └── WinTypes.h
└── docs/
    └── test_results.md         ← Captured test results on real hardware
```

---

## Build

### Windows (MSVC x64)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Executables are written to `build\bin\Release\`.

### Linux (GCC) — CMake を使う場合

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Executables are written to `build/bin/`.

### Linux (GCC) — CMake なしで直接ビルドする場合

CMake がない環境では `g++` 1コマンドでビルドできます:

```bash
# ライブラリをコンパイルしてアーカイブ
g++ -std=c++20 -O2 \
    -Iinclude -Ithird_party/ftd2xx/libftd2xx \
    -c src/MpsseDevice.cpp src/SpiMaster.cpp src/Gpio.cpp src/Eeprom.cpp
ar rcs libd2xx_mpsse.a MpsseDevice.o SpiMaster.o Gpio.o Eeprom.o

# 各テストをビルド (TARGET を差し替えるだけ)
TARGET=test_debug   # test_open / test_loopback / test_eeprom_ftdi / test_eeprom_spi
g++ -std=c++20 -O2 -Iinclude \
    tests/${TARGET}.cpp libd2xx_mpsse.a \
    -Lthird_party/ftd2xx -lftd2xx -lpthread -ldl \
    -o ${TARGET}

# 実行 (共有ライブラリを使う場合は LD_LIBRARY_PATH が必要)
LD_LIBRARY_PATH=third_party/ftd2xx ./${TARGET}
```

> **静的リンク** (`libftd2xx.a`) を使う場合は `-lftd2xx` が静的リンクになるため
> `LD_LIBRARY_PATH` の設定は不要です。

---

## Linux Setup — ゼロからの実行手順

コードの変更は不要です。以下の手順を上から順に実行してください。

### 前提パッケージのインストール

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y git cmake g++ libusb-1.0-0

# Fedora / RHEL
sudo dnf install -y git cmake gcc-c++ libusb1
```

### リポジトリの取得

```bash
git clone https://github.com/<your-username>/D2XXTest.git
cd D2XXTest
```

### D2XX ライブラリの入手と配置

FTDI 公式サイトから Linux 用ライブラリをダウンロードします:

```bash
# x86_64 の場合 (バージョンは最新に読み替えてください)
wget https://ftdichip.com/wp-content/uploads/2022/07/libftd2xx-x86_64-1.4.27.tgz
tar xf libftd2xx-x86_64-1.4.27.tgz

# .so を third_party に配置
cp release/build/x86_64/libftd2xx.so.1.4.27 third_party/ftd2xx/
ln -s libftd2xx.so.1.4.27 third_party/ftd2xx/libftd2xx.so
```

配置後の構成:

```
third_party/ftd2xx/
├── libftd2xx/
│   ├── ftd2xx.h
│   └── WinTypes.h
├── libftd2xx.so         → libftd2xx.so.1.4.27 (symlink)
└── libftd2xx.so.1.4.27
```

> **静的リンクを使う場合** (`libftd2xx.a`): DLL 不要で配布が楽になります。
> `release/build/x86_64/libftd2xx.a` を同様に配置してください。

### `ftdi_sio` カーネルモジュールのアンバインド

Linux カーネルの `ftdi_sio` ドライバと D2XX は同時に使えません:

```bash
# 接続前に確認
lsmod | grep ftdi

# アンバインド (接続中でも可)
sudo rmmod ftdi_sio

# 再起動後も維持する場合
echo "blacklist ftdi_sio" | sudo tee /etc/modprobe.d/ftdi_sio.conf
sudo update-initramfs -u    # Ubuntu / Debian のみ
```

### USB アクセス権限の設定 (udev)

`sudo` なしで実行できるようにします:

```bash
# FT2232H (VID=0x0403, PID=0x6010)
sudo tee /etc/udev/rules.d/99-ftdi.rules << 'EOF'
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", ATTR{idProduct}=="6010", MODE="0666", TAG+="uaccess"
SUBSYSTEM=="usb", ATTR{idVendor}=="0403", ATTR{idProduct}=="6011", MODE="0666", TAG+="uaccess"
EOF
sudo udevadm control --reload-rules && sudo udevadm trigger

# デバイスが認識されているか確認
lsusb | grep 0403
```

### ビルド

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

ビルド成功時の出力:
```
build/bin/test_open
build/bin/test_loopback
build/bin/test_eeprom_ftdi
build/bin/test_eeprom_spi
build/bin/test_debug
```

### テスト実行

共有ライブラリのパスを設定してから実行します
(静的リンクの場合は `LD_LIBRARY_PATH` 不要):

```bash
export LD_LIBRARY_PATH=$PWD/third_party/ftd2xx:$LD_LIBRARY_PATH

# デバイス認識確認
./build/bin/test_open

# 内部ループバック
./build/bin/test_loopback

# FTDI 内蔵 EEPROM 読み出し
./build/bin/test_eeprom_ftdi

# 4 ステップ デバッグ確認
./build/bin/test_debug
```

### トラブルシューティング

| 症状 | 確認事項 |
|---|---|
| `FT_DEVICE_NOT_FOUND` | `lsusb` で 0403:6010 が見えるか確認。`ftdi_sio` が残っていないか `lsmod` で確認 |
| `FT_INVALID_HANDLE` または permission error | udev ルールが適用されているか確認。または `sudo` で実行 |
| `libftd2xx.so: No such file` | `LD_LIBRARY_PATH` の設定を確認 |
| チャンネルが見つからない | `./build/bin/test_open` でインデックスを確認してから他のテストを実行 |

---

## WSL2 USB パススルー (usbipd-win)

WSL2 は USB デバイスを直接認識しません。**usbipd-win** を使ってパススルーします。

### 1. usbipd-win のインストール (Windows 側・初回のみ)

```powershell
winget install --id dorssel.usbipd-win
```

インストール後は PowerShell を再起動してください。

### 2. デバイスを WSL2 にアタッチ (接続のたびに実行)

```powershell
# USB デバイス一覧を確認 (FT2232H は VID=0403 PID=6010)
usbipd list

# 初回のみ: bind (管理者権限が必要、UACプロンプトが出ます)
usbipd bind --busid <BUSID>        # 例: usbipd bind --busid 3-2

# WSL2 にアタッチ (WSL2 インスタンスが起動していること)
usbipd attach --wsl --busid <BUSID>
```

### 3. WSL2 側で ftdi_sio をアンバインド

```bash
# root で実行 (sudo パスワード不要)
wsl -d Ubuntu-24.04 -u root bash -c "\
  echo '1-1:1.0' > /sys/bus/usb/drivers/ftdi_sio/unbind 2>/dev/null; \
  echo '1-1:1.1' > /sys/bus/usb/drivers/ftdi_sio/unbind 2>/dev/null"
```

> インターフェース番号 (`1-1:1.0`) は `ls /sys/bus/usb/drivers/ftdi_sio/` で確認できます。

### 4. テスト実行

```bash
wsl -d Ubuntu-24.04 -u root bash -c \
  "/mnt/e/path/to/D2XXTest/build_wsl/test_debug_static"
```

### デタッチ (Windows 側に戻す場合)

```powershell
usbipd detach --busid <BUSID>
```

---

## Usage

### Run all tests

```powershell
# Windows
.\build\bin\Release\test_open.exe
.\build\bin\Release\test_loopback.exe
.\build\bin\Release\test_eeprom_ftdi.exe
.\build\bin\Release\test_eeprom_spi.exe
.\build\bin\Release\test_debug.exe
```

```bash
# Linux
./build/bin/test_open
./build/bin/test_loopback
./build/bin/test_eeprom_ftdi
./build/bin/test_eeprom_spi
./build/bin/test_debug
```

Default channel index is **1** (FT4232H Channel B).
Pass a different index as the first argument:

```bash
./build/bin/test_open 0     # Channel A
```

### 4-step debug procedure

`test_debug` implements the FT4232HL SPI debug guide:

1. **Open** — verify CS#=3.3 V (High) and SCK=0 V (Low) after MPSSE init
2. **MPSSE echo-back** — confirm `0xAA → 0xFA 0xAA` (verified in constructor)
3. **SCK toggle** — drive SCK High / Low and read back via `FT_GetBitMode`
4. **SPI transfer** — send `{0xAA, 0x55, 0xFF, 0x00}` with internal loopback, verify TX == RX

```bash
# Linux
./build/bin/test_debug        # channel 1 (default = FT4232H CH-B)
./build/bin/test_debug 0      # channel 0 (CH-A)
```

```powershell
# Windows
.\build\bin\Release\test_debug.exe
.\build\bin\Release\test_debug.exe 0
```

---

## Quick API Reference

```cpp
#include "d2xx_mpsse/MpsseDevice.hpp"
#include "d2xx_mpsse/SpiMaster.hpp"
#include "d2xx_mpsse/Gpio.hpp"
#include "d2xx_mpsse/Eeprom.hpp"

// Open Channel B at 4 MHz
d2xx::MpsseDevice dev(1, 4'000'000);

// SPI transfer (full-duplex)
d2xx::SpiMaster spi(dev);
uint8_t tx[] = {0x9F};
uint8_t rx[3];
spi.csLow();
spi.transfer(tx, nullptr, 1);       // send command
spi.transfer(nullptr, rx, 3);       // receive 3 bytes
spi.csHigh();

// GPIO — high byte
d2xx::Gpio gpio(dev, d2xx::Gpio::Bank::High);
gpio.setDirection(0xFF);            // all outputs
gpio.write(0xA5);
uint8_t pins = gpio.read();

// FTDI internal EEPROM
d2xx::Eeprom eeprom(dev);
auto words = eeprom.readFtdiInternal(0, 64);
// words[1] == 0x0403  (VID)
// words[2] == PID

// External SPI EEPROM (25-series, 2-byte address)
d2xx::Eeprom spiEeprom(dev, spi);
auto data = spiEeprom.readSpiEeprom(0x000000, 32, 2);
```

---

## MPSSE Protocol Notes

| Command | Meaning |
|---|---|
| `0x86 DivL DivH` | Set SCK divisor (60 MHz / ((1 + Div) × 2)) |
| `0x80 Val Dir` | Set low-byte GPIO (SCK/MOSI/MISO/CS# + bits4-7) |
| `0x81` | Read low-byte GPIO |
| `0x82 Val Dir` | Set high-byte GPIO |
| `0x83` | Read high-byte GPIO |
| `0x84` | Enable internal loopback |
| `0x85` | Disable internal loopback |
| `0x87` | Send Immediate (flush USB packet) |
| `0x31 LenL LenH data…` | Clock bytes out on MOSI + in on MISO (SPI Mode 0) |

---

## License

This repository contains only original C++ source code.
The FTDI D2XX SDK (ftd2xx.h, WinTypes.h, ftd2xx.lib) is **not** included and must
be obtained separately from [FTDI](https://ftdichip.com/drivers/d2xx-drivers/)
under FTDI's own license terms.
