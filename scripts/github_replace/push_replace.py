import os
import sys
import argparse
import base64
import json
import urllib.parse
from pathlib import Path

try:
    import requests
except Exception:
    print('Missing dependency: requests. Run: python -m pip install requests')
    sys.exit(1)


GITHUB_API = 'https://api.github.com'


def to_posix_path(p: str) -> str:
    return p.replace('\\', '/').lstrip('/')


def get_file_sha(owner, repo, path, branch, token):
    url = f"{GITHUB_API}/repos/{owner}/{repo}/contents/{path}"
    headers = {'Authorization': f'token {token}', 'Accept': 'application/vnd.github.v3+json'}
    params = {'ref': branch}
    r = requests.get(url, headers=headers, params=params)
    if r.status_code == 200:
        return r.json().get('sha')
    elif r.status_code == 404:
        return None
    else:
        raise RuntimeError(f'GET {url} -> {r.status_code}: {r.text}')


def put_file(owner, repo, path, branch, token, content_b64, message, sha=None):
    url = f"{GITHUB_API}/repos/{owner}/{repo}/contents/{path}"
    headers = {'Authorization': f'token {token}', 'Accept': 'application/vnd.github.v3+json'}
    data = {'message': message, 'content': content_b64, 'branch': branch}
    if sha:
        data['sha'] = sha
    r = requests.put(url, headers=headers, data=json.dumps(data))
    return r


def push_folder(owner, repo, src_folder, branch, token, commit_msg, dry_run=False):
    src = Path(src_folder)
    if not src.exists():
        raise FileNotFoundError(f'src folder not found: {src_folder}')

    files = [p for p in src.rglob('*') if p.is_file()]
    # Exclude repository internals and build artifacts
    EXCLUDE_DIRS = {'.git', '.pio', '__pycache__'}
    SKIP_EXTS = {'.o', '.elf', '.hex', '.a', '.bin', '.pyc'}
    MAX_FILE_SIZE = 50 * 1024 * 1024  # 50 MB
    filtered = []
    for p in files:
        if any(part in EXCLUDE_DIRS for part in p.parts):
            continue
        if p.suffix.lower() in SKIP_EXTS:
            continue
        try:
            if p.stat().st_size > MAX_FILE_SIZE:
                print(f'Skipping large file: {p} ({p.stat().st_size} bytes)')
                continue
        except Exception:
            pass
        filtered.append(p)
    files = filtered
    if not files:
        print('No files to push.')
        return

    for f in files:
        rel = f.relative_to(src)
        repo_path = to_posix_path(str(rel))
        with f.open('rb') as fh:
            data = fh.read()
        content_b64 = base64.b64encode(data).decode('ascii')

        print(f'Processing: {repo_path} ({f.stat().st_size} bytes)')

        if dry_run:
            print('  dry-run: would fetch sha and PUT content')
            continue

        try:
            # URL-encode path segments to handle non-ASCII filenames
            encoded_repo_path = urllib.parse.quote(repo_path, safe='/')
            sha = get_file_sha(owner, repo, encoded_repo_path, branch, token)
        except Exception as e:
            print(f'  ERROR getting sha: {e}')
            continue

        try:
            r = put_file(owner, repo, encoded_repo_path, branch, token, content_b64, commit_msg, sha)
            if r.status_code in (200, 201):
                print(f'  OK -> {r.status_code}');
            else:
                print(f'  FAIL -> {r.status_code}: {r.text}')
        except Exception as e:
            print(f'  ERROR putting file: {e}')


def main(argv=None):
    p = argparse.ArgumentParser(description='Push entire folder replacing files on GitHub via Contents API')
    p.add_argument('owner')
    p.add_argument('repo')
    p.add_argument('src_folder')
    p.add_argument('branch', nargs='?', default='main')
    p.add_argument('commit_msg', nargs='?', default='Replace files via script')
    p.add_argument('--token', help='GitHub token (overrides GITHUB_TOKEN env)')
    p.add_argument('--dry-run', action='store_true')
    args = p.parse_args(argv)

    token = args.token or os.environ.get('GITHUB_TOKEN')
    if not token:
        print('GITHUB_TOKEN not set. Provide via env or --token')
        sys.exit(1)

    push_folder(args.owner, args.repo, args.src_folder, args.branch, token, args.commit_msg, dry_run=args.dry_run)


if __name__ == '__main__':
    main()
