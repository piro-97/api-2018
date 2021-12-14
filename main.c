#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define blank '_'
#define BUFFER_LENGTH 32
#define DIMENSIONE_BASE_STATI 512
#define DIMENSIONE_BASE_NASTRO 4096
#define DIMENSIONE_STRING_BUFFER 8192

// definizione di tipi per l'input degli stati della MT
typedef enum
{
  false,
  true
} boolean;

typedef struct
{
  char WriteChar;
  char SpostaTestina;
  int ProxStato;
} CaratteristicaTransizione;

typedef struct A
{
  // singola transizione a partire da uno stato (caratterizzata dalla lettura di un carattere specifico)
  char CurrChar;
  CaratteristicaTransizione *Lista; // lista delle possibilità nondeterministiche (biforcazioni della computazione)
  int Possibilita;                  // indica quante biforcazioni possiamo avere
  struct A *right;                  // figlio destro
  struct A *left;                   // figlio sinistro
} TipoTransizione;

typedef struct
{
  // nodi del grafo
  TipoTransizione *Testa; // radice di un albero BST contenente tutte le transizioni da quel determinato stato (archi del grafo)
  boolean Accept;         // segnala se lo stato è di accettazione
} Stato;

typedef struct
{
  Stato *Array; // array dinamico contenente l'elenco di tutti gli stati (i nodi del grafo)
  int InitializedSize;
  int UsedSize;
  int MaxSize;
} ResizableArray;

// definizione di tipi per la gestione delle configurazioni e del nastro della MT
typedef struct
{
  char *ParteDx; // parte destra del nastro gestita tramite array dinamico
  char *ParteSx; // parte sinistra del nastro gestita tramite array dinamico
  int CurrDimDx;
  int CurrDimSx;
  int UsedDx;
  int UsedSx;
  int Condivisione; // segnala che il nastro è condiviso tra # configurazioni (se 0 nastro non condiviso)
} TipoNastro;

typedef struct C
{
  // singola configurazione della MT
  int Stato;
  int PosizioneTestina;
  TipoNastro *Nastro;
  struct C *Next;
} TipoConfigurazione;

typedef enum
{
  refuse,
  accept,
  U
} AccettaStringa;

// variabili globali
long MaxStep = 0;         // numero massimo di "mosse" prima di U (max indicato nel file input)
boolean EndInput = false; // segnala EOF

// prototipi funzioni ausiliarie utilizzate
void ReadTransitions(ResizableArray *ArrayPtr); // legge da stdin l'elenco delle transizioni e degli stati e crea un grafo gestito con liste d'incidenza e un array
void Resize(ResizableArray *ArrayPtr, int MinimumSize); // ridimensiona array dinamico degli stati
void TreeInsert(TipoTransizione **Root, char CurrChar, char WriteChar, char SpostaTestina, int ProxStato); // inserisce la transizione in posizione corretta
TipoTransizione *FindTransition(TipoTransizione *Root, char DaTrovare); // cerca nondeterminismo
TipoNastro *ReadString(void); // legge una stringa da stdin e ne restituisce una lista biconcatenata (nastro d'ingresso)
void DuplicaConfig(TipoConfigurazione **Nuova, TipoConfigurazione **Vecchia, boolean FlagNastro); // duplica la configurazione ed eventualmente anche il nastro
void DuplicaNastro(TipoConfigurazione **Nuova, TipoConfigurazione **Vecchia); // duplica il nastro di una configurazione vecchia e lo assegna a una nuova
void ResizeNastro(char **DaRidimensionare, int *OldDim); // ridimensiona la parte destra o sinistra del nastro
void FreeConfigurationList(TipoConfigurazione *DaEliminare); // elimina la lista delle configurazioni e le liste Nastro associate (da chiamare in coppia con DaEliminare=NULL)
void EseguiMossa(TipoConfigurazione *ConfigurazioneCorrente, TipoTransizione *TransizioneCorrente, int NumeroTransizione); // esegue la mossa della MT

/*FUNZIONI DEBUG
  void FREE (TipoTransizione* x);
  void Print (ResizableArray Stampa);
  void Stampante (TipoTransizione* x);
*/

