# moco

`moco` 是一个用 C 编写的 Minecraft 启动器/管理工具，提供初始化、登录、安装、更新、搜索和启动游戏等基础命令。
目前处于早期开发阶段，功能和稳定性可能有限，但目标是成为一个轻量级、跨平台的 Minecraft 命令行启动器解决方案。

## 依赖

### 系统依赖

- GCC / GNU C 编译器
- CMake 3.16+
- `libcurl4-openssl-dev`
- `make` 或 Ninja（取决于你的构建生成器）
- `Threads`、OpenSSL/`crypto` 相关系统库（通常由发行版提供）

### 代码依赖

项目会通过 CMake 自动拉取并构建：

- `cJSON`
- `tomlc17`

## 构建

暂时只支持在 Linux 下构建：

```bash
make clean && make build
```

## 使用

`moco` 的常见命令如下：

```bash
moco init
moco login
moco install
moco update
moco search version <verion>
moco start
```

### 命令说明

- `init`：初始化工作目录和配置文件
- `login`：登录并保存账号信息
- `install`：下载并准备 Minecraft 版本
- `update`：更新资源或依赖
- `search`：搜索可用版本/资源
- `start`：根据本地配置组装启动参数并启动游戏

## 运行前准备

`start` 命令通常需要以下文件/目录存在：

- `instance.toml`
- `.moco/profile.toml`
- `.moco/account.toml`
- `.minecraft/versions/version.json`

如果缺少这些文件，请先执行 `init`、`login`、`install` 等命令完成准备。