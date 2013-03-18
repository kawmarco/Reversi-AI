#include <stdint.h>
#define TABULEIRO_LARGURA 8
#define TABULEIRO_ALTURA 8
#define JOGADOR1C 'X'
#define JOGADOR2C '0'
#define VAZIOC '.'
#undef int64_t
#define int64_t uint64_t
#define MAX64 (int64_t)0XFFFFFFFFFFFFFFFF

#define max(a,b) (((a) > (b)) ? a : b)
#define min(a,b) (((a) > (b)) ? b : a)

//Codigos de saida de jogar()
#define JOGADA_INVALIDA 0
#define JOGADA_VALIDA 1
#define JOGADA_SEM_OPONENTES_ADJACENTES 2
#define JOGADA_POSICAO_JAH_PREENCHIDA 3
#define JOGADA_COM_POSICAO_INVALIDA 4

#include <assert.h>

int posf(int x, int y);
//#define pos(x,y) posf(x,y)
#define pos(x,y) ((y)*8+(x))
#define cursor(pos) ((int64_t)1<<pos)
#define preenchido(tabPtr, pos) (((tabPtr)->preenchido>>(pos))&1)

typedef struct{
    int64_t pecas; //bit 0==jogador 1; bit 1==jogador2
    int64_t preenchido; //bit 0==vazio, bit 1==preenchido
} tabuleiro;

void inicializaTabuleiro(tabuleiro* tab);
void finalizaTabuleiro(tabuleiro* tab);

char* mensagensErroJogar[5];
int jogar(tabuleiro* tab, char *jogador, int posX, int posY);
int ehFimDeJogo(tabuleiro* tab);
void imprimeTabuleiro(tabuleiro* tab);

#define oponente(jogador) ( (jogador == JOGADOR1C) ? (JOGADOR2C) : (JOGADOR1C) )

int64_t getPecas(tabuleiro* tab, char jogador);

//#define SEM_CONVERSAO_DE_COORDENADAS
#define CONVERSAO_COORDENADAS_JASON

#ifdef SEM_CONVERSAO_DE_COORDENADAS
static int xExtParaInt(int x){ return x; }
static int yExtParaInt(int y){ return y; }

static int xIntParaExt(int x){ return x; }
static int yIntParaExt(int y){ return y; }
#endif

#ifdef CONVERSAO_COORDENADAS_JASON
static int xExtParaInt(int x){ return x-1; }
static int yExtParaInt(int y){ return 8-y; }

static int xIntParaExt(int x){ return x+1; }
static int yIntParaExt(int y){ return 8-y; }
#endif

#include <stdio.h>

static void pegaPosicao(char* prompt, int* posx, int* posy){
    printf("%s", prompt);
    char buffer[30];
    buffer[0]=0;
    while(fgets(buffer, 30, stdin) && (buffer[0]=='\n'));
    while(sscanf(buffer,"%d %d",posx, posy) != 2 || xExtParaInt(*posx) < 0 || xExtParaInt(*posx) >= TABULEIRO_LARGURA || yExtParaInt(*posy) < 0 || yExtParaInt(*posy) >= TABULEIRO_ALTURA){
        printf("Valor invalido. Tente novamente.\n\n");
        printf("%s (posicao X Y separada por espaco):\n", prompt);
        while(!fgets(buffer, 6, stdin));
    }
	*posx = xExtParaInt(*posx);
	*posy = yExtParaInt(*posy) ;
}

static void printPos(int pos){
    printf("pos=%d,%d (%d)\n",pos%8,pos/8, pos);
}
