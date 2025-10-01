#
# Makefile para LAB2_CORNEJO_LOBOS.cpp
# Simulador de Supervivencia con IPC (Memoria Compartida y fork)
#

# --- 1. Variables de Configuración ---

# Nombre del ejecutable que se generará
TARGET = simulacion_supervivencia

# Archivo fuente principal
SRC = LAB2_CORNEJO_LOBOS.cpp

# Compilador a utilizar
CXX = g++

# Banderas de compilación
# -std=c++17: Requerido por librerías modernas como <random>
# -Wall -Wextra: Activar todas las advertencias
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

# Banderas del linker (Librerías del Sistema)
# -lrt: Librería de tiempo real (para IPC/shm en algunos sistemas)
# -lpthread: Librería de hilos POSIX (buena práctica con fork y atómicas)
LDFLAGS = -lrt -lpthread


# --- 2. Reglas (Targets) ---

# Regla por defecto: 'make' sin argumentos ejecutará la regla 'run'
all: run

# Regla para COMPILAR: cómo construir el ejecutable
$(TARGET): $(SRC)
# -> Línea de comando: debe empezar con TAB
	$(CXX) $(SRC) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET)

# Regla para EJECUTAR: compila si es necesario, y luego ejecuta
run: $(TARGET)
# -> Línea de comando: debe empezar con TAB
	./$(TARGET) $(ARGS)

# Regla de limpieza: elimina el ejecutable
clean:
# -> Línea de comando: debe empezar con TAB
	rm -f $(TARGET)
	rm -f *.o

# Regla de ayuda: muestra los comandos disponibles
help:
# -> Línea de comando: debe empezar con TAB
	@echo "--- Comandos de Makefile ---"
	@echo "  make ................. Compila y EJECUTA el programa (equivalente a 'make run')."
	@echo "  make ARGS=\"N\" ......... Compila y ejecuta con N días (ej: make ARGS=\"20\")."
	@echo "  make clean ........... Elimina el ejecutable."
	@echo "  make help ............ Muestra esta ayuda."

# Declarar targets que no son archivos para evitar conflictos
.PHONY: all clean run help $(TARGET)