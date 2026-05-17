---
name: Bug report
about: Something in lemon isn't working correctly
title: ""
labels: "A: bug"
assignees: ""
---

## Info

<!--Paste lemon version from running "lemon -v"-->
<!--
Wlroots library needs to be built from this repository to avoid crashes
https://github.com/DreamMaoMao/wlroots.git
-->

lemon version:
wlroots version:

## Crash track
1.you need to build lemon by enable asan flag.
```bash
meson build -Dprefix=/usr -Dasan=true
``
2.run lemon in tty.
```bash
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:log_path=/home/xxx/asan.log"
lemon

```

3.after lemon crash,paste the log file `/home/xxx/asan.log` here.

## Description

<!--
Only report bugs that can be reproduced on the main line
-->
