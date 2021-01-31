/*
Simulador parametro de predictor de performance de cache

MIT License

Manuel Ferreria 2012

*/

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "pin.H"

//#define DEBUG 1

#define CACHE_LEVELS 2
#define INSTRUCTION_CACHE_LEVELS 1

using namespace std;

typedef struct {
  UINT32 tag;
  UINT32 p;
  UINT32 dirty;
  UINT32 age;
} _cache;

// Memoria cache, todos los niveles
_cache** cache;

UINT64* cacheWrite;
UINT64* cacheRead;
UINT64* cacheHit;
UINT64* cacheMiss;
UINT64* cacheWriteBacks;

UINT32* size;
UINT32* bitssize;

UINT32 sizeVia;

UINT32 sizeLine;
UINT32 bitssizeLine;

UINT32 lru;

string executableParameter;

KNOB<string> KnobsizeL1(
    KNOB_MODE_WRITEONCE, "pintool", "L1", "16",
    "Especifica la cantidad de bits que tiene una direccion en la L1 (tamaño)");

KNOB<string> KnobsizeL2(
    KNOB_MODE_WRITEONCE, "pintool", "L2", "20",
    "Especifica la cantidad de bits que tiene una direccion en la L2 (tamaño)");

KNOB<string> KnobVias(KNOB_MODE_WRITEONCE, "pintool", "V", "4",
                      "Especifica la cantidad de vias que hay en la cache");

KNOB<string> KnobLine(
    KNOB_MODE_WRITEONCE, "pintool", "L", "4",
    "Especifica la cantidad de bits que tiene una direccion de memoria que se "
    "usan para indexar dentro de la linea (tamaño)");

KNOB<string> KnobOutputFile(
    KNOB_MODE_WRITEONCE, "pintool", "o", "cache.out",
    "Especifica el nombre del archivo de salida del informe");

KNOB<string> KnobLRU(KNOB_MODE_WRITEONCE, "pintool", "lru", "0",
                     "Utiliza desalojo LRU de la linea en vez de FIFO");

// Liberamos el primer elemento de la linea y devuelve el indice liberado
UINT32 removeFIFO(_cache* thisLevelLine, UINT32 sizeVia) {
  UINT32 i = 0;
  for (i = 0; i < sizeVia - 1; i++) {
    thisLevelLine[i].tag = thisLevelLine[i + 1].tag;
    thisLevelLine[i].p = thisLevelLine[i + 1].p;
    thisLevelLine[i].dirty = thisLevelLine[i + 1].dirty;
    thisLevelLine[i].age = thisLevelLine[i + 1].age;
  }
  return sizeVia - 1;
}

// Liberamos el elemento mas viejo de la linea y devuelve el indice liberado
UINT32 removeLRU(_cache* thisLevelLine, UINT32 sizeVia) {
  UINT32 i = 0;
  UINT32 older_index, older_age;

  older_age = thisLevelLine[0].age;
  older_index = 0;

  for (i = 0; i < sizeVia - 1; i++) {
    if (older_age < thisLevelLine[i].age) {
      older_age = thisLevelLine[i].age;
      older_index = i;
    }
  }
  return older_index;
}

void accessCache(UINT32 writeAccess, UINT64 address, UINT32 cacheLevel) {
  if (cacheLevel >= CACHE_LEVELS) return;  // no tenemos tantos niveles de cache

  // Obtenemos el numero de la linea en la que va a estar
  UINT32 linea = (address >> bitssizeLine) % size[cacheLevel];
  // Obtenemos el tag de la direccion
  UINT32 tag = address >> (bitssizeLine + bitssize[cacheLevel]);

  // obtenemos la linea de la cache actual
  _cache* L = &(cache[cacheLevel][linea * sizeVia]);

  UINT32 i = 0;
  UINT32 tagEncontrado = 0;
  UINT32 Read = 0;
  UINT32 Write = 1;
  UINT32 indice_encontrado = 0;
  UINT32 nuevo_indice = 0;

  // Contabilizamos si es un read o un write en ese nivel
  if (writeAccess)
    cacheRead[cacheLevel]++;
  else
    cacheWrite[cacheLevel]++;

  // Buscamos el tag en toda la linea
  for (i = 0; i < sizeVia; i++) {
    if (L[i].p == 1) {
      if (L[i].tag == tag) {
        tagEncontrado = 1;
        indice_encontrado = i;
        L[i].age = 0;
      } else  // los envejecemos a todos los demas que estan
      {
        L[i].age++;
      }
    }
  }
  // Encontramos ese tag, vamos a marcar el hit y anotamos el dirty si es
  // necesario
  if (tagEncontrado) {
    cacheHit[cacheLevel]++;
    if (writeAccess) L[indice_encontrado].dirty = true;

  }
  // hubo miss en este nivel, ahora vemos si podemos traer de un nivel superior
  // o de mas arriba
  else {
    // Es miss en este nivel
    cacheMiss[cacheLevel]++;

    // Vamos a hacer una lectura en los caches superiores a ver si lo
    // encontramos

    accessCache(Read, address, cacheLevel + 1);

    // buscamos si hay un espacio libre
    for (i = 0; i < sizeVia; i++) {
      if (L[i].p == 0) break;
    }
    if (i != sizeVia)  // encontramos un lugar vacio; contamos que vamos a
                       // guardar el dato ahi
    {
      cacheWrite[cacheLevel]++;
      nuevo_indice = i;
    } else  // todo lleno, vamos a desalojar
    {
      if (lru == 1)  // Quitamos el elemento usado hace mas tiempo
        nuevo_indice = removeLRU(L, sizeVia);
      else  // Quitamos de esta via el primer elemento
        nuevo_indice = removeFIFO(L, sizeVia);

      if (L[nuevo_indice].dirty)  // si el desalojamos, habia sido escrito, hay
                                  // que propagarlo
      {
        cacheWriteBacks[cacheLevel]++;
        // Y propagamos la escritura a niveles superiores
        accessCache(Write, address, cacheLevel + 1);
      }
    }
    // Guardamos en nuestra linea este acceso
    L[nuevo_indice].p = 1;
    L[nuevo_indice].tag = tag;
    L[nuevo_indice].dirty = writeAccess;
    L[nuevo_indice].age = 0;
  }
}

