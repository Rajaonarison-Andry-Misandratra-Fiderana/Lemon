---
title: Installation
description: Install lemonwm on AerynOS, Arch, Fedora, Gentoo, PikaOS, or build from source.
---

## Package Installation

lemonwm is available as a pre-built package on several distributions. Choose your distribution below.

---

### AerynOS

lemonwm is available in the **AerynOS package repository**.

You can install it using the `moss` package manager:

```bash
sudo moss install lemonwm
```

---

### Arch Linux

lemonwm is available in the **Arch User Repository (AUR)**.

You can install it using an AUR helper like `yay` or `paru`:

```bash
yay -S lemonwm-git
```

> **Tip:** This package pulls the latest git version, ensuring you have the newest features and fixes.

---

### Fedora

The package is in the third-party **Terra repository**. First, add the Terra Repository.

> **Warning:** Both commands require root privileges. Use `sudo` if needed.

```bash
dnf install --nogpgcheck --repofrompath 'terra,https://repos.fyralabs.com/terra$releasever' terra-release
```

Then, install the package:

```bash
dnf install lemonwm
```

---

### Gentoo

The package is hosted in the community-maintained **GURU** repository.

1. **Add the GURU repository**

   ```bash
   emerge --ask --verbose eselect-repository
   eselect repository enable guru
   emerge --sync guru
   ```

2. **Unmask packages**
   Add the required packages to your `package.accept_keywords` file:
   - `gui-libs/scenefx`
   - `gui-wm/lemonwm`

3. **Install lemon**
   ```bash
   emerge --ask --verbose gui-wm/lemonwm
   ```

---

### PikaOS

lemonwm is available in the **PikaOS package repository**.

You can install it using the `pikman` package manager:

```bash
pikman install lemonwm
```

---

## Building from Source

If your distribution isn't listed above, or you want the latest unreleased changes, you can build lemonwm from source.

> **Info:** Ensure the following dependencies are installed before proceeding:
>
> - `wayland`
> - `wayland-protocols`
> - `libinput`
> - `libdrm`
> - `libxkbcommon`
> - `pixman`
> - `libdisplay-info`
> - `libliftoff`
> - `hwdata`
> - `seatd`
> - `pcre2`
> - `xorg-xwayland`
> - `libxcb`

You will need to build `wlroots` and `scenefx` manually as well.

1. **Build wlroots**
   Clone and install the specific version required (check README for latest version).

   ```bash
   git clone -b 0.19.3 https://gitlab.freedesktop.org/wlroots/wlroots.git
   cd wlroots
   meson build -Dprefix=/usr
   sudo ninja -C build install
   ```

2. **Build scenefx**
   This library handles the visual effects.

   ```bash
   git clone -b 0.4.1 https://github.com/wlrfx/scenefx.git
   cd scenefx
   meson build -Dprefix=/usr
   sudo ninja -C build install
   ```

3. **Build lemonwm**
   Finally, compile the compositor itself.
   ```bash
   git clone https://github.com/lemonwm/lemon.git
   cd lemon
   meson build -Dprefix=/usr
   sudo ninja -C build install
   ```
