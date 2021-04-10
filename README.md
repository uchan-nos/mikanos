# MikanOS

MikanOS はレガシーフリーなアーキテクチャ（UEFI BIOS、Intel 64 モード）で動作する教育用オペレーティングシステムです。

## ファイル構成

- MikanLoaderPkg
    - UEFI アプリとして構成したブートローダ
- kernel
    - MikanOS のカーネル
- resource/nihongo.ttf
    - IPA ゴシックのフォントファイル
- IPA_Font_License_Agreement_v1.0.txt
    - IPA フォントのライセンス文書

## ビルド方法

[mikanos-build リポジトリ](https://github.com/uchan-nos/mikanos-build/) に MikanOS をビルドするためのスクリプトがあります。
mikanos-build の手順に沿って開発ツールを導入した後、devenv/buildenv.sh を読み込むことでビルド可能です。
（devenv/buildenv.sh により環境変数 CPPFLAGS などが適切に設定されます。）

MikanOS の最新版をビルドするためには mikanos-build の最新版が必要です。

## 教科書

MikanOS の作り方を説明した教科書があります。
[ゼロからのOS自作入門](https://zero.osdev.jp/)

## スクリーンショット

「ゼロからのOS自作入門」の最終章を終えたときの姿
![30章後の姿](mikanos-after30-photo.png)

## 開発への参加

MikanOS への機能追加、バグ修正の提案は Pull Request にてお願いします。
Pull Request の出し方はこちらで説明しています。 [プルリクエストの送り方](https://github.com/uchan-nos/mikanos/blob/master/docs/how-to-send-pull-request.md)

実装が伴わない「単なる要望」は基本的に受け付けません。
実装をきちんと作ってから Pull Request を提出してください。

もし、実装したいけど力が不足して実装できない、という場合はお気軽に Issues でご連絡ください。
実装ができるようになるように、できるだけご協力いたします。
