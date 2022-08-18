## MikanOS 簡易配布セットの使い方

by uchan (2020-04-30)

mikanos_distrib-X.tar.gz を解凍すると次のファイルを得られます。

    mikanos_distrib-X/
      BOOTX64.EFI    ブートローダー
      kernel.elf     MikanOS のカーネル本体
      apps/          アプリケーション
      resource/      リソース
      install.sh     USB メモリへのインストールスクリプト
      disk.img       起動イメージファイル
      README.md      このファイル
      LICENSE        MikanOS の利用ライセンス
      NOTICE         MikanOS の著作権表示

disk.img を使うと QEMU で起動実験ができます。

USB メモリへインストールして使うには，条件を満たした USB メモリを用意して
mikanos_distrib-X ディレクトリで install.sh スクリプトを実行します。

USB メモリが /dev/sdb1 として認識されている場合，次のコマンドを実行します。
USB メモリでないデバイスを指定すると PC が破壊されるので十分注意してください。

    $ cd mikanos_distrib-X
    $ sudo umount /dev/sdb1          # まず USB メモリのマウントを解除してから
    $ sudo ./install.sh /dev/sdb1    # install.sh を叩く

mikanos_distrib-X.tar.gz の改変や再配布は LICENSE の条件に従うかぎり可能です。


### USB メモリの条件

install.sh を使うと中身がフォーマットされてしまいます。
USB メモリは中身が消えても良い物を使ってください。

ブートローダーの制約で，USB メモリの容量がある程度小さい必要があります。
おすすめは 1GB 程度です。16GB の製品でギリギリだと思います。

大きいものしかない場合，第 1 パーティションを小さくするのでも良いです。
Ubuntu なら「GNOME ディスクユーティリティ」，
Windows なら「コンピューターの管理」にある「ディスクの管理」から可能です。

install.sh を使わず USB メモリにインストールする場合，
毎回 USB メモリのフォーマットをしてください。
フォーマットせずにファイルをコピーするだけだと，USB メモリ後方のクラスタが
使用されてしまい，MikanOS がファイルにアクセスできません。


### install.sh が何をしているか

Ubuntu 以外を使っていたりして install.sh コマンドが使えない場合でも
MikanOS を試せるように，install.sh がやっていることを紹介します。
（$USB は USB メモリのルートディレクトリを表すとします。）

- USB メモリを FAT でフォーマットする
- USB メモリの内容を次の通りにする
    $USB/
      EFI/BOOT/
        BOOTX64.EFI
      kernel.elf
      apps/
        cube
        ...
      nihongo.ttf
      mikanos.txt
      ... （その他 resource/ にあるファイル）


## USB マウス・キーボード

MikanOS は USB のマウスとキーボードに対応しています。USB ハブには未対応ですので
パソコンの USB 3.0 ポートに直接接続してください。

MikanOS は PS/2 のマウスとキーボードには対応していません。ノートパソコンの
タッチパッドやキーボードは PS/2 接続の場合がありますので，反応しないなと
思ったら外付けマウスとキーボードを試してください。
