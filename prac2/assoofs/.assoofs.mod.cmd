savedcmd_assoofs.mod := printf '%s\n'   assoofs.o | awk '!x[$$0]++ { print("./"$$0) }' > assoofs.mod
