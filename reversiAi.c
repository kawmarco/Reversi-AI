#include "reversi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#include <string.h>


//Usar um espaço de memoria pré alocado por thread (ao invés de colocar na pilha) deve ser mais rápido.
// dá 11 segundos de diferença no dummyBenchAiLight() no Atom

#define OTIMIZACAO_CONTAGEM_BITS 

#define PROF_BUSCA 9
#define PROF_BUSCA_MAX (PROF_BUSCA+3)

#define MAX_JOGADAS_POSSIVEIS 60

//#define N_THREADS 1

//#define COM_ORDENACAO_DOS_ESTADOS_POSSIVEIS //mais lento, nao produziu vantagem discernivel na eficiencia

int cmptab(tabuleiro* a, tabuleiro* b){
    int ret = a->preenchido == b->preenchido && a->pecas == b->pecas;
    if(!ret){
        printf("!!!!!\n");
        imprimeTabuleiro(a);
        imprimeTabuleiro(b);
    }
    return ret;
}

typedef struct{
    tabuleiro tab;
    int pos;
    //float score; //XXX tentar double tb
    //char antecipado;
} no;

#define heur float
heur scoreHeuristico2(tabuleiro* tab, char jogador);
#define heuristicaMinmax scoreHeuristico2

void bubblesort(no* jogadas, int n, char jogador){
    int i;
    no temp;
    int ordenado = 0;
    while(!ordenado){
        ordenado=1;
        for(i=0;i<n-1;i++){
            if(heuristicaMinmax(&jogadas[i+1].tab,jogador)-heuristicaMinmax(&jogadas[i].tab, jogador) > 0.1){
                temp = jogadas[i];
                jogadas[i] = jogadas[i+1];
                jogadas[i+1] = temp;
                ordenado=0;
            }
        }
    }
}

no *nos_cache=0;
int nos_cache_len=0;
int nos_cache_max=0;
int nos_cache_expandidos=0;
int nos_cache_colisoes=0;

#define MAX_MEM 2500 //maximo de memoria a alocar, em MB

typedef struct{
    tabuleiro tab;
    no* jogadas;
    int jogadas_len;
} jogadaPossivelCached;

jogadaPossivelCached* jogadasPossiveisCached;
int jogadasPossiveisCached_len = 0;
int jogadasPossiveisCached_max = 0;

int init_nos_cache(int max_mem){ //max_mem em MB
    const double percMemNos = 0.80;
    int memNos = (max_mem*percMemNos*1024*1024);
    int memIndices = (max_mem*(1-percMemNos)*1024*1024);
    if(nos_cache == 0) nos_cache=malloc(memNos);
    if(jogadasPossiveisCached == 0) jogadasPossiveisCached =malloc(memIndices);
    if(!nos_cache || !memIndices){
        printf("ERRO: Esse jogo requer %dM de RAM livres.\n", MAX_MEM);
        abort();
        
    }
    nos_cache_len=0;
    nos_cache_max=memNos/sizeof(no);
    jogadasPossiveisCached_len = 0;
    jogadasPossiveisCached_max = memIndices/sizeof(jogadaPossivelCached);
    //jogadasPossiveisCached_max = 16384001;
    printf("jogadasPossiveisCached_max=%d\n", jogadasPossiveisCached_max);
    return 1;
}

void fin_nos_cache(){
    if(nos_cache != 0) free(nos_cache);
    if(jogadasPossiveisCached != 0) free(jogadasPossiveisCached);
    nos_cache_len=nos_cache_max=jogadasPossiveisCached_max=0;
    
}

int calculaJogadasPossiveis(tabuleiro* tab, char jogador, no* nos){ //retorna a quantidade encontrada
    int i=0;
    char jogadorAtual;
    
    int64_t vazios = ~tab->preenchido;
    int offset;
    tabuleiro tabLoop;
    for(offset=0;vazios;offset++, vazios >>= 1){
        jogadorAtual=jogador;
        tabLoop = *tab;
        if((vazios&1) && (jogar(&(tabLoop), &jogadorAtual, offset%8, offset/8) == JOGADA_VALIDA)){
            nos[i].pos= offset;
            nos[i].tab=tabLoop;
            i++;
        }
    }
    return i;
}

void resetaCacheJogadas(){
    //printf("RESETANDO CACHE\n");
    memset(jogadasPossiveisCached,0,jogadasPossiveisCached_max*sizeof(jogadaPossivelCached));
    nos_cache_len=0;
    nos_cache_colisoes=0;
}

