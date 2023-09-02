# ファイル同期

ネットワークでファイルの同期をしたい。

## 環境

Mac Studio(M1 ultra)でビルド・テストしています。

## ビルド

```shell
git submodule init
git submodule update --recursive
mkdir build
cd build
cmake -G Ninja ..
ninja
```

## 実行

共通オプション

- v :詳細表示モード(verbose)
- p <port> :ポート指定

### サーバー

```fsrv [options] <path>```で実行。

オプションは
- r ディレクトリ再帰

```shell
fsrc -r contents
```

### クライアント

```fcli [options] <url> <path> <command> [prefix]```で実行。

コマンドは
- dir ディレクトリの表示
- files ファイルの表示
- sync ファイルの同期

```prefix```は必要なファイルのみを抽出したい場合に、先頭部分にマッチする文字列を指定する。

```shell
fcli -r localhost images files
```
