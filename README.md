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