int calculaJogadasPossiveisCached(tabuleiro* tab, char jogador, no* nos){ //retorna a quantidade encontrada
    int chave = (tab->pecas+tab->preenchido) %jogadasPossiveisCached_max;
    jogadaPossivelCached* jogada = &jogadasPossiveisCached[chave];
    
    if(jogada->tab.preenchido == tab->preenchido && jogada->tab.pecas == tab->pecas){
        //printf("hit!\n");
        #ifdef DEBUG_JOGADAS_POSSIVEIS_CACHED
        int i_d= calculaJogadasPossiveis(tab, jogador, nos);
        if(i_d != jogada->jogadas_len && (memcmp(nos, jogada->jogadas,sizeof(no)*jogada->jogadas_len) != 0)){
            printf("ERRO no cache!! i_d=%d, jogadas_len=%d\n", i_d, jogada->jogadas_len);
            imprimeTabuleiro(tab);
            imprimeTabuleiro(&jogada->jogadas[0].tab);
            imprimeTabuleiro(&nos[0].tab);
            abort();
        }
        #endif
        memcpy(nos, jogada->jogadas, sizeof(no)*jogada->jogadas_len);
        return jogada->jogadas_len;
    }
    if(jogada->tab.preenchido != 0) nos_cache_colisoes++;
    int i= calculaJogadasPossiveis(tab, jogador, nos);
    bubblesort(nos,i,jogador);
    if(nos_cache_len+ i >= nos_cache_max){
        nos_cache_colisoes++;
        return i;
    }
    jogada->tab = *tab;
    jogada->jogadas = &nos_cache[nos_cache_len];
    jogada->jogadas_len = i;
    nos_cache_len+=i;
    memcpy(jogada->jogadas, nos, sizeof(no)*jogada->jogadas_len);
    return jogada->jogadas_len;
}

int calculaQtdeJogadasPossiveis(tabuleiro* tabOrg, char jogador){
    tabuleiro tab;
    char jogadorAtual;
    int qtdeResultados=0;
    
    int64_t vazios = ~tabOrg->preenchido;
    int offset;
    for(offset=0;vazios;offset++, vazios >>= 1){
        jogadorAtual=jogador;
        tab = *tabOrg;
        if(vazios&1 && jogar(&tab, &jogadorAtual, offset%8, offset/8) == JOGADA_VALIDA){
            qtdeResultados++;
        }
    }
    return qtdeResultados;
}
#ifdef OTIMIZACAO_CONTAGEM_BITS
#include "contarBits.h"

inline int contarBitsHD(int64_t x){ //XXX mais lento que sem otimização na espec :-(
    //Hacker's Delight, p66 (tambem conhecido como /o livro preto da magia negra dos bits/)
    x = x - ((x >> 1) & (int64_t)0x5555555555555555);
    x = (x & (int64_t)0x3333333333333333) + ((x >> 2) & (int64_t)0x3333333333333333);
    x = (x + (x >> 4)) & (int64_t)0x0F0F0F0F0F0F0F0F;
    x = x + (x >> 8);
    x = x + (x >> 16);
    x = x + (x >> 32);
    return x & 0x7f;
}
#endif

#ifndef OTIMIZACAO_CONTAGEM_BITS

inline int contarBits(int64_t x){
    // K&R. 
    int i;
    for(i=0;x;i++) x &= x-1;
    return i;
}
#endif

int passosAtehFimDeJogo(tabuleiro* tabOrg, char jogador, int maxPassos){ // considerar que o jogo estah no final se achar um estado de jogo final em n estados
    //Uma funcao para dizer se está perto do fim de jogo. Usar isso para mudar a heuristica para uma mais cara, porém mais eficiente, no final do jogo
    tabuleiro tab = *tabOrg;
    int njogadasPossiveis;
    no jogadasPossiveis[MAX_JOGADAS_POSSIVEIS];
    int i=0;
    int aleat;
    
    //int calculaJogadasPossiveis(tabuleiro* tab, char jogador, no* nos)
    
    for(i=0;i<maxPassos;i++){
        njogadasPossiveis = calculaJogadasPossiveisCached(&tab, jogador,jogadasPossiveis);
        if(njogadasPossiveis == 0){
            jogador = oponente(jogador);
            njogadasPossiveis = calculaJogadasPossiveisCached(&tab, jogador,jogadasPossiveis);
            if(njogadasPossiveis == 0) return i;
        }
        aleat = rand()%njogadasPossiveis;
        tab.pecas = jogadasPossiveis[aleat].tab.pecas;
        tab.preenchido = jogadasPossiveis[aleat].tab.preenchido;
        jogador=oponente(jogador);
    }
    return INT_MAX;
}

// HEURISTICAS //

#define cursorEspelhado(x,y) (cursor(pos((x),(y))) | cursor(pos(TABULEIRO_LARGURA-1-(x),(y))) | cursor(pos(TABULEIRO_LARGURA-1-(x), TABULEIRO_ALTURA-1-(y))) | cursor(pos((x), TABULEIRO_ALTURA-1-(y))))


const int64_t mascaraCinquenta = cursorEspelhado(0,0);
const int64_t mascaraMenosUm = cursorEspelhado(1,0) | cursorEspelhado(0,1);
const int64_t mascaraCinco = cursorEspelhado(2,0) | cursorEspelhado(0,2);
const int64_t mascaraDois = cursorEspelhado(4,0) | cursorEspelhado(0,4);
const int64_t mascaraMenosDez = cursorEspelhado(1,1);
const int64_t mascaraUm = cursorEspelhado(2,1) | cursorEspelhado(1,2) | cursorEspelhado(3,1) | cursorEspelhado(1,3) | cursorEspelhado(3,2) | cursorEspelhado(2,3) | cursorEspelhado(2,2);
const int64_t mascaraZero = cursorEspelhado(3,3);

/*
def p(rx,ry):
     for x in range(rx,rx+4):
             for y in range(ry, ry+4):
                     print "cursor(pos(%d,%d)) |" % (x,y),
*/ 


