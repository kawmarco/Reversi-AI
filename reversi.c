#include "reversi.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

//#define DESABILITAR_PREENCHIMENTO_D1
//#define DESABILITAR_PREENCHIMENTO_D2
//#define DESABILITAR_PREENCHIMENTO_X
//#define DESABILITAR_PREENCHIMENTO_Y

#define OTIMIZACAO_DETECAO_DE_MOVIMENTO 

//#define OTIMIZACOES_ESTUPIDAS //aumentam um pouco o desempenho com a chance de quebrar algo //NAO RECOMENDAVEL!!!!

#define jogadorDeChar(jogador) ((jogador == JOGADOR1C) ? (0) : (1)) 

int posf(int x, int y){
    int ret = ((y)*8+(x));
    if(! (ret >= 0 && ret <= ((7)*8+(7)))){
        printf("posicao invalida ");
        printPos(ret);
        //assert(ret >= 0 && ret <= ((7)*8+(7)));
    }
    return ret;
}

inline char valorChar(tabuleiro* tab, const int pos){
    return ( (!preenchido(tab, pos)) ? (VAZIOC) : (((tab->pecas>>(pos))&1) ? (JOGADOR2C) : JOGADOR1C) ); //equivalente ao codigo abaixo, mas muito mais rapido no Atom
    //if(!preenchido(tab,x,y)) return VAZIOC;
    //if((tab->pecas>>(y*8+x))&1) return JOGADOR2C;
    //else return JOGADOR1C;
}

inline int atribJogadorSeVirar(tabuleiro* tab, char jogador, int pos){
    int virouPeca = valorChar(tab, pos) == oponente(jogador);
    if(virouPeca){
        tab->preenchido|=((int64_t)1<<(pos));
        tab->pecas&=(~((int64_t)1<<(pos)));
        tab->pecas|=((int64_t)jogadorDeChar(jogador)<<(pos));
    }
    return virouPeca;
}

inline void atribJogador(tabuleiro* tab, char jogador, int pos){
    tab->preenchido|=((int64_t)1<<(pos));
    tab->pecas&=(~((int64_t)1<<(pos)));
    tab->pecas|=((int64_t)jogadorDeChar(jogador)<<(pos));
}
int64_t getPecas(tabuleiro* tab, char jogador){
    return ((jogador == JOGADOR1C) ? (~tab->pecas) : tab->pecas)&tab->preenchido;
    //~ int64_t pecas = tab->pecas;
    //~ if(jogador == JOGADOR1C) pecas=~pecas;
    //~ pecas&=tab->preenchido;
    //~ return pecas;
}


//#define getLinha(tabPtr, y) (((tabPtr->pecas & tabPtr->preenchido)>>(y*8))&0xFF)
#define ehDoJogadorUm(tabPtr, pos) (preenchido(tabPtr,x,y) & (((tabPtr->pecas>>(y*8+x))&1) == 0))
#define ehDoJogadorDois(tabPtr, pos) (preenchido(tabPtr,x,y) & (tabPtr->pecas>>(y*8+x)))
#define atribJogadorUm(tabPtr, pos) (tabPtr->preenchido|=((int64_t)1<<(pos))); (tabPtr->pecas&=(~((int64_t)1<<(pos))))
#define atribJogadorDois(tabPtr, pos) (tabPtr->preenchido|=((int64_t)1<<(pos))) ; (tabPtr->pecas|=((int64_t)1<<(pos)))

void bin(int64_t x){
    int i;
    for(i=63;i>=0;i--) printf("%d",(int)((x>>i)&1));
    printf("\n");
}


void inicializaTabuleiro(tabuleiro* tab){
    tab->preenchido = 103481868288;
    tab->pecas = 34628173824;
    //Equivalente ao código comentado abaixo
    /*
    atribJogadorUm(tab,pos(3,3));
    atribJogadorUm(tab,pos(4,4));
    atribJogadorDois(tab,pos(3,4));
    atribJogadorDois(tab,pos(4,3));
    */
}







