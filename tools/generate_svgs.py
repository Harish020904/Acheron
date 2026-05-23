import os

SVG_DIR = "assets/generated/svg"
os.makedirs(SVG_DIR, exist_ok=True)

svgs = {
    "district_res.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <rect width="64" height="64" fill="none"/>
    <path d="M12 48 L12 28 L32 12 L52 28 L52 48 Z" fill="none" stroke="#00FF99" stroke-width="4"/>
    <rect x="26" y="32" width="12" height="16" fill="none" stroke="#00FF99" stroke-width="3"/>
</svg>''',
    
    "district_com.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <rect width="64" height="64" fill="none"/>
    <rect x="16" y="16" width="32" height="40" fill="none" stroke="#00CCFF" stroke-width="4"/>
    <rect x="22" y="24" width="20" height="8" fill="none" stroke="#00CCFF" stroke-width="2"/>
    <rect x="22" y="36" width="20" height="8" fill="none" stroke="#00CCFF" stroke-width="2"/>
</svg>''',

    "district_ind.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <rect width="64" height="64" fill="none"/>
    <path d="M12 52 L12 24 L24 24 L24 12 L36 24 L44 24 L44 16 L52 24 L52 52 Z" fill="none" stroke="#FFCC00" stroke-width="4"/>
</svg>''',

    "congestion_warning.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <circle cx="32" cy="32" r="28" fill="none" stroke="#FF3333" stroke-width="4" stroke-dasharray="10 5"/>
    <path d="M22 32 L42 32 M32 22 L32 42" stroke="#FF3333" stroke-width="6"/>
</svg>''',

    "power_outage.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <path d="M36 4 L20 32 L32 32 L28 60 L44 32 L32 32 Z" fill="none" stroke="#FF3333" stroke-width="4"/>
</svg>''',

    "economy_up.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <path d="M16 48 L48 16 M32 16 L48 16 L48 32" fill="none" stroke="#00FF99" stroke-width="6"/>
</svg>''',

    "economy_down.svg": '''<svg width="64" height="64" xmlns="http://www.w3.org/2000/svg">
    <path d="M16 16 L48 48 M32 48 L48 48 L48 32" fill="none" stroke="#FF3333" stroke-width="6"/>
</svg>'''
}

for name, content in svgs.items():
    path = os.path.join(SVG_DIR, name)
    with open(path, "w") as f:
        f.write(content)

print(f"Generated {len(svgs)} SVG files in {SVG_DIR}")