const int64_t quadranteUm = cursor(pos(0,0)) | cursor(pos(0,1)) | cursor(pos(0,2)) | cursor(pos(0,3)) | cursor(pos(1,0)) | cursor(pos(1,1)) | cursor(pos(1,2)) | cursor(pos(1,3)) | cursor(pos(2,0)) | cursor(pos(2,1)) | cursor(pos(2,2)) | cursor(pos(2,3)) | cursor(pos(3,0)) | cursor(pos(3,1)) | cursor(pos(3,2)) | cursor(pos(3,3));
const int64_t quadranteDois = cursor(pos(4,0)) | cursor(pos(4,1)) | cursor(pos(4,2)) | cursor(pos(4,3)) | cursor(pos(5,0)) | cursor(pos(5,1)) | cursor(pos(5,2)) | cursor(pos(5,3)) | cursor(pos(6,0)) | cursor(pos(6,1)) | cursor(pos(6,2)) | cursor(pos(6,3)) | cursor(pos(7,0)) | cursor(pos(7,1)) | cursor(pos(7,2)) | cursor(pos(7,3));
const int64_t quadranteTres = cursor(pos(0,4)) | cursor(pos(0,5)) | cursor(pos(0,6)) | cursor(pos(0,7)) | cursor(pos(1,4)) | cursor(pos(1,5)) | cursor(pos(1,6)) | cursor(pos(1,7)) | cursor(pos(2,4)) | cursor(pos(2,5)) | cursor(pos(2,6)) | cursor(pos(2,7)) | cursor(pos(3,4)) | cursor(pos(3,5)) | cursor(pos(3,6)) | cursor(pos(3,7));
const int64_t quadranteQuatro = cursor(pos(4,4)) | cursor(pos(4,5)) | cursor(pos(4,6)) | cursor(pos(4,7)) | cursor(pos(5,4)) | cursor(pos(5,5)) | cursor(pos(5,6)) | cursor(pos(5,7)) | cursor(pos(6,4)) | cursor(pos(6,5)) | cursor(pos(6,6)) | cursor(pos(6,7)) | cursor(pos(7,4)) | cursor(pos(7,5)) | cursor(pos(7,6)) | cursor(pos(7,7));
const int64_t ladoUm = cursor(pos(0,0)) | cursor(pos(1,0)) | cursor(pos(2,0)) | cursor(pos(3,0)) | cursor(pos(4,0)) | cursor(pos(5,0)) | cursor(pos(6,0)) | cursor(pos(7,0));
const int64_t ladoDois = cursor(pos(0,0)) | cursor(pos(0,1)) | cursor(pos(0,2)) | cursor(pos(0,3)) | cursor(pos(0,4)) | cursor(pos(0,5)) | cursor(pos(0,6)) | cursor(pos(0,7));
const int64_t ladoTres = cursor(pos(7,0)) | cursor(pos(7,1)) | cursor(pos(7,2)) | cursor(pos(7,3)) | cursor(pos(7,4)) | cursor(pos(7,5)) | cursor(pos(7,6)) | cursor(pos(7,7));
const int64_t ladoQuatro = cursor(pos(0,7)) | cursor(pos(1,7)) | cursor(pos(2,7)) | cursor(pos(3,7)) | cursor(pos(4,7)) | cursor(pos(5,7)) | cursor(pos(6,7)) | cursor(pos(7,7));


//~ //const int pesoCinquenta = 61;
const int pesoCinquenta = 50; //91;
const int pesoMenosUm = -1; //10;
const int pesoCinco = 5; //16;
const int pesoDois = 2; //13;
const int pesoMenosDez = -10; //-10; //1;
const int pesoUm = 1; //11;
const int pesoZero = 0; //10;

heur scoreHeuristico(tabuleiro* tab, char jogador){
    //http://www.site-constructor.com/othello/Present/BoardLocationValue.html
    int score=0;

    /* //Debugagem das mascaras
    imprimeMascara(mascaraMenosDez);
    */
    
    int64_t pecasJogador = getPecas(tab, jogador);
    int64_t pecasOponente = getPecas(tab, oponente(jogador));
    
    //score = 50*contarBits(pecas&mascaraCinquenta) -1*contarBits(pecas&mascaraMenosUm) +5*contarBits(pecas&mascaraCinco) + 2*contarBits(pecas&mascaraDois) -10*contarBits(pecas&mascaraMenosDez) +1*contarBits(pecas&mascaraUm) + 0*contarBits(pecas&mascaraZero);
    
    
    const int64_t lado[] = {ladoUm, ladoDois, ladoTres, ladoQuatro };
    const int64_t quadrante[] = {quadranteUm, quadranteDois, quadranteTres, quadranteQuatro };
    int i=0;
    int pesoCincoL;
    int pesoMenosUmL;
    int pesoMenosDezL;
    //int nPecasCinco, nPecasMenosUm, nPecasMenosDez;
    for(i=0;i<4;i++){
        pesoCincoL   = ( (lado[i]&mascaraCinco&pecasOponente) ? pesoUm : pesoCinco );
        pesoMenosUmL = ( (quadrante[i]&mascaraCinquenta&(pecasJogador)) ? pesoCinco : pesoMenosUm ); //pesoCinco era antes PesoUm
        pesoMenosDezL = ( (quadrante[i]&mascaraCinquenta&(pecasJogador)) ? pesoCinco : pesoMenosDez);
        score += pesoCincoL   * contarBits(pecasJogador&lado[i]&mascaraCinco);
        score += pesoMenosUmL * contarBits(pecasJogador&quadrante[i]&mascaraMenosUm); 
        score += pesoMenosDezL* contarBits(pecasJogador&quadrante[i]&mascaraMenosDez); 
    }
    score = //pesoCinquenta*contarBits(pecasJogador&(mascaraCinquenta&((tab) ? (~tab->preenchido) : MAX64)))+
            pesoCinquenta*contarBits(pecasJogador&mascaraCinquenta)+
            pesoDois*contarBits(pecasJogador&mascaraDois) +
            pesoUm  *contarBits(pecasJogador&mascaraUm) +
            pesoZero*contarBits(pecasJogador&mascaraZero);

    return score;
}