int main()
{

  // dichiarazione variabili
  ResizableArray TuringMachine; // array che contiene l'elenco degli stati e liste delle transizioni (grafo tramite liste)
  TipoTransizione *Pointer;     // puntatore per lo scorrimento e l'individuazione delle transizioni

  TipoNastro *Stringa;                 // lista di lettura
  AccettaStringa CurrStringAcceptance; // variabile per tenere in considerazione lo "stato" di accettazione della stringa in analisi

  TipoConfigurazione *CurrConfig, *ScorriConfig;  // Puntatori a lista di configurazioni correnti della MT, usate per l'avanzamento in parallelo delle computazioni
  TipoConfigurazione *NextStepConfig, *NewConfig; // questi due puntatori servono per la creazione a ogni passo della lista "aggiornata"

  long CurrentStep;

  // inizio codice: lettura struttura della macchina e del massimo numero di passi
  ReadTransitions(&TuringMachine);

  // esecuzione
  while (!EndInput)
  { // esco dal ciclo solo se non ho più stringhe da analizzare (End Of File dato da ReadString)

    CurrStringAcceptance = refuse;
    // Accetto la stringa (accept:1) se: trovo almeno una computazione che accetta (CurrStringAcceptance diventera 1)
    // Il risultato è undefined (U) se: almeno una computazione è U e le altre sono tutte 0 (oppure a loro volta U)
    // Rifiuto la stringa (refuse:0) se: ogni computazione rifiuta la stringa (CurrStringAcceptance rimarra 0)

    // lettura stringa, salvataggio sul nastro
    Stringa = ReadString();

    // CONDIZIONE USCITA CICLO: non devo effettuare l'ultima analisi!
    if (NULL == Stringa) // non ho più stringhe da analizzare
      break;

    CurrConfig = NULL;

    // crea configurazione base ("partenza" della MT)
    CurrConfig = malloc(sizeof(TipoConfigurazione));
    CurrConfig->Stato = 0;
    CurrConfig->Nastro = Stringa;     // collega nastro a stringa appena letta
    CurrConfig->PosizioneTestina = 0; // la testina è sul primo carattere della stringa
    CurrConfig->Next = NULL;          // prossima configurazione al livello attuale (nondeterminismo)

    NextStepConfig = NULL;

    CurrentStep = 0;

    // fase di avanzamento delle computazioni sulla stringa (ANALISI STRINGA CORRENTE)
    while ((refuse == CurrStringAcceptance) && (CurrentStep <= MaxStep))
    {

      // aggiornamento lista delle configurazioni (NextStepConfig) dopo tutte le possibili mosse sulla lista vecchia (CurrConfig)
      ScorriConfig = CurrConfig;

      // ciclo per scorrere la lista CurrConfig (scorrimento livello orizzontale corrente)
      while ((refuse == CurrStringAcceptance) && (NULL != CurrConfig))
      {
        // CurrConfig è la singola configurazione della lista che sto considerando in questo ciclo

        // CONTROLLO ACCETTAZIONE
        if (true == (TuringMachine.Array[CurrConfig->Stato]).Accept)
          CurrStringAcceptance = accept;

        // RICERCA TRANSIZIONI (tree search)
        if (CurrConfig->PosizioneTestina >= 0)
          Pointer = FindTransition(TuringMachine.Array[CurrConfig->Stato].Testa, (CurrConfig->Nastro)->ParteDx[CurrConfig->PosizioneTestina]);
        else
          Pointer = FindTransition(TuringMachine.Array[CurrConfig->Stato].Testa, (CurrConfig->Nastro)->ParteSx[-1 - CurrConfig->PosizioneTestina]);

        // inizio avanzamento computazione
        int scorrimento = 0; // serve per tenere traccia di quali transizioni ho già simulato

        if (NULL == Pointer)
        {
          // significa che da quella configurazione non posso proseguire
          // aggiorno la testa della lista e rimuovo la vecchia configurazione
          ScorriConfig = CurrConfig;
          CurrConfig = CurrConfig->Next;

          // elimino il nastro se non è condiviso
          if (0 == (ScorriConfig->Nastro)->Condivisione)
          {
            free((ScorriConfig->Nastro)->ParteDx);
            (ScorriConfig->Nastro)->ParteDx = NULL;
            free((ScorriConfig->Nastro)->ParteSx);
            (ScorriConfig->Nastro)->ParteSx = NULL;
            free(ScorriConfig->Nastro);
          }
          else
          {
            (ScorriConfig->Nastro)->Condivisione--;
          }

          ScorriConfig->Nastro = NULL;
          free(ScorriConfig);
          ScorriConfig = NULL;
        }

        while ((NULL != Pointer) && (scorrimento < Pointer->Possibilita))
        {

          if (Pointer->Possibilita > scorrimento + 1)
          {
            // ho almeno una biforcazione quindi devo per forza di cose sdoppiare la configurazione vecchia

            NewConfig = NULL;

            // RICICLO NASTRO
            // se scrivo lo stesso carattere che ho letto tengo il nastro in condivisione, nel momento in cui scrivo un carattere diverso da quello letto
            // devo duplicare il nastro e togliere la condivisione
            if (Pointer->CurrChar == Pointer->Lista[scorrimento].WriteChar)
            {
              DuplicaConfig(&NewConfig, &CurrConfig, false); // copia la configurazione vecchia ma senza duplicare il nastro
            }
            else
            {
              DuplicaConfig(&NewConfig, &CurrConfig, true);
            }

            NewConfig->Stato = Pointer->Lista[scorrimento].ProxStato; // TRANSIZIONE STATO

            // ESECUZIONE MOSSA
            TipoConfigurazione *ConfigurazioneCorrente = NewConfig;
            TipoTransizione *TransizioneCorrente = Pointer;
            int NumeroTransizione = scorrimento;
            enum
            {
              left,
              right
            } Side;
            int RealPosition; // posizione reale della testina (serve per array sx che è sfasato)

            if (ConfigurazioneCorrente->PosizioneTestina >= 0)
            {
              Side = right;
              RealPosition = ConfigurazioneCorrente->PosizioneTestina;
            }
            else
            {
              Side = left;
              RealPosition = -1 - ConfigurazioneCorrente->PosizioneTestina;
            }

            if (right == Side)
              (ConfigurazioneCorrente->Nastro)->ParteDx[RealPosition] = TransizioneCorrente->Lista[NumeroTransizione].WriteChar; // SCRITTURA CARATTERE
            else
              (ConfigurazioneCorrente->Nastro)->ParteSx[RealPosition] = TransizioneCorrente->Lista[NumeroTransizione].WriteChar; // SCRITTURA CARATTERE

            if ('R' == (TransizioneCorrente->Lista[NumeroTransizione]).SpostaTestina)
            {

              if (right == Side)
              { // sono nel nastro destro

                // se ho raggiunto il "limite destro" del nastro devo ingrandire la parte destra
                if ((ConfigurazioneCorrente->Nastro)->CurrDimDx == RealPosition + 1)
                {
                  ResizeNastro(&(((ConfigurazioneCorrente)->Nastro)->ParteDx), &((ConfigurazioneCorrente->Nastro)->CurrDimDx));
                }

                // se la testina si sposta oltre lo spazio correntemente utilizzato aumento quest'ultimo
                if ((ConfigurazioneCorrente->Nastro)->UsedDx == RealPosition + 1)
                {
                  (ConfigurazioneCorrente->Nastro)->UsedDx++; // dopo UsedDx trovo solo blank
                }
              }
              else
              { // sono nel nastro sinistro

                // se non esiste l'altro array devo crearlo
                if ((0 == RealPosition) && (0 == (ConfigurazioneCorrente->Nastro)->UsedDx))
                {
                  (ConfigurazioneCorrente->Nastro)->CurrDimDx = 0;
                  ResizeNastro(&((ConfigurazioneCorrente->Nastro)->ParteDx), &((ConfigurazioneCorrente->Nastro)->CurrDimDx));
                  (ConfigurazioneCorrente->Nastro)->UsedDx = 1;
                }

                // diminuzione grandezza nastro se ho scritto blank in fondo, nastro non condiviso
                if ((0 == (ConfigurazioneCorrente->Nastro)->Condivisione) && (1 < (ConfigurazioneCorrente->Nastro)->UsedSx) && (blank == (ConfigurazioneCorrente->Nastro)->ParteSx[(ConfigurazioneCorrente->Nastro)->UsedSx - 1]))
                {
                  (ConfigurazioneCorrente->Nastro)->UsedSx--; // dopo UsedSx avrei solo blank
                }
              }

              // movimento effettivo testina

              ConfigurazioneCorrente->PosizioneTestina++;
            }

            if ('L' == (TransizioneCorrente->Lista[NumeroTransizione]).SpostaTestina)
            {

              if (left == Side)
              { // sono nel nastro sinistro

                // se ho raggiunto il "limite sinistro" del nastro devo ingrandire la parte sinistra
                if ((ConfigurazioneCorrente->Nastro)->CurrDimSx == RealPosition + 1)
                {
                  ResizeNastro(&(((ConfigurazioneCorrente)->Nastro)->ParteSx), &((ConfigurazioneCorrente->Nastro)->CurrDimSx));
                }

                // se la testina si sposta oltre lo spazio correntemente utilizzato aumento quest'ultimo
                if ((ConfigurazioneCorrente->Nastro)->UsedSx == RealPosition + 1)
                {
                  (ConfigurazioneCorrente->Nastro)->UsedSx++; // dopo UsedSx trovo solo blank
                }
              }
              else
              { // sono nel nastro destro

                // se non esiste l'altro array devo crearlo
                if ((0 == RealPosition) && (0 == (ConfigurazioneCorrente->Nastro)->UsedSx))
                {
                  (ConfigurazioneCorrente->Nastro)->CurrDimSx = 0;
                  ResizeNastro(&((ConfigurazioneCorrente->Nastro)->ParteSx), &((ConfigurazioneCorrente->Nastro)->CurrDimSx));
                  (ConfigurazioneCorrente->Nastro)->UsedSx = 1;
                }

                // diminuzione grandezza nastro se ho scritto blank in fondo, nastro non condiviso
                if ((0 == (ConfigurazioneCorrente->Nastro)->Condivisione) && (1 < (ConfigurazioneCorrente->Nastro)->UsedDx) && (blank == (ConfigurazioneCorrente->Nastro)->ParteDx[(ConfigurazioneCorrente->Nastro)->UsedDx - 1]))
                {
                  (ConfigurazioneCorrente->Nastro)->UsedDx--; // dopo UsedSx avrei solo blank
                }
              }

              // movimento effettivo testina

              ConfigurazioneCorrente->PosizioneTestina--;
            }

            // se trovo S non devo fare nulla (la testina non si sposta)
          }
          else
          {
            //"stacco" CurrConfig dalla vecchia lista, ScorriConfig ora è l'elemento da riciclare
            ScorriConfig = CurrConfig;
            CurrConfig = CurrConfig->Next;

            NewConfig = ScorriConfig;

            NewConfig->Stato = Pointer->Lista[scorrimento].ProxStato; // TRANSIZIONE STATO

            // GESTIONE CONDIVISIONE NASTRO
            if (Pointer->CurrChar == Pointer->Lista[scorrimento].WriteChar || ((NewConfig->Nastro)->Condivisione == 0))
            {
              // posso scrivere sul nastro senza alterare altre configurazioni
            }
            else
            {
              // devo creare un nastro nuovo per NewConfig staccando quello condiviso
              (NewConfig->Nastro)->Condivisione--;
              DuplicaNastro(&NewConfig, &ScorriConfig); // sto in realtà passando a DuplicaNastro la stessa configurazione, effettuo implicitamente lo staccamento del nastro
            }

            // ESECUZIONE MOSSA
            TipoConfigurazione *ConfigurazioneCorrente = NewConfig;
            TipoTransizione *TransizioneCorrente = Pointer;
            int NumeroTransizione = scorrimento;
            enum
            {
              left,
              right
            } Side;
            int RealPosition; // posizione reale della testina (serve per array sx che è sfasato)

            if (ConfigurazioneCorrente->PosizioneTestina >= 0)
            {
              Side = right;
              RealPosition = ConfigurazioneCorrente->PosizioneTestina;
            }
            else
            {
              Side = left;
              RealPosition = -1 - ConfigurazioneCorrente->PosizioneTestina;
            }

            if (right == Side)
              (ConfigurazioneCorrente->Nastro)->ParteDx[RealPosition] = TransizioneCorrente->Lista[NumeroTransizione].WriteChar; // SCRITTURA CARATTERE
            else
              (ConfigurazioneCorrente->Nastro)->ParteSx[RealPosition] = TransizioneCorrente->Lista[NumeroTransizione].WriteChar; // SCRITTURA CARATTERE

            if ('R' == (TransizioneCorrente->Lista[NumeroTransizione]).SpostaTestina)
            {

              if (right == Side)
              { // sono nel nastro destro

                // se ho raggiunto il "limite destro" del nastro devo ingrandire la parte destra
                if ((ConfigurazioneCorrente->Nastro)->CurrDimDx == RealPosition + 1)
                {
                  ResizeNastro(&(((ConfigurazioneCorrente)->Nastro)->ParteDx), &((ConfigurazioneCorrente->Nastro)->CurrDimDx));
                }

                // se la testina si sposta oltre lo spazio correntemente utilizzato aumento quest'ultimo
                if ((ConfigurazioneCorrente->Nastro)->UsedDx == RealPosition + 1)
                {
                  (ConfigurazioneCorrente->Nastro)->UsedDx++; // dopo UsedDx trovo solo blank
                }
              }
              else
              { // sono nel nastro sinistro

                // se non esiste l'altro array devo crearlo
                if ((0 == RealPosition) && (0 == (ConfigurazioneCorrente->Nastro)->UsedDx))
                {
                  (ConfigurazioneCorrente->Nastro)->CurrDimDx = 0;
                  ResizeNastro(&((ConfigurazioneCorrente->Nastro)->ParteDx), &((ConfigurazioneCorrente->Nastro)->CurrDimDx));
                  (ConfigurazioneCorrente->Nastro)->UsedDx = 1;
                }

                // diminuzione grandezza nastro se ho scritto blank in fondo, nastro non condiviso
                if ((0 == (ConfigurazioneCorrente->Nastro)->Condivisione) && (1 < (ConfigurazioneCorrente->Nastro)->UsedSx) && (blank == (ConfigurazioneCorrente->Nastro)->ParteSx[(ConfigurazioneCorrente->Nastro)->UsedSx - 1]))
                {
                  (ConfigurazioneCorrente->Nastro)->UsedSx--; // dopo UsedSx avrei solo blank
                }
              }

              // movimento effettivo testina

              ConfigurazioneCorrente->PosizioneTestina++;
            }

            if ('L' == (TransizioneCorrente->Lista[NumeroTransizione]).SpostaTestina)
            {

              if (left == Side)
              { // sono nel nastro sinistro

                // se ho raggiunto il "limite sinistro" del nastro devo ingrandire la parte sinistra
                if ((ConfigurazioneCorrente->Nastro)->CurrDimSx == RealPosition + 1)
                {
                  ResizeNastro(&(((ConfigurazioneCorrente)->Nastro)->ParteSx), &((ConfigurazioneCorrente->Nastro)->CurrDimSx));
                }

                // se la testina si sposta oltre lo spazio correntemente utilizzato aumento quest'ultimo
                if ((ConfigurazioneCorrente->Nastro)->UsedSx == RealPosition + 1)
                {
                  (ConfigurazioneCorrente->Nastro)->UsedSx++; // dopo UsedSx trovo solo blank
                }
              }
              else
              { // sono nel nastro destro

                // se non esiste l'altro array devo crearlo
                if ((0 == RealPosition) && (0 == (ConfigurazioneCorrente->Nastro)->UsedSx))
                {
                  (ConfigurazioneCorrente->Nastro)->CurrDimSx = 0;
                  ResizeNastro(&((ConfigurazioneCorrente->Nastro)->ParteSx), &((ConfigurazioneCorrente->Nastro)->CurrDimSx));
                  (ConfigurazioneCorrente->Nastro)->UsedSx = 1;
                }

                // diminuzione grandezza nastro se ho scritto blank in fondo, nastro non condiviso
                if ((0 == (ConfigurazioneCorrente->Nastro)->Condivisione) && (1 < (ConfigurazioneCorrente->Nastro)->UsedDx) && (blank == (ConfigurazioneCorrente->Nastro)->ParteDx[(ConfigurazioneCorrente->Nastro)->UsedDx - 1]))
                {
                  (ConfigurazioneCorrente->Nastro)->UsedDx--; // dopo UsedSx avrei solo blank
                }
              }

              // movimento effettivo testina

              ConfigurazioneCorrente->PosizioneTestina--;
            }

            // se trovo S non devo fare nulla (la testina non si sposta)
          }

          scorrimento++;
          NewConfig->Next = NextStepConfig;
          NextStepConfig = NewConfig;
        }

        // ATTENZIONE: indipendentemente da Possibilità un riciclo di CurrConfig avviene sempre!
        // se non avviene significa che non avevo possibili transizioni e quindi elimino direttamente CurrConfig

        // l'indice del ciclo (CurrConfig) è già aggiornato (dai casi dovuti a possibilità = 1 oppure = 0)

        // esco dal ciclo se ho creato tutti i casi nondeterministici dovuti a TUTTE le vecchie configurazioni (NextStepConfig è pronta)
      }

      FreeConfigurationList(CurrConfig); // se sono avanzate configurazioni "vecchie" significa che sono uscito dal ciclo perché dovevo accettare la stringa

      CurrConfig = NextStepConfig; // aggiornamento per l'iterazione successiva
      NextStepConfig = NULL;

      CurrentStep++;

      // valutazione loop (U dovuto al raggiungimento di MaxStep)
      if ((accept != CurrStringAcceptance) && (CurrentStep > MaxStep)) // il risultato delle computazioni è U se non è 1
        CurrStringAcceptance = U;

      // FINE CICLO se non ho computazioni rimaste
      if ((NULL == CurrConfig)) // significa che non posso più lavorare su altre configurazioni perché le precedenti sono terminate
        break;

      // esco dal ciclo se trovo uno stato di accettazione, se ho oltrepassato max oppure se le computazioni sono terminate
    }

    // liberazione memoria delle strutture utilizzate
    FreeConfigurationList(CurrConfig);
    CurrConfig = NULL;

    // SCRITTURA RISULTATO
    if (accept == CurrStringAcceptance)
      printf("1\n");
    else if (refuse == CurrStringAcceptance)
      printf("0\n");
    else
      printf("U\n");

    // ciclo finito: leggo prossima stringa
    // esco dal ciclo se le stringhe in input sono terminate (feof)
  }

  // LIBERA MEM ALLOCATA
  /*
  for (int i = 0; i < TuringMachine.MaxSize; i++) {
    if ((TuringMachine.Array[i].Testa) != NULL) {
      FREE ((TuringMachine.Array[i].Testa)->left);
      FREE ((TuringMachine.Array[i].Testa)->right);
      free((TuringMachine.Array[i].Testa)->Lista);
      (TuringMachine.Array[i].Testa)->Lista = NULL;
      free(TuringMachine.Array[i].Testa);
      TuringMachine.Array[i].Testa = NULL;
    }
  }
  free (TuringMachine.Array);
  TuringMachine.Array = NULL;
  TuringMachine.MaxSize = 0;
  TuringMachine.InitializedSize = 0;
  TuringMachine.UsedSize = 0;
  */

  return 0;
}

