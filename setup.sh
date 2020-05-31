# Monolinux C library root folder.
export ML_ROOT=$(readlink -f .)

export PATH=$PATH:$ML_ROOT/bin
export ML_INC=../3pp/include