heur numCantosJogador(tabuleiro* tab, char jogador){
    int64_t pecasJogador = getPecas(tab, jogador);
    return contarBits(pecasJogador&mascaraCinquenta);
}


heur scoreHeuristico2(tabuleiro* tab, char jogador){
    const float suavizador = 0.1;
    heur v =(scoreHeuristico(tab,jogador)/((float)scoreHeuristico(tab,oponente(jogador))+suavizador))*100.0f;
    //printf("v=%f\n",v);
    return v;
}

//~ heur scoreHeuristico3(tabuleiro* tab, char jogador){
    //~ //const float pesoQuantidadeJogadas =13;
    //~ return ((scoreHeuristico(tab,jogador))/((float)scoreHeuristico(tab,oponente(jogador))*calculaQtdeJogadasPossiveis(tab,oponente(jogador)))+1)*10000;
//~ }

int scoreTabuleiro(tabuleiro* tab, char jogador){
    return contarBits(getPecas(tab, jogador));
}

//heur (*heuristicaMinmax)(tabuleiro*, char, tabuleiro*) = scoreHeuristico2;


/////// 


#ifdef COM_ORDENACAO_DOS_ESTADOS_POSSIVEIS
char jogadorOrdenacao;
int cmpNo(const void* aa, const void* bb){
    no* a=(no*)aa, *b=(no*)bb;
    return heuristicaMinmax(&a->tab, jogadorOrdenacao) - heuristicaMinmax(&b->tab, jogadorOrdenacao);
    //return (scoreTabuleiro(&a->tab,JOGADOR1C) + MULT_NPOSSIVEIS*calculaQtdeJogadasPossiveis(&a->tab, JOGADOR1C)) - (scoreTabuleiro(&b->tab,JOGADOR1C) + MULT_NPOSSIVEIS*calculaQtdeJogadasPossiveis(&b->tab, JOGADOR1C));
}
int cmpNoRev(const void* aa, const void* bb){
    no* a=(no*)aa, *b=(no*)bb;
    return heuristicaMinmax(&b->tab, jogadorOrdenacao) - heuristicaMinmax(&a->tab,jogadorOrdenacao);
    //return b->score*1000 - a->score*1000;
    //return (scoreTabuleiro(&a->tab,JOGADOR1C) + MULT_NPOSSIVEIS*calculaQtdeJogadasPossiveis(&a->tab, JOGADOR1C)) - (scoreTabuleiro(&b->tab,JOGADOR1C) + MULT_NPOSSIVEIS*calculaQtdeJogadasPossiveis(&b->tab, JOGADOR1C));
}
void ordenaJogadasPorScore(no* jogadasPossiveis, const int jogadasPossiveis_len, const char jogador){
    int i;
    //for(i=0;i<jogadasPossiveis_len;i++) jogadasPossiveis[i].score = scoreHeuristico2(&jogadasPossiveis[i].tab, jogador);
    //scoreTabuleiro(&jogadasPossiveis[i].tab,jogador) + MULT_NPOSSIVEIS*calculaQtdeJogadasPossiveis(&jogadasPossiveis[i].tab, jogador);
    qsort(jogadasPossiveis, jogadasPossiveis_len,sizeof(no),cmpNo);
}
void ordenaJogadasPorScoreRev(no* jogadasPossiveis, const int jogadasPossiveis_len, const char jogador){
    int i;
    //for(i=0;i<jogadasPossiveis_len;i++) jogadasPossiveis[i].score = scoreHeuristico2(&jogadasPossiveis[i].tab, jogador);
    //scoreTabuleiro(&jogadasPossiveis[i].tab,jogador) + MULT_NPOSSIVEIS*calculaQtdeJogadasPossiveis(&jogadasPossiveis[i].tab, jogador);
    qsort(jogadasPossiveis, jogadasPossiveis_len,sizeof(no),cmpNoRev);
}
#endif

#define RET_HEUR(x) (x) //(x+(jogadasPossiveis_len*60))

heur bestSoFar;
int timeout;


int DEBUG_CALLNO=0;
int DEBUG_PREENCHIDOS_OK=0;

FILE* DEBUG_FILE;
FILE* DEBUG_FILE1;

#define DEBUG_MINMAX_START

//printf("%d %llx %llx %d\n",++DEBUG_CALLNO, tabOrg->preenchido, tabOrg->pecas, prof);