// FUNZIONI AUSILIARIE

// funzione per la lettura dell'elenco delle transizioni da stdin (segnala anche acc, max e termina leggendo run)
void ReadTransitions(ResizableArray *ArrayPtr)
{
  int StatoBase, StatoArrivo;
  char CharBase, CharScritto, Spostamento;
  char Buffer[BUFFER_LENGTH]; // serve per tenere traccia della stringa letta tramite fgets
  int maximum = 0;

  // inizializza Buffer
  for (size_t i = 0; i < BUFFER_LENGTH; i++)
  {
    Buffer[i] = '\0';
  }

  // Creazione dell'array degli stati
  ArrayPtr->MaxSize = DIMENSIONE_BASE_STATI; //[ArrayPtr->MaxSize] è la prima cella libera dell'array
  ArrayPtr->InitializedSize = 0;             //[ArrayPtr->InitializedSize] è la prima cella dell'array non ancora inizializzata
  ArrayPtr->UsedSize = 0;                    //[ArrayPtr->UsedSize] è l'ultima cella dell'array da cui partono transizioni
  ArrayPtr->Array = malloc((ArrayPtr->MaxSize) * sizeof(Stato));

  // inizializzazione dei puntatori alle transizioni dei singoli stati
  for (int index = ArrayPtr->InitializedSize; index < ArrayPtr->MaxSize; index++)
  {
    (ArrayPtr->Array[index]).Testa = NULL;
    (ArrayPtr->Array[index]).Accept = false;
  }
  ArrayPtr->InitializedSize = ArrayPtr->MaxSize;

  // inizio lettura
  fgets(Buffer, BUFFER_LENGTH, stdin); // Buffer contiene la prima riga del file, cioè tr\n
  fgets(Buffer, BUFFER_LENGTH, stdin); // Ora ho letto la prima riga significativa che identifica le transizioni

  // creazione grafo stati/transizioni a seguito della lettura da stdin finché non incontro "acc"
  while (!((0 == strcmp("acc\n", Buffer)) || (0 == strcmp("acc\r\n", Buffer))))
  { // esco dal ciclo solo se leggo "acc\n"

    sscanf(Buffer, "%d %c %c %c %d", &StatoBase, &CharBase, &CharScritto, &Spostamento, &StatoArrivo);

    // ridimensionamento array se necessario

    if (StatoArrivo < StatoBase)
      maximum = StatoBase;
    else
      maximum = StatoArrivo;

    if (maximum >= ArrayPtr->MaxSize)
      Resize(ArrayPtr, maximum); // ridimensiona array, considerando la nuova dimensione minima = maximum

    // aggiornamento dimensioni dell'utilizzo dell'array: UsedSize = max(statopartenza) forall transitions
    if (ArrayPtr->UsedSize < maximum)
      ArrayPtr->UsedSize = maximum;

    // inserimento elemento
    TreeInsert(&((ArrayPtr->Array[StatoBase]).Testa), CharBase, CharScritto, Spostamento, StatoArrivo); // inserisce la transizione nell'apposito posto dell'albero
    // notare che il valore di Testa è per forza inizializzato o all'inizio di ReadTransitions o in seguito a Resize

    fgets(Buffer, BUFFER_LENGTH, stdin); // leggo la riga successiva
  }

  // una volta lette tutte le transizioni "elimino la memoria non utilizzata" ridimensionando l'array
  ArrayPtr->MaxSize = ArrayPtr->UsedSize + 1;
  ArrayPtr->Array = realloc(ArrayPtr->Array, (ArrayPtr->MaxSize) * sizeof(Stato));
  ArrayPtr->InitializedSize = ArrayPtr->MaxSize;

  // aggiunta flag agli stati d'accettazione
  fgets(Buffer, BUFFER_LENGTH, stdin); // ora il buffer contiene il primo stato d'accettazione

  while (!((0 == strcmp("max\n", Buffer)) || (0 == strcmp("max\r\n", Buffer))))
  { // esco dal ciclo solo se leggo "acc\n"

    sscanf(Buffer, "%d", &StatoBase);

    if (StatoBase < ArrayPtr->MaxSize)
      (ArrayPtr->Array[StatoBase]).Accept = true;
    // se lo stato di accettazione supera la dimensione dell'array significa che non ho alcuna transizione verso questo, quindi è inutile

    fgets(Buffer, BUFFER_LENGTH, stdin);
  }

  // lettura del numero massimo di step prima di considerare loop
  fgets(Buffer, BUFFER_LENGTH, stdin);
  sscanf(Buffer, "%ld", &MaxStep);

  // lettura del tag "run"
  fgets(Buffer, BUFFER_LENGTH, stdin);
  // ora posso leggere le stringhe
  return;
}

