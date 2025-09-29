#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include <fcntl.h>
#include <random>
#include <atomic>

using namespace std;

// Estructura para recursos
struct Recursos {
    int agua;
    int alimentos;
    int construccion;
    int senales;
};

// Estructura para shared memory
struct SharedMemoryData {
    int recursos_equipos[4]; // 0: agua, 1: alimentos, 2: construccion, 3: senales
    atomic<int> equipos_completados;
    int dia_actual;
};

// Constantes
const int MIN_AGUA = 8;
const int MIN_ALIMENTOS = 12;
const int MIN_CONSTRUCCION = 4;
const int MIN_SENALES = 2;
const int MORAL_INICIAL = 100;
const int DIAS_RESCATE = 10;
const int MAX_DIAS = 30;
const int MIN_DIAS = 10;

// Nombres de equipos
const vector<string> nombres_equipos = {
    "AGUA", "ALIMENTOS", "CONSTRUCCION", "SEÑALES"
};

// Mensajes de actividades
const vector<string> actividades_exploracion = {
    "Explorando fuentes de agua...",
    "Explorando territorio para cazar...",
    "Buscando materiales de construcción...",
    "Recolectando combustible seco..."
};

const vector<string> actividades_recoleccion = {
    "Recolectando agua del arroyo encontrado...",
    "Intentando pescar en la laguna...",
    "Cortando ramas útiles...",
    "Manteniendo fogata de señales..."
};

const vector<string> actividades_finalizacion = {
    "Purificando agua recolectada...",
    "¡Capturado pez pequeño! Preparando...",
    "Construyendo refugio básico...",
    "Creando señales de humo..."
};

// Generador aleatorio thread-safe
mt19937& get_random_generator() {
    static thread_local mt19937 generator(random_device{}());
    return generator;
}

// Función para generar resultado aleatorio
int generar_resultado(int dia, int equipo_id, pid_t pid) {
    auto& gen = get_random_generator();
    uniform_int_distribution<> dis(1, 100);
    
    // Semilla única basada en día, equipo y PID
    int semilla = dia * 1000 + equipo_id * 100 + pid % 1000;
    gen.seed(semilla + time(nullptr));
    
    int probabilidad = dis(gen);
    
    if (probabilidad <= 30) {
        return 100; // Éxito total
    } else if (probabilidad <= 80) {
        uniform_int_distribution<> partial_dis(50, 80);
        return partial_dis(gen); // Éxito parcial
    } else {
        uniform_int_distribution<> fail_dis(5, 29);
        return fail_dis(gen); // Fracaso
    }
}

// Función para calcular unidades recolectadas
int calcular_unidades(int porcentaje, int equipo_id, int dia, pid_t pid) {
    auto& gen = get_random_generator();
    
    // Semilla única para mayor variabilidad
    int semilla = dia * 2000 + equipo_id * 200 + pid % 2000;
    gen.seed(semilla + time(nullptr));
    
    int min_base, max_base;
    
    switch (equipo_id) {
        case 0: min_base = 6; max_base = 14; break;    // Agua
        case 1: min_base = 8; max_base = 20; break;    // Alimentos
        case 2: min_base = 4; max_base = 12; break;    // Construcción
        case 3: min_base = 2; max_base = 8; break;     // Señales
        default: min_base = 5; max_base = 10;
    }
    
    uniform_int_distribution<> dis(min_base, max_base);
    int objetivo_base = dis(gen);
    
    int unidades = (objetivo_base * porcentaje) / 100;
    
    // Variación final aleatoria
    uniform_int_distribution<> bonus_dis(-2, 3);
    unidades += bonus_dis(gen);
    
    return max(1, unidades);
}

// Función del equipo de recolección
void equipo_recoleccion(int equipo_id, int shm_id, int dia) {
    pid_t pid = getpid();
    
    // Conectar a shared memory
    SharedMemoryData* shared_data = (SharedMemoryData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    
    // Actividades de exploración
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] " 
         << actividades_exploracion[equipo_id] << endl;
    
    // Tiempos de espera aleatorios
    auto& gen = get_random_generator();
    uniform_int_distribution<> sleep_dis(1, 3);
    sleep(sleep_dis(gen));
    
    // Actividades de recolección
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] " 
         << actividades_recoleccion[equipo_id] << endl;
    sleep(sleep_dis(gen));
    
    // Actividades de finalización
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid << "] " 
         << actividades_finalizacion[equipo_id] << endl;
    sleep(1);
    
    // Generar resultado único para este equipo en este día
    int porcentaje_exito = generar_resultado(dia, equipo_id, pid);
    int unidades = calcular_unidades(porcentaje_exito, equipo_id, dia, pid);
    
    // Escribir resultado en shared memory
    shared_data->recursos_equipos[equipo_id] = unidades;
    shared_data->equipos_completados++;
    
    cout << "[EQUIPO " << nombres_equipos[equipo_id] << " - PID: " << pid 
         << "] Completo ciclo: " << unidades << " unidades obtenidas" << endl;
    
    shmdt(shared_data);
    exit(0);
}

