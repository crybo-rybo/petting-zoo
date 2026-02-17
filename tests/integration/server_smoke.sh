#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "usage: $0 <server_binary> <curl_binary>" >&2
  exit 2
fi

SERVER_BIN="$1"
CURL_BIN="$2"
BASE_URL="http://127.0.0.1:8080"

TMP_DIR="$(mktemp -d)"
SERVER_LOG="${TMP_DIR}/server.log"
BODY_FILE="${TMP_DIR}/body.json"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "${SERVER_PID}" >/dev/null 2>&1 || true
    wait "${SERVER_PID}" 2>/dev/null || true
  fi
  rm -rf "${TMP_DIR}"
}
trap cleanup EXIT

"${SERVER_BIN}" >"${SERVER_LOG}" 2>&1 &
SERVER_PID=$!

for _ in {1..40}; do
  if "${CURL_BIN}" -sS "${BASE_URL}/healthz" >/dev/null 2>&1; then
    break
  fi
  sleep 0.25
done

"${CURL_BIN}" -fsS "${BASE_URL}/healthz" | grep -q '"status":"ok"'
"${CURL_BIN}" -fsS "${BASE_URL}/api/models" | grep -q '"models"'
"${CURL_BIN}" -fsS "${BASE_URL}/" | grep -q 'Petting Zoo'

code="$("${CURL_BIN}" -sS -o "${BODY_FILE}" -w "%{http_code}" \
  -X POST "${BASE_URL}/api/chat/complete" \
  -H 'Content-Type: application/json' \
  -d '{"message":"hello"}')"

if [[ "${code}" != "409" ]]; then
  echo "expected 409 from /api/chat/complete without active model, got ${code}" >&2
  cat "${BODY_FILE}" >&2
  exit 1
fi

grep -q '"APP-STATE-409"' "${BODY_FILE}"