// funzione che inserisce la transizione in posizione corretta
void TreeInsert(TipoTransizione **Root, char CurrChar, char WriteChar, char SpostaTestina, int ProxStato)
{

  TipoTransizione *Scorri, *Prev;

  if ((*Root) == NULL)
  {
    // devo creare la radice
    *Root = malloc(sizeof(TipoTransizione));
    (*Root)->CurrChar = CurrChar;
    (*Root)->Lista = NULL;
    (*Root)->Possibilita = 0;
    (*Root)->left = NULL;
    (*Root)->right = NULL;
  }

  // ricerca carattere nell'albero per inserimento
  Scorri = *Root;
  while ((Scorri != NULL) && (Scorri->CurrChar != CurrChar))
  {
    Prev = Scorri;
    if (CurrChar < Scorri->CurrChar)
    {
      Scorri = Scorri->left;
    }
    else
      Scorri = Scorri->right;
  }

  // inserimento o creazione nuovo elemento
  if (Scorri == NULL)
  {
    // creo nuovo elemento
    if (CurrChar < Prev->CurrChar)
    {
      (Prev->left) = malloc(sizeof(TipoTransizione));
      Scorri = Prev->left;
    }
    else
    {
      (Prev->right) = malloc(sizeof(TipoTransizione));
      Scorri = Prev->right;
    }
    Scorri->CurrChar = CurrChar;
    Scorri->Lista = malloc(sizeof(CaratteristicaTransizione));
    Scorri->Possibilita = 0;
    Scorri->left = NULL;
    Scorri->right = NULL;
    Scorri->Lista[Scorri->Possibilita].WriteChar = WriteChar;
    Scorri->Lista[Scorri->Possibilita].SpostaTestina = SpostaTestina;
    Scorri->Lista[Scorri->Possibilita].ProxStato = ProxStato;
    Scorri->Possibilita++;
  }
  else
  {
    // ho una transizione da inserire nella lista
    Scorri->Lista = realloc(Scorri->Lista, (Scorri->Possibilita + 1) * sizeof(CaratteristicaTransizione));
    Scorri->Lista[Scorri->Possibilita].WriteChar = WriteChar;
    Scorri->Lista[Scorri->Possibilita].SpostaTestina = SpostaTestina;
    Scorri->Lista[Scorri->Possibilita].ProxStato = ProxStato;
    Scorri->Possibilita++;
  }
}

