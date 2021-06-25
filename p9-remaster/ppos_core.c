// GRR20186983 João Luis Ribeiro Okimoto
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/time.h>
#include "ppos.h"
#include "ppos_data.h"

/*  -- Variaveis estáticas --

  STACKSIZE: tamanho da pilha alocada para uma tarefa
  PRIORITY: define a politica de prioridade das tarefas,
            0 -> FIFO, 1 -> prioridade por envelhecimento.
*/

#define STACKSIZE 64*1024
#define PRIORITY 0
#define QUANTUM 20

/* -- Variaveis globais do sistema --
  curr_task:  tarefa corrente
  main_task:  tarefa principal
  num_tasks:  numero de tarefas criadas
  task_queue: fila de tarefas prontas
  user_tasks: numero de tarefas correntes
*/

task_t *curr_task, *task_queue, *sleep_queue = NULL;
task_t main_task, dispatcher;
unsigned int clock = 0;
int num_tasks = 1, user_tasks = 0;

/* -- Estruturas de interrupção --
  action:  ação que trata um sinal
  timer:   um temporizador que emite uma interrupção de forma ciclica
*/

struct sigaction action;
struct itimerval timer;

// chamada do scheduler
task_t *scheduler(){
  if(task_queue == NULL) return NULL;
  task_t *aux = task_queue;
  task_t *next_task = aux;

  task_t *valid_task = NULL;

  do{
    if(next_task->suspended == 0){
      valid_task = next_task;
      break;
    }
    next_task = next_task->next;
  }while(next_task != task_queue);

  next_task = valid_task;
  if(next_task == NULL) return NULL;
  // próxima tarefa é a que tiver maior prioridade de acordo com a politica
  if(PRIORITY){
    // percorre a fila procurando a tarefa com maior prio
    int highest_prio = aux->dynamic_prio;
    while(aux->next != task_queue){
      aux = aux->next;

      // achei uma prioridade maior e não está suspensa
      if(aux->dynamic_prio < highest_prio && aux->suspended == 0){
        highest_prio = aux->dynamic_prio;
        next_task = aux;
      }
    }

    // implementa envelhecimento de tarefas
    aux = task_queue;
    while(aux->next != task_queue){
      if(aux != next_task) aux->dynamic_prio--;
      aux = aux->next;
    }

    // retorna o valor da prioridade estatica para a tarefa escolhida
    next_task->dynamic_prio = task_getprio(next_task);
  }
  // proxima tarefa é a primeira da fila de acordo com a politica FIFO

  // remove a tarefa da fila de prontas para executa-la
  queue_remove((queue_t **)&task_queue, (queue_t *)next_task);
  //printf("id : %d, suspended : %d \n", next_task->id, next_task->suspended);
  return next_task;
}

// chamada do dispatcher
void dispatcher_call(){
  task_t *next_task;

  // enquanto houverem tarefas correntes
  while(user_tasks > 0){

    int n_asleep = queue_size((queue_t *)sleep_queue);

    // existem tarefas dormentes
    if(n_asleep > 0){
      task_t *asleep_task = sleep_queue;
      task_t *aux = asleep_task;

      while(n_asleep > 0){
        aux = aux->next;
        //printf("id : %d, suspended : %d , time : %d\n", asleep_task->id, asleep_task->sleep_time, systime());
        // se a tarefa dormiu pelo tempo setado
        if(asleep_task->sleep_time == systime()){

          aux = asleep_task->next;
          // retira a tarefa da fila de dormentes
          queue_remove((queue_t **)&sleep_queue, (queue_t *)asleep_task);
          // adiciona a fila de prontas
          queue_append((queue_t **)&task_queue, (queue_t *)asleep_task);
          asleep_task->sleep_time = 0;
        }

        asleep_task = aux;
        n_asleep--;
      }
    }
    // pegue próxima tarefa
    next_task = scheduler();

    // coloque a tarefa para executar e configure seu quantum
    if(next_task != NULL){
      next_task->quantum = QUANTUM;
      task_switch(next_task);
    }
  }
  task_exit(0);
}

// chamada de interrupção
void interrupt_handler(){
  clock++;
  // se a tarefa corrente é dispatcher  ignore
  if(curr_task == &dispatcher) return;

  // se o quantum máximo da tarefa expirou, coloque ela no final da fila
  // e retorne o fluxo para o dispatcher
  if(curr_task->quantum == 0){
    if(task_queue != NULL) queue_remove((queue_t **)&task_queue, (queue_t *)curr_task);
    queue_append((queue_t **)&task_queue, (queue_t *)curr_task);
    curr_task->activations++;
    dispatcher.activations++;
    task_switch(&dispatcher);
    return;
  }

  // caso seja uma tarefa do usuario
  curr_task->ps_time++;
  curr_task->quantum--;
}

// inicialização do PingasOS
void ppos_init(){

  // ponteiros da fila, para os da tarefa principal NULL
  main_task.next = NULL;
  main_task.prev = NULL;

  // ID da main 0 por default
  main_task.id = 0;
  main_task.request_join = 0;
  main_task.suspended = 0;
  main_task.sleep_time = 0;

  // Salva contexto da tarefa main
  getcontext(&main_task.context);

  // A primeira tarefa corrente = main
  curr_task = &main_task;
  user_tasks++;
  queue_append((queue_t **)&task_queue, (queue_t *)curr_task);
  setvbuf(stdout, 0, _IONBF, 0);

  // cria o dispatcher
  task_create(&dispatcher, (void*)(*dispatcher_call), NULL);

  // configura a ação de tratamento da interrupção
  action.sa_handler = interrupt_handler ;
  sigemptyset (&action.sa_mask) ;
  action.sa_flags = 0 ;
  sigaction (SIGALRM, &action, 0);

   // configura valores do temporizador para disparar a cada 1ms
   timer.it_value.tv_usec = 1000 ;              // primeiro disparo, em micro-segundos
   timer.it_value.tv_sec  = 0 ;                 // primeiro disparo, em segundos
   timer.it_interval.tv_usec = 1000 ;           // disparos subsequentes, em micro-segundos
   timer.it_interval.tv_sec  = 0 ;              // disparos subsequentes, em segundos
   setitimer (ITIMER_REAL, &timer, 0);
}