// Función para evaluar supervivencia del día
bool evaluar_supervivencia(const Recursos& totales, int& moral_perdida, string& razon_penalizacion) {
    bool sobrevive = true;
    moral_perdida = 0;
    vector<string> razones;
    
    if (totales.agua < MIN_AGUA) {
        int perdida = (MIN_AGUA - totales.agua) * 3;
        moral_perdida += perdida;
        razones.push_back("falta de agua");
        sobrevive = false;
    }
    if (totales.alimentos < MIN_ALIMENTOS) {
        int perdida = (MIN_ALIMENTOS - totales.alimentos) * 2;
        moral_perdida += perdida;
        razones.push_back("falta de comida");
        sobrevive = false;
    }
    if (totales.construccion < MIN_CONSTRUCCION) {
        int perdida = (MIN_CONSTRUCCION - totales.construccion) * 4;
        moral_perdida += perdida;
        razones.push_back("falta de refugio");
        sobrevive = false;
    }
    if (totales.senales < MIN_SENALES) {
        int perdida = (MIN_SENALES - totales.senales) * 5;
        moral_perdida += perdida;
        razones.push_back("señales insuficientes");
        sobrevive = false;
    }
    
    // Construir mensaje de penalización
    if (!razones.empty()) {
        razon_penalizacion = "por ";
        for (size_t i = 0; i < razones.size(); i++) {
            if (i > 0) {
                razon_penalizacion += (i == razones.size() - 1) ? " y " : ", ";
            }
            razon_penalizacion += razones[i];
        }
    }
    
    return sobrevive;
}