// funzione per il ridimensionamento dell'array
void Resize(ResizableArray *ArrayPtr, int MinimumSize)
{

  if (MinimumSize > (ArrayPtr->MaxSize) * 2)
    ArrayPtr->MaxSize = 3 * MinimumSize / 2;
  else
    ArrayPtr->MaxSize = 2 * (ArrayPtr->MaxSize); // raddopio le dimensioni vecchie dell'array

  ArrayPtr->Array = realloc(ArrayPtr->Array, (ArrayPtr->MaxSize) * sizeof(Stato));

  // inizializzazione dei puntatori alle transizioni dei singoli stati(non ancora utilizzati)
  for (int index = ArrayPtr->InitializedSize; index < ArrayPtr->MaxSize; index++)
  {
    (ArrayPtr->Array[index]).Testa = NULL;
    (ArrayPtr->Array[index]).Accept = false;
  }

  ArrayPtr->InitializedSize = ArrayPtr->MaxSize;

  return;
}

// funzione per eliminare una lista di configurazioni
void FreeConfigurationList(TipoConfigurazione *DaEliminare)
{
  TipoConfigurazione *NextConfigDaElim;

  while (NULL != DaEliminare)
  {

    // elimino il nastro
    if ((NULL != DaEliminare->Nastro) && (0 == (DaEliminare->Nastro)->Condivisione))
    {
      free((DaEliminare->Nastro)->ParteDx);
      (DaEliminare->Nastro)->ParteDx = NULL;
      free((DaEliminare->Nastro)->ParteSx);
      (DaEliminare->Nastro)->ParteSx = NULL;
      free(DaEliminare->Nastro);
    }
    else if (0 < (DaEliminare->Nastro)->Condivisione)
    {
      (DaEliminare->Nastro)->Condivisione--; // non posso cancellare un nastro condiviso
    }

    DaEliminare->Nastro = NULL;

    NextConfigDaElim = DaEliminare->Next;
    free(DaEliminare);
    DaEliminare = NULL;
    DaEliminare = NextConfigDaElim;
  }

  return;
}

