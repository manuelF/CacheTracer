/*
Simulador parametrico de predictor de saltos, utilizando distintos metodos

MIT License

Manuel Ferreria 2012

*/

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include "pin.H"

// Desdefinir para no correr todos los predictores
#define SINGLEBIT 1  // Intenta repetir el ultimo salto
#define ALWAYSJMP 1  // Siempre salta
#define NEVERJMP 1   // Nunca salta
#define LOWERADDR 1  // Siempre salta para atras, nunca adelante
#define TWOBITSAT 1  // Contador de saturacion
#define TWOBITHIST \
  1  // Contador de saturacion usando metodo alternativo de histeresis

using namespace std;

UINT32 bhtsize;       // tamaño de la BHT
UINT64 cantJmps = 0;  // cuantos jmps (cond o no) tiene el programa

string executableParameter;  // ejecutable pasado por parametro

#ifdef ALWAYSJMP
UINT64 misJmpsAlways = 0;
// El predictor si siempre salto en branch
VOID AlwaysJmp(VOID* eip, VOID* tgt, INT32 taken) {
  misJmpsAlways += (1 - taken);  // anoto si no tomo la branch
  return;
}
#endif

#ifdef NEVERJMP

UINT64 misJmpsNever = 0;
// El predictor de nunca salto en branch
VOID NeverJmp(VOID* eip, VOID* tgt, INT32 taken) {
  misJmpsNever += taken;  // anoto si tomo la branch
  return;
}
#endif

#ifdef SINGLEBIT

bool* single_saltar = NULL;

UINT64 misJmpsSingle = 0;
// El predictor que recuerda solo el ultimo cambio
VOID SingleBit(VOID* eip, VOID* tgt, INT32 taken) {
  // hacemos una mascara del estilo EIP AND 0x0001111111 para quedarnos con los
  // 'bhtsize' cantidad de bits de EIP, asi podemos indexar una tabla eliminamos
  // los 2 bits menos signif del eip porque casi no hay instrucciones de un solo
  // byte que valga la pena predecir, asi que mejor usarlos mas arriba
  bool* salto = &single_saltar[(((UINT64)eip) >> 2) & (bhtsize - 1)];

  if (*salto != taken) {
    misJmpsSingle++;
    *salto = !(*salto);
  }

  return;
}
#endif

#ifdef LOWERADDR
UINT64 misJmpsLower = 0;
// El predictor que dice que
// si eip > tgt, va a saltar hacia atras
//=>:  si eip < tgt, no va saltar

VOID LowerAddress(VOID* eip, VOID* tgt, INT32 taken) {
  if (tgt < eip)  // salto solo si target addr < eip (hacia atras)
  {
    misJmpsLower += (1 - taken);  // anoto si no volvi hacia atras
  } else  // es hacia adelante el salto, si lo tome, entonces hay misprediction
  {
    misJmpsLower += taken;  // anoto si salte hacia adelante
  }

  return;
}
#endif

#ifdef TWOBITSAT
UINT64 misJmpsTwoBitSat = 0;
UINT8* twobits_sat_saltar = NULL;
// El predictor que usa una maquina de estados (llamada en la literatura
// "Saturation Counter" o "Bimodal Predictor") Que tiene 4 estados que van desde
// fuertemente convencido que no va a saltar (N) a fuertemente convencido que va
// a saltar (T) pasando por 2 estados intermedios (n, t) pero que siguen
// prediciendo lo mismo. Seria como un equivalente al FIFO Second chance, donde
// por mas que haya un fallo de prediccion, el predictor todavia va a estar
// convencido de que esta en el estado correcto
//
// El esquema de la maquina de estados es   (do not take branch) N <--> n <--> t
// <--> T (take branch)
//
// En particular, este predictor se usaba en los Intel Pentium (pre MMX)
VOID TwoBitsSat(VOID* eip, VOID* tgt, INT32 taken) {
  // hacemos una mascara del estilo EIP AND 0x0001111111 para quedarnos con los
  // 'bhtsize' cantidad de bits de EIP, asi podemos indexar una tabla eliminamos
  // los 2 bits menos signif del eip porque casi no hay instrucciones de un solo
  // byte que valga la pena predecir, asi que mejor usarlos mas arriba
  UINT8* salto = &twobits_sat_saltar[(((UINT64)eip) >> 2) & (bhtsize - 1)];
  switch (*salto) {
    case 0:  // N
      if (taken) {
        (*salto)++;  // N -> n
        misJmpsTwoBitSat++;
      }
      break;
    case 1:  // n
      if (taken) {
        (*salto)++;  // n -> t
        misJmpsTwoBitSat++;
      } else {
        (*salto)--;  // n -> N
      }
      break;
    case 2:  // t
      if (taken) {
        (*salto)++;  // t -> T

      } else {
        (*salto)--;  // t -> n
        misJmpsTwoBitSat++;
      }
      break;
    case 3:  // T
      if (!taken) {
        (*salto)--;  // T -> t
        misJmpsTwoBitSat++;
      }
      break;
  }

  return;
}
#endif

