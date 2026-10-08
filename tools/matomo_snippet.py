#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""One Matomo count for designburgapps.com, dx7.designburgapps.com and zp12.designburgapps.com (site ID 1 on the
self-hosted matomo.designburgapps.com): every page title starts with its host, so the three are told apart in the
reports. No cookies, Do Not Track respected (IPs are anonymised on the server).
    python3 tools/matomo_snippet.py FILE.html ...   replaces the Matomo block of each page, or puts it before </body>"""
import sys
from pathlib import Path

BLOCK = """<!-- Matomo (self-hosted, matomo.designburgapps.com): one count for designburgapps.com, dx7. and zp12., the page
     title with its host; no cookies, IPs anonymised on the server, Do Not Track respected -->
<script>
  var _paq = window._paq = window._paq || [];
  _paq.push(['setDocumentTitle', location.hostname + ' / ' + document.title]);
  _paq.push(['setDomains', ['designburgapps.com', '*.designburgapps.com']]);
  _paq.push(['disableCookies']);
  _paq.push(['setDoNotTrack', true]);
  _paq.push(['trackPageView']);
  _paq.push(['enableLinkTracking']);
  (function() {
    var u="https://matomo.designburgapps.com/";
    _paq.push(['setTrackerUrl', u+'matomo.php']);
    _paq.push(['setSiteId', '1']);
    var d=document, g=d.createElement('script'), s=d.getElementsByTagName('script')[0];
    g.async=true; g.src=u+'matomo.js'; s.parentNode.insertBefore(g,s);
  })();
</script>
<!-- End Matomo Code -->"""

def apply(p):
    s = p.read_text(encoding="utf-8")
    a, b = s.find("<!-- Matomo"), s.find("<!-- End Matomo Code -->")
    if a >= 0 and b > a:
        s2 = s[:a] + BLOCK + s[b + len("<!-- End Matomo Code -->"):]
    else:
        i = s.rfind("</body>")
        if i < 0:
            return f"{p}: no </body>, left alone"
        s2 = s[:i] + BLOCK + "\n" + s[i:]
    if s2 != s:
        p.write_text(s2, encoding="utf-8")
        return f"{p}: set"
    return f"{p}: as it was"

if __name__ == "__main__":
    for f in sys.argv[1:]:
        print(apply(Path(f)))
