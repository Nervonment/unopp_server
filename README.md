# unopp_server
unopp 是一个联机桌游应用。本仓库是 unopp 的服务端。

## 依赖
unopp_server 依赖于 C++ Boost 库、Sqlite3。在构建前请在你的机器上安装这两个依赖。下面是可参考的安装方法（Ubuntu）。

### C++ Boost
直接使用 `apt-get` 安装即可。
```sh
sudo apt-get install libboost-all-dev
```

### Sqlite3
首先从 Sqlite3 官网下载 sqlite-autoconf-*.tar.gz。在写下这篇文档的时候，可以使用以下命令：
```sh
wget https://www.sqlite.org/2024/sqlite-autoconf-3450100.tar.gz
```
然后进行配置和编译：
```sh
tar xvzf sqlite-autoconf-3450100.tar.gz
cd sqlite-autoconf-3450100
./configure --prefix=/usr/local
make
make install
```

## 构建
本项目使用 CMake 构建。输出文件为 `build/unopp_server`。
```sh
cd unopp_server
cmake .
make
```

*如果目录 `SQLiteCpp` 是空的，请先执行下面的命令下载子模块。*
```
git submodule update --init --recursive
```

构建之后，请先运行 `setup.sh` 初始化数据库，然后再运行服务端
```sh
./setup.sh
./build/unopp_server
```

unopp_server 会监听 `1145` 和 `1146` 端口。其中 `1145` 用于 WebSocket 连接，`1146` 用于 Http 连接。你可以在 `main.cpp` 中修改监听的端口。端口的配置需和前端/客户端中一致，详见前端/客户端仓库。

## 参考
本项目使用了下面的开源项目：  
[JsonCpp](https://github.com/open-source-parsers/jsoncpp)  
[cpp-httplib](https://github.com/yhirose/cpp-httplib)  
[WebSocket++](https://github.com/zaphoyd/websocketpp)  
[SQLiteC++](https://github.com/SRombauts/SQLiteCpp)  