#ifdef TWOBITHIST
UINT64 misJmpsTwoBitHist = 0;
UINT8* twobits_hist_saltar = NULL;
// El predictor que usa una maquina de estados (llamada en la literatura
// "Saturation Counter" o "Bimodal Predictor") Que tiene 4 estados que van desde
// fuertemente convencido que no va a saltar (N) a fuertemente convencido que va
// a saltar (T) pasando por 2 estados intermedios (n, t) pero que siguen
// prediciendo lo mismo. La diferencia con el saturation counter anterior, es que
// si si falla 2 veces, ya pasa a estar fuertemente convencido de que la
// prediccion original estaba mal, en vez de debilmente convencido
//																		|--------------V
// El esquema de la maquina de estados es   (do not take branch) N <--> n t <-->
// T (take branch)
//																 ^--------------|
// En particular, una minima variante de este predictor se usaba en los Intel
// Pentium (pre MMX)

VOID TwoBitsHist(VOID* eip, VOID* tgt, INT32 taken) {
  UINT8* salto =
      &twobits_hist_saltar[(((UINT64)eip) >> 2) &
                           (bhtsize - 1)];  // tomamos de bhtsize del eip
  switch (*salto) {
    case 0:  // N
      if (taken) {
        (*salto)++;  // N -> n
        misJmpsTwoBitHist++;
      }
      break;
    case 1:  // n
      if (taken) {
        (*salto) += 2;  // n -> T
        misJmpsTwoBitHist++;
      } else {
        (*salto)--;  // n -> N
      }
      break;
    case 2:  // t
      if (taken) {
        (*salto)++;  // t -> T

      } else {
        (*salto) -= 2;  // t -> N
        misJmpsTwoBitHist++;
      }
      break;
    case 3:  // T
      if (!taken) {
        (*salto)--;  // T -> t
        misJmpsTwoBitHist++;
      }
      break;
  }

  return;
}
#endif

// RecordBranch anota el salto para cada metodo de prediccion
// y registra si fue bien o mal predicho
VOID RecordBranch(VOID* eip, VOID* tgt, INT32 taken) {
  cantJmps++;
// dispatchea a todas
#ifdef LOWERADDR
  LowerAddress(eip, tgt, taken);
#endif
#ifdef ALWAYSJMP
  AlwaysJmp(eip, tgt, taken);
#endif
#ifdef NEVERJMP
  NeverJmp(eip, tgt, taken);
#endif
#ifdef SINGLEBIT
  SingleBit(eip, tgt, taken);
#endif
#ifdef TWOBITSAT
  TwoBitsSat(eip, tgt, taken);
#endif
#ifdef TWOBITHIST
  TwoBitsHist(eip, tgt, taken);
#endif
}

// Instruction se llama en cada instruccion y es la encargada de contabilizar
// lo que haga falta, en este caso los saltos
VOID Instruction(INS ins, VOID* v) {
  // Instruments memory accesses using a predicated call, i.e.
  // the instrumentation is called iff the instruction will actually be
  // executed.
  //

  if (INS_IsBranch(ins)) {
    INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordBranch,
                             IARG_INST_PTR, IARG_BRANCH_TARGET_ADDR,
                             IARG_BRANCH_TAKEN, IARG_END);
  }

  return;
}

KNOB<string> KnobOutputFile(
    KNOB_MODE_WRITEONCE, "pintool", "o", "jmp.out",
    "Especifica el nombre del archivo de salida del reporte");

KNOB<string> KnobSizeBHT(KNOB_MODE_WRITEONCE, "pintool", "s", "12",
                         "Especifica la cantidad de bits de la tabla BHT");

