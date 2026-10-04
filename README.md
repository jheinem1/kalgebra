# KAlgebra desktop fixes

This fork of [KDE KAlgebra](https://invent.kde.org/education/kalgebra) fixes shared functions, dark-theme contrast, and keyboard completion. It includes a pinned [Analitza fork](https://github.com/jheinem1/analitza) because the expression editor and syntax highlighter live in that library.

## Behavior

- Calculator definitions and constants are available in the 2D and 3D graph inputs, their completion lists, and 2D interval inputs.
- A graph is published as a callable function under its displayed name (`f0`, `f1`, etc.). Names are unique across the calculator and both graph tabs. Renaming a graph publishes its new name; previous definitions remain available to existing expressions.
- Changes to calculator functions or constants rebuild dependent plots. Editing a plotted function updates its shared definition.
- Removing a plot removes the view, retaining its definition in the calculator so other expressions can continue to use it.
- The calculator log follows the active palette, including theme changes. Input syntax colors meet a 4.5:1 contrast ratio against the theme's base color; validation backgrounds are tinted from that base.
- Enter, Return, and Tab accept the highlighted completion instead of the first match. Enter with no active completion still submits the expression, and Up/Down still navigate calculator history.
- The empty calculator uses the active theme background from startup, before any equations are entered.
- Calculator → Result Format offers Fractions and Decimals. Switching reformats existing results and is remembered on the next launch. Rational arithmetic, including `ans` and named functions, keeps reduced fractions; irrational results use decimals. Fractions which exceed the supported integer range fall back to decimals.

For an explicit plot such as `x->x**2`, `f0(3)` returns `9`. Inferred arguments are sorted by name; explicitly written argument lists retain their order. Implicit 2D equations that are linear in `y` also support a one-argument call which solves for `y`: for `x=13*y`, `f0(12)` returns `12/13` in Fractions mode, or its decimal value in Decimals mode. This includes equations such as `y=x^2` and `x*y=1`. The original residual call remains available: `f0(13,1)` returns `0`. Nonlinear relations in `y`, such as a circle with two branches, require both arguments.

## Native build

Install a C++ compiler, CMake, Ninja, Qt 6 development packages (including Widgets, QML/Quick, XML, SVG, PrintSupport, Test, OpenGLWidgets, and WebEngineWidgets), KDE Frameworks 6 development packages (ECM, I18n, CoreAddons, ConfigWidgets, WidgetsAddons, KIO, DocTools, and XmlGui), and optionally Eigen3 and readline/ncurses.

```sh
git clone --recurse-submodules https://github.com/jheinem1/kalgebra.git
cd kalgebra
./scripts/build.sh
./build/install/bin/kalgebra
```

The build script builds and tests the pinned Analitza first, then KAlgebra, and installs into the checkout's `build/install` directory. An optional first argument selects another install prefix. The installed system KAlgebra is not replaced. Desktop builds fail with a clear dependency error if Qt WebEngine is missing; `-DBUILD_DESKTOP=OFF` remains available for mobile-only builds.

## Flatpak build

Use `scripts/build-flatpak.sh` on a Flatpak-enabled Linux host. It uses the KDE 6.11 SDK and Qt WebEngine BaseApp, builds both projects, runs their tests, and creates `build/kalgebra-fixes.flatpak`. Its separate app ID is `org.kde.kalgebra.Devel`.

```sh
flatpak install --user flathub org.kde.Sdk//6.11 org.kde.Platform//6.11 io.qt.qtwebengine.BaseApp//6.11
./scripts/build-flatpak.sh
flatpak install --user ./build/kalgebra-fixes.flatpak
flatpak run org.kde.kalgebra.Devel
```

The development package uses its own application data directory. Building it does not install or restart KAlgebra.

## Validation

Analitza includes regression tests for arrow-key selection of `ans` instead of `and`, all three completion keys, normal submission/history, and light/dark highlighting. KAlgebra tests cover shared 2D/3D functions, graph edits and names, calculator redefinitions, updated curve variables, implicit-equation arity, completion in the real graph editors, rendered console colors, and live palette changes. The UI test uses software rendering; 3D expressions and export are checked by the model and Analitza suites.
