# SPDX-License-Identifier: LGPL-2.1-or-later
"""/packaging/: how a build is named and laid out, what it bundles, and
which files a release has. Versions come from the newest build where it
has them."""

from . import content
from .buildpage import head, wrap
from .data import version_key
from .markup import el, inline, paragraphs

# A file name's parts, for the example wine-macfixes-11.21-build1-macos-x86_64.tar.xz.
PARTS = [
    ("flavour", "-", "macfixes", "flavour",
     ["**Flavour.** The patch sets applied to the source. Left out for plain WineHQ (", el("code", "wine"), "). ",
      el("a", "What each flavour is", href="/flavours/"), "."]),
    ("version", "-", "11.21", "Wine version",
     ["**Wine version.** It sets the channel: ", el("code", "X.0"), " is stable, ", el("code", "X.Y"),
      " development, ", el("code", "X.0-rcN"), " a release candidate."]),
    ("build", "-", "build1", "build number",
     ["**Build number.** Starts at 1 and goes up by one for every new build of the same flavour and version. "
      "A new build never replaces a file."]),
    ("platform", "-", "macos-x86_64", "platform",
     ["**Platform.** Each platform is a separate release, with its own files."]),
    ("file", "", " ", "release file",
     ["**Release file.** Absent for the build itself; ", el("code", "-debug"), ", ", el("code", "-tests"), ", ",
      el("code", "-sources"), ", ", el("code", ".build-info"), " and the others name the other files."]),
    ("ext", ".", "tar.xz", "format",
     ["**Format.** Archives are xz-compressed tarballs, which macOS's own ", el("code", "tar"),
      " extracts. There are no .zip, .dmg or .pkg versions."]),
]

FILES = [
    (".tar.xz", ["Wine itself, ready to run"], "build"),
    ("-debug.tar.xz", ["Debug symbols (", el("code", ".dSYM"), ", ", el("code", ".pdb"), ") split from this exact build"], "debug"),
    ("-tests.tar.xz", [el("code", "winetest.exe"), " for both architectures"], "tests"),
    ("-sources.tar.xz", ["Corresponding source: Wine, its patches, and the add-ons"], "sources"),
    (".build-info.json", ["BUILD-INFO: source, patches, toolchain, flags, components"], "build_info"),
    (".spdx.json", ["Software Bill of Materials (SBOM), in SPDX format"], "sbom"),
    (".SHA256SUMS", ["Checksums of the files above, in the format ", el("code", "shasum -a 256 -c"), " reads"], "sha256sums"),
    (".tests.json", ["What the build's tests showed. Written after the tests run, so not in SHA256SUMS"], "test_results"),
]

TREE = [
    ("bin/", "Wine's programs: wine, wineserver and the other tools.", [
        ("wine", "Starts Windows programs. `wine --version` prints the version string, such as `wine-11.21 (macfixes)`."),
        ("wineserver", "The background process that coordinates a Wine prefix.")]),
    ("lib/", "Bundled libraries, GStreamer and Wine's own modules. Load paths point inside the folder, so it runs from anywhere.", [
        ("GStreamer.framework/", "GStreamer. Wine uses only this bundled copy."),
        ("*.dylib", "The libraries Wine loads, built from source for the build's minimum macOS: FreeType, GnuTLS, SDL2, MoltenVK, the Vulkan loader and others."),
        ("wine/", "Wine's own modules, one folder per architecture: i386-windows, x86_64-windows and x86_64-unix (the macOS side).")]),
    ("share/", "Data files.", [
        ("wine/", "Wine's data, fonts and code page tables, plus Gecko and Mono already extracted."),
        ("vulkan/icd.d/", "MoltenVK's driver manifest. The bundled Vulkan loader is pointed here unless `VK_DRIVER_FILES` is set.")]),
    ("BUILD-INFO.json", "The same file as the release's `.build-info.json`.", []),
    ("LICENSES/", "The licences of everything bundled, one folder per component.", []),
    ("README.md", "What the build is, and a link to its release.", []),
]


def component_versions(d):
    """{component: version} from the newest build that lists it."""
    out = {}
    for b in sorted(d.builds.values(), key=lambda b: (version_key(b["wine_version"]), b["published"] or "")):
        for c in b["primary"]["components"]:
            if c.get("version"):
                out[c["name"]] = c["version"].split("_")[0]
    return out