// funzione per duplicare una configurazione
void DuplicaConfig(TipoConfigurazione **Nuova, TipoConfigurazione **Vecchia, boolean FlagNastro)
{

  *Nuova = malloc(sizeof(TipoConfigurazione));

  (*Nuova)->Next = NULL;

  // COPIA STATO
  (*Nuova)->Stato = (*Vecchia)->Stato;

  // COPIA POSIZIONE TESTINA
  (*Nuova)->PosizioneTestina = (*Vecchia)->PosizioneTestina; // la nuova testina è nella stessa posizione di quella vecchia

  // COPIA NASTRO
  if (true == FlagNastro)
  { // mi viene indicato che devo duplicare anche il nastro
    DuplicaNastro(Nuova, Vecchia);
  }
  else
  {
    (*Nuova)->Nastro = (*Vecchia)->Nastro;
    ((*Nuova)->Nastro)->Condivisione++;
  }
}

// funzione per la duplicazione del nastro
void DuplicaNastro(TipoConfigurazione **Nuova, TipoConfigurazione **Vecchia)
{
  // NON AGISCE IN ALCUN MODO SUL VECCHIO NASTRO: la condivisione va diminuita a priori prima della chiamata a DuplicaNastro

  TipoNastro *Temporaneo;

  Temporaneo = (*Vecchia)->Nastro;

  // COPIA NASTRO e parametri di gestione
  (*Nuova)->Nastro = malloc(sizeof(TipoNastro));

  ((*Nuova)->Nastro)->Condivisione = 0; // il nastro non è condiviso con altre configurazioni

  ((*Nuova)->Nastro)->CurrDimDx = Temporaneo->UsedDx; // ricopio solo la parte significativa del vecchio nastro

  ((*Nuova)->Nastro)->CurrDimSx = Temporaneo->UsedSx; // ricopio solo la parte significativa del vecchio nastro

  // creazione nastro

  ((*Nuova)->Nastro)->ParteDx = malloc((((*Nuova)->Nastro)->CurrDimDx) * sizeof(char));
  ((*Nuova)->Nastro)->ParteSx = malloc((((*Nuova)->Nastro)->CurrDimSx) * sizeof(char));

  ((*Nuova)->Nastro)->UsedDx = Temporaneo->UsedDx;
  ((*Nuova)->Nastro)->UsedSx = Temporaneo->UsedSx;

  memcpy(((*Nuova)->Nastro)->ParteDx, Temporaneo->ParteDx, sizeof(char) * ((*Nuova)->Nastro)->CurrDimDx);
  memcpy(((*Nuova)->Nastro)->ParteSx, Temporaneo->ParteSx, sizeof(char) * ((*Nuova)->Nastro)->CurrDimSx);
}

