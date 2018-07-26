// Francisco Teixeira de Barros, LETI (85069) & Mafalda Maria Ferreira de Vilar Gaspar, LEIC (77921)
// Projeto SO - exercicio 4, version 1.2, GRUPO 83
// Sistemas Operativos, DEI/IST/ULisboa 2016-17

#include "i-banco.h"
#include "contas.h"

sem_t sem_read_ctrl, sem_write_ctrl;

comando_t cmd;
comando_t cmd_buffer[CMD_BUFFER_DIM];

int buff_write_idx = 0;
int buff_read_idx = 0;
int pedidos_pendentes = 0;

int t_num[NUM_TRABALHADORAS];

int rv, readingPipeDescriptor, writingPipeDescriptor, logFileDescriptor;

char pipeName[128] = "i-banco-pipe";

pthread_t thread_id[NUM_TRABALHADORAS];
pthread_cond_t condSimular;
pthread_mutex_t buffer_ctrl, pedidos_pendentes_ctrl;

/**************************************************************************/
/*				Aplicaocao "Produtor"						              */
/**************************************************************************/

/* O i-banco inicia sem argumentos portanto pode ser main(void) */

int main () {

	int numFilhos = 0;

	pid_t pidFilhos[MAXFILHOS];

	inicia_log();
	inicializarContas();
	inicia_sinais();
	inicia_trincos();
	inicia_variaveis_condicao();
	inicia_semaforos();
	inicia_tarefas();


	printf("Bem-vinda/o ao i-banco\n\n");
    
	/* Cria pipe para leitura de comandos de todos os terminais com permissoes para WRX para todos */
	while ((rv = mkfifo(pipeName, 0777)) == -1) {
		/* Se o pipe ja existir elimina-o e volta a tentar */
		if (errno == EEXIST) {
			if ((rv = unlink(pipeName)) == -1) {
				perror("Erro ao eliminar pipe de rececao de comando desatualizado do Sistema de Ficheiros");
				exit(EXIT_FAILURE);
			}
		}
		/* Caso nao seja possivel criar o pipe no diretorio atual por falta de permissoes modifica-se o dir */
		else if (errno == EACCES || errno == EROFS) {
			strcpy(pipeName, "/tmp/i-banco-pipe");
		}
		/* Caso contrario emite-se erro e termina-se aplicacao */
		else {
			perror("Erro na criacao de pipe para leitura de comandos");
			exit(EXIT_FAILURE);
		}
	}

	/* Abre pipe em modo nao bloqueante para ler comandos vindos do terminal */
	if ((readingPipeDescriptor = open(pipeName, O_RDONLY)) == -1) {
		perror("Erro ao abrir pipe para leitura de comandos em i-banco");
		exit(EXIT_FAILURE);
	}

	while (1) {

		int send_cmd_to_thread = 0;
		/* Le-se uma estrutura comando_t do pipe de rececao de dados dos clientes */
		read(readingPipeDescriptor, &cmd, sizeof(comando_t));

/**************************************************************************/
/*				Sair e sair agora							              */
/**************************************************************************/

		/* Se operacao for sair ou sair agora entao nao corre em tarefas */
		if (cmd.operacao == OP_SAIR) {
			int i;
			if (cmd.valor == OP_SAIR_AGORA) {
				for (i = 0; i < numFilhos; i++)
					kill(pidFilhos[i], SIGUSR1);
			}
		
			for (i = 0; i < NUM_TRABALHADORAS; ++i) {
				wait_on_write_sem();
				put_cmd(&cmd);
				post_to_read_sem();
			}

		 	/* Espera pela terminação de cada thread */
			for (i = 0; i < NUM_TRABALHADORAS; ++i) {
				if ((errno = pthread_join(thread_id[i], NULL)) != 0) {
					perror("Error joining thread.");
					exit(EXIT_FAILURE);
				}
			}

		  	/* Espera pela terminacao de processos filho */
			while (numFilhos > 0) {
				int status;
				pid_t childpid;
				childpid = wait(&status);
				if (childpid < 0) {
					if (errno == EINTR) {
					/* Erro: chegou signal que interrompeu a espera pela terminacao de filho; logo voltamos a esperar */
						continue;
					}
					else if (errno == ECHILD) {
					/* Erro: não há mais processos filho */
						break;
					}
					else {
						perror("Erro inesperado ao esperar por processo filho.");
						exit (EXIT_FAILURE);
					}
				}

				numFilhos --;
				if (WIFEXITED(status))
					printf("FILHO TERMINADO: (PID=%d; terminou normalmente)\n", childpid);
				else
					printf("FILHO TERMINADO: (PID=%d; terminou abruptamente)\n", childpid);
			}
			printf("--\ni-banco terminou.\n");

			termina_semaforos();
			termina_trincos();
			termina_variaveis_condicao();
			destroy_mutex_contas();
			fecha_ficheiro_log();

            if ((rv = close(readingPipeDescriptor)) == -1) {
                perror("Erro ao fechar pipe de rececao de comandos ao sair de i-banco\n");
                exit(EXIT_FAILURE);
            }

            if ((rv = unlink(pipeName)) == -1) {
                perror("Erro ao eliminar pipe de rececao de comandos ao sair de i-banco\n");
                exit(EXIT_FAILURE);
            }

			exit(EXIT_SUCCESS);
		}

/**************************************************************************/
/*				Simular										              */
/**************************************************************************/

		/* Se operacao for simular entao nao corre em tarefas e nao faz outputs para o pipe */
		else if (cmd.operacao == OP_SIMULAR) {

			int pid;
			int numAnos = cmd.valor;

			if (numFilhos >= MAXFILHOS) {
				printf("%s: Atingido o numero maximo de processos filho a criar.\n", COMANDO_SIMULAR);
				continue;
			}

			lock_pedidos_pendentes();
			while(pedidos_pendentes > 0)
				wait_cond_simular();
			unlock_pedidos_pendentes();

			pid = fork();
			if (pid < 0) {
				perror("Failed to create new process.\n");
				exit(EXIT_FAILURE);
			}

			if (pid > 0) { 	 
				pidFilhos[numFilhos] = pid;
				numFilhos ++;
				printf("%s(%d): Simulacao iniciada em background.\n\n", COMANDO_SIMULAR, numAnos);
				continue;
			}
			else {
				cria_ficheiro_simular();
				simular(numAnos);
				fecha_ficheiro_log();
				exit(EXIT_SUCCESS);
			}
		}

/**************************************************************************/
/*				Comando para threads						              */
/**************************************************************************/

		/* Caso a operacao nao seja sair nem simular entao o comando lido da pipe pode ser enviado para o cmd_buffer */
		else
			send_cmd_to_thread = 1;

		/* Produz um comando e coloca-o no array */
		if (send_cmd_to_thread == 1) {
			wait_on_write_sem();
			put_cmd(&cmd);  
			post_to_read_sem();

			lock_pedidos_pendentes();
			pedidos_pendentes++;
			unlock_pedidos_pendentes();
		}
	} 
}

