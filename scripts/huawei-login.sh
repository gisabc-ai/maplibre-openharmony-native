#!/bin/bash
# Huawei Developer login script for GitHub Actions
# 使用 HW_CREDENTIALS secret (格式: phone:password) 登录并获取下载 token
set -e

CRED_B64="$(echo -n '$HW_CREDENTIALS' | base64)"
CRED="$(echo "$CRED_B64" | base64 -d)"
PHONE="${CRED%%:*}"
PASSWORD="${CRED##*:}"

echo "=== Huawei Developer Login ==="
echo "Login ID: ${PHONE:0:3}***${PHONE: -4}"

# ============================================================
# 方式1: 通过 connect.huawei.com OAuth2
# ============================================================
TOKEN_FILE="$1"
if [ -z "$TOKEN_FILE" ]; then
    TOKEN_FILE="/tmp/hw_token.txt"
fi

# 尝试 OAuth2 获取 token
echo "Trying OAuth2 login..."

# Step 1: 获取 authorization code (简化流程)
AUTH_RESP=$(curl -s --max-time 20 \
    -X POST "https://oauth-login.cloud.huawei.com/oauth2/v3/token" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "grant_type=password&username=${PHONE}&password=${PASSWORD}&client_id=100492903&client_secret=test" \
    2>&1)

if echo "$AUTH_RESP" | grep -q "refresh_token"; then
    REFRESH_TOKEN=$(echo "$AUTH_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('refresh_token',''))")
    echo "Got refresh token: ${REFRESH_TOKEN:0:20}..."
    echo "$REFRESH_TOKEN" > "$TOKEN_FILE"
    echo "SUCCESS"
    exit 0
fi

# ============================================================
# 方式2: 通过华为开发者中心直接登录
# ============================================================
echo "Trying developer.huawei.com login..."

# 创建工作目录
WORKDIR="/tmp/hw_login_$$"
mkdir -p "$WORKDIR"
cd "$WORKDIR"

# 获取登录页面
CSRF=$(curl -s --max-time 15 -c cookies.txt \
    "https://developer.huawei.com/consumer/cn/" 2>/dev/null | \
    grep -o 'csrf" value="[^"]*"' | head -1 | cut -d'"' -f3)

# 执行登录
LOGIN_RESP=$(curl -s --max-time 20 -c cookies.txt -b cookies.txt \
    -L -X POST "https://developer.huawei.com/consumer/cn/login" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "phone=${PHONE}&password=${PASSWORD}&csrf_token=${CSRF}" 2>&1)

# 检查登录结果
if echo "$LOGIN_RESP" | grep -q "success\|token\|dashboard"; then
    echo "Login successful via developer portal"
    echo "$LOGIN_RESP" | grep -i "token" | head -3
    echo "SUCCESS" > "$TOKEN_FILE"
    exit 0
fi

# ============================================================
# 方式3: 通过 AppGallery Connect
# ============================================================
echo "Trying AppGallery Connect..."

AG_RESP=$(curl -s --max-time 20 \
    -X POST "https://connect.huawei.com/consumer/oauth2/v3/token" \
    -H "Content-Type: application/x-www-form-urlencoded" \
    -d "grant_type=password&username=${PHONE}&password=${PASSWORD}&client_id=100492903" 2>&1)

if echo "$AG_RESP" | grep -q "refresh_token\|access_token"; then
    REFRESH_TOKEN=$(echo "$AG_RESP" | python3 -c "import sys,json; print(json.load(sys.stdin).get('refresh_token', json.load(sys.stdin).get('access_token',''))" 2>/dev/null)
    if [ -n "$REFRESH_TOKEN" ]; then
        echo "Got token: ${REFRESH_TOKEN:0:20}..."
        echo "$REFRESH_TOKEN" > "$TOKEN_FILE"
        echo "SUCCESS"
        exit 0
    fi
fi

echo "All login methods failed"
echo "Response: ${AG_RESP:0:200}"
exit 1
