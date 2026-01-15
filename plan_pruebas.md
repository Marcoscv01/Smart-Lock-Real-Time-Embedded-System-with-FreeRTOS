# Plan de pruebas

## 1. Casos de prueba

### CP1 – Funcionamiento normal
- Introducción de PIN correcto
- Verificación apertura de servo y mensaje en LCD

### CP2 – Acceso denegado
- PIN incorrecto
- Mensaje de error + no apertura

### CP3 – Emergencia
- Pulsación botón PANIC
- Entrada inmediata a estado PANIC

### CP4 – Carga concurrente
- RTC + LCD + SD activos
- Verificación de jitter y latencias

## 2. Métricas medidas

- Latencia FSM → LCD / SERVO / SD
- Latencia ISR → FSM
- Jitter de tareas periódicas (RTC, LCD)
- Uso de CPU por tarea
- Uso de stacks
- Ocupación de colas

## 3. Umbrales aceptables

| Métrica | Umbral |
|-------|--------|
| FSM → Servo | < 1 ms |
| FSM → LCD | < 500 ms |
| ISR → FSM | < 200 µs |
| Jitter RTC | < 5 ms |
| CPU total | < 70% |

## 4. Resultados iniciales

- Servo y SD cumplen sobradamente
- LCD presenta latencias mayores por I2C
- Jitter bajo mientras no hay contención fuerte
