// Francisco Teixeira de Barros, LETI (85069) & Mafalda Maria Ferreira de Vilar Gaspar, LEIC (77921)
// Projeto SO - exercicio 4, version 1.2, GRUPO 83
// Sistemas Operativos, DEI/IST/ULisboa 2016-17
#ifndef I_BANCO_H
#define I_BANCO_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>

#define COMANDO_DEBITAR 	   "debitar"
#define COMANDO_CREDITAR 	   "creditar"
#define COMANDO_TRANSFERIR 	   "transferir"
#define COMANDO_LER_SALDO 	   "lerSaldo"
#define COMANDO_SIMULAR		   "simular"
#define COMANDO_SAIR 		   "sair"
#define COMANDO_ARG_SAIR_AGORA "agora"
#define COMANDO_SAIR_TERMINAL  "sair-terminal"

#define OP_LER_SALDO			0
#define OP_CREDITAR				1
#define OP_DEBITAR 				2
#define OP_SAIR      			3
#define OP_SAIR_AGORA			4
#define OP_TRANSFERIR   		5
#define OP_SIMULAR				6
#define OP_CRIA_PIPE			7
#define SEMSIGNFICADO			8

#define NUM_TRABALHADORAS  		3
#define CMD_BUFFER_DIM     		(NUM_TRABALHADORAS * 2)

#define MAXARGS					4
#define BUFFER_SIZE 			100

#define MAXFILHOS 				20

typedef struct {
	int operacao;
	int idConta;
	int idContaDestino;
	int valor;
	pid_t processId;
} comando_t;

void lock_trinco_ficheiro_log();
void unlock_trinco_ficheiro_log();
void escreve_no_ficheiro_log(char* logEntry);
void fecha_ficheiro_log();
void cria_ficheiro_simular();
void fecha_ficheiro_simular(int simularFileDescriptor);
void wait_on_read_sem(void); 
void post_to_read_sem(void);
void wait_on_write_sem(void);
void post_to_write_sem(void);
void lock_pedidos_pendentes(void);
void unlock_pedidos_pendentes(void);
void wait_cond_simular();
void signal_cond_simular();
void lock_cmd_buffer(void);
void unlock_cmd_buffer(void);
void put_cmd(comando_t *cmd);
void get_cmd(comando_t *cmd);
void inicia_log();
void inicia_sinais();
void inicia_trincos();
void inicia_variaveis_condicao();
void inicia_semaforos();
void inicia_tarefas();
void termina_semaforos();
void termina_trincos();
void termina_variaveis_condicao();
void *thread_main(void *arg_ptr);

#endif