/**************************************************************************/
/*				Aplicaocao "Consumidor"						              */
/**************************************************************************/

void *thread_main(void *arg_ptr) {
	pid_t processId;

	int t_num, rv, operacao, idConta, idContaDestino, valor, stdoutOrigin;

	long unsigned int tid = pthread_self();

	char logEntry[128] = "";
	char outputePipeName[128] = "";

	comando_t cmd;
	t_num = *((int *)arg_ptr);

	while(1) {
		wait_on_read_sem();
		lock_cmd_buffer();
		get_cmd(&cmd);
		unlock_cmd_buffer();
		post_to_write_sem();

		operacao = cmd.operacao;
		idConta = cmd.idConta;
		idContaDestino = cmd.idContaDestino;
		valor = cmd.valor;
		processId = cmd.processId;

		/* Se a operacao for sair nao existe necessidade de abrir pipes, apenas terminamos a thread */
		if (operacao == OP_SAIR) {
			return NULL;
		}

		/* Guardamos o output da tarefa atual para o repor mais tarde */
		if ((stdoutOrigin = dup(1)) == -1) {
			perror("Erro ao guardar stdout de tarefa trabalhadora\n");
			exit(EXIT_FAILURE);
		}

        /* Gera nome do pipe para escrever outputs para o terminal que enviou os comandos */
		sprintf(outputePipeName, "/tmp/i-banco-terminal-%u", processId);
        /* Abre pipe para envio de outputs para o terminal que gerou o comando */
		if ((writingPipeDescriptor = open(outputePipeName, O_WRONLY)) == -1) {
			if (errno == ENOENT) {
				continue;
			}
			perror("Erro abrir pipe para escrita de outputs em i-banco\n");
			exit(EXIT_FAILURE);
		}

        /* Quaisquer printfs que seriam escritos no stdout sao ao inves disso escritos no pipe do terminal que lancou a operacao */
		if ((rv = dup2(writingPipeDescriptor, fileno(stdout))) == -1) {
			perror("Erro ao redirecionar outputs do stdout para pipe de escrita de outputs\n");
			exit(EXIT_FAILURE);
		}

		if (operacao == OP_LER_SALDO) {
			int saldo;
			saldo = lerSaldo (idConta);
			if (saldo < 0) {
				printf("Erro ao ler saldo da conta %d.\n", cmd.idConta);
			}
			else {
				printf("%s(%d): O saldo da conta é %d.\n\n", COMANDO_LER_SALDO, cmd.idConta, saldo);
			}
			sprintf(logEntry, "%lu: %s (%d)\n", tid, COMANDO_LER_SALDO, idConta);
			escreve_no_ficheiro_log(logEntry);
		}

		else if (operacao == OP_CREDITAR) {
			if (creditar (idConta, valor) < 0) {
				printf("Erro ao creditar %d na conta %d.\n", cmd.valor, cmd.idConta);
			}
			else {
				printf("%s(%d, %d): OK\n\n", COMANDO_CREDITAR, cmd.idConta, cmd.valor);
			}
			sprintf(logEntry, "%lu: %s (%d, %d)\n", tid, COMANDO_CREDITAR, idConta, valor);
			escreve_no_ficheiro_log(logEntry);
		}

		else if (operacao == OP_DEBITAR) {
			if (debitar (idConta, valor) < 0) {
				printf("Erro ao debitar %d na conta %d.\n", cmd.valor, cmd.idConta);
			}
			else {
				printf("%s(%d, %d): OK\n\n", COMANDO_DEBITAR, cmd.idConta, cmd.valor);
			}
			sprintf(logEntry, "%lu: %s (%d, %d)\n", tid, COMANDO_DEBITAR, idConta, valor);
			escreve_no_ficheiro_log(logEntry);
		}

		else if (operacao == OP_TRANSFERIR) {
			if (transferir (idConta, idContaDestino, valor) < 0) {
				printf("Erro ao transferir %d da conta %d para a conta %d.\n", cmd.valor, cmd.idConta, cmd.idContaDestino);
			}
			else {
				printf("%s(%d, %d, %d): OK\n\n", COMANDO_TRANSFERIR, cmd.idConta, cmd.idContaDestino, cmd.valor);
			}
			sprintf(logEntry, "%lu: %s (%d, %d, %d)\n", tid, COMANDO_TRANSFERIR, idConta, idContaDestino, valor);
			escreve_no_ficheiro_log(logEntry);
		}

		else {
			printf("Thread %d: Unknown command: %d\n", t_num, cmd.operacao);
			continue;
		}

		fflush(stdout);

		if ((rv = close(writingPipeDescriptor)) == -1) {
			perror("Erro ao fechar pipe de escrita de outputs durante a finalizacao de um comando\n");
			exit(EXIT_FAILURE);
		}

        /* Repoe o stdout por defeito */
		if ((rv = dup2(stdoutOrigin, fileno(stdout))) == -1) {
			perror("Erro ao redirecionar outputs do stdout para pipe de escrita de outputs\n");
			exit(EXIT_FAILURE);
		}

		/* Fechamos o sdtoutOrigin pois ja foi reposto o stdout e ja nao necessitamos deste file descriptor */
		if ((rv = close(stdoutOrigin)) == -1) {
			perror("Erro ao fechar stdoutOrigin na terminacao de um comando\n");
			exit(EXIT_FAILURE);
		}

		lock_pedidos_pendentes();
		pedidos_pendentes--;
		if( !(pedidos_pendentes > 0))
			signal_cond_simular();
		unlock_pedidos_pendentes();


	}
}

