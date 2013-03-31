CacheTracer
===========

Intel PIN Simulator extensions to simulate different many level caches and different jump predictors. It is
mainly a project to study different performance improvements employed in the microarchitecture of CPU's.

Goal
======
Implement by software some of the mechanisms in use in CPU microarchitectures of modern processors,
so we can evaluate the performance of these tools synthetically, without having to build them.

The project was focused on two of the most pressing issues considering microarchitectural performance,
the cache hit rate and the jump predictor.

Both simulators were built around Intel tools, using the PIN library (http://www.pintool.org/)

Multilevel cache simulator
================

a) To run:

	./cachesim.sh <parameters> -- <executable>

i.e to run "ls -la" using a 6-way LRU Associative cache:

	./cachesim.sh -V 6 -lru 1 -- ls -la

and results can be seen in "cache.out"


Possible parameters are:

-L   [default 4]
	Specify the amount of bits a memory address has to be used to index inside the line
	(line size)

-L1  [default 16]
	Specify the amount of bits an L1 address has. (cache size)

-L2  [default 20]
	Specify the amount of bits an L1 address has. (cache size)

-V   [default 4]
	Specify the amount of ways a cache line has.

-lru [default 0]
	Use LRU instead of FIFO as a line replacement policy.

-o   [default cache.out]
	Specify the report output filename.


b) Details

The cache simulator code can be found in:
	simulators/cache.cpp

The simulator is designed to keep an equivalent of a writeback cache memory, in RAM.

That is, it has the necessary data structures to be a cache (present bit, dirty bit and assigned tag to that line)

There is an only function to access the cache, "accessCache", and its parameters indicate if it is a write or a read,
and what in what cache level it has to look up that data.

This function takes care to address theline in the accessed cache level and sees if it matches the tag in every line.
If it doesn't, it tries to read in an upper level to see if it finds it. If there are no more levels, it counts a miss and
makes as if it has to look for that data in RAM. Then, if needed, it releases a way in that line and kees the data in the lower cache.
This allows the next access to be in the L1, improving efficiency.

If the data is found at some level, it simply records that hit.

c) Notes on the cache simulator

The simulator is designed to be completely extensible to the necessary cache levels, from one to five or more, if needed.
The only changed need would be to add the extra parameters to be able to customize them.hacer facilmente.


Jump Predictor Simulator
===================

a) To run:
	./jmpsim.sh <parameters> -- <executable>


i.e, to run "ls -la" with a BHT of 2^4 entries:

	./jmpsim.sh -s 4 -- ls -la


Report results are, by default, in "jmp.out"


Possible parameters are:
-s   [default 12]
	Specifies the amount of bits in the BHT table (size, amount of bits of a memory address
	used to index a state machine table.)
-o   [default jmp.out]
	Specifies the report filename.

b) Details
The jump predictor simulator code is at:
	simulators/jmp.cpp

The simulators was conceived to try to understand basic jump prediction mechanisms,
to avoid pipeline stalls when conditional branches arise.

The internal mechanism is simple. A binding to the program passed by arguments
is created using PIN, so the simulator counts every time a conditional instruction is executed.

Several prediction mechanisms were implemented, some of the obvious and stateless,
"Always Jump" and "Never Jumps" as proof of how jumps in a program are located.
Some of the more interesting ones such as "Jump if and only if going to a lower address" proved
to be much more effective than naive ones, reasonable performance for adding little additional logic.

Two more advanced ones, '2 bit sat counter' and '2 bit hystheresis' were also implemented. They
work by state machines indexed by some bits of the instruction point.

Some of these methods were discussed in the  Henessy-Patterson book, "Computer Architecture".


Interesting programs to try
================

