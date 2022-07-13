# USB デバイスを MikanOS に接続する方法

この文書では、ホスト OS に接続された USB デバイスを MikanOS に接続する方法を説明
します。Windows 11 上の WSL2 で動作する Ubuntu 20.04 を前提としますが、他のバー
ジョンでも参考になると思います。

## Windows に接続された USB デバイスを Ubuntu に認識させる方法

IP ネットワークで USB デバイスを共有する USB/IP という仕組みを用いると、Windows
（ホスト OS）に接続された USB デバイスを Ubuntu on WSL から使えるようにできます。
詳しい方法は [USB デバイスを接続する | Microsoft Docs][usbipd] を参照してくださ
い。

[usbipd]: https://docs.microsoft.com/ja-jp/windows/wsl/connect-usb

以降では usbipd コマンドが使用可能になっている前提で説明します。WSL を使わず、実
機で Ubuntu を実行している場合はこの手順をスキップして次に進んでください。

tools/attach_usbdev.sh は usbipd コマンドを使用して USB デバイスを Ubuntu に接続
するためのスクリプトです。引数無しで実行すると、Windows に接続されている USB デ
バイスのリストが表示されます。

    $ tools/attach_usbdev.sh
    Please specify a bus id to be attached
    BUSID  DEVICE                                 STATE
    ...
    2-4    USB シリアル デバイス                  Not attached

例えば BUSID = 2-4 が希望する USB デバイスだとします。希望するデバイスがどれか分
からない場合、USB デバイスを抜いた状態と挿した状態それぞれで attach_usbdev.sh を
実行すれば判明すると思います。

Ubuntu に接続したい USB デバイスの BUSID が分かったら、それを引数としてもう一度
スクリプトを実行します。

    $ tools/attach_usbdev.sh 2-4
    Attaching a specified device: BUS_ID = 2-4

実行すると、「このアプリがデバイスに変更を加えることを許可しますか？」という
Windows のユーザーアカウント制御ダイアログが出ますので「はい」を選んでください。
次に「[sudo] password for ユーザー名:」のようなプロンプトが表示されると思います。
ここには Ubuntu のログインパスワードを入力してください（sudo コマンドの実行時に
入力するパスワードです）。

これで指定した USB デバイスが Ubuntu に接続されたはずです。attach_usbdev.sh を実
行してきちんと接続されたことを確認してください。STATE が Attached になっていれば
成功です。

    $ tools/attach_usbdev.sh
    Please specify a bus id to be attached
    BUSID  DEVICE                                 STATE
    ...
    2-4    USB シリアル デバイス                  Attached - Ubuntu-20.04

## MikanOS に USB デバイスを接続する

Ubuntu に USB デバイスが接続されたら、lsusb コマンドでバス番号とデバイス番号を確
認します。

    $ lsusb
    Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
    Bus 001 Device 003: ID 04d8:000a Microchip Technology, Inc. CDC RS-232 Emulation Demo
    Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub

実機で Ubuntu を実行している場合はもっとたくさんのデバイスが表示されると思います。
上記の例は Ubuntu on WSL なので、最低限のデバイスしか見えていませんね。「Linux
Foundation」と書いてあるデバイスは Ubuntu に最初から接続されているものです。ここ
では「Microchip Technology, Inc.」と書いてあるデバイスを MikanOS に接続するとし
て説明を進めます。

MikanOS を動かす QEMU がこの USB デバイスを読み書きできるようにするため、デバイ
スファイルのパーミッションを調整します。lsusb の出力で Bus と Device の番号を調
べ（ここでは Bus 001、Device 003）、デバイスファイルを探します。この USB デバイ
スに対応するデバイスファイルは /dev/bus/usb/<Bus>/<Device> にあります。

    $ ls -l /dev/bus/usb/001/003
    crw------- 1 root root 189, 2 Jul  6 17:46 /dev/bus/usb/001/003

初期状態では、パーミッションが 600（rw-------）になっていることが分かります。一
般ユーザーが読み書きできるよう、666（rw-rw-rw-）に変更します。

    $ sudo chmod 666 /dev/bus/usb/001/003

ここまできたら、QEMU に USB デバイスを接続するオプションを渡して MikanOS を起動
すれば、MikanOS に USB デバイスを接続することができます。QEMU のオプションは
`-device usb-host,hostbus=<Bus>,hostaddr=<Device>` という形式です。Bus=1、Device
=3 ですので、最終的に次のコマンドとなります。

    QEMU_OPTS="-device usb-host,hostbus=1,hostaddr=3" ./run.sh

run.sh は MikanOS を手軽に実行するためのスクリプトです。QEMU_OPTS 変数に指定した
文字列は QEMU のオプションとして渡されます。

## USB シリアル変換の速度設定

MikanOS は現在、CDC ACM という USB クラスを試験的にサポートしています。CDC ACM
は通信を主目的とする USB デバイスのためのクラスで、USB シリアル変換やイーサネッ
トなどの通信デバイスが該当します。先の例で接続した「CDC RS-232 Emulation Demo」
は USB とシリアル通信（RS-232）を変換するデバイスです。

シリアル通信は通信速度を相手側デバイスと合わせなければ正しく通信できません。
MikanOS にはまだ CDC ACM にしたがって通信速度を設定する機能がありませんので、外
部から通信速度を設定する必要があります。CDC ACM 機器が Linux に接続されると、初
期値として 9600bps に設定されるようです。それ以外の速度に変更するためには、QEMU
を起動する前に次のコマンドを実行します。

    $ sudo stty -F /dev/ttyACM0 115200

`/dev/ttyACM0` は実際のデバイスファイル名に書き換えてください。`115200` はお好み
の通信速度（ボーレート）に変えてください。

どうやら QEMU を終了すると設定が 9600 のデフォルト値に戻ってしまうようですので、
QEMU を起動する前に毎回上記のコマンドを実行する必要があります。
