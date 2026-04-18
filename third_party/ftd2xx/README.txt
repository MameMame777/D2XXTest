FTDI D2XX SDK — 配置ガイド
==========================

このディレクトリに FTDI D2XX SDK のファイルを配置してください。
SDK は https://ftdichip.com/drivers/d2xx-drivers/ からダウンロードできます。

---

## Windows の場合 (推奨: 64ビット静的リンク)

CDM ドライバパッケージの ZIP を展開し、以下の構成になるよう配置してください。

### 最小構成 (64ビット向け — 静的リンク、DLL 不要)

  third_party/ftd2xx/
  ├── ftd2xx.h
  └── Static/
      └── amd64/
          └── ftd2xx.lib    ← 静的ライブラリ (FT_StatLib.lib と呼ばれる場合もあり)

### インポートライブラリ (DLL あり)

  third_party/ftd2xx/
  ├── ftd2xx.h
  ├── amd64/
  │   ├── ftd2xx.lib        ← インポートライブラリ
  │   └── ftd2xx.dll        ← ← ← 実行ファイルと同じフォルダに必要
  └── i386/
      ├── ftd2xx.lib
      └── ftd2xx.dll

CMake は Static/amd64/ftd2xx.lib を優先検索し、なければ amd64/ftd2xx.lib を使います。
DLL を選んだ場合、実行時に build/bin/ へ自動コピーされます。

---

## Linux の場合

  third_party/ftd2xx/
  ├── ftd2xx.h
  └── libftd2xx.so (または libftd2xx.a)

または LD_LIBRARY_PATH でシステムの libftd2xx を指定しても構いません。
FT4232H 使用時は ftdi_sio カーネルモジュールをアンバインドしてください:
  sudo rmmod ftdi_sio

---

## ビルド例 (Windows, 64ビット MSVC)

  cmake -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release

実行ファイルは build/bin/Release/ に生成されます。

---

## ビルド例 (Linux / GCC)

  cmake -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j$(nproc)

実行ファイルは build/bin/ に生成されます。
