# Monolinux C library root folder.
export ML_ROOT=$(readlink -f .)

export PATH=$PATH:$ML_ROOT/bin