/**************************************************************************/
/*				Manipulacao de ficheiros 					   	          */
/**************************************************************************/

void escreve_no_ficheiro_log(char* logEntry) {
	int rc;
	int logEntryLenght = strlen(logEntry);
	if ((rc = write(logFileDescriptor, logEntry, logEntryLenght)) == -1) {
		perror("Erro ao escreve no ficheiro log.txt\n");
		exit(EXIT_FAILURE);
	}
}

void fecha_ficheiro_log() {
	int rc;
	if ((rc = close(logFileDescriptor)) != 0) {
		perror("Erro ao fechar o ficheiro log.txt\n");
		exit(EXIT_FAILURE);
	}
}

void cria_ficheiro_simular() {
	int rc;
	int simularFileDescriptor;
	char simularEntry[128]= "";
	sprintf(simularEntry, "i-banco-sim-%u.txt", getpid());
	if ((simularFileDescriptor = open(simularEntry, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO )) == -1) {
		perror("Erro ao criar ficheiro simular.\n");
		exit(EXIT_FAILURE);
	}
	if ((rc = dup2(simularFileDescriptor, fileno(stdout))) == -1) {
		perror("Erro ao redirecionar stdout para ficheiro de registo simular\n");
		exit(EXIT_FAILURE);
	}
}