// Fini se llama cuando se sale del programa y termina el sim
VOID Fini(INT32 code, VOID* v) {
  // Guardamos a un archivo porque cerr y cout pueden ser cerrados por la
  // aplicacion
  ofstream OutFile;
  OutFile.open(KnobOutputFile.Value().c_str());
  OutFile.setf(ios::showbase);
  string title =
      "Analisis de prediccion de saltos para  \"" + executableParameter + "\"";
  OutFile << endl << title << endl << string(title.length(), '=') << endl;

  OutFile << "Tamaño BHT: " << bhtsize << " entradas" << endl;
  OutFile << "Cantidad saltos en total: " << cantJmps << endl << endl;
  OutFile << "Eficiencia: " << endl << "===================" << endl;
  OutFile << "  Metodo     \t  Mispredictions \t\t\tEficiencia" << endl;
#ifdef LOWERADDR
  OutFile << "-- LowerAddress \t" << misJmpsLower << " \t\t\t"
          << 100.0 - ((float)100 * misJmpsLower / cantJmps) << "%" << endl;
#endif
#ifdef ALWAYSJMP
  OutFile << "-- AlwaysJmp    \t" << misJmpsAlways << " \t\t\t"
          << 100.0 - ((float)100 * misJmpsAlways / cantJmps) << "%" << endl;
#endif
#ifdef NEVERJMP
  OutFile << "-- NeverJmp     \t" << misJmpsNever << " \t\t\t"
          << 100.0 - ((float)100 * misJmpsNever / cantJmps) << "%" << endl;
#endif
#ifdef SINGLEBIT
  OutFile << "-- SingleBit    \t" << misJmpsSingle << " \t\t\t"
          << 100.0 - ((float)100 * misJmpsSingle / cantJmps) << "%" << endl;
#endif
#ifdef TWOBITSAT
  OutFile << "-- TwoBit Sat   \t" << misJmpsTwoBitSat << " \t\t\t"
          << 100.0 - ((float)100 * misJmpsTwoBitSat / cantJmps) << "%" << endl;
#endif
#ifdef TWOBITHIST
  OutFile << "-- TwoBit Hist  \t" << misJmpsTwoBitHist << " \t\t\t"
          << 100.0 - ((float)100 * misJmpsTwoBitHist / cantJmps) << "%" << endl;
#endif
  // Cerramos el archivo de output
  OutFile << endl;
  OutFile.close();
  // Liberamos las tablas BHT
  free(single_saltar);
  free(twobits_hist_saltar);
  free(twobits_sat_saltar);
}

VOID ExitError(int errorCode) {
  if (errorCode == 1)  // bht size fuera de rango
  {
    cerr << "Error, el parametro \"s\"  del programa debe encontrarse entre 1 "
            "y 28, inclusive "
         << endl;
    exit(-1);
  }
  return;
}

// Creamos los buffers de memoria que hacen falta para los metodos
// que usan tablas BHT
void initBuffs() {
  // La BHT se mide en bits, por lo que voy a crear bloques grandes de memoria
  //(si la BHT es grande)
  int bhtsizeBits = atoi(KnobSizeBHT.Value().c_str());

  if ((bhtsizeBits < 1) || (bhtsizeBits > 28))
    ExitError(1);  // no voy a crear tres tabla de mas de 2**28 bytes

  bhtsize = 1 << atoi(KnobSizeBHT.Value().c_str());

#ifdef SINGLEBIT
  single_saltar = (bool*)calloc(bhtsize, sizeof(bool));
#endif

#ifdef TWOBITSAT
  twobits_sat_saltar = (UINT8*)calloc(bhtsize, sizeof(UINT8));
#endif

#ifdef TWOBITHIST
  twobits_hist_saltar = (UINT8*)calloc(bhtsize, sizeof(UINT8));
#endif
}
// En caso de que se pida, hay una descripcion escencial de lo que hace el
// programa
INT32 Usage() {
  cerr << endl
       << endl
       << "Esta herramienta hace un analisis de la performance del predictor";
  cerr << "de saltos del CPU con respecto a un programa especifico." << endl;
  cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
  return -1;
}
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

// Funcion principal
int main(int argc, char* argv[]) {
  // Si no se pueden inicializar correctamente los parametros, salimos
  if (PIN_Init(argc, argv)) return Usage();

  // cargamos de los parametros el path del ejecutable para ponerlo en el
  // informe
  getProgramNameToProfile(argc, argv);

  // Bindeamos la funcion "Instruction" a PIN, para que pueda interrumpir al
  // programa
  INS_AddInstrumentFunction(Instruction, 0);
  PIN_AddFiniFunction(Fini, 0);

  // Creamos los buffers
  initBuffs();

  // Never returns
  PIN_StartProgram();

  return 0;
}
