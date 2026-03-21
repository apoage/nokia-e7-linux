#!/usr/bin/env python3
"""Crawl nokiahacking.pl for relevant technical content."""

import requests
import re
import os
import time
from html import unescape

# Disable SSL warnings (site has cert issues)
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

BASE = "http://nokiahacking.pl"
OUT_DIR = "/home/lukas/ps3/e7/docs/nokiahacking-crawl"
os.makedirs(OUT_DIR, exist_ok=True)

SEARCH_TERMS = [
    "ekern", "patcher.ldd", "TCB", "capabilities", "uprawnienia",
    "kernel", "jądro", "pamięć", "memory", "register", "rejestr",
    "CapsSwitch", "PlatSec", "platsec", "fshell", "FShell",
    "readmem", "Sorcerer", "debug", "OMAP", "bootloader",
    "reverse", "hack.*kernel", "sterownik", "driver",
    "DPlatChunk", "LoadLogical", "BB5.*adres", "adresowanie",
]

SESSION = requests.Session()
SESSION.verify = False
SESSION.headers = {"User-Agent": "Mozilla/5.0 (research project)"}

visited = set()

def fetch(url):
    """Fetch a page, return text or None."""
    if url in visited:
        return None
    visited.add(url)
    try:
        r = SESSION.get(url, timeout=15)
        r.encoding = 'utf-8'
        if r.status_code == 200:
            return r.text
    except Exception as e:
        print(f"  ERR: {e}")
    return None

def strip_html(html):
    """Remove HTML tags."""
    text = re.sub(r'<[^>]+>', ' ', html)
    text = unescape(text)
    text = re.sub(r'\s+', ' ', text).strip()
    return text

def extract_links(html, base_url):
    """Extract forum thread links."""
    links = set()
    for m in re.finditer(r'href="([^"]*-vt\d+[^"]*)"', html):
        link = m.group(1)
        if not link.startswith("http"):
            link = BASE + "/" + link.lstrip("/")
        links.add(link)
    return links

def search_page(url, html):
    """Check if page contains any search terms."""
    text_lower = strip_html(html).lower()
    hits = []
    for term in SEARCH_TERMS:
        try:
            if re.search(term.lower(), text_lower):
                hits.append(term)
        except:
            if term.lower() in text_lower:
                hits.append(term)
    return hits

def save_page(url, html, hits):
    """Save relevant page."""
    slug = re.sub(r'[^\w]', '_', url.split('/')[-1])[:80]
    fname = os.path.join(OUT_DIR, f"{slug}.txt")
    text = strip_html(html)
    with open(fname, 'w') as f:
        f.write(f"URL: {url}\n")
        f.write(f"HITS: {', '.join(hits)}\n")
        f.write(f"{'='*60}\n\n")
        f.write(text)
    print(f"  SAVED: {fname} ({len(hits)} hits: {', '.join(hits)})")

def crawl_forum_section(url, name):
    """Crawl a forum section page and its threads."""
    print(f"\n=== Crawling: {name} ===")
    print(f"  URL: {url}")

    html = fetch(url)
    if not html:
        print("  Failed to fetch")
        return

    # Find thread links
    threads = extract_links(html, url)
    print(f"  Found {len(threads)} threads")

    # Also find pagination links for the forum section
    for page_match in re.finditer(r'href="([^"]*vf\d+[^"]*)"', html):
        page_url = page_match.group(1)
        if not page_url.startswith("http"):
            page_url = BASE + "/" + page_url.lstrip("/")
        page_html = fetch(page_url)
        if page_html:
            threads |= extract_links(page_html, page_url)
            time.sleep(0.5)

    print(f"  Total threads: {len(threads)}")

    # Check each thread
    for i, thread_url in enumerate(sorted(threads)):
        time.sleep(0.3)
        html = fetch(thread_url)
        if not html:
            continue
        hits = search_page(thread_url, html)
        if hits:
            save_page(thread_url, html, hits)

        # Check thread pages
        for page_match in re.finditer(r'href="([^"]*-vt\d+-\d+\.htm)"', html):
            purl = page_match.group(1)
            if not purl.startswith("http"):
                purl = BASE + "/" + purl.lstrip("/")
            time.sleep(0.3)
            phtml = fetch(purl)
            if phtml:
                phits = search_page(purl, phtml)
                if phits:
                    save_page(purl, phtml, phits)

        if (i+1) % 10 == 0:
            print(f"  Progress: {i+1}/{len(threads)}")

# Forum sections to crawl
SECTIONS = [
    (f"{BASE}/reverse-engineering-vf63.htm", "Reverse Engineering"),
    (f"{BASE}/strefa-symbian-s60-vf29.htm", "Symbian S60 Zone"),
    (f"{BASE}/modyfikacje-s60v3-vf5.htm", "Modyfikacje S60v3"),
    (f"{BASE}/modyfikacje-s60v5-1-vf21.htm", "Modyfikacje S60v5"),
    (f"{BASE}/tutoriale-vf3.htm", "Tutoriale S60"),
    (f"{BASE}/warsztat-vf35.htm", "Warsztat (Workshop)"),
    (f"{BASE}/zmodyfikowane-softy-custom-fw-symbian-3-vf32.htm", "Custom FW Symbian^3"),
]

if __name__ == "__main__":
    print("Nokia Hacking PL Crawler")
    print(f"Output: {OUT_DIR}")
    print(f"Search terms: {len(SEARCH_TERMS)}")

    for url, name in SECTIONS:
        crawl_forum_section(url, name)

    # Summary
    saved = [f for f in os.listdir(OUT_DIR) if f.endswith('.txt')]
    print(f"\n=== DONE ===")
    print(f"Pages visited: {len(visited)}")
    print(f"Pages saved: {len(saved)}")
    for f in sorted(saved):
        print(f"  {f}")