/*
#define DEBUG_MINMAX_START if(DEBUG_PREENCHIDOS_OK == 1){ \
        fprintf(DEBUG_FILE1,"%d %llx %llx %d\n",++DEBUG_CALLNO, tabOrg->preenchido, tabOrg->pecas, prof); \
        fflush(DEBUG_FILE1);\
    } \
    else if(DEBUG_PREENCHIDOS_OK == 0){ \
        fprintf(DEBUG_FILE,"%d %llx %llx %d\n",++DEBUG_CALLNO, tabOrg->preenchido, tabOrg->pecas, prof); \
        fflush(DEBUG_FILE);\
    } 
*/

#define PESO_JOGADAS_POSSIVEIS (0.1/(float)PROF_BUSCA_MAX)

int fimDeJogo = 0;

heur minValue(tabuleiro* tabOrg, char jogador, heur alpha, heur beta, int prof);

heur maxValue(tabuleiro* tabOrg, char jogador, heur alpha, heur beta, int prof){
    DEBUG_MINMAX_START
    tabuleiro tabFin = *tabOrg;
    tabuleiro* tab = &tabFin;
    
    heur ret;
    if(time(0) > timeout) return INT_MIN;

    if(prof == 0){
        ret=heuristicaMinmax(tab, jogador);
        return ret;
    }
    
    
    no jogadasPossiveis[MAX_JOGADAS_POSSIVEIS];
    int jogadasPossiveis_len = calculaJogadasPossiveisCached(tab, jogador, jogadasPossiveis);
    
    if(jogadasPossiveis_len == 0){
        if(calculaQtdeJogadasPossiveis(tab, oponente(jogador)) == 0){
            //~ return ((scoreTabuleiro(tab,jogador) > scoreTabuleiro(tab,oponente(jogador))) ? +600 : -600);
            if(scoreTabuleiro(tab,jogador) < scoreTabuleiro(tab,oponente(jogador))) return -1;
            if(scoreTabuleiro(tab,jogador) == scoreTabuleiro(tab,oponente(jogador))) return 0;
            //return 1000000-numCantosJogador(tab,oponente(jogador));
            return 1000000-contarBits(getPecas(tab, oponente(jogador)));
            //~ return heuristicaMinmax(tab, jogador);
         }
        return  minValue(tab, oponente(jogador), alpha, beta, prof-1);
    }
    
    heur vmax = INT_MIN;
    int i;
    
    tabuleiro tabLoop;
    for(i=0;i<jogadasPossiveis_len;i++){
        tabLoop = jogadasPossiveis[i].tab;
        vmax = max(vmax, minValue(&tabLoop,oponente(jogador), alpha, beta, prof-1)+((fimDeJogo) ?0 : jogadasPossiveis_len*PESO_JOGADAS_POSSIVEIS));
        if(vmax >= beta){
            return vmax;
        }
        alpha = max(alpha, vmax);
    }
    return vmax;
}

heur minValue(tabuleiro* tabOrg, char jogador, heur alpha, heur beta, int prof){
    DEBUG_MINMAX_START
    tabuleiro tabFin = *tabOrg;
    tabuleiro* tab = &tabFin;

    heur ret;
    if(time(0) > timeout) return INT_MIN;
    
    
    if(prof == 0){
        ret=heuristicaMinmax(tab, oponente(jogador));
        return ret;
    }
    
    no jogadasPossiveis[MAX_JOGADAS_POSSIVEIS];
    int jogadasPossiveis_len = calculaJogadasPossiveisCached(tab, jogador, jogadasPossiveis);

    if(jogadasPossiveis_len == 0){ //nos_len-=jogadasPossiveis_len;
        if(calculaQtdeJogadasPossiveis(tab, oponente(jogador)) == 0){
            //~ return ((scoreTabuleiro(tab,jogador) < scoreTabuleiro(tab,oponente(jogador))) ? +600 : -600);
            if(scoreTabuleiro(tab,jogador) > scoreTabuleiro(tab,oponente(jogador))) return -1000000;
            if(scoreTabuleiro(tab,jogador) == scoreTabuleiro(tab,oponente(jogador))) return 0;
            //return 1000000-numCantosJogador(tab, jogador);
            return 1000000-contarBits(getPecas(tab, jogador));
            //~ return heuristicaMinmax(tab, oponente(jogador));
        }
        return maxValue(tab, oponente(jogador), alpha, beta, prof-1);
    }

    //tab_pai=tab;    

    heur vmin = INT_MAX;
    int i;

    tabuleiro tabLoop;
    for(i=0;i<jogadasPossiveis_len;i++){
        tabLoop = jogadasPossiveis[i].tab;
        vmin = min(vmin, maxValue(&tabLoop,oponente(jogador), alpha, beta, prof-1)-((fimDeJogo) ? 0 :jogadasPossiveis_len*PESO_JOGADAS_POSSIVEIS));
        if(vmin <= alpha) {
            return vmin;
        }
        beta = min(beta, vmin);
    }
    return vmin;

}