// Función principal del coordinador
void coordinador(int dias_simulacion) {
    int moral = MORAL_INICIAL;
    int senales_activas_consecutivas = 0;
    bool rescate_exitoso = false;
    
    cout << "=== SISTEMA DE SUPERVIVENCIA ACTIVADO ===" << endl;
    cout << "Moral inicial: " << MORAL_INICIAL << "/100" << endl;
    cout << "Días de simulación: " << dias_simulacion << endl << endl;
    
    // Crear shared memory
    key_t key = ftok(".", 'S');
    int shm_id = shmget(key, sizeof(SharedMemoryData), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(1);
    }
    
    // Conectar a shared memory
    SharedMemoryData* shared_data = (SharedMemoryData*)shmat(shm_id, NULL, 0);
    if (shared_data == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    
    for (int dia = 1; dia <= dias_simulacion && moral > 0 && !rescate_exitoso; dia++) {
        cout << "=== DÍA " << dia << " DE SUPERVIVENCIA ===" << endl;
        cout << "Iniciando equipos de recolección..." << endl << endl;
        
        // Inicializar shared memory para este día
        shared_data->equipos_completados = 0;
        shared_data->dia_actual = dia;
        for (int i = 0; i < 4; i++) {
            shared_data->recursos_equipos[i] = 0;
        }
        
        vector<pid_t> pids(4);
        Recursos recursos_dia = {0, 0, 0, 0};
        
        // Crear procesos equipos
        for (int i = 0; i < 4; i++) {
            pids[i] = fork();
            
            if (pids[i] == 0) {
                // Proceso hijo
                equipo_recoleccion(i, shm_id, dia);
            } else if (pids[i] < 0) {
                perror("fork");
                exit(1);
            }
        }
        
        // Esperar a que todos los equipos completen
        while (shared_data->equipos_completados < 4) {
            usleep(100000); // Espera 100ms
        }
        
        // Recoger resultados de shared memory
        recursos_dia.agua = shared_data->recursos_equipos[0];
        recursos_dia.alimentos = shared_data->recursos_equipos[1];
        recursos_dia.construccion = shared_data->recursos_equipos[2];
        recursos_dia.senales = shared_data->recursos_equipos[3];
        
        // Esperar a que todos los procesos terminen
        for (int i = 0; i < 4; i++) {
            waitpid(pids[i], NULL, 0);
        }
        
        cout << "\nREPORTES FINALES:" << endl;
        cout << "- Equipo Agua: " << recursos_dia.agua << " unidades obtenidas" << endl;
        cout << "- Equipo Alimentos: " << recursos_dia.alimentos << " unidades obtenidas" << endl;
        cout << "- Equipo Construcción: " << recursos_dia.construccion << " unidades obtenidas" << endl;
        cout << "- Equipo Señales: " << recursos_dia.senales << " unidades obtenidas" << endl;
        
        cout << "\nRESULTADOS DEL DÍA:" << endl;
        cout << "- Agua recolectada: " << recursos_dia.agua << "/" << MIN_AGUA << " (" 
             << (recursos_dia.agua >= MIN_AGUA ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "- Alimentos obtenidos: " << recursos_dia.alimentos << "/" << MIN_ALIMENTOS << " (" 
             << (recursos_dia.alimentos >= MIN_ALIMENTOS ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "- Materiales de construcción: " << recursos_dia.construccion << "/" << MIN_CONSTRUCCION << " (" 
             << (recursos_dia.construccion >= MIN_CONSTRUCCION ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        cout << "- Señales mantenidas: " << recursos_dia.senales << "/" << MIN_SENALES << " (" 
             << (recursos_dia.senales >= MIN_SENALES ? "SUFICIENTE" : "INSUFICIENTE") << ")" << endl;
        
        // Evaluar supervivencia
        int moral_perdida;
        string razon_penalizacion;
        bool dia_sobrevivido = evaluar_supervivencia(recursos_dia, moral_perdida, razon_penalizacion);
        
        moral -= moral_perdida;
        if (moral < 0) moral = 0;
        
        // Actualizar contador de señales activas consecutivas
        if (recursos_dia.senales >= MIN_SENALES) {
            senales_activas_consecutivas++;
            cout << "✅ Señales activas: Día " << senales_activas_consecutivas << " consecutivo" << endl;
        } else {
            senales_activas_consecutivas = 0;
            cout << "❌ Señales insuficientes, contador reiniciado" << endl;
        }
        
        cout << "\nEstado: " << (dia_sobrevivido ? "DÍA SUPERADO" : "SUPERVIVENCIA CRÍTICA") << endl;
        cout << "Moral del grupo: " << moral << "/100";
        if (moral_perdida > 0) {
            cout << " (-" << moral_perdida << " " << razon_penalizacion << ")";
        } else {
            cout << " (sin cambio)";
        }
        cout << endl;
        
        // Verificar condiciones de victoria/derrota
        if (senales_activas_consecutivas >= DIAS_RESCATE) {
            cout << "\n🎉 ¡RESCATE EXITOSO! 🎉" << endl;
            cout << "Han mantenido señales activas por " << senales_activas_consecutivas 
                 << " días consecutivos" << endl;
            cout << "¡El equipo de rescate los ha encontrado!" << endl;
            rescate_exitoso = true;
            break;
        }
        
        if (moral <= 0) {
            cout << "\n💀 FRACASO - MORAL AGOTADA 💀" << endl;
            cout << "El grupo ha perdido la esperanza después de " << dia << " días" << endl;
            break;
        }
        
        if (dia == dias_simulacion && !rescate_exitoso) {
            cout << "\n⏰ LÍMITE DE TIEMPO ALCANZADO ⏰" << endl;
            cout << "Sobrevivieron " << dias_simulacion << " días con moral " << moral << "/100" << endl;
            cout << "Señales activas consecutivas: " << senales_activas_consecutivas << "/" << DIAS_RESCATE << endl;
        }
        
        cout << "\n----------------------------------------\n" << endl;
        sleep(1);
    }
    
    // Mensaje final adicional
    if (rescate_exitoso) {
        cout << "\n✨ FELICIDADES - MISIÓN CUMPLIDA ✨" << endl;
        cout << "Todos los sobrevivientes han sido rescatados exitosamente" << endl;
    }
    
    // Limpiar shared memory
    shmdt(shared_data);
    shmctl(shm_id, IPC_RMID, NULL);
}

int main(int argc, char* argv[]) {
    int dias_simulacion;
    
    if (argc == 2) {
        dias_simulacion = atoi(argv[1]);
        if (dias_simulacion < MIN_DIAS || dias_simulacion > MAX_DIAS) {
            cout << "Error: Los días deben estar entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
            return 1;
        }
    } else if (argc == 1) {
        // Valor por defecto
        dias_simulacion = 15;
        cout << "Usando valor por defecto: 15 días" << endl;
    } else {
        cout << "Uso: " << argv[0] << " [días_simulacion]" << endl;
        cout << "Días deben estar entre " << MIN_DIAS << " y " << MAX_DIAS << endl;
        return 1;
    }
    
    coordinador(dias_simulacion);
    
    return 0;
}