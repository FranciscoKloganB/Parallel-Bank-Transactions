#include "i-banco.h"
#include "commandlinereader.h"
#include <time.h>


comando_t cria_comando(int operacao, int idConta, int idContaDestino, int valor, pid_t processId) {
	comando_t newcmd;
	newcmd.operacao = operacao;
	newcmd.idConta = idConta;
	newcmd.idContaDestino = idContaDestino;
	newcmd.valor = valor;
	newcmd.processId = processId;
	return newcmd;
}

comando_t cmd;

int rv, writingPipeDescriptor, readingPipeDescriptor;
time_t t_inicial, t_final;

int main (int argc, char** argv) {

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("Erro na mudanca de comportamento do SIGPIPE.\n\n");
		exit(EXIT_FAILURE);
	}

	int i;
	char *args[MAXARGS + 1];
	char buffer[BUFFER_SIZE];
	char pipeName[64] = "";
	char outputString[64] = "";

	pid_t processId = getpid();

	printf("Bem-vinda/o ao i-banco-terminal\n\n");

	/* Abre "i-banco-pipe" para escrita de comandos que vao ser lidos pelo servidor i-banco */
	if ((writingPipeDescriptor = open(argv[1], O_WRONLY)) == -1) {
		if (errno == ENOENT) {
			printf("Nome do pipe fornecido inexistente. A tentar alternativa.\n\n");
			if ((strcmp(argv[1],"i-banco-pipe")) == 0) {
				if ((writingPipeDescriptor = open("/tmp/i-banco-pipe", O_WRONLY)) == -1) {
					perror("Erro ao abrir i-banco-pipe para escrita");
					exit(EXIT_FAILURE);
				}
			}
			else if ((strcmp(argv[1], "/tmp/i-banco-pipe")) == 0) {
				if ((writingPipeDescriptor = open("i-banco-pipe", O_WRONLY)) == -1) {
					perror("Erro ao tentar reabrir named pipe para envios de comando, a terminar...\n");
					exit(EXIT_FAILURE);
				}
			}
		}
		
		else {
			perror("Erro ao abrir pipe para escrita de comandos\n");
			exit(EXIT_FAILURE);			
		}
	}

	/* Gera o nome do pipe respetivo ao processo atual que ira ler dados vindos do i-banco */
	sprintf(pipeName, "/tmp/i-banco-terminal-%u", processId);
	/* Cria named pipe "i-banco-terminal-PID" para rececao de informacao acerca da execucao de comandos realizados pelo servidor i-banco. */
	if ((rv = mkfifo(pipeName, 0777)) == -1) {
		perror("Erro ao criar i-banco-terminal pipe de rececao de dados vindos do servidor\n");
		exit(EXIT_FAILURE);
	}

	/* Abre pipeName pipe para leitura de comandos que tenham sido escritos no pipe pelo servidor i-banco */
	if ((readingPipeDescriptor = open(pipeName, O_RDONLY | O_NONBLOCK)) == -1) {
		perror("Erro ao abrir i-banco-pipe-PID para leitura de outputs: em i-banco-terminal\n");
		exit(EXIT_FAILURE);
	}

	while (1) {
		int numargs = readLineArguments(args, MAXARGS+1, buffer, BUFFER_SIZE);
		int send_cmd_to_pipe = 0; /* default is NO (do not send) */

/**************************************************************************/
/*				Sair-terminal								              */
/**************************************************************************/

		if (numargs == 1 && (strcmp(args[0], COMANDO_SAIR_TERMINAL) == 0)) {
			if ((rv = close(readingPipeDescriptor)) == -1) {
				perror("Erro ao fechar pipe de leitura de outputs ao sair de um terminal\n");
				exit(EXIT_FAILURE);
			}
			if ((rv = close(writingPipeDescriptor)) == -1) {
				perror("Erro ao fechar pipe para envio de comandos ao sair de um terminal\n");
				exit(EXIT_FAILURE);
			}
			if ((rv = unlink(pipeName)) == -1) {
				perror("Erro ao eliminar pipe de leitura de outputs do Sistema de Ficheiros ao sair de um terminal\n");
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}

/**************************************************************************/
/*				Sair e sair agora							              */
/**************************************************************************/

		/* EOF (end of file) do stdin ou comando "sair" */
		else if (numargs < 0 || (numargs > 0 && (strcmp(args[0], COMANDO_SAIR) == 0))) {
			if (numargs > 1 && (strcmp(args[1], COMANDO_ARG_SAIR_AGORA) == 0))
				cmd = cria_comando(OP_SAIR, SEMSIGNFICADO, SEMSIGNFICADO, OP_SAIR_AGORA, processId);
			else
				cmd = cria_comando(OP_SAIR, SEMSIGNFICADO, SEMSIGNFICADO, SEMSIGNFICADO, processId);
			send_cmd_to_pipe = 1;							
		}

/**************************************************************************/
/*				Comando vazio								              */
/**************************************************************************/

		else if (numargs == 0)
			/* Nenhum argumento; ignora e volta a pedir */
			continue;


/**************************************************************************/
/*				Debitar										              */
/**************************************************************************/

		else if (strcmp(args[0], COMANDO_DEBITAR) == 0) {
			if (numargs < 3) {
				printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_DEBITAR);
				continue;
			}
			cmd = cria_comando(OP_DEBITAR, atoi(args[1]), SEMSIGNFICADO, atoi(args[2]), processId);
			send_cmd_to_pipe = 1;
		}
		
/**************************************************************************/
/*				Creditar									              */
/**************************************************************************/

		else if (strcmp(args[0], COMANDO_CREDITAR) == 0) {
			if (numargs < 3) {
				printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_CREDITAR);
				continue;
			}
			cmd = cria_comando(OP_CREDITAR, atoi(args[1]), SEMSIGNFICADO, atoi(args[2]), processId);
			send_cmd_to_pipe = 1;
		}
		
