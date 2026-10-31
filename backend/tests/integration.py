# Live upstream smoke test; detailed deterministic coverage is in memory_integration.py.
import json, os, urllib.request, urllib.error
base = os.environ.get('BASE_URL', 'http://127.0.0.1:8081')
url = base + '/signatures/Lynx%20Titan.png?design=classic&background=blue'
with urllib.request.urlopen(url, timeout=90) as response:
    png = response.read()
    etag = response.headers['ETag']
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
try:
    urllib.request.urlopen(urllib.request.Request(url, headers={'If-None-Match':etag}), timeout=90)
    raise AssertionError('Expected 304')
except urllib.error.HTTPError as error:
    assert error.code == 304
print('PASS: live hiscores PNG and ETag revalidation')
