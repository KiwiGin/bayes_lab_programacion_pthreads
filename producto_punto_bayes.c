#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct{
    double *vectorA;
    double *vectorB;
    int inicio;
    int fin;
    double resultado_parcial;
} ThreadData;

double resultado_global=0.0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* calcular_producto_parcial(void* arg){
    ThreadData* data = (ThreadData*)arg;
    double suma_local=0.0;

    for(int i=data->inicio; i<data->fin; i++){
        suma_local += data->vectorA[i] * data->vectorB[i];
    }

    data->resultado_parcial = suma_local;

    pthread_mutex_lock(&mutex);
    resultado_global += suma_local;
    pthread_mutex_unlock(&mutex);
    return NULL;
}

double producto_punto_secuencial(double* A, double* B, int N){
    double resultado = 0.0;
    for(int i=0; i<N; i++){
        resultado += A[i] * B[i];
    }
    return resultado;
}

double producto_punto_paralelo(double* A, double* B, int N, int num_hilos){
    pthread_t* hilos;
    ThreadData* datos_hilos;
    resultado_global=0.0;
    hilos=(pthread_t*)malloc(num_hilos * sizeof(pthread_t));
    datos_hilos=(ThreadData*)malloc(num_hilos * sizeof(ThreadData));

    if(hilos == NULL || datos_hilos == NULL){
        fprintf(stderr, "Error: No se pudo asignar memoria\n");
        return -1;
    }

    int elementos_por_hilo = N / num_hilos;
    int elementos_restantes = N % num_hilos;

    for(int i=0; i<num_hilos; i++){
        datos_hilos[i].vectorA= A;
        datos_hilos[i].vectorB = B;
        datos_hilos[i].inicio = i*elementos_por_hilo;
        datos_hilos[i].fin=(i+1)*elementos_por_hilo;

        if(i==num_hilos - 1){
            datos_hilos[i].fin += elementos_restantes;
        }

        datos_hilos[i].resultado_parcial=0.0;

        if(pthread_create(&hilos[i], NULL, calcular_producto_parcial, &datos_hilos[i])){
            fprintf(stderr, "Error: No se pudo crear el hilo %d\n",i);
            free(hilos);
            free(datos_hilos);
            return -1;
        }
    }

    for(int i = 0; i<num_hilos; i++){
        if(pthread_join(hilos[i], NULL) != 0){
            fprintf(stderr, "Error: No se pudo hacer join con el hilo %d\n", i);
        }
    }

    double resultado = resultado_global;

    free(hilos);
    free(datos_hilos);

    return resultado;
}

double obtener_tiempo(){
    struct timeval tiempo;
    gettimeofday(&tiempo, NULL);
    return tiempo.tv_sec + tiempo.tv_usec / 1000000.0;
}

void inicializar_vectores(double* A, double* B, int N){
    srand(time(NULL));
    for(int i=0; i<N; i++){
        A[i]=(double)rand()/RAND_MAX*10.0;
        B[i]=(double)rand()/RAND_MAX*10.0;
    }
}

void mostrar_vectores(double* A, double* B, int N){
    if(N<=10){
        printf("Vector A: [");
        for(int i=0; i<N; i++){
            printf("%.2f", A[i]);
            if(i<N-1) printf(", ");
        }
        printf("]\n");

        printf("Vector B: [");
        for(int i=0; i<N; i++){
            printf("%.2f", B[i]);
            if(i<N-1) printf(", ");
        }

        printf("]\n");
    }
}

int main(int argc, char* argv[]){
    int N, num_hilos;
    double *A, *B;
    double resultado_seq, resultado_par;
    double tiempo_inicio, tiempo_fin;
    double tiempo_seq, tiempo_par;

    if(argc<2){
        printf("Uso: %s <tamaño_vectores> [numero_hilos]\n", argv[0]);
        printf("Si no se especifica numero_hilos, se usaará el numero de procesadores disponibles\n");
        return 1;
    }

    N=atoi(argv[1]);
    if(N<=0){
        fprintf(stderr, "Error: El tamaño de los vectores debe ser positivo\n");
        return 1;
    }

    //nro de hilos
    if(argc>=3){
        num_hilos=atoi(argv[2]);
        if(num_hilos<=0){
            fprintf(stderr, "Error: El numero de hilos debe ser positivo\n");
            return 1;
        }
    }else{
        num_hilos=sysconf(_SC_NPROCESSORS_ONLN);
        if(num_hilos<=0){
            num_hilos=4;
        }
    }

    printf("---CALCULO DEL PRODUCTO PUNTO---\n");
    printf("Tamaño de vectores: %d\n", N);
    printf("Numero de hilos: %d\n", num_hilos);
    printf("--------------------------------\n\n");

    //asignar memoria para los vectores
    A=(double*)malloc(N * sizeof(double));
    B=(double*)malloc(N * sizeof(double));

    if(A==NULL || B==NULL){
        fprintf(stderr, "Error: No se pudo asignar memoria para los vectores\n");
        return 1;
    }

    inicializar_vectores(A,B,N);
    mostrar_vectores(A,B,N);

    //PRODUCTO PUNTO SECUENCIAL
    printf("Calculando producto punto de forma secuencial\n");
    tiempo_inicio = obtener_tiempo();
    resultado_seq=producto_punto_secuencial(A,B,N);
    tiempo_fin=obtener_tiempo();
    tiempo_seq=tiempo_fin - tiempo_inicio;

    printf("Resultado secuencial: %.6f\n", resultado_seq);
    printf("Tiempo secuencial: %.6f segundos\n\n", tiempo_seq);

    //PRODUCTO PUNTO PARALELA
    printf("Calculando producto punto paralelo con %d hilos\n", num_hilos);
    tiempo_inicio=obtener_tiempo();
    resultado_par=producto_punto_paralelo(A,B,N,num_hilos);
    tiempo_fin = obtener_tiempo();
    tiempo_par = tiempo_fin - tiempo_inicio;

    if(resultado_par == -1){
        fprintf(stderr, "Error en el calculo paralelo\n");
        free(A);
        free(B);
        return 1;
    }

    printf("Resultado paralelo: %.6f\n", resultado_par);
    printf("Tiempo paralelo: %.6f segundos\n\n", tiempo_par);

    double diferencia = abs(resultado_seq - resultado_par);
    printf("Diferencia entre resultados: %.10f\n", diferencia);

    if(diferencia < 1e-6){
        printf("[OK] Los resultados son correctos - diferencia despreciable\n");
    }else{
        printf("[WARNING] Los resultados difieren significativamente\n");
    }

    if(tiempo_par >0){
        double speedup = tiempo_seq / tiempo_par;
        printf("\nSpeedup: %.2fx\n", speedup);

        if(speedup > 1.0){
            printf("[OK] La versioon paralela es mas rapidda\n");
        }else{
            printf("[WARNING] La version secuencial es mas rapida - overheaad puede ser mayor que el beneficio\n");
        }
    }

    if(N!=3){
        printf("\n---EJEMPLO CON VECTORES PEQUEÑOS---\n");
        double A_ejemplo[] = {1.0, 2.0, 3.0};
        double B_ejemplo[] = {4.0, 5.0, 6.0};
        double resultado_ejemplo = producto_punto_secuencial(A_ejemplo, B_ejemplo, 3);
        printf("A=[1,2,3],  B=[4,5,6]\n");
        printf("Producto punto = %.0f\n", resultado_ejemplo);
    }

    free(A);
    free(B);
    pthread_mutex_destroy(&mutex);

    return 0;
}
