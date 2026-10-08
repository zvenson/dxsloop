#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Matomo for designburgapps.com and its subdomains (self-hosted matomo.designburgapps.com): one Matomo site per host,
picked by the page's hostname, so each has its own reports (dx7 = 1, zp12 = 3, fm1 = 4, designburgapps.com = 5).
No cookies, Do Not Track respected (IPs are anonymised on the server); the heartbeat measures the time on a page.
    python3 tools/matomo_snippet.py FILE.html ...   replaces the Matomo block of each page, or puts it before </body>"""
import sys
from pathlib import Path

BLOCK = """<!-- Matomo (self-hosted, matomo.designburgapps.com): one site per host (dx7 1, zp12 3, fm1 4, designburgapps.com 5);
     no cookies, IPs anonymised on the server, Do Not Track respected -->
<script>
  var _paq = window._paq = window._paq || [];
  var _mtmSite = {"zp12.designburgapps.com": "3", "fm1.designburgapps.com": "4", "designburgapps.com": "5",
                  "www.designburgapps.com": "5"}[location.hostname] || "1";
  _paq.push(['setDomains', [location.hostname]]);
  _paq.push(['disableCookies']);
  _paq.push(['setDoNotTrack', true]);
  _paq.push(['enableHeartBeatTimer']);
  _paq.push(['trackPageView']);
  _paq.push(['enableLinkTracking']);
  (function() {
    var u="https://matomo.designburgapps.com/";
    _paq.push(['setTrackerUrl', u+'matomo.php']);
    _paq.push(['setSiteId', _mtmSite]);
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
