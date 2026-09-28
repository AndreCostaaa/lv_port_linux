# Icons

A set of basic UI icons sourced from [Lucide](https://lucide.dev)
([MIT licensed](https://github.com/lucide-icons/lucide/blob/main/LICENSE)).

## Layout

```
images/icons/
├── raw/            256×256 source PNGs — black stroke, transparent background
├── gen_icons.py    downloader / renderer (Lucide SVG → raw PNG)
└── README.md       this file
```

`raw/*.png` are **sources**, not the assets the UI loads directly. The
`<convert>` entries in `globals.xml` downscale each raw PNG to `#icon_size`
(`globals.xml` → `int name="icon_size"`) and emit the runtime asset at
`images/icons/<name>.png`. Icons are drawn black so they recolor cleanly at
the use-site; the alpha channel carries the shape.

## Registering an icon in `globals.xml`

Each icon needs two lines inside the `<images>` block — a `<convert>` (raw →
sized runtime PNG) and a `<data>` (the loadable asset):

```xml
<convert src="images/icons/raw/home.png" dest="images/icons/home.png" width="#icon_size" color_format="argb8888" />
<data name="home" src_path="images/icons/home.png" color_format="argb8888" />
```

Reference the registered `name` from a widget (e.g. an `lv_image` `src`).

## Adding new icons

Browse names at <https://lucide.dev/icons/>, then run the generator from this
folder:

```bash
# one or more icons — 'lucide-name' or 'lucide-name:output-name'
python3 gen_icons.py wind volume-x:mute battery-charging

# regenerate the entire default set
python3 gen_icons.py

# list the default set / change stroke color or size
python3 gen_icons.py --list
python3 gen_icons.py sun --color '#FFFFFF' --size 512
```

The run prints the matching `<convert>` / `<data>` lines to paste into the
`<images>` block of `globals.xml`.

### Requirements

- `cairosvg` — `pip install --break-system-packages cairosvg`
  (needs the system `libcairo2`).
- Network access to `raw.githubusercontent.com`.

## Regenerating runtime assets

After editing `globals.xml` or adding raw PNGs, re-run the editor pipeline
from the project root:

```bash
lved generate .
```