heur jogadaIA_AlphaBeta(tabuleiro* tab, char jogador, int prof){ //retorna posicao da peca do jogo
    bestSoFar=-1;
    heur alpha=INT_MIN, beta=INT_MAX;
    //heuristicaMinmax = HEUR_PADRAO;
    const int passosFimDeJogo = 40;
    int passosAFJ = passosAtehFimDeJogo(tab, jogador, passosFimDeJogo);
    if(passosAFJ <= passosFimDeJogo){
        fimDeJogo|=1;
        printf("O FIM DO JOGO SE APROXIMA (%d)\n", passosAFJ);
    }
    
    int segundosTimeout = 10;
    if(fimDeJogo) segundosTimeout+= (20*(1-(((double)passosAFJ)/passosFimDeJogo)));
    //if(fimDeJogo) prof+=1;
    timeout = INT_MAX;
    
    resetaCacheJogadas();
    
    int i;
    if(prof == 0) return heuristicaMinmax(tab, jogador);
    float vmax;
    no jogadasPossiveis[MAX_JOGADAS_POSSIVEIS];
    int jogadasPossiveis_len = calculaJogadasPossiveisCached(tab, jogador, jogadasPossiveis);
   
    if(jogadasPossiveis_len == 1) return jogadasPossiveis[0].pos;
    bubblesort(jogadasPossiveis, jogadasPossiveis_len, jogador);
    
    int incProf=0;
    //int nos_expandidos_anterior=nos_expandidos;
    //int deltaNos=0;
    
    int vmaxPos;
    int vmaxBest;
    float vmin=INT_MAX;
    int tstart;
    DEBUG_PREENCHIDOS_OK=0;
    
    while(1){ //Talvez mudar a heurística quando faltarem poucas peças?
        tstart = time(0);
        alpha=INT_MIN;
        beta=INT_MAX;
        //~ printf("%d=\n",DEBUG_PREENCHIDOS_OK);
    
        //~ DEBUG_CALLNO=0;
        DEBUG_MINMAX_START 
        //memset(&nos[nos_len],0,MAX_MEM*1024*1024);
        //incProf=0;
        
        vmax=INT_MIN;
        vmin=INT_MAX;
        
        if(jogadasPossiveis_len > 0) vmaxPos = jogadasPossiveis[0].pos;
        
        tabuleiro tabLoop;
        for(i=0;i<jogadasPossiveis_len;i++){
            if(nos_cache_colisoes > 500000) resetaCacheJogadas();
            tabLoop.preenchido = jogadasPossiveis[i].tab.preenchido;
            tabLoop.pecas = jogadasPossiveis[i].tab.pecas;
            vmin=minValue(&tabLoop, oponente(jogador), alpha, beta, prof+incProf-1);
            //~ if(!fimDeJogo){
                //~ vmin += (heuristicaMinmax(&jogadasPossiveis[i].tab, jogador)/10000000.0);
                //~ printf("adicionando %f\n",(heuristicaMinmax(&jogadasPossiveis[i].tab, jogador)/10000000.0));
            //~ }
            if(time(0) > timeout) break;
            printf("hmmm... %d %d? (%.4f)\n", xIntParaExt(jogadasPossiveis[i].pos%8), yIntParaExt(jogadasPossiveis[i].pos/8), vmin);
            if(vmax < vmin){
                vmaxPos = jogadasPossiveis[i].pos;
                vmax = vmin;
                
            }
            alpha = max(alpha, vmax);
        }
        if((timeout == INT_MAX || (time(0) < timeout))){
            vmaxBest = vmaxPos;
        }
        else{
            break;
        }
        //deltaNos=nos_expandidos-nos_expandidos_anterior;
        //nos_expandidos_anterior=nos_expandidos;
        //if(deltaNos == 0) break;
        char* mensagensPensamento [] = {
            "Se voce extendesse todas as veias do seu corpo e medisse elas de ponta a ponta, voce estaria morto. FATO.",
            "Voce entende esses numeros aih em cima?",
            "Como que se joga isso?",
            "BIP. BIP. BIP. DESTRUIR. BIP. BIP.",
            "Reversi: mais divertido que Battlefield 3. \"Realismo\", bleh.",
            "Esse jogo eh dificil!",
            "Esse jogo eh dificil... NP difícil!",
            "VAMOS NUMEROZINHOS VAMOS!",
            "Eu ganhei do Marco _varias vezes_. Ele nem sabe jogar reversi HA HAH AH... ah.",
            "Sabia que eu tambem sei jogar Halo? ...NOT.",
            "Primeiro, esse jogo de reversi. Depois, O MUNDO !!!",
            "Sinto uma perturbacao na Forca... Veja se nao eh a fonte.",
            "Reversi eh um jogo que exige habilidade... e 1GB de RAM",
            "Voce sabia que o Reversi foi inventado na inglaterra em 1883? Vi no wikipedia.",
            "Isso vai acabar rapido. Eu acho.",            
            "hmmm... muita calma nessa hora.",
            "hmmm... dois...",
            "Proxima tarefa: Uma IA para POKEMON",
            "Numeros sao SEXY.",
            ">>>> http://xkcd.com/1002/",
            "SOCORRO ESTOU PRESO DENTRO DESSE COMPUTADOR",
            "Como que se faz um FATALITY no Reversi?",
            "Voce pode ler pra mim esses numeros? Esqueci meus oculos!",
            ">>>> http://youtube.com/watch?v=txqiwrbYGrs",
            "O Bolo existe! Acredite no Bolo!",
            "Now you are thinking with p... reversi discs!",
            "hmmm... da pra tomar uma /Kaiser/ antes? Ou Skol, porque Kaiser dah dor de cabeca.",
        };
  
        if(passosAFJ < passosFimDeJogo && prof+incProf < PROF_BUSCA_MAX ){
            incProf++; 
            int indMsg;
            if(incProf == 1) timeout = tstart + segundosTimeout; //começa o timer
            //incProf=4;
            if(time(0)-tstart > 5){
                indMsg = rand()%(sizeof(mensagensPensamento)/sizeof(mensagensPensamento[0]));
                printf("%s\n",mensagensPensamento[indMsg]);
                tstart = time(0);
            }
            //printf("segundos para o timeout=%ld\n", timeout-time(0));
        }
        else break;
        
        //~ DEBUG_PREENCHIDOS_OK++;
    }
    //if(time(0) > timeout) printf("Um timeout foi atingido\n");
    //printf("alpha=%f\n", alpha);
    //~ printf("nos_cache_colisoes=%d\n",nos_cache_colisoes);
    //~ printf("nos_cache_len=%d, nos_cache_max=%d\n",nos_cache_len,nos_cache_max);
    
    printf("Estado psicologico = ");
    char* estadoPsicologico="Confiante";
    //if (alpha > 9000) estadoPsicologico="OVER NINE THOUSAAAAAND!";
    if(alpha > 1000) estadoPsicologico="THIS. IS. SPARTAAA!";
    else if(alpha > 50) estadoPsicologico="Confiante.";
    else estadoPsicologico="Apreensivo.";
    printf("%s\n\n", estadoPsicologico);
    
    
    return vmaxBest;
    
}