// funzione per la lettura di stringhe
TipoNastro *ReadString(void)
{
  TipoNastro *Head;
  char Buffer[DIMENSIONE_STRING_BUFFER];

  // creazione elemento Nastro
  Head = malloc(sizeof(TipoNastro));

  Head->CurrDimDx = DIMENSIONE_BASE_NASTRO; // grandezza iniziale default
  Head->CurrDimSx = 0;
  Head->ParteDx = malloc(DIMENSIONE_BASE_NASTRO * sizeof(char));
  Head->ParteSx = NULL;
  Head->UsedDx = 0;
  Head->UsedSx = 0;
  Head->Condivisione = 0; // il nastro non è condiviso con altre configurazioni

  // inizializzazione Array
  for (int i = Head->UsedDx; i < Head->CurrDimDx; i++)
    Head->ParteDx[i] = blank;

  // inizializza Buffer
  for (int i = 0; i < DIMENSIONE_STRING_BUFFER; i++)
  {
    Buffer[i] = '\0';
  }
  Buffer[DIMENSIONE_STRING_BUFFER - 2] = '\n';

  fgets(Buffer, DIMENSIONE_STRING_BUFFER, stdin); // lettura prima parte della riga

  while ((!feof(stdin)) && (Buffer[DIMENSIONE_STRING_BUFFER - 2] != '\0') && (Buffer[DIMENSIONE_STRING_BUFFER - 2] != '\n'))
  {

    while (Head->CurrDimDx <= Head->UsedDx + DIMENSIONE_STRING_BUFFER)
    {
      // ridimensiona l'array per il contenimento della stringa
      ResizeNastro(&(Head->ParteDx), &(Head->CurrDimDx));
    }

    strcpy(&(Head->ParteDx[Head->UsedDx]), Buffer); // strcpy considera anche '\0'

    Head->UsedDx = Head->UsedDx + strlen(Buffer); // strlen non considera '\0'

    Buffer[DIMENSIONE_STRING_BUFFER - 2] = '\n';

    fgets(Buffer, DIMENSIONE_STRING_BUFFER, stdin);
  }

  if (Buffer[DIMENSIONE_STRING_BUFFER - 3] == '\r')
    Buffer[DIMENSIONE_STRING_BUFFER - 3] = blank;

  while (Head->CurrDimDx <= Head->UsedDx + strlen(Buffer) + 1)
  {
    // ridimensiona l'array per il contenimento della stringa
    ResizeNastro(&(Head->ParteDx), &(Head->CurrDimDx));
  }

  strcpy(&(Head->ParteDx[Head->UsedDx]), Buffer);

  Head->UsedDx = Head->UsedDx + strlen(Buffer); // considero anche '\n' in posizione Head->UsedDx - 1

  if ('\0' == Head->ParteDx[Head->UsedDx])
    Head->ParteDx[Head->UsedDx] = blank; // elimino '\0'

  if ((Head->UsedDx > 0) && ('\n' == Head->ParteDx[Head->UsedDx - 1]))
    Head->ParteDx[Head->UsedDx - 1] = blank; // elimino '\n'

  Head->UsedDx--;

  if (feof(stdin))
  {
    EndInput = true;
  }

  if (Head->ParteDx[0] == blank) // stringa vuota, feof
    return NULL;

  return Head;
}

