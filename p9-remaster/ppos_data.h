// GRR20186983 João Luis Ribeiro Okimoto
// PingPongOS - PingPong Operating System
// Prof. Carlos A. Maziero, DINF UFPR

// Versão 1.1 -- Julho de 2016

// Estruturas de dados internas do sistema operacional

#ifndef __PPOS_DATA__
#define __PPOS_DATA__

#include <ucontext.h>		// biblioteca POSIX de trocas de contexto
#include "queue.h"		// biblioteca de filas genéricas

// Estrutura que define um Task Control Block (TCB)
typedef struct task_t
{
   struct task_t *prev, *next ;		      // ponteiros para usar em filas
   int id ;				                      // identificador da tarefa
   ucontext_t context ;			            // contexto armazenado da tarefa
   int suspended;                       // flag que indica que a tarefa esta suspensa
   int request_join;                    // flag que indica que a tarefa requisitou um join
   int static_prio, dynamic_prio;       // nivel de prioridade estatica
   int quantum, activations;            // quantum de cada tarefa e numero de ativações
   unsigned int ps_time, start_time;    // tempo de processamento e de inicio
   unsigned int sleep_time;             // tempo de dormencia da tarefa
   int exit;
   // ... (outros campos serão adicionados mais tarde)
} task_t ;

// estrutura que define um semáforo
typedef struct
{
  // preencher quando necessário
} semaphore_t ;

// estrutura que define um mutex
typedef struct
{
  // preencher quando necessário
} mutex_t ;

// estrutura que define uma barreira
typedef struct
{
  // preencher quando necessário
} barrier_t ;

// estrutura que define uma fila de mensagens
typedef struct
{
  // preencher quando necessário
} mqueue_t ;

#endif
