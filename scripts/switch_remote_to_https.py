#!/usr/bin/env python3
"""
将 Git 远程 origin 从 SSH 改为 HTTPS，便于在没有配置 SSH 密钥时推送。
用法：在仓库根目录执行 python scripts/switch_remote_to_https.py
"""
import subprocess
import sys
import re


def get_remote_url():
    r = subprocess.run(
        ["git", "remote", "get-url", "origin"],
        capture_output=True,
        text=True,
    )
    if r.returncode != 0:
        print("未找到远程 origin，或当前目录不是 Git 仓库。")
        return None
    return r.stdout.strip()


def ssh_to_https(url):
    """git@github.com:user/repo.git -> https://github.com/user/repo.git"""
    m = re.match(r"git@github\.com:(.+?)(\.git)?$", url)
    if m:
        repo = m.group(1)
        return f"https://github.com/{repo}.git" if not repo.endswith(".git") else f"https://github.com/{repo}"
    if url.startswith("https://github.com/"):
        print("当前已是 HTTPS 地址，无需切换。")
        return None
    print("无法识别该远程地址格式，仅支持 github.com 的 SSH 地址。")
    return None


def main():
    url = get_remote_url()
    if not url:
        sys.exit(1)
    print(f"当前 origin: {url}")
    new_url = ssh_to_https(url)
    if not new_url:
        sys.exit(1)
    print(f"将改为:     {new_url}")
    r = subprocess.run(["git", "remote", "set-url", "origin", new_url])
    if r.returncode != 0:
        print("设置失败。")
        sys.exit(1)
    print("已切换为 HTTPS。下次 push 时请使用 GitHub 用户名和 Personal Access Token 作为密码。")
    print("（Token 在 GitHub → Settings → Developer settings → Personal access tokens 中创建）")


if __name__ == "__main__":
    main()