// Registramos que accedimos a addr para leer
void memAccessR(VOID* addr) {
  UINT32 writeAccess = 0;        // Es un read
  UINT32 initialCacheLevel = 0;  // 0 -> L1, 1->L2 ...
  UINT64 dir = (UINT64)addr;     // Pasamos la direccion como entero
  accessCache(writeAccess, dir, initialCacheLevel);
}

// Registramos que accedimos a addr para escribir
void memAccessW(VOID* addr) {
  UINT32 writeAccess = 1;        // Es un write
  UINT32 initialCacheLevel = 0;  // 0 -> L1, 1->L2 ...
  UINT64 dir = (UINT64)addr;     // Pasamos la direccion como entero
  accessCache(writeAccess, dir, initialCacheLevel);
}

void initBuffs() {
  // Los contadores de cada caso para cada nivel de cache
  cacheWrite = (UINT64*)calloc(CACHE_LEVELS, sizeof(UINT64));
  cacheRead = (UINT64*)calloc(CACHE_LEVELS, sizeof(UINT64));
  cacheHit = (UINT64*)calloc(CACHE_LEVELS, sizeof(UINT64));
  cacheMiss = (UINT64*)calloc(CACHE_LEVELS, sizeof(UINT64));
  cacheWriteBacks = (UINT64*)calloc(CACHE_LEVELS, sizeof(UINT64));

  size = (UINT32*)calloc(CACHE_LEVELS, sizeof(UINT32));
  bitssize = (UINT32*)calloc(CACHE_LEVELS, sizeof(UINT32));

  // El arreglo de niveles de cache de memoria
  cache = (_cache**)calloc(CACHE_LEVELS, sizeof(_cache*));

  bitssizeLine = atoi(KnobLine.Value().c_str());
  sizeLine = 1 << bitssizeLine;
  sizeVia = atoi(KnobVias.Value().c_str());

  // El tamaño de la Lx incluye los bits de la linea
  bitssize[0] = atoi(KnobsizeL1.Value().c_str()) - bitssizeLine;
  bitssize[1] = atoi(KnobsizeL2.Value().c_str()) - bitssizeLine;

  UINT32 i;
  for (i = 0; i < CACHE_LEVELS; i++) {
    size[i] = 1 << bitssize[i];
    // cache por nivel
    cache[i] = (_cache*)calloc(size[i] * sizeVia, sizeof(_cache));
  }
  // Si hay un 1
  lru = 0;
  if (atoi(KnobLRU.Value().c_str()) == 1) {
    lru = 1;
  }

  return;
}
// Liberamos toda la memoria pedida
void freeBuffers() {
  free(size);
  free(bitssize);

  free(cacheWrite);
  free(cacheRead);
  free(cacheHit);
  free(cacheMiss);
  free(cacheWriteBacks);

  UINT32 i = 0;
  for (i = 0; i < CACHE_LEVELS; i++) free(cache[i]);
  free(cache);
}

// Instruction se llama en cada instruccion y es la encargada de contabilizar
// lo que haga falta, en este caso los saltos
VOID Instruction(INS ins, VOID* v) {
  // Instruments memory accesses using a predicated call, i.e.
  // the instrumentation is called iff the instruction will actually be
  // executed.
  UINT32 memOperands = INS_MemoryOperandCount(ins);

  // Por cada operando de memoria, vemos si es lectura o escritura y llamamos a
  // la funcion correspondiente para contabilizar
  for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
    if (INS_MemoryOperandIsRead(ins, memOp)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)memAccessR,
                               IARG_MEMORYOP_EA, memOp, IARG_END);
    }
    if (INS_MemoryOperandIsWritten(ins, memOp)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)memAccessW,
                               IARG_MEMORYOP_EA, memOp, IARG_END);
    }
  }
}

