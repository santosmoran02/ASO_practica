savedcmd_helloWorldModule.mod := printf '%s\n'   helloWorldModule.o | awk '!x[$$0]++ { print("./"$$0) }' > helloWorldModule.mod