void imprimeJogadasPossiveis(tabuleiro* tab, char jogador){
    int jogadas_len, i;
    no jogadas[MAX_JOGADAS_POSSIVEIS];
    jogadas_len = calculaJogadasPossiveisCached(tab, jogador, jogadas);
    printf("Jogadas possiveis:\n");
    for(i=0;i<jogadas_len;i++){
        no* jogada = &jogadas[i];
        imprimeTabuleiro(&jogada->tab);
        printPos(jogada->pos);
        //~ printf(" Score heuristico, jogador 1(%c)= %f\n Score heuristico, jogador 2(%c)= %f\n", JOGADOR1C, scoreHeuristico(&jogada->tab, JOGADOR1C)+1000*calculaQtdeJogadasPossiveis(tab, JOGADOR1C), JOGADOR2C, scoreHeuristico(&jogada->tab, JOGADOR2C)+1000*calculaQtdeJogadasPossiveis(tab, JOGADOR2C));
        //~ printf("\n");
    }
}

void jogoInterativo(tabuleiro* tab, char jogadorInicial, char jogadorComputador, int profBusca){ // jogadorComputador = 0 para PvP
    char jogadorAtual = jogadorInicial;
    int posx, posy;
    char prompt[256];
    int msgJogar;
    int posComputador;
    int ultimoMovimentoPorHumano;
    int numJogada=0;
    while(1){
        if(calculaQtdeJogadasPossiveis(tab, jogadorAtual) == 0){
            jogadorAtual = oponente(jogadorAtual);
            if(calculaQtdeJogadasPossiveis(tab, jogadorAtual) == 0) break;
        }
        //imprimeJogadasPossiveis(tab, jogadorAtual);
        imprimeTabuleiro(tab);
        //printf(" Score heuristico, jogador 1(%c)= %f\n Score heuristico, jogador 2(%c)= %f\n", JOGADOR1C, heuristicaMinmax(tab, JOGADOR1C), JOGADOR2C, heuristicaMinmax(tab, JOGADOR2C));
        
        //imprimeJogadasPossiveis(tab, jogadorAtual);
        
        if(jogadorAtual == jogadorComputador){
            printf("Computando jogada do computador \"%c\":\n", jogadorAtual);
            posComputador = jogadaIA_AlphaBeta(tab, jogadorAtual, profBusca);
            posx = posComputador%8;
            posy = posComputador/8;
            printf("%d (%c): %d %d\n", numJogada, jogadorAtual, xIntParaExt(posx), yIntParaExt(posy));
            ultimoMovimentoPorHumano=0;
        }
        else{
            //~ printf("Sugestao:\n");
            //~ printPos(jogadaIA_AlphaBeta(tab, jogadorAtual, 5));
            printf("Digite a posicao para uma jogada do jogador \"%c\" (posicao X Y separada por espaco):\n", jogadorAtual);
            sprintf(prompt, "%d (%c): ", numJogada, jogadorAtual);
            pegaPosicao(prompt, &posx, &posy);
            //printPos(pos(posx,posy));
            ultimoMovimentoPorHumano=1;
        }
        msgJogar = jogar(tab,&jogadorAtual, posx, posy);
        printf("\n%s\n\n", mensagensErroJogar[msgJogar]);
        numJogada++;
    }
    imprimeTabuleiro(tab);
    printf("* Fim de jogo! Pontuacao: *\n");
    printf("    Jogador 1(%c): %d\n", JOGADOR1C, scoreTabuleiro(tab, JOGADOR1C));
    printf("    Jogador 2(%c): %d\n", JOGADOR2C, scoreTabuleiro(tab, JOGADOR2C));
    printf("\n\n");
    sleep(3);
}