// Fini se llama cuando se sale del programa y termina el sim, e imprime el
// informe
VOID Fini(INT32 code, VOID* v) {
  UINT32 i;                           // iterador de lineas
  UINT32 level;                       // iterador de niveles de cache
  UINT32 lineasUsadas[CACHE_LEVELS];  // contador de lineas usadas en cada nivel
                                      // de cache
  UINT32 celdasUsadas[CACHE_LEVELS];  // contador de vias usadas en cada nivel
                                      // de cache

  // Escribimos a un archivo porque el programa puede modificar cerr y cout
  // normalmente
  ofstream OutFile;
  OutFile.open(KnobOutputFile.Value().c_str());
  OutFile.setf(ios::showbase);
  char niveles[10];
  sprintf(niveles, "%d", CACHE_LEVELS);
  string title = "Analisis de cache writeback para  \"" + executableParameter +
                 "\" usando " + string(niveles) +
                 " niveles de cache usando reemplazo ";
  if (lru == 1)
    title += "LRU";
  else
    title += "FIFO";
  OutFile << endl << title << endl << string(title.length(), '=') << endl;

  for (level = 0; level < CACHE_LEVELS; level++) {
    OutFile << "Tamaño de L" << level + 1 << ": "
            << size[level] * sizeVia / 1024 << "kb ";
    OutFile << "Cantidad de lineas de L" << level + 1 << ": " << size[level]
            << endl;
  }
  OutFile << endl;
  OutFile << "Tamaño de la linea: " << sizeLine << endl;
  OutFile << "Cantidad de Vias:  " << sizeVia << endl << endl;
  for (level = 0; level < CACHE_LEVELS; level++) {
    OutFile << "Hitrate de L" << level + 1 << ": "
            << ((double)cacheHit[level] /
                (double)(cacheHit[level] + cacheMiss[level])) *
                   100
            << "%" << endl;
    OutFile << "Cantidad de Hits en L" << level + 1 << ": " << cacheHit[level]
            << endl;
    OutFile << "Cantidad de Miss L" << level + 1 << ": " << cacheMiss[level]
            << endl;
    OutFile << "Cantidad de Read L" << level + 1 << ": " << cacheRead[level]
            << endl;
    OutFile << "Cantidad de Write L" << level + 1 << ": " << cacheWrite[level]
            << endl;
    OutFile << "Cantidad de WriteBacks al siguiente nivel de L" << level + 1
            << ": " << cacheWriteBacks[level] << endl;
    OutFile << endl;
  }
  OutFile << endl;
  for (level = 0; level < CACHE_LEVELS; level++) {
    lineasUsadas[level] = 0;

    for (i = 0; i < size[level]; i++) {
      lineasUsadas[level] += cache[level][i * sizeVia].p;
    }
    OutFile << "Lineas de L" << level + 1 << " usadas: " << lineasUsadas[level]
            << " de " << size[level] << endl;
  }
  OutFile << endl;

  for (level = 0; level < CACHE_LEVELS; level++) {
    celdasUsadas[level] = 0;
    for (i = 0; i < size[level] * sizeVia; i++) {
      celdasUsadas[level] += cache[level][i].p;
    }
    OutFile << "Lineas Internas (Lineas * Vias) de L" << level + 1
            << " usadas: " << celdasUsadas[level] << " de "
            << size[level] * sizeVia << endl;
  }
  OutFile.close();
  freeBuffers();
}

INT32 Usage() {
  cerr << endl
       << "Esta herramienta simula una cache de CPU parametrizable al correr "
          "un programa"
       << endl;
  cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}

// Buscamos el delimitador del parametro nombre de archivo, para poner en el
// informe
VOID getProgramNameToProfile(int argc, char* argv[]) {
  int executableArgumentIndex = -1;
  for (int i = argc - 1; i > 0; --i) {
    if (string("--") == string(argv[i])) {
      executableArgumentIndex = i + 1;
      break;
    }
  }
  executableParameter = string(argv[executableArgumentIndex]);
  for (int i = executableArgumentIndex + 1; i < argc; i++)
    executableParameter += (" " + string(argv[i]));
  return;
}

int main(int argc, char* argv[]) {
  if (PIN_Init(argc, argv)) return Usage();

  // cargamos de los parametros el path del ejecutable para ponerlo en el
  // informe
  getProgramNameToProfile(argc, argv);

  initBuffs();

  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Never returns
  PIN_StartProgram();

  return 0;
}
