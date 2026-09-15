# Git hooks（cloudzao_endpoints 开源门禁）

`pre-commit` / `pre-push`：若 `main/cloudzao_endpoints.c` 中有非空字符串字面量，则拒绝提交或推送。

## 启用

在仓库根目录执行：

```bash
git config core.hooksPath .githooks
```

## 停止

```bash
git config --unset core.hooksPath
```

仅影响当前仓库。确认是否已启用：

```bash
git config --get core.hooksPath
```

有输出 `.githooks` 表示已启用；无输出表示已停止（使用默认 `.git/hooks`）。

## 手动检查 / 置空

```bash
# 检查（非空则失败）
py -3 scripts/blank_cloudzao_endpoints.py --check

# 将所有字符串置为 ""
py -3 scripts/blank_cloudzao_endpoints.py
```
