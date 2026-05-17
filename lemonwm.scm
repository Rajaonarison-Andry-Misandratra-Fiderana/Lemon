(define-module (lemonwm)
  #:use-module (guix download)
  #:use-module (guix git-download)
  #:use-module (guix gexp)
  #:use-module (guix packages)
  #:use-module (guix utils)
  #:use-module (gnu packages wm)
  #:use-module (gnu packages freedesktop)
  #:use-module (gnu packages xdisorg)
  #:use-module (gnu packages pciutils)
  #:use-module (gnu packages admin)
  #:use-module (gnu packages pcre)
  #:use-module (gnu packages xorg)
  #:use-module (gnu packages build-tools)
  #:use-module (gnu packages ninja)
  #:use-module (gnu packages pkg-config)
  #:use-module (guix build-system meson)
  #:use-module ((guix licenses) #:prefix license:))


(define-public lemonwm-git
  (package
    (name "lemonwm")
    (version "git")
    (source (local-file "." "lemonwm-checkout"
                        #:recursive? #t
                        #:select? (or (git-predicate (current-source-directory))
                                      (const #t))))
    (build-system meson-build-system)
    (arguments
     (list
      #:configure-flags
      #~(list (string-append "-Dsysconfdir=" #$output "/etc"))
      #:phases
      #~(modify-phases %standard-phases
          (add-before 'configure 'patch-meson
            (lambda _
              (substitute* "meson.build"
                ;; LemonWM ignores sysconfdir handling for NixOS.
                ;; We also need to skip that sysconfdir edits.
                (("is_nixos = false")
                 "is_nixos = true")
                ;; Unhardcode path.  Fixes loading default config.
                (("'-DSYSCONFDIR=\\\"@0@\\\"'.format\\('/etc'\\)")
                 "'-DSYSCONFDIR=\"@0@\"'.format(sysconfdir)")))))))
    (inputs (list wayland
                  libinput
                  libdrm
                  libxkbcommon
                  pixman
                  libdisplay-info
                  libliftoff
                  hwdata
                  seatd
                  pcre2
                  libxcb
                  xcb-util-wm
                  wlroots-0.19
                  scenefx))
    (native-inputs (list pkg-config wayland-protocols))
    (home-page "https://github.com/lemonwm/lemon")
    (synopsis "Wayland compositor based on wlroots and scenefx")
    (description
     "LemonWM is a modern, lightweight, high-performance Wayland compositor
built on dwl — crafted for speed, flexibility, and a customizable desktop experience.")
    (license (list license:gpl3 ;lemonwm itself, dwl
                   license:expat ;dwm, sway, wlroots
                   license:cc0)))) ;tinywl

(define-deprecated-package lemonwc
  lemonwm-git)

lemonwm-git