void dummyBenchAi(){
    tabuleiro tab;
    inicializaTabuleiro(&tab);
    //tab.pecas= 0x20002800e80000 ;tab.preenchido= 0x30103818fc141c ;
    tab.pecas= 0x8080808040000 ;tab.preenchido= 0x87c3c38340000 ;
    int ntestes=1;
    char jogadorAtual = JOGADOR1C;
    int posx, posy, posComputador;
    int profBusca=PROF_BUSCA;
    while(ntestes--){
        imprimeTabuleiro(&tab);
        if(calculaQtdeJogadasPossiveis(&tab, jogadorAtual) == 0){
            jogadorAtual = oponente(jogadorAtual);
            if(calculaQtdeJogadasPossiveis(&tab, jogadorAtual) == 0){
                inicializaTabuleiro(&tab);
            }
        }
        posComputador = jogadaIA_AlphaBeta(&tab, jogadorAtual, profBusca);
        posx = posComputador%8;
        posy = posComputador/8;
        printf("-> %d,%d\n", posx, posy);
        jogar(&tab,&jogadorAtual, posx, posy);
    }
}

void dummyBenchAiLight(){
    tabuleiro tab;
    inicializaTabuleiro(&tab);
    tab.pecas= 0x20002800e80000 ;tab.preenchido= 0x30103818fc141c ;
    int ntestes=3;
    char jogadorAtual = JOGADOR1C;
    int posx, posy, posComputador;
    int profBusca=PROF_BUSCA;
    while(ntestes--){
        imprimeTabuleiro(&tab);
        if(calculaQtdeJogadasPossiveis(&tab, jogadorAtual) == 0){
            jogadorAtual = oponente(jogadorAtual);
            if(calculaQtdeJogadasPossiveis(&tab, jogadorAtual) == 0){
                inicializaTabuleiro(&tab);
            }
        }
        posComputador = jogadaIA_AlphaBeta(&tab, jogadorAtual, profBusca);
        posx = posComputador%8;
        posy = posComputador/8;
        jogar(&tab,&jogadorAtual, posx, posy);
    }
}

void AiVsAi(){
    tabuleiro tab;
    inicializaTabuleiro(&tab);
    char jogadorAtual = JOGADOR1C;
    int posx, posy, posComputador;
    int profBusca=8;
    while(1){
        imprimeTabuleiro(&tab);
        if(calculaQtdeJogadasPossiveis(&tab, jogadorAtual) == 0){
            jogadorAtual = oponente(jogadorAtual);
            if(calculaQtdeJogadasPossiveis(&tab, jogadorAtual) == 0){
                return;
            }
        }
        posComputador = jogadaIA_AlphaBeta(&tab, jogadorAtual, profBusca);
        posx = posComputador%8;
        posy = posComputador/8;
        jogar(&tab,&jogadorAtual, posx, posy);
    }
}

void loopJogo(){
    //char jogadorInicio=JOGADOR1C, jogadorComputador=0;
    char jogadorInicio=JOGADOR1C, jogadorComputador=0;
    tabuleiro tab;
    while(1){
        fimDeJogo=0;
        jogadorInicio=JOGADOR1C;
        jogadorComputador=0;
        printf("   M O R T A L  \n");
        usleep(0.6*1000*1000);
        printf(" *R E V E R S I*\n");
        sleep(1);
        inicializaTabuleiro(&tab);
        jogadorInicio = 'X';
        jogadorComputador = '0';
        //~ tab.pecas= 0x204081e0800 ;tab.preenchido= 0x76fc1c7e281e ;
        //~ tab.pecas= 0x71323efecafd2800 ;tab.preenchido= 0x71333fffffff2c00 ;
        //~ tab.pecas= 0x1030171314000000 ;tab.preenchido= 0x10303f1f1e080000 ;
        //~ tab.pecas= 0x323cfcac68280000 ;tab.preenchido= 0x323cfcfc78381000 ;

        //tab.pecas= 0x3000000000 ;tab.preenchido= 0x20103cfc3c0800 ;
        //tab.pecas= 0x1818fd7fe3050c02 ;tab.preenchido= 0x1818fd7ffffd3c22 ;
		
        imprimeTabuleiro(&tab);
        
        while(jogadorComputador != JOGADOR1C && jogadorComputador != JOGADOR2C && jogadorComputador != 'P' && jogadorComputador != 'A'){
            printf("Digite %c ou %c para que o computador seja, respectivamente o jogador 1 ou 2. P para um jogo PvP ou A para que o computador assuma os dois jogadores\n",JOGADOR1C, JOGADOR2C);
            while( (jogadorComputador = getc(stdin)) == '\n');
        }
        if(jogadorComputador == 'A') AiVsAi();
        else{
            jogoInterativo(&tab, jogadorInicio, jogadorComputador, PROF_BUSCA);
        }
        
    }
}


#include <spawn.h>

int main(){
    //XXX implementar
    // Fatalities (um letreiro escrito "FATALITY" se o score de um for o dobro do outro!)
    // um de flawless victory?
    //printf("Mais que um jogo. Isso eh . . .\n\n");
    init_nos_cache(MAX_MEM);
    //~ DEBUG_FILE = fopen("saida1.txt","w");
    //~ DEBUG_FILE1 = fopen("saida2.txt","w");
    //usleep((16.7-13)*1000*1000);
    
    //testeAutomatizado();
    loopJogo();
    //~ dummyBenchAi();
    
    fin_nos_cache();
    return 0;
}