// funzione per il raddoppio di una porzione (dx o sx) del nastro
void ResizeNastro(char **DaRidimensionare, int *OldDim)
{
  int Inizializzato;

  Inizializzato = *OldDim;

  if (0 == (*OldDim))
    *OldDim = 1;

  *OldDim = 2 * (*OldDim);

  (*DaRidimensionare) = realloc((*DaRidimensionare), (*OldDim) * sizeof(char));

  for (int i = Inizializzato; i < *OldDim; i++)
  {
    (*DaRidimensionare)[i] = blank;
  }
}

// funzione per la ricerca della transizione
TipoTransizione *FindTransition(TipoTransizione *Root, char DaTrovare)
{

  while ((NULL != Root) && (DaTrovare != Root->CurrChar))
  {

    if (DaTrovare < Root->CurrChar)
    {
      Root = Root->left;
    }
    else
    {
      Root = Root->right;
    }
  }

  return Root;
}

/*FUNZIONI DEBUG
void FREE (TipoTransizione* x) {
  if (x == NULL)  return;

  FREE (x->left);
  x->left = NULL;
  FREE (x->right);
  x->right = NULL;

  free(x->Lista);
  x->Lista = NULL;

  free(x);
  x = NULL;


}

void Print (ResizableArray Stampa)  {
  for (size_t i = 0; i <= Stampa.UsedSize; i++) {
    printf("Stato %d:\n",i );
    Stampante (Stampa.Array[i].Testa);
    printf("\n" );
  }
}

void Stampante (TipoTransizione* x) {
  if (x == NULL)
    return;

  Stampante (x->left);

  for (size_t i = 0; i < x->Possibilita; i++) {
    printf("%c %c %c %d\n",x->CurrChar,x->Lista[i].WriteChar,x->Lista[i].SpostaTestina,x->Lista[i].ProxStato );
  }

  Stampante (x->right);
}
*/