void imprimeTabuleiro(tabuleiro* tab){
    int x,y;
    printf("\nY\n");
    for(y=TABULEIRO_ALTURA-1;y>=0;y--){
        printf("%d ", yIntParaExt(y));
        for(x=0;x<TABULEIRO_LARGURA;x++){
            printf("%c ", valorChar(tab, pos(x,y)));
        }
        if(y == 4) printf(" (%c) = %d", JOGADOR1C, scoreTabuleiro(tab, JOGADOR1C));
        if(y == 3) printf(" (%c) = %d", JOGADOR2C, scoreTabuleiro(tab, JOGADOR2C));
        printf("\n");
    }
    printf("  ");
    for(x=0;x<TABULEIRO_LARGURA;x++){
		printf("%d ", xIntParaExt(x));
	}
	printf(" X   ");
    printf("tab.pecas= 0x%llx ;tab.preenchido= 0x%llx ;\n\n", tab->pecas, tab->preenchido);
}

void imprimeMascara(int64_t mascara){
    tabuleiro tabM;
    tabM.preenchido = tabM.pecas = mascara;
    imprimeTabuleiro(&tabM);
}

inline int naVizinhanca(tabuleiro* tab, const char jogador, const int x, const int y){
    int64_t pecas = getPecas(tab, jogador);
    const int64_t mascaraAdjacentes = cursor(pos(0,0)) | cursor(pos(1,0)) | cursor(pos(2,0)) | cursor(pos(0,1)) | cursor(pos(2,1)) | cursor(pos(0,2)) | cursor(pos(1,2)) | cursor(pos(2,2));
    const int64_t mascaraXZero = cursor(pos(0,0)) | cursor(pos(1,0)) | cursor(pos(1,1)) | cursor(pos(1,2)) | cursor(pos(0,2));
    const int64_t mascaraXMax = cursor(pos(0,0)) | cursor(pos(1,0)) | cursor(pos(0,1)) | cursor(pos(1,2)) | cursor(pos(0,2));
    const int64_t mascaraYZero = cursor(pos(0,0)) | cursor(pos(0,1)) | cursor(pos(1,1)) | cursor(pos(2,1)) | cursor(pos(2,0));
    const int64_t mascaraZeroZero = cursor(pos(0,1)) | cursor(pos(1,1)) | cursor(pos(1,0));
    const int64_t mascaraYMaxXZero = cursor(pos(0,1)) | cursor(pos(1,1)) | cursor(pos(0,0));
    
    //Idéias de otimização. Ver se maneiras diferentes de colocar o mesmo if influenciam no desempenho
    // Idéia 1 (mais rapido na espec)
    if(x > 0 && x < TABULEIRO_LARGURA-1){
        if(y  > 0) return (((mascaraAdjacentes<< pos(x-1,y-1))&pecas) != 0);
        if(y == 0) return (((mascaraYZero << pos(x-1,y))&pecas) != 0);
    }
    if(x == 0){
        if(y  > 0) return (((mascaraXZero << pos(x,y-1))&pecas) != 0);
        if(y == 0) return (((mascaraZeroZero << pos(x,y))&pecas) != 0);
    }
    if(x == TABULEIRO_LARGURA-1){
        if(y  > 0) return (((mascaraXMax << pos(x-1,y-1))&pecas) != 0);
        if(y == 0) return (((mascaraYMaxXZero << pos(x-1,y))&pecas) != 0);
    }
    return 0;
    
    //Idéia 2 (praticamente o mesmo desempenho que a idéia 3)
    //~ return ((x > 0 && x < TABULEIRO_LARGURA-1) ?
                //~ (
                    //~ (y  > 0) ? (((mascaraAdjacentes<< pos(x-1,y-1))&pecas) != 0)
                    //~ : (((mascaraYZero << pos(x-1,y))&pecas) != 0)
                //~ )
            //~ :((x == 0) ?
                //~ (
                    //~ (y  > 0) ? (((mascaraXZero << pos(x,y-1))&pecas) != 0)
                    //~ : (((mascaraZeroZero << pos(x,y))&pecas) != 0)
                //~ )
            //~ :((x == TABULEIRO_LARGURA-1) ?
                //~ (
                    //~ (y  > 0) ? (((mascaraXMax << pos(x-1,y-1))&pecas) != 0)
                    //~ : (((mascaraYMaxXZero << pos(x-1,y))&pecas) != 0)
                //~ )
            //~ :0)
        //~ ));
            //~ 
              
    // Idéia 3
    //~ if(x > 0 && y > 0 && x < TABULEIRO_LARGURA-1){
        //~ return (((mascaraAdjacentes<< pos(x-1,y-1))&pecas) != 0); //ok
    //~ }
    //~ else if(x == 0 && y > 0){
        //~ return (((mascaraXZero << pos(x,y-1))&pecas) != 0); //ok
    //~ }
    //~ else if(x == TABULEIRO_LARGURA-1 && y > 0){
        //~ return (((mascaraXMax << pos(x-1,y-1))&pecas) != 0); // ok
    //~ }
    //~ else if(x > 0 && x < TABULEIRO_LARGURA-1 && y == 0){
        //~ //imprimeMascara(mascaraYZero << pos(x-1,y));
        //~ //imprimeMascara((mascaraYZero << pos(x-1,y))&pecas);
        //~ return (((mascaraYZero << pos(x-1,y))&pecas) != 0); //ok
    //~ }
    //~ else if(x == 0 && y == 0){
        //~ return (((mascaraZeroZero << pos(x,y))&pecas) != 0); //ok
    //~ }
    //~ else if(x == TABULEIRO_LARGURA-1 && y == 0){
        //~ return (((mascaraYMaxXZero << pos(x-1,y))&pecas) != 0); //ok
    //~ }
    //~ return 0;

}