"ls -la"
"traceroute google.com"
"bash" (and use it normally, quit by logout)
"firefox"
"locate <file>" (even if it doesn't exist, has to use sudo for it)
"ssh <user>@<maquina>"


Notes
=====

The folder "simulators" has to be placed in an accesible place to the PIN compiler files.

The scripts "./cachesim.sh" and "./jmpsim.sh" run the PIN simulator with the corresponding cache and jump predictors.

Example output can be found in "cache.out" and "jmp.out". It is in spanish so far, but it can be translanted easily.



-----------------------------
(In Spanish)

Meta
======

Implementar por software algunos mecanismos que implementan las microarquitecturas de
los procesadores modernos, para poder evaluar rendimiento de estos mecanismos de forma
sintetica, sin tener que construirlos realmente.

El proyecto se concentro en dos de los temas mas criticos a la hora de performance en la
microarquitectura, el hit rate de la cache y el predictor de saltos.

Ambos simuladores fueron construidos alrededor de las herramientas
que provee Intel, a traves de la libreria PIN. (http://www.pintool.org/)


Simulador de Cache multinivel
================

a) Para ejecutar:

	./cachesim.sh <parametros> -- <ejecutable>

Por ejemplo, para correr un "ls -la" con 6 vias de asociatividad usando LRU

	./cachesim.sh -V 6 -lru 1 -- ls -la

Y los resultados se pueden ver en "cache.out"



Los parametros posibles son:
-L   [default 4]
	Especifica la cantidad de bits que tiene una direccion de memoria que se
	usan para indexar dentro de la linea (tamaño de la linea)
-L1  [default 16]
	Especifica la cantidad de bits que tiene una direccion en la L1	(tamaño de la cache)
-L2  [default 20]
	Especifica la cantidad de bits que tiene una direccion en la L2	(tamaño de la cache)
-V   [default 4]
	Especifica la cantidad de vias que hay en una linea de cache
-lru [default 0]
	Utiliza desalojo LRU de la linea en vez de FIFO
-o   [default cache.out]
	Especifica el nombre del archivo de salida del informe




b) Detalles

El codigo del simulador de cache se encuentra en:
	simuladores/cache.cpp

El simulador esta pensado para mantener un equivalente de la memoria cache
writeback, pero en memoria RAM.

Es decir, cuenta con las estructuras de datos necesarias para ser una cache
(el bit de presente, el bit de dirty y el tag asignado a esa linea).

Existe una funcion unica para acceder a la cache, "accessCache", y sus
parametros indican si es lectura o escritura, y en que nivel de la cache tiene
que buscar el dato en la direccion de memoria.

Esta funcion se encarga de direccionar la linea en el nivel de cache
accedido y ve si matchea el tag en todas las vias.
Si no lo hace, intenta hacer un read en un nivel superior, a ver si lo encuentra.
Si se pasa el maximo nivel de cache, entonces hace el equivalente de buscar en la RAM.
Luego, si necesita, libera una via de esa linea, y guarda el dato en la cache
de mas abajo. Esto permite que el proximo acceso sea a la L1, maximizando la eficiencia.

Si encuentra el dato en un nivel, simplemente contabiliza ese hit, para computarlo
en el hitrate final.


c) Notas sobre el simulador de cache

El simulador esta armado para ser totalmente extensible a la cantidad de niveles de memoria necesarios, de uno a cinco, o mas incluso.

El unico cambio necesario seria agregar los parametros para customizarlo, que se podria hacer facilmente.




Predictor de saltos
===================

a) Para ejecutar:
	./jmpsim.sh <parametros> -- <ejecutable>


Por ejemplo, para correr un "ls -la" con una bht de 2^4 entradas

	./jmpsim.sh -s 4 -- ls -la


Y los resultados se pueden ver, por default, en "jmp.out"


Los parametros posibles son:
-s   [default 12]
	Especifica la cantidad de bits de la tabla BHT (tamaño, la cantidad de
	bits de una direccion que se toman para indexar una tabla con maquina de estados)
-o   [default jmp.out]
	Especifica el nombre del archivo de salida del reporte


b) Detalles

El codigo del simulador de prediccion de saltos se encuentra en:
	simualdores/jmp.cpp

El simulador esta pensado para intentar entender mecanismos basicos de prediccion
de salto, para evitar stalls de pipeline cuando existen branches condicionales.

El mecanismo interno es sencillo. Se crea un binding usando PIN al programa que
se ejecuta por parametro, de modo que cada vez que haya una instruccion que sea
un jump

Se implementaron varios mecanismos de prediccion, algunos obvios y sin memoria,
'Siempre Salta' y 'Nunca Salta' como prueba de como ocurren los saltos en un
programa; algunos mas interesantes como 'Salta si y solo si es a una direccion anterior'
que resulta ser un predictor decente para no tener logica alguna; y dos mas
avanzados: '2 bit Saturation counter' y '2 bit Histeresis counter', que funcionan
mediante maquinas de estado indexadas por el instruction pointer.

Algunos de estos metodos fueron tratados en el libro de Henessy-Patterson
"Computer Architecture".




Programas interesantes para probar
================

Las comillas son para mostrarlas aca, en los simuladores van sin comillas

"ls -la"
"traceroute google.com"
"bash" (y usarla normalmente. Para terminar, desloguearse)
"firefox"
"locate archivo" (incluso si no existe, hay que usar sudo para el script)
"ssh <user>@<maquina>"

Notas
=====

La carpeta "simulators" tiene que ser puesta en un lugar accesible a PIN.
Los scripts "./cachesim.sh" y "./jmpsim.sh" corren el simulador PIN con los parametros pasados como argumentos.

Salidas de ejemplo se pueden encontrar en "cache.out" y "jmp.out".