unsigned int systime(){
  return clock;
}

// define a prioridade estática de uma tarefa (ou a tarefa atual)
void task_setprio (task_t *task, int prio){
  // coloque um limite no valor de prioridade
  if(prio < -QUANTUM) prio = -QUANTUM;
  if(prio > QUANTUM) prio = QUANTUM;

  // mude a prioridade, caso não seja especificada, mude a da tarefa atual
  if(task != NULL){
    task->static_prio = prio;
    task->dynamic_prio = prio;
  }
  else{
     curr_task->static_prio = prio;
     curr_task->dynamic_prio = prio;
  }
}


// retorna a prioridade estática de uma tarefa (ou a tarefa atual)
int task_getprio (task_t *task){
  if(task != NULL) return task->static_prio;
  else return curr_task->static_prio;
}

int task_join (task_t *task){
  if(task == NULL || task->exit == 1) return 1;


  // flag que indica que a tarefa pediu um join
  task->request_join++;

  // tarefa atual é suspensa, id de quem pediu join é guardado
  curr_task->suspended = task->id;

  // fluxo volta para o dispatcher
  task_yield();

  int code = curr_task->request_join;
  curr_task->request_join = 0;
  return code;
}

void task_sleep (int t){

  // tarefa atual recebe o tempo de dormencia t
  curr_task->sleep_time += systime() + t;
  // retira a tarefa da fila de prontas
  queue_remove((queue_t **)&task_queue, (queue_t *)curr_task);
  // adiciona a fila de dormentes
  queue_append((queue_t **)&sleep_queue, (queue_t *)curr_task);
  // fluxo volta para o dispatcher
  task_yield();
}

// fluxo retorna para o dispatcher
void task_yield(){

  // se a tarefa não foi colocada para dormir
  if(curr_task->sleep_time == 0 && curr_task->exit == 0){
    // Se a tarefa atual for de usuário colocamos-a no final da fila
    if(task_queue != NULL) queue_remove((queue_t **)&task_queue, (queue_t *)curr_task);
    queue_append((queue_t **)&task_queue, (queue_t *)curr_task);
  }

  // fluxo volta para o dispatcher
  task_switch(&dispatcher);
}

// Cria uma nova tarefa. Retorna um ID> 0 ou erro.
int task_create (task_t *task, void (*start_func)(void *), void *arg){
  char *stack;

  // guarda o contexto atual
  getcontext(&task->context);

  // aloca espaço para a pilha
  stack = malloc(STACKSIZE);
  if(stack)
   {
      task->context.uc_stack.ss_sp = stack ;
      task->context.uc_stack.ss_size = STACKSIZE ;
      task->context.uc_stack.ss_flags = 0 ;
      task->context.uc_link = 0 ;
   }
   else
   {
      perror ("Erro na criação da pilha: ") ;
      return 1;
   }

   // seta o endereço da funçao a ser executada e empilha parametros
   makecontext (&task->context, (void*)(*start_func), 1, arg);

   // seta id, prioridade e suspenso
   task->id = num_tasks;
   task->static_prio = 0;
   task->dynamic_prio = task->static_prio;
   task->suspended = 0;
   task->exit = 0;
   task->request_join = 0;
   num_tasks++;

   // seta tempo de processamento e de ativações
   task->ps_time = 0;
   task->start_time = systime();
   task->sleep_time = 0;
   task->activations = 0;

   // se a tarefa for de usuario, adicione-a na fila e incremente o contador
   if(task != &dispatcher){
     user_tasks++;
     queue_append((queue_t **)&task_queue, (queue_t *)task);
   }

   return task->id;
}

// Termina a tarefa corrente, indicando um valor de status encerramento
void task_exit (int exitCode){

  // se ela for de usuario, atualize o contador
  user_tasks--;

  printf ("Task %d exit: execution time %4d ms, processor time %4d ms, %d activations \n", curr_task->id, systime() - curr_task->start_time, curr_task->ps_time, curr_task->activations) ;

  if(curr_task == &dispatcher) return;

  // acorda as tarefas suspensas se a task atual requisitou um join
  if(curr_task->request_join > 0){

    // varra a fila até que todas as tarefas tenham sido acordadas.
    task_t *suspended_task = task_queue;
    int n_suspended = curr_task->request_join;

    // contador n_suspended armazena a quantidade de tarefas
    // que foram suspensas pela atual tarefa.
    while(n_suspended > 0){
      if(suspended_task->suspended == curr_task->id){
        // armazena o exitCode no request_join da tarefa acordada.
        suspended_task->request_join = exitCode;
        suspended_task->suspended = 0;
        // tira da fila de suspensas e coloca na fila de tarefas
        n_suspended--;
      }
      suspended_task = suspended_task->next;
    }
  }
  curr_task->exit = 1;
  // retorne para dispatcher caso seja de usuario
  task_switch(&dispatcher);
}


// alterna a execução para a tarefa indicada
int task_switch (task_t *task){
  ucontext_t *aux_context = &curr_task->context;
  curr_task = task;
  //printf("id : %d, suspended : %d \n", curr_task->id, curr_task->suspended);
  swapcontext(aux_context, &task->context);
  return 1;
}

// retorna o identificador da tarefa corrente (main deve ser 0)
int task_id(){
  return (curr_task->id);
}