void fecha_ficheiro_simular(int simularFileDescriptor) {
	int rc;
	if ((rc = close(simularFileDescriptor)) != 0) {
		perror("Erro ao fechar ficheiro de registro de simulacao\n");
		exit(EXIT_FAILURE);
	}
}

/**************************************************************************/
/*				Controlo de semaforos 					    	          */
/**************************************************************************/

void wait_on_read_sem(void) {
	while (sem_wait(&sem_read_ctrl) != 0) {
		if (errno == EINTR)
			continue;

		perror("Error waiting at semaphore \"sem_read_ctrl\"");
		exit(EXIT_FAILURE);
	}
}

void post_to_read_sem(void) {
	if (sem_post(&sem_read_ctrl) != 0) {
		perror("Error posting at semaphore \"sem_read_ctrl\"");
		exit(EXIT_FAILURE);
	}
}

void wait_on_write_sem(void) {
	if (sem_wait(&sem_write_ctrl) != 0) {
		perror("Error waiting at semaphore \"sem_write_ctrl\"");
		exit(EXIT_FAILURE);
	}
}

void post_to_write_sem(void) {
	if (sem_post(&sem_write_ctrl) != 0) {
		perror("Error posting at semaphore \"sem_write_ctrl\"");
		exit(EXIT_FAILURE);
	}
}

/**************************************************************************/
/*				Controlo variaveis de condicao				              */
/**************************************************************************/

void lock_pedidos_pendentes(void) {
	if ((errno = pthread_mutex_lock(&pedidos_pendentes_ctrl)) != 0) {
		perror("Error in pthread_mutex_lock()");
		exit(EXIT_FAILURE);
	}
}

void unlock_pedidos_pendentes(void) {
	if ((errno = pthread_mutex_unlock(&pedidos_pendentes_ctrl)) != 0) {
		perror("Error in pthread_mutex_unlock()");
		exit(EXIT_FAILURE);
	}
}

void wait_cond_simular() {
	if ((errno = pthread_cond_wait(&condSimular, &pedidos_pendentes_ctrl)) != 0) {
		perror("Error in pthread_cond_wait()");
		exit(EXIT_FAILURE);
	}
}

void signal_cond_simular() {
	if ((errno = pthread_cond_signal(&condSimular)) != 0) {
		perror("Error in pthread_cond_signal()");
		exit(EXIT_FAILURE);
	}
}

/**************************************************************************/
/*				Controlo buffer de comandos					              */
/**************************************************************************/

void lock_cmd_buffer(void) {
	if ((errno = pthread_mutex_lock(&buffer_ctrl)) != 0) {
		perror("Error in pthread_mutex_lock()");
		exit(EXIT_FAILURE);
	}
}

