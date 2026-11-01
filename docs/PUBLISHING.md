# GitHub 发布资料 / GitHub publishing materials

本套资料已经准备好仓库内容和发行附件；没有替用户创建远程仓库或上传文件。

This kit prepares repository contents and release assets. It has not created a remote repository or uploaded files on the user's behalf.

## 仓库信息 / Repository settings

仓库名 / Repository name：**AtracDEnc-AAL**

英文 About 描述 / English About description：

```text
An ATRAC Advanced Lossless (AAL3/AALP) encoder based on AtracDEnc, with a standalone CLI, C API and Windows test GUI for Windows/macOS.
```

中文介绍 / Chinese description：

```text
基于 AtracDEnc 开发的 ATRAC Advanced Lossless（AAL3/AALP）编码器，提供 Windows/macOS 命令行、C 接口及 Windows 测试界面。
```

Topics：`atrac`、`atracdenc`、`aal`、`aalp`、`lossless-audio`、`audio-codec`、`sony-walkman`、`cpp17`、`command-line`。

## 目录对应关系 / Kit layout

- `repository/`：将此目录的**内容**作为 GitHub 仓库根目录，包含 `.github/` 隐藏目录。Use its **contents** as the repository root, including the hidden `.github/` directory.
- `release-assets/`：作为 GitHub Release 的附件上传。Upload these files as Release assets.
- `RELEASE_NOTES.md`：复制到 Release 描述；仓库内相同文档位于 `docs/releases/v0.2.0.md`。Use as the Release body; the same text is in the repository.
- `PREPARATION_REPORT.json`：本地准备与包校验记录，不需要放到仓库源码中。Local preparation/package audit; it does not need to be committed.

源码和二进制已经分开组织；不要把构建目录、研究目录或旧工作区整体推送。完整源码包保留全部生产源和数据夹具。

Source and binaries are organized separately. Push the prepared repository rather than an entire old build/research workspace. The source archive retains all production code and fixtures.

## 上传仓库 / Push the repository

以下是由仓库拥有者执行的命令。需要本机 Git，以及已经登录自己账号的 GitHub CLI；本资料准备过程未执行这些远程操作。

The repository owner can run the commands below using Git and an authenticated GitHub CLI. These remote operations were not executed while preparing this kit.

在 `repository/` 中执行 / From `repository/`：

```bash
git init -b main
git add .
git commit -m "Prepare AtracDEnc-AAL 0.2.0"
gh repo create AtracDEnc-AAL --public --source . --remote origin --push
git tag v0.2.0
git push origin v0.2.0
```

也可以先在 GitHub 网页创建空仓库，再按网页显示的真实远程地址添加 `origin` 并推送。请使用自己的账号和仓库地址。

Alternatively, create an empty repository in GitHub's UI and use the actual remote URL it provides. Use your own account and repository URL.

## Release

在 GitHub 选择标签 **v0.2.0** 创建 Release，标题使用 **AtracDEnc-AAL v0.2.0**，正文复制 `RELEASE_NOTES.md`，附件上传 `release-assets/` 下的三个 ZIP 和 `SHA256SUMS.txt`。

Create a Release for tag **v0.2.0**, titled **AtracDEnc-AAL v0.2.0**, paste `RELEASE_NOTES.md` as the body, and attach the three ZIP archives and `SHA256SUMS.txt` from `release-assets/`.

当前说明保留实验状态和已知测试边界；不要把“PCM 还原一致”改写成“官方完整文件逐字节相同”，也不要把交叉编译改写成 Mac 实机验证。运行 Actions 后，可根据实际结果补充验证记录。

The notes retain experimental status and tested limits. PCM equality should not be described as complete Sony byte identity, and cross-compilation should not be described as native Mac execution. Update validation records after actual Actions runs.

GitHub CI 只构建、测试并保存 Actions 产物，不自动发布 Release。现有二进制及本地校验结果不依赖尚未执行的远程 CI。

CI only builds, tests and uploads Actions artifacts; it does not publish Releases automatically. Supplied binaries and local verification do not depend on unexecuted remote CI.
