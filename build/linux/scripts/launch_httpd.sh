DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

OUT_DIR=$DIR/../out
SRC_DIR=$DIR/../../../
set -e

MODE=$1
PREFIX=$OUT_DIR/install/armadito-av

if [[ $MODE == "gdb" ]];
then
        G_MESSAGES_DEBUG=all LD_LIBRARY_PATH=$PREFIX/lib PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig libtool --mode=execute gdb $PREFIX/sbin/armadito-httpd
fi

if [[ $MODE == "valgrind" ]];
then
        G_MESSAGES_DEBUG=all LD_LIBRARY_PATH=$PREFIX/lib PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig valgrind --leak-check=full --show-leak-kinds=definite --log-file="a6o_leaks.log" $PREFIX/sbin/armadito-httpd
fi

G_MESSAGES_DEBUG=all LD_LIBRARY_PATH=$PREFIX/lib PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig $PREFIX/sbin/armadito-httpd