void unlock_cmd_buffer(void) {
	if ((errno = pthread_mutex_unlock(&buffer_ctrl)) != 0) {
		perror("Error in pthread_mutex_unlock()");
		exit(EXIT_FAILURE);
	}
}

/**************************************************************************/
/*				Manipulacao do buffer de comandos			              */
/**************************************************************************/

void put_cmd(comando_t *cmd) {
	cmd_buffer[buff_write_idx] = *cmd;
	buff_write_idx = (buff_write_idx+1) % CMD_BUFFER_DIM;
}

void get_cmd(comando_t *cmd) {
	*cmd = cmd_buffer[buff_read_idx];
	buff_read_idx = (buff_read_idx+1) % CMD_BUFFER_DIM;
}

/**************************************************************************/
/*				Inicializacao da aplicacao					              */
/**************************************************************************/

void inicia_log() {
	if ((logFileDescriptor = open("log.txt", O_CREAT | O_WRONLY | O_TRUNC | O_APPEND, S_IRWXU | S_IRWXG | S_IRWXO )) == -1) {
		perror("Erro ao criar ficheiro log.txt.");
		exit(EXIT_FAILURE);
	}
}

void inicia_sinais() {
	if (signal(SIGUSR1, trataSignal) == SIG_ERR) {
		perror("Erro ao definir signal.");
		exit(EXIT_FAILURE);
	}
}

void inicia_trincos() {
	int rc;
	if ((rc = pthread_mutex_init(&buffer_ctrl, NULL)) != 0) {
		perror("Could not initialize mutex \"buffer_ctrl\"");
		exit(EXIT_FAILURE);
	}

	if ((rc = pthread_mutex_init(&pedidos_pendentes_ctrl, NULL)) != 0) {
		perror("Could not initialize mutex \"pedidos_pendentes_ctrl\"");
		exit(EXIT_FAILURE);
	}
}

void inicia_variaveis_condicao() {
	if ((errno = pthread_cond_init(&condSimular, NULL)) != 0) {
		perror("Could not initialize condition \"condSimular\"");
		exit(EXIT_FAILURE);
	}
}

void inicia_semaforos() {
	if(sem_init(&sem_read_ctrl, 0, 0) != 0) {
		perror("Could not initialize semaphore \"sem_read_ctrl\"");
		exit(EXIT_FAILURE);
	}

	if(sem_init(&sem_write_ctrl, 0, CMD_BUFFER_DIM) != 0) {
		perror("Could not initialize semaphore \"sem_read_ctrl\"");
		exit(EXIT_FAILURE);
	}
}

void inicia_tarefas() {
	int i;
	for(i=0; i<NUM_TRABALHADORAS; ++i) {
		t_num[i] = i;
		if ((errno = pthread_create(&thread_id[i], NULL, thread_main, (void *)&t_num[i])) != 0) {
			perror("Error creating thread");
			exit(EXIT_FAILURE);
		}
	}
}

/**************************************************************************/
/*				Finalizacao da aplicacao					              */
/**************************************************************************/

void termina_semaforos() {
	int rc;
	if((rc = sem_destroy(&sem_read_ctrl)) != 0) {
		perror("erro ao destruir semaforo de leitura");
		exit(EXIT_FAILURE);
	}
	if((rc = sem_destroy(&sem_write_ctrl)) != 0) {
		perror("erro ao destruir semaforo de escrita");
		exit(EXIT_FAILURE);
	}
}
	
void termina_trincos() {
	int rc;
	if ((rc = pthread_mutex_destroy(&buffer_ctrl)) != 0) {
		perror("erro ao destruir trinco buffer_ctrl");
		exit(EXIT_FAILURE);
	}	

	if ((rc = pthread_mutex_destroy(&pedidos_pendentes_ctrl)) != 0) {
		perror("erro ao destruir trinco pedidos_pendentes_ctrl");
		exit(EXIT_FAILURE);
	}	
}

void termina_variaveis_condicao() {
	int rc;
	if ((rc = pthread_cond_destroy(&condSimular)) != 0) {
		perror("erro ao destruir variavel de condicao condSimular");
		exit(EXIT_FAILURE);	
	}
}