char* mensagensErroJogar[] = {
    "ERRO: Jogada invalida", 
    "Jogada bem sucedida.", 
    "ERRO: Uma das posicoes adjacentes deve ser do jogador oponente.", 
    "ERRO: Posicao jah preenchida.",
    "ERRO: Jogada aceita."
};

int jogar(tabuleiro* tab, char* jogador, int x, int y){
    //XXX Verificar se o jogador pode jogar mesmo (tem situacoes em que apenas o jogador oponente pode jogar
    if     (preenchido(tab, pos(x,y))) return JOGADA_POSICAO_JAH_PREENCHIDA;// printf();
    else if(!naVizinhanca(tab, oponente(*jogador),x,y)) return JOGADA_SEM_OPONENTES_ADJACENTES;
    else if(x < 0 || y < 0 || x > TABULEIRO_LARGURA-1 || y > TABULEIRO_ALTURA-1) return JOGADA_COM_POSICAO_INVALIDA;
    else{
        int i;
        int pos = pos(x,y);
        int infY=y, supY=y, infX=x, supX=x, infD1=x, supD1=x, infD2=x, supD2=x; //limites inferior e superior de linhas possiveis
        tabuleiro original = *tab;

        int offset;
        int64_t cur;
        int64_t pecas = tab->pecas;
        if(*jogador == JOGADOR1C) pecas=~pecas;
        pecas&=tab->preenchido;
        
        atribJogador(tab,*jogador,pos);
        
        //Paralelo ao eixo Y
        #ifndef DESABILITAR_PREENCHIMENTO_Y
        
        infY = supY = y;
        #ifdef OTIMIZACOES_ESTUPIDAS
        for(offset=pos(x,y-1), cur=cursor(offset); offset >= pos(x,0); offset-=8, cur>>=8){
        #else
        for(offset=pos(x,y-1), cur=cursor(offset); offset >= pos(x,0); offset-=8, cur= cursor(offset)){
        #endif
            if(pecas&cur){
                infY = offset/8; 
                break; 
            } 
            else if(!preenchido(tab, offset)) break;
        }
        
        
        #ifdef OTIMIZACOES_ESTUPIDAS
        for(offset=pos(x,y+1), cur=cursor(offset); offset <= pos(x,TABULEIRO_ALTURA-1); offset+=8, cur<<=8){
        #else
        for(offset=pos(x,y+1), cur=cursor(offset); offset <= pos(x,TABULEIRO_ALTURA-1); offset+=8, cur= cursor(offset)){
        #endif
            if(pecas&cur){
                supY = offset/8; 
                break; 
            } 
            else if(!preenchido(tab, offset)) break;
        }
        #endif
        
        //Paralelo ao Eixo X
        
        #ifndef DESABILITAR_PREENCHIMENTO_X
        infX=supX=x;
        #ifdef OTIMIZACOES_ESTUPIDAS
        for(offset=pos(x-1,y), cur=cursor(offset); offset >= pos(0,y); offset--, cur>>=1){
        #else
        for(offset=pos(x-1,y), cur=cursor(offset); offset >= pos(0,y); offset--, cur= cursor(offset)){
        #endif
            //printPos(offset);
            if(pecas&cur){
                infX = offset%8; 
                break; 
            } 
            else if(!preenchido(tab, offset)) break;
        }
        
        
        #ifdef OTIMIZACOES_ESTUPIDAS
        for(offset=pos(x+1,y), cur=cursor(offset); offset <= pos(TABULEIRO_LARGURA-1,y); offset++, cur<<=1){
        #else
        for(offset=pos(x+1,y), cur=cursor(offset); offset <= pos(TABULEIRO_LARGURA-1,y); offset++, cur= cursor(offset)){
        #endif
            if(pecas&cur){
                supX = offset%8; 
                break; 
            } 
            else if(!preenchido(tab, offset)) break;
        }
        //printf("infX, supX = %d, %d\n", infX, supX);
        #endif
        
        //Diagonais
        int xd, yd;
        #ifndef DESABILITAR_PREENCHIMENTO_D1
        infD1=supD1=x;
        if(x != TABULEIRO_LARGURA-1 && y < TABULEIRO_ALTURA-1){
            for(offset=pos(x+1,y+1), cur=cursor(offset); offset%8 > x && offset <= pos(TABULEIRO_LARGURA-1,TABULEIRO_ALTURA-1); offset+=9, cur= cursor(offset)){
                //printPos(offset);
                if(pecas&cur){
                    supD1 = offset%8; 
                    break; 
                } 
                else if(!preenchido(tab, offset)) break;
            }
        }
        //printf("B\n");
        
        if(x != 0){
            #ifdef OTIMIZACOES_ESTUPIDAS
            for(offset=pos(x-1,y-1), cur=cursor(offset); offset >= 0 && offset%8 < x; offset-=9, cur>>=9){
            #else
            for(offset=pos(x-1,y-1), cur=cursor(offset); offset >= 0 && offset%8 < x; offset-=9, cur= cursor(offset)){
            #endif
                //printPos(offset);
                if(pecas&cur){
                    infD1 = offset%8; 
                    break; 
                } 
                else if(!preenchido(tab, offset)) break;
            }
        }
        
        //printf("infD1, supD1 = %d, %d\n", infD1, supD1);
        #endif
        
        #ifndef DESABILITAR_PREENCHIMENTO_D2
        supD2=infD2=x;
        
        //printf("x asd s %d %d\n", x, TABULEIRO_LARGURA-1);

        if(x != TABULEIRO_LARGURA-1){
			xd = x+1;
			yd= y-1;
            #ifdef OTIMIZACOES_ESTUPIDAS
            //printPos(offsetMax);
            for(offset=pos(x+1,y-1), cur=cursor(offset); offset >= 0 && offset%8 >x; offset-=7, cur>>=7){
            #else
            for(offset=pos(x+1,y-1), cur=cursor(offset); offset >= 0 && offset%8 >x; offset-=7, cur= cursor(offset)){ // offset <= pos(TABULEIRO_LARGURA-1,TABULEIRO_ALTURA-1)
            #endif
                //printPos(offset);
                if(pecas&cur){
                    supD2 = offset%8; 
                    break; 
                } 
                else if(!preenchido(tab, offset)) break;
            }
        }
        //printf("B!\n");
        
        if(x != 0 && y < TABULEIRO_ALTURA-1){
            #ifdef OTIMIZACOES_ESTUPIDAS
            int offsetMin = pos(0,0+x+y);
            for(offset=pos(x-1,y+1), cur=cursor(offset); offset <= offsetMin && offset <= pos(TABULEIRO_LARGURA-1,TABULEIRO_ALTURA-1); offset+=7, cur<<=7){
            #else
            for(offset=pos(x-1,y+1), cur=cursor(offset); offset%8 < x && offset <= pos(TABULEIRO_LARGURA-1,TABULEIRO_ALTURA-1); offset+=7, cur= cursor(offset)){
            #endif
                //printPos(offset);
                if(pecas&cur){
                    infD2 = offset%8; 
                    break; 
                } 
                else if(!preenchido(tab, offset)) break;
            }
        }
        
        //printf("infD2, supD2 = %d, %d\n", infD2, supD2);
        #endif
        
        //printf("infX=%d, supX=%d\n", infX, supX);
        
        //vira as pecas relevantes
        #ifdef OTIMIZACAO_DETECAO_DE_MOVIMENTO
        int movimentoValido = (supX-x > 1 || x-infX >1 || supY-y > 1 || y-infY > 1 || supD2-x > 1 || x-infD2 >1 || supD1-x > 1 || x-infD1 > 1);
        if(!movimentoValido){
            *tab = original;
            return JOGADA_INVALIDA;
        }
        for(offset= pos(x,infY); offset < pos(x,supY); offset+=8)  atribJogador(tab, *jogador, offset); // eixo Y
        for(offset= pos(infX,y); offset < pos(supX,y); offset++)   atribJogador(tab, *jogador, offset); // eixo X
        for(i=infD1;i<supD1;i++) atribJogador(tab, *jogador, pos(i, i-x+y)); //diagonal 1
        for(i=infD2;i<supD2;i++) atribJogador(tab, *jogador, pos(i,-i+x+y)); //diagonal 2
        #endif
        
        #ifndef OTIMIZACAO_DETECAO_DE_MOVIMENTO
        int movimentoValido = 0; //O movimento eh valido se ele virou pelo menos uma peca inimiga
        for(offset= pos(x,infY); offset < pos(x,supY); offset+=8)  movimentoValido |= atribJogadorSeVirar(tab, *jogador, offset); // eixo Y
        for(offset= pos(infX,y); offset < pos(supX,y); offset++)   movimentoValido |= atribJogadorSeVirar(tab, *jogador, offset); // eixo X
        for(i=infD1;i<supD1;i++) movimentoValido |= atribJogadorSeVirar(tab, *jogador, pos(i, i-x+y)); //diagonal 1
        for(i=infD2;i<supD2;i++) movimentoValido |= atribJogadorSeVirar(tab, *jogador, pos(i,-i+x+y)); //diagonal 2
        //~ int movimentoValidoAlt = (supX-x > 1 || x-infX >1 || supY-y > 1 || y-infY > 1 || supD2-x > 1 || x-infD2 >1 || supD1-x > 1 || x-infD1 > 1);
        //~ if(movimentoValido != movimentoValidoAlt){
            //~ printf("\nDiscrepancia na otimizacao de movimentoValido x=%d, y=%d. Jogador = %c:\n", x,y, *jogador);
            //~ printf("normal=%d\notimizado=%d\n", movimentoValido, movimentoValidoAlt);
            //~ printf("infX, supX = %d, %d\n", infX, supX);
            //~ printf("infY, supY = %d, %d\n", infY, supY);
            //~ printf("infD1, supD1 = %d, %d\n", infD1, supD1);
            //~ printf("infD2, supD2 = %d, %d\n", infD2, supD2);
            //~ imprimeTabuleiro(&original);
            //~ printf("\n\n");
            //~ sleep(1);
            //~ 
        //~ }
        if(!movimentoValido){
            *tab = original;
            return JOGADA_INVALIDA;
        }
        #endif
        //Troca de jogador
        *jogador = oponente(*jogador);//XXX Verificar se o jogador pode jogar mesmo (tem situacoes em que apenas um jogador pode jogar, mesmo sendo em situações mais comuns a vez de um jogador específico
        return JOGADA_VALIDA;
        //~ }
        //~ else{
            //~ *tab = original;
            //~ return JOGADA_INVALIDA;
        //~ }
    }
    return 0;
}



int ehFimDeJogo(tabuleiro* tab){
    printf("ehFimDeJogo(tabuleiro* tab) nao implementado!\n");
    return 0;
}

void testeAutomatizado(){
    tabuleiro tab;
    char jogador;
    
    int todosOsTestesOK = 1;
    int testeOK;
    int retJogar;
    printf("Teste da reta discontinua... ");
    jogador = JOGADOR2C;
    tab.pecas = 20959574884352;
    tab.preenchido = 2285996748308480;
    jogar(&tab, &jogador, 2, 6);
    testeOK=tab.pecas == 1155655574749184 && tab.preenchido == 3411896655151104;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Teste basico de reta no eixo x... ");
    jogador = JOGADOR1C;
    tab.pecas = 34628173824;
    tab.preenchido = 103481868288;
    jogar(&tab, &jogador, 2, 4);
    testeOK=tab.pecas == 268435456 && tab.preenchido == 120661737472;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Teste basico de reta no eixo y... ");
    jogador = JOGADOR1C;
    tab.pecas = 34628173824;
    tab.preenchido = 103481868288;
    jogar(&tab, &jogador, 3, 5);
    jogar(&tab, &jogador, 4, 5);
    jogar(&tab, &jogador, 5, 4);
    jogar(&tab, &jogador, 4, 2);
    jogar(&tab, &jogador, 2, 5);
    jogar(&tab, &jogador, 4, 2);
    testeOK=tab.pecas == 0x1c0800000000 && tab.preenchido == 0x1c3818100000;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Teste basico diagonal 1... ");
    jogador = JOGADOR1C;
    tab.pecas= 0x1c0818080000 ;tab.preenchido= 0x1c3c18080000 ;
    jogar(&tab, &jogador, 4, 6);
    testeOK=tab.pecas == 0x40818080000 && tab.preenchido == 0x101c3c18080000;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Outro teste de diagonal... ");
    jogador = JOGADOR2C;
    tab.pecas= 0x20140010000000 ;tab.preenchido= 0x281c3c18080800 ;
    jogar(&tab, &jogador, 2, 3);
    testeOK = tab.pecas== 0x20140c1c000000 && tab.preenchido== 0x281c3c1c080800 ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Duas diagonais e uma reta x... ");
    jogador=JOGADOR1C;
    tab.pecas= 0x20140c1c000000 ;tab.preenchido= 0x281c3c1c080800;
    jogar(&tab, &jogador, 1, 4);
    testeOK = tab.pecas== 0x20100018000000 && tab.preenchido== 0x281c3e1c080800 ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Diagonal de coefAngular baixo... ");
    jogador=JOGADOR1C;
    tab.pecas= 0x101810000000 ;tab.preenchido= 0x1e1818000000 ;
    jogar(&tab, &jogador, 5, 3);
    testeOK = tab.pecas== 0x100800000000 && tab.preenchido== 0x1e1838000000;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Teste basico diverso... ");
    jogador = JOGADOR1C;
    tab.pecas = 34628173824;
    tab.preenchido = 103481868288;
    //Movimentos invalidos
    jogar(&tab, &jogador, 1, 4);
    jogar(&tab, &jogador, 1, 3);
    jogar(&tab, &jogador, 1, 1);
    jogar(&tab, &jogador, 6, 1);
    jogar(&tab, &jogador, 4, 1);
    //movimentos validos
    jogar(&tab, &jogador, 2, 4);
    jogar(&tab, &jogador, 4, 5);
    jogar(&tab, &jogador, 5, 3);
    jogar(&tab, &jogador, 0, 4);
    jogar(&tab, &jogador, 1, 4);
    jogar(&tab, &jogador, 3, 5);
    testeOK=tab.pecas == 17617955848192 && tab.preenchido == 26518067609600;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Movimento diagonal ateh o canto superior... ");
    jogador = JOGADOR1C;
    tab.pecas= 0x4181000000000 ;tab.preenchido= 0x41e1818200000 ;
    jogar(&tab, &jogador, 3, 7);
    jogar(&tab, &jogador, 1, 7);
    jogar(&tab, &jogador, 2, 7);
    testeOK=tab.pecas== 0x200181000000000 && tab.preenchido== 0xe041e1818200000;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Movimento diagonal ateh o canto inferior... ");
    jogador = JOGADOR2C;
    tab.pecas= 0xbf89a5ddab85cf9f ;tab.preenchido= 0xffffffffffffdf9f ;
    retJogar = jogar(&tab, &jogador, 5, 0);
    testeOK= retJogar == JOGADA_VALIDA && tab.pecas== 0xbf89a5dfaf8ddfbf && tab.preenchido== 0xffffffffffffdfbf ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    //tab.pecas= 0xff80bcbcbcbc8080 ;tab.preenchido= 0x7f7f67677f7f7f ; //limtest, feito manualmente

    printf("Movimento diagonal ateh o canto direito... ");
    jogador = JOGADOR1C;
    tab.pecas= 0xf07c5f5e7e7f75f0 ;tab.preenchido= 0xf4fcfffefefff5f0 ;
    retJogar = jogar(&tab, &jogador, 3, 1);
    testeOK= retJogar == JOGADA_VALIDA && tab.pecas== 0xf07c5f1e5e6f05f0 && tab.preenchido== 0xf4fcfffefefffdf0 ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    printf("Movimento diagonal do topo... ");
    jogador = JOGADOR2C;
    tab.pecas= 0x10181e363e0c2020;
	tab.preenchido= 0x14381fbffffda020;
    retJogar = jogar(&tab, &jogador, 6, 5);
    testeOK= retJogar == JOGADA_VALIDA && tab.pecas== 0x10385e363e0c2020 && tab.preenchido== 0x14385fbffffda020 ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    
    printf("Movimento diagonal do topo 2... ");
    jogador = JOGADOR1C;
    tab.pecas= 0x30383c42361c3020;
	tab.preenchido= 0x74b9fffffffdb020;
	//imprimeTabuleiro(&tab);
    retJogar = jogar(&tab, &jogador, 3, 7);
    //imprimeTabuleiro(&tab);
    testeOK= retJogar == JOGADA_VALIDA && tab.pecas== 0x201402361c3020 && tab.preenchido== 0x7cb9fffffffdb020 ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));
    
    
    printf("Movimento y quase do topo com diagonal errada... ");
    jogador = JOGADOR1C;
    tab.pecas= 0x4041efee280bc00;
	tab.preenchido= 0x4051fffffffffff;
	//imprimeTabuleiro(&tab);
    retJogar = jogar(&tab, &jogador, 1, 6);
    //imprimeTabuleiro(&tab);
    testeOK= retJogar == JOGADA_VALIDA && tab.pecas== 0x40418f4e080bc00 && tab.preenchido== 0x4071fffffffffff ;
    todosOsTestesOK = todosOsTestesOK && testeOK;
    printf("%s\n",((testeOK) ? ("OK") : ("FALHA")));    
    
    
    
    if(todosOsTestesOK) printf("\nTODOS OS TESTES OK!\n\n");
    else printf("\nTESTES COM FALHAS!!\n\n");
    
    //printf("Fim dos testes.\n");
    
}

//~ void dummyBench(){
    //~ tabuleiro tab;
    //~ inicializaTabuleiro(&tab);
    //~ int ntestes=1000000;
    //~ char jogador = JOGADOR1C;
    //~ while(ntestes--){
        //~ if(ntestes%200 == 0) inicializaTabuleiro(&tab);
        //~ //imprimeTabuleiro(&tab);
        //~ jogar(&tab, &jogador,rand()%8, rand()%8);
    //~ }
//~ }

/*
int main(){
    
    //~ tabuleiro tab;
    //~ inicializaTabuleiro(&tab);
    //~ imprimeTabuleiro(&tab);
    //~ jogoInterativo(&tab);
    
    dummyBench();
    return 0;
}
*/

