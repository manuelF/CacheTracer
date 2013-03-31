#! /bin/bash

#Deteccion de x64 o x86 para saber que pin correr
thisarch=$(uname -m)
tgt="x86_64"
if [ $tgt = $thisarch ]
then
	#echo "x64"

	exec ./pin.sh -injection child -t source/tools/Tp/obj-intel64/jmp.so "${@}"
else
	#echo "x86"

	exec ./pin.sh -injection child -t source/tools/Tp/obj-ia32/jmp.so "${@}"
fi
