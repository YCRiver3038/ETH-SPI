# ETH-SPI

EthernetほかLANを経由して、Raspberry Piからその他の機器へSPIで取得したデータを送信するプログラム

## 動機

AD変換ICを使ってオシロスコープにしたいけどC++でグラフをリアルタイムでプロットする方法を知らない主が、
苦節の末「ラズパイでSPIでデータ取ってイーサネットで送信すればあとは高性能PCでプロットすればええやん？」という結論に至り、
それを実行したかった

## ファイルの内容

* for_raspi/ - Raspberry Pi向けソースコード
    * es_main.cpp
        * raspiで実行するメインプログラム
    * GPIO.*
        * GPIO用コード類
    * network.*
        * LAN(socket)で送信するためのコード類
    * buffers.hpp
        * バッファの簡単な実装
* rt_plot.py
    * 受信側のPCでプロットするためのプログラム
* README.md
    * このファイル

## 依存ライブラリ等

* rt_plot.py 依存パッケージ
    * pyqtgraph
    * numpy
* for_raspi
    * 依存ライブラリ
        * pigpio
    * C++標準
        * 想定: C++17
        * これ以前については未確認

## コンパイル

for_raspi以下について、ビルドにRaspberry Piを用いることを条件に下のコマンド  

```
g++ -std=c++17 es_main.cpp GPIO.cpp network.cpp -lpigpio -lpthread
```

にてコンパイルできるはずです。  
できなかった場合は以下を確認してみてください。

* 依存ライブラリはインストールされているか
* パスは通っているか

## SPIで接続するデバイスについて
動作が確認できているのはMCP3002I/Pのみです。
その他のSPI ADCを使う場合は、SPIで送信するデータを対象のデバイスに合わせたものにしてコンパイルしてください  
※該当する変数： `u_rw_ds w_data` (関数 `thr_read_spi_data` 内)  
ここは、そのうち何かしらの手段を以てコンパイル後に変更できるようにしたいと思っています。

## 動作確認環境
### Raspberry Pi

HW: Raspberry Pi Model B V1.1  
OS: Raspberry Pi OS 32bit  
(Linux raspberrypi 5.15.61-v7+ #1579 SMP Fri Aug 26 11:10:59 BST 2022 armv7l GNU/Linux)  
SPI connected device: MCP3002I/P @3.3V

### Desktop PC

#### Python
Python 3.10.5

#### OS
Edition: Windows 11 Home  
Version: 21H2  
Build: 22000.978  

#### Hardware
CPU: AMD Ryzen 5 5600X  
RAM: 32GB DDR4 3600MHz  
GPU: NVIDIA GeForce GTX 1650 Super  

