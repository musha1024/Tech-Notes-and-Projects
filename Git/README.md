# GitHub 使用指南（从拉取到同步）

> 本文档教你如何：**把 GitHub 仓库拉到本地 → 修改代码 → 同步回 GitHub**。  
> 每一部分都可折叠/展开。

---

<details>
<summary>一、环境准备</summary>

git --version
git config --global user.name "你的名字或ID"
git config --global user.email "你的GitHub绑定邮箱"

yaml
复制代码
</details>

---

<details>
<summary>二、选择连接方式（HTTPS / SSH）</summary>

### HTTPS（简单）
git clone https://github.com/owner/repo.git

shell
复制代码
> 首次 push 需要输入 Personal Access Token（代替密码）

### SSH（推荐）
生成密钥
ssh-keygen -t ed25519 -C "你的邮箱"

添加公钥 (~/.ssh/id_ed25519.pub) 到 GitHub: Settings → SSH and GPG keys
测试
ssh -T git@github.com


<summary>三、克隆仓库</summary>

cd ~/code
git clone git@github.com:owner/repo.git
cd repo


<summary>四、本地修改并提交</summary>

git status
git add .
git commit -m "feat: 新增 xxx 功能"


<summary>五、推送到 GitHub</summary>

已有分支
git push

新建分支
git checkout -b feature/awesome
git push -u origin feature/awesome


<summary>六、同步远端更新</summary>

保持线性历史
git pull --rebase origin main

解决冲突后
git add <file>
git rebase --continue


<summary>七、分支协作</summary>

查看分支
git branch -a

新建分支
git checkout -b feature/login

合并到主分支
git checkout main
git pull --rebase origin main
git merge --no-ff feature/login
git push


<summary>八、常见操作</summary>

### 删除文件
git rm file
git commit -m "chore: 删除 file"
git push

shell
复制代码

### 重命名文件
git mv old.cpp new.cpp
git commit -m "refactor: 文件重命名"
git push

shell
复制代码

### 忽略文件
/build/
/out/
/.vscode/
/.idea/
/.DS_Store


<summary>九、常见问题</summary>

### 身份信息缺失
git config --global user.name "Your Name"
git config --global user.email "you@example.com"

shell
复制代码

### non-fast-forward
git pull --rebase origin main
git push

shell
复制代码

### 设置代理
git config --global http.proxy http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890

shell
复制代码

### 撤销/回退
git restore --staged <file> # 取消暂存
git restore <file> # 丢弃修改
git reset --soft HEAD^ # 回退提交，保留改动
git reset --hard HEAD^ # 回退并丢弃修改


<summary>十、速查表 (Cheat Sheet)</summary>

克隆
git clone git@github.com:owner/repo.git
cd repo

修改 → 提交 → 推送
git add .
git commit -m "feat: xxx"
git push

新建分支
git checkout -b feature/xxx
git push -u origin feature/xxx

同步更新
git pull --rebase origin main