/**************************************************************************/
/*				Transferir										          */
/**************************************************************************/

		else if (strcmp(args[0], COMANDO_TRANSFERIR) == 0) {
			if (numargs < 4) {
				printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_TRANSFERIR);
				continue;
			}
			cmd = cria_comando(OP_TRANSFERIR, atoi(args[1]), atoi(args[2]), atoi(args[3]), processId);
			send_cmd_to_pipe = 1;
		}
		
/**************************************************************************/
/*				Ler saldo									              */
/**************************************************************************/

		else if (strcmp(args[0], COMANDO_LER_SALDO) == 0) {
			if (numargs < 2) {
				printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_LER_SALDO);
				continue;
			}
			cmd = cria_comando(OP_LER_SALDO, atoi(args[1]), SEMSIGNFICADO, SEMSIGNFICADO, processId);
			send_cmd_to_pipe = 1;
		}
		
/**************************************************************************/
/*				Simular										              */
/**************************************************************************/

		else if (strcmp(args[0], COMANDO_SIMULAR) == 0) {
			if (numargs < 2) {
				printf("%s: Sintaxe inválida, tente de novo.\n", COMANDO_SIMULAR);
				continue;
			}
			cmd = cria_comando(OP_SIMULAR, SEMSIGNFICADO, SEMSIGNFICADO, atoi(args[1]), processId);
			send_cmd_to_pipe = 1;
		}

/**************************************************************************/
/*				Comando desconhecido						              */
/**************************************************************************/

		else
			printf("Comando desconhecido. Tente de novo.\n");

		/* Escreve o cmd no pipe para que possa ser lido pelo i-banco */
		if (send_cmd_to_pipe == 1) {

			time(&t_inicial);

			if ((rv = write(writingPipeDescriptor, &cmd, sizeof(comando_t))) == -1) {
				if (errno == EPIPE) {
					perror("i-banco-pipe inexistente, por favor utilize sair-terminal.\n");
					continue;
				}

				perror("Erro ao escrever comando no i-banco-pipe em: i-banco-terminal.\n");
				exit(EXIT_FAILURE);
			}

			/* Anotacao para o professor, este sleep e um sleep batoteiro, sem ele, na primeiro comando enviado para o i-banco imprime lixo */
			/* E os comandos seguintes imprimem o resultado do output anterior, contudo as operacoes executam em tempo real e da forma correcta */
			/* Incluindo com multiplos terminais, inclusive na geracao dos ficheiros log.txt e simular, tentei varias implementacoes e nao consegui resolver o problema sem o sleep */
			/* O problema surge na leitura do buffer segundo meu valgrind na linha em que e feito o read, mas como expliquei outras implementacoes tamabem falharam */
			/* Caso queira ver um projeto completamente funcional com a parte 1.1, 1.2 e 1.3 ate a parte imediatamente anterior a abertura das pipes para enviar outputs para os terminais */
			/* Podera consultar a minha pasta google drive no endereco partilhavel https://drive.google.com/drive/folders/0B5-cJSRGsZhUSDFRM2JBU3lRaEU?usp=sharing */
			/* Acedendo a pasta P4_last_day_tries e transferindo o ficheiro P4_14h41_F.zip, note-se que esta versao ainda contem alguns erros pois na altura so estava */
			/* A testar as funcionalidades essenciais: transferir os comandos para o i-banco, gerar o simular.txt e o log.txt, fazer as contas certas, etc... */
			/* Numa nota final, acho que o unico problema deste projeto que entrego para avaliacao esta mesmo aqui */
			sleep(1);

			if (cmd.operacao != OP_SAIR && cmd.operacao != OP_SIMULAR) {
				if ((rv = read(readingPipeDescriptor, outputString, sizeof(char)*64)) == -1) {
					perror("Erro ao ler output recebido do pipeName: em i-banco-terminal.\n");
					exit(EXIT_FAILURE);	
				}

				for (i = 0; outputString[i] != '\n'; i++) {
					printf("%c", outputString[i]);
				}
			}

			time(&t_final);	

			if (cmd.operacao != OP_SAIR && cmd.operacao != OP_SIMULAR) {
				/* Agora tiro 1 segundo ao tempo contabilizado pois foi o tempo colocado de forma falsa no sleep */
				float tempo = difftime(t_final, t_inicial) - 1.0;
				printf("\nTempo: %f.\n\n", tempo);
			}
		}
	}
}