def anatomy():
    name, desc = [el("span", el("b", "wine"), class_="part plain", aria_hidden="true")], []
    for key, sep, txt, label, text in PARTS:
        if sep:
            name.append(el("span", sep, class_="sep", aria_hidden="true"))
        name.append(el("span", el("b", txt), el("i", label), class_=f"part p-{key}", data_p=key))
        desc.append(el("li", el("span", el("code", txt.strip()) if txt.strip() else "(none)", class_="k"),
                       el("span", *[inline(t) if type(t) is str else t for t in text], class_="v"), data_p=key))
    return el("div",
              el("p", "Example: ", el("code", "wine-macfixes-11.21-build1-macos-x86_64.tar.xz"), class_="muted small"),
              el("div", *name, class_="name-parts", role="img",
                 aria_label="wine-macfixes-11.21-build1-macos-x86_64.tar.xz, in its parts"),
              el("ul", *desc, class_="pdesc"), class_="anat")


def tree():
    items = []
    for entry, note, kids in TREE:
        if kids:
            items.append(el("details", el("summary", el("span", entry, class_="dir"), " ", el("span", inline(note), class_="c")),
                            *[el("span", k, " ", el("span", inline(n), class_="c"), class_="leaf") for k, n in kids],
                            open=entry != "bin/"))
        else:
            items.append(el("span", entry, " ", el("span", inline(note), class_="c"), class_="leaf"))
    return el("div", el("details", el("summary", el("span", "wine-macfixes-11.21-build1-macos-x86_64/", class_="dir")),
                        *items, open=True), class_="tree")


def render(site, d):
    c = content.load().get("packaging", {})
    v = component_versions(d)
    mins = sorted({b["primary"]["minimum_macos"] for b in d.builds.values() if b["primary"]["minimum_macos"]},
                  key=lambda x: [int(n) for n in x.split(".")])
    minimum = mins[0] if mins else "10.15"
    hero = el("div", el("div", el("h1", "Packaging"), paragraphs(c.get("body"), class_="lede"), class_="hero"),
              class_="wrap")

    def ver(name, what):
        """'MoltenVK 1.4.0', or just 'MoltenVK' when no build names a version."""
        return f"{what} {v[name]}" if v.get(name) else what

    contents = [
        ("Wine, both architectures", ["i386 and x86_64 Windows sides (", el("code", "--enable-archs"),
                                      "), so 32-bit Windows programs run without 32-bit macOS libraries."]),
        ("Gecko and Mono", [f"{ver('wine-gecko', 'Wine Gecko')} and Wine Mono (the version each Wine release asks "
                            "for" + (f"; {v['wine-mono']} in the newest build" if v.get("wine-mono") else "")
                            + "), already extracted into ", el("code", "share/wine/"), "."]),
        ("Vulkan", [f"{ver('MoltenVK', 'MoltenVK')} and {ver('vulkan-loader', 'the Vulkan loader')}, with the "
                    "loader pointed at the bundled driver."]),
        ("GStreamer", [f"{ver('GStreamer.framework', 'GStreamer')}, for media playback."]),
        ("Libraries", [f"About 20 libraries built from source with MacPorts for macOS {minimum}: FreeType, "
                       "GnuTLS, SDL2, libpng, zstd and others. Only libraries Wine actually loads."]),
        ("Platform", [f"macOS x86_64, minimum {minimum}. Runs on Apple silicon under Rosetta 2. Ad hoc signed; "
                      "not notarised."]),
    ]
    files_rows = [el("tr", el("td", el("code", sfx)), el("td", *desc)) for sfx, desc, _ in FILES]
    body = [
        hero,
        wrap(head("Build names", "Every file name carries the build's full identity, so a downloaded file still "
                                 "says exactly what it is."), anatomy(), cls="block tight"),
        wrap(head("Package outline", "One top-level folder with the same name as the archive, holding Wine's own "
                                     "install layout."), tree()),
        wrap(head("Contents", "The same in every flavour."),
             el("div", *[el("div", el("h3", t), el("p", *x)) for t, x in contents], class_="contents")),
        wrap(head("Release files", "Each build's GitHub release has these files, all named after the build ID."),
             el("div", el("table", el("thead", el("tr", el("th", "Suffix"), el("th", "What it is"))),
                          el("tbody", *files_rows), class_="files"), class_="tw")),
        wrap(head("Channels and build numbers"),
             el("div",
                el("div", el("h3", "Stable"), el("p", el("code", "X.0"), " and ", el("code", "X.0.Y"),
                                                  ", such as 10.0 or 11.0.3.")),
                el("div", el("h3", "Development"), el("p", el("code", "X.Y"), ", such as 11.21.")),
                el("div", el("h3", "Release candidate"), el("p", el("code", "X.0-rcN"), ", such as 11.0-rc2.")),
                el("div", el("h3", "Build number"), el("p", "Starts at 1 and goes up for every new build of the same "
                                                            "flavour and version. A new build never replaces a file.")),
                class_="chan")),
    ]
    site.page("/packaging/", title="Packaging", nav="packaging", body=body,
              description="How the Wine builds for macOS are named and laid out, what they bundle, and which files "
                          "each release has.")
